/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d9_device.hpp"
#include "d3d9_swapchain.hpp"
#include "d3d9_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "hook_manager.hpp"
#include "addon_manager.hpp"

// These are defined in d3d9.h, but are used as function names below
#undef IDirect3D9_CreateDevice
#undef IDirect3D9Ex_CreateDeviceEx

static std::string format_to_string(D3DFORMAT format)
{
	switch (format)
	{
	case D3DFMT_UNKNOWN:
		return "D3DFMT_UNKNOWN";
	case D3DFMT_A8R8G8B8:
		return "D3DFMT_A8R8G8B8";
	case D3DFMT_X8R8G8B8:
		return "D3DFMT_X8R8G8B8";
	case D3DFMT_R5G6B5:
		return "D3DFMT_R5G6B5";
	case D3DFMT_X1R5G5B5:
		return "D3DFMT_X1R5G5B5";
	case D3DFMT_A2R10G10B10:
		return "D3DFMT_A2R10G10B10";
	default:
		char temp_string[11];
		return std::string(temp_string, std::snprintf(temp_string, std::size(temp_string), "%lu", static_cast<DWORD>(format)));
	}
}

void dump_and_modify_present_parameters(D3DPRESENT_PARAMETERS &pp, [[maybe_unused]] IDirect3D9 *d3d, [[maybe_unused]] UINT adapter_index, [[maybe_unused]] HWND focus_window)
{
	reshade::log::message(reshade::log::level::info, "Dumping presentation parameters:");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Parameter                               | Value                                   |");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | BackBufferWidth                         |"                                " %-39u |", pp.BackBufferWidth);
	reshade::log::message(reshade::log::level::info, "  | BackBufferHeight                        |"                                " %-39u |", pp.BackBufferHeight);
	reshade::log::message(reshade::log::level::info, "  | BackBufferFormat                        |"                                " %-39s |", format_to_string(pp.BackBufferFormat).c_str());
	reshade::log::message(reshade::log::level::info, "  | BackBufferCount                         |"                                " %-39u |", pp.BackBufferCount);
	reshade::log::message(reshade::log::level::info, "  | MultiSampleType                         |"                               " %-39lu |", static_cast<DWORD>(pp.MultiSampleType));
	reshade::log::message(reshade::log::level::info, "  | MultiSampleQuality                      |"                               " %-39lu |", pp.MultiSampleQuality);
	reshade::log::message(reshade::log::level::info, "  | SwapEffect                              |"                               " %-39lu |", static_cast<DWORD>(pp.SwapEffect));
	reshade::log::message(reshade::log::level::info, "  | DeviceWindow                            |"                                " %-39p |", pp.hDeviceWindow);
	reshade::log::message(reshade::log::level::info, "  | Windowed                                |"                                " %-39s |", pp.Windowed != FALSE ? "TRUE" : "FALSE");
	reshade::log::message(reshade::log::level::info, "  | EnableAutoDepthStencil                  |"                                " %-39s |", pp.EnableAutoDepthStencil ? "TRUE" : "FALSE");
	reshade::log::message(reshade::log::level::info, "  | AutoDepthStencilFormat                  |"                               " %-39lu |", static_cast<DWORD>(pp.AutoDepthStencilFormat));
	reshade::log::message(reshade::log::level::info, "  | Flags                                   |"                              " %-#39lx |", pp.Flags);
	reshade::log::message(reshade::log::level::info, "  | FullScreen_RefreshRateInHz              |"                                " %-39u |", pp.FullScreen_RefreshRateInHz);
	reshade::log::message(reshade::log::level::info, "  | PresentationInterval                    |"                               " %-#39x |", pp.PresentationInterval);
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");

#if RESHADE_ADDON
	reshade::api::swapchain_desc desc = {};
	desc.back_buffer.type = reshade::api::resource_type::surface;
	desc.back_buffer.texture.width = pp.BackBufferWidth;
	desc.back_buffer.texture.height = pp.BackBufferHeight;
	desc.back_buffer.texture.depth_or_layers = 1;
	desc.back_buffer.texture.levels = 1;
	desc.back_buffer.texture.format = reshade::d3d9::convert_format(pp.BackBufferFormat);
	desc.back_buffer.heap = reshade::api::memory_heap::gpu_only;
	desc.back_buffer.usage = reshade::api::resource_usage::render_target;

	if (pp.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES)
		desc.back_buffer.texture.samples = static_cast<uint16_t>(pp.MultiSampleType);
	else if (pp.MultiSampleType == D3DMULTISAMPLE_NONMASKABLE)
		desc.back_buffer.texture.samples = static_cast<uint16_t>(1 << pp.MultiSampleQuality);
	else
		desc.back_buffer.texture.samples = 1;

	const HWND window = (pp.hDeviceWindow != nullptr) ? pp.hDeviceWindow : focus_window;

	if (pp.Windowed)
	{
		RECT window_rect = {};
		GetClientRect(window, &window_rect);
		if (pp.BackBufferWidth == 0)
			desc.back_buffer.texture.width = window_rect.right;
		if (pp.BackBufferHeight == 0)
			desc.back_buffer.texture.height = window_rect.bottom;

		if (D3DDISPLAYMODE current_mode;
			pp.BackBufferFormat == D3DFMT_UNKNOWN &&
			SUCCEEDED(d3d->GetAdapterDisplayMode(adapter_index, &current_mode)))
		{
			desc.back_buffer.texture.format = reshade::d3d9::convert_format(current_mode.Format);
		}
	}

	desc.back_buffer_count = pp.BackBufferCount;
	desc.present_mode = pp.SwapEffect;
	desc.present_flags = pp.Flags;
	desc.fullscreen_state = pp.Windowed == FALSE;
	desc.fullscreen_refresh_rate = static_cast<float>(pp.FullScreen_RefreshRateInHz);

	if ((pp.PresentationInterval & D3DPRESENT_INTERVAL_IMMEDIATE) != 0)
		desc.sync_interval = 0;
	else if ((pp.PresentationInterval & D3DPRESENT_INTERVAL_ONE) != 0)
		desc.sync_interval = 1;
	else if ((pp.PresentationInterval & D3DPRESENT_INTERVAL_TWO) != 0)
		desc.sync_interval = 2;
	else if ((pp.PresentationInterval & D3DPRESENT_INTERVAL_THREE) != 0)
		desc.sync_interval = 3;
	else if ((pp.PresentationInterval & D3DPRESENT_INTERVAL_FOUR) != 0)
		desc.sync_interval = 4;
	else
	{
		assert(pp.PresentationInterval == D3DPRESENT_INTERVAL_DEFAULT);
		desc.sync_interval = UINT32_MAX;
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(reshade::api::device_api::d3d9, desc, window))
	{
		pp.BackBufferWidth = desc.back_buffer.texture.width;
		pp.BackBufferHeight = desc.back_buffer.texture.height;
		pp.BackBufferFormat = reshade::d3d9::convert_format(desc.back_buffer.texture.format);
		pp.BackBufferCount = desc.back_buffer_count;

		if (desc.back_buffer.texture.samples > 1)
		{
			if (pp.MultiSampleType == D3DMULTISAMPLE_NONMASKABLE)
			{
				BitScanReverse(&pp.MultiSampleQuality, desc.back_buffer.texture.samples);
			}
			else
			{
				pp.MultiSampleType = static_cast<D3DMULTISAMPLE_TYPE>(desc.back_buffer.texture.samples);
				pp.MultiSampleQuality = 0;
			}
		}
		else
		{
			pp.MultiSampleType = D3DMULTISAMPLE_NONE;
			pp.MultiSampleQuality = 0;
		}

		pp.SwapEffect = static_cast<D3DSWAPEFFECT>(desc.present_mode);
		pp.Flags = desc.present_flags;

		if (!desc.fullscreen_state)
		{
			pp.Windowed = TRUE;
			pp.FullScreen_RefreshRateInHz = 0;
		}
		else
		{
			pp.Windowed = FALSE;
			pp.FullScreen_RefreshRateInHz = static_cast<UINT>(desc.fullscreen_refresh_rate);

			// Use default values when not provided
			if (D3DDISPLAYMODE current_mode;
				SUCCEEDED(d3d->GetAdapterDisplayMode(adapter_index, &current_mode)))
			{
				if (desc.back_buffer.texture.width == 0)
					pp.BackBufferWidth = current_mode.Width;
				if (desc.back_buffer.texture.height == 0)
					pp.BackBufferHeight = current_mode.Height;
				if (desc.back_buffer.texture.format == reshade::api::format::unknown)
					pp.BackBufferFormat = current_mode.Format;
				if (desc.fullscreen_refresh_rate == 0)
					pp.FullScreen_RefreshRateInHz = current_mode.RefreshRate;
			}
		}

		switch (desc.sync_interval)
		{
		case 0:
			pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
			break;
		case 1:
			pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
			break;
		case 2:
			pp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
			break;
		case 3:
			pp.PresentationInterval = D3DPRESENT_INTERVAL_THREE;
			break;
		case 4:
			pp.PresentationInterval = D3DPRESENT_INTERVAL_FOUR;
			break;
		case UINT32_MAX:
			pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
			break;
		}
	}
#endif
}
void dump_and_modify_present_parameters(D3DPRESENT_PARAMETERS &pp, D3DDISPLAYMODEEX &fullscreen_desc, IDirect3D9 *d3d, UINT adapter_index, [[maybe_unused]] HWND focus_window)
{
	dump_and_modify_present_parameters(pp, d3d, adapter_index, focus_window);

	assert(fullscreen_desc.Size == sizeof(D3DDISPLAYMODEEX));

	// Update fullscreen display mode in case it was not provided by the application
	if (!pp.Windowed && fullscreen_desc.RefreshRate == 0)
	{
		fullscreen_desc.Width = pp.BackBufferWidth;
		fullscreen_desc.Height = pp.BackBufferHeight;
		fullscreen_desc.RefreshRate = pp.FullScreen_RefreshRateInHz;
		fullscreen_desc.Format = pp.BackBufferFormat;
	}
}

extern void init_device_proxy_for_d3d9on12(Direct3DDevice9 *device_proxy);

template <typename T>
static void init_device_proxy(T *&device, D3DDEVTYPE device_type, bool use_software_rendering)
{
	// Enable software vertex processing if the application requested a software device
	if (use_software_rendering)
		device->SetSoftwareVertexProcessing(TRUE);

	if (device_type == D3DDEVTYPE_NULLREF)
	{
		reshade::log::message(reshade::log::level::warning, "Skipping device because the device type is 'D3DDEVTYPE_NULLREF'.");
		return;
	}

	IDirect3DSwapChain9 *swapchain = nullptr;
	device->GetSwapChain(0, &swapchain);
	assert(swapchain != nullptr); // There should always be an implicit swap chain

	const auto device_proxy = new Direct3DDevice9(device, use_software_rendering);
	device_proxy->_implicit_swapchain = new Direct3DSwapChain9(device_proxy, swapchain);

	// Overwrite returned device with proxy device
	device = device_proxy;

	// Check if this device was created via D3D9on12 and hook it too if so
	init_device_proxy_for_d3d9on12(device_proxy);

#if 1
	// Upgrade to extended interface if available to prevent compatibility issues with some games
	com_ptr<IDirect3DDevice9Ex> deviceex;
	device_proxy->QueryInterface(IID_PPV_ARGS(&deviceex));
#endif

#if RESHADE_VERBOSE_LOG
	reshade::log::message(
		reshade::log::level::debug,
		"Returning IDirect3DDevice9%s object %p (%p).",
		device_proxy->_extended_interface ? "Ex" : "", device_proxy, device_proxy->_orig);
#endif
}

// Needs to be set before entering the D3D9 runtime, to avoid hooking internal D3D device creation (e.g. when PIX is attached)
thread_local bool g_in_d3d9_runtime = false;

// Also needs to be set during D3D9 device creation, to avoid hooking internal D3D11 devices created on Windows 10
extern thread_local bool g_in_dxgi_runtime;

HRESULT STDMETHODCALLTYPE IDirect3D9_CreateDevice(IDirect3D9 *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DDevice9 **ppReturnedDeviceInterface)
{
	auto trampoline = reshade::hooks::call(IDirect3D9_CreateDevice, reshade::hooks::vtable_from_instance(pD3D) + 16);

	// Pass on unmodified in case this called from within the runtime, to avoid hooking internal devices
	if (g_in_d3d9_runtime)
		return trampoline(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDirect3D9::CreateDevice(this = %p, Adapter = %u, DeviceType = %d, hFocusWindow = %p, BehaviorFlags = %#x, pPresentationParameters = %p, ppReturnedDeviceInterface = %p) ...",
		pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	if (pPresentationParameters == nullptr)
		return D3DERR_INVALIDCALL;

	if ((BehaviorFlags & D3DCREATE_ADAPTERGROUP_DEVICE) != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Adapter group devices are unsupported.");
		return D3DERR_NOTAVAILABLE;
	}

#if RESHADE_ADDON
	// Load add-ons before 'create_device' event and 'create_swapchain' event in 'dump_and_modify_present_parameters'
	reshade::load_addons();
#endif
#if RESHADE_ADDON >= 2
	uint32_t api_version = 0x9000;
	if (reshade::invoke_addon_event<reshade::addon_event::create_device>(reshade::api::device_api::d3d9, api_version) && api_version > 0x9000)
	{
		// Upgrade Direct3D 9 to Direct3D 9Ex
		trampoline =
			[](IDirect3D9 *, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DDevice9 **ppReturnedDeviceInterface) {
				assert(g_in_d3d9_runtime);
				com_ptr<IDirect3D9Ex> d3dex;
				if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &d3dex)))
					return D3DERR_NOTAVAILABLE;
				return d3dex->CreateDeviceEx(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, nullptr, reinterpret_cast<IDirect3DDevice9Ex **>(ppReturnedDeviceInterface));
			};
	}
#endif

	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, pD3D, Adapter, hFocusWindow);

	const bool use_software_rendering = (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0;
	if (use_software_rendering)
	{
		reshade::log::message(reshade::log::level::info, "> Replacing 'D3DCREATE_SOFTWARE_VERTEXPROCESSING' flag with 'D3DCREATE_MIXED_VERTEXPROCESSING' to allow for hardware rendering.");

		BehaviorFlags = (BehaviorFlags & ~D3DCREATE_SOFTWARE_VERTEXPROCESSING) | D3DCREATE_MIXED_VERTEXPROCESSING;
	}

	assert(!g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	const HRESULT hr = trampoline(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, &pp, ppReturnedDeviceInterface);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;

	// Update output values (see https://docs.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3d9-createdevice)
	pPresentationParameters->BackBufferWidth = pp.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = pp.BackBufferHeight;
	pPresentationParameters->BackBufferFormat = pp.BackBufferFormat;
	pPresentationParameters->BackBufferCount = pp.BackBufferCount;

	if (SUCCEEDED(hr))
	{
		init_device_proxy(*ppReturnedDeviceInterface, DeviceType, use_software_rendering);
	}
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3D9::CreateDevice failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}

#if RESHADE_ADDON
	// Device proxy was created at this point, which increased the add-on manager reference count, so can release the one from above again
	reshade::unload_addons();
#endif

	return hr;
}

HRESULT STDMETHODCALLTYPE IDirect3D9Ex_CreateDeviceEx(IDirect3D9Ex *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode, IDirect3DDevice9Ex **ppReturnedDeviceInterface)
{
	const auto trampoline = reshade::hooks::call(IDirect3D9Ex_CreateDeviceEx, reshade::hooks::vtable_from_instance(pD3D) + 20);

	if (g_in_d3d9_runtime)
		return trampoline(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDirect3D9Ex::CreateDeviceEx(this = %p, Adapter = %u, DeviceType = %d, hFocusWindow = %p, BehaviorFlags = %#x, pPresentationParameters = %p, pFullscreenDisplayMode = %p, ppReturnedDeviceInterface = %p) ...",
		pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

	if (pPresentationParameters == nullptr)
		return D3DERR_INVALIDCALL;

	if ((BehaviorFlags & D3DCREATE_ADAPTERGROUP_DEVICE) != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Adapter group devices are unsupported.");
		return D3DERR_NOTAVAILABLE;
	}

#if RESHADE_ADDON
	// Load add-ons before 'create_device' event and 'create_swapchain' event in 'dump_and_modify_present_parameters'
	reshade::load_addons();
#endif
#if RESHADE_ADDON >= 2
	uint32_t api_version = 0x9100;
	reshade::invoke_addon_event<reshade::addon_event::create_device>(reshade::api::device_api::d3d9, api_version);
#endif

	D3DDISPLAYMODEEX fullscreen_mode = { sizeof(fullscreen_mode) };
	if (pFullscreenDisplayMode != nullptr)
		fullscreen_mode = *pFullscreenDisplayMode;
	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, fullscreen_mode, pD3D, Adapter, hFocusWindow);

	const bool use_software_rendering = (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0;
	if (use_software_rendering)
	{
		reshade::log::message(reshade::log::level::info, "> Replacing 'D3DCREATE_SOFTWARE_VERTEXPROCESSING' flag with 'D3DCREATE_MIXED_VERTEXPROCESSING' to allow for hardware rendering.");

		BehaviorFlags = (BehaviorFlags & ~D3DCREATE_SOFTWARE_VERTEXPROCESSING) | D3DCREATE_MIXED_VERTEXPROCESSING;
	}

	assert(!g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	const HRESULT hr = trampoline(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, &pp, pp.Windowed ? nullptr : &fullscreen_mode, ppReturnedDeviceInterface);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;

	// Update output values (see https://docs.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3d9ex-createdeviceex)
	pPresentationParameters->BackBufferWidth = pp.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = pp.BackBufferHeight;
	pPresentationParameters->BackBufferFormat = pp.BackBufferFormat;
	pPresentationParameters->BackBufferCount = pp.BackBufferCount;

	if (SUCCEEDED(hr))
	{
		init_device_proxy(*ppReturnedDeviceInterface, DeviceType, use_software_rendering);
	}
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3D9Ex::CreateDeviceEx failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}

#if RESHADE_ADDON
	// Device proxy was created at this point, which increased the add-on manager reference count, so can release the one from above again
	reshade::unload_addons();
#endif

	return hr;
}

extern "C" IDirect3D9 *WINAPI Direct3DCreate9(UINT SDKVersion)
{
	const auto trampoline = reshade::hooks::call(Direct3DCreate9);

	if (g_in_d3d9_runtime)
		return trampoline(SDKVersion);

	reshade::log::message(reshade::log::level::info, "Redirecting Direct3DCreate9(SDKVersion = %#x) ...", SDKVersion);

	assert(!g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	IDirect3D9 *const res = trampoline(SDKVersion);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;
	if (res == nullptr)
	{
		reshade::log::message(reshade::log::level::warning, "Direct3DCreate9 failed.");
		return nullptr;
	}

	reshade::hooks::install("IDirect3D9::CreateDevice", reshade::hooks::vtable_from_instance(res), 16, reinterpret_cast<reshade::hook::address>(&IDirect3D9_CreateDevice));

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning IDirect3D9 object %p.", res);
#endif
	return res;
}

extern "C"     HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **ppD3D)
{
	const auto trampoline = reshade::hooks::call(Direct3DCreate9Ex);

	if (g_in_d3d9_runtime)
		return trampoline(SDKVersion, ppD3D);

	reshade::log::message(reshade::log::level::info, "Redirecting Direct3DCreate9Ex(SDKVersion = %#x, ppD3D = %p) ...", SDKVersion, ppD3D);

	assert(!g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	const HRESULT hr = trampoline(SDKVersion, ppD3D);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "Direct3DCreate9Ex failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	assert(ppD3D != nullptr);

	reshade::hooks::install("IDirect3D9Ex::CreateDevice", reshade::hooks::vtable_from_instance(*ppD3D), 16, reinterpret_cast<reshade::hook::address>(&IDirect3D9_CreateDevice));
	reshade::hooks::install("IDirect3D9Ex::CreateDeviceEx", reshade::hooks::vtable_from_instance(*ppD3D), 20, reinterpret_cast<reshade::hook::address>(&IDirect3D9Ex_CreateDeviceEx));

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning IDirect3D9Ex object %p.", *ppD3D);
#endif
	return hr;
}
