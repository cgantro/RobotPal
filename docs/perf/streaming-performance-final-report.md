# RobotPal camera streaming performance report

> Status: measurement template. Do not fill values from the old 2026 ablation benchmark. Use new PresentMon/Tracy runs from the three profiling branches.

## 1. Problem

RobotPal camera streaming was completed end-to-end at commit `2dc99a37` (2025-12-04). The objective of this experiment is to measure how the simulation application's frame performance changes when GPU readback and JPEG processing are optimized in controlled stages.

The repository history already contained PBO readback before the first completed streaming commit. Therefore the no-PBO baseline is a reconstruction of `2dc99a37`: the application and streaming structure are preserved while only readback is restored to synchronous `glReadPixels`.

## 2. Compared branches

| Stage | Branch | GPU readback | JPEG execution |
| --- | --- | --- | --- |
| Baseline | `perf/streaming-baseline-sync` | synchronous `glReadPixels` | caller/main path |
| PBO | `perf/streaming-pbo` | PBO ping-pong | caller/main path |
| PBO + MT | `perf/streaming-pbo-mt` | PBO ping-pong | 4 worker threads |

NetworkEngine's historical I/O threads exist in all three branches and are not treated as the streaming-compute optimization variable.

## 3. Tools

- **PresentMon**: authoritative whole-application result
  - App FPS
  - Frame Time p95
- **Tracy Profiler**: bottleneck/timeline evidence
  - Main / Render thread
  - readback zone
  - JPEG zone
  - worker-thread execution in the multithreaded stage
- **Google Benchmark**: optional isolated JPEG microbenchmark only

Receiver/sink FPS and manual `std::chrono` benchmark statistics are excluded from the headline comparison.

## 4. Test environment

Fill only what is necessary:

- CPU:
- GPU:
- OS:
- Build: Release
- Camera framebuffer: 400 x 400
- JPEG quality: 85
- Warm-up:
- Measurement duration:
- Repetitions: 3 per branch

Keep scene, window size, GPU selection, camera state, and workload identical.

## 5. Whole-application results

Use the median of three PresentMon runs.

| Stage | App FPS | Frame Time p95 |
| --- | ---: | ---: |
| Synchronous baseline | TBD | TBD ms |
| PBO | TBD | TBD ms |
| PBO + multithreading | TBD | TBD ms |

### Improvement summary

- Baseline -> PBO App FPS: TBD
- PBO -> PBO + MT App FPS: TBD
- Baseline -> final App FPS: TBD
- Baseline -> final Frame Time p95: TBD

Do not add more headline metrics unless they are required to explain an unexpected result.

## 6. Tracy bottleneck evidence

### 6.1 Synchronous baseline

Capture:
- `Frame`
- `Streaming.Readback.Sync`
- `Streaming.SendFrame`
- `Streaming.JPEG`

Finding:
- TBD

### 6.2 PBO only

Capture:
- `Frame`
- `Streaming.Readback.PBO`
- `Streaming.SendFrame`
- `Streaming.JPEG`

Finding:
- TBD

### 6.3 PBO + multithreading

Capture:
- `Frame`
- `Streaming.Readback.PBO`
- `Streaming.Enqueue`
- `Streaming.WorkerJob`
- `Streaming.JPEG`
- worker-thread timeline

Finding:
- TBD

The report should explain bottleneck movement, not merely list profiling zones.

## 7. Optional JPEG microbenchmark

Only include this section if the Google Benchmark result helps explain the timeline.

| Benchmark | Result |
| --- | ---: |
| 400 x 400 RGB, JPEG Q85 | TBD |

This number represents isolated JPEG code-path cost and must not be described as RobotPal application performance.

## 8. Conclusion

Write the final conclusion in this order:

1. How much the synchronous streaming baseline affected App FPS / Frame Time.
2. Whether PBO reduced the main-thread readback stall and how the application result changed.
3. Whether moving JPEG work to worker threads removed work from the caller/main path and how the application result changed.
4. Any remaining bottleneck visible in Tracy.

Do not claim an optimization worked unless both the profiler evidence and PresentMon result support that claim.

## Portfolio summary

After measurement, write a 3-4 sentence summary using only the measured values.

Recommended structure:

> Camera streaming initially placed GPU readback and JPEG processing on the simulation update path, causing [measured effect]. PresentMon and Tracy were used to separate whole-app frame degradation from code-level bottlenecks. PBO readback and JPEG worker separation were applied in controlled stages, changing App FPS from [initial] to [final] and Frame Time p95 from [initial] to [final]. These values are measurements from the stated test environment and are not a general performance guarantee.
