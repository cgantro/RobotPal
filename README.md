# RobotPal
![Demo](./.github/assets/demo.webp)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/junwo/RobotPal/actions)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Web-blue)](https://github.com/junwo/RobotPal)
[![License](https://img.shields.io/badge/license-MIT-green)](./LICENSE)
[![Live Demo](https://img.shields.io/badge/Live_Demo-Try_It_Now!-brightgreen)](https://junwoo-seo-1998.github.io/RobotPal/)
![demoimg](./.github/assets/demoimg.png)

This README is also available in [Korean](#robotpal-korean).

## Table of Contents
- [Overview](#overview)
- [Live Demo](#live-demo)
- [Motivation](#motivation)
- [Key Features](#key-features)
- [Getting Started](#getting-started)
- [Contributing](#contributing)
- [License](#license)

## Overview

RobotPal is a virtual simulation environment designed for training and testing the JETANK robot arm. The project was born from the idea of ​​overcoming the limitations of training a robot arm in the real world, where it is difficult to physically change various situations and environments.

Inspired by simulators like Gazebo, RobotPal provides a space to freely experiment and validate robot arm movements and logic in a virtual environment. A key feature of this project is its cross-platform support; it runs on desktop and is also available as a web application through Emscripten.

## Live Demo

You can try RobotPal instantly in your browser without any installation!

**[>> Click here to launch the Live Demo <<](https://junwoo-seo-1998.github.io/RobotPal/)**

## Motivation

Training robotic systems like JETANK in the real world presents significant challenges:
- **Physical Constraints:** Preparing various objects and environments is costly and time-consuming.
- **Safety Issues:** Repetitive testing can lead to wear and tear on the hardware, and incorrect operation can cause damage or safety accidents.
- **Lack of Repetitiveness:** It is difficult to perfectly reproduce the same situation, which makes precise testing and performance evaluation difficult.

RobotPal was created to address these challenges. By moving the training and testing process to a virtual environment, we can create any desired scenario at no cost, conduct tests safely, and ensure consistent results through repetition.

## Key Features

- **Realistic Simulation:** Provides a virtual environment to test the JETANK robot arm's movements and controls.
- **Cross-Platform:** Supports major desktop operating systems such as Windows, Linux, and macOS.
- **Web-Based Accessibility:** Built with Emscripten, it can be run directly in a web browser without any special installation.
- **Modular Architecture:** The code is structured to easily add new features or change existing ones.

## Getting Started

### Pre-built Releases (Easiest Way)

For the easiest access, you can download the latest pre-built version for your operating system from the **[Releases](https://github.com/Junwoo-Seo-1998/RobotPal/releases)** page. This allows you to run the application without building it from the source code.

### Build from Source

#### Prerequisites

- Git
- C++ Compiler (MSVC, GCC, Clang)
- CMake (version 3.16 or higher)
- Emscripten SDK (for web builds)

#### Desktop Build

```bash
# Clone the repository
git clone https://github.com/Junwoo-Seo-1998/RobotPal.git
cd RobotPal

# Configure and build the project
cmake -B build
cmake --build build

# Run the application
./build/RobotPal
```

#### Web (Emscripten) Build

This is for testing your own local changes. For a quick look, please use the [Live Demo](#live-demo) link above.

```bash
# Make sure the Emscripten SDK environment is activated
# e.g., source ./emsdk_env.sh

# Clone the repository
git clone https://github.com/Junwoo-Seo-1998/RobotPal.git
cd RobotPal

# Configure and build the project for the web
emcmake cmake -B build-web -S .
cmake --build build-web

# Run a local server in the build-web directory to view the results
cd build-web
python -m http.server
# Open your browser and go to http://localhost:8000/index.html
```

## Camera-task verification and frame-rate interpretation

RobotPal provides a JetBot-compatible Python camera interface (`RobotPal-python/src/robotpal/camera/virtual_camera.py`). Its default configuration requests 224×224 at 30 FPS; this setting does **not** establish the actual received or analyzed FPS.

During project trials, **line following and license-plate recognition operated successfully** with the virtual camera setup (participant-reported functional result). The repository does not establish numerical recognition accuracy, Jetson Nano inference throughput, end-to-end latency, or the frame rate at which each task was verified.

The separate **1232×832** desktop benchmark measures simulation updates and camera **sending**, not Jetson inference. It used 60 FPS as an **upper performance target**, not as a proven requirement for line following or plate recognition. A configured 30/40/60 FPS permits about 33.3/25.0/16.7 ms per frame respectively; the achievable analysis rate depends on the model, resolution, preprocessing, and hardware. NVIDIA has reported substantially different Jetson Nano end-to-end license-plate processing rates across pipelines ([9.2 FPS full pipeline](https://developer.nvidia.com/blog/?p=23943); [40 FPS project example](https://developer.nvidia.com/embedded/community/jetson-projects/real_time-alpr_jetson_nano)). Neither figure is a RobotPal measurement.

The streaming benchmark results and limitations are recorded in [the performance report](./docs/streaming-performance-result.md).

## Contributing

Contributions are always welcome! If you would like to contribute to RobotPal, please fork the repository and create a pull request. You can also open an issue to report bugs or suggest new features.

## Performance Benchmark Guide

For a reproducible streaming bottleneck test setup, see `docs/perf/streaming_bottleneck_test_setup.md`.

## Contributing

Contributions are always welcome! If you would like to contribute to RobotPal, please fork the repository and create a pull request. You can also open an issue to report bugs or suggest new features.

## License

This project is licensed under the MIT License. See the `LICENSE` file for more details.

## Performance Note: PBO-based Asynchronous Readback

When sending rendered frames over network streaming, one expensive step is reading pixel data from GPU memory back to CPU memory (`glReadPixels`).

- **Synchronous readback (direct pointer):** CPU may block until GPU rendering finishes.
- **PBO readback (pixel pack buffer):** GPU writes into a buffer object first, and CPU maps a previously written buffer in the next frame.

In RobotPal, the desktop path uses **double-buffered PBO ping-pong** in `Texture::GetAsyncData`.

1. Frame N: issue `glReadPixels(..., 0)` to `PBO[writeIndex]` (GPU-side write request)
2. Frame N: map `PBO[readIndex]` and copy previous frame data on CPU
3. Swap indices and repeat

This does not remove all stalls, but it typically reduces CPU-GPU synchronization pressure versus immediate readback.

---

# RobotPal (Korean)
![Demo](./.github/assets/demo.webp)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/junwo/RobotPal/actions)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Web-blue)](https://github.com/junwo/RobotPal)
[![License](https://img.shields.io/badge/license-MIT-green)](./LICENSE)
[![Live Demo](https://img.shields.io/badge/Live_Demo-Try_It_Now!-brightgreen)](https://junwoo-seo-1998.github.io/RobotPal/)
![demoimg](./.github/assets/demoimg.png)

이 README는 [영문](#robotpal)으로도 제공됩니다.

## 목차
- [프로젝트 개요](#프로젝트-개요)
- [라이브 데모](#라이브-데모)
- [개발 동기](#개발-동기)
- [주요 특징](#주요-특징)
- [시작하기](#시작하기)
- [기여하기](#기여하기)
- [라이선스](#라이선스)

## 프로젝트 개요

RobotPal은 JETANK 로봇팔의 훈련과 테스트를 위해 설계된 가상 시뮬레이션 환경입니다. 현실 세계에서 로봇팔을 훈련시키는 것은 다양한 상황과 환경을 물리적으로 바꿔가며 실험하기 어렵다는 한계점에서 아이디어를 얻어 시작된 프로젝트입니다.

Gazebo와 같은 시뮬레이터에서 영감을 받아, 가상 환경에서 자유롭게 로봇팔의 움직임과 로직을 실험하고 검증할 수 있는 공간을 제공합니다. 이 프로젝트의 가장 큰 특징은 크로스플랫폼을 지원한다는 점입니다. 데스크톱은 물론, Emscripten을 통해 웹 애플리케이션으로도 제공됩니다.

## 라이브 데모

설치 없이 웹 브라우저에서 RobotPal을 즉시 체험해볼 수 있습니다!

**[>> 여기를 클릭하여 라이브 데모 실행하기 <<](https://junwoo-seo-1998.github.io/RobotPal/)**

## 개발 동기

JETANK와 같은 로봇 시스템을 현실 세계에서 훈련하는 것은 다음과 같은 상당한 어려움이 따릅니다.
- **물리적 제약:** 다양한 사물과 환경을 구성하는 데 많은 비용과 시간이 소요됩니다.
- **안전 문제:** 반복적인 테스트는 하드웨어의 마모를 유발할 수 있으며, 잘못된 작동은 파손이나 안전사고로 이어질 수 있습니다.
- **반복성의 부재:** 동일한 상황을 완벽하게 재현하기 어려워 정밀한 테스트와 성능 평가가 힘듭니다.

RobotPal은 이러한 문제들을 해결하기 위해 만들어졌습니다. 훈련 및 테스트 과정을 가상 환경으로 옮김으로써, 원하는 모든 시나리오를 비용 없이 구성하고, 안전하게 테스트를 수행하며, 일관된 반복 실험을 보장할 수 있습니다.

## 주요 특징

- **사실적인 시뮬레이션:** JETANK 로봇팔의 움직임과 제어를 테스트할 수 있는 가상 환경을 제공합니다.
- **크로스플랫폼:** Windows, Linux, macOS 등 주요 데스크톱 운영체제를 모두 지원합니다.
- **웹 기반 접근성:** Emscripten으로 빌드되어 별도의 설치 없이 웹 브라우저에서 바로 실행할 수 있습니다.
- **모듈식 아키텍처:** 새로운 기능을 추가하거나 기존 기능을 변경하기 용이하도록 코드가 구조화되어 있습니다.

## 시작하기

### 사전 빌드된 릴리스 (가장 쉬운 방법)

가장 쉽게 사용하는 방법은 **[Releases](https://github.com/Junwoo-Seo-1998/RobotPal/releases)** 페이지에서 자신의 운영체제에 맞는 최신 버전을 다운로드하는 것입니다. 소스 코드를 직접 빌드하지 않고도 애플리케이션을 바로 실행할 수 있습니다.

### 소스 코드로 직접 빌드하기

#### 필수 설치 항목

- Git
- C++ 컴파일러 (MSVC, GCC, Clang)
- CMake (3.16 버전 이상)
- Emscripten SDK (웹 빌드를 위한)

#### 데스크톱 빌드

```bash
# 저장소 클론
git clone https://github.com/Junwoo-Seo-1998/RobotPal.git
cd RobotPal

# 프로젝트 구성 및 빌드
cmake -B build
cmake --build build

# 애플리케이션 실행
./build/RobotPal
```

#### 웹 (Emscripten) 빌드

이 방법은 직접 수정한 코드를 테스트할 때 사용합니다. 간단한 체험은 상단의 [라이브 데모](#라이브-데모) 링크를 이용해주세요.

```bash
# Emscripten SDK 환경이 활성화되었는지 확인
# 예: source ./emsdk_env.sh

# 저장소 클론
git clone https://github.com/Junwoo-Seo-1998/RobotPal.git
cd RobotPal

# 웹을 위한 프로젝트 구성 및 빌드
emcmake cmake -B build-web -S .
cmake --build build-web

# build-web 디렉토리에서 로컬 서버를 실행하여 결과 확인
cd build-web
python -m http.server
# 브라우저를 열고 http://localhost:8000/index.html 로 이동
```

## 기여하기

기여는 언제나 환영입니다! RobotPal에 기여하고 싶다면, 저장소를 포크(fork)하고 풀 리퀘스트(pull request)를 생성해주세요. 또한 버그를 보고하거나 새로운 기능을 제안하기 위해 이슈(issue)를 열 수도 있습니다.

## 라이선스

이 프로젝트는 MIT 라이선스를 따릅니다. 자세한 내용은 `LICENSE` 파일을 참고하세요.

## 카메라 활용 결과와 FPS 해석

가상 카메라 영상을 이용한 **라인트레이싱과 번호판 인식이 실제 실습에서 정상 동작했음**을 확인했습니다(프로젝트 참여자의 기능 확인). 다만 해당 실습의 인식률, 제어 주기, 추론 FPS, 종단 간 지연은 정량 측정 자료가 없어 수치로 주장하지 않습니다. Jetson Nano에서 실행한 알고리즘의 처리량을 RobotPal의 송신 FPS와 동일하게 해석하지도 않습니다.

Python 가상 카메라 어댑터(`RobotPal-python/src/robotpal/camera/virtual_camera.py`)는 JetBot 호환 인터페이스를 제공하며 기본 **224×224, 설정 30 FPS**를 사용합니다. 이 값은 설정값일 뿐 수신·추론 속도를 측정한 결과는 아닙니다.

| 카메라 설정 프레임률 | 프레임당 시간 | 해석 |
| --- | ---: | --- |
| 30 FPS | 약 33.3ms | 실시간 처리 여부를 확인하기 위한 합리적인 출발점. 안정성은 별도 측정 필요 |
| 40 FPS | 25.0ms | 더 자주 영상을 받을 수 있지만, 실제 인식·제어 속도는 알고리즘 성능에 좌우됨 |
| 60 FPS | 약 16.7ms | 프레임 수집이 가능해도 매 프레임 추론·제어가 같은 주기로 수행된다는 보장은 없음 |

NVIDIA 공개 사례에서도 Jetson Nano의 번호판 관련 처리량은 작업 범위에 따라 달랐습니다. [번호판 인식 프로젝트 사례](https://developer.nvidia.com/embedded/community/jetson-projects/real_time-alpr_jetson_nano)는 40FPS를 보고했지만, [차량 탐지부터 번호판 인식까지 포함한 전체 파이프라인](https://developer.nvidia.com/blog/?p=23943)은 9.2FPS였습니다. 어느 쪽도 RobotPal 실습 결과로 인용할 수 없습니다.

아래 **1232×832** 성능 시험은 시뮬레이터의 **영상 송신률**과 **시뮬레이션 갱신률**을 비교한 것입니다. 60FPS는 송신 경로의 상향 목표였으며, 라인트레이싱이나 번호판 인식에 반드시 필요한 프레임률로 검증한 값이 아닙니다.

## 성능 노트: 스트리밍 병목

최신 1232×832 벤치마크는 동기 Readback, PBO 단독, PBO + JPEG 4 Worker 구조를 비교했으며, 각 조건의 Streaming OFF/ON을 5회 측정해 median을 사용했습니다.

Tracy로 계측한 결과 초기 가설과 달리 GPU Readback이 주된 Main Thread 병목은 아니었습니다. Sync 조건에서 Readback p50은 **1.144 ms**였지만 JPEG 압축 p50은 **16.406 ms**였습니다. 따라서 PBO 단독 적용만으로는 애플리케이션 전체 성능이 개선되지 않았습니다.

실제 효과가 컸던 변경은 JPEG 압축을 simulation critical path에서 분리한 것입니다. PBO + JPEG 4 Worker 구조에서:

- Streaming ON Simulation FPS: **66.70 → 96.01** (+43.9%)
- Camera Send FPS: **26.95 → 40.05** (+48.6%)
- Streaming Penalty: **34.84% → 6.74%** (-28.1%p)

최종 카메라 송신률 **40.05FPS**는 송신 경로의 성능이며, 외부 영상 인식의 실제 처리 FPS와 구분합니다. 60FPS 송신이라는 상향 목표에는 도달하지 못했습니다. 다음 개선 대상은 JPEG 인코더 처리량, 불필요한 CPU 메모리 복사, 4 Worker 처리 구조입니다.

최신 기준 수치와 한계는 [docs/streaming-performance-result.md](./docs/streaming-performance-result.md)를 사용합니다.
