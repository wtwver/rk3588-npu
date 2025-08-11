# Chapter 1 — Project Overview and Build

This project is a C library and tests for driving the Rockchip RK3588 NPU (via DRM ioctl), with a focus on generating and submitting matrix-multiply (matmul) workloads. It uses Meson to build a library `rk3588-npu` and several test executables.

## Layout
- `meson.build`: project and test targets
- `include/`: hardware and API headers
- `src/`: library sources (`npu_interface.c`, `npu_matmul.c`)
- `tests/`: sample/test executables (FP16/INT8 matmul)
- `blog/`: background notes and reverse engineering writeups

## Build
```bash
# From the project root
meson setup build
meson compile -C build
meson test -C build | cat
```

If `libdrm` headers are missing, install your distro's `libdrm-dev` package.

## Run example tests
```bash
# Runs a small fp16 case (configured in meson.build)
meson test -C build 'matmul fp16 1x32x16' | cat
```

## High-Level Architecture
- `npu_interface.[ch]`: open/close device, allocate/free DMA buffers, reset NPU
- `npu_matmul.[ch]`: build command sequences (CNA/Core/DPU) for matmul, generate tasks
- `tests/matmul_*.c`: allocate buffers, fill input/weights, invoke generators, submit

## ASCII Diagram — Module Overview
```
+---------------------+         +----------------------+         +------------------+
|  tests/matmul_*.c  |  uses   |  npu_matmul.[ch]     |  uses   |  npu_interface.c |
|  (apps/examples)   +--------->  (command builder)   +--------->  (DRM ioctl, DMA) |
+---------------------+         +----------------------+         +------------------+
                                                                ^
                                                                |
                                                        +---------------+
                                                        |  /dev/dri/*   |
                                                        |  (rknpu drm)  |
                                                        +---------------+
```

## Next
Proceed to Chapter 2 for device I/O: opening the NPU, memory allocation, and cleanup.