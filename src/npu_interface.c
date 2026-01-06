/*
 * Copyright (C) 2024  Jasbir Matharu, <jasjnuk@gmail.com>
 *
 * This file is part of rk3588-npu.
 *
 * rk3588-npu is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * rk3588-npu is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with rk3588-npu.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <stdbool.h>

#include "rknpu-ioctl.h"
#include "npu_hw.h"

typedef struct {
  uint32_t handle;
  uint64_t dma_addr;
} HandleDmaEntry;

#define HANDLE_DMA_CAPACITY 64
static HandleDmaEntry handle_dma_map[HANDLE_DMA_CAPACITY];
static size_t handle_dma_count = 0;

static void reset_rknpu_info_file(void) {
  FILE *f = fopen("/tmp/rknpu_info", "w");
  if (f) fclose(f);
}

static void log_rknpu_info(const char *fmt, ...) {
  FILE *f = fopen("/tmp/rknpu_info", "a");
  if (!f) return;
  va_list args;
  va_start(args, fmt);
  vfprintf(f, fmt, args);
  va_end(args);
  fclose(f);
}

static void reset_handle_dma_map(void) {
  handle_dma_count = 0;
  reset_rknpu_info_file();
}

static void store_handle_dma(uint32_t handle, uint64_t dma_addr) {
  for (size_t i = 0; i < handle_dma_count; i++) {
    if (handle_dma_map[i].handle == handle) {
      handle_dma_map[i].dma_addr = dma_addr;
      return;
    }
  }
  if (handle_dma_count < HANDLE_DMA_CAPACITY) {
    handle_dma_map[handle_dma_count].handle = handle;
    handle_dma_map[handle_dma_count].dma_addr = dma_addr;
    handle_dma_count++;
  }
}

static bool find_dma_for_handle(uint32_t handle, uint64_t *dma_addr) {
  for (size_t i = 0; i < handle_dma_count; i++) {
    if (handle_dma_map[i].handle == handle) {
      if (dma_addr) *dma_addr = handle_dma_map[i].dma_addr;
      return true;
    }
  }
  return false;
}

void* mem_allocate(int fd, size_t size, uint64_t *dma_addr, uint64_t *obj, uint32_t flags, uint32_t *handle) {

  int ret;
  struct rknpu_mem_create mem_create = {
    .flags = flags | RKNPU_MEM_NON_CACHEABLE,
    .size = size,
  };

  ret = ioctl(fd, DRM_IOCTL_RKNPU_MEM_CREATE, &mem_create);
  if(ret < 0)  {
    printf("RKNPU_MEM_CREATE failed %d\n",ret);
    return NULL;
  }

  struct rknpu_mem_map mem_map = { .handle = mem_create.handle, .offset=0 };
  ret = ioctl(fd, DRM_IOCTL_RKNPU_MEM_MAP, &mem_map);
  if(ret < 0) {
    printf("RKNPU_MEM_MAP failed %d\n",ret);
    return NULL;
  }

  void *map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mem_map.offset);

  *dma_addr = mem_create.dma_addr;
  *obj = mem_create.obj_addr;
  *handle = mem_create.handle;
  store_handle_dma(mem_create.handle, mem_create.dma_addr);
  return map;
}

void mem_destroy(int fd, uint32_t handle, uint64_t obj_addr) {

  int ret;
  struct rknpu_mem_destroy destroy = {
    .handle = handle ,
    .obj_addr = obj_addr
  };

  ret = ioctl(fd, DRM_IOCTL_RKNPU_MEM_DESTROY, &destroy);
  if (ret <0) {
    printf("RKNPU_MEM_DESTROY failed %d\n",ret);
  }
}

int create_flink_name(int fd, uint32_t handle, uint32_t *flink_name, const char *name) {
  struct drm_gem_flink flink_req = {
    .handle = handle,
    .name = 0
  };

  int ret = ioctl(fd, DRM_IOCTL_GEM_FLINK, &flink_req);
  if (ret < 0) {
    printf("ERROR: DRM_IOCTL_GEM_FLINK failed: %s (%d)\n", strerror(errno), errno);
    return ret;
  }

  *flink_name = flink_req.name;
  printf("SUCCESS: Created flink name %u for handle %u (%s)\n", *flink_name, handle, name ? name : "unknown");
  uint64_t dma_addr = 0;
  if (find_dma_for_handle(handle, &dma_addr)) {
    printf("dma addr: 0x%llx gem name: %u (handle %u)\n",
      (unsigned long long)dma_addr, *flink_name, handle);
    log_rknpu_info("FLINK handle=%u flink=%u dma=0x%llx\n",
      handle, *flink_name, (unsigned long long)dma_addr);
  }
  return 0;
}

int npu_open() {

  char buf1[256], buf2[256], buf3[256];

  memset(buf1, 0 ,sizeof(buf1));
  memset(buf2, 0 ,sizeof(buf2));
  memset(buf3, 0, sizeof(buf3));

  // Open DRI called "rknpu"
  int fd = open("/dev/dri/card1", O_RDWR);
  if(fd<0) {
    printf("Failed to open /dev/dri/card1 %d\n",errno);
    return fd;
  }

  struct drm_version dv;
  memset(&dv, 0, sizeof(dv));
  dv.name = buf1;
  dv.name_len = sizeof(buf1);
  dv.date = buf2;
  dv.date_len = sizeof(buf2);
  dv.desc = buf3;
  dv.desc_len = sizeof(buf3);

  int ret = ioctl(fd, DRM_IOCTL_VERSION, &dv);
  if (ret <0) {
    printf("DRM_IOCTL_VERISON failed %d\n",ret);
    return ret;
  }
  printf("drm name is %s - %s - %s\n", dv.name, dv.date, dv.desc);
  reset_handle_dma_map();
  return fd;
}

int npu_close(int fd) {
  return close(fd);	
}

int npu_reset(int fd) {

  // Reset the NPU
  struct rknpu_action act = {
    .flags = RKNPU_ACT_RESET,
  };
  return ioctl(fd, DRM_IOCTL_RKNPU_ACTION, &act);	
}
