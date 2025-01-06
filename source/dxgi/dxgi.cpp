/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dxgi_swapchain.hpp"
#include "d3d10/d3d10_device.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "com_utils.hpp"
#include "hook_manager.hpp"
#include "addon_manager.hpp"

extern bool is_windows7();

// Needs to be set whenever a DXGI call can end up in 'CDXGISwapChain::EnsureChildDeviceInternal', to avoid hooking internal D3D device creation
extern thread_local bool g_in_dxgi_runtime;

#if RESHADE_ADDON
static auto floating_point_to_rational(float value) -> DXGI_RATIONAL
{
	float integer_component = 0.0f;
	const float fractional_component = std::modf(value, &integer_component);
	if (fractional_component == 0.0f)
		return DXGI_RATIONAL { static_cast<UINT>(value), 1 };
	else
		return DXGI_RATIONAL { static_cast<UINT>(value * 1000), 1000 };
}
static auto rational_to_floating_point(DXGI_RATIONAL value) -> float
{
	return value.Denominator != 0 ? static_cast<float>(value.Numerator) / static_cast<float>(value.Denominator) : 0.0f;
}

bool modify_swapchain_desc(DXGI_SWAP_CHAIN_DESC &internal_desc)
{
	reshade::api::swapchain_desc desc = {};
	desc.back_buffer.type = reshade::api::resource_type::texture_2d;
	desc.back_buffer.texture.width = internal_desc.BufferDesc.Width;
	desc.back_buffer.texture.height = internal_desc.BufferDesc.Height;
	desc.back_buffer.texture.depth_or_layers = 1;
	desc.back_buffer.texture.levels = 1;
	desc.back_buffer.texture.format = static_cast<reshade::api::format>(internal_desc.BufferDesc.Format);
	desc.back_buffer.texture.samples = static_cast<uint16_t>(internal_desc.SampleDesc.Count);
	desc.back_buffer.heap = reshade::api::memory_heap::gpu_only;

	RECT window_rect = {};
	GetClientRect(internal_desc.OutputWindow, &window_rect);
	if (internal_desc.BufferDesc.Width == 0)
		desc.back_buffer.texture.width = window_rect.right;
	if (internal_desc.BufferDesc.Height == 0)
		desc.back_buffer.texture.height = window_rect.bottom;

	if (internal_desc.BufferUsage & DXGI_USAGE_SHADER_INPUT)
		desc.back_buffer.usage |= reshade::api::resource_usage::shader_resource;
	if (internal_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT)
		desc.back_buffer.usage |= reshade::api::resource_usage::render_target;
	if (internal_desc.BufferUsage & DXGI_USAGE_SHARED)
		desc.back_buffer.flags |= reshade::api::resource_flags::shared;
	if (internal_desc.BufferUsage & DXGI_USAGE_UNORDERED_ACCESS)
		desc.back_buffer.usage |= reshade::api::resource_usage::unordered_access;

	desc.back_buffer_count = internal_desc.BufferCount;
	desc.present_mode = static_cast<uint32_t>(internal_desc.SwapEffect);
	desc.present_flags = internal_desc.Flags;
	desc.fullscreen_state = internal_desc.Windowed == FALSE;
	desc.fullscreen_refresh_rate = rational_to_floating_point(internal_desc.BufferDesc.RefreshRate);

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(desc, internal_desc.OutputWindow))
	{
		internal_desc.BufferDesc.Width = desc.back_buffer.texture.width;
		internal_desc.BufferDesc.Height = desc.back_buffer.texture.height;
		internal_desc.BufferDesc.RefreshRate = floating_point_to_rational(desc.fullscreen_refresh_rate);
		internal_desc.BufferDesc.Format = static_cast<DXGI_FORMAT>(desc.back_buffer.texture.format);
		internal_desc.SampleDesc.Count = desc.back_buffer.texture.samples;

		if ((desc.back_buffer.usage & reshade::api::resource_usage::shader_resource) != 0)
			internal_desc.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
		if ((desc.back_buffer.usage & reshade::api::resource_usage::render_target) != 0)
			internal_desc.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		if ((desc.back_buffer.flags & reshade::api::resource_flags::shared) != 0)
			internal_desc.BufferUsage |= DXGI_USAGE_SHARED;
		if ((desc.back_buffer.usage & reshade::api::resource_usage::unordered_access) != 0)
			internal_desc.BufferUsage |= DXGI_USAGE_UNORDERED_ACCESS;

		internal_desc.BufferCount = desc.back_buffer_count;
		internal_desc.Windowed = !desc.fullscreen_state;
		internal_desc.SwapEffect = static_cast<DXGI_SWAP_EFFECT>(desc.present_mode);
		internal_desc.Flags = desc.present_flags;

		return true;
	}

	return false;
}
bool modify_swapchain_desc(DXGI_SWAP_CHAIN_DESC1 &internal_desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fullscreen_desc, HWND window)
{
	reshade::api::swapchain_desc desc = {};
	desc.back_buffer.type = reshade::api::resource_type::texture_2d;
	desc.back_buffer.texture.width = internal_desc.Width;
	desc.back_buffer.texture.height = internal_desc.Height;
	desc.back_buffer.texture.depth_or_layers = internal_desc.Stereo ? 2 : 1;
	desc.back_buffer.texture.levels = 1;
	desc.back_buffer.texture.format = static_cast<reshade::api::format>(internal_desc.Format);
	desc.back_buffer.texture.samples = static_cast<uint16_t>(internal_desc.SampleDesc.Count);
	desc.back_buffer.heap = reshade::api::memory_heap::gpu_only;

	if (window != nullptr)
	{
		RECT window_rect = {};
		GetClientRect(window, &window_rect);
		if (internal_desc.Width == 0)
			desc.back_buffer.texture.width = window_rect.right;
		if (internal_desc.Height == 0)
			desc.back_buffer.texture.height = window_rect.bottom;
	}

	if (internal_desc.BufferUsage & DXGI_USAGE_SHADER_INPUT)
		desc.back_buffer.usage |= reshade::api::resource_usage::shader_resource;
	if (internal_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT)
		desc.back_buffer.usage |= reshade::api::resource_usage::render_target;
	if (internal_desc.BufferUsage & DXGI_USAGE_SHARED)
		desc.back_buffer.flags |= reshade::api::resource_flags::shared;
	if (internal_desc.BufferUsage & DXGI_USAGE_UNORDERED_ACCESS)
		desc.back_buffer.usage |= reshade::api::resource_usage::unordered_access;

	desc.back_buffer_count = internal_desc.BufferCount;
	desc.present_mode = static_cast<uint32_t>(internal_desc.SwapEffect);
	desc.present_flags = internal_desc.Flags;

	if (fullscreen_desc != nullptr)
	{
		desc.fullscreen_state = fullscreen_desc->Windowed == FALSE;
		desc.fullscreen_refresh_rate = rational_to_floating_point(fullscreen_desc->RefreshRate);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(desc, window))
	{
		internal_desc.Width = desc.back_buffer.texture.width;
		internal_desc.Height = desc.back_buffer.texture.height;
		internal_desc.Format = static_cast<DXGI_FORMAT>(desc.back_buffer.texture.format);
		internal_desc.Stereo = desc.back_buffer.texture.depth_or_layers > 1;
		internal_desc.SampleDesc.Count = desc.back_buffer.texture.samples;

		if ((desc.back_buffer.usage & reshade::api::resource_usage::shader_resource) != 0)
			internal_desc.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
		if ((desc.back_buffer.usage & reshade::api::resource_usage::render_target) != 0)
			internal_desc.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		if ((desc.back_buffer.flags & reshade::api::resource_flags::shared) != 0)
			internal_desc.BufferUsage |= DXGI_USAGE_SHARED;
		if ((desc.back_buffer.usage & reshade::api::resource_usage::unordered_access) != 0)
			internal_desc.BufferUsage |= DXGI_USAGE_UNORDERED_ACCESS;

		internal_desc.BufferCount = desc.back_buffer_count;
		internal_desc.SwapEffect = static_cast<DXGI_SWAP_EFFECT>(desc.present_mode);
		internal_desc.Flags = desc.present_flags;

		if (fullscreen_desc != nullptr)
		{
			fullscreen_desc->RefreshRate = floating_point_to_rational(desc.fullscreen_refresh_rate);
			fullscreen_desc->Windowed = !desc.fullscreen_state;
		}

		return true;
	}

	return false;
}
#endif

static std::string format_to_string(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_UNKNOWN:
		return "DXGI_FORMAT_UNKNOWN";
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return "DXGI_FORMAT_R8G8B8A8_UNORM";
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB";
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return "DXGI_FORMAT_B8G8R8A8_UNORM";
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB";
	case DXGI_FORMAT_R10G10B10A2_UNORM:
		return "DXGI_FORMAT_R10G10B10A2_UNORM";
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return "DXGI_FORMAT_R16G16B16A16_FLOAT";
	default:
		char temp_string[11];
		return std::string(temp_string, std::snprintf(temp_string, std::size(temp_string), "%u", static_cast<unsigned int>(format)));
	}
}

static void dump_and_modify_swapchain_desc(DXGI_SWAP_CHAIN_DESC &desc)
{
	reshade::log::message(reshade::log::level::info, "> Dumping swap chain description:");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Parameter                               | Value                                   |");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Width                                   |"                                " %-39u |", desc.BufferDesc.Width);
	reshade::log::message(reshade::log::level::info, "  | Height                                  |"                                " %-39u |", desc.BufferDesc.Height);
	reshade::log::message(reshade::log::level::info, "  | RefreshRate                             |"            " %-19u"            " %-19u |", desc.BufferDesc.RefreshRate.Numerator, desc.BufferDesc.RefreshRate.Denominator);
	reshade::log::message(reshade::log::level::info, "  | Format                                  |"                                " %-39s |", format_to_string(desc.BufferDesc.Format).c_str());
	reshade::log::message(reshade::log::level::info, "  | ScanlineOrdering                        |"                                " %-39d |", static_cast<int>(desc.BufferDesc.ScanlineOrdering));
	reshade::log::message(reshade::log::level::info, "  | Scaling                                 |"                                " %-39d |", static_cast<int>(desc.BufferDesc.Scaling));
	reshade::log::message(reshade::log::level::info, "  | SampleCount                             |"                                " %-39u |", desc.SampleDesc.Count);
	reshade::log::message(reshade::log::level::info, "  | SampleQuality                           |"                                " %-39d |", static_cast<int>(desc.SampleDesc.Quality));
	reshade::log::message(reshade::log::level::info, "  | BufferUsage                             |"                               " %-#39x |", desc.BufferUsage);
	reshade::log::message(reshade::log::level::info, "  | BufferCount                             |"                                " %-39u |", desc.BufferCount);
	reshade::log::message(reshade::log::level::info, "  | OutputWindow                            |"                                " %-39p |", desc.OutputWindow);
	reshade::log::message(reshade::log::level::info, "  | Windowed                                |"                                " %-39s |", desc.Windowed ? "TRUE" : "FALSE");
	reshade::log::message(reshade::log::level::info, "  | SwapEffect                              |"                                " %-39d |", static_cast<int>(desc.SwapEffect));
	reshade::log::message(reshade::log::level::info, "  | Flags                                   |"                               " %-#39x |", desc.Flags);
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");

#if RESHADE_ADDON
	modify_swapchain_desc(desc);
#endif
}
static void dump_and_modify_swapchain_desc(DXGI_SWAP_CHAIN_DESC1 &desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fullscreen_desc = nullptr, [[maybe_unused]] HWND window = nullptr)
{
	reshade::log::message(reshade::log::level::info, "> Dumping swap chain description:");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Parameter                               | Value                                   |");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Width                                   |"                                " %-39u |", desc.Width);
	reshade::log::message(reshade::log::level::info, "  | Height                                  |"                                " %-39u |", desc.Height);
	if (fullscreen_desc != nullptr)
	{
	reshade::log::message(reshade::log::level::info, "  | RefreshRate                             |"              " %-19u"          " %-19u |", fullscreen_desc->RefreshRate.Numerator, fullscreen_desc->RefreshRate.Denominator);
	}
	reshade::log::message(reshade::log::level::info, "  | Format                                  |"                                " %-39s |", format_to_string(desc.Format).c_str());
	reshade::log::message(reshade::log::level::info, "  | Stereo                                  |"                                " %-39s |", desc.Stereo ? "TRUE" : "FALSE");
	if (fullscreen_desc != nullptr)
	{
	reshade::log::message(reshade::log::level::info, "  | ScanlineOrdering                        |"                                " %-39d |", static_cast<int>(fullscreen_desc->ScanlineOrdering));
	reshade::log::message(reshade::log::level::info, "  | Scaling                                 |"                                " %-39d |", static_cast<int>(fullscreen_desc->Scaling));
	}
	reshade::log::message(reshade::log::level::info, "  | SampleCount                             |"                                " %-39u |", desc.SampleDesc.Count);
	reshade::log::message(reshade::log::level::info, "  | SampleQuality                           |"                                " %-39d |", static_cast<int>(desc.SampleDesc.Quality));
	reshade::log::message(reshade::log::level::info, "  | BufferUsage                             |"                               " %-#39x |", desc.BufferUsage);
	reshade::log::message(reshade::log::level::info, "  | BufferCount                             |"                                " %-39u |", desc.BufferCount);
	if (fullscreen_desc != nullptr)
	{
	reshade::log::message(reshade::log::level::info, "  | Windowed                                |"                                " %-39s |", fullscreen_desc->Windowed ? "TRUE" : "FALSE");
	}
	reshade::log::message(reshade::log::level::info, "  | SwapEffect                              |"                                " %-39d |", static_cast<int>(desc.SwapEffect));
	reshade::log::message(reshade::log::level::info, "  | AlphaMode                               |"                                " %-39d |", static_cast<int>(desc.AlphaMode));
	reshade::log::message(reshade::log::level::info, "  | Flags                                   |"                               " %-#39x |", desc.Flags);
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");

#if RESHADE_ADDON
	modify_swapchain_desc(desc, fullscreen_desc, window);
#endif
}

UINT query_device(IUnknown *&device, com_ptr<IUnknown> &device_proxy)
{
	if (com_ptr<D3D10Device> device_d3d10;
		SUCCEEDED(device->QueryInterface(&device_d3d10)))
	{
		device = device_d3d10->_orig; // Set device pointer back to original object so that the swap chain creation functions work as expected
		device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(device_d3d10));
		return 10;
	}
	if (com_ptr<D3D11Device> device_d3d11;
		SUCCEEDED(device->QueryInterface(&device_d3d11)))
	{
		device = device_d3d11->_orig;
		device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(device_d3d11));
		return 11;
	}
	if (com_ptr<D3D12CommandQueue> command_queue_d3d12;
		SUCCEEDED(device->QueryInterface(&command_queue_d3d12)))
	{
		device = command_queue_d3d12->_orig;
		device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(command_queue_d3d12));
		return 12;
	}

	// Fall back to checking private data in case original device pointer was passed in (e.g. because D3D11 device was created with video support and then queried though 'D3D11Device::QueryInterface')
	// Note that D3D11 devices can expose the 'ID3D10Device' interface too, if 'ID3D11Device::CreateDeviceContextState' has been called
	// But since there is a follow-up check for the proxy 'D3D10Device' interface that is only set on real D3D10 devices, the query order doesn't matter here
	if (com_ptr<ID3D10Device> device_d3d10_orig;
		SUCCEEDED(device->QueryInterface(&device_d3d10_orig)))
	{
		if (com_ptr<D3D10Device> device_d3d10 = get_private_pointer_d3dx<D3D10Device>(device_d3d10_orig.get()))
		{
			assert(device_d3d10_orig == device_d3d10->_orig);
			device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(device_d3d10));
			return 10;
		}
	}
	if (com_ptr<ID3D11Device> device_d3d11_orig;
		SUCCEEDED(device->QueryInterface(&device_d3d11_orig)))
	{
		if (com_ptr<D3D11Device> device_d3d11 = get_private_pointer_d3dx<D3D11Device>(device_d3d11_orig.get()))
		{
			assert(device_d3d11_orig == device_d3d11->_orig);
			device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(device_d3d11));
			return 11;
		}
	}

	// Did not find a proxy device
	return 0;
}

template <typename T>
static void init_swapchain_proxy(T *&swapchain, UINT direct3d_version, const com_ptr<IUnknown> &device_proxy, DXGI_USAGE usage)
{
	DXGISwapChain *swapchain_proxy = nullptr;

	if ((usage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		reshade::log::message(reshade::log::level::warning, "Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.");
	}
	else if (direct3d_version == 10)
	{
		const com_ptr<D3D10Device> &device = reinterpret_cast<const com_ptr<D3D10Device> &>(device_proxy);

		swapchain_proxy = new DXGISwapChain(device.get(), swapchain); // Overwrite returned swap chain with proxy swap chain
	}
	else if (direct3d_version == 11)
	{
		const com_ptr<D3D11Device> &device = reinterpret_cast<const com_ptr<D3D11Device> &>(device_proxy);

		swapchain_proxy = new DXGISwapChain(device.get(), swapchain);
	}
	else if (direct3d_version == 12)
	{
		if (com_ptr<IDXGISwapChain3> swapchain3;
			SUCCEEDED(swapchain->QueryInterface(&swapchain3)))
		{
			const com_ptr<D3D12CommandQueue> &command_queue = reinterpret_cast<const com_ptr<D3D12CommandQueue> &>(device_proxy);

			swapchain_proxy = new DXGISwapChain(command_queue.get(), swapchain3.get());
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Skipping swap chain because it is missing support for the IDXGISwapChain3 interface.");
		}
	}
	else
	{
		reshade::log::message(reshade::log::level::warning, "Skipping swap chain because it was created without a proxy Direct3D device.");
	}

	if (swapchain_proxy != nullptr)
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::debug, "Returning IDXGISwapChain%hu object %p (%p).", swapchain_proxy->_interface_version, swapchain_proxy, swapchain_proxy->_orig);
#endif
		swapchain = swapchain_proxy;
	}
}

HRESULT STDMETHODCALLTYPE IDXGIFactory_CreateSwapChain(IDXGIFactory *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
	const auto trampoline = reshade::hooks::call(IDXGIFactory_CreateSwapChain, reshade::hooks::vtable_from_instance(pFactory) + 10);

	if (g_in_dxgi_runtime)
		return trampoline(pFactory, pDevice, pDesc, ppSwapChain);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDXGIFactory::CreateSwapChain(this = %p, pDevice = %p, pDesc = %p, ppSwapChain = %p) ...",
		pFactory, pDevice, pDesc, ppSwapChain);

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	DXGI_SWAP_CHAIN_DESC desc = *pDesc;
	dump_and_modify_swapchain_desc(desc);

	com_ptr<IUnknown> device_proxy;
	const UINT direct3d_version = query_device(pDevice, device_proxy);

	g_in_dxgi_runtime = true;
	const HRESULT hr = trampoline(pFactory, pDevice, &desc, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "IDXGIFactory::CreateSwapChain failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage);

	return hr;
}

HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForHwnd(IDXGIFactory2 *pFactory, IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	const auto trampoline = reshade::hooks::call(IDXGIFactory2_CreateSwapChainForHwnd, reshade::hooks::vtable_from_instance(pFactory) + 15);

	if (g_in_dxgi_runtime)
		return trampoline(pFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDXGIFactory2::CreateSwapChainForHwnd(this = %p, pDevice = %p, hWnd = %p, pDesc = %p, pFullscreenDesc = %p, pRestrictToOutput = %p, ppSwapChain = ) ...",
		pFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc = {};
	if (pFullscreenDesc != nullptr)
		fullscreen_desc = *pFullscreenDesc;
	else
		fullscreen_desc.Windowed = TRUE;
	dump_and_modify_swapchain_desc(desc, &fullscreen_desc, hWnd);

	com_ptr<IUnknown> device_proxy;
	const UINT direct3d_version = query_device(pDevice, device_proxy);

	g_in_dxgi_runtime = true;
	const HRESULT hr = trampoline(pFactory, pDevice, hWnd, &desc, fullscreen_desc.Windowed ? nullptr : &fullscreen_desc, pRestrictToOutput, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "IDXGIFactory2::CreateSwapChainForHwnd failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage);

	return hr;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForCoreWindow(IDXGIFactory2 *pFactory, IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	const auto trampoline = reshade::hooks::call(IDXGIFactory2_CreateSwapChainForCoreWindow, reshade::hooks::vtable_from_instance(pFactory) + 16);

	if (g_in_dxgi_runtime)
		return trampoline(pFactory, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDXGIFactory2::CreateSwapChainForCoreWindow(this = %p, pDevice = %p, pWindow = %p, pDesc = %p, pRestrictToOutput = %p, ppSwapChain = ) ...",
		pFactory, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
	// UWP applications cannot be set into fullscreen mode
	dump_and_modify_swapchain_desc(desc);

	com_ptr<IUnknown> device_proxy;
	const UINT direct3d_version = query_device(pDevice, device_proxy);

	g_in_dxgi_runtime = true;
	const HRESULT hr = trampoline(pFactory, pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "IDXGIFactory2::CreateSwapChainForCoreWindow failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage);

	return hr;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForComposition(IDXGIFactory2 *pFactory, IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	const auto trampoline = reshade::hooks::call(IDXGIFactory2_CreateSwapChainForComposition, reshade::hooks::vtable_from_instance(pFactory) + 24);

	if (g_in_dxgi_runtime)
		return trampoline(pFactory, pDevice, pDesc, pRestrictToOutput, ppSwapChain);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDXGIFactory2::CreateSwapChainForComposition(this = %p, pDevice = %p, pDesc = %p, pRestrictToOutput = %p, ppSwapChain = ) ...",
		pFactory, pDevice, pDesc, pRestrictToOutput, ppSwapChain);

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
	// Composition swap chains cannot be set into fullscreen mode
	dump_and_modify_swapchain_desc(desc);

	com_ptr<IUnknown> device_proxy;
	const UINT direct3d_version = query_device(pDevice, device_proxy);

	g_in_dxgi_runtime = true;
	const HRESULT hr = trampoline(pFactory, pDevice, &desc, pRestrictToOutput, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "IDXGIFactory2::CreateSwapChainForComposition failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage);

	return hr;
}

extern "C" HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory)
{
#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::info, "Redirecting CreateDXGIFactory(riid = %s, ppFactory = %p) ...", reshade::log::iid_to_string(riid).c_str(), ppFactory);
	reshade::log::message(reshade::log::level::info, "> Passing on to CreateDXGIFactory1:");
#endif

	// DXGI 1.1 should always be available, so to simplify code just call 'CreateDXGIFactory' which is otherwise identical
	return CreateDXGIFactory1(riid, ppFactory);
}
extern "C" HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
	// Do NOT skip in case this is called internally for D3D10/D3D11 (otherwise the 'IDXGIFactory::CreateSwapChain' call in 'D3D10/11CreateDeviceAndSwapChain' will not be redirected)

	reshade::log::message(reshade::log::level::info, "Redirecting CreateDXGIFactory1(riid = %s, ppFactory = %p) ...", reshade::log::iid_to_string(riid).c_str(), ppFactory);

	const HRESULT hr = reshade::hooks::call(CreateDXGIFactory1)(riid, ppFactory);
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "CreateDXGIFactory1 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);

	reshade::hooks::install("IDXGIFactory::CreateSwapChain", reshade::hooks::vtable_from_instance(factory), 10, reinterpret_cast<reshade::hook::address>(&IDXGIFactory_CreateSwapChain));

	// Check for DXGI 1.2 support and install IDXGIFactory2 hooks if it exists
	if (com_ptr<IDXGIFactory2> factory2;
		SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForHwnd", reshade::hooks::vtable_from_instance(factory2.get()), 15, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForHwnd));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForCoreWindow", reshade::hooks::vtable_from_instance(factory2.get()), 16, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForComposition", reshade::hooks::vtable_from_instance(factory2.get()), 24, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForComposition));
	}

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning IDXGIFactory object %p.", factory);
#endif
	return hr;
}
extern "C" HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void **ppFactory)
{
	// Possible interfaces:
	//   IDXGIFactory  {7B7166EC-21C7-44AE-B21A-C9AE321AE369}
	//   IDXGIFactory1 {770AAE78-F26F-4DBA-A829-253C83D1B387}
	//   IDXGIFactory2 {50C83A1C-E072-4C48-87B0-3630FA36A6D0}
	//   IDXGIFactory3 {25483823-CD46-4C7D-86CA-47AA95B837BD}
	//   IDXGIFactory4 {1BC6EA02-EF36-464F-BF0C-21CA39E5168A}
	//   IDXGIFactory5 {7632E1f5-EE65-4DCA-87FD-84CD75F8838D}
	//   IDXGIFactory6 {C1B6694F-FF09-44A9-B03C-77900A0A1D17}
	//   IDXGIFactory7 {A4966EED-76DB-44DA-84C1-EE9A7AFB20A8}

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting CreateDXGIFactory2(Flags = %#x, riid = %s, ppFactory = %p) ...",
		Flags, reshade::log::iid_to_string(riid).c_str(), ppFactory);

	const auto trampoline = is_windows7() ? nullptr : reshade::hooks::call(CreateDXGIFactory2);

	// CreateDXGIFactory2 is not available on Windows 7, so fall back to CreateDXGIFactory1 if the application calls it
	// This needs to happen because some applications only check if CreateDXGIFactory2 exists, which is always the case if they load ReShade, to decide whether to call it or CreateDXGIFactory1
	if (trampoline == nullptr)
	{
		reshade::log::message(reshade::log::level::info, "> Passing on to CreateDXGIFactory1:");

		return CreateDXGIFactory1(riid, ppFactory);
	}

	// It is crucial that ReShade hooks this after the Steam overlay already hooked it, so that ReShade is called first and the Steam overlay is called through the trampoline below
	// This is because the Steam overlay only hooks the swap chain creation functions when the vtable entries for them still point to the original functions, it will no longer do so once ReShade replaced them ("... points to another module, skipping hooks" in GameOverlayRenderer.log)

	// Upgrade factory interface to the highest available at creation, to ensure the virtual function table cannot be replaced afterwards during 'QueryInterface'
	static constexpr IID iid_lookup[] = {
		__uuidof(IDXGIFactory),
		__uuidof(IDXGIFactory1),
		__uuidof(IDXGIFactory2),
		__uuidof(IDXGIFactory3),
		__uuidof(IDXGIFactory4),
		__uuidof(IDXGIFactory5),
		__uuidof(IDXGIFactory6),
		__uuidof(IDXGIFactory7),
	};
	HRESULT hr = E_NOINTERFACE;
	if (std::find(std::begin(iid_lookup), std::end(iid_lookup), riid) == std::end(iid_lookup))
	{
		hr = trampoline(Flags, riid, ppFactory); // Fall back in case of unknown interface version
	}
	else
	{
		for (auto it = std::rbegin(iid_lookup); it != std::rend(iid_lookup); ++it)
			if (SUCCEEDED(hr = trampoline(Flags, *it, ppFactory)) || *it == riid)
				break;
	}

	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "CreateDXGIFactory2 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);

	reshade::hooks::install("IDXGIFactory::CreateSwapChain", reshade::hooks::vtable_from_instance(factory), 10, reinterpret_cast<reshade::hook::address>(&IDXGIFactory_CreateSwapChain));

	if (com_ptr<IDXGIFactory2> factory2;
		SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForHwnd", reshade::hooks::vtable_from_instance(factory2.get()), 15, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForHwnd));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForCoreWindow", reshade::hooks::vtable_from_instance(factory2.get()), 16, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForComposition", reshade::hooks::vtable_from_instance(factory2.get()), 24, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForComposition));
	}

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning IDXGIFactory object %p.", factory);
#endif
	return hr;
}

extern "C" HRESULT WINAPI DXGIGetDebugInterface1(UINT Flags, REFIID riid, void **pDebug)
{
	const auto trampoline = reshade::hooks::call(DXGIGetDebugInterface1);

	// DXGIGetDebugInterface1 is not available on Windows 7, so act as if Windows SDK is not installed
	if (trampoline == nullptr)
		return E_NOINTERFACE;

	return trampoline(Flags, riid, pDebug);
}

extern "C" HRESULT WINAPI DXGIDeclareAdapterRemovalSupport()
{
	const auto trampoline = reshade::hooks::call(DXGIDeclareAdapterRemovalSupport);

	// DXGIDeclareAdapterRemovalSupport is supported on Windows 10 version 1803 and up, silently ignore on older systems
	if (trampoline == nullptr)
		return S_OK;

	return trampoline();
}
