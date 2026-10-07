# Streaming performance stage: synchronous baseline

## Purpose

This branch is the controlled pre-PBO / pre-streaming-worker baseline for RobotPal camera streaming.

Base application state: `2dc99a37` ("TCP Stream Test Complete", 2025-12-04), the earliest commit found where the RobotPal camera path is connected end-to-end to the streaming manager.

Important historical note: the repository's Texture implementation already contained PBO readback before that end-to-end streaming commit. Therefore no literal committed "completed streaming + no PBO" snapshot exists in the available history. This branch reconstructs that missing baseline by preserving the `2dc99a37` application/streaming structure and replacing only GPU readback with synchronous `glReadPixels`.

This is not a runtime PBO-off switch and is not derived from the current optimized pipeline.

## Stage definition

- Camera streaming: enabled
- GPU readback: synchronous `glReadPixels`
- PBO: none
- Streaming JPEG worker threads: none
- JPEG: historical stb_image_write path, quality 85
- Camera framebuffer: historical 400 x 400 setup
- NetworkEngine I/O threads: preserved because they are part of the completed historical networking implementation, not the streaming compute optimization under test

## Measurement policy

Whole-application performance is measured externally with **PresentMon**.

Only two headline metrics are retained:

1. App FPS
2. Frame Time p95

Run the same RobotPal scenario three times and report the median of the three runs. Do not use an internal FPS counter or manual `std::chrono` accumulator as the final performance result.

**Tracy** is diagnostic only. It is used to show where the main/render thread spends time (especially synchronous readback and JPEG encoding). Tracy-enabled runs are not used as the headline PresentMon result because profiler instrumentation adds overhead.

**Google Benchmark** is optional and isolated. The included target benchmarks the historical 400 x 400, Q85 JPEG encode path. Its result describes that code path only; it must not be presented as RobotPal application FPS.

Receiver/sink FPS is intentionally not part of this experiment.

## Build modes

### PresentMon measurement build

Build Release with Tracy disabled:

```powershell
cmake -S . -B build-presentmon -DCMAKE_BUILD_TYPE=Release -DROBOTPAL_ENABLE_TRACY=OFF -DROBOTPAL_BUILD_MICROBENCHMARKS=OFF
cmake --build build-presentmon --config Release
```

Run RobotPal under PresentMon, filter to `RobotPal.exe`, keep the scenario and window/GPU settings identical across all three branches, and export CSV.

### Tracy diagnosis build

```powershell
cmake -S . -B build-tracy -DCMAKE_BUILD_TYPE=Release -DROBOTPAL_ENABLE_TRACY=ON -DROBOTPAL_BUILD_MICROBENCHMARKS=OFF
cmake --build build-tracy --config Release
```

Capture a representative run and inspect:
- `Frame`
- `Streaming.Readback.Sync`
- `Streaming.SendFrame`
- `Streaming.JPEG`

### Google Benchmark

```powershell
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DROBOTPAL_BUILD_MICROBENCHMARKS=ON -DROBOTPAL_ENABLE_TRACY=OFF
cmake --build build-bench --config Release --target RobotPalJpegBenchmark
```

## Comparison rule

Compare this branch against:
- `perf/streaming-pbo`
- `perf/streaming-pbo-mt`

Do not compare against the 2026 benchmark harness as if it were the historical baseline.
