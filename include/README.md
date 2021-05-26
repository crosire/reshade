ReShade API
===========

The ReShade API is a graphics API (Direct3D 9/10/11/12/OpenGL/Vulkan) agnostic toolset that lets you register callbacks to certain events the application ReShade is loaded into produces and in those interact with the graphics objects created by the application.\
This means it is possible to write add-ons that change how an application calls its graphics API, without actually having to know the specifics and also ensures such add-ons work on all applications ReShade supports, regardless of the graphics API they use.

A ReShade add-on is simply a DLL that uses the header-only ReShade API to register events and do work in the callbacks. There are no further requirements, no functions need to be exported and no libraries need to be linked against. Simply add this include directory to your DLL project and include the `reshade.hpp` header to get started.

Here is a very basic code example of an add-on that registers a callback that gets executed every time a new frame is presented to the screen:

```cpp
#define RESHADE_ADDON_IMPL // Define this before including the ReShade header in exactly one source file
#include <reshade.hpp>

static void on_present(reshade::api::command_queue *, reshade::api::swapchain *swapchain)
{
	// ...
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // Call 'reshade::init_addon' before you call any other function of the ReShade API
        // This will look for the ReShade instance in the current process and initialize the API when found
        if (!reshade::init_addon())
            return FALSE;
        // This registers a callback for the 'present' event, which occurs every time a new frame is presented to the screen
        // The function signature has to match the type defined by 'reshade::addon_event_traits<reshade::addon_event::present>::decl'
        // For more details check the inline documentation for each event in 'reshade_events.hpp'
        reshade::register_event<reshade::addon_event::present>(on_present);
        break;
    case DLL_PROCESS_DETACH:
        // Before the add-on is unloaded, be sure to unregister any event callbacks that where previously registered
        reshade::unregister_event<reshade::addon_event::present>(on_present);
        break;
    }
    return TRUE;
}
```
