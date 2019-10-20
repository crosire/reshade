/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "d3d11_device.hpp"
#include "d3d11_command_list.hpp"
#include "runtime_d3d11.hpp"

D3D11CommandList::D3D11CommandList(D3D11Device *device, ID3D11CommandList *original) :
	_orig(original),
	_device(device) {
	assert(original != nullptr);
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

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D11CommandList::AddRef()
{
	++_ref;

	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE D3D11CommandList::Release()
{
	--_ref;

	// Decrease internal reference count and verify it against our own count
	const ULONG ref = _orig->Release();
	if (ref != 0 && _ref != 0)
		return ref;
	else if (ref != 0)
		LOG(WARN) << "Reference count for ID3D11CommandList object " << this << " is inconsistent: " << ref << ", but expected 0.";

	assert(_ref <= 0);
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroyed ID3D11CommandList object " << this << '.';
#endif
	delete this;

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
