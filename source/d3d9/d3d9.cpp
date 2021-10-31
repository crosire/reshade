/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d9_device.hpp"
#include "d3d9_swapchain.hpp"
#include "d3d9_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "ini_file.hpp"
#include "hook_manager.hpp"

// These are defined in d3d9.h, but are used as function names below
#undef IDirect3D9_CreateDevice
#undef IDirect3D9Ex_CreateDeviceEx

void dump_and_modify_present_parameters(D3DPRESENT_PARAMETERS &pp, IDirect3D9 *d3d, UINT adapter_index)
{
	LOG(INFO) << "> Dumping presentation parameters:";
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
	reshade::api::resource_desc buffer_desc = {};
	buffer_desc.type = reshade::api::resource_type::surface;
	buffer_desc.texture.width = pp.BackBufferWidth;
	buffer_desc.texture.height = pp.BackBufferHeight;
	buffer_desc.texture.depth_or_layers = 1;
	buffer_desc.texture.levels = 1;
	buffer_desc.texture.format = reshade::d3d9::convert_format(pp.BackBufferFormat);
	buffer_desc.texture.samples = pp.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES ? static_cast<uint16_t>(pp.MultiSampleType) : 1;
	buffer_desc.heap = reshade::api::memory_heap::gpu_only;
	buffer_desc.usage = reshade::api::resource_usage::render_target;

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(buffer_desc, pp.hDeviceWindow))
	{
		pp.BackBufferWidth = buffer_desc.texture.width;
		pp.BackBufferHeight = buffer_desc.texture.height;
		pp.BackBufferFormat = reshade::d3d9::convert_format(buffer_desc.texture.format);

		if (buffer_desc.texture.samples > 1)
			pp.MultiSampleType = static_cast<D3DMULTISAMPLE_TYPE>(buffer_desc.texture.samples);
		else
			pp.MultiSampleType = D3DMULTISAMPLE_NONE;
	}
#endif

	if (reshade::global_config().get("APP", "ForceVSync"))
	{
		pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	}

	if (reshade::global_config().get("APP", "ForceWindowed"))
	{
		pp.Windowed = TRUE;
		pp.FullScreen_RefreshRateInHz = 0;
	}
	if (reshade::global_config().get("APP", "ForceFullscreen"))
	{
		D3DDISPLAYMODE current_mode = {};
		d3d->GetAdapterDisplayMode(adapter_index, &current_mode);

		pp.BackBufferWidth = current_mode.Width;
		pp.BackBufferHeight = current_mode.Height;
		pp.BackBufferFormat = current_mode.Format;
		pp.Windowed = FALSE;
		pp.FullScreen_RefreshRateInHz = current_mode.RefreshRate;
	}

	if (unsigned int force_resolution[2] = {};
		reshade::global_config().get("APP", "ForceResolution", force_resolution) && force_resolution[0] != 0 && force_resolution[1] != 0)
	{
		pp.BackBufferWidth = force_resolution[0];
		pp.BackBufferHeight = force_resolution[1];
	}

	if (reshade::global_config().get("APP", "Force10BitFormat"))
	{
		pp.BackBufferFormat = D3DFMT_A2R10G10B10;
	}
}
void dump_and_modify_present_parameters(D3DPRESENT_PARAMETERS &pp, D3DDISPLAYMODEEX &fullscreen_desc, IDirect3D9 *d3d, UINT adapter_index)
{
	dump_and_modify_present_parameters(pp, d3d, adapter_index);

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

	// Overwrite returned device with hooked one
	device = device_proxy;

#if 1
	// Upgrade to extended interface if available to prevent compatibility issues with some games
	com_ptr<IDirect3DDevice9Ex> deviceex;
	device_proxy->QueryInterface(IID_PPV_ARGS(&deviceex));
#endif

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning IDirect3DDevice9" << (device_proxy->_extended_interface ? "Ex" : "") << " object " << device << '.';
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
		return reshade::hooks::call(IDirect3D9_CreateDevice, vtable_from_instance(pD3D) + 16)(
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

	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, pD3D, Adapter);

	const bool use_software_rendering = (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0;
	if (use_software_rendering)
	{
		LOG(INFO) << "> Replacing 'D3DCREATE_SOFTWARE_VERTEXPROCESSING' flag with 'D3DCREATE_MIXED_VERTEXPROCESSING' to allow for hardware rendering ...";
		BehaviorFlags = (BehaviorFlags & ~D3DCREATE_SOFTWARE_VERTEXPROCESSING) | D3DCREATE_MIXED_VERTEXPROCESSING;
	}

	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	const HRESULT hr = reshade::hooks::call(IDirect3D9_CreateDevice, vtable_from_instance(pD3D) + 16)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, &pp, ppReturnedDeviceInterface);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;

	// Update output values (see https://docs.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3d9-createdevice)
	pPresentationParameters->BackBufferWidth = pp.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = pp.BackBufferHeight;
	pPresentationParameters->BackBufferFormat = pp.BackBufferFormat;
	pPresentationParameters->BackBufferCount = pp.BackBufferCount;

	if (FAILED(hr))
	{
		LOG(WARN) << "IDirect3D9::CreateDevice" << " failed with error code " << hr << '.';
		return hr;
	}

	init_device_proxy(*ppReturnedDeviceInterface, DeviceType, use_software_rendering);

	return hr;
}

HRESULT STDMETHODCALLTYPE IDirect3D9Ex_CreateDeviceEx(IDirect3D9Ex *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode, IDirect3DDevice9Ex **ppReturnedDeviceInterface)
{
	if (g_in_d3d9_runtime)
		return reshade::hooks::call(IDirect3D9Ex_CreateDeviceEx, vtable_from_instance(pD3D) + 20)(
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

	D3DDISPLAYMODEEX fullscreen_mode = { sizeof(fullscreen_mode) };
	if (pFullscreenDisplayMode != nullptr)
		fullscreen_mode = *pFullscreenDisplayMode;
	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, fullscreen_mode, pD3D, Adapter);

	const bool use_software_rendering = (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0;
	if (use_software_rendering)
	{
		LOG(INFO) << "> Replacing 'D3DCREATE_SOFTWARE_VERTEXPROCESSING' flag with 'D3DCREATE_MIXED_VERTEXPROCESSING' to allow for hardware rendering ...";
		BehaviorFlags = (BehaviorFlags & ~D3DCREATE_SOFTWARE_VERTEXPROCESSING) | D3DCREATE_MIXED_VERTEXPROCESSING;
	}

	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	const HRESULT hr = reshade::hooks::call(IDirect3D9Ex_CreateDeviceEx, vtable_from_instance(pD3D) + 20)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, &pp, pp.Windowed ? nullptr : &fullscreen_mode, ppReturnedDeviceInterface);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;

	// Update output values (see https://docs.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3d9ex-createdeviceex)
	pPresentationParameters->BackBufferWidth = pp.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = pp.BackBufferHeight;
	pPresentationParameters->BackBufferFormat = pp.BackBufferFormat;
	pPresentationParameters->BackBufferCount = pp.BackBufferCount;

	if (FAILED(hr))
	{
		LOG(WARN) << "IDirect3D9Ex::CreateDeviceEx" << " failed with error code " << hr << '.';
		return hr;
	}

	init_device_proxy(*ppReturnedDeviceInterface, DeviceType, use_software_rendering);

	return hr;
}

HOOK_EXPORT IDirect3D9 *WINAPI Direct3DCreate9(UINT SDKVersion)
{
	if (g_in_d3d9_runtime)
		return reshade::hooks::call(Direct3DCreate9)(SDKVersion);

	LOG(INFO) << "Redirecting " << "Direct3DCreate9" << '(' << "SDKVersion = " << SDKVersion << ')' << " ...";

	g_in_d3d9_runtime = true;
	IDirect3D9 *const res = reshade::hooks::call(Direct3DCreate9)(SDKVersion);
	g_in_d3d9_runtime = false;
	if (res == nullptr)
	{
		LOG(WARN) << "Direct3DCreate9" << " failed!";
		return nullptr;
	}

	reshade::hooks::install("IDirect3D9::CreateDevice", vtable_from_instance(res), 16, reinterpret_cast<reshade::hook::address>(IDirect3D9_CreateDevice));

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning IDirect3D9 object " << res << '.';
#endif
	return res;
}

HOOK_EXPORT     HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **ppD3D)
{
	if (g_in_d3d9_runtime)
		return reshade::hooks::call(Direct3DCreate9Ex)(SDKVersion, ppD3D);

	LOG(INFO) << "Redirecting " << "Direct3DCreate9Ex" << '(' << "SDKVersion = " << SDKVersion << ", ppD3D = " << ppD3D << ')' << " ...";

	g_in_d3d9_runtime = true;
	const HRESULT hr = reshade::hooks::call(Direct3DCreate9Ex)(SDKVersion, ppD3D);
	g_in_d3d9_runtime = false;
	if (FAILED(hr))
	{
		LOG(WARN) << "Direct3DCreate9Ex" << " failed with error code " << hr << '.';
		return hr;
	}

	reshade::hooks::install("IDirect3D9::CreateDevice", vtable_from_instance(*ppD3D), 16, reinterpret_cast<reshade::hook::address>(IDirect3D9_CreateDevice));
	reshade::hooks::install("IDirect3D9Ex::CreateDeviceEx", vtable_from_instance(*ppD3D), 20, reinterpret_cast<reshade::hook::address>(IDirect3D9Ex_CreateDeviceEx));

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning IDirect3D9Ex object " << *ppD3D << '.';
#endif
	return hr;
}

HOOK_EXPORT IDirect3D9 *WINAPI Direct3DCreate9on12(UINT SDKVersion, struct D3D9ON12_ARGS *pOverrideList, UINT NumOverrideEntries) // Export ordinal 20
{
	LOG(INFO) << "Redirecting " << "Direct3DCreate9on12" << '(' << "SDKVersion = " << SDKVersion << ", pOverrideList = " << pOverrideList << ", NumOverrideEntries = " << NumOverrideEntries << ')' << " ...";

	return reshade::hooks::call(Direct3DCreate9on12)(SDKVersion, pOverrideList, NumOverrideEntries);
}

HOOK_EXPORT     HRESULT WINAPI Direct3DCreate9on12Ex(UINT SDKVersion, struct D3D9ON12_ARGS *pOverrideList, UINT NumOverrideEntries, IDirect3D9Ex **ppOutputInterface) // Export ordinal 21
{
	LOG(INFO) << "Redirecting " << "Direct3DCreate9on12Ex" << '(' << "SDKVersion = " << SDKVersion << ", pOverrideList = " << pOverrideList << ", NumOverrideEntries = " << NumOverrideEntries << ", ppOutputInterface = " << ppOutputInterface << ')' << " ...";

	return reshade::hooks::call(Direct3DCreate9on12Ex)(SDKVersion, pOverrideList, NumOverrideEntries, ppOutputInterface);
}
