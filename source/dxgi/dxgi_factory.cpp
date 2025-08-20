/*
 * Copyright (C) 2025 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dxgi_factory.hpp"
#include "dxgi_adapter.hpp"
#include "dxgi_swapchain.hpp"
#include "d3d10/d3d10_device.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "com_utils.hpp"
#include "addon_manager.hpp"

#ifndef RESHADE_IDXGIFACTORY_VTABLE

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

bool modify_swapchain_desc(reshade::api::device_api api, DXGI_SWAP_CHAIN_DESC &internal_desc, UINT &sync_interval)
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

	const bool use_window_size = internal_desc.BufferDesc.Width == 0 || internal_desc.BufferDesc.Height == 0;
	if (use_window_size)
	{
		RECT window_rect = {};
		// If either the width or height are zero, then the swapchain will be sized to the current window/screen size (which doesn't mean it will follow later window size changes).
		// Note that this could return 0 in edges cases (especially if fullscreen mode is enabled), in that case, the swapchain creation or resize might fail,
		// so ideally we should fall back on the screen resolution.
		GetClientRect(internal_desc.OutputWindow, &window_rect);
		desc.back_buffer.texture.width = window_rect.right;
		desc.back_buffer.texture.height = window_rect.bottom;
		assert(desc.back_buffer.texture.width != 0 && desc.back_buffer.texture.height != 0);
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
	assert(desc.back_buffer_count != 0);
	desc.present_mode = static_cast<uint32_t>(internal_desc.SwapEffect);
	desc.present_flags = internal_desc.Flags;
	desc.fullscreen_state = internal_desc.Windowed == FALSE;
	desc.fullscreen_refresh_rate = rational_to_floating_point(internal_desc.BufferDesc.RefreshRate);

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(api, desc, internal_desc.OutputWindow))
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

		assert(desc.sync_interval <= 4 || desc.sync_interval == UINT_MAX);
		sync_interval = desc.sync_interval;

		return true;
	}

	return false;
}
bool modify_swapchain_desc(reshade::api::device_api api, DXGI_SWAP_CHAIN_DESC1 &internal_desc, UINT &sync_interval, DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fullscreen_desc, HWND window)
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

	const bool use_window_size = internal_desc.Width == 0 || internal_desc.Height == 0;
	if (window != nullptr && use_window_size)
	{
		RECT window_rect = {};
		// If either the width or height are zero, then the swapchain will be sized to the current window/screen size (which doesn't mean it will follow later window size changes).
		// Note that this could return 0 in edges cases (especially if fullscreen mode is enabled), in that case, the swapchain creation or resize might fail,
		// so ideally we should fall back on the screen resolution.
		GetClientRect(window, &window_rect);
		desc.back_buffer.texture.width = window_rect.right;
		desc.back_buffer.texture.height = window_rect.bottom;
		assert(desc.back_buffer.texture.width != 0 && desc.back_buffer.texture.height != 0);
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
	assert(desc.back_buffer_count != 0);
	desc.present_mode = static_cast<uint32_t>(internal_desc.SwapEffect);
	desc.present_flags = internal_desc.Flags;

	if (fullscreen_desc != nullptr)
	{
		desc.fullscreen_state = fullscreen_desc->Windowed == FALSE;
		desc.fullscreen_refresh_rate = rational_to_floating_point(fullscreen_desc->RefreshRate);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(api, desc, window))
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

		assert(desc.sync_interval <= 4 || desc.sync_interval == UINT_MAX);
		sync_interval = desc.sync_interval;

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

static bool dump_and_modify_swapchain_desc([[maybe_unused]] reshade::api::device_api api, DXGI_SWAP_CHAIN_DESC &desc, [[maybe_unused]] UINT &sync_interval)
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
	return modify_swapchain_desc(api, desc, sync_interval);
#else
	return false;
#endif
}
static bool dump_and_modify_swapchain_desc([[maybe_unused]] reshade::api::device_api api, DXGI_SWAP_CHAIN_DESC1 &desc, [[maybe_unused]] UINT &sync_interval, DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fullscreen_desc = nullptr, [[maybe_unused]] HWND window = nullptr)
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
	return modify_swapchain_desc(api, desc, sync_interval, fullscreen_desc, window);
#else
	return false;
#endif
}

reshade::api::device_api query_device(IUnknown *&device, com_ptr<IUnknown> &device_proxy)
{
	if (com_ptr<D3D10Device> device_d3d10;
		SUCCEEDED(device->QueryInterface(&device_d3d10)))
	{
		device = device_d3d10->_orig; // Set device pointer back to original object so that the swap chain creation functions work as expected
		device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(device_d3d10));
		return reshade::api::device_api::d3d10;
	}
	if (com_ptr<D3D11Device> device_d3d11;
		SUCCEEDED(device->QueryInterface(&device_d3d11)))
	{
		device = device_d3d11->_orig;
		device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(device_d3d11));
		return reshade::api::device_api::d3d11;
	}
	if (com_ptr<D3D12CommandQueue> command_queue_d3d12;
		SUCCEEDED(device->QueryInterface(&command_queue_d3d12)))
	{
		device = command_queue_d3d12->_orig;
		device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(command_queue_d3d12));
		return reshade::api::device_api::d3d12;
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
			return reshade::api::device_api::d3d10;
		}
	}
	if (com_ptr<ID3D11Device> device_d3d11_orig;
		SUCCEEDED(device->QueryInterface(&device_d3d11_orig)))
	{
		if (com_ptr<D3D11Device> device_d3d11 = get_private_pointer_d3dx<D3D11Device>(device_d3d11_orig.get()))
		{
			assert(device_d3d11_orig == device_d3d11->_orig);
			device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(device_d3d11));
			return reshade::api::device_api::d3d11;
		}
	}

	// Did not find a proxy device
	return static_cast<reshade::api::device_api>(0);
}

template <typename T>
static void init_swapchain_proxy(T *&swapchain, reshade::api::device_api direct3d_version, const com_ptr<IUnknown> &device_proxy, DXGI_USAGE usage, [[maybe_unused]] UINT sync_interval, [[maybe_unused]] const DXGI_SWAP_CHAIN_DESC &orig_desc, [[maybe_unused]] bool desc_modified)
{
	DXGISwapChain *swapchain_proxy = nullptr;

	if ((usage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		reshade::log::message(reshade::log::level::warning, "Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.");
	}
	else if (direct3d_version == reshade::api::device_api::d3d10)
	{
		const com_ptr<D3D10Device> &device = reinterpret_cast<const com_ptr<D3D10Device> &>(device_proxy);

		swapchain_proxy = new DXGISwapChain(device.get(), swapchain); // Overwrite returned swap chain with proxy swap chain
	}
	else if (direct3d_version == reshade::api::device_api::d3d11)
	{
		const com_ptr<D3D11Device> &device = reinterpret_cast<const com_ptr<D3D11Device> &>(device_proxy);

		swapchain_proxy = new DXGISwapChain(device.get(), swapchain);
	}
	else if (direct3d_version == reshade::api::device_api::d3d12)
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
#if RESHADE_ADDON
		swapchain_proxy->_orig_desc = orig_desc;
		// Fill up the size from the swap chain if it was inferred from the window size
		if (orig_desc.BufferDesc.Width == 0 || orig_desc.BufferDesc.Height == 0)
		{
			DXGI_SWAP_CHAIN_DESC desc = {};
			swapchain->GetDesc(&desc);
			swapchain_proxy->_orig_desc.BufferDesc.Width = desc.BufferDesc.Width;
			swapchain_proxy->_orig_desc.BufferDesc.Height = desc.BufferDesc.Height;
		}
		swapchain_proxy->_desc_modified = desc_modified;
		swapchain_proxy->_sync_interval = sync_interval;
#endif
		swapchain = swapchain_proxy;
	}
}

#endif // RESHADE_IDXGIFACTORY_VTABLE

DXGIFactory::DXGIFactory(IDXGIFactory *original) :
	_orig(original),
	_interface_version(0)
{
	assert(_orig != nullptr);

	DXGIFactory *const factory_proxy = this;
	_orig->SetPrivateData(__uuidof(DXGIFactory), sizeof(factory_proxy), &factory_proxy);
}

DXGIFactory::~DXGIFactory()
{
	_orig->SetPrivateData(__uuidof(DXGIFactory), 0, nullptr); // Clear private data	
}

bool DXGIFactory::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDXGIObject))
		return true;

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

	for (unsigned short version = 0; version < ARRAYSIZE(iid_lookup); ++version)
	{
		if (riid != iid_lookup[version])
			continue;

		if (version > _interface_version)
		{
			IUnknown *new_interface = nullptr;
			if (FAILED(_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface))))
				return false;
#if RESHADE_VERBOSE_LOG
			reshade::log::message(reshade::log::level::debug, "Upgrading IDXGIFactory%hu object %p to IDXGIFactory%hu.", _interface_version, this, version);
#endif
			_orig->Release();
			_orig = static_cast<IDXGIFactory *>(new_interface);
			_interface_version = version;
		}

		return true;
	}

	return false;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE DXGIFactory::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE DXGIFactory::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
	{
		_orig->Release();
		return ref;
	}

	const auto orig = _orig;
	const auto interface_version = _interface_version;
#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Destroying IDXGIFactory%hu object %p (%p).", interface_version, this, orig);
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		reshade::log::message(reshade::log::level::warning, "Reference count for IDXGIFactory%hu object %p (%p) is inconsistent (%lu).", interface_version, this, orig, ref_orig);
	return 0;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
{
	return _orig->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::GetParent(REFIID riid, void **ppParent)
{
	return _orig->GetParent(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapters(UINT Adapter, IDXGIAdapter **ppAdapter)
{
	HRESULT hr = _orig->EnumAdapters(Adapter, ppAdapter);
	if (hr == S_OK)
	{
		IDXGIAdapter *const adapter = *ppAdapter;
		auto adapter_proxy = get_private_pointer_d3dx<DXGIAdapter>(adapter);
		if (adapter_proxy == nullptr)
		{
			adapter_proxy = new DXGIAdapter(adapter);
		}
		else
		{
			adapter_proxy->_ref++;
		}
		*ppAdapter = adapter_proxy;
	}
	return hr;
}
HRESULT STDMETHODCALLTYPE DXGIFactory::MakeWindowAssociation(HWND WindowHandle, UINT Flags)
{
	return _orig->MakeWindowAssociation(WindowHandle, Flags);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::GetWindowAssociation(HWND *pWindowHandle)
{
	return _orig->GetWindowAssociation(pWindowHandle);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChain(IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
#ifndef RESHADE_IDXGIFACTORY_VTABLE
	if (g_in_dxgi_runtime)
		return _orig->CreateSwapChain(pDevice, pDesc, ppSwapChain);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDXGIFactory::CreateSwapChain(this = %p, pDevice = %p, pDesc = %p, ppSwapChain = %p) ...",
		_orig, pDevice, pDesc, ppSwapChain);

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	com_ptr<IUnknown> device_proxy;
	const reshade::api::device_api direct3d_version = query_device(pDevice, device_proxy);

	DXGI_SWAP_CHAIN_DESC desc = *pDesc;
	UINT sync_interval = UINT_MAX;
	const bool modified = dump_and_modify_swapchain_desc(direct3d_version, desc, sync_interval);

	g_in_dxgi_runtime = true;
	const HRESULT hr = _orig->CreateSwapChain(pDevice, &desc, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "IDXGIFactory::CreateSwapChain failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage, sync_interval, *pDesc, modified);

	return hr;
#else
	return IDXGIFactory_CreateSwapChain_Impl(this, pDevice, pDesc, ppSwapChain,
		[](IDXGIFactory *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain) -> HRESULT {
			return static_cast<DXGIFactory *>(pFactory)->_orig->CreateSwapChain(pDevice, pDesc, ppSwapChain);
		});
#endif
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSoftwareAdapter(HMODULE Module, IDXGIAdapter **ppAdapter)
{
	HRESULT hr = _orig->CreateSoftwareAdapter(Module, ppAdapter);
	if (hr == S_OK)
	{
		IDXGIAdapter *const adapter = *ppAdapter;
		auto adapter_proxy = get_private_pointer_d3dx<DXGIAdapter>(adapter);
		if (adapter_proxy == nullptr)
		{
			adapter_proxy = new DXGIAdapter(adapter);
		}
		else
		{
			adapter_proxy->_ref++;
		}
		*ppAdapter = adapter_proxy;
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapters1(UINT Adapter, IDXGIAdapter1 **ppAdapter)
{
	assert(_interface_version >= 1);
	HRESULT hr = static_cast<IDXGIFactory1 *>(_orig)->EnumAdapters1(Adapter, ppAdapter);
	if (hr == S_OK)
	{
		IDXGIAdapter1 *const adapter = *ppAdapter;

		auto adapter_proxy = get_private_pointer_d3dx<DXGIAdapter>(adapter);
		if (adapter_proxy == nullptr)
		{
			adapter_proxy = new DXGIAdapter(adapter);
		}
		else
		{
			assert(adapter_proxy->check_and_upgrade_interface(__uuidof(IDXGIAdapter1)));
			adapter_proxy->_ref++;
		}

#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::debug, "DXGIFactory::EnumAdapters1 returning IDXGIAdapter%hu object %p (%p).", adapter_proxy->_interface_version, adapter_proxy, adapter_proxy->_orig);
#endif
		*ppAdapter = adapter_proxy;
	}
	return hr;
}
BOOL    STDMETHODCALLTYPE DXGIFactory::IsCurrent()
{
	assert(_interface_version >= 1);
	return static_cast<IDXGIFactory1 *>(_orig)->IsCurrent();
}

BOOL    STDMETHODCALLTYPE DXGIFactory::IsWindowedStereoEnabled(void)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->IsWindowedStereoEnabled();
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChainForHwnd(IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	assert(_interface_version >= 2);
#ifndef RESHADE_IDXGIFACTORY_VTABLE
	if (g_in_dxgi_runtime)
		return static_cast<IDXGIFactory2 *>(_orig)->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDXGIFactory2::CreateSwapChainForHwnd(this = %p, pDevice = %p, hWnd = %p, pDesc = %p, pFullscreenDesc = %p, pRestrictToOutput = %p, ppSwapChain = ) ...",
		_orig, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	com_ptr<IUnknown> device_proxy;
	const reshade::api::device_api direct3d_version = query_device(pDevice, device_proxy);

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
	UINT sync_interval = UINT_MAX;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc = {};
	if (pFullscreenDesc != nullptr)
		fullscreen_desc = *pFullscreenDesc;
	else
		fullscreen_desc.Windowed = TRUE;
	const bool modified = dump_and_modify_swapchain_desc(direct3d_version, desc, sync_interval, &fullscreen_desc, hWnd);

	// Space Engineers 2 does not set any usage flags, change default to at least include render target output support
	if (0 == desc.BufferUsage && direct3d_version == reshade::api::device_api::d3d12)
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	g_in_dxgi_runtime = true;
	const HRESULT hr = static_cast<IDXGIFactory2 *>(_orig)->CreateSwapChainForHwnd(pDevice, hWnd, &desc, fullscreen_desc.Windowed ? nullptr : &fullscreen_desc, pRestrictToOutput, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "IDXGIFactory2::CreateSwapChainForHwnd failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage, sync_interval,
		DXGI_SWAP_CHAIN_DESC {
			{ pDesc->Width, pDesc->Height, pFullscreenDesc != nullptr ? pFullscreenDesc->RefreshRate : DXGI_RATIONAL {}, pDesc->Format, DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED, pDesc->Scaling == DXGI_SCALING_ASPECT_RATIO_STRETCH ? DXGI_MODE_SCALING_CENTERED : DXGI_MODE_SCALING_STRETCHED },
			pDesc->SampleDesc,
			pDesc->BufferUsage,
			pDesc->BufferCount,
			hWnd,
			pFullscreenDesc == nullptr || pFullscreenDesc->Windowed,
			pDesc->SwapEffect,
			pDesc->Flags
		},
		modified);
	return hr;
#else
	return IDXGIFactory2_CreateSwapChainForHwnd_Impl(this, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain,
		[](IDXGIFactory2 *pFactory, IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) -> HRESULT {
			return static_cast<IDXGIFactory2 *>(static_cast<DXGIFactory *>(pFactory)->_orig)->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	});
#endif
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChainForCoreWindow(IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	assert(_interface_version >= 2);
#ifndef RESHADE_IDXGIFACTORY_VTABLE
	if (g_in_dxgi_runtime)
		return static_cast<IDXGIFactory2 *>(_orig)->CreateSwapChainForCoreWindow(pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDXGIFactory2::CreateSwapChainForCoreWindow(this = %p, pDevice = %p, pWindow = %p, pDesc = %p, pRestrictToOutput = %p, ppSwapChain = ) ...",
		_orig, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	com_ptr<IUnknown> device_proxy;
	const reshade::api::device_api direct3d_version = query_device(pDevice, device_proxy);

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
	UINT sync_interval = UINT_MAX;
	// UWP applications cannot be set into fullscreen mode
	const bool modified = dump_and_modify_swapchain_desc(direct3d_version, desc, sync_interval);

	g_in_dxgi_runtime = true;
	const HRESULT hr = static_cast<IDXGIFactory2 *>(_orig)->CreateSwapChainForCoreWindow(pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "IDXGIFactory2::CreateSwapChainForCoreWindow failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage, sync_interval,
		DXGI_SWAP_CHAIN_DESC {
			{ pDesc->Width, pDesc->Height, DXGI_RATIONAL {}, pDesc->Format, DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED, pDesc->Scaling == DXGI_SCALING_ASPECT_RATIO_STRETCH ? DXGI_MODE_SCALING_CENTERED : DXGI_MODE_SCALING_STRETCHED },
			pDesc->SampleDesc,
			pDesc->BufferUsage,
			pDesc->BufferCount,
			nullptr,
			TRUE,
			pDesc->SwapEffect,
			pDesc->Flags
		},
		modified);
	return hr;
#else
	return IDXGIFactory2_CreateSwapChainForCoreWindow_Impl(this, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain,
		[](IDXGIFactory2 *pFactory, IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) -> HRESULT {
			return static_cast<IDXGIFactory2 *>(static_cast<DXGIFactory *>(pFactory)->_orig)->CreateSwapChainForCoreWindow(pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
	});
#endif
}
HRESULT STDMETHODCALLTYPE DXGIFactory::GetSharedResourceAdapterLuid(HANDLE hResource, LUID *pLuid)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->GetSharedResourceAdapterLuid(hResource, pLuid);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterStereoStatusWindow(HWND WindowHandle, UINT wMsg, DWORD *pdwCookie)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->RegisterStereoStatusWindow(WindowHandle, wMsg, pdwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterStereoStatusEvent(HANDLE hEvent, DWORD *pdwCookie)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->RegisterStereoStatusEvent(hEvent, pdwCookie);
}
void    STDMETHODCALLTYPE DXGIFactory::UnregisterStereoStatus(DWORD dwCookie)
{
	assert(_interface_version >= 2);
	static_cast<IDXGIFactory2 *>(_orig)->UnregisterStereoStatus(dwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterOcclusionStatusWindow(HWND WindowHandle, UINT wMsg, DWORD *pdwCookie)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterOcclusionStatusEvent(HANDLE hEvent, DWORD *pdwCookie)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
}
void    STDMETHODCALLTYPE DXGIFactory::UnregisterOcclusionStatus(DWORD dwCookie)
{
	assert(_interface_version >= 2);
	static_cast<IDXGIFactory2 *>(_orig)->UnregisterOcclusionStatus(dwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChainForComposition(IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	assert(_interface_version >= 2);
#ifndef RESHADE_IDXGIFACTORY_VTABLE
	if (g_in_dxgi_runtime)
		return static_cast<IDXGIFactory2 *>(_orig)->CreateSwapChainForComposition(pDevice, pDesc, pRestrictToOutput, ppSwapChain);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDXGIFactory2::CreateSwapChainForComposition(this = %p, pDevice = %p, pDesc = %p, pRestrictToOutput = %p, ppSwapChain = ) ...",
		_orig, pDevice, pDesc, pRestrictToOutput, ppSwapChain);

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	com_ptr<IUnknown> device_proxy;
	const reshade::api::device_api direct3d_version = query_device(pDevice, device_proxy);

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
	UINT sync_interval = UINT_MAX;
	// Composition swap chains cannot be set into fullscreen mode
	const bool modified = dump_and_modify_swapchain_desc(direct3d_version, desc, sync_interval);

	g_in_dxgi_runtime = true;
	const HRESULT hr = static_cast<IDXGIFactory2 *>(_orig)->CreateSwapChainForComposition(pDevice, &desc, pRestrictToOutput, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "IDXGIFactory2::CreateSwapChainForComposition failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage, sync_interval,
		DXGI_SWAP_CHAIN_DESC {
			{ pDesc->Width, pDesc->Height, DXGI_RATIONAL {}, pDesc->Format, DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED, pDesc->Scaling == DXGI_SCALING_ASPECT_RATIO_STRETCH ? DXGI_MODE_SCALING_CENTERED : DXGI_MODE_SCALING_STRETCHED },
			pDesc->SampleDesc,
			pDesc->BufferUsage,
			pDesc->BufferCount,
			nullptr,
			TRUE,
			pDesc->SwapEffect,
			pDesc->Flags
		},
		modified);
	return hr;
#else
	return IDXGIFactory2_CreateSwapChainForComposition_Impl(this, pDevice, pDesc, pRestrictToOutput, ppSwapChain,
		[](IDXGIFactory2 *pFactory, IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) -> HRESULT {
			return static_cast<IDXGIFactory2 *>(static_cast<DXGIFactory *>(pFactory)->_orig)->CreateSwapChainForComposition(pDevice, pDesc, pRestrictToOutput, ppSwapChain);
	});
#endif
}

UINT    STDMETHODCALLTYPE DXGIFactory::GetCreationFlags()
{
	assert(_interface_version >= 3);
	return static_cast<IDXGIFactory3 *>(_orig)->GetCreationFlags();
}

HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapterByLuid(LUID AdapterLuid, REFIID riid, void **ppvAdapter)
{
	assert(_interface_version >= 4);
	HRESULT hr = static_cast<IDXGIFactory4 *>(_orig)->EnumAdapterByLuid(AdapterLuid, riid, ppvAdapter);
	if (hr == S_OK)
	{
		IUnknown *const adapter_unknown = static_cast<IUnknown *>(*ppvAdapter);
		if (com_ptr<IDXGIAdapter> adapter;
			SUCCEEDED(adapter_unknown->QueryInterface(&adapter)))
		{
			auto adapter_proxy = get_private_pointer_d3dx<DXGIAdapter>(adapter.get());
			if (adapter_proxy == nullptr)
			{
				adapter_proxy = new DXGIAdapter(adapter.get());
			}
			else
			{
				adapter_proxy->_ref++;
			}

			if (adapter_proxy->check_and_upgrade_interface(riid))
			{
#if RESHADE_VERBOSE_LOG
				reshade::log::message(
					reshade::log::level::debug,
					"DXGIFactory::EnumAdapterByLuid returning IDXGIAdapter%hu object %p (%p) with interface %s.",
					adapter_proxy->_interface_version, adapter_proxy, adapter_proxy->_orig, reshade::log::iid_to_string(riid).c_str());
#endif

				*ppvAdapter = adapter_proxy;
			}
			else // Do not hook object if we do not support the requested interface
			{
				reshade::log::message(reshade::log::level::warning, "Unknown interface %s in DXGIFactory::EnumAdapterByLuid.", reshade::log::iid_to_string(riid).c_str());

				delete adapter_proxy; // Delete instead of release to keep reference count untouched
			}
		}
	}
	return hr;
}
HRESULT STDMETHODCALLTYPE DXGIFactory::EnumWarpAdapter(REFIID riid, void **ppvAdapter)
{
	assert(_interface_version >= 4);
	HRESULT hr = static_cast<IDXGIFactory4 *>(_orig)->EnumWarpAdapter(riid, ppvAdapter);
	if (hr == S_OK)
	{
		IUnknown *const adapter_unknown = static_cast<IUnknown *>(*ppvAdapter);
		if (com_ptr<IDXGIAdapter> adapter;
			SUCCEEDED(adapter_unknown->QueryInterface(&adapter)))
		{
			auto adapter_proxy = get_private_pointer_d3dx<DXGIAdapter>(adapter.get());
			if (adapter_proxy == nullptr)
			{
				adapter_proxy = new DXGIAdapter(adapter.get());
			}
			else
			{
				adapter_proxy->_ref++;
			}

			if (adapter_proxy->check_and_upgrade_interface(riid))
			{
#if RESHADE_VERBOSE_LOG
				reshade::log::message(
					reshade::log::level::debug,
					"DXGIFactory::EnumWarpAdapter returning IDXGIAdapter%hu object %p (%p).",
					adapter_proxy->_interface_version, adapter_proxy, adapter_proxy->_orig);
#endif

				*ppvAdapter = adapter_proxy;
			}
			else // Do not hook object if we do not support the requested interface
			{
				reshade::log::message(reshade::log::level::warning, "Unknown interface %s in DXGIFactory::EnumWarpAdapter.", reshade::log::iid_to_string(riid).c_str());

				delete adapter_proxy; // Delete instead of release to keep reference count untouched
			}
		}
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::CheckFeatureSupport(DXGI_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize)
{
	assert(_interface_version >= 5);
	return static_cast<IDXGIFactory5 *>(_orig)->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}

HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapterByGpuPreference(UINT Adapter, DXGI_GPU_PREFERENCE GpuPreference, REFIID riid, void **ppvAdapter)
{
	assert(_interface_version >= 6);
	HRESULT hr = static_cast<IDXGIFactory6 *>(_orig)->EnumAdapterByGpuPreference(Adapter, GpuPreference, riid, ppvAdapter);
	if (hr == S_OK)
	{
		IUnknown *const adapter_unknown = static_cast<IUnknown *>(*ppvAdapter);
		if (com_ptr<IDXGIAdapter> adapter;
			SUCCEEDED(adapter_unknown->QueryInterface(&adapter)))
		{
			auto adapter_proxy = get_private_pointer_d3dx<DXGIAdapter>(adapter.get());
			if (adapter_proxy == nullptr)
			{
				adapter_proxy = new DXGIAdapter(adapter.get());
			}
			else
			{
				adapter_proxy->_ref++;
			}

			if (adapter_proxy->check_and_upgrade_interface(riid))
			{
#if RESHADE_VERBOSE_LOG
				reshade::log::message(
					reshade::log::level::debug,
					"DXGIFactory::EnumAdapterByGpuPreference returning IDXGIAdapter%hu object %p (%p).",
					adapter_proxy->_interface_version, adapter_proxy, adapter_proxy->_orig);
#endif

				*ppvAdapter = adapter_proxy;
			}
			else // Do not hook object if we do not support the requested interface
			{
				reshade::log::message(reshade::log::level::warning, "Unknown interface %s in DXGIFactory::EnumAdapterByGpuPreference.", reshade::log::iid_to_string(riid).c_str());

				delete adapter_proxy; // Delete instead of release to keep reference count untouched
			}
		}
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterAdaptersChangedEvent(HANDLE hEvent, DWORD *pdwCookie)
{
	assert(_interface_version >= 7);
	return static_cast<IDXGIFactory7 *>(_orig)->RegisterAdaptersChangedEvent(hEvent, pdwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::UnregisterAdaptersChangedEvent(DWORD dwCookie)
{
	assert(_interface_version >= 7);
	return static_cast<IDXGIFactory7 *>(_orig)->UnregisterAdaptersChangedEvent(dwCookie);
}
