#ifndef NPU_UMDR_H
#define NPU_UMDR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque device handle
struct npu_device;

// DMA buffer descriptor
struct npu_dma_buf {
    void *host_ptr;      // Userspace pointer
    uint64_t iova;       // IO virtual address visible to the device
    size_t size_bytes;   // Size of the mapping
};

// Open an NPU device by PCI BDF (e.g., "0000:65:00.0").
// Returns 0 on success, negative errno otherwise.
int npu_device_open(const char *pci_bdf, struct npu_device **out_device);

// Close and free resources
void npu_device_close(struct npu_device *device);

// Allocate a DMA-capable buffer and map it to an IOVA for the device.
// The buffer will be page-aligned. Returns 0 on success.
int npu_alloc_dma(struct npu_device *device, size_t size_bytes, struct npu_dma_buf *out_buf);

// Free a DMA buffer and unmap from IOMMU
int npu_free_dma(struct npu_device *device, struct npu_dma_buf *buf);

// MMIO access helpers (32-bit)
uint32_t npu_mmio_read32(struct npu_device *device, uint32_t offset);
void npu_mmio_write32(struct npu_device *device, uint32_t offset, uint32_t value);

// Example: submit a simple command where the hardware expects input/output IOVA
// in specific registers and a doorbell write to kick the engine.
// This is a template – adjust register layout for your hardware.
int npu_submit_simple(struct npu_device *device, const struct npu_dma_buf *input, const struct npu_dma_buf *output);

#ifdef __cplusplus
}
#endif

#endif // NPU_UMDR_H