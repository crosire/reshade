#pragma once

#include "Hooks\D3D10.hpp"
#include "Hooks\D3D11.hpp"
#include "Hooks\D3D12.hpp"

#include <dxgi1_4.h>

struct __declspec(uuid("CB285C3B-3677-4332-98C7-D6339B9782B1"))
DXGIDevice : public IDXGIDevice3
{
	DXGIDevice(IDXGIDevice *original, D3D10Device *direct3DDevice) : mRef(1), mOrig(original), mInterfaceVersion(0), mDirect3DDevice(direct3DDevice)
	{
		assert(original != nullptr);
		assert(direct3DDevice != nullptr);
	}
	DXGIDevice(IDXGIDevice *original, D3D11Device *direct3DDevice) : mRef(1), mOrig(original), mInterfaceVersion(0), mDirect3DDevice(direct3DDevice)
	{
		assert(original != nullptr);
		assert(direct3DDevice != nullptr);
	}

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

	LONG mRef;
	IDXGIDevice *mOrig;
	unsigned int mInterfaceVersion;
	IUnknown *const mDirect3DDevice;

private:
	DXGIDevice(const DXGIDevice &) = delete;
	DXGIDevice &operator=(const DXGIDevice &) = delete;
};
struct __declspec(uuid("1F445F9F-9887-4C4C-9055-4E3BADAFCCA8"))
DXGISwapChain : public IDXGISwapChain3
{
	DXGISwapChain(D3D10Device *device, IDXGISwapChain  *original, const std::shared_ptr<ReShade::Runtime> &runtime) : mRef(1), mOrig(original), mInterfaceVersion(0), mDirect3DDevice(device), mDirect3DVersion(10), mRuntime(runtime)
	{
		assert(device != nullptr);
		assert(original != nullptr);
	}
	DXGISwapChain(D3D11Device *device, IDXGISwapChain  *original, const std::shared_ptr<ReShade::Runtime> &runtime) : mRef(1), mOrig(original), mInterfaceVersion(0), mDirect3DDevice(device), mDirect3DVersion(11), mRuntime(runtime)
	{
		assert(device != nullptr);
		assert(original != nullptr);
	}
	DXGISwapChain(D3D12CommandQueue *device, IDXGISwapChain  *original, const std::shared_ptr<ReShade::Runtime> &runtime) : mRef(1), mOrig(original), mInterfaceVersion(0), mDirect3DDevice(device), mDirect3DVersion(12), mRuntime(runtime)
	{
		assert(device != nullptr);
		assert(original != nullptr);
	}
	DXGISwapChain(D3D10Device *device, IDXGISwapChain1 *original, const std::shared_ptr<ReShade::Runtime> &runtime) : mRef(1), mOrig(original), mInterfaceVersion(1), mDirect3DDevice(device), mDirect3DVersion(10), mRuntime(runtime)
	{
		assert(device != nullptr);
		assert(original != nullptr);
	}
	DXGISwapChain(D3D11Device *device, IDXGISwapChain1 *original, const std::shared_ptr<ReShade::Runtime> &runtime) : mRef(1), mOrig(original), mInterfaceVersion(1), mDirect3DDevice(device), mDirect3DVersion(11), mRuntime(runtime)
	{
		assert(device != nullptr);
		assert(original != nullptr);
	}
	DXGISwapChain(D3D12CommandQueue *device, IDXGISwapChain1 *original, const std::shared_ptr<ReShade::Runtime> &runtime) : mRef(1), mOrig(original), mInterfaceVersion(1), mDirect3DDevice(device), mDirect3DVersion(12), mRuntime(runtime)
	{
		assert(device != nullptr);
		assert(original != nullptr);
	}

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

	LONG mRef;
	IDXGISwapChain *mOrig;
	unsigned int mInterfaceVersion;
	IUnknown *const mDirect3DDevice;
	const unsigned int mDirect3DVersion;
	std::shared_ptr<ReShade::Runtime> mRuntime;

private:
	DXGISwapChain(const DXGISwapChain &) = delete;
	DXGISwapChain &operator=(const DXGISwapChain &) = delete;
};
