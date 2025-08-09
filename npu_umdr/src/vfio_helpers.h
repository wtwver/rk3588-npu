#ifndef NPU_VFIO_HELPERS_H
#define NPU_VFIO_HELPERS_H

#include <linux/vfio.h>
#include <stdint.h>
#include <stddef.h>

struct vfio_context {
    int container_fd;
    int group_fd;
    int device_fd;

    // BAR0 mapping
    void *bar0;
    size_t bar0_size;

    // IOVA allocation state
    uint64_t next_iova;
    uint64_t iova_end;
};

// Discover IOMMU group for a PCI BDF and open VFIO fds, set up IOMMU
int vfio_init_for_bdf(const char *bdf, struct vfio_context *out);

// Tear down VFIO
void vfio_destroy(struct vfio_context *ctx);

// Map BAR0 into userspace
int vfio_map_bar0(struct vfio_context *ctx);

// Map a userspace buffer into device IOVA
int vfio_map_dma(struct vfio_context *ctx, void *addr, size_t size, uint64_t *out_iova);

// Unmap a previously mapped IOVA
int vfio_unmap_dma(struct vfio_context *ctx, uint64_t iova, size_t size);

#endif // NPU_VFIO_HELPERS_H