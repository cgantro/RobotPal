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
- Camera framebuffer: fixed 1232 x 832 requirement for license-plate recognition
- NetworkEngine I/O threads: preserved as part of the original completed networking implementation

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

Run the same scenario three times and report the median. Do not use an internal FPS counter or manual `std::chrono` accumulator as the final result.

**Tracy** provides the camera-stream send-rate measurement through the named frame set `CameraStreamSend`, emitted only after a JPEG packet is submitted to `NetworkEngine`. It is also used for bottleneck diagnosis. Use it to inspect the `Frame`, `Streaming.Readback.PBO`, `Streaming.SendFrame`, and `Streaming.JPEG` zones. Tracy-enabled runs are not the headline PresentMon runs.

**Google Benchmark** is optional and isolated. The included JPEG benchmark is a control for the same 1232 x 832 Q85 JPEG test path; it is not RobotPal FPS.

Receiver/sink FPS is intentionally excluded. Send FPS means frames successfully encoded and submitted by RobotPal to `NetworkEngine`, not receiver decode FPS.

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
