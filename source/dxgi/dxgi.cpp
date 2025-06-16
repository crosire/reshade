/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dxgi_swapchain.hpp"
#include "dxgi_factory.hpp"
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
	// External hooks may want to create a DXGI factory and rewrite the vtables
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(CreateDXGIFactory1)(riid, ppFactory);


	const HRESULT hr = reshade::hooks::call(CreateDXGIFactory1)(riid, ppFactory);
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "CreateDXGIFactory1 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	// The returned factory should alway implement the 'IDXGIFactory' base interface
	const auto factory = static_cast<IDXGIFactory *>(*ppFactory);

	const auto factory_proxy = new DXGIFactory(factory);

	// Upgrade to the actual interface version requested here
	if (factory_proxy->check_and_upgrade_interface(riid))
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(
			reshade::log::level::debug,
			"Returning IDXGIFactory%hu object %p (%p).",
			factory_proxy->_interface_version, factory_proxy, factory_proxy->_orig);
#endif
		*ppFactory = factory_proxy;
	}
	else // Do not hook object if we do not support the requested interface
	{
		reshade::log::message(reshade::log::level::warning, "Unknown interface %s in CreateDXGIFactory1.", reshade::log::iid_to_string(riid).c_str());

		delete factory_proxy; // Delete instead of release to keep reference count untouched
	}

	return hr;

}
extern "C" HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void **ppFactory)
{
	// External hooks may want to create a DXGI factory and rewrite the vtables (eg: Nvidia Smooth Motion)
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(CreateDXGIFactory2)(Flags, riid, ppFactory);

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

	const HRESULT hr = trampoline(Flags, riid, ppFactory);

	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "CreateDXGIFactory2 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	// The returned factory should alway implement the 'IDXGIFactory' base interface
	const auto factory = static_cast<IDXGIFactory *>(*ppFactory);

	const auto factory_proxy = new DXGIFactory(factory);

	// Upgrade to the actual interface version requested here
	if (factory_proxy->check_and_upgrade_interface(riid))
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(
			reshade::log::level::debug,
			"Returning IDXGIFactory%hu object %p (%p).",
			factory_proxy->_interface_version, factory_proxy, factory_proxy->_orig);
#endif
		*ppFactory = factory_proxy;
	}
	else // Do not hook object if we do not support the requested interface
	{
		reshade::log::message(reshade::log::level::warning, "Unknown interface %s in CreateDXGIFactory2.", reshade::log::iid_to_string(riid).c_str());

		delete factory_proxy; // Delete instead of release to keep reference count untouched
	}

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
