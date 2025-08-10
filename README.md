# rk3588-npu

Reverse engineering notes and a minimal C library to drive the RK3588 NPU via the DRM rknpu interface. Includes FP16 and INT8 matrix multiplication (GEMM) examples and tests.

Status: experimental. Tested primarily on Linux kernel 5.10 for RK3588. APIs and behavior may change.

## Contents
- Library: `lib rk3588-npu` built from `src/` and public headers in `include/`
- Tests/Examples (built binaries):
  - `matmul_4_36_16`
  - `matmul_fp16` (A: FP16, B: FP16, C: FP32)
  - `matmul_int8` (A: INT8, B: INT8, C: INT32)
  - `matmul_fp16_fp16` (A: FP16, B: FP16, C: FP16)

## Requirements
- RK3588 device with rknpu DRM driver (`/dev/dri/card1`) available
- Linux kernel 5.10 (tested). 6.1 may work but is not validated here
- Build tools: `meson`, `ninja`
- Dependencies: `libdrm` headers

Install on Debian/Ubuntu:
```bash
sudo apt update
sudo apt install -y build-essential meson ninja-build pkg-config libdrm-dev
```

## Build
```bash
mkdir build
meson build
ninja -C build
```

## Run tests
```bash
ninja -C build test
```
Tests invoke the example binaries with representative shapes. They require an RK3588 host with a working rknpu device.

## Example binaries and usage
All binaries take dimensions in the order M K N (row x inner x col).

- FP16 inputs, FP32 output:
  ```bash
  ./build/matmul_fp16 4 1024 1024
  ```
- INT8 inputs, INT32 output:
  ```bash
  ./build/matmul_int8 4 1024 1024
  ```
- FP16 inputs, FP16 output:
  ```bash
  ./build/matmul_fp16_fp16 1 768 2048
  ```

### Dimension constraints
Current generator is simplified and limited by on-chip CBUF capacity:
- M: 1 or a multiple of 4. Max tested: 384 for FP16, 544 for INT8
- K: multiple of 32. Max tested: 4096 for FP16/INT8; FP16→FP16 tests up to 8192
- N: multiple of 16 (or 1 for special cases). Max tested: 4096 (FP16/INT8) and 8192 (FP16→FP16)
If limits are exceeded, the generator returns an error (e.g. `-1`/`-2`).

## Programming model (API)
Public headers are in `include/`.

- Device and memory helpers (`include/npu_interface.h`):
  ```c
  void* mem_allocate(int fd, size_t size, uint64_t *dma_addr, uint64_t *obj, uint32_t flags, uint32_t *handle);
  void  mem_destroy(int fd, uint32_t handle, uint64_t obj_addr);

  int npu_open();
  int npu_close(int fd);
  int npu_reset(int fd);
  ```
  - `npu_open` expects `/dev/dri/card1` to be the rknpu node.
  - `mem_allocate` returns a CPU mapping and fills DMA/obj/handle for submissions.

- Matmul generator (`include/npu_matmul.h`):
  ```c
  typedef struct {
    uint16_t  m, k, n;          // GEMM dimensions
    uint32_t  input_dma;        // A (feature) DMA addr
    uint32_t  weights_dma;      // B (weights) DMA addr
    uint32_t  output_dma;       // C output DMA addr
    uint64_t *tasks;            // output: PC/reg command buffer (>=112 u64s)
    uint8_t   fp32tofp16;       // 0: FP32 out, 1: FP16 out
  } matmul_params_t;

  int gen_matmul_fp16(matmul_params_t *params);
  int gen_matmul_int8(matmul_params_t *params);

  // Helpers to compute packed offsets for inputs/weights
  int feature_data(int C, int H, int W, int C2, int c, int h, int w);
  int weight_fp16(int C, int k, int c);
  int weight_int8(int C, int k, int c);
  ```

### Data layout and packing
The hardware expects packed layouts. Use the helper indexers when filling buffers:
- Feature map A (shape MxK, treated as H=M, W=1, C=K)
  - FP16: pack with `feature_data(K, M, 1, 8, k, m, 1)`
  - INT8: pack with `feature_data(K, M, 1, 16, k, m, 1)`
- Weights B (shape NxK, kernels=N, channels=K)
  - FP16: use `weight_fp16(K, n, k)`
  - INT8: use `weight_int8(K, n, k)`

Example (FP16 inputs, FP32 output):
```c
_Float16 *a = input;   // size M*K
_Float16 *b = weights; // size N*K
// Fill A
for (int m = 1; m <= M; ++m)
  for (int k = 1; k <= K; ++k)
    ((_Float16*)input)[feature_data(K, M, 1, 8, k, m, 1)] = a[(m-1)*K + (k-1)];
// Fill B
for (int n = 1; n <= N; ++n)
  for (int k = 1; k <= K; ++k)
    ((_Float16*)weights)[weight_fp16(K, n, k)] = b[(n-1)*K + (k-1)];
```

### Minimal flow
1) `fd = npu_open()`; `npu_reset(fd)`
2) Allocate PC/regcmd buffer, task buffer, and input/weight/output buffers via `mem_allocate`
3) Fill input and weight buffers using the helpers above
4) Prepare `matmul_params_t` and call `gen_matmul_*`
5) Copy generated `params.tasks` into your regcmd/PC buffer and submit via `DRM_IOCTL_RKNPU_SUBMIT`

See `tests/` sources for end-to-end usage.

## Troubleshooting
- Known issue (repro):
  ```
  ninja -C build test
  ./matmul_fp16_fp16 1 8192 $((16*485))
  Segmentation fault
  ```
  Track this scenario in `tests/matmul_fp16_fp16.c`. It likely hits CBUF/layout limits or output stride edge cases.
- Ensure `/dev/dri/card1` exists and belongs to rknpu. Run as root or set udev rules if needed.
- Kernel/user-space mismatch can cause IOCTL errors. Verify `rknpu-ioctl.h` matches your kernel’s rknpu driver.
- Check `dmesg` for rknpu faults and `errno` prints from helpers.

## Example: llama2.c integration
A simple integration of `llama2.c` TinyStories is described here and in the blog post linked below:
```bash
git clone https://github.com/karpathy/llama2.c
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin
cd llama2.c
make run
./run stories110M.bin
```

## Background and blog posts
Internals and examples are covered in:
- RK3588 NPU internals, NVDLA similarities, matmul support: [blog post](http://jas-hacks.blogspot.com/2024/02/rk3588-reverse-engineering-rknn.html)
- Example running TinyStories with llama2.c: [blog post](http://jas-hacks.blogspot.com/2024/05/rk3588-reverse-engineering-rknn-running.html)

## License
GPL-3.0-or-later. See `LICENSE`.
