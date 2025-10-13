/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dxgi_device.hpp"
#include "dxgi_adapter.hpp"
#include "dll_log.hpp"

DXGIDevice::DXGIDevice(IDXGIAdapter *adapter, IDXGIDevice1 *original) :
	_orig(original),
	_interface_version(1),
	_parent_adapter(adapter)
{
	assert(_orig != nullptr && _parent_adapter != nullptr);
	_parent_adapter->AddRef();
}
DXGIDevice::~DXGIDevice()
{
	_parent_adapter->Release();
}

bool DXGIDevice::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) || // This is the IID_IUnknown identity object
		riid == __uuidof(IDXGIObject))
		return true;

	static constexpr IID iid_lookup[] = {
		__uuidof(IDXGIDevice),  // {54EC77FA-1377-44E6-8C32-88FD5F44C84C}
		__uuidof(IDXGIDevice1), // {77DB970F-6276-48BA-BA28-070143b4392C}
		__uuidof(IDXGIDevice2), // {05008617-FBFD-4051-A790-144884b4f6A9}
		__uuidof(IDXGIDevice3), // {6007896C-3244-4AFD-BF18-A6D3BEDA5023}
		__uuidof(IDXGIDevice4), // {95B4F95F-D8DA-4CA4-9EE6-3B76D5968A10}
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
			reshade::log::message(reshade::log::level::debug, "Upgrading IDXGIDevice%hu object %p to IDXGIDevice%hu.", _interface_version, this, version);
#endif
			_orig->Release();
			_orig = static_cast<IDXGIDevice1 *>(new_interface);
			_interface_version = version;
		}

		return true;
	}

	return false;
}

HRESULT STDMETHODCALLTYPE DXGIDevice::GetParent(REFIID riid, void **ppParent)
{
	return _parent_adapter->QueryInterface(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE DXGIDevice::GetAdapter(IDXGIAdapter **pAdapter)
{
	return _parent_adapter->QueryInterface(pAdapter);
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
