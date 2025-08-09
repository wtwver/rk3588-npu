#ifndef LLAMA2_NPU_H
#define LLAMA2_NPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Thin context to reuse regcmd/task buffers across matmul calls
// Intended to be long-lived during a llama2.c run

typedef struct {
  int fd;

  // Command buffer for register programming
  uint64_t *regcmd;
  uint64_t regcmd_dma;
  uint64_t regcmd_obj;
  uint32_t regcmd_handle;

  // Task descriptor buffer (kernel mapped)
  struct rknpu_task *tasks;
  uint64_t tasks_dma;
  uint64_t tasks_obj;
  uint32_t tasks_handle;
} npu_ctx_t;

// Initialize the context (opens device, allocates regcmd/tasks buffers)
int npu_ctx_init(npu_ctx_t *ctx);

// Destroy the context and free resources
void npu_ctx_destroy(npu_ctx_t *ctx);

// Run a single FP16 matmul on the NPU: C[M,N] = A[M,K] x B[N,K]^T with B given as [N,K]
// Inputs:
//  - A: row-major MxK (_Float16)
//  - B: row-major NxK (_Float16), same layout as llama2.c weights for linear layers
// Output:
//  - C: row-major MxN (_Float16) if output_fp16 != 0, otherwise values are converted back to fp32 then stored as fp16 bit-patterns
// Returns 0 on success, <0 on error
int npu_matmul_fp16_run(npu_ctx_t *ctx,
                        int M, int K, int N,
                        const _Float16 *A,
                        const _Float16 *B,
                        _Float16 *C,
                        int output_fp16);

#ifdef __cplusplus
}
#endif

#endif // LLAMA2_NPU_H