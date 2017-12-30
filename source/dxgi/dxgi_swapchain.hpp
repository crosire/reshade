/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "dxgi.hpp"

struct DXGISwapChain : IDXGISwapChain4
{
	DXGISwapChain(D3D10Device *device, IDXGISwapChain *original, const std::shared_ptr<reshade::runtime> &runtime) :
		_orig(original),
		_interface_version(0),
		_direct3d_device(device),
		_direct3d_version(10),
		_runtime(runtime) { }
	DXGISwapChain(D3D10Device *device, IDXGISwapChain1 *original, const std::shared_ptr<reshade::runtime> &runtime) :
		_orig(original),
		_interface_version(1),
		_direct3d_device(device),
		_direct3d_version(10),
		_runtime(runtime) { }
	DXGISwapChain(D3D11Device *device, IDXGISwapChain *original, const std::shared_ptr<reshade::runtime> &runtime) :
		_orig(original),
		_interface_version(0),
		_direct3d_device(device),
		_direct3d_version(11),
		_runtime(runtime) { }
	DXGISwapChain(D3D11Device *device, IDXGISwapChain1 *original, const std::shared_ptr<reshade::runtime> &runtime) :
		_orig(original),
		_interface_version(1),
		_direct3d_device(device),
		_direct3d_version(11),
		_runtime(runtime) { }

	DXGISwapChain(const DXGISwapChain &) = delete;
	DXGISwapChain &operator=(const DXGISwapChain &) = delete;

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
	#pragma region IDXGIDeviceSubObject
	virtual HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **ppDevice) override;
	#pragma endregion
	#pragma region IDXGISwapChain
	virtual HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags) override;
	virtual HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void **ppSurface) override;
	virtual HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget) override;
	virtual HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **ppTarget) override;
	virtual HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc) override;
	virtual HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) override;
	virtual HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters) override;
	virtual HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput **ppOutput) override;
	virtual HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats) override;
	virtual HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT *pLastPresentCount) override;
	#pragma endregion
	#pragma region IDXGISwapChain1
	virtual HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc) override;
	virtual HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc) override;
	virtual HRESULT STDMETHODCALLTYPE GetHwnd(HWND *pHwnd) override;
	virtual HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID refiid, void **ppUnk) override;
	virtual HRESULT STDMETHODCALLTYPE Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters) override;
	virtual BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override;
	virtual HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput) override;
	virtual HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA *pColor) override;
	virtual HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA *pColor) override;
	virtual HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation) override;
	virtual HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION *pRotation) override;
	#pragma endregion
	#pragma region IDXGISwapChain2
	virtual HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height) override;
	virtual HRESULT STDMETHODCALLTYPE GetSourceSize(UINT *pWidth, UINT *pHeight) override;
	virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override;
	virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency) override;
	virtual HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override;
	virtual HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F *pMatrix) override;
	virtual HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F *pMatrix) override;
	#pragma endregion
	#pragma region IDXGISwapChain3
	virtual UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override;
	virtual HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT *pColorSpaceSupport) override;
	virtual HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) override;
	virtual HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT *pCreationNodeMask, IUnknown *const *ppPresentQueue) override;
	#pragma endregion
	#pragma region IDXGISwapChain4
	virtual HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void *pMetaData) override;
	#pragma endregion

	// local methods
	void perform_present(UINT PresentFlags);
	void perform_callcounter_cleanup();

	// fields
	LONG _ref = 1;
	IDXGISwapChain *_orig;
	unsigned int _interface_version;
	IUnknown *const _direct3d_device;
	const unsigned int _direct3d_version;
	std::shared_ptr<reshade::runtime> _runtime;
};
