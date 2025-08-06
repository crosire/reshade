/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dxgi_adapter.hpp"
#include "dxgi_device.hpp"
#include "dll_log.hpp"
#include "com_utils.hpp"
#include "com_ptr.hpp"

DXGIDevice::DXGIDevice(IDXGIDevice1 *original) :
	_orig(original),
	_interface_version(1)
{
	assert(_orig != nullptr);
}

bool DXGIDevice::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) || // This is the IID_IUnknown identity object
		riid == __uuidof(IDXGIObject))
		return true;

	static constexpr IID iid_lookup[] = {
		__uuidof(IDXGIDevice),
		__uuidof(IDXGIDevice1),
		__uuidof(IDXGIDevice2),
		__uuidof(IDXGIDevice3),
		__uuidof(IDXGIDevice4),
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
	HRESULT hr = _orig->GetParent(riid, ppParent);
	if (hr == S_OK)
	{
		IUnknown *const parent_unknown = static_cast<IUnknown *>(*ppParent);
		if (com_ptr<IDXGIAdapter> adapter;
			SUCCEEDED(parent_unknown->QueryInterface(&adapter)))
		{
			auto adapter_proxy = get_private_pointer_d3dx<DXGIAdapter>(adapter.get());
			if (adapter_proxy == nullptr)
			{
				adapter_proxy = new DXGIAdapter(adapter.get());
			}
			else
			{
				adapter_proxy->_ref++;
			}

			if (adapter_proxy->check_and_upgrade_interface(riid))
			{
#if RESHADE_VERBOSE_LOG
				reshade::log::message(reshade::log::level::debug, "DXGIDevice::GetParent returning IDXGIAdapter%hu object %p (%p).", adapter_proxy->_interface_version, adapter_proxy, adapter_proxy->_orig);
#endif

				*ppParent = adapter_proxy;
			}
			else // Do not hook object if we do not support the requested interface
			{
				reshade::log::message(reshade::log::level::warning, "Unknown interface %s in DXGIDevice::GetParent.", reshade::log::iid_to_string(riid).c_str());

				delete adapter_proxy; // Delete instead of release to keep reference count untouched
			}
		}
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIDevice::GetAdapter(IDXGIAdapter **pAdapter)
{
	HRESULT hr = _orig->GetAdapter(pAdapter);
	if (hr == S_OK)
	{
		IDXGIAdapter *const adapter = *pAdapter;
		auto adapter_proxy = get_private_pointer_d3dx<DXGIAdapter>(adapter);
		if (adapter_proxy == nullptr)
		{
			adapter_proxy = new DXGIAdapter(adapter);
		}
		else
		{
			adapter_proxy->_ref++;
		}
		*pAdapter = adapter_proxy;
	}
	return hr;
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
