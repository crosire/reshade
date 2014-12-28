#include "Log.hpp"
#include "HookManager.hpp"
#include "Runtimes\RuntimeD3D11.hpp"

#include <d3d11.h>

// -----------------------------------------------------------------------------------------------------

extern const GUID sRuntimeGUID;

// -----------------------------------------------------------------------------------------------------

// ID3D11DepthStencilView
ULONG STDMETHODCALLTYPE ID3D11DepthStencilView_Release(ID3D11DepthStencilView *pDepthStencilView)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11DepthStencilView_Release);

	ID3D11Device *device = nullptr;
	pDepthStencilView->GetDevice(&device);

	assert(device != nullptr);

	const ULONG ref = trampoline(pDepthStencilView);

	if (ref == 0)
	{
		ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);

		if (SUCCEEDED(device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
		{
			runtime->OnDeleteDepthStencilView(pDepthStencilView);
		}
	}

	device->Release();

	return ref;
}

// ID3D11DeviceContext
void STDMETHODCALLTYPE ID3D11DeviceContext_DrawIndexed(ID3D11DeviceContext *pDeviceContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11DeviceContext_DrawIndexed);

	ID3D11Device *device = nullptr;
	pDeviceContext->GetDevice(&device);

	assert(device != nullptr);

	ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->OnDrawInternal(pDeviceContext, IndexCount);
	}

	device->Release();

	trampoline(pDeviceContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_Draw(ID3D11DeviceContext *pDeviceContext, UINT VertexCount, UINT StartVertexLocation)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11DeviceContext_Draw);

	ID3D11Device *device = nullptr;
	pDeviceContext->GetDevice(&device);

	assert(device != nullptr);

	ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->OnDrawInternal(pDeviceContext, VertexCount);
	}

	device->Release();

	trampoline(pDeviceContext, VertexCount, StartVertexLocation);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_DrawIndexedInstanced(ID3D11DeviceContext *pDeviceContext, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11DeviceContext_DrawIndexedInstanced);

	ID3D11Device *device = nullptr;
	pDeviceContext->GetDevice(&device);

	assert(device != nullptr);

	ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->OnDrawInternal(pDeviceContext, IndexCountPerInstance * InstanceCount);
	}

	device->Release();

	trampoline(pDeviceContext, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_DrawInstanced(ID3D11DeviceContext *pDeviceContext, UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11DeviceContext_DrawInstanced);

	ID3D11Device *device = nullptr;
	pDeviceContext->GetDevice(&device);

	assert(device != nullptr);

	ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->OnDrawInternal(pDeviceContext, VertexCountPerInstance * InstanceCount);
	}

	device->Release();

	trampoline(pDeviceContext, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_OMSetRenderTargets(ID3D11DeviceContext *pDeviceContext, UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11DeviceContext_OMSetRenderTargets);

	if (pDepthStencilView != nullptr)
	{
		ID3D11Device *device = nullptr;
		pDeviceContext->GetDevice(&device);

		assert(device != nullptr);

		ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);

		if (SUCCEEDED(device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
		{
			runtime->OnSetDepthStencilView(pDepthStencilView);
		}

		device->Release();
	}

	trampoline(pDeviceContext, NumViews, ppRenderTargetViews, pDepthStencilView);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews(ID3D11DeviceContext *pDeviceContext, UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews);

	if (pDepthStencilView != nullptr)
	{
		ID3D11Device *device = nullptr;
		pDeviceContext->GetDevice(&device);

		assert(device != nullptr);

		ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);

		if (SUCCEEDED(device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
		{
			runtime->OnSetDepthStencilView(pDepthStencilView);
		}

		device->Release();
	}

	trampoline(pDeviceContext, NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_CopyResource(ID3D11DeviceContext *pDeviceContext, ID3D11Resource *pDstResource, ID3D11Resource *pSrcResource)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11DeviceContext_CopyResource);

	ID3D11Device *device = nullptr;
	pDeviceContext->GetDevice(&device);

	assert(device != nullptr);

	ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->OnCopyResource(pDstResource, pSrcResource);
	}

	device->Release();

	trampoline(pDeviceContext, pDstResource, pSrcResource);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_ClearDepthStencilView(ID3D11DeviceContext *pDeviceContext, ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11DeviceContext_ClearDepthStencilView);

	ID3D11Device *device = nullptr;
	pDeviceContext->GetDevice(&device);

	assert(device != nullptr);

	ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->OnClearDepthStencilView(pDepthStencilView);
	}

	device->Release();

	trampoline(pDeviceContext, pDepthStencilView, ClearFlags, Depth, Stencil);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_OMGetRenderTargets(ID3D11DeviceContext *pDeviceContext, UINT NumViews, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11DeviceContext_OMGetRenderTargets);

	trampoline(pDeviceContext, NumViews, ppRenderTargetViews, ppDepthStencilView);

	if (ppDepthStencilView != nullptr)
	{
		ID3D11Device *device = nullptr;
		pDeviceContext->GetDevice(&device);

		assert(device != nullptr);

		ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);

		if (SUCCEEDED(device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
		{
			runtime->OnGetDepthStencilView(*ppDepthStencilView);
		}

		device->Release();
	}
}
void STDMETHODCALLTYPE ID3D11DeviceContext_OMGetRenderTargetsAndUnorderedAccessViews(ID3D11DeviceContext *pDeviceContext, UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11DeviceContext_OMGetRenderTargetsAndUnorderedAccessViews);

	if (ppDepthStencilView != nullptr)
	{
		ID3D11Device *device = nullptr;
		pDeviceContext->GetDevice(&device);

		assert(device != nullptr);

		ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);

		if (SUCCEEDED(device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
		{
			runtime->OnGetDepthStencilView(*ppDepthStencilView);
		}

		device->Release();
	}

	trampoline(pDeviceContext, NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
}

// ID3D11Device
HRESULT STDMETHODCALLTYPE ID3D11Device_CreateDepthStencilView(ID3D11Device *pDevice, ID3D11Resource *pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D11DepthStencilView **ppDepthStencilView)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D11Device_CreateDepthStencilView);

	const HRESULT hr = trampoline(pDevice, pResource, pDesc, ppDepthStencilView);

	ReShade::Runtimes::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(hr) && SUCCEEDED(pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		ReShade::Hooks::Install(VTABLE(*ppDepthStencilView), 2, reinterpret_cast<ReShade::Hook::Function>(&ID3D11DepthStencilView_Release));

		runtime->OnCreateDepthStencilView(pResource, *ppDepthStencilView);
	}

	return hr;
}

// D3D11
EXPORT HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext)
{
	LOG(INFO) << "Redirecting '" << "D3D11CreateDevice" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << pFeatureLevels << ", " << FeatureLevels << ", " << SDKVersion << ", " << ppDevice << ", " << pFeatureLevel << ", " << ppImmediateContext << ")' ...";
	LOG(INFO) << "> Passing on to 'D3D11CreateDeviceAndSwapChain':";

	return D3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, nullptr, nullptr, ppDevice, pFeatureLevel, ppImmediateContext);
}
EXPORT HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext)
{
	LOG(INFO) << "Redirecting '" << "D3D11CreateDeviceAndSwapChain" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << pFeatureLevels << ", " << FeatureLevels << ", " << SDKVersion << ", " << pSwapChainDesc << ", " << ppSwapChain << ", " << ppDevice << ", " << pFeatureLevel << ", " << ppImmediateContext << ")' ...";

#ifdef _DEBUG
	Flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	ID3D11DeviceContext *pImmediateContext = nullptr;

	const HRESULT hr = ReShade::Hooks::Call(&D3D11CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, &pImmediateContext);

	if (ppImmediateContext != nullptr)
	{
		*ppImmediateContext = pImmediateContext;
	}

	if (SUCCEEDED(hr) && (ppDevice != nullptr && *ppDevice != nullptr && pImmediateContext != nullptr))
	{
		ReShade::Hooks::Install(VTABLE(*ppDevice)[10], reinterpret_cast<ReShade::Hook::Function>(&ID3D11Device_CreateDepthStencilView));
		ReShade::Hooks::Install(VTABLE(pImmediateContext)[12], reinterpret_cast<ReShade::Hook::Function>(&ID3D11DeviceContext_DrawIndexed));
		ReShade::Hooks::Install(VTABLE(pImmediateContext)[13], reinterpret_cast<ReShade::Hook::Function>(&ID3D11DeviceContext_Draw));
		ReShade::Hooks::Install(VTABLE(pImmediateContext)[20], reinterpret_cast<ReShade::Hook::Function>(&ID3D11DeviceContext_DrawIndexedInstanced));
		ReShade::Hooks::Install(VTABLE(pImmediateContext)[21], reinterpret_cast<ReShade::Hook::Function>(&ID3D11DeviceContext_DrawInstanced));
		ReShade::Hooks::Install(VTABLE(pImmediateContext)[33], reinterpret_cast<ReShade::Hook::Function>(&ID3D11DeviceContext_OMSetRenderTargets));
		ReShade::Hooks::Install(VTABLE(pImmediateContext)[34], reinterpret_cast<ReShade::Hook::Function>(&ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews));
		ReShade::Hooks::Install(VTABLE(pImmediateContext)[47], reinterpret_cast<ReShade::Hook::Function>(&ID3D11DeviceContext_CopyResource));
		ReShade::Hooks::Install(VTABLE(pImmediateContext)[53], reinterpret_cast<ReShade::Hook::Function>(&ID3D11DeviceContext_ClearDepthStencilView));
		ReShade::Hooks::Install(VTABLE(pImmediateContext)[89], reinterpret_cast<ReShade::Hook::Function>(&ID3D11DeviceContext_OMGetRenderTargets));
		ReShade::Hooks::Install(VTABLE(pImmediateContext)[90], reinterpret_cast<ReShade::Hook::Function>(&ID3D11DeviceContext_OMGetRenderTargetsAndUnorderedAccessViews));
	}

	return hr;
}