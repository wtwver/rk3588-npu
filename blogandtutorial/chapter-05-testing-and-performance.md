# Chapter 5 — Testing, Constraints, and Performance

This chapter consolidates how to run tests, interpret constraints, and improve performance.

## Running Built-in Tests
```bash
meson test -C build | cat
# or run individual
meson test -C build 'matmul int8 1x64x64' | cat
```
Sizes and cases are defined in `meson.build`.

## Input Constraints (from tests)
- FP16 path:
  - `M` ∈ {1 or multiple of 4}
  - `K` multiple of 32
  - `N` multiple of 16
- INT8 path: similar multiples, see `tests/matmul_int8.c`
- Max sizes seen: `M<=384, K<=4096, N<=4096` (adjust per hardware limits)

## Validating Correctness
- Compare with CPU reference (see `matmul_fp16.c: matmul_fp32`)
- For larger sizes, compare statistical metrics (mean abs err) if exact match isn’t required due to quantization

## Performance Tips
- Align buffers to cache-line or page boundaries when possible
- Prefer multiples that match hardware grains: `K%32==0`, `N%16==0`, `M%4==0`
- Reuse persistent buffers to avoid repeated allocation overhead
- Batch multiple tasks in one submission where supported
- Pin CPU affinity to reduce jitter during benchmarking

## ASCII Diagram — Benchmark Loop
```
for size in SIZES:
  fill(A,B)
  gen_ops(size)
  submit()
  wait()
  validate(C)
  record_time()
```

## Where to Go Next
- Explore `src/npu_matmul.c` deeper to extend to convolution or fused ops
- Add new Meson test targets with representative workloads
- Integrate with a higher-level runtime for model execution