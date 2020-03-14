/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <dxgi1_5.h>

struct DECLSPEC_UUID("CB285C3B-3677-4332-98C7-D6339B9782B1") DXGIDevice : IDXGIDevice4
{
	DXGIDevice(IDXGIDevice1 *original, IUnknown *direct3d_device);

	DXGIDevice(const DXGIDevice &) = delete;
	DXGIDevice &operator=(const DXGIDevice &) = delete;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;

	#pragma region IDXGIObject
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) override;
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) override;
	HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) override;
	#pragma endregion
	#pragma region IDXGIDevice
	HRESULT STDMETHODCALLTYPE GetAdapter(IDXGIAdapter **pAdapter) override;
	HRESULT STDMETHODCALLTYPE CreateSurface(const DXGI_SURFACE_DESC *pDesc, UINT NumSurfaces, DXGI_USAGE Usage, const DXGI_SHARED_RESOURCE *pSharedResource, IDXGISurface **ppSurface) override;
	HRESULT STDMETHODCALLTYPE QueryResourceResidency(IUnknown *const *ppResources, DXGI_RESIDENCY *pResidencyStatus, UINT NumResources) override;
	HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT Priority) override;
	HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT *pPriority) override;
	#pragma endregion
	#pragma region IDXGIDevice1
	HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override;
	HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency) override;
	#pragma endregion
	#pragma region IDXGIDevice2
	HRESULT STDMETHODCALLTYPE OfferResources(UINT NumResources, IDXGIResource *const *ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority) override;
	HRESULT STDMETHODCALLTYPE ReclaimResources(UINT NumResources, IDXGIResource *const *ppResources, BOOL *pDiscarded) override;
	HRESULT STDMETHODCALLTYPE EnqueueSetEvent(HANDLE hEvent) override;
	#pragma endregion
	#pragma region IDXGIDevice3
	void    STDMETHODCALLTYPE Trim() override;
	#pragma endregion
	#pragma region IDXGIDevice4
	HRESULT STDMETHODCALLTYPE OfferResources1(UINT NumResources, IDXGIResource *const *ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority, UINT Flags) override;
	HRESULT STDMETHODCALLTYPE ReclaimResources1(UINT NumResources, IDXGIResource *const *ppResources, DXGI_RECLAIM_RESOURCE_RESULTS *pResults) override;
	#pragma endregion

	bool check_and_upgrade_interface(REFIID riid);

	LONG _ref = 1;
	IDXGIDevice1 *_orig;
	unsigned int _interface_version;
	IUnknown *const _direct3d_device;
};
