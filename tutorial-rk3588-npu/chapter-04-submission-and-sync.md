# Chapter 4 — Submitting Tasks and Synchronization

Use the tests in `tests/matmul_fp16.c` and `tests/matmul_int8.c` as practical references. After generating command buffers and tasks, the program sets up `rknpu_task` and submits via ioctl.

## Core Flow (simplified)
```c
// 1) Open, allocate regcmd buffer and tasks buffer
uint64_t  regcmd_dma, regcmd_obj;  uint32_t regcmd_handle;
uint64_t *regcmd = mem_allocate(fd, 1024, &regcmd_dma, &regcmd_obj, 0, &regcmd_handle);

uint64_t  tasks_dma, tasks_obj;    uint32_t tasks_handle;
struct rknpu_task *tasks = mem_allocate(fd, 1024, &tasks_dma, &tasks_obj, RKNPU_MEM_KERNEL_MAPPING, &tasks_handle);

// 2) Fill input/weights, generate ops into regcmd and tasks
//    via gen_matmul_fp16(...) or gen_matmul_int8(...)

// 3) Submit using DRM_IOCTL_RKNPU_SUBMIT (see tests for struct fields)
// ioctl(fd, DRM_IOCTL_RKNPU_SUBMIT, &submit);

// 4) Wait for completion if needed, then read output buffer

// 5) Cleanup: mem_destroy(...), npu_close(fd)
```

Consult `src/npu_matmul.c` for how the `ops` array is populated with registers like `CNA_*`, `CORE_*`, and `DPU_*`, and how `OP_ENABLE` and PC registers are used to kick off execution.

## ASCII Diagram — Submission Sequence
```
App           npu_matmul        npu_interface        rknpu (kernel)
 |                |                   |                    |
 |  gen params    |                   |                    |
 |--------------->|                   |                    |
 |  ops/tasks     |                   |                    |
 |<---------------|                   |                    |
 |                |   DRM ioctl       |                    |
 |----------------------------------->|  SUBMIT            |
 |                |                   |------------------->|
 |                |                   |       run          |
 |                |                   |<-------------------|
 |                |   read output     |                    |
 v                v                   v                    v
```

## Validation
- Compare output buffer against a CPU reference (see `matmul_fp16.c` function `matmul_fp32`)
- Use small sizes initially (e.g., 1x32x16) before scaling up

## Next
Proceed to Chapter 5 for testing, constraints, and performance hints.