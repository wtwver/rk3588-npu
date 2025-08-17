/*
 * Copyright (C) 2024  Jasbir Matharu, <jasjnuk@gmail.com>
 *
 * This file is part of rk3588-npu.
 *
 * rk3588-npu is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rk3588-npu is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rk3588-npu.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include "npu_hw.h"
#include "npu_cna.h"
#include "npu_dpu.h"
#include "npu_matmul.h" // reuse task emission helper signature and packing helpers
#include "npu_conv.h"

extern void gen_matmul_task(uint64_t *ops, npu_cna_desc *cna_desc, npu_core_desc *core_desc, npu_dpu_desc *dpu_desc);

static int compute_bank_allocation_fp16(uint32_t fd_bytes, uint32_t weight_bytes_per_kernel, unsigned int *fd_banks_out, unsigned int *weight_banks_out) {
  unsigned int fd_banks = (fd_bytes / NPU_CBUF_BANK_SIZE);
  fd_banks = ((fd_bytes % NPU_CBUF_BANK_SIZE) == 0) ? fd_banks : fd_banks + 1;
  if (fd_banks > NPU_CBUF_BANKS - 1) {
    return -1;
  }
  if (weight_bytes_per_kernel > NPU_CBUF_BANK_SIZE) {
    return -2;
  }
  *fd_banks_out = fd_banks;
  *weight_banks_out = NPU_CBUF_BANKS - fd_banks;
  return 0;
}

static int compute_bank_allocation_int8(uint32_t fd_bytes, uint32_t weight_bytes_per_kernel, unsigned int *fd_banks_out, unsigned int *weight_banks_out) {
  unsigned int fd_banks = (fd_bytes / NPU_CBUF_BANK_SIZE);
  fd_banks = ((fd_bytes % NPU_CBUF_BANK_SIZE) == 0) ? fd_banks : fd_banks + 1;
  if (fd_banks > NPU_CBUF_BANKS - 1) {
    return -1;
  }
  if (weight_bytes_per_kernel > NPU_CBUF_BANK_SIZE) {
    return -2;
  }
  *fd_banks_out = fd_banks;
  *weight_banks_out = NPU_CBUF_BANKS - fd_banks;
  return 0;
}

int gen_conv2d_fp16(conv2d_params_t *params) {
  npu_cna_desc cna_desc;
  npu_core_desc core_desc;
  npu_dpu_desc dpu_desc;

  // Set CNA for 2D convolution
  cna_desc.conv_mode = direct_convolution;
  cna_desc.in_precision = precision_float16;
  cna_desc.proc_precision = precision_float16;

  cna_desc.kernel_groups = 0;
  cna_desc.feature_grains = params->height + 1; // heuristic similar to matmul
  cna_desc.conv_x_stride = params->stride_x > 0 ? params->stride_x : 1;
  cna_desc.conv_y_stride = params->stride_y > 0 ? params->stride_y : 1;

  cna_desc.datain_width = params->width;
  cna_desc.datain_height = params->height;
  cna_desc.datain_channel = params->in_channels;

  // Output shape (no dilation): H_out = floor((H + pad_top - k_h) / stride_y) + 1
  // Using pad_left only for now (as per available fields)
  uint16_t out_h = (params->height + params->pad_top - params->kernel_h) / cna_desc.conv_y_stride + 1;
  uint16_t out_w = (params->width + params->pad_left - params->kernel_w) / cna_desc.conv_x_stride + 1;
  cna_desc.dataout_width = out_w;
  cna_desc.dataout_height = out_h;
  cna_desc.dataout_atomics = (uint32_t)(out_w * out_h);

  // Weights
  cna_desc.weight_width = params->kernel_w;
  cna_desc.weight_height = params->kernel_h;
  cna_desc.weight_kernels = params->out_channels;
  cna_desc.weight_bytes_per_kernel = (uint32_t)cna_desc.weight_width * cna_desc.weight_height * cna_desc.datain_channel * sizeof(__fp16);
  cna_desc.weight_bytes = cna_desc.weight_bytes_per_kernel * cna_desc.weight_kernels;

  // Bank allocation
  unsigned int fd_banks = 0, weight_banks = 0;
  uint32_t fd_bytes = (uint32_t)cna_desc.datain_width * cna_desc.datain_height * cna_desc.datain_channel * sizeof(__fp16);
  int ba = compute_bank_allocation_fp16(fd_bytes, cna_desc.weight_bytes_per_kernel, &fd_banks, &weight_banks);
  if (ba != 0) {
    return ba;
  }

  cna_desc.weight_bank = (uint8_t)weight_banks;
  cna_desc.data_bank = (uint8_t)fd_banks;
  cna_desc.data_entries = (uint16_t)((cna_desc.datain_width * cna_desc.datain_channel) / 32);
  cna_desc.data_entries = (uint16_t)(((cna_desc.datain_width * cna_desc.datain_channel) % 32) == 0 ? cna_desc.data_entries : (cna_desc.data_entries + 1));
  cna_desc.data_sign = 0x1;
  cna_desc.cvt_type = 0x1;
  cna_desc.cvt_bypass = 0x1;
  cna_desc.cvt_scale0 = 0x1;
  cna_desc.cvt_scale1 = 0x1;
  cna_desc.cvt_scale2 = 0x1;
  cna_desc.cvt_scale3 = 0x1;
  cna_desc.fc_skip_en = 0;
  cna_desc.data_offset = 0x0;
  cna_desc.pad_left = params->pad_left;
  cna_desc.pad_top = params->pad_top;
  cna_desc.feature_base_addr = params->input_dma;
  cna_desc.weight_offset = 0;
  cna_desc.weight_burst_len = 0xF;
  cna_desc.data_burst_len = 0xF;
  cna_desc.line_stride = cna_desc.datain_width * 4; // fp16 packs 8 per 32B line unit as in matmul
  int surf_stride = (int)(cna_desc.line_stride * ((cna_desc.datain_height / 4) - 1));
  surf_stride = surf_stride < 0 ? surf_stride + 1 : surf_stride;
  cna_desc.surf_stride = surf_stride;
  cna_desc.dma_width = cna_desc.datain_width;
  cna_desc.dma_height = cna_desc.datain_height;
  cna_desc.dma_channel = cna_desc.datain_channel;
  cna_desc.decompress_addr0 = params->weights_dma;
  cna_desc.dataout_height = out_h; // copied for core usage convenience

  // Core
  core_desc.proc_precision = precision_float16;
  core_desc.qd_en = 1;
  core_desc.dataout_height = (uint16_t)(cna_desc.dataout_height - 1);
  core_desc.dataout_width = (uint16_t)(cna_desc.dataout_width - 1);
  core_desc.dataout_channel = (uint16_t)(cna_desc.weight_kernels - 1);

  // DPU writes output tensor to memory
  dpu_desc.burst_len = 0xF;
  dpu_desc.conv_mode = direct_convolution;
  dpu_desc.output_mode = 0x2;
  dpu_desc.flying_mode = 0x0;
  dpu_desc.out_precision = (params->fp32tofp16 == 0) ? precision_float32 : precision_float16;
  dpu_desc.in_precision = precision_float16;
  dpu_desc.proc_precision = precision_float16;
  dpu_desc.dst_base_addr = params->output_dma;
  dpu_desc.dst_surf_stride = cna_desc.dataout_height * cna_desc.dataout_width;
  dpu_desc.width = core_desc.dataout_width;
  dpu_desc.height = core_desc.dataout_height;
  dpu_desc.channel = core_desc.dataout_channel;
  dpu_desc.bs_bypass = 1;
  dpu_desc.bs_alu_bypass = 1;
  dpu_desc.bs_mul_bypass = 1;
  dpu_desc.bs_relu_bypass = 1;
  dpu_desc.bn_bypass = 1;
  dpu_desc.bn_alu_bypass = 1;
  dpu_desc.bn_mul_bypass = 1;
  dpu_desc.bn_relu_bypass = 1;
  dpu_desc.ew_bypass = 1;
  dpu_desc.ew_op_bypass = 1;
  dpu_desc.ew_lut_bypass = 1;
  dpu_desc.ew_op_cvt_bypass = 1;
  dpu_desc.ew_relu_bypass = 1;
  dpu_desc.fp32tofp16_en = params->fp32tofp16 & 0x1;
  dpu_desc.out_cvt_scale = 1;
  if (params->fp32tofp16 == 0) {
    dpu_desc.size_e_2 = 3;
    dpu_desc.size_e_1 = 3;
    dpu_desc.size_e_0 = 3;
  } else {
    dpu_desc.size_e_2 = 1;
    dpu_desc.size_e_1 = 1;
    dpu_desc.size_e_0 = 1;
  }
  dpu_desc.od_bypass = 1;
  dpu_desc.width_wdma = core_desc.dataout_width;
  dpu_desc.height_wdma = core_desc.dataout_height;
  dpu_desc.channel_wdma = core_desc.dataout_channel;
  dpu_desc.surf_add = (!params->fp32tofp16) ? dpu_desc.dst_surf_stride * 4 : dpu_desc.dst_surf_stride * 2;

  gen_matmul_task(params->tasks, &cna_desc, &core_desc, &dpu_desc);
  return 0;
}

int gen_conv2d_int8(conv2d_params_t *params) {
  npu_cna_desc cna_desc;
  npu_core_desc core_desc;
  npu_dpu_desc dpu_desc;

  cna_desc.conv_mode = direct_convolution;
  cna_desc.in_precision = precision_int8;
  cna_desc.proc_precision = precision_int8;

  cna_desc.kernel_groups = 0;
  cna_desc.feature_grains = params->height + 1;
  cna_desc.conv_x_stride = params->stride_x > 0 ? params->stride_x : 1;
  cna_desc.conv_y_stride = params->stride_y > 0 ? params->stride_y : 1;

  cna_desc.datain_width = params->width;
  cna_desc.datain_height = params->height;
  cna_desc.datain_channel = params->in_channels;

  uint16_t out_h = (params->height + params->pad_top - params->kernel_h) / cna_desc.conv_y_stride + 1;
  uint16_t out_w = (params->width + params->pad_left - params->kernel_w) / cna_desc.conv_x_stride + 1;
  cna_desc.dataout_width = out_w;
  cna_desc.dataout_height = out_h;
  cna_desc.dataout_atomics = (uint32_t)(out_w * out_h);

  cna_desc.weight_width = params->kernel_w;
  cna_desc.weight_height = params->kernel_h;
  cna_desc.weight_kernels = params->out_channels;
  cna_desc.weight_bytes_per_kernel = (uint32_t)cna_desc.weight_width * cna_desc.weight_height * cna_desc.datain_channel * sizeof(int8_t);
  cna_desc.weight_bytes = cna_desc.weight_bytes_per_kernel * cna_desc.weight_kernels;

  unsigned int fd_banks = 0, weight_banks = 0;
  uint32_t fd_bytes = (uint32_t)cna_desc.datain_width * cna_desc.datain_height * cna_desc.datain_channel * sizeof(int8_t);
  int ba = compute_bank_allocation_int8(fd_bytes, cna_desc.weight_bytes_per_kernel, &fd_banks, &weight_banks);
  if (ba != 0) {
    return ba;
  }

  cna_desc.weight_bank = (uint8_t)weight_banks;
  cna_desc.data_bank = (uint8_t)fd_banks;
  cna_desc.data_entries = (uint16_t)((cna_desc.datain_width * cna_desc.datain_channel) / 64);
  cna_desc.data_entries = (uint16_t)(((cna_desc.datain_width * cna_desc.datain_channel) % 64) == 0 ? cna_desc.data_entries : (cna_desc.data_entries + 1));
  cna_desc.data_sign = 0x1;
  cna_desc.cvt_type = 0x1;
  cna_desc.cvt_bypass = 0x1;
  cna_desc.cvt_scale0 = 0x1;
  cna_desc.cvt_scale1 = 0x1;
  cna_desc.cvt_scale2 = 0x1;
  cna_desc.cvt_scale3 = 0x1;
  cna_desc.fc_skip_en = 0;
  cna_desc.data_offset = 0x0;
  cna_desc.pad_left = params->pad_left;
  cna_desc.pad_top = params->pad_top;
  cna_desc.feature_base_addr = params->input_dma;
  cna_desc.weight_offset = 0;
  cna_desc.weight_burst_len = 0xF;
  cna_desc.data_burst_len = 0xF;
  cna_desc.line_stride = cna_desc.datain_width * 4;
  int surf_stride = (int)(cna_desc.line_stride * ((cna_desc.datain_height / 4) - 1));
  surf_stride = surf_stride < 0 ? surf_stride + 1 : surf_stride;
  cna_desc.surf_stride = surf_stride;
  cna_desc.dma_width = cna_desc.datain_width;
  cna_desc.dma_height = cna_desc.datain_height;
  cna_desc.dma_channel = cna_desc.datain_channel;
  cna_desc.decompress_addr0 = params->weights_dma;
  cna_desc.dataout_height = out_h;

  core_desc.proc_precision = precision_int8;
  core_desc.qd_en = 0;
  core_desc.dataout_height = (uint16_t)(cna_desc.dataout_height - 1);
  core_desc.dataout_width = (uint16_t)(cna_desc.dataout_width - 1);
  core_desc.dataout_channel = (uint16_t)(cna_desc.weight_kernels - 1);

  dpu_desc.burst_len = 0xF;
  dpu_desc.conv_mode = direct_convolution;
  dpu_desc.output_mode = 0x2;
  dpu_desc.flying_mode = 0x0;
  dpu_desc.out_precision = precision_int32;
  dpu_desc.in_precision = precision_int8;
  dpu_desc.proc_precision = precision_int8;
  dpu_desc.dst_base_addr = params->output_dma;
  dpu_desc.dst_surf_stride = cna_desc.dataout_height * cna_desc.dataout_width;
  dpu_desc.width = core_desc.dataout_width;
  dpu_desc.height = core_desc.dataout_height;
  dpu_desc.channel = core_desc.dataout_channel;
  dpu_desc.bs_bypass = 1;
  dpu_desc.bs_alu_bypass = 1;
  dpu_desc.bs_mul_bypass = 1;
  dpu_desc.bs_relu_bypass = 1;
  dpu_desc.bn_bypass = 1;
  dpu_desc.bn_alu_bypass = 1;
  dpu_desc.bn_mul_bypass = 1;
  dpu_desc.bn_relu_bypass = 1;
  dpu_desc.ew_bypass = 1;
  dpu_desc.ew_op_bypass = 1;
  dpu_desc.ew_lut_bypass = 1;
  dpu_desc.ew_op_cvt_bypass = 1;
  dpu_desc.ew_relu_bypass = 1;
  dpu_desc.fp32tofp16_en = 0;
  dpu_desc.out_cvt_scale = 1;
  dpu_desc.size_e_2 = 7;
  dpu_desc.size_e_1 = 7;
  dpu_desc.size_e_0 = 7;
  dpu_desc.od_bypass = 1;
  dpu_desc.width_wdma = core_desc.dataout_width;
  dpu_desc.height_wdma = core_desc.dataout_height;
  dpu_desc.channel_wdma = core_desc.dataout_channel;
  dpu_desc.surf_add = dpu_desc.dst_surf_stride * 8;

  gen_matmul_task(params->tasks, &cna_desc, &core_desc, &dpu_desc);
  return 0;
}