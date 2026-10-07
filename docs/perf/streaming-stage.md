# Streaming performance stage: PBO only

## Purpose

This branch isolates the first optimization stage: PBO double-buffered GPU readback, while JPEG encoding remains synchronous on the caller/main update path.

Base application state: `2dc99a37` ("TCP Stream Test Complete", 2025-12-04).

The application structure, historical stb_image_write JPEG path, quality 85, and 1232 x 832 camera framebuffer are intentionally kept aligned with `perf/streaming-baseline-sync`. The relevant difference is the historical PBO ping-pong readback implementation.

This branch is not a runtime feature toggle.

## Stage definition

- Camera streaming: enabled
- GPU readback: PBO ping-pong / double buffering
- Streaming JPEG worker threads: none
- JPEG: historical stb_image_write path, quality 85
- Camera framebuffer: fixed 1232 x 832 test setup
- NetworkEngine I/O threads: preserved as part of the original completed networking implementation

## Measurement policy

Whole-application performance is measured externally with **PresentMon**.

Only two headline metrics are retained:

1. App FPS
2. Frame Time p95

Run the same scenario three times and report the median. Do not use an internal FPS counter or manual `std::chrono` accumulator as the final result.

**Tracy** is diagnostic only. Use it to inspect the `Frame`, `Streaming.Readback.PBO`, `Streaming.SendFrame`, and `Streaming.JPEG` zones. Tracy-enabled runs are not the headline PresentMon runs.

**Google Benchmark** is optional and isolated. The included JPEG benchmark is a control for the same 1232 x 832 Q85 JPEG test path; it is not RobotPal FPS.

Receiver/sink FPS is intentionally excluded.

## Build modes

### PresentMon measurement build

```powershell
cmake -S . -B build-presentmon -DCMAKE_BUILD_TYPE=Release -DROBOTPAL_ENABLE_TRACY=OFF -DROBOTPAL_BUILD_MICROBENCHMARKS=OFF
cmake --build build-presentmon --config Release
```

Run RobotPal under PresentMon, filter to `RobotPal.exe`, and keep the scene, window, GPU, and run duration identical to the other branches.

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

## Comparison rule

Compare against:
- `perf/streaming-baseline-sync`: same completed streaming structure, synchronous readback
- `perf/streaming-pbo-mt`: same PBO stage plus streaming JPEG worker threads

The PBO effect should be attributed from the baseline -> this branch comparison, not from the old 2026 ablation benchmark.
