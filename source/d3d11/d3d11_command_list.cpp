/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d11_device.hpp"
#include "d3d11_command_list.hpp"
#include "dll_log.hpp"
#include "addon_manager.hpp"

D3D11CommandList::D3D11CommandList(D3D11Device *device, ID3D11CommandList *original) :
	command_list_impl(device, original),
	_device(device)
{
	assert(_orig != nullptr && _device != nullptr);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::init_command_list>(this);
#endif
}
D3D11CommandList::~D3D11CommandList()
{
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_command_list>(this);
#endif
}

bool D3D11CommandList::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(ID3D11DeviceChild) ||
		riid == __uuidof(ID3D11CommandList))
		return true;

	return false;
}

HRESULT STDMETHODCALLTYPE D3D11CommandList::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

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

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D11CommandList::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D11CommandList::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
	{
		_orig->Release();
		return ref;
	}

	const auto orig = _orig;
#if 0
	reshade::log::message(reshade::log::level::debug, "Destroying ID3D11CommandList object %p (%p).", this, orig);
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		reshade::log::message(reshade::log::level::warning, "Reference count for ID3D11CommandList object %p (%p) is inconsistent (%lu).", this, orig, ref_orig);
	return 0;
}

void    STDMETHODCALLTYPE D3D11CommandList::GetDevice(ID3D11Device **ppDevice)
{
	_device->AddRef();
	*ppDevice = _device;
}
HRESULT STDMETHODCALLTYPE D3D11CommandList::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D11CommandList::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D11CommandList::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}

UINT    STDMETHODCALLTYPE D3D11CommandList::GetContextFlags()
{
	return _orig->GetContextFlags();
}
