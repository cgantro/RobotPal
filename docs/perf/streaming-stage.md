# Streaming performance stage: PBO + multithreaded JPEG

## Purpose

This branch is the third controlled stage in the RobotPal camera-streaming comparison.

It is derived directly from `perf/streaming-pbo`, so the PBO readback code, fixed test camera setup, JPEG implementation, JPEG quality, and profiling toolchain are unchanged. The added variable is streaming JPEG work being moved off the caller/main update path to a fixed worker pool.

This branch is not a runtime multithreading toggle.

## Stage definition

- Camera streaming: enabled
- GPU readback: same PBO ping-pong implementation as `perf/streaming-pbo`
- JPEG: same historical stb_image_write path, quality 85
- Streaming JPEG workers: 4
- Queue: simple FIFO work queue
- Drop policy: none
- Camera framebuffer: fixed 1232 x 832 requirement for license-plate recognition
- NetworkEngine I/O threads: unchanged from the historical completed implementation

No bounded-queue/drop-oldest policy or later libjpeg-turbo change is added here, because those would introduce additional variables into the PBO + multithreading comparison.

## Fixed camera requirement

- Camera framebuffer is fixed at **1232 x 832** because this resolution was selected to preserve reliable JETANK license-plate recognition quality.
- Resolution reduction is therefore **not** considered a valid performance optimization in this experiment.
- Camera capture/streaming requests are capped at **60 FPS**, matching the practical stock Jetson Nano / IMX219 camera target. The simulation/render loop itself remains uncapped.
- The optimization target is the transfer/processing path itself: GPU readback and JPEG execution, while keeping image resolution constant.

## Measurement policy

The final whole-application comparison uses **PresentMon** only for the headline metrics:

1. App FPS
2. Frame Time p95

Run each branch three times under the same scene, GPU, window, build mode, warm-up, and measurement duration. Use the median result.

Do not use internal FPS counters, receiver FPS, or manual `std::chrono` statistics as headline data.

**Tracy** is diagnostic. Compare the timeline against the previous two branches:

- `Frame`
- `Streaming.Readback.PBO`
- `Streaming.Enqueue`
- `Streaming.WorkerJob`
- `Streaming.JPEG`
- thread `Main / Render`
- threads `Streaming JPEG Worker`

The expected structural change is not assumed to be faster in advance; verify that JPEG work actually leaves the main/update path and whether App FPS / Frame Time improve.

**Google Benchmark** remains an isolated control for the same 1232 x 832 Q85 JPEG code path. It is not application FPS.

## Build modes

### PresentMon measurement build

```powershell
cmake -S . -B build-presentmon -DCMAKE_BUILD_TYPE=Release -DROBOTPAL_ENABLE_TRACY=OFF -DROBOTPAL_BUILD_MICROBENCHMARKS=OFF
cmake --build build-presentmon --config Release
```

### Tracy diagnosis build

```powershell
cmake -S . -B build-tracy -DCMAKE_BUILD_TYPE=Release -DROBOTPAL_ENABLE_TRACY=ON -DROBOTPAL_BUILD_MICROBENCHMARKS=OFF
cmake --build build-tracy --config Release
```

### Google Benchmark

```powershell
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DROBOTPAL_BUILD_MICROBENCHMARKS=ON -DROBOTPAL_ENABLE_TRACY=OFF
cmake --build build-bench --config Release --target RobotPalJpegBenchmark
```

## Comparison sequence

```text
perf/streaming-baseline-sync
  synchronous glReadPixels + synchronous JPEG

        -> PBO only

perf/streaming-pbo
  PBO ping-pong + synchronous JPEG

        -> streaming multithreading only

perf/streaming-pbo-mt
  PBO ping-pong + 4 JPEG workers
```

This three-branch sequence is the controlled performance comparison. The later 2026 benchmark harness remains a separate ablation experiment and must not be presented as the historical initial-to-final result.
