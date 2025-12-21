cmake_minimum_required(VERSION 3.23)
project(ReShade LANGUAGES C;CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CONFIGURATION_TYPES Debug;Release)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(deps)

# ReShade

add_library(ReShade SHARED)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(RESHADE_SUFFIX 64)
else()
  set(RESHADE_SUFFIX 32)
endif()
set_target_properties(ReShade PROPERTIES PREFIX "")
set_target_properties(ReShade PROPERTIES SUFFIX ${RESHADE_SUFFIX}${CMAKE_SHARED_LIBRARY_SUFFIX})

set(RESHADE_SOURCE
  examples/09-depth/generic_depth_addon.cpp
  examples/15-effect_runtime_sync/runtime_sync_addon.cpp
  res/fonts/forkawesome.h
  res/fonts/forkawesome.inl
  res/exports.def
  res/resource.h
  res/resource.rc
  res/version.h
  res/version.rc2
  source/addon.cpp
  source/addon.hpp
  source/addon_manager.cpp
  source/addon_manager.hpp
  source/com_ptr.hpp
  source/com_utils.hpp
  source/dll_log.cpp
  source/dll_log.hpp
  source/dll_main.cpp
  source/dll_resources.cpp
  source/dll_resources.hpp
  source/hook.cpp
  source/hook.hpp
  source/hook_manager.cpp
  source/hook_manager.hpp
  source/imgui_code_editor.cpp
  source/imgui_code_editor.hpp
  source/imgui_function_table.cpp
  source/imgui_function_table_18600.cpp
  source/imgui_function_table_18600.hpp
  source/imgui_function_table_18971.cpp
  source/imgui_function_table_18971.hpp
  source/imgui_function_table_19000.cpp
  source/imgui_function_table_19000.hpp
  source/imgui_function_table_19040.cpp
  source/imgui_function_table_19040.hpp
  source/imgui_function_table_19180.cpp
  source/imgui_function_table_19180.hpp
  source/imgui_function_table_19191.cpp
  source/imgui_function_table_19191.hpp
  source/imgui_function_table_19222.cpp
  source/imgui_function_table_19222.hpp
  source/imgui_widgets.cpp
  source/imgui_widgets.hpp
  source/ini_file.cpp
  source/ini_file.hpp
  source/input.cpp
  source/input.hpp
  source/input_gamepad.cpp
  source/input_gamepad.hpp
  source/platform_utils.cpp
  source/platform_utils.hpp
  source/runtime.cpp
  source/runtime.hpp
  source/runtime_api.cpp
  source/runtime_gui.cpp
  source/runtime_internal.hpp
  source/runtime_manager.cpp
  source/runtime_manager.hpp
  source/runtime_update_check.cpp
  source/state_block.cpp
  source/state_block.hpp
)
set(RESHADE_SOURCE_DIRECTX
  source/d2d1/d2d1.cpp
  source/d3d10/d3d10.cpp
  source/d3d10/d3d10_device.cpp
  source/d3d10/d3d10_device.hpp
  source/d3d10/d3d10_impl_command_list.cpp
  source/d3d10/d3d10_impl_command_queue.cpp
  source/d3d10/d3d10_impl_device.cpp
  source/d3d10/d3d10_impl_device.hpp
  source/d3d10/d3d10_impl_state_block.cpp
  source/d3d10/d3d10_impl_state_block.hpp
  source/d3d10/d3d10_impl_swapchain.cpp
  source/d3d10/d3d10_impl_swapchain.hpp
  source/d3d10/d3d10_impl_type_convert.cpp
  source/d3d10/d3d10_impl_type_convert.hpp
  source/d3d10/d3d10_resource.cpp
  source/d3d10/d3d10_resource.hpp
  source/d3d10/d3d10_resource_call_vtable.inl
  source/d3d11/d3d11.cpp
  source/d3d11/d3d11on12.cpp
  source/d3d11/d3d11on12_device.cpp
  source/d3d11/d3d11on12_device.hpp
  source/d3d11/d3d11_command_list.cpp
  source/d3d11/d3d11_command_list.hpp
  source/d3d11/d3d11_device.cpp
  source/d3d11/d3d11_device.hpp
  source/d3d11/d3d11_device_context.cpp
  source/d3d11/d3d11_device_context.hpp
  source/d3d11/d3d11_extensions.cpp
  source/d3d11/d3d11_extensions.hpp
  source/d3d11/d3d11_impl_command_list.cpp
  source/d3d11/d3d11_impl_command_queue.cpp
  source/d3d11/d3d11_impl_device.cpp
  source/d3d11/d3d11_impl_device.hpp
  source/d3d11/d3d11_impl_device_context.hpp
  source/d3d11/d3d11_impl_state_block.cpp
  source/d3d11/d3d11_impl_state_block.hpp
  source/d3d11/d3d11_impl_swapchain.cpp
  source/d3d11/d3d11_impl_swapchain.hpp
  source/d3d11/d3d11_impl_type_convert.cpp
  source/d3d11/d3d11_impl_type_convert.hpp
  source/d3d11/d3d11_resource.cpp
  source/d3d11/d3d11_resource.hpp
  source/d3d11/d3d11_resource_call_vtable.inl
  source/d3d12/d3d12.cpp
  source/d3d12/d3d12_command_list.cpp
  source/d3d12/d3d12_command_list.hpp
  source/d3d12/d3d12_command_queue.cpp
  source/d3d12/d3d12_command_queue.hpp
  source/d3d12/d3d12_command_queue_downlevel.cpp
  source/d3d12/d3d12_command_queue_downlevel.hpp
  source/d3d12/d3d12_descriptor_heap.cpp
  source/d3d12/d3d12_device.cpp
  source/d3d12/d3d12_device_downlevel.cpp
  source/d3d12/d3d12_impl_command_list.cpp
  source/d3d12/d3d12_impl_command_list.hpp
  source/d3d12/d3d12_impl_command_list_immediate.cpp
  source/d3d12/d3d12_impl_command_list_immediate.hpp
  source/d3d12/d3d12_impl_command_queue.cpp
  source/d3d12/d3d12_impl_command_queue.hpp
  source/d3d12/d3d12_impl_device.cpp
  source/d3d12/d3d12_impl_device.hpp
  source/d3d12/d3d12_impl_swapchain.cpp
  source/d3d12/d3d12_impl_swapchain.hpp
  source/d3d12/d3d12_impl_type_convert.cpp
  source/d3d12/d3d12_impl_type_convert.hpp
  source/d3d12/d3d12_pipeline_library.cpp
  source/d3d12/d3d12_pipeline_library.hpp
  source/d3d12/d3d12_pix.cpp
  source/d3d12/d3d12_resource.cpp
  source/d3d12/d3d12_resource.hpp
  source/d3d12/d3d12_resource_call_vtable.inl
  source/d3d9/d3d9.cpp
  source/d3d9/d3d9on12.cpp
  source/d3d9/d3d9on12_device.cpp
  source/d3d9/d3d9on12_device.hpp
  source/d3d9/d3d9_device.cpp
  source/d3d9/d3d9_impl_command_list.cpp
  source/d3d9/d3d9_impl_command_queue.cpp
  source/d3d9/d3d9_impl_device.cpp
  source/d3d9/d3d9_impl_device.hpp
  source/d3d9/d3d9_impl_state_block.cpp
  source/d3d9/d3d9_impl_state_block.hpp
  source/d3d9/d3d9_impl_swapchain.cpp
  source/d3d9/d3d9_impl_swapchain.hpp
  source/d3d9/d3d9_impl_type_convert.cpp
  source/d3d9/d3d9_impl_type_convert.hpp
  source/d3d9/d3d9_internal.cpp
  source/d3d9/d3d9_pix.cpp
  source/d3d9/d3d9_resource.cpp
  source/d3d9/d3d9_resource.hpp
  source/d3d9/d3d9_resource_call_vtable.inl
  source/d3d9/d3d9_swapchain.cpp
  source/d3d9/d3d9_swapchain.hpp
  source/ddraw/ddraw.cpp
  source/dxgi/dxgi.cpp
  source/dxgi/dxgi_adapter.cpp
  source/dxgi/dxgi_adapter.hpp
  source/dxgi/dxgi_d3d10.cpp
  source/dxgi/dxgi_device.cpp
  source/dxgi/dxgi_device.hpp
  source/dxgi/dxgi_factory.cpp
  source/dxgi/dxgi_factory.hpp
  source/dxgi/dxgi_swapchain.cpp
  source/dxgi/dxgi_swapchain.hpp
)
set(RESHADE_SOURCE_OPENGL
  source/opengl/opengl.cpp
  source/opengl/opengl_hooks.hpp
  source/opengl/opengl_hooks_ffp.cpp
  source/opengl/opengl_hooks_wgl.cpp
  source/opengl/opengl_impl_command_list.cpp
  source/opengl/opengl_impl_command_queue.cpp
  source/opengl/opengl_impl_device.cpp
  source/opengl/opengl_impl_device.hpp
  source/opengl/opengl_impl_device_context.hpp
  source/opengl/opengl_impl_state_block.cpp
  source/opengl/opengl_impl_state_block.hpp
  source/opengl/opengl_impl_swapchain.cpp
  source/opengl/opengl_impl_swapchain.hpp
  source/opengl/opengl_impl_type_convert.cpp
  source/opengl/opengl_impl_type_convert.hpp
)
set(RESHADE_SOURCE_VULKAN
  source/vulkan/vulkan.cpp
  source/vulkan/vulkan_hooks.hpp
  source/vulkan/vulkan_hooks_command_list.cpp
  source/vulkan/vulkan_hooks_device.cpp
  source/vulkan/vulkan_hooks_instance.cpp
  source/vulkan/vulkan_hooks_swapchain.cpp
  source/vulkan/vulkan_impl_command_list.cpp
  source/vulkan/vulkan_impl_command_list.hpp
  source/vulkan/vulkan_impl_command_list_immediate.cpp
  source/vulkan/vulkan_impl_command_list_immediate.hpp
  source/vulkan/vulkan_impl_command_queue.cpp
  source/vulkan/vulkan_impl_command_queue.hpp
  source/vulkan/vulkan_impl_device.cpp
  source/vulkan/vulkan_impl_device.hpp
  source/vulkan/vulkan_impl_swapchain.cpp
  source/vulkan/vulkan_impl_swapchain.hpp
  source/vulkan/vulkan_impl_type_convert.cpp
  source/vulkan/vulkan_impl_type_convert.hpp
)
set(RESHADE_SOURCE_VR
  source/openvr/openvr.cpp
  source/openvr/openvr_impl_swapchain.cpp
  source/openvr/openvr_impl_swapchain.hpp
  source/openxr/openxr.cpp
  source/openxr/openxr_hooks.hpp
  source/openxr/openxr_hooks_instance.cpp
  source/openxr/openxr_hooks_swapchain.cpp
  source/openxr/openxr_impl_swapchain.cpp
  source/openxr/openxr_impl_swapchain.hpp
  source/runtime_gui_vr.cpp
)
set(RESHADE_SOURCE_WINDOWS
  source/windows/dinput.cpp
  source/windows/dinput8.cpp
  source/windows/user32.cpp
  source/windows/ws2_32.cpp
)

target_sources(
  ReShade
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      include
    FILES
      include/reshade.hpp
      include/reshade_api.hpp
      include/reshade_api_device.hpp
      include/reshade_api_format.hpp
      include/reshade_api_pipeline.hpp
      include/reshade_api_resource.hpp
      include/reshade_events.hpp
      include/reshade_overlay.hpp
  PRIVATE
    ${RESHADE_SOURCE}
    ${RESHADE_SOURCE_OPENGL}
    ${RESHADE_SOURCE_VULKAN}
    ${RESHADE_SOURCE_DIRECTX}
    ${RESHADE_SOURCE_VR}
    ${RESHADE_SOURCE_WINDOWS}
)

set_source_files_properties(source/input_gamepad.cpp PROPERTIES COMPILE_DEFINITIONS _WIN32_WINNT=_WIN32_WINNT_WIN7)
set_source_files_properties(source/vulkan/vulkan_impl_device.cpp PROPERTIES COMPILE_DEFINITIONS VMA_IMPLEMENTATION)
set_source_files_properties(examples/09-depth/generic_depth_addon.cpp PROPERTIES COMPILE_DEFINITIONS BUILTIN_ADDON)
set_source_files_properties(examples/15-effect_runtime_sync/runtime_sync_addon.cpp PROPERTIES COMPILE_DEFINITIONS BUILTIN_ADDON)
if(MSVC)
  set_source_files_properties(examples/09-depth/generic_depth_addon.cpp PROPERTIES COMPILE_FLAGS /FIreshade.hpp)
  set_source_files_properties(examples/15-effect_runtime_sync/runtime_sync_addon.cpp PROPERTIES COMPILE_FLAGS /FIreshade.hpp)
else()
  set_source_files_properties(examples/09-depth/generic_depth_addon.cpp PROPERTIES COMPILE_FLAGS "-include reshade.hpp")
  set_source_files_properties(examples/15-effect_runtime_sync/runtime_sync_addon.cpp PROPERTIES COMPILE_FLAGS "-include reshade.hpp")
endif()

target_include_directories(
  ReShade
  PRIVATE
    res
    source
)

target_compile_definitions(
  ReShade
  PRIVATE
    RESHADE_GUI
    RESHADE_API_LIBRARY_EXPORT
    RESHADE_ADDON=2
    RESHADE_LOCALIZATION
    $<$<CONFIG:Debug>:RESHADE_VERBOSE_LOG>
    _GDI32_
    WINSOCK_API_LINKAGE=
    _UNICODE
    _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

if(MSVC)
  target_compile_options(
    ReShade
    PRIVATE
      /Zc:threadSafeInit-
      /utf-8
      /Zc:char8_t-
      $<$<CONFIG:Release>:/Oi /GL /GF /GS- /Gy>
      /fp:fast
      $<$<CONFIG:Release>:/GR->
  )
  target_link_options(
    ReShade
    PRIVATE
      $<$<CONFIG:Release>:/OPT:REF /OPT:NOICF /LTCG>
  )
else()
  target_compile_options(
    ReShade
    PRIVATE
      -DSTDMETHODCALLTYPE=
      -Wno-changes-meaning
      -fPIC
      $<$<CONFIG:Release>:-fno-rtti>
  )
endif()

target_link_libraries(ReShade PRIVATE ReShadeFX D3D12On7 fpng glad ImGui jxl MinHook OpenVR OpenXR stb utfcpp VMA Windows)

# ReShade FX

add_library(ReShadeFX STATIC)

target_sources(
  ReShadeFX
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      source
    FILES
      source/effect_codegen.hpp
      source/effect_expression.hpp
      source/effect_lexer.hpp
      source/effect_module.hpp
      source/effect_parser.hpp
      source/effect_preprocessor.hpp
      source/effect_symbol_table.hpp
      source/effect_token.hpp
  PRIVATE
    source/effect_codegen_dxbc.cpp
    source/effect_codegen_glsl.cpp
    source/effect_codegen_hlsl.cpp
    source/effect_codegen_spirv.cpp
    source/effect_expression.cpp
    source/effect_lexer.cpp
    source/effect_parser_exp.cpp
    source/effect_parser_stmt.cpp
    source/effect_preprocessor.cpp
    source/effect_symbol_table.cpp
    source/effect_symbol_table_intrinsics.inl
)

target_compile_definitions(
  ReShadeFX
  PRIVATE
    _UNICODE
)

if(MSVC)
  target_compile_options(
    ReShadeFX
    PRIVATE
      /utf-8
      /Zc:char8_t-
      $<$<CONFIG:Release>:/Oi /GL /GF /GS- /Gy>
      $<$<CONFIG:Release>:/GR->
  )
else()
  target_compile_options(
    ReShadeFX
    PRIVATE
      -fPIC
      $<$<CONFIG:Release>:-fno-rtti>
  )
endif()

target_link_libraries(ReShadeFX PRIVATE SPIRV)
