/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d11_device.hpp"
#include "d3d11_device_context.hpp"
#include "d3d11on12_device.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "hook_manager.hpp"

extern thread_local bool g_in_dxgi_runtime;

extern "C" HRESULT WINAPI D3D11On12CreateDevice(IUnknown *pDevice, UINT Flags, CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, IUnknown *CONST *ppCommandQueues, UINT NumQueues, UINT NodeMask, ID3D11Device **ppDevice, ID3D11DeviceContext **ppImmediateContext, D3D_FEATURE_LEVEL *pChosenFeatureLevel)
{
	const auto trampoline = reshade::hooks::call(D3D11On12CreateDevice);

	// The Steam overlay creates a D3D11on12 device during 'IDXGISwapChain::Present', which can be ignored, so return early in that case
	if (g_in_dxgi_runtime)
		return trampoline(pDevice, Flags, pFeatureLevels, FeatureLevels, ppCommandQueues, NumQueues, NodeMask, ppDevice, ppImmediateContext, pChosenFeatureLevel);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting D3D11On12CreateDevice(pDevice = %p, Flags = %#x, pFeatureLevels = %p, FeatureLevels = %u, ppCommandQueues = %p, NumQueues = %u, NodeMask = %u, ppDevice = %p, ppImmediateContext = %p, pChosenFeatureLevel = %p) ...",
		pDevice, Flags, pFeatureLevels, FeatureLevels, ppCommandQueues, NumQueues, NodeMask, ppDevice, ppImmediateContext, pChosenFeatureLevel);

	com_ptr<D3D12Device> device_proxy_12;
	if (SUCCEEDED(pDevice->QueryInterface(&device_proxy_12)))
	{
		pDevice = device_proxy_12->_orig;
	}
	else
	{
		reshade::log::message(reshade::log::level::warning, "Skipping D3D11on12 device because it was created without a proxy Direct3D 12 device.");

		return trampoline(pDevice, Flags, pFeatureLevels, FeatureLevels, ppCommandQueues, NumQueues, NodeMask, ppDevice, ppImmediateContext, pChosenFeatureLevel);
	}

	temp_mem<IUnknown *> command_queues(NumQueues);
	for (UINT i = 0; i < NumQueues; ++i)
	{
		assert(ppCommandQueues[i] != nullptr);

		if (com_ptr<D3D12CommandQueue> command_queue_proxy;
			SUCCEEDED(ppCommandQueues[i]->QueryInterface(&command_queue_proxy)))
			command_queues[i] = command_queue_proxy->_orig;
		else
			command_queues[i] = ppCommandQueues[i];
	}

	// Use local feature level variable in case the application did not pass one in
	D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

	g_in_dxgi_runtime = true;
	HRESULT hr = trampoline(pDevice, Flags, pFeatureLevels, FeatureLevels, command_queues.p, NumQueues, NodeMask, ppDevice, nullptr, &FeatureLevel);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "D3D11On12CreateDevice failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	if (pChosenFeatureLevel != nullptr) // Copy feature level value to application variable if the argument exists
		*pChosenFeatureLevel = FeatureLevel;

	reshade::log::message(reshade::log::level::info, "Using feature level %x.", FeatureLevel);

	// It is valid for the device out parameter to be NULL if the application wants to check feature level support, so just return early in that case
	if (ppDevice == nullptr)
	{
		assert(ppImmediateContext == nullptr);
		return hr;
	}

	auto device = *ppDevice;
	// Query for the DXGI device, D3D11on12 device and immediate device context since we need to reference them in the proxy device
	com_ptr<IDXGIDevice1> dxgi_device;
	hr = device->QueryInterface(&dxgi_device);
	assert(SUCCEEDED(hr));

	ID3D11On12Device *d3d11on12_device = nullptr;
	hr = device->QueryInterface(&d3d11on12_device);
	assert(SUCCEEDED(hr));

	ID3D11DeviceContext *device_context = nullptr;
	device->GetImmediateContext(&device_context);

	const auto device_proxy = new D3D11Device(dxgi_device.get(), device);
	device_proxy->_d3d11on12_device = new D3D11On12Device(device_proxy, device_proxy_12.get(), d3d11on12_device);
	device_proxy->_immediate_context = new D3D11DeviceContext(device_proxy, device_context);

#if RESHADE_VERBOSE_LOG
	reshade::log::message(
		reshade::log::level::debug,
		"Returning ID3D11Device0 object %p (%p) and IDXGIDevice1 object %p (%p).",
		static_cast<ID3D11Device *>(device_proxy), device_proxy->_orig,
		static_cast<IDXGIDevice1 *>(device_proxy), static_cast<DXGIDevice *>(device_proxy)->_orig);
#endif
	*ppDevice = device_proxy;

	if (ppImmediateContext != nullptr)
	{
		device_proxy->GetImmediateContext(ppImmediateContext);
	}

	return hr;
}
