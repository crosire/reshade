# Reflex Latency Markers in ReShade

This document explains the six NVIDIA Reflex latency markers and how ReShade exposes corresponding addon callbacks so addons can observe timestamps when they occur. ReShade does not generate Reflex markers; it only notifies when analogous points happen in the rendering loop.

## The six markers

- SIMULATION_START: The game begins simulation for a frame (input sampling, game logic start).
- SIMULATION_END: The game finishes simulation for a frame.
- RENDERSUBMIT_START: CPU begins submitting rendering work for the frame.
- RENDERSUBMIT_END: CPU finishes submitting rendering work for the frame.
- PRESENT_START: Immediately before the platform present call.
- PRESENT_END: Immediately after the present call returns.

## How Special-K generates markers (for reference)

Special-K integrates with NVAPI and PCLStats. It intercepts `NvAPI_D3D_SetLatencyMarker` and uses `PCLSTATS_MARKER` to log events. Relevant code:

```deps/SpecialK/src/render/reflex/reflex.cpp
// PCLSTATS_MARKER (Marker, FrameID) is used to log, e.g. SIMULATION_START, RENDERSUBMIT_START, PRESENT_START, etc.
```

Special-K maps events:
- SIMULATION_* markers are invoked when the game (or SK heuristics) identifies game logic boundaries.
- RENDERSUBMIT_* are around command list submissions.
- PRESENT_* surround the present call.

## ReShade addon callbacks

ReShade adds six new addon events that mirror these semantics. Each callback includes a high-resolution monotonic timestamp in nanoseconds.

Event signatures:
- latency_simulation_start(queue, swapchain, timestamp_ns)
- latency_simulation_end(queue, swapchain, timestamp_ns)
- latency_rendersubmit_start(queue, swapchain, timestamp_ns)
- latency_rendersubmit_end(queue, swapchain, timestamp_ns)
- latency_present_start(queue, swapchain, timestamp_ns)
- latency_present_end(queue, swapchain, timestamp_ns)

Notes:
- `queue` is the best available command submission object for the API. For D3D12/Vulkan this is the real queue; for D3D11/D3D10 it is the immediate context/device; for D3D9/OpenGL, it may be null if not applicable.
- `swapchain` is the active swapchain for the frame if available.
- `timestamp_ns` is `std::chrono::high_resolution_clock::now()` expressed in nanoseconds. It is intended for relative timing within a process, not cross-process correlation.

## Where ReShade emits these callbacks

- PRESENT_START: Emitted at the top of `DXGISwapChain::Present(...)`, before ReShade’s internal `on_present` handling.
- PRESENT_END: Emitted immediately after `IDXGISwapChain::Present` returns.
- RENDERSUBMIT_START/END (D3D12): Emitted in `D3D12CommandQueue::ExecuteCommandLists` before and after calling the original queue submission.
- RENDERSUBMIT_START/END (D3D11): Emitted in `D3D11DeviceContext::ExecuteCommandList` on the immediate context path, around the underlying `ExecuteCommandList` call.
- SIMULATION_*: Not emitted yet; these are reserved for future integration (e.g., if ReShade observes NVAPI markers). Addons should not rely on them being called today.

## Addon usage example

```cpp
static void on_present_start(reshade::api::command_queue *q, reshade::api::swapchain *sc, uint64_t tns) {
    // record timestamp
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        if (!reshade::register_addon(hModule)) return FALSE;
        reshade::register_event<reshade::addon_event::latency_present_start>(on_present_start);
    } else if (reason == DLL_PROCESS_DETACH) {
        reshade::unregister_addon(hModule);
    }
    return TRUE;
}
```

## Limitations

- Timestamps are process-local; correlating to GPU hardware timestamps or OS compositor events is out of scope here.
- SIMULATION_* callbacks are placeholders for now.
- Exact queuing boundaries vary by API and application.
