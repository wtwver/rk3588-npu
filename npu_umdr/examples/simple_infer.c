#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "npu_umdr.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <PCI_BDF>\n", argv[0]);
        fprintf(stderr, "Example: %s 0000:65:00.0\n", argv[0]);
        return 2;
    }

    const char *bdf = argv[1];

    struct npu_device *dev = NULL;
    int ret = npu_device_open(bdf, &dev);
    if (ret) {
        fprintf(stderr, "Failed to open device %s: %s\n", bdf, strerror(-ret));
        return 1;
    }

    struct npu_dma_buf in = {0}, out = {0};

    ret = npu_alloc_dma(dev, 4096, &in);
    if (ret) {
        fprintf(stderr, "alloc in failed: %s\n", strerror(-ret));
        npu_device_close(dev);
        return 1;
    }

    ret = npu_alloc_dma(dev, 4096, &out);
    if (ret) {
        fprintf(stderr, "alloc out failed: %s\n", strerror(-ret));
        npu_free_dma(dev, &in);
        npu_device_close(dev);
        return 1;
    }

    // Fill input buffer with a recognizable pattern
    memset(in.host_ptr, 0xAB, in.size_bytes);
    memset(out.host_ptr, 0x00, out.size_bytes);

    ret = npu_submit_simple(dev, &in, &out);
    if (ret) {
        fprintf(stderr, "submit failed: %s\n", strerror(-ret));
    } else {
        printf("submit succeeded. First 8 bytes of output: ");
        for (int i = 0; i < 8; ++i) {
            printf("%02X ", ((unsigned char*)out.host_ptr)[i]);
        }
        printf("\n");
    }

    npu_free_dma(dev, &out);
    npu_free_dma(dev, &in);
    npu_device_close(dev);
    return ret ? 1 : 0;
}