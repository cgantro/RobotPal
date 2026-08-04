# 카메라 스트리밍 성능 측정 최종 정리

이 문서는 RobotPal의 실제 카메라 스트리밍 경로에서 동기 Readback과 PBO Readback, JPEG 단일·멀티워커를 비교한 탐색 측정 결과를 정리한다. 조건별 1회 측정이므로 수치는 현재 환경에서 병목의 위치와 변화 방향을 판단하기 위한 값이며, 일반적인 성능 보장이나 확정 개선률로 사용하지 않는다.

## 1. 측정 조건 및 사용 기술

### 측정 조건

| 항목 | 조건 |
|---|---|
| 빌드 | x64 Release |
| 카메라/FBO 해상도 | 1232×832 RGBA |
| 입력 선택률 | `rate=1` |
| JPEG 품질 | 70 |
| JPEG 워커 | 1개 또는 4개로 고정 |
| 인코딩 큐 | bounded queue, 용량 6, 초과 시 가장 오래된 프레임 폐기 |
| 네트워크 | localhost TCP |
| 수신부 | 실제 Python TCP 수신, JPEG decode 및 consume 경로 |
| 실행시간 | 조건별 약 60초 |
| 워밍업 | 최초 10초 제외 |
| 분석 구간 | 약 49.8초 |
| 반복 | 조건별 1회 |

### 실제 측정 경로

```text
카메라 FBO 렌더링
→ GPU Readback
→ 인코딩 큐 적재 및 대기
→ JPEG 압축
→ 전송 큐
→ C++ TCP 송신
→ Python TCP 수신
→ JPEG decode 및 consume
```

### 사용 기술과 계측

- OpenGL 직접 `glReadPixels`를 사용하는 동기 Readback
- 두 개의 PBO를 ping-pong 방식으로 사용하는 비동기 Readback
- `glMapBufferRange`를 통한 PBO 데이터 회수
- C++ JPEG 단일·멀티워커
- bounded queue와 drop-oldest 정책
- C++ JSONL 계측으로 프레임 생성, Readback, 큐 대기, JPEG, TCP 송신 기록
- Python 계측으로 TCP 수신, JPEG decode 및 최종 consume 기록
- PowerShell 프로세스 샘플링으로 CPU 사용률과 최대 Working Set 기록
- 분석 스크립트로 처리량, p50·p95·p99·최대값 및 누락 원인 계산

### 시간 계측 구현

단일 프로세스 안의 구간 처리시간은 시스템 시간 변경의 영향을 받지 않는 단조 시계를 사용했다.

| 실행 영역 | 사용 시계 | 기록 목적 |
|---|---|---|
| C++ 송신 프로그램 | `std::chrono::steady_clock`, nanosecond | Readback, 큐 대기, JPEG, TCP send 등 프로세스 내부 구간시간 |
| C++ 송신 프로그램 | `std::chrono::system_clock`, Unix nanosecond | Python 수신 프로그램과 종단간 시각 연결 |
| Python 수신 프로그램 | `time.perf_counter_ns()` | JPEG decode 및 consume 구간시간 |
| Python 수신 프로그램 | `time.time_ns()` | C++ 생성 시각과 비교한 종단간 지연 |

각 구간은 시작과 종료 시점의 단조 시계 차이를 `duration_ns`로 기록했다. 송신 프로그램과 수신 프로그램은 별도 프로세스이므로, 프로세스 간 종단간 지연은 같은 장비의 Unix nanosecond 시각을 사용해 다음과 같이 계산했다.

```text
구간 처리시간 = steady_end_ns - steady_start_ns
종단간 지연 = receiver_consumed_unix_ns - frame_generated_unix_ns
```

localhost의 같은 Windows 시스템 시계를 공유했기 때문에 별도 장비 간 시계 동기화 오차는 없지만, 운영체제 스케줄링과 시스템 시계 보정의 영향은 남을 수 있다.

### 프레임 단위 추적과 로그 형식

각 프레임에 32비트 `frame_id`와 생성 시각 `generated_unix_ns`를 부여했다. 벤치마크 실행 중에는 JPEG 앞에 다음 메타데이터를 추가해 TCP/WebSocket 수신 후에도 동일 프레임을 연결했다.

```text
RPBENCH1 magic 8바이트
+ frame_id 4바이트
+ generated_unix_ns 8바이트
+ JPEG payload
```

송신부와 수신부는 이벤트 한 건을 JSONL 한 줄로 즉시 기록했다. 주요 필드는 다음과 같다.

| 필드 | 의미 |
|---|---|
| `event` | 측정 구간 또는 실패·폐기 원인 |
| `frame_id` | 생성부터 최종 소비까지 연결할 프레임 식별자 |
| `steady_ns` | 프로세스 내부 이벤트 순서와 경과시간 기준 |
| `unix_ns` | 송신·수신 프로세스 간 종단간 시간 기준 |
| `duration_ns` | 해당 구간에서 측정한 처리시간 |
| `value` | 데이터 크기 또는 측정 시점의 큐 길이 |
| `reason` | 폐기 및 실패 원인 |

기록한 주요 이벤트는 다음과 같다.

```text
frame_generated
readback_sync_glreadpixels
readback_pbo_submit / readback_pbo_map / readback_pbo_copy
readback_completed / readback_failed
encode_queued / encode_dequeued / encode_queue_dropped
encode_completed / encode_failed
transport_queued / send_queued / send_completed / send_failed
received / consumed / receive_failed / receive_dropped
```

이를 통해 단순히 목표 FPS와 최종 수신 수의 차이를 모두 드랍으로 처리하지 않고, 입력 생성 수, 큐 폐기, 인코딩 실패, 송신 실패, 수신 실패 및 종료 시 파이프라인 잔여 프레임을 구분했다.

### 통계와 시스템 자원 계산

최초 10초는 워밍업으로 제외하고, 이후 생성된 `frame_id`만 송신·수신 로그에서 선택했다. 각 구간의 `duration_ns` 표본을 ms로 변환해 정렬한 뒤 p50·p95·p99를 선형 보간으로 계산하고 최대값을 함께 기록했다.

```text
측정시간 = 마지막 선택 이벤트 시각 - 최초 선택 이벤트 시각
생성 처리량 = frame_generated / 측정시간
최종 처리량 = consumed / 측정시간
큐 폐기율 = encode_queue_dropped / encode_queued
전체 미소비율 = (frame_generated - consumed) / frame_generated
```

PowerShell에서 1초마다 송신·수신 관련 프로세스의 누적 CPU 시간과 Working Set을 샘플링했다.

```text
CPU 사용률 = 누적 CPU 시간 증가량 / 샘플 간격 × 100
최대 메모리 = 샘플 중 프로세스 Working Set 합계의 최대값
```

CPU 사용률은 프로세스가 사용한 전체 논리 코어 시간을 합산하므로 멀티코어 사용 시 100%를 넘을 수 있다. 또한 실행별 생성·인코딩·송신·수신·소비 건수와 중복 ID를 대조하고, `송신=수신`, `수신=소비`, 실패 이벤트 0건 여부를 신뢰성 조건으로 확인했다.

프레임 폐기율은 다음과 같이 정의했다.

```text
인코딩 큐 폐기율 = encode_queue_dropped / encode_queued × 100
```

최종 처리량은 워밍업을 제외한 구간에서 Python 수신부가 최종 소비한 프레임 수를 측정시간으로 나눈 값이다.

```text
최종 처리량 = consumed / measured_seconds
```

## 2. 단계별 결과

비교 시나리오는 다음과 같다.

1. 동기 Readback + JPEG 단일 워커
2. PBO 비동기 Readback + JPEG 단일 워커
3. PBO 비동기 Readback + JPEG 4개 워커

### 전체 결과

| 단계 | 생성률 | 최종 처리량 | Readback p50 | JPEG p50 | 큐 대기 p50 | 큐 폐기율 | 종단간 p50 |
|---|---:|---:|---:|---:|---:|---:|---:|
| 동기 + worker1 | 34.857fps | 34.857fps | 24.459ms | 25.186ms | 0.165ms | 0% | 73.382ms |
| PBO + worker1 | 38.892fps | 32.889fps | 20.728ms | 27.601ms | 141.157ms | 15.33% | 240.824ms |
| PBO + worker4 | 37.348fps | 37.328fps | 20.680ms | 28.403ms | 0.073ms | 0% | 101.802ms |

### 2.1 동기 Readback + 단일 워커

동기 방식에서는 직접 `glReadPixels` 내부 p50이 약 22.718ms였고 Readback 전체 p50은 24.459ms였다. 이 대기로 프레임 생성률이 약 34.857fps로 제한됐다.

JPEG p50은 25.186ms였지만 생성률이 단일 워커 처리 용량을 크게 넘지 않아 큐 대기 p50은 0.165ms, 큐 폐기는 0건이었다. 따라서 이 조건에서는 JPEG 워커보다 GPU Readback 동기화가 먼저 전체 입력률을 제한했다.

### 2.2 PBO 비동기 Readback + 단일 워커

PBO를 적용하자 Readback 전체 p50은 24.459ms에서 20.728ms로 15.26% 감소했고, 생성률은 34.857fps에서 38.892fps로 11.58% 증가했다.

하지만 현재 PBO 구현은 명령 제출만 빠르다. PBO submit p50은 0.051ms지만 이전 PBO를 `glMapBufferRange`로 회수하는 데 p50 17.937ms가 걸렸다. 즉 GPU 대기가 제거된 것이 아니라 `glReadPixels`에서 PBO map으로 상당 부분 이동했다.

생성률이 증가하면서 단일 JPEG 워커의 처리 용량을 넘어섰다. 인코딩 큐 대기 p50이 141.157ms까지 증가했고, 큐에 적재된 1,936프레임 중 297프레임이 폐기되어 폐기율은 15.33%였다. 최종 처리량은 오히려 32.889fps로 낮아졌다.

이 단계의 의미는 PBO만으로 전체 성능이 개선됐다는 것이 아니다. Readback이 일부 개선되면서 기존에 가려져 있던 JPEG 단일 워커 병목이 드러난 것이다.

### 2.3 PBO 비동기 Readback + 멀티워커

PBO 조건에서 JPEG 워커를 1개에서 4개로 늘리자 최종 처리량은 32.889fps에서 37.328fps로 13.50% 높아졌다. 큐 대기 p50은 141.157ms에서 0.073ms로 99.95% 감소했고, 큐 폐기는 297건에서 0건으로 줄었다.

다만 JPEG 한 프레임의 압축시간은 27.601ms에서 28.403ms로 줄지 않았다. 멀티워커는 개별 압축을 가속한 것이 아니라 여러 프레임을 병렬 처리해 파이프라인의 JPEG 처리 용량을 높였다.

### 최초 조건과 최종 조건 비교

| 지표 | 동기+worker1 | PBO+worker4 | 변화 |
|---|---:|---:|---:|
| 생성률 | 34.857fps | 37.348fps | 7.15% 높음 |
| 최종 처리량 | 34.857fps | 37.328fps | 7.09% 높음 |
| Readback p50 | 24.459ms | 20.680ms | 15.45% 짧음 |
| 큐 폐기율 | 0% | 0% | 동일 |
| 종단간 p50 | 73.382ms | 101.802ms | 38.73% 길음 |
| 평균 CPU | 187.99% | 233.59% | 24.26% 높음 |

이 결과는 처리량과 지연이 서로 다른 최적화 목표임을 보여준다. 최종 구성은 처리량을 높이고 PBO 적용 후의 JPEG 적체를 제거했지만, PBO의 한 프레임 파이프라인 지연과 map 대기로 종단간 지연 및 CPU 사용량은 증가했다.

따라서 멀티스레딩은 최초 병목에 대한 근본 해결책이 아니었다. Readback 개선으로 입력률이 높아진 뒤 발생한 JPEG 백프레셔를 해소하는 보완책이었다.

## 3. 한계점 및 개선 방안

### 측정 및 결과의 한계

- 조건별 1회만 실행해 반복 변동성과 실행 순서 영향을 분리하지 못했다.
- localhost TCP를 사용했으므로 실제 네트워크 지연, 패킷 손실 및 대역폭 제한은 반영하지 않았다.
- 카메라 한 대만 측정했으므로 여러 카메라를 동시에 전송할 수 있는 최대 개수를 판단할 수 없다.
- CPU에서 관찰한 OpenGL 호출시간이므로 순수 GPU 복사시간과 드라이버 대기시간을 분리하지 못했다.
- PBO 실행 종료 시 파이프라인에 worker1은 2프레임, worker4는 1프레임이 남았다.
- PBO와 멀티워커를 모두 변경한 최초·최종 비교만으로 각 변경의 독립적인 인과관계를 설명해서는 안 된다. 중간 조건을 함께 봐야 한다.

### 구현의 한계

현재 PBO 구현은 두 버퍼를 번갈아 사용하고 다음 프레임에서 이전 버퍼를 바로 map한다. GPU 복사가 아직 끝나지 않았다면 `glMapBufferRange`가 블로킹되므로 비동기 제출의 이점을 충분히 활용하지 못한다.

또한 GPU의 RGBA 프레임을 CPU 메모리로 복사한 후 CPU JPEG 인코더에 전달하므로, 카메라 수가 증가하면 렌더링·Readback 대역폭·메모리 복사·JPEG 처리량이 모두 증가한다.

### 개선 방향

1. PBO ring을 3개 이상으로 늘리고 `glFenceSync`로 완료 여부를 확인한다.
2. 아직 준비되지 않은 PBO를 즉시 map하지 않고 충분히 오래된 완료 버퍼만 회수한다.
3. 여러 카메라는 Readback 명령을 먼저 제출한 뒤 완료된 결과를 나중에 회수해 GPU 복사와 렌더링을 중첩한다.
4. 저지연이 중요하면 PBO 파이프라인 깊이와 처리량 사이의 절충을 별도로 측정한다.
5. 여러 카메라 지원이 목표라면 1·2·4대의 실제 1232×832 카메라로 카메라별 생성·송신·수신 FPS와 폐기율을 측정한다.
6. 가능하다면 GPU 텍스처를 CPU RGBA로 Readback하지 않고 하드웨어 인코더에 직접 전달하는 경로를 검토한다.
7. JPEG가 다시 병목이 되는 조건에서만 워커 수를 조정하고 CPU 사용량과 지연을 함께 비교한다.

## 결론

이번 탐색 측정에서 최초 동기+단일 워커 구성은 Readback 대기로 입력률이 제한됐고, PBO 적용은 Readback 시간을 일부 줄였지만 PBO map 대기를 제거하지 못했다. 높아진 입력률은 단일 JPEG 워커의 큐 적체와 폐기를 발생시켰으며, 4개 워커는 이 2차 병목을 해소했다.

따라서 현재 결과의 핵심은 단순히 “멀티스레딩으로 성능을 개선했다”가 아니다. 실제 종단간 경로를 계측해 GPU Readback 병목, 병목 이동, JPEG 백프레셔를 구분했고, 멀티워커가 필요한 조건과 그 대가인 CPU·지연 증가를 확인했다는 점이다.
