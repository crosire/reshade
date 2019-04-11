/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "dxgi_device.hpp"
#include "dxgi_swapchain.hpp"
#include <assert.h>

DXGIDevice::DXGIDevice(IDXGIDevice1 *original, IUnknown *direct3d_device) :
	_orig(original),
	_interface_version(1),
	_direct3d_device(direct3d_device) {
	assert(original != nullptr);
}

bool DXGIDevice::check_and_upgrade_interface(REFIID riid)
{
	static const IID iid_lookup[] = {
		__uuidof(IDXGIDevice ),
		__uuidof(IDXGIDevice1),
		__uuidof(IDXGIDevice2),
		__uuidof(IDXGIDevice3),
		__uuidof(IDXGIDevice4),
	};

	for (unsigned int new_version = _interface_version + 1; new_version < ARRAYSIZE(iid_lookup); ++new_version)
	{
		if (riid == iid_lookup[new_version])
		{
			IUnknown *new_interface = nullptr;
			if (FAILED(_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface))))
				return false;
#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded IDXGIDevice" << _interface_version << " object " << this << " to IDXGIDevice" << new_version << '.';
#endif
			_orig->Release();
			_orig = static_cast<IDXGIDevice1 *>(new_interface);
			_interface_version = new_version;
			break;
		}
	}

	return true;
}

HRESULT STDMETHODCALLTYPE DXGIDevice::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) || // This is the IID_IUnknown identity object
		riid == __uuidof(IDXGIObject) ||
		riid == __uuidof(IDXGIDevice) ||
		riid == __uuidof(IDXGIDevice1) ||
		riid == __uuidof(IDXGIDevice2) ||
		riid == __uuidof(IDXGIDevice3) ||
		riid == __uuidof(IDXGIDevice4))
	{
		if (!check_and_upgrade_interface(riid))
			return E_NOINTERFACE;

		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _direct3d_device->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE DXGIDevice::AddRef()
{
	++_ref;

	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE DXGIDevice::Release()
{
	--_ref;

	const ULONG ref = _orig->Release();

	// The D3D device still holds a reference, so check reference count against one instead of zero
	if (ref > 1 && _ref != 0)
		return ref;
	else if (ref != 1)
		LOG(WARN) << "Reference count for IDXGIDevice" << _interface_version << " object " << this << " is inconsistent: " << ref << ", but expected 1.";

	assert(_ref <= 0);
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroyed IDXGIDevice" << _interface_version << " object " << this << '.';
#endif
	delete this;
	return 0;
}

HRESULT STDMETHODCALLTYPE DXGIDevice::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
{
	return _orig->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetParent(REFIID riid, void **ppParent)
{
	return _orig->GetParent(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE DXGIDevice::GetAdapter(IDXGIAdapter **pAdapter)
{
	return _orig->GetAdapter(pAdapter);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::CreateSurface(const DXGI_SURFACE_DESC *pDesc, UINT NumSurfaces, DXGI_USAGE Usage, const DXGI_SHARED_RESOURCE *pSharedResource, IDXGISurface **ppSurface)
{
	return _orig->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::QueryResourceResidency(IUnknown *const *ppResources, DXGI_RESIDENCY *pResidencyStatus, UINT NumResources)
{
	return _orig->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::SetGPUThreadPriority(INT Priority)
{
	return _orig->SetGPUThreadPriority(Priority);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetGPUThreadPriority(INT *pPriority)
{
	return _orig->GetGPUThreadPriority(pPriority);
}

HRESULT STDMETHODCALLTYPE DXGIDevice::SetMaximumFrameLatency(UINT MaxLatency)
{
	assert(_interface_version >= 1);
	return _orig->SetMaximumFrameLatency(MaxLatency);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetMaximumFrameLatency(UINT *pMaxLatency)
{
	assert(_interface_version >= 1);
	return _orig->GetMaximumFrameLatency(pMaxLatency);
}

HRESULT STDMETHODCALLTYPE DXGIDevice::OfferResources(UINT NumResources, IDXGIResource *const *ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIDevice2 *>(_orig)->OfferResources(NumResources, ppResources, Priority);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::ReclaimResources(UINT NumResources, IDXGIResource *const *ppResources, BOOL *pDiscarded)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIDevice2 *>(_orig)->ReclaimResources(NumResources, ppResources, pDiscarded);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::EnqueueSetEvent(HANDLE hEvent)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIDevice2 *>(_orig)->EnqueueSetEvent(hEvent);
}

void    STDMETHODCALLTYPE DXGIDevice::Trim()
{
	assert(_interface_version >= 3);
	return static_cast<IDXGIDevice3 *>(_orig)->Trim();
}

HRESULT STDMETHODCALLTYPE DXGIDevice::OfferResources1(UINT NumResources, IDXGIResource *const *ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority, UINT Flags)
{
	assert(_interface_version >= 4);
	return static_cast<IDXGIDevice4 *>(_orig)->OfferResources1(NumResources, ppResources, Priority, Flags);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::ReclaimResources1(UINT NumResources, IDXGIResource *const *ppResources, DXGI_RECLAIM_RESOURCE_RESULTS *pResults)
{
	assert(_interface_version >= 4);
	return static_cast<IDXGIDevice4 *>(_orig)->ReclaimResources1(NumResources, ppResources, pResults);
}
