https://jas-hacks.blogspot.com/2024/05/rk3588-reverse-engineering-rknn-running.html



Tiny Devices
Embedded Software Development

Sunday, 19 May 2024
RK3588 - Reverse engineering the RKNN - Running llama2.c with TinyStories
I've finally reached a point with reverse engineering where we can start evaluating the usefulness of the NPU for LMMs. I've crafted a basic library (rk3588-npu) with sufficient features for initial integration. A good reference application for integration is llama2.c because its a single C file, the code structure is straightforward to follow and more importantly modify. We will use TinyStories (stories110m) for testing since the models are relatively small, making it easier to troubleshoot when the outputs go a stray. Credit to karpathy for providing llama2.c

Note, I've set the cpu and npu cores to max clk speeds.



We'll start with running run.c against stories110m, this is the fp32 version using the cpu with a single thread (single core). As we see roughly 9.7 tokens/second.
 
 

 




Next I converted run.c to use fp16 (_Float16) along with the weights. We see a slight drop in performance to roughly 9.3 tokens/s as arithmetic operations require a conversion back to fp32.










As with the fp32 version a single core run at a 100% as the code is single thread.










 

The next step was to offload all FP16 multiplications to the NPU. With a vocabulary size of 32,000, the largest multiplication is 768 x 32,000, with others being 768 x 768, 768 x 2048, and 2048 x 768. For efficient execution, the model weights need to reside entirely in memory, accessible to the NPU. This requires them to be within the 4GB address space, which can be problematic for larger models. In our case, the weights are roughly 256MB, requiring an expansion of the kernel CMA memory allocation to 512MB. Additionally, the weights needed conversion to the NPU format.

The changes result in a additional uplift of roughly 21 to 23 tokens/s depending on the length of the output as per the video below. Conservatively we could say a doubling.



 

CPU fluctuates between 30-60% for the single. The CPU is still critical for a number of reason:

1. We're still having to rely on memory copies to send/receive the remaining data for the multiplication to occur on the NPU.

2. Invocation of the NPU kernel driver requires CPU cycles.

3. The rest of the llam2.c code stills runs of the CPU.



Although the results look promising we need to bear mind that TinyStories is very small model as per the architecture. Furthermore its fortunate that the converted weights can fit in memory without having to shuffle weights between userspace and physical memory. In additional fp16 format would further limit the possibility for larger models to run efficiently. So conclusion so far there is some uplift but mileage will vary depending on model size and number of layers.

Posted by Jas at 11:32 
Email This
BlogThis!
Share to X
Share to Facebook
Share to Pinterest
Labels: llama2.c, NPU, radxa, reverse engineering, RK3588, Rock 5b, rock-5b, Rockchip, TinyStories
3 comments:

sandybeauty20 May 2024 at 22:34
Hi, great job. How can I convert the weights to NPU format? Do you have any tools similar to what llama2.c provides?

Reply
Replies

Jas21 May 2024 at 11:02
As of yet no tool, you use the code from this function https://github.com/mtx512/rk3588-npu/blob/5d86093190c203a62c0259036c2659acc3900e9a/src/npu_matmul.c#L485 . Example here https://github.com/mtx512/rk3588-npu/blob/5d86093190c203a62c0259036c2659acc3900e9a/tests/matmul_fp16_fp16.c#L192


sandybeauty21 May 2024 at 19:20
Thank you.

Reply



Newer PostOlder PostHome
Subscribe to: Post Comments (Atom)
Tiny Devices is sponsored by motiveorder.com
Motiveorder
Contact Me – Looking for consultancy/review or evaluate your product/want to donate hardware
Name
Email *
Message *
Blog Archive
►  2025 (2)
▼  2024 (3)
►  September (1)
▼  May (1)
RK3588 - Reverse engineering the RKNN - Running ll...
►  February (1)
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