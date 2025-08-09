#define _GNU_SOURCE
#include "npu_umdr.h"
#include "vfio_helpers.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

// Example register layout (adjust to your hardware)
#define NPU_REG_INPUT_ADDR_LO   0x0000
#define NPU_REG_INPUT_ADDR_HI   0x0004
#define NPU_REG_OUTPUT_ADDR_LO  0x0008
#define NPU_REG_OUTPUT_ADDR_HI  0x000C
#define NPU_REG_DOORBELL        0x0010
#define NPU_REG_STATUS          0x0014

struct npu_device {
    struct vfio_context vfio;
};

static inline uint32_t load32(const volatile void *base, uint32_t off) {
    const volatile uint32_t *p = (const volatile uint32_t *)((const volatile uint8_t *)base + off);
    return *p;
}

static inline void store32(volatile void *base, uint32_t off, uint32_t val) {
    volatile uint32_t *p = (volatile uint32_t *)((volatile uint8_t *)base + off);
    *p = val;
}

int npu_device_open(const char *pci_bdf, struct npu_device **out_device) {
    if (!pci_bdf || !out_device) return -EINVAL;

    struct npu_device *dev = calloc(1, sizeof(*dev));
    if (!dev) return -ENOMEM;

    int ret = vfio_init_for_bdf(pci_bdf, &dev->vfio);
    if (ret) {
        free(dev);
        return ret;
    }

    ret = vfio_map_bar0(&dev->vfio);
    if (ret) {
        vfio_destroy(&dev->vfio);
        free(dev);
        return ret;
    }

    *out_device = dev;
    return 0;
}

void npu_device_close(struct npu_device *device) {
    if (!device) return;
    vfio_destroy(&device->vfio);
    free(device);
}

int npu_alloc_dma(struct npu_device *device, size_t size_bytes, struct npu_dma_buf *out_buf) {
    if (!device || !out_buf || size_bytes == 0) return -EINVAL;

    size_t page = (size_t) sysconf(_SC_PAGESIZE);
    size_t alloc_size = (size_bytes + page - 1) / page * page;

    void *ptr = NULL;
    int r = posix_memalign(&ptr, page, alloc_size);
    if (r != 0) {
        return -r;
    }
    memset(ptr, 0, alloc_size);

    uint64_t iova = 0;
    int ret = vfio_map_dma(&device->vfio, ptr, alloc_size, &iova);
    if (ret) {
        free(ptr);
        return ret;
    }

    out_buf->host_ptr = ptr;
    out_buf->iova = iova;
    out_buf->size_bytes = alloc_size;
    return 0;
}

int npu_free_dma(struct npu_device *device, struct npu_dma_buf *buf) {
    if (!device || !buf || !buf->host_ptr) return -EINVAL;
    int ret = vfio_unmap_dma(&device->vfio, buf->iova, buf->size_bytes);
    free(buf->host_ptr);
    memset(buf, 0, sizeof(*buf));
    return ret;
}

uint32_t npu_mmio_read32(struct npu_device *device, uint32_t offset) {
    return load32(device->vfio.bar0, offset);
}

void npu_mmio_write32(struct npu_device *device, uint32_t offset, uint32_t value) {
    store32(device->vfio.bar0, offset, value);
}

int npu_submit_simple(struct npu_device *device, const struct npu_dma_buf *input, const struct npu_dma_buf *output) {
    if (!device || !input || !output) return -EINVAL;

    uint64_t in = input->iova;
    uint64_t out = output->iova;

    store32(device->vfio.bar0, NPU_REG_INPUT_ADDR_LO, (uint32_t)(in & 0xFFFFFFFFu));
    store32(device->vfio.bar0, NPU_REG_INPUT_ADDR_HI, (uint32_t)(in >> 32));
    store32(device->vfio.bar0, NPU_REG_OUTPUT_ADDR_LO, (uint32_t)(out & 0xFFFFFFFFu));
    store32(device->vfio.bar0, NPU_REG_OUTPUT_ADDR_HI, (uint32_t)(out >> 32));

    // Ring doorbell
    store32(device->vfio.bar0, NPU_REG_DOORBELL, 1u);

    // Busy-wait for completion bit in status
    // Real implementations should use interrupts or a wait with timeout
    for (int i = 0; i < 10000000; ++i) {
        uint32_t st = load32(device->vfio.bar0, NPU_REG_STATUS);
        if ((st & 0x1u) != 0) {
            return 0;
        }
    }
    return -ETIMEDOUT;
}