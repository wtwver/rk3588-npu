# NPU User-Mode Driver (VFIO-based)

This is a minimal, educational user-mode driver (UMD) for a PCIe-attached NPU on Linux using VFIO. It demonstrates:

- Discovering the IOMMU group for a PCI BDF
- Setting up a VFIO container and group
- Mapping BAR0 into userspace
- Allocating userspace buffers and mapping them into device IOVA space
- Writing MMIO registers to submit a simple command

It is a template – you must replace the example register layout with your NPU's actual programming model.

## Build

Requires Linux kernel headers providing VFIO userspace API (e.g., `linux-headers-$(uname -r)`).

```
make -C npu_umdr
```

Artifacts:
- `build/libnpuumdr.a`: static library
- `build/simple_infer`: example program

## Run (example)

- Bind the device to `vfio-pci` and ensure you have the necessary permissions (root or proper udev rules).
- Run the example with your device BDF:

```
sudo ./build/simple_infer 0000:65:00.0
```

Note: The example `npu_submit_simple` uses placeholder register offsets. Update `src/npu_umdr.c` to match your hardware.

## Public API

See `include/npu_umdr.h` for:
- `npu_device_open` / `npu_device_close`
- `npu_alloc_dma` / `npu_free_dma`
- `npu_mmio_read32` / `npu_mmio_write32`
- `npu_submit_simple`

## Adapting to Your Hardware

- Replace the register offsets and command protocol in `src/npu_umdr.c`.
- Optionally add MSI/MSI-X interrupt handling using eventfd/epoll for completion instead of polling.
- Consider adding a ring buffer submission queue, command tracking, and timeouts.

## Security and Stability

Running a userspace driver with VFIO gives direct device access. Ensure isolation through IOMMU and restrict access to trusted users.