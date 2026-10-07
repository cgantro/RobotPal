# RobotPal camera streaming performance report

> Status: measurement template. Do not fill values from the old 2026 ablation benchmark. Use new PresentMon/Tracy runs from the three profiling branches.

## 1. Problem

RobotPal camera streaming was completed end-to-end at commit `2dc99a37` (2025-12-04). The camera framebuffer is fixed at **1232 x 832** because that resolution was selected to preserve reliable JETANK license-plate recognition quality. The objective is therefore to improve the simulation application's frame performance **without reducing image resolution**, by optimizing the GPU readback and JPEG processing path in controlled stages.

The repository history already contained PBO readback before the first completed streaming commit. Therefore the no-PBO baseline is a reconstruction of `2dc99a37`: the application and streaming structure are preserved while only readback is restored to synchronous `glReadPixels`.

## 2. Compared branches

| Stage | Branch | GPU readback | JPEG execution |
| --- | --- | --- | --- |
| Baseline | `perf/streaming-baseline-sync` | synchronous `glReadPixels` | caller/main path |
| PBO | `perf/streaming-pbo` | PBO ping-pong | caller/main path |
| PBO + MT | `perf/streaming-pbo-mt` | PBO ping-pong | 4 worker threads |

NetworkEngine's historical I/O threads exist in all three branches and are not treated as the streaming-compute optimization variable.

## 3. Tools

- **PresentMon**: authoritative Simulation FPS
- **Tracy Profiler**:
  - Camera Streaming Send FPS via the named frame set `CameraStreamSend`
  - bottleneck/timeline evidence
  - Main / Render thread
  - readback zone
  - JPEG zone
  - worker-thread execution in the multithreaded stage
- **Google Benchmark**: optional isolated JPEG microbenchmark only

Receiver/sink FPS and manual `std::chrono` benchmark statistics are excluded from the headline comparison.

## 4. Streaming ON/OFF procedure

Each branch is measured in two states.

```powershell
# Streaming OFF
$env:ROBOTPAL_STREAMING="0"
.\RobotPal.exe

# Streaming ON
$env:ROBOTPAL_STREAMING="1"
.\RobotPal.exe
```

Streaming OFF skips camera readback, JPEG encode, and streaming-worker initialization. NetworkEngine remains available so the comparison isolates the camera-streaming workload.

Run each state three times and use the median Simulation FPS. Camera Streaming Send FPS is measured only in the ON state.

## 5. Test environment

Fill only what is necessary:

- CPU:
- GPU:
- OS:
- Build: Release
- Camera framebuffer: 1232 x 832 (fixed for license-plate recognition quality)
- Camera streaming FPS cap: 60 FPS
- Simulation/render FPS cap: none
- JPEG quality: 85
- Warm-up:
- Measurement duration:
- Repetitions: 3 per branch

Keep scene, window size, GPU selection, camera state, and workload identical. Do not lower camera resolution. The only FPS cap is the fixed 60 FPS camera-stream target; the simulation/render loop must remain uncapped.

## 6. Performance results

Use three repeated runs per branch and report the median.

| Stage | Simulation FPS (Streaming OFF) | Simulation FPS (Streaming ON) | Streaming Penalty | Camera Streaming Send FPS (ON) |
| --- | ---: | ---: | ---: | ---: |
| Synchronous baseline | TBD | TBD | TBD % | TBD / 60 |
| PBO | TBD | TBD | TBD % | TBD / 60 |
| PBO + multithreading | TBD | TBD | TBD % | TBD / 60 |

### Success criteria

- **Streaming OFF Simulation FPS** establishes the branch's application baseline.
- **Streaming ON Simulation FPS** shows the real cost of camera streaming on the simulation.
- **Streaming Penalty (%) = (OFF FPS - ON FPS) / OFF FPS × 100** quantifies that cost.
- Camera streaming should sustain as close to the **60 FPS camera target** as possible without exceeding it.
- As PBO and worker separation are applied, the goal is to reduce Streaming Penalty while approaching the 60 FPS send target.
- Receiver/decode FPS is not part of this experiment.
- Frame-time percentiles may be retained only as secondary diagnostic evidence.

Do not add more headline metrics unless they are required to explain an unexpected result.

## 7. Tracy bottleneck evidence

### 7.1 Synchronous baseline

Capture:
- `Frame`
- `Streaming.Readback.Sync`
- `Streaming.SendFrame`
- `Streaming.JPEG`

Finding:
- TBD

### 7.2 PBO only

Capture:
- `Frame`
- `Streaming.Readback.PBO`
- `Streaming.SendFrame`
- `Streaming.JPEG`

Finding:
- TBD

### 7.3 PBO + multithreading

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

## 8. Optional JPEG microbenchmark

Only include this section if the Google Benchmark result helps explain the timeline.

| Benchmark | Result |
| --- | ---: |
| 1232 x 832 RGB, JPEG Q85 | TBD |

This number represents isolated JPEG code-path cost and must not be described as RobotPal application performance.

## 9. Conclusion

Write the final conclusion in this order:

1. The Simulation FPS with Streaming OFF and ON for every branch.
2. The resulting Streaming Penalty for every branch.
3. Whether the synchronous baseline can sustain the 60 FPS camera-stream target.
4. Whether PBO reduces the Streaming Penalty by reducing readback stalls.
5. Whether moving JPEG work to worker threads further reduces the penalty while sustaining the 60 FPS send target.
6. Any remaining bottleneck visible in Tracy.

Do not claim an optimization worked unless both the profiler evidence and PresentMon result support that claim.

## Portfolio summary

After measurement, write a 3-4 sentence summary using only the measured values.

Recommended structure:

> Camera streaming used the fixed 1232 x 832 resolution required for JETANK license-plate recognition and targeted a 60 FPS real-camera operating ceiling. Each stage was measured with Streaming OFF and ON to quantify the simulation-side cost of camera streaming, while Tracy measured completed Camera Streaming Send FPS and exposed readback/JPEG bottlenecks. PBO readback and JPEG worker separation changed the streaming penalty from [initial penalty] to [final penalty] while Camera Streaming Send FPS changed from [initial] to [final]. These values are measurements from the stated test environment and are not a general performance guarantee.
