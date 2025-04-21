https://jas-hacks.blogspot.com/2024/02/rk3588-reverse-engineering-rknn.html


Tiny Devices
Embedded Software Development

Thursday, 8 February 2024
RK3588 - Reverse engineering the RKNN (Rockchip Neural Processing Unit)


The internal operations and capabilities of the RK3588 NPUs are mainly concealed within a closed-source SDK known as RKNPU2. Given the huge interest in Large Language Models (LLMs) and the quest for optimal matrix multiplications for transformer models. I was curious to understand the implementation of the newly introduced matrix multiplication API (rknn_matmul_run) to the sdk . A thorough examination of the RKNN section in the TRM (Technical Reference Manual) reveals no native mechanism for matrix multiplication, especially for vectors.

To grasp whats going on, the initial step was to understand how the NPU functioned. While the TRM furnished a detailed list of registers and a brief overview of the core units constituting the NPU. It notably lacked essential information on programming the registers for executing operations. For example there were no specifics about deriving or calculating register values based on factors such as data formats (e.g., int8 vs. float16) or the size of input data or weights. Furthermore there was no information on how construct a pipeline for the NPU to execute. Fortunately, I had a slight advantage from a previous reverse engineering attempt on the V831 NPU. Nevertheless, even armed with this knowledge, it has still required several months of trial and error, extensive analysis of data streams, encountering a few dead ends, and numerous attempts at reverse engineering. Finally, I managed to understand how to activate the NPU and get it to execute simple operations.

The RK3588 NPU seems to be distant cousin of the NVDLA architecture in that the some of the terminology is similar and the core units has similar functions and pipe lines to NVDLA although they have been named differently. One of primarily differences is that we can give the NPU a list of tasks (RKNN terminology) to execute and then wait for completion. For example if I have simple neural network consisting of 3 layers and each layer consists of convolution + bias then it is possible to feed 3 tasks (each performing convolution + bias) to the NPU along with the necessary input, weight and bias values. Subsequently we just wait for the NPU to notify when its complete.



The image presented above is extracted from the TRM and has been altered because the description provided in the TRM doesn't entirely align with their diagram, and, more crucially, the register naming convention. Here is my interpretation, each NPU comprises of three distinct units:
CNA - Convolution Network Accelerator (include CORE rectangle). In the TRM it refers to the Neural Network Accelerating Engine, CNA isn't described.
DPU - Data Processing Unit
PPU - Planar Processing Unit
Based on the above, the NPU is primarily designed for running conventional Convolutional Neural Networks (CNNs). This is attributed to the CNA core feature, which revolves around executing convolutions by inputting image or feature data along with the corresponding weights. The emphasis on CNNs is further evident by the majority of RKNPU2 samples provided, such as YOLOX, Mobilenet, and ResNet. The CNA output can be directed to the DPU, where element-wise operations such as addition, multiplication, and RELU can be carried out. Subsequently, the DPU's output can be channeled to the PPU, where operations like min, max, and average pooling are executed. Additionally, there is the option to directly feed data to the DPU or PPU without necessitating a convolution step.

To execute convolutions efficiently, the CNA employs multiply-accumulate (MAC) operations. The performance of a CNA is partially determined by the number of MAC units used. According to the TRM, for a single NPU core the count of MAC operations depends on the input data type:

1024 int8 MAC operations per cycle
512 float16 MAC operations per cycle
Each MAC cell caches 1x1x16 weight bytes, for int 8 its 16 values whilst for float16 it  reduces to 8. We require 2 MAC cells to perform float 16 hence the reduction in operations per cycle. Internally feature and weight data must conform to Rockchips NC1HWC2 format where C2 is the aforementioned value. One 1x1x16 cube of feature data is then shared by all MAC cells to calculate partial sums which are then sent to the accumulator. At higher level the CNA appears to execute a block operation, as observed in my tests where, for instance, the MAC caches 32 channels of weight data for fp16. Hence the requirement to layout weights in kernel groups each with 32 channels.

 
Performance is also affected by the access time to input and weight data, the CNA incorporates a second level cache known as convolution buffer (cbuf). In the above diagram the 384KB onboard memory is partly for that purpose. Importantly the numbers of MAC units plus the cbuf influence how large of a convolution can be completed in one task.
Some of you may have already deduced that the matrix multiplication API is essentially executed through a 2D convolution. For instance, let's consider matrix A as [M x K] and matrix B as [K x N]. Matrix A represents the feature data arranged in an Mx1xK (hwc) format, while matrix B denotes the weight data organized in a 1x1xNxK (hwck) format. Consequently, the resulting matrix C [M x N] is arranged as Mx1xN. I'm at the point where I have a simple test running which asks the NPU to perform a matrix multiplication. I'm using matrices data derived from a GGML testcase (test-mul-mat.cpp) to verify the output is correct. To run the test check out my repo and build, sadly I'm still testing against a kernel 5.10 on a Rock-5b. If the test runs output should be as below & screenshot above.

rock@rock-5b:~/rk3588-npu/build$ ./matmul_4_36_16
drm name is rknpu - 20220829 - RKNPU driver
input dma is ffff8000, output dma is ffffa000, weights dma is ffff9000
Size of npu_regs 112
RKNPU_SUBMIT returned 0
=========================================================================================================
 1224.0 1023.0 1158.0 1259.0 1359.0 1194.0 1535.0 1247.0 1185.0 1029.0  889.0 1182.0  955.0 1179.0 1147.0
 1216.0 1087.0 1239.0 1361.0 1392.0 1260.0 1247.0 1563.0 1167.0 1052.0  942.0 1214.0 1045.0 1134.0 1264.0
 1125.0  966.0 1079.0 1333.0 1287.0 1101.0 1185.0 1167.0 1368.0  990.0  967.0 1121.0  971.0 1086.0 1130.0
  999.0  902.0 1020.0 1056.0 1076.0  929.0 1029.0 1052.0  990.0 1108.0  823.0  989.0  759.0 1041.0 1003.0
=========================================================================================================

Regarding reverse engineering, I've reached a stage where I understand the majority of register settings that impact convolution when dealing with feature data as input. The primary uncertainty lies in determining the bank sizes for feature/weight data, however I'm hopeful that this can be deduced. After dedicating a significant amount of time to analyzing the NPU, here is a list of key areas that you should be aware of:

1. All data pointers within the NPU (e.g., input, weights, outputs, task lists) are 32-bit and must reference physical memory. Consequently, this restricts the memory range to 4GB, making it impractical to leverage a board with 16/32GB memory for the NPU to use. Moreover, it potentially imposes limitations on the types of models that can be executed on the NPU.
2. The claim of 6 TOPS should be approached with caution. While each NPU core is rated at 2 TOPS, there are registers that could potentially enable convolution across all 3 cores. However, after analyzing the data streams generated by the SDK, it appears that this feature is never utilized. Additionally, there doesn't seem to be a similar capability available for the DPU/PPU units, which would restrict its usability. In my view, the most effective approach is to treat them as individual cores and execute models on each one, taking into account the memory constraints mentioned earlier.

3. The SDK matrix multiplication API, in certain aspects, represents an inefficient utilization of the NPU. There is the overhead of memory allocation, a kernel call, and instructing the NPU to execute a single convolution. Ideally, the NPU should be tasked with executing multiple operations and providing all the supplied data for those operations. Typically this is how the NPU is utilized when running a CNN model (ie YOLOvX). The caveat here is that the converted model is limited to contains layers where the operations are supported by the NPU.

4. Initial bench marking for the multiplication of two fp16 [512 x 512] matrices suggests that I could achieve completion in a respectable time of around 1ms. Please note, this involves sending 2 tasks to the NPU, as mentioned earlier due to the cbuf limitation. Unfortunately, this is only part of the story when it comes using vectors data as input. The costly operations involve converting the matrices to feature and weight data formats, and vice versa for the output, if done at runtime. I made an effort to create a highly optimized conversion routine for vector to feature data conversion. According to my benchmarks, this process takes approximately 2ms for fp16 [512 x 512] matrices. I would estimate 12-15ms to perform all the conversions for the matrices mentioned above. Ideally, the matrix for the weight data should be converted ahead of time to reduce conversion overhead and, if possible, persisted for reuse.

5. I was hoping there was the capability to use a programmable core to perform custom operations. Unfortunately this isn't case and your left with using OpenCL as the alternative. This brings it own challenges if you need to shuffle data between OpenCL and the NPU.

There is still more to discover about the other units (DPU/NPU) and I'll spend time doing that. Lastly TRM v1.0 contains numerous gaps and inconsistencies for RKNN, if anyone has later version it would be greatly appreciated.

Posted by Jas at 07:46 
Email This
BlogThis!
Share to X
Share to Facebook
Share to Pinterest
Labels: NPU, reverse engineering, RK3588, RKNN, Rock 5b, rock-5b, Rockchip
No comments:

Post a Comment


Newer PostOlder PostHome
Subscribe to: Post Comments (Atom)
Tiny Devices is sponsored by motiveorder.com
Motiveorder
Contact Me – Looking for consultancy/review or evaluate your product/want to donate hardware
Name
Email *
Message *
Blog Archive
▼  2024 (3)
►  September (1)
►  May (1)
▼  February (1)
RK3588 - Reverse engineering the RKNN (Rockchip Ne...
►  2023 (3)
►  2022 (1)
►  2021 (1)
►  2020 (1)
►  2019 (5)
►  2018 (3)
►  2017 (8)
►  2016 (4)
►  2015 (3)
►  2014 (8)
►  2013 (10)
►  2012 (20)
Simple theme. Powered by Blogger.