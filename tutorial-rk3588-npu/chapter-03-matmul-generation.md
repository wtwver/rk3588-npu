# Chapter 3 — Generating Matmul Workloads

`include/npu_matmul.h` exposes a small API to generate command buffers for FP16 and INT8 matmul on the RK3588 NPU.

## Key Types and Functions
```c
typedef struct {
  uint16_t m, k, n;            // matrix dimensions
  uint32_t input_dma;          // A (features)
  uint32_t weights_dma;        // B (weights)
  uint32_t output_dma;         // C (output)
  uint64_t *tasks;             // task buffer (PC ops)
  uint8_t  fp32tofp16;         // output cvt flag
} matmul_params_t;

int gen_matmul_fp16(matmul_params_t *params);
int gen_matmul_int8(matmul_params_t *params);
```

## Steps

1) Prepare device buffers (see Chapter 2)
- Input A: size `m*k*sizeof(_Float16)`
- Weights B: size `n*k*sizeof(_Float16)` (FP16) or `n*k*sizeof(int8_t)` (INT8)
- Output C: size `m*n*sizeof(float)` for FP16 path (accumulate in fp32) or `m*n*sizeof(int8_t)` for INT8 path depending on config

2) Fill matrices in row-major layout matching tests
- A index: `i*k + l`
- B index: `j*k + l` (note: B is arranged by columns of output)

3) Create parameters and call generator
```c
matmul_params_t p = {
  .m = M, .k = K, .n = N,
  .input_dma = (uint32_t)input_dma,
  .weights_dma = (uint32_t)weights_dma,
  .output_dma = (uint32_t)output_dma,
  .tasks = (uint64_t*)tasks,
  .fp32tofp16 = 0, // keep output in fp32 by default
};
int rc = gen_matmul_fp16(&p);
```

4) Submit tasks via DRM ioctl as shown in tests (next chapter)

## ASCII Diagram — Matmul Dataflow
```
A (M x K)   x   B (N x K)^T   -->   C (M x N)

   input_dma       weights_dma             output_dma
       |                |                        |
       v                v                        v
     CNA/Core ------ configure -------> DPU ---- write ---> memory
```

## Constraints (mirrors tests)
- FP16: `M` in {1 or multiple of 4}, `K` multiple of 32, `N` multiple of 16
- Respect maximums seen in tests: M<=384, K<=4096, N<=4096 (adjust per hardware)

## Next
Proceed to Chapter 4 to submit and synchronize tasks with the kernel driver.