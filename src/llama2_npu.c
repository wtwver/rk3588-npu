#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <libdrm/drm.h>

#include "rknpu-ioctl.h"
#include "npu_interface.h"
#include "npu_matmul.h"
#include "llama2_npu.h"

// Internal helper to submit a single pre-filled regcmd as one task
static int npu_submit_once(npu_ctx_t *ctx, size_t reg_count) {
  // Fill one task descriptor
  ctx->tasks[0].flags  = 0;
  ctx->tasks[0].op_idx = 0;
  ctx->tasks[0].enable_mask = 0xd;
  ctx->tasks[0].int_mask = 0x300; // wait for DPU to finish
  ctx->tasks[0].int_clear = 0x1ffff;
  ctx->tasks[0].int_status = 0;
  ctx->tasks[0].regcfg_amount = reg_count - (RKNPU_PC_DATA_EXTRA_AMOUNT+4);
  ctx->tasks[0].regcfg_offset = 0;
  ctx->tasks[0].regcmd_addr = ctx->regcmd_dma;

  struct rknpu_submit submit = {
    .flags = RKNPU_JOB_PC | RKNPU_JOB_BLOCK | RKNPU_JOB_PINGPONG,
    .timeout = 6000,
    .task_start = 0,
    .task_number = 1,
    .task_counter = 0,
    .priority = 0,
    .task_obj_addr = ctx->tasks_obj,
    .regcfg_obj_addr = 0,
    .task_base_addr = 0,
    .user_data = 0,
    .core_mask = 1,
    .fence_fd = -1,
    .subcore_task = { // Only use core 1
      { .task_start = 0, .task_number = 1 },
      { 1, 0 }, { 2, 0 }, { 0, 0 }, { 0, 0 }
    },
  };

  int ret = ioctl(ctx->fd, DRM_IOCTL_RKNPU_SUBMIT, &submit);
  return ret;
}

int npu_ctx_init(npu_ctx_t *ctx) {
  memset(ctx, 0, sizeof(*ctx));

  int fd = npu_open();
  if (fd < 0) return -1;
  ctx->fd = fd;

  // Allocate regcmd buffer (room for 112 uint64 regs)
  ctx->regcmd = mem_allocate(ctx->fd, 1024, &ctx->regcmd_dma, &ctx->regcmd_obj, 0, &ctx->regcmd_handle);
  if (ctx->regcmd == NULL) return -2;

  // Allocate kernel-mapped tasks buffer
  ctx->tasks = mem_allocate(ctx->fd, 1024, &ctx->tasks_dma, &ctx->tasks_obj, RKNPU_MEM_KERNEL_MAPPING, &ctx->tasks_handle);
  if (ctx->tasks == NULL) return -3;

  // Reset NPU once
  npu_reset(ctx->fd);
  return 0;
}

void npu_ctx_destroy(npu_ctx_t *ctx) {
  if (ctx->regcmd) {
    munmap(ctx->regcmd, 1024);
    mem_destroy(ctx->fd, ctx->regcmd_handle, ctx->regcmd_obj);
  }
  if (ctx->tasks) {
    munmap(ctx->tasks, 1024);
    mem_destroy(ctx->fd, ctx->tasks_handle, ctx->tasks_obj);
  }
  if (ctx->fd > 0) {
    npu_close(ctx->fd);
  }
  memset(ctx, 0, sizeof(*ctx));
}

int npu_matmul_fp16_run(npu_ctx_t *ctx,
                        int M, int K, int N,
                        const _Float16 *A,
                        const _Float16 *B,
                        _Float16 *C,
                        int output_fp16) {
  if (M <= 0 || K <= 0 || N <= 0) return -10;
  if (!A || !B || !C) return -11;

  // Constraints per current generator
  if (!(((M % 4) == 0) || (M == 1))) return -12;
  if ((K % 32) != 0) return -13;
  if (!(((N % 16) == 0) || (N == 1))) return -14;

  // Allocate device buffers for this call
  uint64_t input_dma = 0, input_obj = 0;
  uint32_t input_handle = 0;
  void *input = mem_allocate(ctx->fd, (size_t)M * (size_t)K * sizeof(_Float16), &input_dma, &input_obj, 0, &input_handle);
  if (!input) return -20;

  uint64_t weights_dma = 0, weights_obj = 0;
  uint32_t weights_handle = 0;
  void *weights = mem_allocate(ctx->fd, (size_t)N * (size_t)K * sizeof(_Float16), &weights_dma, &weights_obj, 0, &weights_handle);
  if (!weights) { mem_destroy(ctx->fd, input_handle, input_obj); return -21; }

  uint64_t output_dma = 0, output_obj = 0;
  uint32_t output_handle = 0;
  void *output = mem_allocate(ctx->fd, (size_t)M * (size_t)N * sizeof(_Float16), &output_dma, &output_obj, 0, &output_handle);
  if (!output) {
    mem_destroy(ctx->fd, input_handle, input_obj);
    mem_destroy(ctx->fd, weights_handle, weights_obj);
    return -22;
  }

  // Prepare task register programming into a local array then copy to regcmd
  uint64_t npu_regs[112] = {0};

  matmul_params_t params;
  params.m = (uint16_t)M;
  params.k = (uint16_t)K;
  params.n = (uint16_t)N;
  params.input_dma = (uint32_t)input_dma;   // driver expects 32-bit dma window
  params.weights_dma = (uint32_t)weights_dma;
  params.output_dma = (uint32_t)output_dma;
  params.tasks = (uint64_t *)&npu_regs[0];
  params.fp32tofp16 = output_fp16 ? 1 : 0;

  int ret = gen_matmul_fp16(&params);
  if (ret != 0) {
    mem_destroy(ctx->fd, input_handle, input_obj);
    mem_destroy(ctx->fd, weights_handle, weights_obj);
    mem_destroy(ctx->fd, output_handle, output_obj);
    return ret;
  }

  memcpy(ctx->regcmd, npu_regs, sizeof(npu_regs));

  // Pack A (feature data) into NPU layout
  _Float16 *feature_data_fp16 = (_Float16 *)input;
  memset(feature_data_fp16, 0, (size_t)M * (size_t)K * sizeof(_Float16));
  for (int m = 1; m <= M; m++) {
    for (int k = 1; k <= K; k++) {
      feature_data_fp16[feature_data(K, M, 1, 8, k, m, 1)] = A[(size_t)(m - 1) * (size_t)K + (size_t)(k - 1)];
    }
  }

  // Pack B (weights) into NPU layout
  _Float16 *weights_fp16 = (_Float16 *)weights;
  memset(weights_fp16, 0, (size_t)N * (size_t)K * sizeof(_Float16));
  for (int n = 1; n <= N; n++) {
    for (int k = 1; k <= K; k++) {
      weights_fp16[weight_fp16(K, n, k)] = B[(size_t)(n - 1) * (size_t)K + (size_t)(k - 1)];
    }
  }

  // Submit
  ret = npu_submit_once(ctx, sizeof(npu_regs) / sizeof(uint64_t));
  if (ret < 0) {
    mem_destroy(ctx->fd, input_handle, input_obj);
    mem_destroy(ctx->fd, weights_handle, weights_obj);
    mem_destroy(ctx->fd, output_handle, output_obj);
    return ret;
  }

  // Read back output and unpack into row-major C
  _Float16 *output_data = (_Float16 *)output;
  for (int m = 1; m <= M; m++) {
    for (int n = 1; n <= N; n++) {
      C[(size_t)(m - 1) * (size_t)N + (size_t)(n - 1)] = output_data[feature_data(N, M, 1, 8, n, m, 1)];
    }
  }

  // Free device buffers
  mem_destroy(ctx->fd, input_handle, input_obj);
  mem_destroy(ctx->fd, weights_handle, weights_obj);
  mem_destroy(ctx->fd, output_handle, output_obj);
  return 0;
}