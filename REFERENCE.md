ReShade API
===========

The ReShade API lets you interact with the resources and rendering commands of applications ReShade was loaded into. It abstracts away differences between the various graphics API ReShade supports (Direct3D 9/10/11/12, OpenGL and Vulkan), to make it possible to write add-ons that work across a wide range of applications, regardless of the graphics API they use.

A ReShade add-on is a DLL or part of the application that uses the header-only ReShade API to register callbacks for [events](#events) and do work in those callbacks after they were invoked by ReShade. There are no further requirements, no functions need to be exported and no libraries need to be linked against (although linking against ReShade is supported as well by defining `RESHADE_API_LIBRARY` before including the headers if so desired). Simply add the [include directory from the ReShade repository](https://github.com/crosire/reshade/tree/main/include) to your project and include the `reshade.hpp` header to get started.

An add-on may optionally export an `AddonInit` function if more complicated one-time initialization than possible in `DllMain` is required. It will be called by ReShade right after loading the add-on module.
```cpp
extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module, HMODULE reshade_module)
{
    return true;
}
```
Similarily it may also export an `AddonUninit` function, which will be called right before unloading (but only if initialization was successfull).
```cpp
extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module, HMODULE reshade_module)
{
}
```

Here is a very basic code example of an add-on that registers a callback that gets executed every time ReShade has finished with a frame to be presented to the screen:

```cpp
#include <reshade.hpp>

static void on_reshade_present(reshade::api::effect_runtime *runtime)
{
    // ...
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // Call 'reshade::register_addon()' before you call any other function of the ReShade API.
        // This will look for the ReShade instance in the current process and initialize the API when found.
        if (!reshade::register_addon(hinstDLL))
            return FALSE;
        // This registers a callback for the 'present' event, which occurs every time a new frame is presented to the screen.
        // The function signature has to match the type defined by 'reshade::addon_event_traits<reshade::addon_event::present>::decl'.
        // For more details check the inline documentation for each event in 'reshade_events.hpp'.
        reshade::register_event<reshade::addon_event::reshade_present>(&on_reshade_present);
        break;
    case DLL_PROCESS_DETACH:
        // Optionally unregister the event callback that was previously registered during process attachment again.
        reshade::unregister_event<reshade::addon_event::reshade_present>(&on_reshade_present);
        // And finally unregister the add-on from ReShade (this will automatically unregister any events and overlays registered by this add-on too).
        reshade::unregister_addon(hinstDLL);
        break;
    }
    return TRUE;
}
```

After building an add-on DLL, change its file extension from `.dll` to `.addon` and put it into the add-on search directory configured in ReShade (which defaults to the same directory as ReShade). It will be picked up and loaded automatically on the next launch of the application.

For more complex examples, see the [examples directory in the repository](https://github.com/crosire/reshade/tree/main/examples). Contents of this document:

* [Events](#events)
* [Overlays](#overlays)
* [Abstraction](#abstraction)
  * [Device & Commands](#device--commands-reshade_api_devicehpp)
  * [Resources & Resource Views](#resources--resource-views-reshade_api_resourcehpp)
  * [Pipelines, Layouts & Descriptor Tables](#pipelines-layouts--descriptor-tables-reshade_api_pipelinehpp)

## Events

The [graphics API abstraction](#abstraction) is modeled after the Direct3D 12 and Vulkan APIs, so much of the terminology used should be familiar to developers that have used those before.

Detailed inline documentation for all classes and methods can be found inside the headers (see `reshade_api_device.hpp` for the abstraction object classes and `reshade_events.hpp` for a list of available events).

The base object everything else is created from is a `reshade::api::device`. This represents a logical rendering device that is typically mapped to a physical GPU (but may also be mapped to multiple GPUs). ReShade will call the `reshade::addon_event::init_device` event after the application created a device, which can e.g. be used to do some initialization work that only has to happen once. The `reshade::addon_event::destroy_device` event is called before this device is destroyed again, which can be used to perform clean up work.
```cpp
// Example callback function that can be registered via 'reshade::register_event<reshade::addon_event::init_device>(&on_init_device)'.
static void on_init_device(reshade::api::device *device)
{
    // In case one wants to do something with the native graphics API object, rather than doing all work
    // through the ReShade API, can retrieve it as follows:
    if (device->get_api() == reshade::api::device_api::d3d11)
    {
        ID3D11Device *const d3d11_device = (ID3D11Device *)device->get_native();
        // ...
    }

    // But preferably things should be done through the graphics API abstraction.
    // E.g. to create a new 800x600 texture in GPU memory, call 'reshade::api::device::create_resource()' like this:
    reshade::api::resource texture = {};
    const reshade::api::resource_desc desc(
        800, 600, 1, 1,
        reshade::api::format::r8g8b8a8_unorm,
        1,
        reshade::api::memory_heap::gpu_only,
        reshade::api::resource_usage::shader_resource | reshade::api::resource_usage::render_target);
    if (!device->create_resource(desc, nullptr, reshade::api::resource_usage::undefined, &texture))
    {
        // Error handling ...
    }

    // ...
}
```

To execute [rendering commands](#device--commands-reshade_api_devicehpp) (like draw/dispatch commands), an application has to record them into a `reshade::api::command_list` and then submit to a `reshade::api::command_queue`. In some graphics APIs there is only a single implicit command list and queue, but modern ones like Direct3D 12 and Vulkan allow the creation of multiple for more efficient multi-threaded rendering. ReShade will call the `reshade::addon_event::init_command_list` and `reshade::addon_event::init_command_queue` events after any such object was created by the application (including the implicit ones for older graphics APIs). Similarily, `reshade::addon_event::destroy_command_list` and `reshade::addon_event::destroy_command_queue` are called upon their destruction.

ReShade will also pass the current command list object to every command event, like `reshade::addon_event::draw`, `reshade::addon_event::dispatch` and so on, which can be used to add additional commands to that command list or replace those of the application.
```cpp
// Example callback function that can be registered via 'reshade::register_event<reshade::addon_event::draw>(&on_draw)'.
static bool on_draw(reshade::api::command_list *cmd_list, uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
    // Clear a render target to red before every time a single triangle is drawn
    if (vertices == 3 && instances == 1)
    {
        reshade::api::resource_view rtv = ...;

        const float clear_color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
        cmd_list->clear_render_target_view(rtv, clear_color);
    }

    // Return 'true' to prevent this application command from actually being executed (e.g. because already having added a new command via 'cmd_list->draw(...)' or similar that should replace it).
    // Return 'false' to leave it unaffected.
    return false;
}
```

Showing results on the screen is done through a `reshade::api::swapchain` object. This is a collection of back buffers that the application can render into, which will eventually be presented to the screen. There may be multiple swap chains, if for example the application is rendering to multiple windows, or to a screen and a VR headset. ReShade again will call the `reshade::addon_event::init_swapchain` event after such an object was created by the application (and `reshade::addon_event::destroy_swapchain` on destruction). In addition ReShade will call the `reshade::addon_event::create_swapchain` event before a swap chain is created, so an add-on may modify its description before that happens. For example, to force the resolution to a specific value, one can do the following:
```cpp
// Example callback function that can be registered via 'reshade::register_event<reshade::addon_event::create_swapchain>(&on_create_swapchain)'.
static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc &desc, void *hwnd)
{
    // Change resolution to 1920x1080 if the application is trying to create a swap chain at 800x600.
    if (desc.back_buffer.texture.width == 800 &&
        desc.back_buffer.texture.height == 600)
    {
        desc.back_buffer.texture.width = 1920;
        desc.back_buffer.texture.height = 1080;
    }

    // Return 'true' for ReShade to overwrite the swap chain description of the application with the values set in this callback.
    // Return 'false' to leave it unaffected.
    return true;
}
```

ReShade associates an independent post-processing effect runtime with most swap chains. This is the runtime one usually controls via the ReShade overlay, but it can also be controlled programatically via the ReShade API using methods of the `reshade::api::effect_runtime` object.

In contrast to the described basic API abstraction objects, any [buffers](#resources--resource-views-reshade_api_resourcehpp), [textures](#resources--resource-views-reshade_api_resourcehpp), [pipelines](#pipelines-layouts--descriptor-tables-reshade_api_pipelinehpp), etc. are referenced via handles. These are either created by the application and passed to events (like `reshade::addon_event::init_resource`, `reshade::addon_event::init_pipeline`, ...) or can be created through the `reshade::api::device` object of the ReShade API (via `reshade::api::device::create_resource()`, `reshade::api::device::create_pipeline()`, ...).

Buffers and textures are referenced via `reshade::api::resource` handles. Depth-stencil, render target, shader resource or unordered access views to such resources are referenced via `reshade::api::resource_view` handles. Sampler state objects are referenced via `reshade::api::sampler` handles, (partial) pipeline state objects via `reshade::api::pipeline` handles and so on.

## Overlays

It is also supported to add an overlay, which can e.g. be used to display debug information or interact with the user in-application.
Overlays are created with the use of the docking branch of [Dear ImGui](https://github.com/ocornut/imgui/tree/v1.92.2b-docking). Including `reshade.hpp` after [`imgui.h`](https://github.com/ocornut/imgui/blob/v1.92.2b-docking/imgui.h) will automatically overwrite all Dear ImGui functions to use the instance created and managed by ReShade. This means all you have to do is include these two headers and use Dear ImGui as usual (without having to build its source code files):

```cpp
#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#include <imgui.h>
#include <reshade.hpp>

bool g_popup_window_visible = false;

static void draw_debug_overlay(reshade::api::effect_runtime *runtime)
{
    ImGui::TextUnformatted("Some text");

    if (ImGui::Button("Press me to open an additional popup window"))
        g_popup_window_visible = true;

    if (g_popup_window_visible)
    {
        ImGui::Begin("Popup", &g_popup_window_visible);
        ImGui::TextUnformatted("Some other text");
        ImGui::End();
    }
}

static void draw_settings_overlay(reshade::api::effect_runtime *runtime)
{
    ImGui::Checkbox("Popup window is visible", &g_popup_window_visible);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // This will also populate the Dear ImGui function table.
        if (!reshade::register_addon(hinstDLL))
            return FALSE;
        // This registers a new overlay with the specified name with ReShade.
        // It will be displayed as an additional window when the ReShade overlay is opened.
        // Its contents are defined by Dear ImGui commands issued in the specified callback function.
        reshade::register_overlay("Test", &draw_debug_overlay);
        // It is also possible to register a special settings overlay by passing 'nullptr' instead of a title.
        // This is shown beneath the add-on information in the add-on list of the ReShade overlay and can be used to present settings to users.
        reshade::register_overlay(nullptr, &draw_settings_overlay);
        break;
    case DLL_PROCESS_DETACH:
        reshade::unregister_addon(hinstDLL);
        break;
    }
    return TRUE;
}
```

Do not call `ImGui::Begin` and `ImGui::End` in the callback to create the overlay window itself, ReShade already does this for you before and after calling the callback function.
You can however call `ImGui::Begin` and `ImGui::End` with a different title to open additional popup windows (this is not recommended though, since those are difficult to navigate in VR).

Overlay names are shared across ReShade and all add-ons, which means you can register with a name already used by ReShade or another add-on to append widgets to their overlay.
For example, `reshade::register_overlay("###settings", ...)` allows you to add widgets to the settings page in ReShade and `reshade::register_overlay("OSD", ...)` allows you to add additional information to the always visible on-screen display (clock, FPS, frametime) ReShade provides.

## Abstraction

### Device & Commands ([reshade_api_device.hpp](https://github.com/crosire/reshade/blob/main/include/reshade_api_device.hpp))

The concept of `reshade::api::device` is functionally equivalent to `ID3D12Device` in D3D12 or `VkDevice` in Vulkan. The concept of `reshade::api::command_list` is functionally equivalent to `ID3D12CommandList` in D3D12 or `VkCommandBuffer` in Vulkan. The concept of `reshade::api::command_queue` is functionally equivalent to `ID3D12CommandQueue` in D3D12 or `VkQueue` in Vulkan.

### Resources & Resource Views ([reshade_api_resource.hpp](https://github.com/crosire/reshade/blob/main/include/reshade_api_resource.hpp))

To allocate memory and create buffers or textures, call `reshade::api::device::create_resource()`. Care has to be taken to specify all the possible ways the resource is going to be used via `reshade::api::resource_desc::usage`.

Resources allocated in GPU memory (`reshade::api::resource_desc::heap` set to `reshade::api::memory_heap::gpu_only`) cannot be mapped and accessed on the CPU, so to fill them with contents either have to specify it during resource creation via the initial data parameter or upload it via `reshade::api::device::update_buffer_region()` or `reshade::api::device::update_texture_region()`.\
Resources allocated in CPU-visible memory (`reshade::api::resource_desc::heap` set to `reshade::api::memory_heap::cpu_to_gpu` or `reshade::api::memory_heap::gpu_to_cpu`) on the other hand can be mapped and directly accessed on the CPU, via `reshade::api::device::map_buffer_region()` or `reshade::api::device::map_texture_region()`.

The concept of `reshade::api::resource` is functionally equivalent to `ID3D12Resource` in D3D12 or `VkBuffer`/`VkImage` in Vulkan. The concept of `reshade::api::resource_view` is functionally equivalent to SRV/UAV/... in D3D12 or `VkBufferView`/`VkImageView` in Vulkan.

### Pipelines, Layouts & Descriptor Tables ([reshade_api_pipeline.hpp](https://github.com/crosire/reshade/blob/main/include/reshade_api_pipeline.hpp))

Shaders and other render state is combined into monolithic pipeline state objects (`reshade::api::pipeline`), which can then be bound at draw time (using `reshade::api::command_list::bind_pipeline()`) to make any following draw calls make use of those shaders and render state.

```cpp
reshade::api::pipeline_subobject subobjects[];

...

reshade::api::shader_desc vertex_shader;
vertex_shader.code = ...;
vertex_shader.code_size = ...;
subobjects[0].type = reshade::api::pipeline_subobject_type::vertex_shader;
subobjects[0].count = 1;
subobjects[0].data = &vertex_shader;

reshade::api::shader_desc pixel_shader;
pixel_shader.code = ...;
pixel_shader.code_size = ...;
subobjects[1].type = reshade::api::pipeline_subobject_type::pixel_shader;
subobjects[1].count = 1;
subobjects[1].data = &pixel_shader;

reshade::api::rasterizer_desc rasterizer_state;
rasterizer_state.cull_mode = reshade::api::cull_mode::none;
subobjects[2].type = reshade::api::pipeline_subobject_type::rasterizer_state;
subobjects[2].count = 1;
subobjects[2].data = &rasterizer_state;
```

To create a pipeline, call `reshade::api::device::create_pipeline()` with a list of sub-objects that should be combined. This can contain graphics shaders and render state to create a graphics pipeline, or just a compute shader sub-object to create a compute pipeline, or ray tracing shaders to create a ray tracing pipeline.

In D3D9, D3D10, D3D11 and OpenGL, pipeline state objects can be partially bound, meaning `reshade::api::command_list::bind_pipeline()` can be called with a subset of `reshade::api::pipeline_stage` flags and only the sub-objects in the pipeline state object corresponding to those flags will be bound. In D3D12 and Vulkan pipeline state objects have to be monolithic and can only be bound as a whole, meaning the pipeline stage flags have to be `reshade::api::pipeline_stage::all_graphics` (for a graphics pipeline), `reshade::api::pipeline_stage::all_compute` (for a compute pipeline) or `reshade::api::pipeline_stage::all_raytracing` (for a ray tracing pipeline) and only a single pipeline per these stage flags can be bound on a command list at a time.

The concept of `reshade::api::pipeline` is functionally equivalent to `ID3D12PipelineState` in D3D12 or `VkPipeline` in Vulkan.

Binding resources and other objects to the shaders in a pipeline is done via descriptors. A descriptor is just a small handle that points to a shader resource view (`reshade::api::resource_view`), a sampler object (`reshade::api::sampler`) or a constant buffer resource (`reshade::api::buffer_range`). These are written into fixed-size linear tables (`reshade::api::descriptor_table`) in memory (`reshade::api::descriptor_heap`), which can be quickly swapped at draw time (using `reshade::api::command_list::bind_descriptor_tables()`). An additional layout object (`reshade::api::pipeline_layout`) is needed to map the entries from these linear tables to shader registers in shaders.

Since this mapping can get pretty complex, below is an example pipeline layout description which describes just a single descriptor table and how the corresponding table would be layed out in memory:
```cpp
reshade::api::pipeline_layout_param params[];

...

params[0].type = reshade::api::pipeline_layout_param_type::descriptor_table;
params[0].descriptor_table.count = 4;

params[0].descriptor_table.ranges[0].binding = 0;
params[0].descriptor_table.ranges[0].dx_register_index = 0; // Base shader register => t0 - t2
params[0].descriptor_table.ranges[0].count = 2;
params[0].descriptor_table.ranges[0].type = reshade::api::descriptor_type::texture_shader_resource_view; // => tX shader register

params[0].descriptor_table.ranges[1].binding = 2;
params[0].descriptor_table.ranges[1].dx_register_index = 6; // Base shader register => s6 - s10
params[0].descriptor_table.ranges[1].count = 5;
params[0].descriptor_table.ranges[1].array_size = 3; // First binding is an array descriptor of size 3, and the remaining 2 descriptors (due to total descriptor count of 5) are put into subsequent bindings (see also documentation on 'reshade::api::descriptor_range::array_size')
params[0].descriptor_table.ranges[1].type = reshade::api::descriptor_type::sampler; // => sX shader register

params[0].descriptor_table.ranges[2].binding = 5;
params[0].descriptor_table.ranges[2].dx_register_index = 1; // Base shader register => b1 - b2
params[0].descriptor_table.ranges[2].count = 2;
params[0].descriptor_table.ranges[2].type = reshade::api::descriptor_type::constant_buffer; // => bX shader register

params[0].descriptor_table.ranges[3].binding = 7;
params[0].descriptor_table.ranges[3].dx_register_index = 3;
params[0].descriptor_table.ranges[3].count = 2;
params[0].descriptor_table.ranges[3].array_size = 2;
params[0].descriptor_table.ranges[3].type = reshade::api::descriptor_type::texture_shader_resource_view;
```
```
               Descriptor Table
              ++-----------++-----------++-----------++-----------++-----------++-----------++-----------++-----------++
              || X         || X         || X | X | X || X         || X         || X         || X         || X   | X   ||
              ++-----------++-----------++-----------++-----------++-----------++-----------++-----------++-----------++

Binding        | 0          | 1          | 2          | 3          | 4          | 5          | 6          | 7          |
Array Offset   | 0          | 0          | 0 | 1 | 2  | 0          | 0          | 0          |            | 0   | 1    |

Offset         | 0          | 1          | 2 | 3 | 4  | 5          | 6          | 7          | 8          | 9   | 10   |

               ^------------------------^^-------------------------------------^^------------------------^^------------^
                Shader Resource Views     Samplers                               Constant Buffers          Shader Resource Views
                register(t0 - t2)         register(s6 - s10)                     register(b1 - b2)         register(t3 - t4)

```

To create a pipeline layout like the above, call `reshade::api::device::create_pipeline_layout()` with a list of pipeline layout parameter descriptions. Each parameter can refer to:
- push constants (when type is `reshade::api::pipeline_layout_param_type::push_constants`, which can then be referenced in `reshade::api::command_list::push_constants()` via its parameter index in the pipeline layout), more on those next
- push descriptors (when type is `reshade::api::pipeline_layout_param_type::push_descriptors`, which can then be referenced in `reshade::api::command_list::push_descriptors()` via its parameter index in the pipeline layout), more on those next
- a single descriptor table (when type is `reshade::api::pipeline_layout_param_type::descriptor_table`, which can then be referenced in `reshade::api::command_list::bind_descriptor_table()` via its parameter index in the pipeline layout)

Only a single pipeline layout per stage can be bound on a command list at a time. It is updated as part of `reshade::api::command_list::bind_descriptor_tables()`, `reshade::api::command_list::push_descriptors()` or `reshade::api::command_list::push_constants()`.

Managing constant buffers can be cumbersome, so for use cases with just a few constants, command lists have a small memory pool built-in, which can be filled with constant data in-place at draw time and bound to a constant buffer register in shaders. These constants, written using `reshade::api::command_list::push_constants()`, are called push constants (equivalent concept in D3D12 is called root constants).

Similarily, managing descriptor tables and the memory they are allocated from manually can be cumbersome, so for simple use cases, command lists also have a small descriptor heap built-in, which can be filled with descriptors in-place at draw time. The descriptors written this way, using `reshade::api::command_list::push_descriptors()` without allocating a descriptor table first, are called push descriptors (since they are pushed into the command list). They are limited to a single linear list of descriptors of the same type per pipeline layout parameter however.

Since descriptor tables are effectively just sections in descriptor heap memory, `reshade::api::descriptor_table` can be thought of a view into a `reshade::api::descriptor_heap`, similar to how `reshade::api::resource_view` are views into a `reshade::api::resource`. Different views can refer to the same underlying memory, so there can be multiple `reshade::api::descriptor_table` pointing to the same descriptors. To uniquely identify a descriptor, `reshade::api::device::get_descriptor_heap_offset()` can be used to query its offset in the descriptor heap memory.

To allocate a new descriptor table, call `reshade::api::device::allocate_descriptor_tables()`, which will do as the name implies from a descriptor heap that ReShade manages internally. The size and layout of that descriptor table is described by the passed in pipeline layout parameter. `reshade::api::device::update_descriptors()` or `reshade::api::device::update_descriptor_tables()` (for multiple updates at once) can then be used to fill the table with descriptors before using it.

The concept of `reshade::api::pipeline_layout` is functionally equivalent to `ID3D12RootSignature` in D3D12 or `VkPipelineLayout` in Vulkan. The concept of `reshade::api::descriptor_table` is functionally equivalent to descriptor tables in D3D12 or `VkDescriptorSet` in Vulkan.

