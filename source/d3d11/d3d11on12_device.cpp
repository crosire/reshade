/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d11_device.hpp"
#include "d3d11on12_device.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "dll_log.hpp"

D3D11On12Device::D3D11On12Device(D3D11Device *device_11, D3D12Device *device_12, ID3D11On12Device *original) :
	_orig(original),
	_interface_version(0),
	_parent_device_11(device_11),
	_parent_device_12(device_12)
{
	assert(_orig != nullptr && _parent_device_11 != nullptr && _parent_device_12 != nullptr);

	_parent_device_12->AddRef();
}
D3D11On12Device::~D3D11On12Device()
{
	_parent_device_12->Release();
}

bool D3D11On12Device::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this))
		return true;

	static constexpr IID iid_lookup[] = {
		__uuidof(ID3D11On12Device),
		__uuidof(ID3D11On12Device1),
		__uuidof(ID3D11On12Device2),
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
			reshade::log::message(reshade::log::level::debug, "Upgrading ID3D11On12Device%hu object %p to ID3D11On12Device%hu.", _interface_version, this, version);
#endif
			_orig->Release();
			_orig = static_cast<ID3D11On12Device *>(new_interface);
			_interface_version = version;
		}

		return true;
	}

	return false;
}

HRESULT STDMETHODCALLTYPE D3D11On12Device::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	// IUnknown is handled by D3D11Device
	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	// Interface ID to query the original object from a proxy object
	constexpr GUID IID_UnwrappedObject = { 0x7f2c9a11, 0x3b4e, 0x4d6a, { 0x81, 0x2f, 0x5e, 0x9c, 0xd3, 0x7a, 0x1b, 0x42 } }; // {7F2C9A11-3B4E-4D6A-812F-5E9CD37A1B42}
	if (riid == IID_UnwrappedObject)
	{
		_orig->AddRef();
		*ppvObj = _orig;
		return S_OK;
	}

	return _parent_device_11->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D11On12Device::AddRef()
{
	return _parent_device_11->AddRef();
}
ULONG   STDMETHODCALLTYPE D3D11On12Device::Release()
{
	return _parent_device_11->Release();
}

HRESULT STDMETHODCALLTYPE D3D11On12Device::CreateWrappedResource(IUnknown *pResource12, const D3D11_RESOURCE_FLAGS *pFlags11, D3D12_RESOURCE_STATES InState, D3D12_RESOURCE_STATES OutState, REFIID riid, void **ppResource11)
{
	return _orig->CreateWrappedResource(pResource12, pFlags11, InState, OutState, riid, ppResource11);
}
void    STDMETHODCALLTYPE D3D11On12Device::ReleaseWrappedResources(ID3D11Resource *const *ppResources, UINT NumResources)
{
	_orig->ReleaseWrappedResources(ppResources, NumResources);
}
void    STDMETHODCALLTYPE D3D11On12Device::AcquireWrappedResources(ID3D11Resource *const *ppResources, UINT NumResources)
{
	_orig->AcquireWrappedResources(ppResources, NumResources);
}

HRESULT STDMETHODCALLTYPE D3D11On12Device::GetD3D12Device(REFIID riid, void **ppvDevice)
{
	assert(_interface_version >= 1);
	return _parent_device_12->QueryInterface(riid, ppvDevice);
}

HRESULT STDMETHODCALLTYPE D3D11On12Device::UnwrapUnderlyingResource(ID3D11Resource *pResource11, ID3D12CommandQueue *pCommandQueue, REFIID riid, void **ppvResource12)
{
	assert(pCommandQueue != nullptr);

	if (com_ptr<D3D12CommandQueue> command_queue_proxy;
		SUCCEEDED(pCommandQueue->QueryInterface(&command_queue_proxy)))
		pCommandQueue = command_queue_proxy->_orig;

	assert(_interface_version >= 2);
	return static_cast<ID3D11On12Device2 *>(_orig)->UnwrapUnderlyingResource(pResource11, pCommandQueue, riid, ppvResource12);
}
HRESULT STDMETHODCALLTYPE D3D11On12Device::ReturnUnderlyingResource(ID3D11Resource *pResource11, UINT NumSync, UINT64 *pSignalValues, ID3D12Fence **ppFences)
{
	assert(_interface_version >= 2);
	return static_cast<ID3D11On12Device2 *>(_orig)->ReturnUnderlyingResource(pResource11, NumSync, pSignalValues, ppFences);
}
