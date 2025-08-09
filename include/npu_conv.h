#ifndef NPU_CONV_H
#define NPU_CONV_H

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

#include <stdint.h>

// Parameters for a single conv2d operation. Currently supports stride >=1 and padding top/left.
// Kernel size 1x1 is fully supported using existing weight/feature packing helpers.
// Other kernel sizes can be enabled once weight packing is confirmed.

typedef struct {
  // Input dimensions (H x W x C)
  uint16_t height;
  uint16_t width;
  uint16_t in_channels;

  // Weights: kernel size and number of output channels
  uint8_t kernel_h;    // use 1 for now
  uint8_t kernel_w;    // use 1 for now
  uint16_t out_channels;

  // Stride and padding
  uint8_t stride_y;    // default 1
  uint8_t stride_x;    // default 1
  uint8_t pad_top;     // default 0
  uint8_t pad_left;    // default 0

  // DMA addresses
  uint32_t input_dma;
  uint32_t weights_dma;
  uint32_t output_dma;

  // Where to emit the register command stream (size must be >= 112 uint64s)
  uint64_t *tasks;

  // For fp16 path: set to 0 to output fp32, 1 to output fp16
  uint8_t fp32tofp16;
} conv2d_params_t;

int gen_conv2d_fp16(conv2d_params_t *params);
int gen_conv2d_int8(conv2d_params_t *params);

#endif // NPU_CONV_H