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
		_runtime(runtime) {}
	DXGISwapChain(D3D10Device *device, IDXGISwapChain1 *original, const std::shared_ptr<reshade::runtime> &runtime) :
		_orig(original),
		_interface_version(1),
		_direct3d_device(device),
		_direct3d_version(10),
		_runtime(runtime) {}
	DXGISwapChain(D3D11Device *device, IDXGISwapChain *original, const std::shared_ptr<reshade::runtime> &runtime) :
		_orig(original),
		_interface_version(0),
		_direct3d_device(device),
		_direct3d_version(11),
		_runtime(runtime) {}
	DXGISwapChain(D3D11Device *device, IDXGISwapChain1 *original, const std::shared_ptr<reshade::runtime> &runtime) :
		_orig(original),
		_interface_version(1),
		_direct3d_device(device),
		_direct3d_version(11),
		_runtime(runtime) {}

	DXGISwapChain(const DXGISwapChain &) = delete;
	DXGISwapChain &operator=(const DXGISwapChain &) = delete;

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual   ULONG STDMETHODCALLTYPE AddRef() override;
	virtual   ULONG STDMETHODCALLTYPE Release() override;

	#pragma region IDXGIObject
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) override;
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) override;
	HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) override;
	#pragma endregion
	#pragma region IDXGIDeviceSubObject
	HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **ppDevice) override;
	#pragma endregion
	#pragma region IDXGISwapChain
	HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags) override;
	HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void **ppSurface) override;
	HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget) override;
	HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **ppTarget) override;
	HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc) override;
	HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) override;
	HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters) override;
	HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput **ppOutput) override;
	HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats) override;
	HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT *pLastPresentCount) override;
	#pragma endregion
	#pragma region IDXGISwapChain1
	HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc) override;
	HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc) override;
	HRESULT STDMETHODCALLTYPE GetHwnd(HWND *pHwnd) override;
	HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID refiid, void **ppUnk) override;
	HRESULT STDMETHODCALLTYPE Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters) override;
	   BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override;
	HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput) override;
	HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA *pColor) override;
	HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA *pColor) override;
	HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation) override;
	HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION *pRotation) override;
	#pragma endregion
	#pragma region IDXGISwapChain2
	HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height) override;
	HRESULT STDMETHODCALLTYPE GetSourceSize(UINT *pWidth, UINT *pHeight) override;
	HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override;
	HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency) override;
	 HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override;
	HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F *pMatrix) override;
	HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F *pMatrix) override;
	#pragma endregion
	#pragma region IDXGISwapChain3
	   UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override;
	HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT *pColorSpaceSupport) override;
	HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) override;
	HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT *pCreationNodeMask, IUnknown *const *ppPresentQueue) override;
	#pragma endregion
	#pragma region IDXGISwapChain4
	HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void *pMetaData) override;
	#pragma endregion

	void perform_present(UINT PresentFlags);

	void clear_drawcall_stats();

	LONG _ref = 1;
	IDXGISwapChain *_orig;
	unsigned int _interface_version;
	IUnknown *const _direct3d_device;
	const unsigned int _direct3d_version;
	std::shared_ptr<reshade::runtime> _runtime;
};
