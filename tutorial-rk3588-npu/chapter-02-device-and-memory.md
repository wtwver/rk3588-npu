# Chapter 2 — Device I/O and DMA Memory

This chapter covers opening the NPU device, allocating DMA buffers, and performing cleanup via the public API in `include/npu_interface.h`.

## Key APIs
- `int npu_open();`
- `int npu_close(int fd);`
- `int npu_reset(int fd);`
- `void* mem_allocate(int fd, size_t size, uint64_t *dma_addr, uint64_t *obj, uint32_t flags, uint32_t *handle);`
- `void mem_destroy(int fd, uint32_t handle, uint64_t obj_addr);`

## Steps

1) Open device and reset
```c
int fd = npu_open();
if (fd < 0) { /* handle error */ }
int ret = npu_reset(fd);
```

2) Allocate DMA buffers
```c
uint64_t buf_dma, buf_obj;
uint32_t buf_handle;
size_t   buf_size = 4096;
void *buf = mem_allocate(fd, buf_size, &buf_dma, &buf_obj, 0, &buf_handle);
// Write into buf, device reads via buf_dma
```

3) Destroy buffers and close device
```c
mem_destroy(fd, buf_handle, buf_obj);
npu_close(fd);
```

## ASCII Diagram — Buffer Lifecyle
```
+---------+    open    +------------------+    allocate    +--------------------+
|  App    +----------->|  npu_interface.c +--------------->|  DRM (rknpu)       |
+---------+            +------------------+                +--------------------+
     |                         |  returns map, dma addr              ^
     | write/read via map      |                                     |
     v                         |  destroy(handle,obj)                 |
+------------------------------+-------------------------------------+
```

## Notes
- `mem_allocate` sets `RKNPU_MEM_NON_CACHEABLE` internally; pass `RKNPU_MEM_KERNEL_MAPPING` for kernel tasks buffer when needed
- Always pair every allocation with `mem_destroy`

## Next
Proceed to Chapter 3 for generating matmul commands with `npu_matmul`.