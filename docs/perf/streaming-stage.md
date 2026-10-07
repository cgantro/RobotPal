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


## Worker-queue stability

The PBO+MT stage uses four JPEG workers and a queue bounded to 8 frames. If the encoder cannot sustain the 60 FPS camera input target, the oldest queued frame is discarded before enqueueing the newest frame.

This bound is required to keep memory usage and latency stable during long measurements. It is not treated as a separate performance stage; Camera Streaming Send FPS still measures completed encoded frames submitted by the worker path.
