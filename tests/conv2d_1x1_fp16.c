/*
 * Simple conv2d (1x1) fp16 test using rk3588-npu driver path similar to matmul tests
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>

#include <libdrm/drm.h>

#include "rknpu-ioctl.h"
#include "npu_interface.h"
#include "npu_conv.h"
#include "npu_matmul.h" // reuse packing helpers feature_data/weight_fp16

#define MAX_H 16
#define MAX_W 16
#define MAX_C 128
#define MAX_OC 128

static uint64_t npu_regs[112];

static void conv1x1_ref_fp32(int H, int W, int C, int OC, const _Float16 *inp, const _Float16 *w, float *out) {
  for (int h = 0; h < H; ++h) {
    for (int widx = 0; widx < W; ++widx) {
      for (int oc = 0; oc < OC; ++oc) {
        float sum = 0.0f;
        for (int c = 0; c < C; ++c) {
          sum += (float)inp[(h*W + widx)*C + c] * (float)w[oc*C + c];
        }
        out[(h*W + widx)*OC + oc] = sum;
      }
    }
  }
}

static float rand_float() { return rand()/(float)RAND_MAX; }

int main(int argc, char **argv) {
  int H = 4, W = 4, C = 32, OC = 32;
  if (argc == 5) {
    H = atoi(argv[1]);
    W = atoi(argv[2]);
    C = atoi(argv[3]);
    OC = atoi(argv[4]);
  }
  if (H <= 0 || H > MAX_H || W <= 0 || W > MAX_W || C <= 0 || C > MAX_C || (C % 32) != 0 || OC <= 0 || OC > MAX_OC || (OC % 16) != 0) {
    printf("Bad sizes H=%d W=%d C=%d OC=%d (C%%32==0, OC%%16==0 required)\n", H, W, C, OC);
    return -1;
  }

  // Open NPU
  int fd = npu_open();

  uint64_t regcmd_dma, regcmd_obj; uint32_t regcmd_handle;
  uint64_t *regcmd = mem_allocate(fd, 1024, &regcmd_dma, &regcmd_obj, 0, &regcmd_handle);
  uint64_t tasks_dma, tasks_obj; uint32_t tasks_handle;
  struct rknpu_task *tasks = mem_allocate(fd, 1024, &tasks_dma, &tasks_obj, RKNPU_MEM_KERNEL_MAPPING, &tasks_handle);

  size_t in_bytes = (size_t)H*W*C*sizeof(_Float16);
  size_t w_bytes  = (size_t)OC*C*sizeof(_Float16);
  size_t out_bytes = (size_t)H*W*OC*sizeof(float);

  uint64_t input_dma, input_obj; uint32_t input_handle; void *input = mem_allocate(fd, in_bytes, &input_dma, &input_obj, 0, &input_handle);
  uint64_t weights_dma, weights_obj; uint32_t weights_handle; void *weights = mem_allocate(fd, w_bytes, &weights_dma, &weights_obj, 0, &weights_handle);
  uint64_t output_dma, output_obj; uint32_t output_handle; void *output = mem_allocate(fd, out_bytes, &output_dma, &output_obj, 0, &output_handle);

  if (!regcmd || !tasks || !input || !weights || !output) { printf("alloc fail\n"); return -1; }

  npu_reset(fd);

  conv2d_params_t params = {
    .height = (uint16_t)H,
    .width = (uint16_t)W,
    .in_channels = (uint16_t)C,
    .kernel_h = 1,
    .kernel_w = 1,
    .out_channels = (uint16_t)OC,
    .stride_y = 1,
    .stride_x = 1,
    .pad_top = 0,
    .pad_left = 0,
    .input_dma = (uint32_t)input_dma,
    .weights_dma = (uint32_t)weights_dma,
    .output_dma = (uint32_t)output_dma,
    .tasks = (uint64_t*)&npu_regs,
    .fp32tofp16 = 0
  };

  int ret = gen_conv2d_fp16(&params);
  if (ret != 0) { printf("gen_conv2d_fp16 failed %d\n", ret); return ret; }

  memcpy(regcmd, npu_regs, sizeof(npu_regs));

  tasks[0].flags = 0;
  tasks[0].op_idx = 0;
  tasks[0].enable_mask = 0xd;
  tasks[0].int_mask = 0x300;
  tasks[0].int_clear = 0x1ffff;
  tasks[0].int_status = 0;
  tasks[0].regcfg_amount = sizeof(npu_regs)/sizeof(uint64_t)-(RKNPU_PC_DATA_EXTRA_AMOUNT+4);
  tasks[0].regcfg_offset = 0;
  tasks[0].regcmd_addr = regcmd_dma;

  // Fill inputs with random whole numbers like matmul test to avoid fp16 diff noise
  srand(time(NULL));
  _Float16 *invec = (_Float16*)input;
  for (int i = 0; i < H*W*C; ++i) invec[i] = (_Float16)(int)(10.0f * rand_float());
  _Float16 *wvec = (_Float16*)weights;
  for (int i = 0; i < OC*C; ++i) wvec[i] = (_Float16)(int)(10.0f * rand_float());

  // Pack weights and feature data in-place to expected layout
  // Weights: for 1x1 conv, per-output-channel kernel is just C elements; reuse weight_fp16(C, oc, c)
  for (int oc = 1; oc <= OC; ++oc) {
    for (int c = 1; c <= C; ++c) {
      wvec[weight_fp16(C, oc, c)] = wvec[(oc-1)*C + (c-1)];
    }
  }
  // Feature data: each pixel (h,w) is a row of C channels; reuse feature_data(C, H*W, 1, 8, c, pos, 1)
  // Flatten HxW into M=H*W rows, width=1 kept consistent with helper
  _Float16 *fdp = (_Float16*)input;
  for (int pos = 1; pos <= H*W; ++pos) {
    for (int c = 1; c <= C; ++c) {
      int hw = pos - 1; int h = hw / W; int widx = hw % W;
      fdp[feature_data(C, H*W, 1, 8, c, pos, 1)] = invec[(h*W + widx)*C + (c-1)];
    }
  }

  // CPU reference
  float *gold = (float*)malloc(out_bytes);
  conv1x1_ref_fp32(H, W, C, OC, invec, wvec, gold);
  memset(output, 0, out_bytes);

  struct rknpu_submit submit = {
    .flags = RKNPU_JOB_PC | RKNPU_JOB_BLOCK | RKNPU_JOB_PINGPONG,
    .timeout = 6000,
    .task_start = 0,
    .task_number = 1,
    .task_counter = 0,
    .priority = 0,
    .task_obj_addr = tasks_obj,
    .regcfg_obj_addr = 0,
    .task_base_addr = 0,
    .user_data = 0,
    .core_mask = 1,
    .fence_fd = -1,
    .subcore_task = { {0,1}, {1,0}, {2,0}, {0,0}, {0,0} },
  };
  ret = ioctl(fd, DRM_IOCTL_RKNPU_SUBMIT, &submit);
  printf("RKNPU_SUBMIT returned %d\n", ret);
  if (ret < 0) return ret;

  // Compare
  int err = 0;
  float *outf = (float*)output;
  for (int h = 0; h < H; ++h) {
    for (int widx = 0; widx < W; ++widx) {
      for (int oc = 0; oc < OC; ++oc) {
        // Output layout uses feature_data with N=OC, M=H*W
        int pos = h*W + widx + 1;
        float actual = outf[feature_data(OC, H*W, 1, 4, oc+1, pos, 1)];
        float expected = gold[(h*W + widx)*OC + oc];
        if (actual != expected) { err = 1; goto done; }
      }
    }
  }

 done:
  if (!err) printf("conv2d 1x1 fp16 [%dx%dx%d] -> [%dx%dx%d] ok\n", H, W, C, H, W, OC);
  else printf("conv2d 1x1 fp16 mismatch\n");

  // Cleanup
  free(gold);
  munmap(regcmd, 1024);
  munmap(tasks, 1024);
  munmap(input, in_bytes);
  munmap(weights, w_bytes);
  munmap(output, out_bytes);
  mem_destroy(fd, regcmd_handle, regcmd_obj);
  mem_destroy(fd, tasks_handle, tasks_obj);
  mem_destroy(fd, input_handle, input_obj);
  mem_destroy(fd, weights_handle, weights_obj);
  mem_destroy(fd, output_handle, output_obj);
  npu_close(fd);
  return err ? -1 : 0;
}