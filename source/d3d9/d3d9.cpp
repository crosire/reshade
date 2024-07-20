/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d9_device.hpp"
#include "d3d9_swapchain.hpp"
#include "d3d9_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "hook_manager.hpp"
#include "addon_manager.hpp"

// These are defined in d3d9.h, but are used as function names below
#undef IDirect3D9_CreateDevice
#undef IDirect3D9Ex_CreateDeviceEx

void dump_and_modify_present_parameters(D3DPRESENT_PARAMETERS &pp, IDirect3D9 *d3d, UINT adapter_index, [[maybe_unused]] HWND focus_window)
{
	LOG(INFO) << "Dumping presentation parameters:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Parameter                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | BackBufferWidth                         | " << std::setw(39) << pp.BackBufferWidth << " |";
	LOG(INFO) << "  | BackBufferHeight                        | " << std::setw(39) << pp.BackBufferHeight << " |";

	const char *format_string = nullptr;
	switch (pp.BackBufferFormat)
	{
	case D3DFMT_UNKNOWN:
		format_string = "D3DFMT_UNKNOWN";
		break;
	case D3DFMT_A8R8G8B8:
		format_string = "D3DFMT_A8R8G8B8";
		break;
	case D3DFMT_X8R8G8B8:
		format_string = "D3DFMT_X8R8G8B8";
		break;
	case D3DFMT_R5G6B5:
		format_string = "D3DFMT_R5G6B5";
		break;
	case D3DFMT_X1R5G5B5:
		format_string = "D3DFMT_X1R5G5B5";
		break;
	case D3DFMT_A2R10G10B10:
		format_string = "D3DFMT_A2R10G10B10";
		break;
	}

	if (format_string != nullptr)
		LOG(INFO) << "  | BackBufferFormat                        | " << std::setw(39) << format_string << " |";
	else
		LOG(INFO) << "  | BackBufferFormat                        | " << std::setw(39) << pp.BackBufferFormat << " |";

	LOG(INFO) << "  | BackBufferCount                         | " << std::setw(39) << pp.BackBufferCount << " |";
	LOG(INFO) << "  | MultiSampleType                         | " << std::setw(39) << pp.MultiSampleType << " |";
	LOG(INFO) << "  | MultiSampleQuality                      | " << std::setw(39) << pp.MultiSampleQuality << " |";
	LOG(INFO) << "  | SwapEffect                              | " << std::setw(39) << pp.SwapEffect << " |";
	LOG(INFO) << "  | DeviceWindow                            | " << std::setw(39) << pp.hDeviceWindow << " |";
	LOG(INFO) << "  | Windowed                                | " << std::setw(39) << (pp.Windowed != FALSE ? "TRUE" : "FALSE") << " |";
	LOG(INFO) << "  | EnableAutoDepthStencil                  | " << std::setw(39) << (pp.EnableAutoDepthStencil ? "TRUE" : "FALSE") << " |";
	LOG(INFO) << "  | AutoDepthStencilFormat                  | " << std::setw(39) << pp.AutoDepthStencilFormat << " |";
	LOG(INFO) << "  | Flags                                   | " << std::setw(39) << std::hex << pp.Flags << std::dec << " |";
	LOG(INFO) << "  | FullScreen_RefreshRateInHz              | " << std::setw(39) << pp.FullScreen_RefreshRateInHz << " |";
	LOG(INFO) << "  | PresentationInterval                    | " << std::setw(39) << std::hex << pp.PresentationInterval << std::dec << " |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

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

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(desc, window))
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
		LOG(WARN) << "Skipping device because the device type is 'D3DDEVTYPE_NULLREF'.";
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
	LOG(DEBUG) << "Returning " << "IDirect3DDevice9" << (device_proxy->_extended_interface ? "Ex" : "") << " object " << device_proxy << " (" << device_proxy->_orig << ").";
#endif
}

// Needs to be set before entering the D3D9 runtime, to avoid hooking internal D3D device creation (e.g. when PIX is attached)
thread_local bool g_in_d3d9_runtime = false;

// Also needs to be set during D3D9 device creation, to avoid hooking internal D3D11 devices created on Windows 10
extern thread_local bool g_in_dxgi_runtime;

HRESULT STDMETHODCALLTYPE IDirect3D9_CreateDevice(IDirect3D9 *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DDevice9 **ppReturnedDeviceInterface)
{
	// Pass on unmodified in case this called from within the runtime, to avoid hooking internal devices
	if (g_in_d3d9_runtime)
		return reshade::hooks::call(IDirect3D9_CreateDevice, reshade::hooks::vtable_from_instance(pD3D) + 16)(
			pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	LOG(INFO) << "Redirecting " << "IDirect3D9::CreateDevice" << '('
		<<   "this = " << pD3D
		<< ", Adapter = " << Adapter
		<< ", DeviceType = " << DeviceType
		<< ", hFocusWindow = " << hFocusWindow
		<< ", BehaviorFlags = " << std::hex << BehaviorFlags << std::dec
		<< ", pPresentationParameters = " << pPresentationParameters
		<< ", ppReturnedDeviceInterface = " << ppReturnedDeviceInterface
		<< ')' << " ...";

	if (pPresentationParameters == nullptr)
		return D3DERR_INVALIDCALL;

	if ((BehaviorFlags & D3DCREATE_ADAPTERGROUP_DEVICE) != 0)
	{
		LOG(WARN) << "Adapter group devices are unsupported.";
		return D3DERR_NOTAVAILABLE;
	}

#if RESHADE_ADDON
	// Load add-ons before 'create_swapchain' event in 'dump_and_modify_present_parameters'
	reshade::load_addons();
#endif

	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, pD3D, Adapter, hFocusWindow);

	const bool use_software_rendering = (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0;
	if (use_software_rendering)
	{
		LOG(INFO) << "> Replacing 'D3DCREATE_SOFTWARE_VERTEXPROCESSING' flag with 'D3DCREATE_MIXED_VERTEXPROCESSING' to allow for hardware rendering.";

		BehaviorFlags = (BehaviorFlags & ~D3DCREATE_SOFTWARE_VERTEXPROCESSING) | D3DCREATE_MIXED_VERTEXPROCESSING;
	}

	assert(!g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	const HRESULT hr = reshade::hooks::call(IDirect3D9_CreateDevice, reshade::hooks::vtable_from_instance(pD3D) + 16)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, &pp, ppReturnedDeviceInterface);
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
		LOG(WARN) << "IDirect3D9::CreateDevice" << " failed with error code " << hr << '.';
	}

#if RESHADE_ADDON
	// Device proxy was created at this point, which increased the add-on manager reference count, so can release the one from above again
	reshade::unload_addons();
#endif

	return hr;
}

HRESULT STDMETHODCALLTYPE IDirect3D9Ex_CreateDeviceEx(IDirect3D9Ex *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode, IDirect3DDevice9Ex **ppReturnedDeviceInterface)
{
	if (g_in_d3d9_runtime)
		return reshade::hooks::call(IDirect3D9Ex_CreateDeviceEx, reshade::hooks::vtable_from_instance(pD3D) + 20)(
			pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

	LOG(INFO) << "Redirecting " << "IDirect3D9Ex::CreateDeviceEx" << '('
		<<   "this = " << pD3D
		<< ", Adapter = " << Adapter
		<< ", DeviceType = " << DeviceType
		<< ", hFocusWindow = " << hFocusWindow
		<< ", BehaviorFlags = " << std::hex << BehaviorFlags << std::dec
		<< ", pPresentationParameters = " << pPresentationParameters
		<< ", pFullscreenDisplayMode = " << pFullscreenDisplayMode
		<< ", ppReturnedDeviceInterface = " << ppReturnedDeviceInterface
		<< ')' << " ...";

	if (pPresentationParameters == nullptr)
		return D3DERR_INVALIDCALL;

	if ((BehaviorFlags & D3DCREATE_ADAPTERGROUP_DEVICE) != 0)
	{
		LOG(WARN) << "Adapter group devices are unsupported.";
		return D3DERR_NOTAVAILABLE;
	}

#if RESHADE_ADDON
	// Load add-ons before 'create_swapchain' event in 'dump_and_modify_present_parameters'
	reshade::load_addons();
#endif

	D3DDISPLAYMODEEX fullscreen_mode = { sizeof(fullscreen_mode) };
	if (pFullscreenDisplayMode != nullptr)
		fullscreen_mode = *pFullscreenDisplayMode;
	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, fullscreen_mode, pD3D, Adapter, hFocusWindow);

	const bool use_software_rendering = (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0;
	if (use_software_rendering)
	{
		LOG(INFO) << "> Replacing 'D3DCREATE_SOFTWARE_VERTEXPROCESSING' flag with 'D3DCREATE_MIXED_VERTEXPROCESSING' to allow for hardware rendering.";

		BehaviorFlags = (BehaviorFlags & ~D3DCREATE_SOFTWARE_VERTEXPROCESSING) | D3DCREATE_MIXED_VERTEXPROCESSING;
	}

	assert(!g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	const HRESULT hr = reshade::hooks::call(IDirect3D9Ex_CreateDeviceEx, reshade::hooks::vtable_from_instance(pD3D) + 20)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, &pp, pp.Windowed ? nullptr : &fullscreen_mode, ppReturnedDeviceInterface);
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
		LOG(WARN) << "IDirect3D9Ex::CreateDeviceEx" << " failed with error code " << hr << '.';
	}

#if RESHADE_ADDON
	// Device proxy was created at this point, which increased the add-on manager reference count, so can release the one from above again
	reshade::unload_addons();
#endif

	return hr;
}

extern "C" IDirect3D9 *WINAPI Direct3DCreate9(UINT SDKVersion)
{
	if (g_in_d3d9_runtime)
		return reshade::hooks::call(Direct3DCreate9)(SDKVersion);

	LOG(INFO) << "Redirecting " << "Direct3DCreate9" << '(' << "SDKVersion = " << SDKVersion << ')' << " ...";

	assert(!g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	IDirect3D9 *const res = reshade::hooks::call(Direct3DCreate9)(SDKVersion);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;
	if (res == nullptr)
	{
		LOG(WARN) << "Direct3DCreate9" << " failed.";
		return nullptr;
	}

	reshade::hooks::install("IDirect3D9::CreateDevice", reshade::hooks::vtable_from_instance(res), 16, reinterpret_cast<reshade::hook::address>(&IDirect3D9_CreateDevice));

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning " << "IDirect3D9" << " object " << res << '.';
#endif
	return res;
}

extern "C"     HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **ppD3D)
{
	if (g_in_d3d9_runtime)
		return reshade::hooks::call(Direct3DCreate9Ex)(SDKVersion, ppD3D);

	LOG(INFO) << "Redirecting " << "Direct3DCreate9Ex" << '(' << "SDKVersion = " << SDKVersion << ", ppD3D = " << ppD3D << ')' << " ...";

	assert(!g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	const HRESULT hr = reshade::hooks::call(Direct3DCreate9Ex)(SDKVersion, ppD3D);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		LOG(WARN) << "Direct3DCreate9Ex" << " failed with error code " << hr << '.';
		return hr;
	}

	reshade::hooks::install("IDirect3D9::CreateDevice", reshade::hooks::vtable_from_instance(*ppD3D), 16, reinterpret_cast<reshade::hook::address>(&IDirect3D9_CreateDevice));
	reshade::hooks::install("IDirect3D9Ex::CreateDeviceEx", reshade::hooks::vtable_from_instance(*ppD3D), 20, reinterpret_cast<reshade::hook::address>(&IDirect3D9Ex_CreateDeviceEx));

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning " << "IDirect3D9Ex" << " object " << *ppD3D << '.';
#endif
	return hr;
}
