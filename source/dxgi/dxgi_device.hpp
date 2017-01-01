/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "dxgi.hpp"

struct DXGIDevice : IDXGIDevice3
{
	DXGIDevice(IDXGIDevice1 *original, D3D10Device *direct3d_device) :
		_orig(original),
		_interface_version(1),
		_direct3d_device(direct3d_device) { }
	DXGIDevice(IDXGIDevice1 *original, D3D11Device *direct3d_device) :
		_orig(original),
		_interface_version(1),
		_direct3d_device(direct3d_device) { }

	DXGIDevice(const DXGIDevice &) = delete;
	DXGIDevice &operator=(const DXGIDevice &) = delete;

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region IDXGIObject
	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) override;
	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) override;
	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) override;
	virtual HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) override;
	#pragma endregion
	#pragma region IDXGIDevice
	virtual HRESULT STDMETHODCALLTYPE GetAdapter(IDXGIAdapter **pAdapter) override;
	virtual HRESULT STDMETHODCALLTYPE CreateSurface(const DXGI_SURFACE_DESC *pDesc, UINT NumSurfaces, DXGI_USAGE Usage, const DXGI_SHARED_RESOURCE *pSharedResource, IDXGISurface **ppSurface) override;
	virtual HRESULT STDMETHODCALLTYPE QueryResourceResidency(IUnknown *const *ppResources, DXGI_RESIDENCY *pResidencyStatus, UINT NumResources) override;
	virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT Priority) override;
	virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT *pPriority) override;
	#pragma endregion
	#pragma region IDXGIDevice1
	virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override;
	virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency) override;
	#pragma endregion
	#pragma region IDXGIDevice2
	virtual HRESULT STDMETHODCALLTYPE OfferResources(UINT NumResources, IDXGIResource *const *ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority) override;
	virtual HRESULT STDMETHODCALLTYPE ReclaimResources(UINT NumResources, IDXGIResource *const *ppResources, BOOL *pDiscarded) override;
	virtual HRESULT STDMETHODCALLTYPE EnqueueSetEvent(HANDLE hEvent) override;
	#pragma endregion
	#pragma region IDXGIDevice3
	virtual void STDMETHODCALLTYPE Trim() override;
	#pragma endregion

	LONG _ref = 1;
	IDXGIDevice1 *_orig;
	unsigned int _interface_version;
	IUnknown *const _direct3d_device;
};
