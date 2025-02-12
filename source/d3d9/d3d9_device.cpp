/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d9_device.hpp"
#include "d3d9_resource.hpp"
#include "d3d9_swapchain.hpp"
#include "d3d9on12_device.hpp"
#include "d3d9_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "com_utils.hpp"
#include "hook_manager.hpp"
#include "addon_manager.hpp"

using reshade::d3d9::to_handle;

extern thread_local bool g_in_d3d9_runtime;
extern thread_local bool g_in_dxgi_runtime;

extern void dump_and_modify_present_parameters(D3DPRESENT_PARAMETERS &pp, IDirect3D9 *d3d, UINT adapter_index, [[maybe_unused]] HWND focus_window);
extern void dump_and_modify_present_parameters(D3DPRESENT_PARAMETERS &pp, D3DDISPLAYMODEEX &fullscreen_desc, IDirect3D9 *d3d, UINT adapter_index, [[maybe_unused]] HWND focus_window);

const reshade::api::subresource_box *convert_rect_to_box(const RECT *rect, reshade::api::subresource_box &box)
{
	if (rect == nullptr)
		return nullptr;

	assert(rect->left >= 0 && rect->top >= 0 && rect->right >= 0 && rect->bottom >= 0);

	box.left = static_cast<uint32_t>(rect->left);
	box.top = static_cast<uint32_t>(rect->top);
	box.front = 0;
	box.right = static_cast<uint32_t>(rect->right);
	box.bottom = static_cast<uint32_t>(rect->bottom);
	box.back = 1;

	return &box;
}
const reshade::api::subresource_box *convert_rect_to_box(const POINT *point, LONG width, LONG height, reshade::api::subresource_box &box)
{
	if (point == nullptr)
		return nullptr;

	assert(point->x >= 0 && point->y >= 0 && width >= 0 && height >= 0);

	box.left = static_cast<uint32_t>(point->x);
	box.top = static_cast<uint32_t>(point->y);
	box.front = 0;
	box.right = static_cast<uint32_t>(point->x + width);
	box.bottom = static_cast<uint32_t>(point->y + height);
	box.back = 1;

	return &box;
}

Direct3DDevice9::Direct3DDevice9(IDirect3DDevice9   *original, bool use_software_rendering) :
	device_impl(original),
	_extended_interface(false),
	_use_software_rendering(use_software_rendering)
{
	assert(_orig != nullptr);

#if RESHADE_ADDON
	reshade::load_addons();
#endif

	on_init();
}
Direct3DDevice9::Direct3DDevice9(IDirect3DDevice9Ex *original, bool use_software_rendering) :
	Direct3DDevice9(static_cast<IDirect3DDevice9 *>(original), use_software_rendering)
{
	_extended_interface = true;
}
Direct3DDevice9::~Direct3DDevice9()
{
	on_reset();

#if RESHADE_ADDON
	reshade::unload_addons();
#endif
}

bool Direct3DDevice9::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DDevice9))
		return true;
	if (riid != __uuidof(IDirect3DDevice9Ex))
		return false;

	if (!_extended_interface)
	{
		IDirect3DDevice9Ex *new_interface = nullptr;
		if (FAILED(_orig->QueryInterface(IID_PPV_ARGS(&new_interface))))
			return false;
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::debug, "Upgrading IDirect3DDevice9 object %p to IDirect3DDevice9Ex.", this);
#endif
		_orig->Release();
		_orig = new_interface;
		_extended_interface = true;
	}

	return true;
}

HRESULT STDMETHODCALLTYPE Direct3DDevice9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	if (riid == __uuidof(IDirect3DDevice9On12))
	{
		if (_d3d9on12_device != nullptr)
			return _d3d9on12_device->QueryInterface(riid, ppvObj);
	}

	// Unimplemented interfaces:
	//   IDirect3DDevice9Video {26DC4561-A1EE-4ae7-96DA-118A36C0EC95}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE Direct3DDevice9::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE Direct3DDevice9::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
	{
		_orig->Release();
		return ref;
	}

	const auto orig = _orig;
	const bool extended_interface = _extended_interface;

	// Borderlands 2 is not counting references correctly and will release the device before 'IDirect3DDevice9::Reset' calls, so try and detect this and prevent deletion
	if (_resource_ref > 5)
	{
		reshade::log::message(reshade::log::level::warning, "Reference count for IDirect3DDevice9%s object %p (%p) is inconsistent! Leaking resources ...", extended_interface ? "Ex" : "", this, orig);
		_ref = 1;
		// Always return zero in case a game is trying to release all references in a while loop, which would otherwise run indefinitely
		return 0;
	}

	if (_d3d9on12_device != nullptr)
	{
		// Release the reference that was added when the D3D9on12 device was first queried in 'init_device_proxy_for_d3d9on12' (see d3d9on12.cpp)
		_d3d9on12_device->_orig->Release();
		delete _d3d9on12_device;
	}

	// Release remaining references to this device
	_implicit_swapchain->Release();

	assert(_additional_swapchains.empty());

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Destroying IDirect3DDevice9%s object %p (%p).", extended_interface ? "Ex" : "", this, orig);
#endif
	// Only call destructor and do not yet free memory before calling final 'Release' below
	// Some resources may still be alive here (e.g. because of a state block from the Steam overlay, which is released on device destruction), which will then call the resource destruction callbacks during the final 'Release' and still access this memory
	this->~Direct3DDevice9();

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		reshade::log::message(reshade::log::level::warning, "Reference count for IDirect3DDevice9%s object %p (%p) is inconsistent (%lu).", extended_interface ? "Ex" : "", this, orig, ref_orig);
	else
		operator delete(this, sizeof(Direct3DDevice9));
	return 0;
}

HRESULT STDMETHODCALLTYPE Direct3DDevice9::TestCooperativeLevel()
{
	return _orig->TestCooperativeLevel();
}
UINT    STDMETHODCALLTYPE Direct3DDevice9::GetAvailableTextureMem()
{
	return _orig->GetAvailableTextureMem();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EvictManagedResources()
{
	return _orig->EvictManagedResources();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDirect3D(IDirect3D9 **ppD3D9)
{
	return _orig->GetDirect3D(ppD3D9);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDeviceCaps(D3DCAPS9 *pCaps)
{
	return _orig->GetDeviceCaps(pCaps);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode)
{
	if (iSwapChain != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Access to multi-head swap chain at index %u is unsupported.", iSwapChain);
		return D3DERR_INVALIDCALL;
	}

	return _implicit_swapchain->GetDisplayMode(pMode);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
	return _orig->GetCreationParameters(pParameters);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 *pCursorBitmap)
{
	return _orig->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
}
void    STDMETHODCALLTYPE Direct3DDevice9::SetCursorPosition(int X, int Y, DWORD Flags)
{
	return _orig->SetCursorPosition(X, Y, Flags);
}
BOOL    STDMETHODCALLTYPE Direct3DDevice9::ShowCursor(BOOL bShow)
{
	return _orig->ShowCursor(bShow);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain9 **ppSwapChain)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDirect3DDevice9::CreateAdditionalSwapChain(this = %p, pPresentationParameters = %p, ppSwapChain = %p) ...",
		this, pPresentationParameters, ppSwapChain);

	if (pPresentationParameters == nullptr)
		return D3DERR_INVALIDCALL;

	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, _d3d.get(), _cp.AdapterOrdinal, _cp.hFocusWindow);

	const HRESULT hr = _orig->CreateAdditionalSwapChain(&pp, ppSwapChain);

	// Update output values (see https://docs.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-createadditionalswapchain)
	pPresentationParameters->BackBufferWidth = pp.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = pp.BackBufferHeight;
	pPresentationParameters->BackBufferFormat = pp.BackBufferFormat;
	pPresentationParameters->BackBufferCount = pp.BackBufferCount;

	if (SUCCEEDED(hr))
	{
		AddRef(); // Add reference which is released when the swap chain is destroyed (see 'Direct3DSwapChain9::Release')

		const auto swapchain_proxy = new Direct3DSwapChain9(this, *ppSwapChain);
		_additional_swapchains.push_back(swapchain_proxy);
		*ppSwapChain = swapchain_proxy;

#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::debug, "Returning IDirect3DSwapChain9 object %p (%p).", swapchain_proxy, swapchain_proxy->_orig);
#endif
	}
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreateAdditionalSwapChain failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9 **ppSwapChain)
{
	if (iSwapChain != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Access to multi-head swap chain at index %u is unsupported.", iSwapChain);
		return D3DERR_INVALIDCALL;
	}

	if (ppSwapChain == nullptr)
		return D3DERR_INVALIDCALL;

	_implicit_swapchain->AddRef();
	*ppSwapChain = _implicit_swapchain;

	return D3D_OK;
}
UINT    STDMETHODCALLTYPE Direct3DDevice9::GetNumberOfSwapChains()
{
	return 1; // Multi-head swap chains are not supported
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::Reset(D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDirect3DDevice9::Reset(this = %p, pPresentationParameters = %p) ...",
		this, pPresentationParameters);

	if (pPresentationParameters == nullptr)
		return D3DERR_INVALIDCALL;

	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, _d3d.get(), _cp.AdapterOrdinal, _cp.hFocusWindow);

	// Release all resources before performing reset
	_implicit_swapchain->on_reset(true);
	on_reset();

	assert(!g_in_d3d9_runtime && !g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	const HRESULT hr = _orig->Reset(&pp);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;

	// Update output values (see https://docs.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-reset)
	pPresentationParameters->BackBufferWidth = pp.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = pp.BackBufferHeight;
	pPresentationParameters->BackBufferFormat = pp.BackBufferFormat;
	pPresentationParameters->BackBufferCount = pp.BackBufferCount;

	if (SUCCEEDED(hr))
	{
		on_init();
		_implicit_swapchain->on_init(true);
	}
	else
	{
		reshade::log::message(reshade::log::level::error, "IDirect3DDevice9::Reset failed with error code %s!", reshade::log::hr_to_string(hr).c_str());

		// Initialize device implementation even when reset failed, so that 'init_device', 'init_command_list' and 'init_command_queue' events are still called
		on_init();
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion)
{
	_implicit_swapchain->on_present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	const HRESULT hr = _orig->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	_implicit_swapchain->handle_device_loss(hr);

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer)
{
	if (iSwapChain != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Access to multi-head swap chain at index %u is unsupported.", iSwapChain);
		return D3DERR_INVALIDCALL;
	}

	return _implicit_swapchain->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus)
{
	if (iSwapChain != 0)
	{
		reshade::log::message(reshade::log::level::info, "Access to multi-head swap chain at index %u is unsupported.", iSwapChain);
		return D3DERR_INVALIDCALL;
	}

	return _implicit_swapchain->GetRasterStatus(pRasterStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetDialogBoxMode(BOOL bEnableDialogs)
{
	return _orig->SetDialogBoxMode(bEnableDialogs);
}
void    STDMETHODCALLTYPE Direct3DDevice9::SetGammaRamp(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP *pRamp)
{
	if (iSwapChain != 0)
	{
		reshade::log::message(reshade::log::level::info, "Access to multi-head swap chain at index %u is unsupported.", iSwapChain);
		return;
	}

	return _orig->SetGammaRamp(0, Flags, pRamp);
}
void    STDMETHODCALLTYPE Direct3DDevice9::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP *pRamp)
{
	if (iSwapChain != 0)
	{
		reshade::log::message(reshade::log::level::info, "Access to multi-head swap chain at index %u is unsupported.", iSwapChain);
		return;
	}

	return _orig->GetGammaRamp(0, pRamp);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_TEXTURE, Usage, Pool, D3DMULTISAMPLE_NONE, 0, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, Levels, FALSE, _caps, pSharedHandle != nullptr);

	if (desc.texture.format != reshade::api::format::unknown && // Skip special textures and unknown texture formats
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, &Levels, nullptr, _caps);

	const HRESULT hr = _orig->CreateTexture(internal_desc.Width, internal_desc.Height, Levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, ppTexture, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppTexture != nullptr);

#if RESHADE_ADDON
		IDirect3DTexture9 *const resource = *ppTexture;
#endif
#if RESHADE_ADDON >= 2
		const auto device_proxy = this;
		resource->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("IDirect3DTexture9::LockRect", reshade::hooks::vtable_from_instance(resource), 19, reinterpret_cast<reshade::hook::address>(&IDirect3DTexture9_LockRect));
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("IDirect3DTexture9::UnlockRect", reshade::hooks::vtable_from_instance(resource), 20, reinterpret_cast<reshade::hook::address>(&IDirect3DTexture9_UnlockRect));

		// Hook surfaces explicitly here, since some applications lock textures via its individual surfaces and they use a different vtable than standalone surfaces
		for (UINT level = 0, levels = resource->GetLevelCount(); level < levels; ++level)
		{
			com_ptr<IDirect3DSurface9> surface;
			if (SUCCEEDED(resource->GetSurfaceLevel(level, &surface)))
			{
				surface->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

				if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
					reshade::hooks::install("IDirect3DSurface9::LockRect", reshade::hooks::vtable_from_instance(surface.get()), 13, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_LockRect));
				if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
					reshade::hooks::install("IDirect3DSurface9::UnlockRect", reshade::hooks::vtable_from_instance(surface.get()), 14, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_UnlockRect));
			}
		}
#endif
#if RESHADE_ADDON
		InterlockedIncrement(&_resource_ref);
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(resource));

		register_destruction_callback_d3d9(resource, [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			InterlockedDecrement(&_resource_ref);
		});

		// Register all surfaces of this texture too
		if (const reshade::api::resource_usage view_usage = desc.usage & (reshade::api::resource_usage::render_target | reshade::api::resource_usage::depth_stencil); view_usage != 0)
		{
			for (UINT level = 0, levels = resource->GetLevelCount(); level < levels; ++level)
			{
				com_ptr<IDirect3DSurface9> surface;
				if (SUCCEEDED(resource->GetSurfaceLevel(level, &surface)))
				{
					reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
						this,
						to_handle(resource),
						view_usage,
						reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d, desc.texture.format, level, 1, 0, 1),
						to_handle(surface.get()));

					if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
					{
						register_destruction_callback_d3d9(surface.get(), [this, resource_view = surface.get()]() {
							reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(resource_view));
						});
					}
				}
			}
		}
		if ((desc.usage & reshade::api::resource_usage::shader_resource) != 0)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
				this,
				to_handle(resource),
				reshade::api::resource_usage::shader_resource,
				reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d, desc.texture.format, 0, UINT32_MAX, 0, UINT32_MAX),
				reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });

			if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
			{
				register_destruction_callback_d3d9(resource, [this, resource]() {
					reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });
				}, 1);
			}
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreateTexture failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9 **ppVolumeTexture, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DVOLUME_DESC internal_desc { Format, D3DRTYPE_VOLUMETEXTURE, Usage, Pool, Width, Height, Depth };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, Levels, pSharedHandle != nullptr);

	if (desc.texture.format != reshade::api::format::unknown &&
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, &Levels, _caps);

	const HRESULT hr = _orig->CreateVolumeTexture(internal_desc.Width, internal_desc.Height, internal_desc.Depth, Levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, ppVolumeTexture, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppVolumeTexture != nullptr);

#if RESHADE_ADDON
		IDirect3DVolumeTexture9 *const resource = *ppVolumeTexture;
#endif
#if RESHADE_ADDON >= 2
		const auto device_proxy = this;
		resource->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("IDirect3DVolumeTexture9::LockBox", reshade::hooks::vtable_from_instance(resource), 19, reinterpret_cast<reshade::hook::address>(&IDirect3DVolumeTexture9_LockBox));
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("IDirect3DVolumeTexture9::UnlockBox", reshade::hooks::vtable_from_instance(resource), 20, reinterpret_cast<reshade::hook::address>(&IDirect3DVolumeTexture9_UnlockBox));

		for (UINT level = 0, levels = resource->GetLevelCount(); level < levels; ++level)
		{
			com_ptr<IDirect3DVolume9> volume;
			if (SUCCEEDED(resource->GetVolumeLevel(level, &volume)))
			{
				volume->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

				if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
					reshade::hooks::install("IDirect3DVolume9::LockBox", reshade::hooks::vtable_from_instance(volume.get()), 9, reinterpret_cast<reshade::hook::address>(&IDirect3DVolume9_LockBox));
				if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
					reshade::hooks::install("IDirect3DVolume9::UnlockBox", reshade::hooks::vtable_from_instance(volume.get()), 10, reinterpret_cast<reshade::hook::address>(&IDirect3DVolume9_UnlockBox));
			}
		}
#endif
#if RESHADE_ADDON
		InterlockedIncrement(&_resource_ref);
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(resource));

		register_destruction_callback_d3d9(resource, [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			InterlockedDecrement(&_resource_ref);
		});

		if ((desc.usage & reshade::api::resource_usage::shader_resource) != 0)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
				this,
				to_handle(resource),
				reshade::api::resource_usage::shader_resource,
				reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_3d, desc.texture.format, 0, UINT32_MAX, 0, UINT32_MAX),
				reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });

			if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
			{
				register_destruction_callback_d3d9(resource, [this, resource]() {
					reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });
				}, 1);
			}
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreateVolumeTexture failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9 **ppCubeTexture, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc { Format, D3DRTYPE_CUBETEXTURE, Usage, Pool, D3DMULTISAMPLE_NONE, 0, EdgeLength, EdgeLength };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, Levels, FALSE, _caps, pSharedHandle != nullptr);

	if (desc.texture.format != reshade::api::format::unknown &&
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, &Levels, nullptr, _caps);

	const HRESULT hr = _orig->CreateCubeTexture(internal_desc.Width, Levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, ppCubeTexture, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppCubeTexture != nullptr);

#if RESHADE_ADDON
		IDirect3DCubeTexture9 *const resource = *ppCubeTexture;
#endif
#if RESHADE_ADDON >= 2
		const auto device_proxy = this;
		resource->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("IDirect3DCubeTexture9::LockRect", reshade::hooks::vtable_from_instance(resource), 19, reinterpret_cast<reshade::hook::address>(&IDirect3DCubeTexture9_LockRect));
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("IDirect3DCubeTexture9::UnlockRect", reshade::hooks::vtable_from_instance(resource), 20, reinterpret_cast<reshade::hook::address>(&IDirect3DCubeTexture9_UnlockRect));

		// Hook surfaces explicitly here, since some applications lock textures via its individual surfaces and they use a different vtable than standalone surfaces
		for (UINT level = 0, levels = resource->GetLevelCount(); level < levels; ++level)
		{
			for (D3DCUBEMAP_FACES face = D3DCUBEMAP_FACE_POSITIVE_X; face <= D3DCUBEMAP_FACE_NEGATIVE_Z; face = static_cast<D3DCUBEMAP_FACES>(face + 1))
			{
				com_ptr<IDirect3DSurface9> surface;
				if (SUCCEEDED(resource->GetCubeMapSurface(face, level, &surface)))
				{
					surface->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

					if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
						reshade::hooks::install("IDirect3DSurface9::LockRect", reshade::hooks::vtable_from_instance(surface.get()), 13, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_LockRect));
					if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
						reshade::hooks::install("IDirect3DSurface9::UnlockRect", reshade::hooks::vtable_from_instance(surface.get()), 14, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_UnlockRect));
				}
			}
		}
#endif
#if RESHADE_ADDON
		InterlockedIncrement(&_resource_ref);
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(resource));

		register_destruction_callback_d3d9(resource, [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			InterlockedDecrement(&_resource_ref);
		});

		// Register all surfaces of this texture too
		if (const reshade::api::resource_usage view_usage = desc.usage & (reshade::api::resource_usage::render_target | reshade::api::resource_usage::depth_stencil); view_usage != 0)
		{
			for (UINT level = 0, levels = resource->GetLevelCount(); level < levels; ++level)
			{
				for (D3DCUBEMAP_FACES face = D3DCUBEMAP_FACE_POSITIVE_X; face <= D3DCUBEMAP_FACE_NEGATIVE_Z; face = static_cast<D3DCUBEMAP_FACES>(face + 1))
				{
					com_ptr<IDirect3DSurface9> surface;
					if (SUCCEEDED(resource->GetCubeMapSurface(face, level, &surface)))
					{
						reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
							this,
							to_handle(resource),
							view_usage,
							reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_2d, desc.texture.format, level, 1, face, 1),
							to_handle(surface.get()));

						if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
						{
							register_destruction_callback_d3d9(surface.get(), [this, resource_view = surface.get()]() {
								reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(resource_view));
							});
						}
					}
				}
			}
		}
		if ((desc.usage & reshade::api::resource_usage::shader_resource) != reshade::api::resource_usage::undefined)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
				this,
				to_handle(resource),
				reshade::api::resource_usage::shader_resource,
				reshade::api::resource_view_desc(reshade::api::resource_view_type::texture_cube, desc.texture.format, 0, UINT32_MAX, 0, UINT32_MAX),
				reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });

			if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
			{
				register_destruction_callback_d3d9(resource, [this, resource]() {
					reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });
				}, 1);
			}
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreateCubeTexture failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle)
{
	// Need to allow buffer for use in software vertex processing, since application uses software and not hardware processing, but device was created with both
	if (_use_software_rendering)
		Usage |= D3DUSAGE_SOFTWAREPROCESSING;

#if RESHADE_ADDON
	D3DVERTEXBUFFER_DESC internal_desc = { D3DFMT_VERTEXDATA, D3DRTYPE_VERTEXBUFFER, Usage, Pool, Length, FVF };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc);

	const HRESULT hr = _orig->CreateVertexBuffer(internal_desc.Size, internal_desc.Usage, internal_desc.FVF, internal_desc.Pool, ppVertexBuffer, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppVertexBuffer != nullptr);

#if RESHADE_ADDON
		IDirect3DVertexBuffer9 *const resource = *ppVertexBuffer;
#endif
#if RESHADE_ADDON >= 2
		const auto device_proxy = this;
		resource->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
			reshade::hooks::install("IDirect3DVertexBuffer9::Lock", reshade::hooks::vtable_from_instance(resource), 11, reinterpret_cast<reshade::hook::address>(&IDirect3DVertexBuffer9_Lock));
		if (reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>())
			reshade::hooks::install("IDirect3DVertexBuffer9::Unlock", reshade::hooks::vtable_from_instance(resource), 12, reinterpret_cast<reshade::hook::address>(&IDirect3DVertexBuffer9_Unlock));
#endif
#if RESHADE_ADDON
		InterlockedIncrement(&_resource_ref);
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(resource));

		register_destruction_callback_d3d9(resource, [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			InterlockedDecrement(&_resource_ref);
		});
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreateVertexBuffer failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle)
{
	if (_use_software_rendering)
		Usage |= D3DUSAGE_SOFTWAREPROCESSING;

#if RESHADE_ADDON
	D3DINDEXBUFFER_DESC internal_desc = { Format, D3DRTYPE_INDEXBUFFER, Usage, Pool, Length };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc);

	const HRESULT hr = _orig->CreateIndexBuffer(internal_desc.Size, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, ppIndexBuffer, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppIndexBuffer != nullptr);

#if RESHADE_ADDON
		IDirect3DIndexBuffer9 *const resource = *ppIndexBuffer;
#endif
#if RESHADE_ADDON >= 2
		const auto device_proxy = this;
		resource->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
			reshade::hooks::install("IDirect3DIndexBuffer9::Lock", reshade::hooks::vtable_from_instance(resource), 11, reinterpret_cast<reshade::hook::address>(&IDirect3DIndexBuffer9_Lock));
		if (reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>())
			reshade::hooks::install("IDirect3DIndexBuffer9::Unlock", reshade::hooks::vtable_from_instance(resource), 12, reinterpret_cast<reshade::hook::address>(&IDirect3DIndexBuffer9_Unlock));
#endif
#if RESHADE_ADDON
		InterlockedIncrement(&_resource_ref);
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(resource));

		register_destruction_callback_d3d9(resource, [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			InterlockedDecrement(&_resource_ref);
		});
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreateIndexBuffer failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_SURFACE, D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT, MultiSample, MultisampleQuality, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, 1, Lockable, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::render_target))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, &Lockable, _caps);

	const HRESULT hr = ((desc.usage & reshade::api::resource_usage::shader_resource) != 0) && !Lockable ?
		create_surface_replacement(internal_desc, ppSurface, pSharedHandle) :
		_orig->CreateRenderTarget(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.MultiSampleType, internal_desc.MultiSampleQuality, Lockable, ppSurface, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;
#endif
#if RESHADE_ADDON >= 2
		const auto device_proxy = this;
		surface->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

		if (Lockable)
		{
			if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
				reshade::hooks::install("IDirect3DSurface9::LockRect", reshade::hooks::vtable_from_instance(surface), 13, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_LockRect));
			if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
				reshade::hooks::install("IDirect3DSurface9::UnlockRect", reshade::hooks::vtable_from_instance(surface), 14, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_UnlockRect));
		}
#endif
#if RESHADE_ADDON
		// In case surface was replaced with a texture resource
		com_ptr<IDirect3DResource9> resource;
		if (SUCCEEDED(surface->GetContainer(IID_PPV_ARGS(&resource))))
			desc.type = reshade::api::resource_type::texture_2d;
		else
			resource = surface;

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::render_target,
			to_handle(resource.get()));
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			to_handle(resource.get()),
			reshade::api::resource_usage::render_target,
			reshade::api::resource_view_desc(desc.texture.samples > 1 ? reshade::api::resource_view_type::texture_2d_multisample : reshade::api::resource_view_type::texture_2d, desc.texture.format, 0, 1, 0, 1),
			to_handle(surface));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
		{
			register_destruction_callback_d3d9(resource.get(), [this, resource = resource.get()]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			});
		}
		if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
		{
			register_destruction_callback_d3d9(surface, [this, surface]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
			}, surface == resource ? 1 : 0);
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreateRenderTarget failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DSURFACE_DESC old_desc = { Format, D3DRTYPE_SURFACE, D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT, MultiSample, MultisampleQuality, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(old_desc, 1, FALSE, _caps, pSharedHandle != nullptr);

	D3DSURFACE_DESC internal_desc = old_desc;
	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::depth_stencil))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, nullptr, _caps);

	const HRESULT hr = ((desc.usage & reshade::api::resource_usage::shader_resource) != 0) ?
		create_surface_replacement(internal_desc, ppSurface, pSharedHandle) :
		_orig->CreateDepthStencilSurface(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.MultiSampleType, internal_desc.MultiSampleQuality, Discard, ppSurface, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;
		*ppSurface = new Direct3DDepthStencilSurface9(this, surface, old_desc);

		// In case surface was replaced with a texture resource
		com_ptr<IDirect3DResource9> resource;
		if (SUCCEEDED(surface->GetContainer(IID_PPV_ARGS(&resource))))
			desc.type = reshade::api::resource_type::texture_2d;
		else
			resource = surface;

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::depth_stencil,
			to_handle(resource.get()));
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			to_handle(resource.get()),
			reshade::api::resource_usage::depth_stencil,
			reshade::api::resource_view_desc(desc.texture.samples > 1 ? reshade::api::resource_view_type::texture_2d_multisample : reshade::api::resource_view_type::texture_2d, desc.texture.format, 0, 1, 0, 1),
			to_handle(surface));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
		{
			register_destruction_callback_d3d9(resource.get(), [this, resource = resource.get()]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			});
		}
		if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
		{
			register_destruction_callback_d3d9(surface, [this, surface]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
			}, surface == resource ? 1 : 0);
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreateDepthStencilSurface failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::UpdateSurface(IDirect3DSurface9 *pSrcSurface, const RECT *pSrcRect, IDirect3DSurface9 *pDstSurface, const POINT *pDstPoint)
{
	assert(pSrcSurface != nullptr && pDstSurface != nullptr);

#if RESHADE_ADDON >= 2
	if (reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
	{
		uint32_t src_subresource;
		reshade::api::subresource_box src_box;
		const reshade::api::resource src_resource = get_resource_from_view(to_handle(pSrcSurface), &src_subresource);
		uint32_t dst_subresource;
		reshade::api::subresource_box dst_box;
		const reshade::api::resource dst_resource = get_resource_from_view(to_handle(pDstSurface), &dst_subresource);

		if (pSrcRect != nullptr)
		{
			convert_rect_to_box(pDstPoint, pSrcRect->right - pSrcRect->left, pSrcRect->bottom - pSrcRect->top, dst_box);
		}
		else
		{
			D3DSURFACE_DESC desc;
			pSrcSurface->GetDesc(&desc);

			convert_rect_to_box(pDstPoint, desc.Width, desc.Height, dst_box);
		}

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(this,
				src_resource, src_subresource, convert_rect_to_box(pSrcRect, src_box),
				dst_resource, dst_subresource, pDstPoint != nullptr ? &dst_box : nullptr, reshade::api::filter_mode::min_mag_mip_point))
			return D3D_OK;
	}
#endif

	return _orig->UpdateSurface(pSrcSurface, pSrcRect, pDstSurface, pDstPoint);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::UpdateTexture(IDirect3DBaseTexture9 *pSrcTexture, IDirect3DBaseTexture9 *pDstTexture)
{
	assert(pSrcTexture != nullptr && pDstTexture != nullptr);

#if RESHADE_ADDON >= 2
	if (reshade::invoke_addon_event<reshade::addon_event::copy_resource>(this, to_handle(pSrcTexture), to_handle(pDstTexture)))
		return D3D_OK;
#endif

	return _orig->UpdateTexture(pSrcTexture, pDstTexture);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderTargetData(IDirect3DSurface9 *pSrcSurface, IDirect3DSurface9 *pDstSurface)
{
	assert(pSrcSurface != nullptr && pDstSurface != nullptr);

#if RESHADE_ADDON >= 2
	if (reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
	{
		uint32_t src_subresource;
		const reshade::api::resource src_resource = get_resource_from_view(to_handle(pSrcSurface), &src_subresource);
		uint32_t dst_subresource;
		const reshade::api::resource dst_resource = get_resource_from_view(to_handle(pDstSurface), &dst_subresource);

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(this, src_resource, src_subresource, nullptr, dst_resource, dst_subresource, nullptr, reshade::api::filter_mode::min_mag_mip_point))
			return D3D_OK;
	}
#endif

	return _orig->GetRenderTargetData(pSrcSurface, pDstSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9 *pDestSurface)
{
	if (iSwapChain != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Access to multi-head swap chain at index %u is unsupported.", iSwapChain);
		return D3DERR_INVALIDCALL;
	}

	return _implicit_swapchain->GetFrontBufferData(pDestSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::StretchRect(IDirect3DSurface9 *pSrcSurface, const RECT *pSrcRect, IDirect3DSurface9 *pDstSurface, const RECT *pDstRect, D3DTEXTUREFILTERTYPE Filter)
{
	assert(pSrcSurface != nullptr && pDstSurface != nullptr);

#if RESHADE_ADDON
	if (com_ptr<Direct3DDepthStencilSurface9> surface_proxy;
		SUCCEEDED(pSrcSurface->QueryInterface(IID_PPV_ARGS(&surface_proxy))))
		pSrcSurface = surface_proxy->_orig;
	if (com_ptr<Direct3DDepthStencilSurface9> surface_proxy;
		SUCCEEDED(pDstSurface->QueryInterface(IID_PPV_ARGS(&surface_proxy))))
		pDstSurface = surface_proxy->_orig;
#endif

#if RESHADE_ADDON >= 2
	if (reshade::has_addon_event<reshade::addon_event::copy_texture_region>() ||
		reshade::has_addon_event<reshade::addon_event::resolve_texture_region>())
	{
		D3DSURFACE_DESC desc;
		pSrcSurface->GetDesc(&desc);

		uint32_t src_subresource;
		const reshade::api::resource src_resource = get_resource_from_view(to_handle(pSrcSurface), &src_subresource);
		uint32_t dst_subresource;
		const reshade::api::resource dst_resource = get_resource_from_view(to_handle(pDstSurface), &dst_subresource);

		if (desc.MultiSampleType == D3DMULTISAMPLE_NONE)
		{
			reshade::api::subresource_box src_box, dst_box;

			if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(this,
					src_resource, src_subresource, convert_rect_to_box(pSrcRect, src_box),
					dst_resource, dst_subresource, convert_rect_to_box(pDstRect, dst_box),
					Filter == D3DTEXF_NONE || Filter == D3DTEXF_POINT ? reshade::api::filter_mode::min_mag_mip_point : reshade::api::filter_mode::min_mag_mip_linear))
				return D3D_OK;
		}
		else
		{
			reshade::api::subresource_box src_box;

			if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(this,
					src_resource, src_subresource, convert_rect_to_box(pSrcRect, src_box),
					dst_resource, dst_subresource, static_cast<uint32_t>(pDstRect != nullptr ? pDstRect->left : 0), static_cast<uint32_t>(pDstRect != nullptr ? pDstRect->top : 0), 0,
					reshade::d3d9::convert_format(desc.Format)))
				return D3D_OK;
		}
	}
#endif

	return _orig->StretchRect(pSrcSurface, pSrcRect, pDstSurface, pDstRect, Filter);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ColorFill(IDirect3DSurface9 *pSurface, const RECT *pRect, D3DCOLOR Color)
{
#if RESHADE_ADDON
	if (const float color[4] = { ((Color >> 16) & 0xFF) / 255.0f, ((Color >> 8) & 0xFF) / 255.0f, (Color & 0xFF) / 255.0f, ((Color >> 24) & 0xFF) / 255.0f };
		reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(this, to_handle(pSurface), color, pRect != nullptr ? 1 : 0, reinterpret_cast<const reshade::api::rect *>(pRect)))
		return D3D_OK;
#endif

	return _orig->ColorFill(pSurface, pRect, Color);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_SURFACE, 0, Pool, D3DMULTISAMPLE_NONE, 0, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, 1, FALSE, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, nullptr, _caps);

	const HRESULT hr = _orig->CreateOffscreenPlainSurface(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.Pool, ppSurface, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;
#endif
#if RESHADE_ADDON >= 2
		const auto device_proxy = this;
		surface->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("IDirect3DSurface9::LockRect", reshade::hooks::vtable_from_instance(surface), 13, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_LockRect));
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("IDirect3DSurface9::UnlockRect", reshade::hooks::vtable_from_instance(surface), 14, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_UnlockRect));
#endif
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::render_target,
			reshade::api::resource { reinterpret_cast<uintptr_t>(surface) });
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			reshade::api::resource { reinterpret_cast<uintptr_t>(surface) },
			reshade::api::resource_usage::render_target,
			reshade::api::resource_view_desc(desc.texture.format),
			to_handle(surface));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
		{
			register_destruction_callback_d3d9(surface, [this, surface]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(surface) });
			});
		}
		if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
		{
			register_destruction_callback_d3d9(surface, [this, surface]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
			}, 1);
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreateOffscreenPlainSurface failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget)
{
	const HRESULT hr = _orig->SetRenderTarget(RenderTargetIndex, pRenderTarget);
#if RESHADE_ADDON
	if (SUCCEEDED(hr) && (
		reshade::has_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>() ||
		reshade::has_addon_event<reshade::addon_event::bind_viewports>()))
	{
		DWORD count = 0;
		com_ptr<IDirect3DSurface9> surface;
		reshade::api::resource_view rtvs[8] = {}, dsv = {};
		for (DWORD i = 0; i < _caps.NumSimultaneousRTs; ++i, surface.reset())
		{
			if (FAILED(_orig->GetRenderTarget(i, &surface)))
				continue;

			// All surfaces that can be used as render target should be registered at this point
			rtvs[i] = to_handle(surface.get());
			count = i + 1;
		}
		if (SUCCEEDED(_orig->GetDepthStencilSurface(&surface)))
		{
			// All surfaces that can be used as depth-stencil should be registered at this point
			dsv = to_handle(surface.get());
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(this, count, rtvs, dsv);

		if (pRenderTarget != nullptr)
		{
			// Setting a new render target will cause the viewport to be set to the full size of the new render target
			// See https://docs.microsoft.com/windows/win32/api/d3d9helper/nf-d3d9helper-idirect3ddevice9-setrendertarget
			D3DSURFACE_DESC desc;
			pRenderTarget->GetDesc(&desc);

			const reshade::api::viewport viewport_data = {
				0.0f,
				0.0f,
				static_cast<float>(desc.Width),
				static_cast<float>(desc.Height),
				0.0f,
				1.0f
			};

			reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(this, 0, 1, &viewport_data);
		}
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget)
{
	return _orig->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil)
{
#if RESHADE_ADDON
	com_ptr<Direct3DDepthStencilSurface9> surface_proxy;
	if (pNewZStencil != nullptr &&
		SUCCEEDED(pNewZStencil->QueryInterface(IID_PPV_ARGS(&surface_proxy))))
		pNewZStencil = surface_proxy->_orig;
#endif

	const HRESULT hr = _orig->SetDepthStencilSurface(pNewZStencil);
#if RESHADE_ADDON
	if (SUCCEEDED(hr))
	{
		_current_depth_stencil = std::move(surface_proxy);

		if (reshade::has_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>())
		{
			DWORD count = 0;
			com_ptr<IDirect3DSurface9> surface;
			reshade::api::resource_view rtvs[8] = {};
			for (DWORD i = 0; i < _caps.NumSimultaneousRTs; ++i, surface.reset())
			{
				if (FAILED(_orig->GetRenderTarget(i, &surface)))
					continue;

				rtvs[i] = to_handle(surface.get());
				count = i + 1;
			}

			reshade::invoke_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(this, count, rtvs, to_handle(pNewZStencil));
		}
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface)
{
	const HRESULT hr = _orig->GetDepthStencilSurface(ppZStencilSurface);
#if RESHADE_ADDON
	if (SUCCEEDED(hr))
	{
		assert(ppZStencilSurface != nullptr);

		if (_current_depth_stencil != nullptr)
		{
			IDirect3DSurface9 *const surface = *ppZStencilSurface;
			Direct3DDepthStencilSurface9 *const surface_proxy = _current_depth_stencil.get();

			surface_proxy->AddRef();
			surface->Release();
			*ppZStencilSurface = surface_proxy;
		}
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::BeginScene()
{
#if RESHADE_ADDON
	// Force next draw call to trigger 'bind_pipeline_states' event with primitive topology
	_current_prim_type = static_cast<D3DPRIMITIVETYPE>(0);
#endif

	return _orig->BeginScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EndScene()
{
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::execute_command_list>(this, this);
#endif

	return _orig->EndScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
#if RESHADE_ADDON
	if ((Flags & (D3DCLEAR_TARGET)) != 0 &&
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>())
	{
		com_ptr<IDirect3DSurface9> surface;
		for (DWORD i = 0; i < _caps.NumSimultaneousRTs; ++i, surface.reset())
		{
			if (FAILED(_orig->GetRenderTarget(i, &surface)))
				continue;

			if (const float color[4] = { ((Color >> 16) & 0xFF) / 255.0f, ((Color >> 8) & 0xFF) / 255.0f, (Color & 0xFF) / 255.0f, ((Color >> 24) & 0xFF) / 255.0f };
				reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(
					this,
					to_handle(surface.get()),
					color,
					Count,
					reinterpret_cast<const reshade::api::rect *>(pRects)))
				Flags &= ~(D3DCLEAR_TARGET); // This will prevent all render targets from getting cleared, not just the current one ...
		}
	}
	if ((Flags & (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL)) != 0 &&
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>())
	{
		com_ptr<IDirect3DSurface9> surface;
		if (SUCCEEDED(_orig->GetDepthStencilSurface(&surface)))
		{
			if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(
					this,
					to_handle(surface.get()),
					(Flags & D3DCLEAR_ZBUFFER) != 0 ? &Z : nullptr,
					(Flags & D3DCLEAR_STENCIL) != 0 ? reinterpret_cast<const uint8_t *>(&Stencil) : nullptr,
					Count,
					reinterpret_cast<const reshade::api::rect *>(pRects)))
				Flags &= ~(D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL);
		}
	}

	if (Flags == 0)
		return D3D_OK;
#endif

	return _orig->Clear(Count, pRects, Flags, Color, Z, Stencil);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix)
{
	return _orig->SetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix)
{
	return _orig->GetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix)
{
	return _orig->MultiplyTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetViewport(const D3DVIEWPORT9 *pViewport)
{
	const HRESULT hr = _orig->SetViewport(pViewport);
#if RESHADE_ADDON
	if (SUCCEEDED(hr))
	{
		const reshade::api::viewport viewport_data = {
			static_cast<float>(pViewport->X),
			static_cast<float>(pViewport->Y),
			static_cast<float>(pViewport->Width),
			static_cast<float>(pViewport->Height),
			pViewport->MinZ,
			pViewport->MaxZ
		};

		reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(this, 0, 1, &viewport_data);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetViewport(D3DVIEWPORT9 *pViewport)
{
	return _orig->GetViewport(pViewport);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetMaterial(const D3DMATERIAL9 *pMaterial)
{
	return _orig->SetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetMaterial(D3DMATERIAL9 *pMaterial)
{
	return _orig->GetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetLight(DWORD Index, const D3DLIGHT9 *pLight)
{
	return _orig->SetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetLight(DWORD Index, D3DLIGHT9 *pLight)
{
	return _orig->GetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::LightEnable(DWORD Index, BOOL Enable)
{
	return _orig->LightEnable(Index, Enable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetLightEnable(DWORD Index, BOOL *pEnable)
{
	return _orig->GetLightEnable(Index, pEnable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetClipPlane(DWORD Index, const float *pPlane)
{
	return _orig->SetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetClipPlane(DWORD Index, float *pPlane)
{
	return _orig->GetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
	const HRESULT hr = _orig->SetRenderState(State, Value);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr) &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		reshade::api::dynamic_state state = reshade::d3d9::convert_dynamic_state(State);
		uint32_t value = Value;

		switch (State)
		{
		case D3DRS_BLENDOP:
		case D3DRS_BLENDOPALPHA:
			value = static_cast<uint32_t>(reshade::d3d9::convert_blend_op(static_cast<D3DBLENDOP>(Value)));
			break;
		case D3DRS_SRCBLEND:
		case D3DRS_DESTBLEND:
		case D3DRS_SRCBLENDALPHA:
		case D3DRS_DESTBLENDALPHA:
			value = static_cast<uint32_t>(reshade::d3d9::convert_blend_factor(static_cast<D3DBLEND>(Value)));
			break;
		case D3DRS_FILLMODE:
			value = static_cast<uint32_t>(reshade::d3d9::convert_fill_mode(static_cast<D3DFILLMODE>(Value)));
			break;
		case D3DRS_CULLMODE:
			value = static_cast<uint32_t>(reshade::d3d9::convert_cull_mode(static_cast<D3DCULL>(Value), false));
			break;
		case D3DRS_ZFUNC:
		case D3DRS_ALPHAFUNC:
		case D3DRS_STENCILFUNC:
		case D3DRS_CCW_STENCILFUNC:
			value = static_cast<uint32_t>(reshade::d3d9::convert_compare_op(static_cast<D3DCMPFUNC>(Value)));
			break;
		case D3DRS_STENCILFAIL:
		case D3DRS_STENCILZFAIL:
		case D3DRS_STENCILPASS:
		case D3DRS_CCW_STENCILFAIL:
		case D3DRS_CCW_STENCILZFAIL:
		case D3DRS_CCW_STENCILPASS:
			value = static_cast<uint32_t>(reshade::d3d9::convert_stencil_op(static_cast<D3DSTENCILOP>(Value)));
			break;
		case D3DRS_POINTSIZE:
			if (Value == 0x7FA05000 /* RESZ code */)
			{
				com_ptr<IDirect3DSurface9> surface;
				_orig->GetDepthStencilSurface(&surface);
				com_ptr<IDirect3DBaseTexture9> texture;
				_orig->GetTexture(0, &texture);

				uint32_t src_subresource;
				const reshade::api::resource src_resource = get_resource_from_view(to_handle(surface.get()), &src_subresource);
				const reshade::api::resource dst_resource = to_handle(texture.get());

				reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(this, src_resource, src_subresource, nullptr, dst_resource, 0, 0, 0, 0, reshade::api::format::unknown);
				return hr;
			}
			if (Value == MAKEFOURCC('A', '2', 'M', '0') || Value == MAKEFOURCC('A', '2', 'M', '1'))
			{
				state = reshade::api::dynamic_state::alpha_to_coverage_enable;
				value = (Value == MAKEFOURCC('A', '2', 'M', '1')) ? 1 : 0;
			}
			break;
		case D3DRS_ADAPTIVETESS_Y:
			if (Value == MAKEFOURCC('A', 'T', 'O', 'C') || Value == 0)
			{
				state = reshade::api::dynamic_state::alpha_to_coverage_enable;
				value = (Value == MAKEFOURCC('A', 'T', 'O', 'C')) ? 1 : 0;
			}
			break;
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue)
{
	return _orig->GetRenderState(State, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9 **ppSB)
{
	return _orig->CreateStateBlock(Type, ppSB);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::BeginStateBlock()
{
	return _orig->BeginStateBlock();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EndStateBlock(IDirect3DStateBlock9 **ppSB)
{
	return _orig->EndStateBlock(ppSB);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetClipStatus(const D3DCLIPSTATUS9 *pClipStatus)
{
	return _orig->SetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetClipStatus(D3DCLIPSTATUS9 *pClipStatus)
{
	return _orig->GetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture)
{
	return _orig->GetTexture(Stage, ppTexture);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture)
{
	const HRESULT hr = _orig->SetTexture(Stage, pTexture);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr) &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		reshade::api::shader_stage shader_stage = reshade::api::shader_stage::pixel;
		if (Stage >= D3DVERTEXTEXTURESAMPLER0)
		{
			shader_stage = reshade::api::shader_stage::vertex;
			Stage -= D3DVERTEXTEXTURESAMPLER0;
		}

		// There are no sampler state objects in D3D9, so cannot deduce a handle to one here meaningfully
		reshade::api::sampler_with_resource_view descriptor_data = {
			reshade::api::sampler { 0 },
			reshade::api::resource_view { reinterpret_cast<uintptr_t>(pTexture) }
		};

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			this,
			shader_stage,
			// See global pipeline layout specified in 'device_impl::on_init'
			_global_pipeline_layout, shader_stage == reshade::api::shader_stage::vertex ? 0 : 1,
			reshade::api::descriptor_table_update { {}, Stage, 0, 1, reshade::api::descriptor_type::sampler_with_resource_view, &descriptor_data });
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue)
{
	return _orig->GetTextureStageState(Stage, Type, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	return _orig->SetTextureStageState(Stage, Type, Value);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue)
{
	return _orig->GetSamplerState(Sampler, Type, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	return _orig->SetSamplerState(Sampler, Type, Value);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ValidateDevice(DWORD *pNumPasses)
{
	return _orig->ValidateDevice(pNumPasses);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY *pEntries)
{
	return _orig->SetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries)
{
	return _orig->GetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetCurrentTexturePalette(UINT PaletteNumber)
{
	return _orig->SetCurrentTexturePalette(PaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetCurrentTexturePalette(UINT *PaletteNumber)
{
	return _orig->GetCurrentTexturePalette(PaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetScissorRect(const RECT *pRect)
{
	const HRESULT hr = _orig->SetScissorRect(pRect);
#if RESHADE_ADDON
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(this, 0, 1, reinterpret_cast<const reshade::api::rect *>(pRect));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetScissorRect(RECT *pRect)
{
	return _orig->GetScissorRect(pRect);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetSoftwareVertexProcessing(BOOL bSoftware)
{
	return _orig->SetSoftwareVertexProcessing(bSoftware);
}
BOOL    STDMETHODCALLTYPE Direct3DDevice9::GetSoftwareVertexProcessing()
{
	return _orig->GetSoftwareVertexProcessing();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetNPatchMode(float nSegments)
{
	return _orig->SetNPatchMode(nSegments);
}
float   STDMETHODCALLTYPE Direct3DDevice9::GetNPatchMode()
{
	return _orig->GetNPatchMode();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
#if RESHADE_ADDON
	if (PrimitiveType != _current_prim_type)
	{
		_current_prim_type = PrimitiveType;
#endif
#if RESHADE_ADDON >= 2
		const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
		const uint32_t value = static_cast<uint32_t>(reshade::d3d9::convert_primitive_topology(PrimitiveType));

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
#endif
#if RESHADE_ADDON
	}

	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, reshade::d3d9::calc_vertex_from_prim_count(PrimitiveType, PrimitiveCount), 1, StartVertex, 0))
		return D3D_OK;
#endif

	return _orig->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount)
{
#if RESHADE_ADDON
	if (PrimitiveType != _current_prim_type)
	{
		_current_prim_type = PrimitiveType;
#endif
#if RESHADE_ADDON >= 2
		const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
		const uint32_t value = static_cast<uint32_t>(reshade::d3d9::convert_primitive_topology(PrimitiveType));

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
#endif
#if RESHADE_ADDON
	}

	if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(this, reshade::d3d9::calc_vertex_from_prim_count(PrimitiveType, PrimitiveCount), 1, StartIndex, BaseVertexIndex, 0))
		return D3D_OK;
#endif

	return _orig->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, StartIndex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
#if RESHADE_ADDON
	const uint32_t vertex_count = reshade::d3d9::calc_vertex_from_prim_count(PrimitiveType, PrimitiveCount);
#endif
#if RESHADE_ADDON >= 2
	if (reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
	{
		const uint32_t vertex_buffer_size = vertex_count * VertexStreamZeroStride;

		resize_primitive_up_buffers(vertex_buffer_size, 0, 0);

		if (void *mapped_vertex_data = nullptr;
			_primitive_up_vertex_buffer != 0 &&
			device_impl::map_buffer_region(_primitive_up_vertex_buffer, 0, vertex_buffer_size, reshade::api::map_access::write_discard, &mapped_vertex_data))
		{
			reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
				this,
				_primitive_up_vertex_buffer,
				0,
				vertex_buffer_size,
				reshade::api::map_access::write_discard,
				const_cast<void **>(&pVertexStreamZeroData));
			std::memcpy(mapped_vertex_data, pVertexStreamZeroData, vertex_buffer_size);
			reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(this, _primitive_up_vertex_buffer);
			device_impl::unmap_buffer_region(_primitive_up_vertex_buffer);
		}

		const uint64_t offset_64 = 0;

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, 0, 1, &_primitive_up_vertex_buffer, &offset_64, &VertexStreamZeroStride);
	}
#endif
#if RESHADE_ADDON
	if (PrimitiveType != _current_prim_type)
	{
		_current_prim_type = PrimitiveType;
#endif
#if RESHADE_ADDON >= 2
		const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
		const uint32_t value = static_cast<uint32_t>(reshade::d3d9::convert_primitive_topology(PrimitiveType));

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
#endif
#if RESHADE_ADDON
	}

	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, vertex_count, 1, 0, 0))
		return D3D_OK;
#endif

	const HRESULT hr = _orig->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr))
	{
		// Following any 'IDirect3DDevice9::DrawPrimitiveUP' call, the stream 0 settings are unset (see https://learn.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawprimitiveup)
		constexpr reshade::api::resource null_buffer = {};
		const uint64_t offset_64 = 0;

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, 0, 1, &null_buffer, &offset_64, nullptr);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void *pIndexData, D3DFORMAT IndexDataFormat, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
#if RESHADE_ADDON
	const uint32_t index_count = reshade::d3d9::calc_vertex_from_prim_count(PrimitiveType, PrimitiveCount);
#endif
#if RESHADE_ADDON >= 2
	if (reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>() ||
		reshade::has_addon_event<reshade::addon_event::bind_index_buffer>())
	{
		const uint32_t vertex_buffer_size = NumVertices * VertexStreamZeroStride;
		const uint32_t index_size = (IndexDataFormat == D3DFMT_INDEX32) ? 4 : 2;
		const uint32_t index_buffer_size = index_count * index_size;

		resize_primitive_up_buffers(vertex_buffer_size, index_buffer_size, index_size);

		if (void *mapped_vertex_data = nullptr;
			_primitive_up_vertex_buffer != 0 &&
			device_impl::map_buffer_region(_primitive_up_vertex_buffer, 0, vertex_buffer_size, reshade::api::map_access::write_discard, &mapped_vertex_data))
		{
			reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
				this,
				_primitive_up_vertex_buffer,
				0,
				vertex_buffer_size,
				reshade::api::map_access::write_discard,
				const_cast<void **>(&pVertexStreamZeroData));
			std::memcpy(mapped_vertex_data, pVertexStreamZeroData, vertex_buffer_size);
			reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(this, _primitive_up_vertex_buffer);
			device_impl::unmap_buffer_region(_primitive_up_vertex_buffer);
		}
		if (void *mapped_index_data = nullptr;
			_primitive_up_index_buffer != 0 &&
			device_impl::map_buffer_region(_primitive_up_index_buffer, 0, index_buffer_size, reshade::api::map_access::write_discard, &mapped_index_data))
		{
			reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
				this,
				_primitive_up_index_buffer,
				0,
				index_buffer_size,
				reshade::api::map_access::write_discard,
				const_cast<void **>(&pIndexData));
			std::memcpy(mapped_index_data, pIndexData, index_buffer_size);
			reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(this, _primitive_up_index_buffer);
			device_impl::unmap_buffer_region(_primitive_up_index_buffer);
		}

		const uint64_t offset_64 = 0;

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, 0, 1, &_primitive_up_vertex_buffer, &offset_64, &VertexStreamZeroStride);
		reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(this, _primitive_up_index_buffer, 0, index_size);
	}
#endif
#if RESHADE_ADDON
	if (PrimitiveType != _current_prim_type)
	{
		_current_prim_type = PrimitiveType;
#endif
#if RESHADE_ADDON >= 2
		const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
		const uint32_t value = static_cast<uint32_t>(reshade::d3d9::convert_primitive_topology(PrimitiveType));

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
#endif
#if RESHADE_ADDON
	}

	if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(this, index_count, 1, 0, 0, 0))
		return D3D_OK;
#endif

	const HRESULT hr = _orig->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr))
	{
		// Following any 'IDirect3DDevice9::DrawIndexedPrimitiveUP' call, the stream 0 and the index buffer settings are unset (see https://learn.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawindexedprimitiveup)
		constexpr reshade::api::resource null_buffer = {};
		const uint64_t offset_64 = 0;

		reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(this, null_buffer, 0, 0);
		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, 0, 1, &null_buffer, &offset_64, nullptr);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9 *pDestBuffer, IDirect3DVertexDeclaration9 *pVertexDecl, DWORD Flags)
{
#if RESHADE_ADDON >= 2
	com_ptr<IDirect3DVertexDeclaration9> prev_decl;
	_orig->GetVertexDeclaration(&prev_decl);

	const reshade::api::resource buffer = to_handle(pDestBuffer);
	const uint64_t offset_64 = DestIndex;

	_current_stream_output = pDestBuffer;
	_current_stream_output_offset = DestIndex;

	reshade::invoke_addon_event<reshade::addon_event::bind_stream_output_buffers>(this, 0, 1, &buffer, &offset_64, nullptr, nullptr, nullptr);
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::input_assembler, to_handle(pVertexDecl));

	com_ptr<IDirect3DVertexDeclaration9> modified_decl;
	_orig->GetVertexDeclaration(&modified_decl);
	if (modified_decl != prev_decl) // In case an add-on changed the vertex declaration during the preceding 'bind_pipeline' event
		pVertexDecl = modified_decl.get();

	HRESULT hr = D3D_OK;
	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, VertexCount, 1, SrcStartIndex, 0) == false)
		hr = _orig->ProcessVertices(SrcStartIndex, _current_stream_output_offset, VertexCount, _current_stream_output, pVertexDecl, Flags);

	const reshade::api::resource prev_buffer = { 0 };
	const uint64_t prev_offset_64 = 0;

	_current_stream_output = nullptr;
	_current_stream_output_offset = 0;

	reshade::invoke_addon_event<reshade::addon_event::bind_stream_output_buffers>(this, 0, 1, &prev_buffer, &prev_offset_64, nullptr, nullptr, nullptr);
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::input_assembler, to_handle(prev_decl.get()));

	return hr;
#else
	return _orig->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);
#endif
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexDeclaration(const D3DVERTEXELEMENT9 *pVertexElements, IDirect3DVertexDeclaration9 **ppDecl)
{
#if RESHADE_ADDON
	std::vector<D3DVERTEXELEMENT9> internal_desc; std::vector<reshade::api::input_element> desc;
	if (pVertexElements != nullptr)
		for (const D3DVERTEXELEMENT9 *internal_element = pVertexElements; internal_element->Stream != 0xFF; ++internal_element)
			desc.push_back(reshade::d3d9::convert_input_element(*internal_element));

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::input_layout, static_cast<uint32_t>(desc.size()), desc.data() }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		internal_desc.reserve(desc.size());
		for (size_t i = 0; i < desc.size(); ++i)
			reshade::d3d9::convert_input_element(desc[i], internal_desc.emplace_back());

		pVertexElements = internal_desc.data();
	}
#endif

	const HRESULT hr = _orig->CreateVertexDeclaration(pVertexElements, ppDecl);
	if (SUCCEEDED(hr))
	{
		assert(ppDecl != nullptr);

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(*ppDecl));
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreateVertexDeclaration failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl)
{
	const HRESULT hr = _orig->SetVertexDeclaration(pDecl);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::input_assembler, to_handle(pDecl));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexDeclaration(IDirect3DVertexDeclaration9 **ppDecl)
{
	return _orig->GetVertexDeclaration(ppDecl);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetFVF(DWORD FVF)
{
	const HRESULT hr = _orig->SetFVF(FVF);
#if 0
	if (SUCCEEDED(hr))
	{
		// TODO: This should invoke the 'bind_pipeline' event with a special input assembler pipeline handle
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::input_assembler, reshade::api::pipeline {});
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetFVF(DWORD *pFVF)
{
	return _orig->GetFVF(pFVF);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexShader(const DWORD *pFunction, IDirect3DVertexShader9 **ppShader)
{
#if RESHADE_ADDON
	if (pFunction == nullptr)
		return D3DERR_INVALIDCALL;

	// First token should be version token (https://docs.microsoft.com/windows-hardware/drivers/display/version-token), so verify its shader type
	assert((pFunction[0] >> 16) == 0xFFFE);

	reshade::api::shader_desc desc = {};
	desc.code = pFunction;

	// Find the end token to calculate token stream size (https://docs.microsoft.com/windows-hardware/drivers/display/end-token)
	desc.code_size = sizeof(DWORD);
	for (int i = 0; pFunction[i] != 0x0000FFFF; ++i)
		desc.code_size += sizeof(DWORD);

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::vertex_shader, 1, &desc }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		pFunction = static_cast<const DWORD *>(desc.code);
	}
#endif

	const HRESULT hr = _orig->CreateVertexShader(pFunction, ppShader);
	if (SUCCEEDED(hr))
	{
		assert(ppShader != nullptr);

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(*ppShader));
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreateVertexShader failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShader(IDirect3DVertexShader9 *pShader)
{
	const HRESULT hr = _orig->SetVertexShader(pShader);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::vertex_shader, to_handle(pShader));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShader(IDirect3DVertexShader9 **ppShader)
{
	return _orig->GetVertexShader(ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount)
{
	const HRESULT hr = _orig->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::vertex,
			// See global pipeline layout specified in 'device_impl::on_init'
			_global_pipeline_layout, 2,
			StartRegister * 4,
			Vector4fCount * 4,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount)
{
	return _orig->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount)
{
	const HRESULT hr = _orig->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::vertex,
			// See global pipeline layout specified in 'device_impl::on_init'
			_global_pipeline_layout, 3,
			StartRegister * 4,
			Vector4iCount * 4,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount)
{
	return _orig->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT BoolCount)
{
	const HRESULT hr = _orig->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::vertex,
			// See global pipeline layout specified in 'device_impl::on_init'
			_global_pipeline_layout, 4,
			StartRegister,
			BoolCount,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
{
	return _orig->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride)
{
	const HRESULT hr = _orig->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr) &&
		reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
	{
		const reshade::api::resource buffer = to_handle(pStreamData);
		const uint64_t offset_64 = OffsetInBytes;

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, StreamNumber, 1, &buffer, &offset_64, &Stride);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData, UINT *OffsetInBytes, UINT *pStride)
{
	return _orig->GetStreamSource(StreamNumber, ppStreamData, OffsetInBytes, pStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetStreamSourceFreq(UINT StreamNumber, UINT Divider)
{
	return _orig->SetStreamSourceFreq(StreamNumber, Divider);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetStreamSourceFreq(UINT StreamNumber, UINT *Divider)
{
	return _orig->GetStreamSourceFreq(StreamNumber, Divider);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetIndices(IDirect3DIndexBuffer9 *pIndexData)
{
	const HRESULT hr = _orig->SetIndices(pIndexData);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr) &&
		reshade::has_addon_event<reshade::addon_event::bind_index_buffer>())
	{
		uint32_t index_size = 0;
		if (pIndexData != nullptr)
		{
			D3DINDEXBUFFER_DESC desc;
			pIndexData->GetDesc(&desc);

			index_size = (desc.Format == D3DFMT_INDEX16) ? 2 : 4;
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(this, to_handle(pIndexData), 0, index_size);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetIndices(IDirect3DIndexBuffer9 **ppIndexData)
{
	return _orig->GetIndices(ppIndexData);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreatePixelShader(const DWORD *pFunction, IDirect3DPixelShader9 **ppShader)
{
#if RESHADE_ADDON
	if (pFunction == nullptr)
		return D3DERR_INVALIDCALL;

	// First token should be version token (https://docs.microsoft.com/windows-hardware/drivers/display/version-token), so verify its shader type
	assert((pFunction[0] >> 16) == 0xFFFF);

	reshade::api::shader_desc desc = {};
	desc.code = pFunction;

	// Find the end token to calculate token stream size (https://docs.microsoft.com/windows-hardware/drivers/display/end-token)
	desc.code_size = sizeof(DWORD);
	for (int i = 0; pFunction[i] != 0x0000FFFF; ++i)
		desc.code_size += sizeof(DWORD);

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::pixel_shader, 1, &desc }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		pFunction = static_cast<const DWORD *>(desc.code);
	}
#endif

	const HRESULT hr = _orig->CreatePixelShader(pFunction, ppShader);
	if (SUCCEEDED(hr))
	{
		assert(ppShader != nullptr);

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(*ppShader));
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9::CreatePixelShader failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShader(IDirect3DPixelShader9 *pShader)
{
	const HRESULT hr = _orig->SetPixelShader(pShader);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::pixel_shader, to_handle(pShader));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShader(IDirect3DPixelShader9 **ppShader)
{
	return _orig->GetPixelShader(ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount)
{
	const HRESULT hr = _orig->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::pixel,
			// See global pipeline layout specified in 'device_impl::on_init'
			_global_pipeline_layout, 5,
			StartRegister * 4,
			Vector4fCount * 4,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount)
{
	return _orig->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount)
{
	const HRESULT hr = _orig->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::pixel,
			// See global pipeline layout specified in 'device_impl::on_init'
			_global_pipeline_layout, 6,
			StartRegister * 4,
			Vector4iCount * 4,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount)
{
	return _orig->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT BoolCount)
{
	const HRESULT hr = _orig->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
#if RESHADE_ADDON >= 2
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::pixel,
			// See global pipeline layout specified in 'device_impl::on_init'
			_global_pipeline_layout, 7,
			StartRegister,
			BoolCount,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
{
	return _orig->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawRectPatch(UINT Handle, const float *pNumSegs, const D3DRECTPATCH_INFO *pRectPatchInfo)
{
	return _orig->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawTriPatch(UINT Handle, const float *pNumSegs, const D3DTRIPATCH_INFO *pTriPatchInfo)
{
	return _orig->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DeletePatch(UINT Handle)
{
	return _orig->DeletePatch(Handle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9 **ppQuery)
{
	return _orig->CreateQuery(Type, ppQuery);
}

HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetConvolutionMonoKernel(UINT width, UINT height, float *rows, float *columns)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->SetConvolutionMonoKernel(width, height, rows, columns);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ComposeRects(IDirect3DSurface9 *pSrc, IDirect3DSurface9 *pDst, IDirect3DVertexBuffer9 *pSrcRectDescs, UINT NumRects, IDirect3DVertexBuffer9 *pDstRectDescs, D3DCOMPOSERECTSOP Operation, int Xoffset, int Yoffset)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->ComposeRects(pSrc, pDst, pSrcRectDescs, NumRects, pDstRectDescs, Operation, Xoffset, Yoffset);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::PresentEx(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
	assert(_extended_interface);

	// Skip when no presentation is requested
	if (((dwFlags & D3DPRESENT_DONOTFLIP) == 0) &&
		// Also skip when the same frame is presented multiple times
		((dwFlags & D3DPRESENT_DONOTWAIT) == 0 || !_implicit_swapchain->_was_still_drawing_last_frame))
	{
		assert(!_implicit_swapchain->_was_still_drawing_last_frame);

		_implicit_swapchain->on_present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	}

	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->PresentEx(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

	_implicit_swapchain->handle_device_loss(hr);

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetGPUThreadPriority(INT *pPriority)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->GetGPUThreadPriority(pPriority);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetGPUThreadPriority(INT Priority)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->SetGPUThreadPriority(Priority);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::WaitForVBlank(UINT iSwapChain)
{
	if (iSwapChain != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Access to multi-head swap chain at index %u is unsupported.", iSwapChain);
		return D3DERR_INVALIDCALL;
	}

	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->WaitForVBlank(0);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CheckResourceResidency(IDirect3DResource9 **pResourceArray, UINT32 NumResources)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->CheckResourceResidency(pResourceArray, NumResources);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetMaximumFrameLatency(UINT MaxLatency)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->SetMaximumFrameLatency(MaxLatency);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetMaximumFrameLatency(UINT *pMaxLatency)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->GetMaximumFrameLatency(pMaxLatency);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CheckDeviceState(HWND hDestinationWindow)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->CheckDeviceState(hDestinationWindow);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateRenderTargetEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	assert(_extended_interface);

#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_SURFACE, Usage, D3DPOOL_DEFAULT, MultiSample, MultisampleQuality, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, 1, Lockable, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::render_target))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, &Lockable, _caps);

	const HRESULT hr = ((desc.usage & reshade::api::resource_usage::shader_resource) != 0) && !Lockable ?
		create_surface_replacement(internal_desc, ppSurface, pSharedHandle) :
		static_cast<IDirect3DDevice9Ex *>(_orig)->CreateRenderTargetEx(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.MultiSampleType, internal_desc.MultiSampleQuality, Lockable, ppSurface, pSharedHandle, internal_desc.Usage);
#else
	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->CreateRenderTargetEx(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle, Usage);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;
#endif
#if RESHADE_ADDON >= 2
		const auto device_proxy = this;
		surface->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

		if (Lockable)
		{
			if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
				reshade::hooks::install("IDirect3DSurface9::LockRect", reshade::hooks::vtable_from_instance(surface), 13, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_LockRect));
			if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
				reshade::hooks::install("IDirect3DSurface9::UnlockRect", reshade::hooks::vtable_from_instance(surface), 14, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_UnlockRect));
		}
#endif
#if RESHADE_ADDON
		// In case surface was replaced with a texture resource
		com_ptr<IDirect3DResource9> resource;
		if (SUCCEEDED(surface->GetContainer(IID_PPV_ARGS(&resource))))
			desc.type = reshade::api::resource_type::texture_2d;
		else
			resource = surface;

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::render_target,
			to_handle(resource.get()));
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			to_handle(resource.get()),
			reshade::api::resource_usage::render_target,
			reshade::api::resource_view_desc(desc.texture.samples > 1 ? reshade::api::resource_view_type::texture_2d_multisample : reshade::api::resource_view_type::texture_2d, desc.texture.format, 0, 1, 0, 1),
			to_handle(surface));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
		{
			register_destruction_callback_d3d9(resource.get(), [this, resource = resource.get()]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			});
		}
		if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
		{
			register_destruction_callback_d3d9(surface, [this, surface]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
			}, surface == resource ? 1 : 0);
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9Ex::CreateRenderTargetEx failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	assert(_extended_interface);

#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_SURFACE, Usage, Pool, D3DMULTISAMPLE_NONE, 0, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, 1, FALSE, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, nullptr, _caps);

	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->CreateOffscreenPlainSurfaceEx(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.Pool, ppSurface, pSharedHandle, internal_desc.Usage);
#else
	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->CreateOffscreenPlainSurfaceEx(Width, Height, Format, Pool, ppSurface, pSharedHandle, Usage);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;
#endif
#if RESHADE_ADDON >= 2
		const auto device_proxy = this;
		surface->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("IDirect3DSurface9::LockRect", reshade::hooks::vtable_from_instance(surface), 13, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_LockRect));
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("IDirect3DSurface9::UnlockRect", reshade::hooks::vtable_from_instance(surface), 14, reinterpret_cast<reshade::hook::address>(&IDirect3DSurface9_UnlockRect));
#endif
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::render_target,
			reshade::api::resource { reinterpret_cast<uintptr_t>(surface) });
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			reshade::api::resource { reinterpret_cast<uintptr_t>(surface) },
			reshade::api::resource_usage::render_target,
			reshade::api::resource_view_desc(desc.texture.format),
			to_handle(surface));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
		{
			register_destruction_callback_d3d9(surface, [this, surface]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(surface) });
			});
		}
		if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
		{
			register_destruction_callback_d3d9(surface, [this, surface]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
			}, 1);
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9Ex::CreateOffscreenPlainSurfaceEx failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateDepthStencilSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	assert(_extended_interface);

#if RESHADE_ADDON
	D3DSURFACE_DESC old_desc = { Format, D3DRTYPE_SURFACE, Usage, D3DPOOL_DEFAULT, MultiSample, MultisampleQuality, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(old_desc, 1, FALSE, _caps, pSharedHandle != nullptr);

	D3DSURFACE_DESC internal_desc = old_desc;
	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::depth_stencil))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, nullptr, _caps);

	const HRESULT hr = ((desc.usage & reshade::api::resource_usage::shader_resource) != 0) ?
		create_surface_replacement(internal_desc, ppSurface, pSharedHandle) :
		static_cast<IDirect3DDevice9Ex *>(_orig)->CreateDepthStencilSurfaceEx(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.MultiSampleType, internal_desc.MultiSampleQuality, Discard, ppSurface, pSharedHandle, internal_desc.Usage);
#else
	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->CreateDepthStencilSurfaceEx(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle, Usage);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;
		*ppSurface = new Direct3DDepthStencilSurface9(this, surface, old_desc);

		// In case surface was replaced with a texture resource
		com_ptr<IDirect3DResource9> resource;
		if (SUCCEEDED(surface->GetContainer(IID_PPV_ARGS(&resource))))
			desc.type = reshade::api::resource_type::texture_2d;
		else
			resource = surface;

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::depth_stencil,
			to_handle(resource.get()));
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			to_handle(resource.get()),
			reshade::api::resource_usage::depth_stencil,
			reshade::api::resource_view_desc(desc.texture.samples > 1 ? reshade::api::resource_view_type::texture_2d_multisample : reshade::api::resource_view_type::texture_2d, desc.texture.format, 0, 1, 0, 1),
			to_handle(surface));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
		{
			register_destruction_callback_d3d9(resource.get(), [this, resource = resource.get()]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			});
		}
		if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
		{
			register_destruction_callback_d3d9(surface, [this, surface]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
			}, surface == resource ? 1 : 0);
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "IDirect3DDevice9Ex::CreateDepthStencilSurfaceEx failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ResetEx(D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode)
{
	assert(_extended_interface);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDirect3DDevice9Ex::ResetEx(this = %p, pPresentationParameters = %p, pFullscreenDisplayMode = %p) ...",
		this, pPresentationParameters, pFullscreenDisplayMode);

	if (pPresentationParameters == nullptr)
		return D3DERR_INVALIDCALL;

	D3DDISPLAYMODEEX fullscreen_mode = { sizeof(fullscreen_mode) };
	if (pFullscreenDisplayMode != nullptr)
		fullscreen_mode = *pFullscreenDisplayMode;
	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, fullscreen_mode, _d3d.get(), _cp.AdapterOrdinal, _cp.hFocusWindow);

	// Release all resources before performing reset
	_implicit_swapchain->on_reset(true);
	on_reset();

	assert(!g_in_d3d9_runtime && !g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->ResetEx(&pp, pp.Windowed ? nullptr : &fullscreen_mode);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;

	// Update output values (see https://docs.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9ex-resetex)
	pPresentationParameters->BackBufferWidth = pp.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = pp.BackBufferHeight;
	pPresentationParameters->BackBufferFormat = pp.BackBufferFormat;
	pPresentationParameters->BackBufferCount = pp.BackBufferCount;

	if (SUCCEEDED(hr))
	{
		on_init();
		_implicit_swapchain->on_init(true);
	}
	else
	{
		reshade::log::message(reshade::log::level::error, "IDirect3DDevice9Ex::ResetEx failed with error code %s!", reshade::log::hr_to_string(hr).c_str());

		// Initialize device implementation even when reset failed, so that 'init_device', 'init_command_list' and 'init_command_queue' events are still called
		on_init();
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDisplayModeEx(UINT iSwapChain, D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation)
{
	if (iSwapChain != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Access to multi-head swap chain at index %u is unsupported.", iSwapChain);
		return D3DERR_INVALIDCALL;
	}

	assert(_extended_interface);
	assert(_implicit_swapchain->_extended_interface);
	return static_cast<IDirect3DSwapChain9Ex *>(_implicit_swapchain)->GetDisplayModeEx(pMode, pRotation);
}

#if RESHADE_ADDON
void Direct3DDevice9::on_init()
{
	device_impl::on_init();

	reshade::invoke_addon_event<reshade::addon_event::init_device>(this);

	const reshade::api::pipeline_layout_param global_pipeline_layout_params[8] = {
		/* s# */ reshade::api::descriptor_range { 0, 0, 0, 4, reshade::api::shader_stage::vertex, 1, reshade::api::descriptor_type::sampler_with_resource_view }, // Vertex shaders only support 4 sampler slots (D3DVERTEXTEXTURESAMPLER0 - D3DVERTEXTEXTURESAMPLER3)
		/* s# */ reshade::api::descriptor_range { 0, 0, 0, _caps.MaxSimultaneousTextures, reshade::api::shader_stage::pixel, 1, reshade::api::descriptor_type::sampler_with_resource_view },
		// See https://docs.microsoft.com/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-vs-registers-vs-3-0
		/* vs_3_0 c# */ reshade::api::constant_range { UINT32_MAX, 0, 0, _caps.MaxVertexShaderConst * 4, reshade::api::shader_stage::vertex },
		/* vs_3_0 i# */ reshade::api::constant_range { UINT32_MAX, 0, 0, 16 * 4, reshade::api::shader_stage::vertex },
		/* vs_3_0 b# */ reshade::api::constant_range { UINT32_MAX, 0, 0, 16 * 1, reshade::api::shader_stage::vertex },
		// See https://docs.microsoft.com/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-ps-registers-ps-3-0
		/* ps_3_0 c# */ reshade::api::constant_range { UINT32_MAX, 0, 0, 224 * 4, reshade::api::shader_stage::pixel },
		/* ps_3_0 i# */ reshade::api::constant_range { UINT32_MAX, 0, 0,  16 * 4, reshade::api::shader_stage::pixel },
		/* ps_3_0 b# */ reshade::api::constant_range { UINT32_MAX, 0, 0,  16 * 1, reshade::api::shader_stage::pixel },
	};
	device_impl::create_pipeline_layout(static_cast<uint32_t>(std::size(global_pipeline_layout_params)), global_pipeline_layout_params, &_global_pipeline_layout);
	reshade::invoke_addon_event<reshade::addon_event::init_pipeline_layout>(this, static_cast<uint32_t>(std::size(global_pipeline_layout_params)), global_pipeline_layout_params, _global_pipeline_layout);

	reshade::invoke_addon_event<reshade::addon_event::init_command_list>(this);
	reshade::invoke_addon_event<reshade::addon_event::init_command_queue>(this);

	init_auto_depth_stencil();
}
void Direct3DDevice9::on_reset()
{
#if RESHADE_ADDON >= 2
	resize_primitive_up_buffers(0, 0, D3DFMT_UNKNOWN);
#endif

	reset_auto_depth_stencil();

	// Force add-ons to release all resources associated with this device before performing reset
	reshade::invoke_addon_event<reshade::addon_event::destroy_command_queue>(this);
	reshade::invoke_addon_event<reshade::addon_event::destroy_command_list>(this);

	for (DWORD i = 0; i < _caps.MaxSimultaneousTextures; ++i)
		_orig->SetTexture(i, nullptr);
	for (DWORD i = 0; i < _caps.MaxStreams; ++i)
		_orig->SetStreamSource(0, nullptr, 0, 0);
	_orig->SetIndices(nullptr);

	for (DWORD i = 0; i < _caps.NumSimultaneousRTs; ++i)
		_orig->SetRenderTarget(i, nullptr);
	// Release reference to the potentially replaced auto depth-stencil resource
	_orig->SetDepthStencilSurface(nullptr);

	reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline_layout>(this, _global_pipeline_layout);
	device_impl::destroy_pipeline_layout(_global_pipeline_layout);

	reshade::invoke_addon_event<reshade::addon_event::destroy_device>(this);

	device_impl::on_reset();
}

void Direct3DDevice9::init_auto_depth_stencil()
{
	assert(_auto_depth_stencil == nullptr);

	com_ptr<IDirect3DSurface9> auto_depth_stencil;
	if (FAILED(_orig->GetDepthStencilSurface(&auto_depth_stencil)))
		return;

	D3DSURFACE_DESC old_desc;
	auto_depth_stencil->GetDesc(&old_desc);
	auto desc = reshade::d3d9::convert_resource_desc(old_desc, 1, FALSE, _caps);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::depth_stencil))
	{
		D3DSURFACE_DESC new_desc = old_desc;
		reshade::d3d9::convert_resource_desc(desc, new_desc, nullptr, nullptr, _caps);

		// Need to replace auto depth-stencil if an add-on modified the description
		if (com_ptr<IDirect3DSurface9> auto_depth_stencil_replacement;
			SUCCEEDED(create_surface_replacement(new_desc, &auto_depth_stencil_replacement)))
			auto_depth_stencil = std::move(auto_depth_stencil_replacement);
	}

	IDirect3DSurface9 *const surface = auto_depth_stencil.release(); // This internal reference is later released in 'reset_auto_depth_stencil' below
	_auto_depth_stencil = new Direct3DDepthStencilSurface9(this, surface, old_desc);

	// The auto depth-stencil starts with a public reference count of zero
	_auto_depth_stencil->_ref = 0;

	// In case surface was replaced with a texture resource
	com_ptr<IDirect3DResource9> resource;
	if (SUCCEEDED(surface->GetContainer(IID_PPV_ARGS(&resource))))
		desc.type = reshade::api::resource_type::texture_2d;
	else
		resource = surface;

	reshade::invoke_addon_event<reshade::addon_event::init_resource>(
		this,
		desc,
		nullptr,
		reshade::api::resource_usage::depth_stencil,
		to_handle(resource.get()));
	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
		this,
		to_handle(resource.get()),
		reshade::api::resource_usage::depth_stencil,
		reshade::api::resource_view_desc(desc.texture.samples > 1 ? reshade::api::resource_view_type::texture_2d_multisample : reshade::api::resource_view_type::texture_2d, desc.texture.format, 0, 1, 0, 1),
		to_handle(surface));

	if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
	{
		register_destruction_callback_d3d9(resource.get(), [this, resource = resource.get()]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
		});
	}
	if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
	{
		register_destruction_callback_d3d9(surface, [this, surface]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
		}, surface == resource ? 1 : 0);
	}

	// Communicate default state to add-ons
	SetDepthStencilSurface(_auto_depth_stencil);
}
void Direct3DDevice9::reset_auto_depth_stencil()
{
	_current_depth_stencil.reset();

	if (_auto_depth_stencil == nullptr)
		return;

	assert(_auto_depth_stencil->_ref == 0);
	// Release the internal reference that was added in 'init_auto_depth_stencil' above
	_auto_depth_stencil->_orig->Release();

	delete _auto_depth_stencil;
	_auto_depth_stencil = nullptr;
}
#endif
#if RESHADE_ADDON >= 2
void Direct3DDevice9::resize_primitive_up_buffers(UINT vertex_buffer_size, UINT index_buffer_size, UINT index_size)
{
	const bool reset = (vertex_buffer_size == 0);

	reshade::api::resource_desc vertex_buffer_desc(0, reshade::api::memory_heap::cpu_to_gpu, reshade::api::resource_usage::vertex_buffer, reshade::api::resource_flags::dynamic);
	if (_primitive_up_vertex_buffer != 0)
		vertex_buffer_desc = device_impl::get_resource_desc(_primitive_up_vertex_buffer);

	// Initialize fake buffers for 'IDirect3DDevice9::DrawPrimitiveUP' and 'IDirect3DDevice9::DrawIndexedPrimitiveUP'
	if (reset || vertex_buffer_size > vertex_buffer_desc.buffer.size)
	{
		if (_primitive_up_vertex_buffer != 0)
		{
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, _primitive_up_vertex_buffer);
			device_impl::destroy_resource(_primitive_up_vertex_buffer);
		}

		vertex_buffer_desc.buffer.size = vertex_buffer_size;

		if (vertex_buffer_size != 0 &&
			device_impl::create_resource(vertex_buffer_desc, nullptr, reshade::api::resource_usage::vertex_buffer, &_primitive_up_vertex_buffer))
		{
			reshade::invoke_addon_event<reshade::addon_event::init_resource>(
				this,
				vertex_buffer_desc,
				nullptr,
				reshade::api::resource_usage::vertex_buffer,
				_primitive_up_vertex_buffer);
		}
	}

	reshade::api::resource_desc index_buffer_desc(0, reshade::api::memory_heap::cpu_to_gpu, reshade::api::resource_usage::index_buffer, reshade::api::resource_flags::dynamic);
	if (_primitive_up_index_buffer != 0)
		index_buffer_desc = device_impl::get_resource_desc(_primitive_up_index_buffer);

	if (reset || index_buffer_size > index_buffer_desc.buffer.size || index_size != index_buffer_desc.buffer.stride)
	{
		if (_primitive_up_index_buffer != 0)
		{
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, _primitive_up_index_buffer);
			device_impl::destroy_resource(_primitive_up_index_buffer);
		}

		index_buffer_desc.buffer.size = index_buffer_size;
		index_buffer_desc.buffer.stride = index_size;

		if (index_buffer_size != 0 &&
			device_impl::create_resource(index_buffer_desc, nullptr, reshade::api::resource_usage::index_buffer, &_primitive_up_vertex_buffer))
		{
			reshade::invoke_addon_event<reshade::addon_event::init_resource>(
				this,
				index_buffer_desc,
				nullptr,
				reshade::api::resource_usage::index_buffer,
				_primitive_up_index_buffer);
		}
	}
}
#endif
