/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <dxgi1_6.h>
#include <mutex>

struct D3D10Device;
struct D3D11Device;
struct D3D12CommandQueue;
namespace reshade::api { enum class device_api; struct swapchain; }

MIDL_INTERFACE("8C803E30-9E41-4DDF-B206-46F28E90E405") IDXGISwapChainTest : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) = 0;
	virtual ULONG   STDMETHODCALLTYPE AddRef() = 0;
	virtual ULONG   STDMETHODCALLTYPE Release() = 0;

	virtual bool    STDMETHODCALLTYPE HasProxyFrontBufferSurface() = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFrameStatisticsTest(struct DXGI_FRAME_STATISTICS_TEST *) = 0;
	virtual void    STDMETHODCALLTYPE EmulateXBOXBehavior(BOOL) = 0;
	virtual DXGI_COLOR_SPACE_TYPE STDMETHODCALLTYPE GetColorSpace1() = 0;
	virtual void    STDMETHODCALLTYPE GetBufferLayoutInfoTest(struct DXGI_BUFFER_LAYOUT_INFO_TEST *) = 0;
	virtual void *  STDMETHODCALLTYPE GetDFlipOutput() = 0;
	virtual UINT    STDMETHODCALLTYPE GetBackBufferImplicitRotationCount() = 0;
};

struct DECLSPEC_UUID("1F445F9F-9887-4C4C-9055-4E3BADAFCCA8") DXGISwapChain final : IDXGISwapChain4
{
	DXGISwapChain(IDXGIFactory *factory, D3D10Device *device, IDXGISwapChain  *original);
	DXGISwapChain(IDXGIFactory *factory, D3D10Device *device, IDXGISwapChain1 *original);
	DXGISwapChain(IDXGIFactory *factory, D3D11Device *device, IDXGISwapChain  *original);
	DXGISwapChain(IDXGIFactory *factory, D3D11Device *device, IDXGISwapChain1 *original);
	DXGISwapChain(IDXGIFactory *factory, D3D12CommandQueue *command_queue, IDXGISwapChain3 *original);
	~DXGISwapChain();

	DXGISwapChain(const DXGISwapChain &) = delete;
	DXGISwapChain &operator=(const DXGISwapChain &) = delete;

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
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
	BOOL    STDMETHODCALLTYPE IsTemporaryMonoSupported() override;
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
	HANDLE  STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override;
	HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F *pMatrix) override;
	HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F *pMatrix) override;
	#pragma endregion
	#pragma region IDXGISwapChain3
	UINT    STDMETHODCALLTYPE GetCurrentBackBufferIndex() override;
	HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT *pColorSpaceSupport) override;
	HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) override;
	HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT *pCreationNodeMask, IUnknown *const *ppPresentQueue) override;
	#pragma endregion
	#pragma region IDXGISwapChain4
	HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void *pMetaData) override;
	#pragma endregion

	void on_init([[maybe_unused]] bool resize);
	void on_reset([[maybe_unused]] bool resize);
	void on_present(UINT flags, [[maybe_unused]] const DXGI_PRESENT_PARAMETERS *params = nullptr);
	void on_finish_present(HRESULT hr);

	bool check_and_upgrade_interface(REFIID riid);

	IDXGISwapChain *_orig;
	LONG _ref = 1;
	unsigned short _interface_version;

	IUnknown *const _direct3d_device;
	// The GOG Galaxy overlay scans the swap chain object memory for the D3D12 command queue, but fails if it cannot find at least two occurences of it.
	// In that case it falls back to using the first (normal priority) direct command queue that 'ID3D12CommandQueue::ExecuteCommandLists' is called on,
	// but if this is not the queue the swap chain was created with (DLSS Frame Generation e.g. creates a separate high priority one for presentation), D3D12 removes the device.
	// Instead spoof a more similar layout to the original 'CDXGISwapChain' implementation, so that the GOG Galaxy overlay successfully extracts and
	// later uses these command queue pointer offsets directly (the second of which is indexed with the back buffer index), ensuring the correct queue is used.
	IUnknown *const _direct3d_command_queue, *_direct3d_command_queue_per_back_buffer[DXGI_MAX_SWAP_CHAIN_BUFFERS] = {};
	const reshade::api::device_api _direct3d_version;

	IDXGIFactory *const _parent_factory;

	std::recursive_mutex _impl_mutex;
	reshade::api::swapchain *const _impl;
	bool _is_initialized = false;
	bool _was_still_drawing_last_frame = false;

#if RESHADE_ADDON
	UINT _sync_interval = UINT_MAX;
	BOOL _current_fullscreen_state = -1;
	bool _is_desc_modified = false;
	DXGI_SWAP_CHAIN_DESC _orig_desc = {};
#endif
};
