# Streaming Bottleneck Test Environment (RobotPal)

This document explains how to reproduce the streaming bottleneck benchmark environment used to compare frame-drop behavior before and after pipeline separation.

## 1) Goal

Measure frame-drop rate under a controlled streaming load where these hot paths are exercised:

- RGBA -> RGB conversion
- JPEG encoding (`CreateJpegEncoder` / libjpeg)
- Producer/consumer queue pressure
- Single worker (before) vs multi worker (after)

Benchmark source:

- `tests/streaming_frame_drop_benchmark.cpp`

---

## 2) Test Host Requirements

### OS / Toolchain

- Linux/macOS/Windows (with a C++17 compiler)
- `g++` or `clang++`
- pthread support
- `libjpeg` development package

### Suggested package install (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y build-essential libjpeg-dev
```

---

## 3) Build Command

From repository root:

```bash
c++ -O2 -std=c++17 \
  tests/streaming_frame_drop_benchmark.cpp \
  RobotPal/src/Util/JpegEncoder_libjpeg.cpp \
  -I./RobotPal/include -pthread -ljpeg \
  -o tests/streaming_frame_drop_benchmark
```

---

## 4) Runtime Parameters

The benchmark accepts the following arguments:

```text
./tests/streaming_frame_drop_benchmark \
  <width> <height> <input_fps> <duration_sec> [quality] [queue_size]
```

- `width`, `height`: frame resolution
- `input_fps`: producer target FPS
- `duration_sec`: benchmark duration in seconds
- `quality` (optional, default 70): JPEG quality
- `queue_size` (optional, default 6): bounded queue size (drop-oldest policy when full)

---

## 5) Baseline Scenario Used for Bottleneck Check

Requested scenario:

- Resolution: `1632 x 1232`
- Duration: `30 sec`
- Input FPS: `60`
- JPEG quality: `70`
- Queue size: `6`

Run:

```bash
./tests/streaming_frame_drop_benchmark 1632 1232 60 30 70 6
```

Output includes:

- `before(single-worker)`
- `after(multi-worker)`
- produced / processed / dropped
- drop_rate / input_fps / output_fps / elapsed

---

## 6) Metric Definition

### Frame drop rate

```text
drop_rate = dropped / produced * 100
```

This benchmark models real-time pressure by dropping oldest queued frames when the bounded queue is full.

### Before vs After meaning

- **before(single-worker)**: one encoding worker (represents old bottleneck-prone path)
- **after(multi-worker)**: multiple encoding workers (represents separated/asynchronous pipeline)

---

## 7) Reproducibility Notes

- Results vary by CPU core count, memory bandwidth, and libjpeg implementation.
- Compare **relative improvement (before vs after)** on the same machine.
- For fair comparison, keep all arguments identical except worker strategy.

---

## 8) Cleanup

```bash
rm -f tests/streaming_frame_drop_benchmark
```
