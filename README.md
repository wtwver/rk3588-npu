# Know issues
```
ninja -C build test
./matmul_fp16_fp16 1 8192 $((16*485))
Segmentation fault
```

# Running llama.c
```
git clone https://github.com/karpathy/llama2.c
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin
cd llama2.c
make run
./run stories110M.bin
```

# rk3588-npu
Reverse engineering the rk3588 npu.

Internals of the NPU, similarity to NVDLA and matrix muplication support covered in blog [post](http://jas-hacks.blogspot.com/2024/02/rk3588-reverse-engineering-rknn.html).
Example integration llama2.c running tinystories covered in blog [post](http://jas-hacks.blogspot.com/2024/05/rk3588-reverse-engineering-rknn-running.html)

To build :
```
sudo apt install -y meson libdrm-dev

mkdir -p build
meson build
cd build
ninja
```

To run tests (tested against 5.10 kernel) :
```
ninja -C build test
```
