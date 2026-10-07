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
- Camera framebuffer: fixed 1232 x 832 requirement for license-plate recognition
- NetworkEngine I/O threads: preserved because they are part of the completed historical networking implementation, not the streaming compute optimization under test

## Fixed camera requirement

- Camera framebuffer is fixed at **1232 x 832** because this resolution was selected to preserve reliable JETANK license-plate recognition quality.
- Resolution reduction is therefore **not** considered a valid performance optimization in this experiment.
- Camera capture/streaming requests are capped at **60 FPS**, matching the practical stock Jetson Nano / IMX219 camera target. The simulation/render loop itself remains uncapped.
- The optimization target is the transfer/processing path itself: GPU readback and JPEG execution, while keeping image resolution constant.


## Streaming ON/OFF simulation baseline

Each branch must be measured in **both** camera-streaming states.

- `ROBOTPAL_STREAMING=0`: camera streaming OFF. No camera readback, JPEG encode, streaming queue, or streaming worker pool is initialized.
- unset or `ROBOTPAL_STREAMING=1`: camera streaming ON. Camera work is capped at 60 FPS.
- The simulation/render loop is uncapped in both states.
- NetworkEngine remains available in both states so the comparison isolates the camera-streaming workload rather than changing the rest of the application architecture.

For each branch, collect:

1. **Simulation FPS — Streaming OFF**
2. **Simulation FPS — Streaming ON**
3. **Camera Streaming Send FPS — Streaming ON**

The useful derived value is the streaming penalty:

```text
Simulation FPS penalty (%) =
(OFF FPS - ON FPS) / OFF FPS * 100
```

This penalty is a derived comparison value, not a separately instrumented metric.

### Reproducible Windows runs

Streaming OFF:

```powershell
$env:ROBOTPAL_STREAMING="0"
.\RobotPal.exe
```

Streaming ON:

```powershell
$env:ROBOTPAL_STREAMING="1"
.\RobotPal.exe
```

Run each state five times with the same scene, window, GPU, warm-up, and measurement duration.

## Measurement policy

Whole-application performance is measured externally with **PresentMon**.

The headline measurements are:

1. Simulation FPS — Streaming OFF
2. Simulation FPS — Streaming ON
3. Camera Streaming Send FPS — Streaming ON

Run the same RobotPal scenario five times and report the median result. Do not use an internal FPS counter or manual `std::chrono` accumulator as the final performance result.

**Tracy** provides the camera-stream send-rate measurement through the named frame set `CameraStreamSend`, emitted only after a JPEG packet is submitted to `NetworkEngine`. It is also used for bottleneck diagnosis. It is used to show where the main/render thread spends time (especially synchronous readback and JPEG encoding). Tracy-enabled runs are not used as the headline PresentMon result because profiler instrumentation adds overhead.

**Google Benchmark** is optional and isolated. The included target benchmarks the 1232 x 832, Q85 JPEG test path. Its result describes that code path only; it must not be presented as RobotPal application FPS.

Receiver/sink FPS is intentionally not part of this experiment. Send FPS means frames successfully encoded and submitted by RobotPal to `NetworkEngine`, not receiver decode FPS.

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


## Benchmark integrity gate

Do not accept performance numbers unless all checks below pass.

1. Use a **separate clean build directory for each branch**. Never reuse one build directory after switching branches.
2. At startup, record the `[PERF]` banner and verify that the executable matches the intended stage:
   - `stage=baseline-sync readback=sync-glReadPixels jpeg=main-thread`
   - `stage=pbo readback=pbo-nonblocking jpeg=main-thread`
   - `stage=pbo-mt readback=pbo-nonblocking jpeg=worker-pool-4`
3. Run Streaming OFF and ON **5 times each** after the same warm-up period and use the median.
4. Alternate run order where practical (OFF/ON/OFF/ON...) to reduce thermal and clock drift.
5. PresentMon headline runs and Tracy diagnostic runs should be separate. Use PresentMon runs for Simulation FPS; use Tracy runs for Camera Streaming Send FPS and bottleneck evidence.
6. The three branches share the same simulation workload when Streaming is OFF. Therefore the median Streaming-OFF Simulation FPS must be reasonably close across branches. If the max/min spread exceeds **5%**, treat the benchmark set as invalid and repeat after checking build identity, GPU selection, power state, window state, background load, and warm-up.
7. Do not infer a PBO win from API choice alone. A valid PBO result must show that `Streaming.Readback.PBO` avoids blocking waits and that any overall gain survives the extra CPU copy from mapped PBO memory.

The previous 3-run result set with Streaming-OFF FPS values of 103.95 / 92.29 / 100.54 fails the cross-branch OFF sanity check and must not be used as the final benchmark.
