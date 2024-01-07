ReShade API
===========

The ReShade API lets you interact with the resources and rendering commands of applications ReShade was loaded into. It [abstracts](#events) away differences between the various graphics API ReShade supports (Direct3D 9/10/11/12, OpenGL and Vulkan), to make it possible to write add-ons that work across a wide range of applications, regardless of the graphics API they use.

A ReShade add-on is a DLL or part of the application that uses the header-only ReShade API to register callbacks for events and do work in those callbacks after they were invoked by ReShade. There are no further requirements, no functions need to be exported and no libraries need to be linked against (although linking against ReShade is supported as well by defining `RESHADE_API_LIBRARY` before including the headers if so desired). Simply add the [include directory from the ReShade repository](https://github.com/crosire/reshade/tree/main/include) to your project and include the `reshade.hpp` header to get started.

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

Here is a very basic code example of an add-on that registers a callback that gets executed every time a new frame is presented to the screen:

```cpp
#include <reshade.hpp>

static void on_present(reshade::api::command_queue *queue, reshade::api::swapchain *swapchain, const reshade::api::rect *source_rect, const reshade::api::rect *dest_rect, uint32_t dirty_rect_count, const reshade::api::rect *dirty_rects)
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
        reshade::register_event<reshade::addon_event::present>(&on_present);
        break;
    case DLL_PROCESS_DETACH:
        // Optionally unregister the event callback that was previously registered during process attachment again.
        reshade::unregister_event<reshade::addon_event::present>(&on_present);
        // And finally unregister the add-on from ReShade (this will automatically unregister any events and overlays registered by this add-on too).
        reshade::unregister_addon(hinstDLL);
        break;
    }
    return TRUE;
}
```

After building an add-on DLL, change its file extension from `.dll` to `.addon` and put it into the add-on search directory configured in ReShade (which defaults to the same directory as ReShade). It will be picked up and loaded automatically on the next launch of the application.

For more complex examples, see the [examples directory in the repository](https://github.com/crosire/reshade/tree/main/examples).

## Events

The graphics API abstraction is modeled after the Direct3D 12 and Vulkan APIs, so much of the terminology used should be familiar to developers that have used those before.

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

To execute rendering commands, an application has to record them into a `reshade::api::command_list` and then submit to a `reshade::api::command_queue`. In some graphics APIs there is only a single implicit command list and queue, but modern ones like Direct3D 12 and Vulkan allow the creation of multiple for more efficient multi-threaded rendering. ReShade will call the `reshade::addon_event::init_command_list` and `reshade::addon_event::init_command_queue` events after any such object was created by the application (including the implicit ones for older graphics APIs). Similarily, `reshade::addon_event::destroy_command_list` and `reshade::addon_event::destroy_command_queue` are called upon their destruction.

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
static bool on_create_swapchain(reshade::api::swapchain_desc &desc, void *hwnd)
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

In contrast to the described basic API abstraction objects, any buffers, textures, pipelines, etc. are referenced via handles. These are either created by the application and passed to events (like `reshade::addon_event::init_resource`, `reshade::addon_event::init_pipeline`, ...) or can be created through the `reshade::api::device` object of the ReShade API (via `reshade::api::device::create_resource()`, `reshade::api::device::create_pipeline()`, ...).

Buffers and textures are referenced via `reshade::api::resource` handles. Depth-stencil, render target, shader resource or unordered access views to such resources are referenced via `reshade::api::resource_view` handles. Sampler state objects are referenced via `reshade::api::sampler` handles, (partial) pipeline state objects via `reshade::api::pipeline` handles and so on.

## Overlays

It is also supported to add an overlay, which can e.g. be used to display debug information or interact with the user in-application.
Overlays are created with the use of the docking branch of [Dear ImGui](https://github.com/ocornut/imgui/tree/v1.90-docking). Including `reshade.hpp` after [`imgui.h`](https://github.com/ocornut/imgui/blob/v1.90-docking/imgui.h) will automatically overwrite all Dear ImGui functions to use the instance created and managed by ReShade. This means all you have to do is include these two headers and use Dear ImGui as usual (without having to build its source code files):

```cpp
#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#define ImTextureID ImU64 // Change ImGui texture ID type to that of a 'reshade::api::resource_view' handle

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
