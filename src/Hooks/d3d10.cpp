#include "Log.hpp"
#include "HookManager.hpp"
#include "Runtimes\RuntimeD3D10.hpp"

#include <d3d10_1.h>

// -----------------------------------------------------------------------------------------------------

extern const GUID sRuntimeGUID;

// -----------------------------------------------------------------------------------------------------

// ID3D10Device
void STDMETHODCALLTYPE ID3D10Device_DrawIndexed(ID3D10Device *pDevice, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D10Device_DrawIndexed);

	ReShade::Runtimes::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->OnDrawInternal(IndexCount);
	}

	trampoline(pDevice, IndexCount, StartIndexLocation, BaseVertexLocation);
}
void STDMETHODCALLTYPE ID3D10Device_Draw(ID3D10Device *pDevice, UINT VertexCount, UINT StartVertexLocation)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D10Device_Draw);

	ReShade::Runtimes::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->OnDrawInternal(VertexCount);
	}

	trampoline(pDevice, VertexCount, StartVertexLocation);
}
void STDMETHODCALLTYPE ID3D10Device_DrawIndexedInstanced(ID3D10Device *pDevice, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D10Device_DrawIndexedInstanced);

	ReShade::Runtimes::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->OnDrawInternal(IndexCountPerInstance * InstanceCount);
	}

	trampoline(pDevice, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE ID3D10Device_DrawInstanced(ID3D10Device *pDevice, UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D10Device_DrawInstanced);

	ReShade::Runtimes::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->OnDrawInternal(VertexCountPerInstance * InstanceCount);
	}

	trampoline(pDevice, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE ID3D10Device_OMSetRenderTargets(ID3D10Device *pDevice, UINT NumViews, ID3D10RenderTargetView *const *ppRenderTargetViews, ID3D10DepthStencilView *pDepthStencilView)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D10Device_OMSetRenderTargets);

	if (pDepthStencilView != nullptr)
	{
		ReShade::Runtimes::D3D10Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);

		if (SUCCEEDED(pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
		{
			runtime->ReplaceDepthStencil(pDepthStencilView);
		}
	}

	trampoline(pDevice, NumViews, ppRenderTargetViews, pDepthStencilView);
}
void STDMETHODCALLTYPE ID3D10Device_CopyResource(ID3D10Device *pDevice, ID3D10Resource *pDstResource, ID3D10Resource *pSrcResource)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D10Device_CopyResource);

	ReShade::Runtimes::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->ReplaceDepthStencilResource(pDstResource);
		runtime->ReplaceDepthStencilResource(pSrcResource);
	}

	trampoline(pDevice, pDstResource, pSrcResource);
}
void STDMETHODCALLTYPE ID3D10Device_ClearDepthStencilView(ID3D10Device *pDevice, ID3D10DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D10Device_ClearDepthStencilView);

	ReShade::Runtimes::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->ReplaceDepthStencil(pDepthStencilView);
	}

	trampoline(pDevice, pDepthStencilView, ClearFlags, Depth, Stencil);
}
HRESULT STDMETHODCALLTYPE ID3D10Device_CreateDepthStencilView(ID3D10Device *pDevice, ID3D10Resource *pResource, const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D10DepthStencilView **ppDepthStencilView)
{
	static const auto trampoline = ReShade::Hooks::Call(&ID3D10Device_CreateDepthStencilView);

	const HRESULT hr = trampoline(pDevice, pResource, pDesc, ppDepthStencilView);

	ReShade::Runtimes::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);

	if (SUCCEEDED(hr) && SUCCEEDED(pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime))))
	{
		runtime->OnCreateDepthStencil(pResource, *ppDepthStencilView);
	}

	return hr;
}

// D3D10
EXPORT HRESULT WINAPI D3D10CreateDevice(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, ID3D10Device **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDevice" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << SDKVersion << ", " << ppDevice << ")' ...";
	LOG(INFO) << "> Passing on to 'D3D10CreateDeviceAndSwapChain':";

	return D3D10CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, SDKVersion, nullptr, nullptr, ppDevice);
}
EXPORT HRESULT WINAPI D3D10CreateDevice1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, ID3D10Device1 **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDevice1" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << HardwareLevel << ", " << SDKVersion << ", " << ppDevice << ")' ...";
	LOG(INFO) << "> Passing on to 'D3D10CreateDeviceAndSwapChain1':";

	return D3D10CreateDeviceAndSwapChain1(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, nullptr, nullptr, ppDevice);
}
EXPORT HRESULT WINAPI D3D10CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDeviceAndSwapChain" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << SDKVersion << ", " << pSwapChainDesc << ", " << ppSwapChain << ", " << ppDevice << ")' ...";

#ifdef _DEBUG
	Flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	const HRESULT hr = ReShade::Hooks::Call(&D3D10CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);

	if (SUCCEEDED(hr) && (ppDevice != nullptr && *ppDevice != nullptr))
	{
		ReShade::Hooks::Register(VTABLE(*ppDevice, 8), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_DrawIndexed));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 9), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_Draw));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 14), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_DrawIndexedInstanced));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 15), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_DrawInstanced));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 24), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_OMSetRenderTargets));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 33), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_CopyResource));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 36), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_ClearDepthStencilView));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 77), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_CreateDepthStencilView));
	}

	return hr;
}
EXPORT HRESULT WINAPI D3D10CreateDeviceAndSwapChain1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device1 **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDeviceAndSwapChain1" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << HardwareLevel << ", " << SDKVersion << ", " << pSwapChainDesc << ", " << ppSwapChain << ", " << ppDevice << ")' ...";

#ifdef _DEBUG
	Flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	const HRESULT hr = ReShade::Hooks::Call(&D3D10CreateDeviceAndSwapChain1)(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);

	if (SUCCEEDED(hr) && (ppDevice != nullptr && *ppDevice != nullptr))
	{
		ReShade::Hooks::Register(VTABLE(*ppDevice, 8), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_DrawIndexed));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 9), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_Draw));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 14), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_DrawIndexedInstanced));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 15), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_DrawInstanced));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 24), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_OMSetRenderTargets));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 33), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_CopyResource));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 36), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_ClearDepthStencilView));
		ReShade::Hooks::Register(VTABLE(*ppDevice, 77), reinterpret_cast<ReShade::Hook::Function>(&ID3D10Device_CreateDepthStencilView));
	}

	return hr;
}