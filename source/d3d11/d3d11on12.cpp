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
	// The Steam overlay creates a D3D11on12 device during 'IDXGISwapChain::Present', which can be ignored, so return early in that case
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(D3D11On12CreateDevice)(
			pDevice, Flags, pFeatureLevels, FeatureLevels, ppCommandQueues, NumQueues, NodeMask, ppDevice, ppImmediateContext, pChosenFeatureLevel);

	LOG(INFO) << "Redirecting " << "D3D11On12CreateDevice" << '('
		<<   "pDevice = " << pDevice
		<< ", Flags = " << std::hex << Flags << std::dec
		<< ", pFeatureLevels = " << pFeatureLevels
		<< ", FeatureLevels = " << FeatureLevels
		<< ", ppCommandQueues = " << ppCommandQueues
		<< ", NumQueues = " << NumQueues
		<< ", NodeMask = " << NodeMask
		<< ", ppDevice = " << ppDevice
		<< ", ppImmediateContext = " << ppImmediateContext
		<< ", pChosenFeatureLevel = " << pChosenFeatureLevel
		<< ')' << " ...";

	com_ptr<D3D12Device> device_proxy_12;
	if (SUCCEEDED(pDevice->QueryInterface(&device_proxy_12)))
	{
		pDevice = device_proxy_12->_orig;
	}
	else
	{
		LOG(WARN) << "Skipping D3D11on12 device because it was created without a proxy Direct3D 12 device.";

		return reshade::hooks::call(D3D11On12CreateDevice)(pDevice, Flags, pFeatureLevels, FeatureLevels, ppCommandQueues, NumQueues, NodeMask, ppDevice, ppImmediateContext, pChosenFeatureLevel);
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
	HRESULT hr = reshade::hooks::call(D3D11On12CreateDevice)(pDevice, Flags, pFeatureLevels, FeatureLevels, command_queues.p, NumQueues, NodeMask, ppDevice, nullptr, &FeatureLevel);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		LOG(WARN) << "D3D11On12CreateDevice" << " failed with error code " << hr << '.';
		return hr;
	}

	if (pChosenFeatureLevel != nullptr) // Copy feature level value to application variable if the argument exists
		*pChosenFeatureLevel = FeatureLevel;

	LOG(INFO) << "Using feature level " << std::hex << FeatureLevel << std::dec << '.';

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
	LOG(DEBUG) << "Returning " << "ID3D11Device0" << " object " << static_cast<ID3D11Device *>(device_proxy) << " (" << device_proxy->_orig << ") and " <<
		"IDXGIDevice1" << " object " << static_cast<IDXGIDevice1 *>(device_proxy) << " (" << static_cast<DXGIDevice *>(device_proxy)->_orig << ").";
#endif
	*ppDevice = device_proxy;

	if (ppImmediateContext != nullptr)
	{
		device_proxy->GetImmediateContext(ppImmediateContext);
	}

	return hr;
}
