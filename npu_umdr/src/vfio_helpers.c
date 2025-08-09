#define _GNU_SOURCE
#include "vfio_helpers.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/pci_regs.h>
#include <linux/types.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef VFIO_API_VERSION
#error "Your libc kernel headers are missing VFIO definitions. Install linux-headers."
#endif

static int get_group_number_for_bdf(const char *bdf, int *group_number_out) {
    // bdf like "0000:65:00.0"
    char path[512];
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/iommu_group", bdf);

    char link_target[512];
    ssize_t len = readlink(path, link_target, sizeof(link_target) - 1);
    if (len < 0) {
        return -errno;
    }
    link_target[len] = '\0';

    // link_target ends with "/groups/<num>"
    char *last_slash = strrchr(link_target, '/');
    if (!last_slash) {
        return -EINVAL;
    }
    int group = atoi(last_slash + 1);
    *group_number_out = group;
    return 0;
}

static int open_group_fd(int group) {
    char devpath[64];
    snprintf(devpath, sizeof(devpath), "/dev/vfio/%d", group);
    int fd = open(devpath, O_RDWR);
    if (fd < 0) {
        return -errno;
    }
    return fd;
}

static int get_device_fd(int group_fd, const char *bdf) {
    // With VFIO, the device string is the PCI BDF path under sysfs:
    // e.g., "0000:65:00.0"
    int dev_fd = ioctl(group_fd, VFIO_GROUP_GET_DEVICE_FD, bdf);
    if (dev_fd < 0) {
        return -errno;
    }
    return dev_fd;
}

int vfio_init_for_bdf(const char *bdf, struct vfio_context *out) {
    memset(out, 0, sizeof(*out));

    int container_fd = open("/dev/vfio/vfio", O_RDWR);
    if (container_fd < 0) {
        return -errno;
    }

    int api_version = ioctl(container_fd, VFIO_GET_API_VERSION);
    if (api_version != VFIO_API_VERSION) {
        close(container_fd);
        return -EINVAL;
    }

    // Check support for the Type1 IOMMU
    if (!ioctl(container_fd, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU)) {
        close(container_fd);
        return -ENOTSUP;
    }

    int group_number = -1;
    int ret = get_group_number_for_bdf(bdf, &group_number);
    if (ret) {
        close(container_fd);
        return ret;
    }

    int group_fd = open_group_fd(group_number);
    if (group_fd < 0) {
        close(container_fd);
        return group_fd;
    }

    // Add group to container
    if (ioctl(group_fd, VFIO_GROUP_SET_CONTAINER, &container_fd) < 0) {
        ret = -errno;
        close(group_fd);
        close(container_fd);
        return ret;
    }

    // Enable IOMMU on container
    if (ioctl(container_fd, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU) < 0) {
        ret = -errno;
        // Try to detach
        ioctl(group_fd, VFIO_GROUP_UNSET_CONTAINER, &container_fd);
        close(group_fd);
        close(container_fd);
        return ret;
    }

    int device_fd = get_device_fd(group_fd, bdf);
    if (device_fd < 0) {
        ret = device_fd;
        ioctl(group_fd, VFIO_GROUP_UNSET_CONTAINER, &container_fd);
        close(group_fd);
        close(container_fd);
        return ret;
    }

    out->container_fd = container_fd;
    out->group_fd = group_fd;
    out->device_fd = device_fd;

    // Set a simple IOVA window (1GB starting at 0x100000000)
    out->next_iova = 0x0000000100000000ULL;
    out->iova_end  = out->next_iova + (1ULL << 30);

    return 0;
}

void vfio_destroy(struct vfio_context *ctx) {
    if (!ctx) return;

    if (ctx->bar0 && ctx->bar0_size) {
        munmap(ctx->bar0, ctx->bar0_size);
    }

    if (ctx->device_fd > 0) close(ctx->device_fd);

    if (ctx->group_fd > 0 && ctx->container_fd > 0) {
        ioctl(ctx->group_fd, VFIO_GROUP_UNSET_CONTAINER, &ctx->container_fd);
    }

    if (ctx->group_fd > 0) close(ctx->group_fd);
    if (ctx->container_fd > 0) close(ctx->container_fd);

    memset(ctx, 0, sizeof(*ctx));
}

int vfio_map_bar0(struct vfio_context *ctx) {
    struct vfio_device_info dev_info = { .argsz = sizeof(dev_info) };
    if (ioctl(ctx->device_fd, VFIO_DEVICE_GET_INFO, &dev_info) < 0) {
        return -errno;
    }

    struct vfio_region_info reg = { .argsz = sizeof(reg), .index = VFIO_PCI_BAR0_REGION_INDEX };
    if (ioctl(ctx->device_fd, VFIO_DEVICE_GET_REGION_INFO, &reg) < 0) {
        return -errno;
    }

    void *bar0 = mmap(NULL, reg.size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->device_fd, reg.offset);
    if (bar0 == MAP_FAILED) {
        return -errno;
    }

    ctx->bar0 = bar0;
    ctx->bar0_size = reg.size;
    return 0;
}

int vfio_map_dma(struct vfio_context *ctx, void *addr, size_t size, uint64_t *out_iova) {
    // Page-align size and start
    size_t page_size = (size_t) sysconf(_SC_PAGESIZE);
    uintptr_t start = (uintptr_t) addr;
    uintptr_t aligned_start = start & ~(page_size - 1);
    size_t offset = start - aligned_start;
    size_t aligned_size = ((size + offset + page_size - 1) / page_size) * page_size;

    if (ctx->next_iova + aligned_size > ctx->iova_end) {
        return -ENOSPC;
    }

    uint64_t iova = ctx->next_iova;

    struct vfio_iommu_type1_dma_map map = {
        .argsz = sizeof(map),
        .flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
        .vaddr = (uint64_t) aligned_start,
        .iova = iova,
        .size = aligned_size,
    };

    if (ioctl(ctx->container_fd, VFIO_IOMMU_MAP_DMA, &map) < 0) {
        return -errno;
    }

    ctx->next_iova += aligned_size;
    *out_iova = iova + offset;
    return 0;
}

int vfio_unmap_dma(struct vfio_context *ctx, uint64_t iova, size_t size) {
    size_t page_size = (size_t) sysconf(_SC_PAGESIZE);
    uint64_t aligned_iova = iova & ~(page_size - 1);
    size_t offset = iova - aligned_iova;
    size_t aligned_size = ((size + offset + page_size - 1) / page_size) * page_size;

    struct vfio_iommu_type1_dma_unmap unmap = {
        .argsz = sizeof(unmap),
        .flags = 0,
        .iova = aligned_iova,
        .size = aligned_size,
    };

    if (ioctl(ctx->container_fd, VFIO_IOMMU_UNMAP_DMA, &unmap) < 0) {
        return -errno;
    }

    return 0;
}