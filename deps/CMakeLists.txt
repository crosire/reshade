if(MSVC)
  add_compile_options(
    /fp:fast
  )
else()
  add_compile_options(
    -fPIC
  )
endif()

# D3D12On7

add_library(D3D12On7 INTERFACE)
target_include_directories(
  D3D12On7
  INTERFACE
    d3d12on7/include
    d3d12/include/directx
    d3d911on12/include
)

# fpng

add_library(fpng STATIC)
target_sources(
  fpng
  PUBLIC
    fpng/src/fpng.h
  PRIVATE
    fpng/src/fpng.cpp
)
target_include_directories(
  fpng
  PUBLIC
    fpng/src
)
target_compile_definitions(
  fpng
  PUBLIC
    FPNG_NO_STDIO
)
if(NOT MSVC)
  target_compile_options(
    fpng
    PRIVATE
      -mpclmul -msse2 -msse4
  )
endif()

# glad

add_library(glad STATIC)
target_include_directories(
  glad
  PUBLIC
    glad/target/include
)
if(WIN32)
  target_compile_definitions(
    glad
    PUBLIC
      VK_USE_PLATFORM_WIN32_KHR
  )
endif()

set(GLAD_APIS
  gl:compatibility=4.6
  vulkan=1.4
)
set(GLAD_EXTENSIONS
  GL_ARB_texture_compression_bptc;GL_EXT_texture;GL_EXT_texture_compression_s3tc;GL_EXT_texture_storage;GL_EXT_memory_object;GL_EXT_memory_object_win32;GL_EXT_semaphore;GL_EXT_semaphore_win32
  VK_EXT_conservative_rasterization;VK_EXT_custom_border_color;VK_EXT_debug_utils;VK_EXT_extended_dynamic_state;VK_EXT_extended_dynamic_state2;VK_EXT_extended_dynamic_state3;VK_EXT_full_screen_exclusive;VK_EXT_graphics_pipeline_library;VK_EXT_index_type_uint8;VK_EXT_load_store_op_none;VK_EXT_mesh_shader;VK_EXT_multi_draw;VK_EXT_private_data;VK_EXT_swapchain_colorspace;VK_EXT_tooling_info;VK_EXT_transform_feedback;VK_KHR_acceleration_structure;VK_KHR_bind_memory2;VK_KHR_buffer_device_address;VK_KHR_copy_commands2;VK_KHR_create_renderpass2;VK_KHR_dedicated_allocation;VK_KHR_depth_stencil_resolve;VK_KHR_descriptor_update_template;VK_KHR_device_group;VK_KHR_display_swapchain;VK_KHR_draw_indirect_count;VK_KHR_dynamic_rendering;VK_KHR_external_memory_capabilities;VK_KHR_external_memory_win32;VK_KHR_external_semaphore_win32;VK_KHR_get_memory_requirements2;VK_KHR_get_physical_device_properties2;VK_KHR_image_format_list;VK_KHR_incremental_present;VK_KHR_maintenance4;VK_KHR_maintenance5;VK_KHR_maintenance9;VK_KHR_pipeline_library;VK_KHR_push_descriptor;VK_KHR_ray_tracing_maintenance1;VK_KHR_ray_tracing_pipeline;VK_KHR_surface;VK_KHR_swapchain;VK_KHR_swapchain_mutable_format;VK_KHR_synchronization2;VK_KHR_timeline_semaphore;VK_KHR_win32_surface
)
set(GLAD_GENERATED_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/glad/target/src/gl.c
  ${CMAKE_CURRENT_SOURCE_DIR}/glad/target/src/vulkan.c
  ${CMAKE_CURRENT_SOURCE_DIR}/glad/target/include/glad/gl.h
  ${CMAKE_CURRENT_SOURCE_DIR}/glad/target/include/glad/vulkan.h
)
if(WIN32)
  set(GLAD_APIS
    ${GLAD_APIS}
    wgl=1.0
  )
  set(GLAD_EXTENSIONS
    ${GLAD_EXTENSIONS}
    WGL_ARB_create_context;WGL_ARB_create_context_profile;WGL_ARB_framebuffer_sRGB;WGL_ARB_multisample;WGL_ARB_pbuffer;WGL_ARB_pixel_format;WGL_ARB_render_texture
  )
  set(GLAD_GENERATED_FILES
    ${GLAD_GENERATED_FILES}
    ${CMAKE_CURRENT_SOURCE_DIR}/glad/target/src/wgl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/glad/target/include/glad/wgl.h
  )
endif()

string(REPLACE ";" "," GLAD_APIS "${GLAD_APIS}")
string(REPLACE ";" "," GLAD_EXTENSIONS "${GLAD_EXTENSIONS}")
file(GLOB GLAD_SPECIFICATIONS "khronos/*.xml")
add_custom_command(
  OUTPUT
    ${GLAD_GENERATED_FILES}
  COMMAND
    pip install -r requirements.txt
  COMMAND 
    ${CMAKE_COMMAND} -E copy ${GLAD_SPECIFICATIONS} ./
  COMMAND
    python -m glad --out-path ./target --api ${GLAD_APIS} --extensions ${GLAD_EXTENSIONS} c --mx
  COMMAND
    ${CMAKE_COMMAND} -E remove gl.xml vk.xml wgl.xml
  WORKING_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/glad
)

target_sources(
  glad
  PRIVATE
    ${GLAD_GENERATED_FILES}
)

# ImGui

add_library(ImGui STATIC)
target_sources(
  ImGui
  PUBLIC
    imgui/imgui.h
    imgui/imgui_internal.h
    imgui_config.hpp
  PRIVATE
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui_config.cpp
)
target_include_directories(
  ImGui
  PUBLIC
    imgui
)
target_compile_definitions(
  ImGui
  PUBLIC
    ImTextureID=ImU64
    IMGUI_USER_CONFIG="../imgui_config.hpp"
    IMGUI_DEFINE_MATH_OPERATORS
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_DISABLE_FILE_FUNCTIONS
    IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
  PRIVATE
    "$<$<CONFIG:Release>:IMGUI_DISABLE_DEMO_WINDOWS>"
    "$<$<CONFIG:Release>:IMGUI_DISABLE_DEBUG_TOOLS>"
    "$<$<CONFIG:Release>:IMGUI_DEBUG_PRINTF=((void)0)>"
)

# jxl

add_library(jxl STATIC)
target_sources(
  jxl
  PUBLIC
    jxl_simple_lossless/simple_lossless.h
    jxl_simple_lossless/color_encoding.h
  PRIVATE
    jxl_simple_lossless/simple_lossless.cc
)
target_include_directories(
  jxl
  PUBLIC
    jxl_simple_lossless
)
target_compile_definitions(
  jxl
  PRIVATE
    main=simple_lossless
)

# MinHook

if(WIN32)
  add_library(MinHook STATIC)
  target_sources(
    MinHook
    PUBLIC
      minhook/include/MinHook.h
    PRIVATE
      minhook/src/buffer.c
      minhook/src/buffer.h
      minhook/src/hde/hde32.c
      minhook/src/hde/hde32.h
      minhook/src/hde/hde64.c
      minhook/src/hde/hde64.h
      minhook/src/hde/pstdint.h
      minhook/src/hde/table32.h
      minhook/src/hde/table64.h
      minhook/src/hook.c
      minhook/src/trampoline.c
      minhook/src/trampoline.h
  )
  target_include_directories(
    MinHook
    PUBLIC
      minhook/include
  )
endif()

# OpenVR

add_library(OpenVR INTERFACE)
target_include_directories(
  OpenVR
  INTERFACE
    openvr/headers
    openvr/src
)
target_compile_definitions(
  OpenVR
  INTERFACE
    OPENVR_BUILD_STATIC
    OPENVR_NO_STL
)

# OpenXR

add_library(OpenXR INTERFACE)
target_include_directories(
  OpenXR
  INTERFACE
    openxr/include
    openxr/src
)
if(WIN32)
  target_compile_definitions(
    OpenXR
    INTERFACE
      XR_USE_PLATFORM_WIN32
  )
endif()

# SPIRV

add_library(SPIRV INTERFACE)
target_include_directories(
  SPIRV
  INTERFACE
    spirv/include/spirv/unified1
)

# stb

add_library(stb STATIC)
target_sources(
  stb
  PUBLIC
    stb/stb_image.h
    stb/stb_image_resize2.h
    stb/stb_image_write.h
    stb_image/stb_image_dds.h
    stb_image/stb_image_write_hdr_png.h
  PRIVATE
    stb_impl.c
)
target_include_directories(
  stb
  PUBLIC
    stb
    stb_image
)
target_compile_definitions(
  stb
  PUBLIC
    STBI_NO_STDIO
    STBI_WRITE_NO_STDIO
    _CRT_SECURE_NO_WARNINGS
)

# utfcpp

add_library(utfcpp INTERFACE)
target_include_directories(
  utfcpp
  INTERFACE
    utfcpp/source
)
target_compile_definitions(
  utfcpp
  INTERFACE
    UTF_CPP_CPLUSPLUS=201703L
)

# VMA

add_library(VMA INTERFACE)
target_include_directories(
  VMA
  INTERFACE
    vma/include
)
target_compile_definitions(
  VMA
  INTERFACE
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=0
    VMA_STATS_STRING_ENABLED=0
)

# Windows

if(WIN32)
  add_library(Windows INTERFACE)
  target_compile_definitions(
    Windows
    INTERFACE
      WIN32_LEAN_AND_MEAN
      NOGDICAPMASKS
      NOMENUS
      NOICONS
      NOKEYSTATES
      NOSYSCOMMANDS
      NORASTEROPS
      NOATOM
      NOCOLOR
      NODRAWTEXT
      NOMEMMGR
      NOMETAFILE
      NOMINMAX
      NOOPENFILE
      NOSCROLL
      NOSERVICE
      NOSOUND
      NOTEXTMETRIC
      NOCOMM
      NOKANJI
      NOHELP
      NOPROFILER
      NODEFERWINDOWPOS
      NOMCX
  )
  target_link_libraries(
    Windows
    INTERFACE
      d3dcompiler.lib
      dwmapi.lib
      dxguid.lib
      ShLwApi.lib
      wininet.lib
      WS2_32.lib
      Version.lib
      WinMM.lib
  )
endif()
