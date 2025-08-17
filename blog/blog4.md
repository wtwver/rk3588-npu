https://jas-hacks.blogspot.com/2023/04/rk3588-adventures-with-external-gpu.html


Tiny Devices
Embedded Software Development

Sunday, 30 April 2023
RK3588 - Adventures with an external GPU through PCIE Gen3 x4 (Radxa Rock-5b)
One of the interesting features of the RK3588 is the pcie controller because of it support for a Gen3 X4 link. I'd started looking into using the controller for a forth coming project and subsequently this lead me to the idea of testing the controller against a external GPU card to gain an understanding of it limitations and potential. From what I understand Jeff Geerling has been a similar journey with the RPI CM4 and has had limited success with help from numerous developers. Furthermore there was a Radxa tweet which a gave a teasing glimpse of the working GPU. So lets see what is or isn't possible using a Rock-5b.

 


 

I'd managed to get hold of a Radeon R7 520 (XFX R7 250 low-profile) card along a with M.2 Key M Extender Cable to PCIE x16 Graphics Card Riser Adapter. To power the card I'd reused a old LR1007 120W 12VDC ATX board which was to hand. Setup as shown below, we reuse the nvme slot for the m.2 adapter and revert back to an sd card for booting an OS. I'd used the Radxa debian image with a custom compiled Radxa kernel to include the graphics card drivers and fixes. Having reviewed the pcie BAR definitions in the rk3588.dtsi there should be enough address space available for the card to use. After removing the hdmi and mali drivers from kernel config, I initially tried the amdgpu driver but that seems to report an error and no display output

[   11.844163] amdgpu 0000:01:00.0: [drm:amdgpu_ring_test_helper [amdgpu]] *ERROR* ring gfx test failed (-110)
[   11.844378] [drm:amdgpu_device_init [amdgpu]] *ERROR* hw_init of IP block <gfx_v6_0> failed -110
[   11.844383] amdgpu 0000:01:00.0: amdgpu: amdgpu_device_ip_init failed
[   11.844388] amdgpu 0000:01:00.0: amdgpu: Fatal error during GPU init
[   11.844414] amdgpu 0000:01:00.0: amdgpu: amdgpu: finishing device.
[   11.846559] [drm] amdgpu: ttm finalized
[   11.848018] amdgpu: probe of 0000:01:00.0 failed with error -110

The radeon driver fared slightly better with a similar error but at least display output for console login



[   12.059398] [drm:r600_ring_test [radeon]] *ERROR* radeon: ring 0 test failed (scratch(0x850C)=0xCAFEDEAD)
[   12.059408] radeon 0000:01:00.0: disabling GPU acceleration

The was puzzling as the card relies on pcie memory mapped I/O which the RK3588 should see a standard memory and be able to read/write too. It turns out Peter Geis who was attempting to mainline a pcie driver for the RK3566 and raised 2 issues per this thread which Rockchip replied too. The same issues weren't improved/fixed on the RK3588 as mentioned here . In simple terms for our requirements:

1. For the pcie dma transfers memory allocation are limited to 32bits so a 4GB board might not see an issue. While a 8GB board like mine the kernel could pick an address range above 4GB.

2. AMD cards rely on pcie snooping, there is no CPU snooping on the RK3588 interconnect. So any cache copies of the same device memory block won't get updated to remain in sync.

If we hack the Radeon driver to work around these issues we get:


[   12.529087] [drm] ring test on 0 succeeded in 1 usecs
[   12.529094] [drm] ring test on 1 succeeded in 1 usecs
[   12.529102] [drm] ring test on 2 succeeded in 1 usecs
[   12.529121] [drm] ring test on 3 succeeded in 8 usecs
[   12.529132] [drm] ring test on 4 succeeded in 3 usecs
[   12.706419] [drm] ring test on 5 succeeded in 2 usecs
[   12.706427] [drm] UVD initialized successfully.
[   12.816582] [drm] ring test on 6 succeeded in 18 usecs
[   12.816625] [drm] ring test on 7 succeeded in 5 usecs
[   12.816627] [drm] VCE initialized successfully.
[   12.816879] [drm:si_irq_set [radeon]] si_irq_set: sw int gfx
[   12.816921] [drm] ib test on ring 0 succeeded in 0 usecs
[   12.816989] [drm:si_irq_set [radeon]] si_irq_set: sw int cp1
[   12.817028] [drm] ib test on ring 1 succeeded in 0 usecs
[   12.817088] [drm:si_irq_set [radeon]] si_irq_set: sw int cp2
[   12.817127] [drm] ib test on ring 2 succeeded in 0 usecs
[   12.817185] [drm:si_irq_set [radeon]] si_irq_set: sw int dma
[   12.817224] [drm] ib test on ring 3 succeeded in 0 usecs
[   12.817281] [drm:si_irq_set [radeon]] si_irq_set: sw int dma1
[   12.817319] [drm] ib test on ring 4 succeeded in 0 usecs
[   13.477677] [drm] ib test on ring 5 succeeded
[   13.984454] [drm] ib test on ring 6 succeeded
[   14.491404] [drm] ib test on ring 7 succeeded
...

[   14.549296] [drm] Initialized radeon 2.50.0 20080528 for 0000:01:00.0 on minor 1


 So potentially we have graphics acceleration ... let try kmstest


rock@rock-5b:~$ kmstest
trying to open device 'i915'...failed
trying to open device 'amdgpu'...failed
trying to open device 'radeon'...done
main: All ok!

Next (fingers crossed) kmscube


rock@rock-5b:~$ kmscube
Using display 0x55b67f0020 with EGL version 1.5
===================================
EGL information:
  version: "1.5"
  vendor: "Mesa Project"
  client extensions: "EGL_EXT_device_base EGL_EXT_device_enumeration EGL_EXT_device_query EGL_EXT_platform_base EGL_KHR_client_get_all_proc_addresses EGL_EXT_client_extensions EGL_KHR_debug EGL_EXT_platform_device EGL_EXT_platform_wayland EGL_KHR_platform_wayland EGL_EXT_platform_x11 EGL_KHR_platform_x11 EGL_MESA_platform_gbm EGL_KHR_platform_gbm EGL_MESA_platform_surfaceless"
  display extensions: "EGL_ANDROID_blob_cache EGL_EXT_buffer_age EGL_EXT_create_context_robustness EGL_EXT_image_dma_buf_import EGL_EXT_image_dma_buf_import_modifiers EGL_KHR_cl_event2 EGL_KHR_config_attribs EGL_KHR_create_context EGL_KHR_create_context_no_error EGL_KHR_fence_sync EGL_KHR_get_all_proc_addresses EGL_KHR_gl_colorspace EGL_KHR_gl_renderbuffer_image EGL_KHR_gl_texture_2D_image EGL_KHR_gl_texture_3D_image EGL_KHR_gl_texture_cubemap_image EGL_KHR_image EGL_KHR_image_base EGL_KHR_image_pixmap EGL_KHR_no_config_context EGL_KHR_reusable_sync EGL_KHR_surfaceless_context EGL_EXT_pixel_format_float EGL_KHR_wait_sync EGL_MESA_configless_context EGL_MESA_drm_image EGL_MESA_image_dma_buf_export EGL_MESA_query_driver EGL_WL_bind_wayland_display "
===================================
OpenGL ES 2.x information:
  version: "OpenGL ES 3.2 Mesa 20.3.5"
  shading language version: "OpenGL ES GLSL ES 3.20"
  vendor: "AMD"
  renderer: "AMD VERDE (DRM 2.50.0, 5.10.110-99-rockchip-g6e21553c2116, LLVM 11.0.1)"
  extensions: "GL_EXT_blend_minmax GL_EXT_multi_draw_arrays GL_EXT_texture_filter_anisotropic GL_EXT_texture_compression_s3tc GL_EXT_texture_compression_dxt1 GL_EXT_texture_compression_rgtc GL_EXT_texture_format_BGRA8888 GL_OES_compressed_ETC1_RGB8_texture GL_OES_depth24 GL_OES_element_index_uint GL_OES_fbo_render_mipmap GL_OES_mapbuffer GL_OES_rgb8_rgba8 GL_OES_standard_derivatives GL_OES_stencil8 GL_OES_texture_3D GL_OES_texture_float GL_OES_texture_float_linear GL_OES_texture_half_float GL_OES_texture_half_float_linear GL_OES_texture_npot GL_OES_vertex_half_float GL_EXT_draw_instanced GL_EXT_texture_sRGB_decode GL_OES_EGL_image GL_OES_depth_texture GL_AMD_performance_monitor GL_OES_packed_depth_stencil GL_EXT_texture_type_2_10_10_10_REV GL_NV_conditional_render GL_OES_get_program_binary GL_APPLE_texture_max_level GL_EXT_discard_framebuffer GL_EXT_read_format_bgra GL_EXT_frag_depth GL_NV_fbo_color_attachments GL_OES_EGL_image_external GL_OES_EGL_sync GL_OES_vertex_array_object GL_OES_viewport_array GL_ANGLE_pack_reverse_row_order GL_ANGLE_texture_compression_dxt3 GL_ANGLE_texture_compression_dxt5 GL_EXT_occlusion_query_boolean GL_EXT_robustness GL_EXT_texture_rg GL_EXT_unpack_subimage GL_NV_draw_buffers GL_NV_read_buffer GL_NV_read_depth GL_NV_read_depth_stencil GL_NV_read_stencil GL_EXT_draw_buffers GL_EXT_map_buffer_range GL_KHR_debug GL_KHR_robustness GL_KHR_texture_compression_astc_ldr GL_NV_pixel_buffer_object GL_OES_depth_texture_cube_map GL_OES_required_internalformat GL_OES_surfaceless_context GL_EXT_color_buffer_float GL_EXT_sRGB_write_control GL_EXT_separate_shader_objects GL_EXT_shader_group_vote GL_EXT_shader_implicit_conversions GL_EXT_shader_integer_mix GL_EXT_tessellation_point_size GL_EXT_tessellation_shader GL_ANDROID_extension_pack_es31a GL_EXT_base_instance GL_EXT_compressed_ETC1_RGB8_sub_texture GL_EXT_copy_image GL_EXT_draw_buffers_indexed GL_EXT_draw_elements_base_vertex GL_EXT_gpu_shader5 GL_EXT_polygon_offset_clamp GL_EXT_primitive_bounding_box GL_EXT_render_snorm GL_EXT_shader_io_blocks GL_EXT_texture_border_clamp GL_EXT_texture_buffer GL_EXT_texture_cube_map_array GL_EXT_texture_norm16 GL_EXT_texture_view GL_KHR_blend_equation_advanced GL_KHR_context_flush_control GL_KHR_robust_buffer_access_behavior GL_NV_image_formats GL_OES_copy_image GL_OES_draw_buffers_indexed GL_OES_draw_elements_base_vertex GL_OES_gpu_shader5 GL_OES_primitive_bounding_box GL_OES_sample_shading GL_OES_sample_variables GL_OES_shader_io_blocks GL_OES_shader_multisample_interpolation GL_OES_tessellation_point_size GL_OES_tessellation_shader GL_OES_texture_border_clamp GL_OES_texture_buffer GL_OES_texture_cube_map_array GL_OES_texture_stencil8 GL_OES_texture_storage_multisample_2d_array GL_OES_texture_view GL_EXT_blend_func_extended GL_EXT_buffer_storage GL_EXT_float_blend GL_EXT_geometry_point_size GL_EXT_geometry_shader GL_EXT_shader_samples_identical GL_KHR_no_error GL_KHR_texture_compression_astc_sliced_3d GL_OES_EGL_image_external_essl3 GL_OES_geometry_point_size GL_OES_geometry_shader GL_OES_shader_image_atomic GL_EXT_clip_cull_distance GL_EXT_disjoint_timer_query GL_EXT_texture_compression_s3tc_srgb GL_EXT_window_rectangles GL_MESA_shader_integer_functions GL_EXT_clip_control GL_EXT_color_buffer_half_float GL_EXT_memory_object GL_EXT_memory_object_fd GL_EXT_texture_compression_bptc GL_KHR_parallel_shader_compile GL_NV_alpha_to_coverage_dither_control GL_EXT_EGL_image_storage GL_EXT_texture_sRGB_R8 GL_EXT_texture_shadow_lod GL_INTEL_blackhole_render GL_MESA_framebuffer_flip_y GL_EXT_depth_clamp GL_EXT_texture_query_lod "
===================================
Using modifier ffffffffffffff
Modifiers failed!
Bus error
The 'bus error' indicates a memory alignment issue and turns out to be a bit of a of rabbit hole. To fix the Radeon kernel driver we are ensuring the cards memory is mapped as 'Device memory' type Device-nGnRnE. If it were 'Normal Memory' then unaligned access is allowed. This implies fixing up userspace drivers/applications as these errors are encountered as these applications can directly manlipulate the cards memory. For this particular bus error it was caused by a memcpy in the radeon gallium driver and fixed applied there and as shown in the video kmscube runs
===================================
Using modifier ffffffffffffff
Modifiers failed!
Using modifier ffffffffffffff
Modifiers failed!
Rendered 120 frames in 2.000246 sec (59.992635 fps)
Rendered 240 frames in 4.000428 sec (59.993577 fps)
Rendered 361 frames in 6.016865 sec (59.998019 fps)
Rendered 481 frames in 8.017015 sec (59.997390 fps)
Rendered 601 frames in 10.017050 sec (59.997704 fps)
Rendered 721 frames in 12.017079 sec (59.997942 fps)
Rendered 841 frames in 14.017118 sec (59.998067 fps)
Rendered 961 frames in 16.017314 sec (59.997574 fps)
Rendered 1082 frames in 18.033850 sec (59.998280 fps)
Similiar fixes were applied to glmark2-drm & glmark2-es2-drm to run successfully (1680x1050 resolution) although the terrain scene displayed a bunch of colored bars on the screen.
=======================================================
    glmark2 2021.12
=======================================================
    OpenGL Information
    GL_VENDOR:      AMD
    GL_RENDERER:    AMD VERDE (DRM 2.50.0, 5.10.110-99-rockchip-g6e21553c2116, LLVM 11.0.1)
    GL_VERSION:     4.5 (Compatibility Profile) Mesa 20.3.5
    Surface Config: buf=32 r=8 g=8 b=8 a=8 depth=24 stencil=0 samples=0
    Surface Size:   1680x1050 fullscreen
=======================================================
[build] use-vbo=false: FPS: 939 FrameTime: 1.066 ms
[build] use-vbo=true: FPS: 2411 FrameTime: 0.415 ms
[texture] texture-filter=nearest: FPS: 1957 FrameTime: 0.511 ms
[texture] texture-filter=linear: FPS: 1958 FrameTime: 0.511 ms
[texture] texture-filter=mipmap: FPS: 2003 FrameTime: 0.499 ms
[shading] shading=gouraud: FPS: 1975 FrameTime: 0.506 ms
[shading] shading=blinn-phong-inf: FPS: 1973 FrameTime: 0.507 ms
[shading] shading=phong: FPS: 1976 FrameTime: 0.506 ms
[shading] shading=cel: FPS: 1974 FrameTime: 0.507 ms
[bump] bump-render=high-poly: FPS: 1739 FrameTime: 0.575 ms
[bump] bump-render=normals: FPS: 2373 FrameTime: 0.422 ms
[bump] bump-render=height: FPS: 2330 FrameTime: 0.429 ms
[effect2d] kernel=0,1,0;1,-4,1;0,1,0;: FPS: 1254 FrameTime: 0.798 ms
[effect2d] kernel=1,1,1,1,1;1,1,1,1,1;1,1,1,1,1;: FPS: 707 FrameTime: 1.415 ms
[pulsar] light=false:quads=5:texture=false: FPS: 1338 FrameTime: 0.747 ms
[desktop] blur-radius=5:effect=blur:passes=1:separable=true:windows=4: FPS: 456 FrameTime: 2.194 ms
[desktop] effect=shadow:windows=4: FPS: 600 FrameTime: 1.667 ms
[buffer] columns=200:interleave=false:update-dispersion=0.9:update-fraction=0.5:update-method=map: FPS: 214 FrameTime: 4.684 ms
[buffer] columns=200:interleave=false:update-dispersion=0.9:update-fraction=0.5:update-method=subdata: FPS: 233 FrameTime: 4.306 ms
[buffer] columns=200:interleave=true:update-dispersion=0.9:update-fraction=0.5:update-method=map: FPS: 347 FrameTime: 2.885 ms
[ideas] speed=duration: FPS: 1430 FrameTime: 0.700 ms
[jellyfish] <default>: FPS: 806 FrameTime: 1.242 ms
[terrain] <default>: FPS: 150 FrameTime: 6.706 ms
[shadow] <default>: FPS: 843 FrameTime: 1.188 ms
[refract] <default>: FPS: 115 FrameTime: 8.718 ms
[conditionals] fragment-steps=0:vertex-steps=0: FPS: 1970 FrameTime: 0.508 ms
[conditionals] fragment-steps=5:vertex-steps=0: FPS: 1980 FrameTime: 0.505 ms
[conditionals] fragment-steps=0:vertex-steps=5: FPS: 1972 FrameTime: 0.507 ms
[function] fragment-complexity=low:fragment-steps=5: FPS: 1979 FrameTime: 0.505 ms
[function] fragment-complexity=medium:fragment-steps=5: FPS: 1971 FrameTime: 0.507 ms
[loop] fragment-loop=false:fragment-steps=5:vertex-steps=5: FPS: 1972 FrameTime: 0.507 ms
[loop] fragment-steps=5:fragment-uniform=false:vertex-steps=5: FPS: 1972 FrameTime: 0.507 ms
[loop] fragment-steps=5:fragment-uniform=true:vertex-steps=5: FPS: 1968 FrameTime: 0.508 ms
=======================================================
                                  glmark2 Score: 1450
=======================================================

Next up was to see if startx would run, unfortunately it drops out with a shader compiler error. Looks like glamor is using egl but encounters an opengl shader to compile, requires further investigation.

[  7916.924] (II) modeset(0): Modeline "360x202"x119.0   11.25  360 372 404 448  202 204 206 211 doublescan -hsync +vsync (25.1 kHz d)
[  7916.924] (II) modeset(0): Modeline "360x202"x118.3   10.88  360 384 400 440  202 204 206 209 doublescan +hsync -vsync (24.7 kHz d)
[  7916.924] (II) modeset(0): Modeline "320x180"x119.7    9.00  320 332 360 400  180 181 184 188 doublescan -hsync +vsync (22.5 kHz d)
[  7916.924] (II) modeset(0): Modeline "320x180"x118.6    8.88  320 344 360 400  180 181 184 187 doublescan +hsync -vsync (22.2 kHz d)
[  7916.925] (II) modeset(0): Output DVI-D-1 status changed to disconnected.
[  7916.925] (II) modeset(0): EDID for output DVI-D-1
[  7916.939] (II) modeset(0): Output VGA-1 status changed to disconnected.
[  7916.939] (II) modeset(0): EDID for output VGA-1
[  7916.939] (II) modeset(0): Output HDMI-1 connected
[  7916.939] (II) modeset(0): Output DVI-D-1 disconnected
[  7916.939] (II) modeset(0): Output VGA-1 disconnected
[  7916.939] (II) modeset(0): Using exact sizes for initial modes
[  7916.939] (II) modeset(0): Output HDMI-1 using initial mode 1680x1050 +0+0
[  7916.939] (==) modeset(0): Using gamma correction (1.0, 1.0, 1.0)
[  7916.939] (==) modeset(0): DPI set to (96, 96)
[  7916.939] (II) Loading sub module "fb"
[  7916.939] (II) LoadModule: "fb"
[  7916.940] (II) Loading /usr/lib/xorg/modules/libfb.so
[  7916.944] (II) Module fb: vendor="X.Org Foundation"
[  7916.944]    compiled for 1.20.11, module version = 1.0.0
[  7916.944]    ABI class: X.Org ANSI C Emulation, version 0.4
[  7916.964] Failed to compile VS: 0:1(1): error: syntax error, unexpected NEW_IDENTIFIER

[  7916.964] Program source:
precision highp float;
attribute vec4 v_position;
attribute vec4 v_texcoord;
varying vec2 source_texture;

void main()
{
    gl_Position = v_position;
    source_texture = v_texcoord.xy;
}
[  7916.964] (EE)
Fatal server error:
[  7916.964] (EE) GLSL compile failure
[  7916.964] (EE)
 
Lastly I installed vappi to attempt video playback unfortunately even after fixing a couple of bus errors in galmium theres more to fix. So this pretty much sums up the nature of the problem to address. Furthermore this does raise the question is the  tweet from Radxa using acclerated graphics given the hardware restrictions of the RK3588.

Posted by Jas at 12:01 
Email This
BlogThis!
Share to X
Share to Facebook
Share to Pinterest
Labels: external GPU, pcie, radxa, RK3588, rock-5b
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
►  2025 (2)
►  2024 (3)
▼  2023 (3)
►  June (1)
▼  April (1)
RK3588 - Adventures with an external GPU through P...
►  January (1)
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