clear
cd build && ninja && cd ../
ninja -C build test
# gdb -x matmul.gdb --args ./build/matmul_fp16 4 32 16