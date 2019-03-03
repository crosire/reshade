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
		#pragma region Update to IDXGIDevice2 interface
		if (riid == __uuidof(IDXGIDevice2) && _interface_version < 2)
		{
			IDXGIDevice2 *device2 = nullptr;
			if (FAILED(_orig->QueryInterface(&device2)))
				return E_NOINTERFACE;

			_orig->Release();

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded IDXGIDevice" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << " object " << this << " to IDXGIDevice2.";
#endif
			_orig = device2;
			_interface_version = 2;
		}
		#pragma endregion
		#pragma region Update to IDXGIDevice3 interface
		if (riid == __uuidof(IDXGIDevice3) && _interface_version < 3)
		{
			IDXGIDevice3 *device3 = nullptr;
			if (FAILED(_orig->QueryInterface(&device3)))
				return E_NOINTERFACE;

			_orig->Release();

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded IDXGIDevice" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << " object " << this << " to IDXGIDevice3.";
#endif
			_orig = device3;
			_interface_version = 3;
		}
		#pragma endregion
		#pragma region Update to IDXGIDevice4 interface
		if (riid == __uuidof(IDXGIDevice4) && _interface_version < 4)
		{
			IDXGIDevice4 *device4 = nullptr;
			if (FAILED(_orig->QueryInterface(&device4)))
				return E_NOINTERFACE;

			_orig->Release();

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded IDXGIDevice" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << " object " << this << " to IDXGIDevice4.";
#endif
			_orig = device4;
			_interface_version = 4;
		}
		#pragma endregion

		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _direct3d_device->QueryInterface(riid, ppvObj);
}
  ULONG STDMETHODCALLTYPE DXGIDevice::AddRef()
{
	_ref++;

	return _orig->AddRef();
}
  ULONG STDMETHODCALLTYPE DXGIDevice::Release()
{
	ULONG ref = _orig->Release();

	if (--_ref == 0 || ref == 1)
	{
		assert(_ref <= 0);

		if (ref != 1)
			LOG(WARN) << "Reference count for IDXGIDevice" << _interface_version << " object " << this << " is inconsistent: " << ref << ", but expected 1.";

#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Destroyed IDXGIDevice" << _interface_version << " object " << this << '.';
#endif
		delete this;

		return 0;
	}

	return ref;
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
	return _orig->SetMaximumFrameLatency(MaxLatency);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetMaximumFrameLatency(UINT *pMaxLatency)
{
	return _orig->GetMaximumFrameLatency(pMaxLatency);
}

// IDXGIDevice2
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

// IDXGIDevice3
   void STDMETHODCALLTYPE DXGIDevice::Trim()
{
	assert(_interface_version >= 3);

	return static_cast<IDXGIDevice3 *>(_orig)->Trim();
}

// IDXGIDevice4
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
