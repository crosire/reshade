#include "Log.hpp"
#include "Runtime.hpp"
#include "HookManager.hpp"

#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <unordered_map>

// -----------------------------------------------------------------------------------------------------

namespace
{
	std::unordered_map<IDXGISwapChain *, std::shared_ptr<ReShade::Runtime>>	sManagers;
	std::unordered_map<IDXGISwapChain *, ULONG>					sReferences;

	inline ULONG												GetRefCount(IUnknown *pUnknown)
	{
		const ULONG ref = pUnknown->AddRef();
		pUnknown->Release();

		return ref - 1;
	}
	LPCSTR														GetErrorString(HRESULT hr)
	{
		switch (hr)
		{
			default:
				__declspec(thread) static CHAR buf[20];
				sprintf_s(buf, "0x%lx", hr);
				return buf;
			case DXGI_ERROR_INVALID_CALL:
				return "DXGI_ERROR_INVALID_CALL";
			case DXGI_ERROR_UNSUPPORTED:
				return "DXGI_ERROR_UNSUPPORTED";
		}
	}
}
namespace ReShade
{
	extern std::shared_ptr<ReShade::Runtime>					CreateEffectRuntime(ID3D10Device *device, IDXGISwapChain *swapchain);
	extern std::shared_ptr<ReShade::Runtime>					CreateEffectRuntime(ID3D11Device *device, IDXGISwapChain *swapchain);
}

// -----------------------------------------------------------------------------------------------------

// IDXGISwapChain
ULONG STDMETHODCALLTYPE											IDXGISwapChain_Release(IDXGISwapChain *pSwapChain)	
{
	static ReHook::Hook hook = ReHook::Find(&IDXGISwapChain_Release);
	static const auto trampoline = ReHook::Call(&IDXGISwapChain_Release);

	ULONG ref = trampoline(pSwapChain);

	const auto it = sReferences.find(pSwapChain);

	if (it != sReferences.end() && ref == it->second)
	{
		LOG(INFO) << "Redirecting final '" << "IDXGISwapChain::Release" << "(" << pSwapChain << ")' ...";

		hook.Enable(false);
		pSwapChain->AddRef();

		sManagers.erase(pSwapChain);
		sReferences.erase(pSwapChain);

		ref = pSwapChain->Release();
		hook.Enable(true);
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE										IDXGISwapChain_Present(IDXGISwapChain *pSwapChain, UINT SyncInterval, UINT Flags)
{
	static const auto trampoline = ReHook::Call(&IDXGISwapChain_Present);
	
	const auto it = sManagers.find(pSwapChain);

	if (it != sManagers.end())
	{
		const std::shared_ptr<ReShade::Runtime> &runtime = it->second;

		runtime->OnPostProcess();
		runtime->OnPresent();
	}

	return trampoline(pSwapChain, SyncInterval, Flags);
}
HRESULT STDMETHODCALLTYPE										IDXGISwapChain_SetFullscreenState(IDXGISwapChain *pSwapChain, BOOL Fullscreen, IDXGIOutput *pTarget)
{
	LOG(INFO) << "Redirecting '" << "IDXGISwapChain::SetFullscreenState" << "(" << pSwapChain << ", " << Fullscreen << ", " << pTarget << ")' ...";

	return ReHook::Call(&IDXGISwapChain_SetFullscreenState)(pSwapChain, Fullscreen, pTarget);
}
HRESULT STDMETHODCALLTYPE										IDXGISwapChain_ResizeBuffers(IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	LOG(INFO) << "Redirecting '" << "IDXGISwapChain::ResizeBuffers" << "(" << pSwapChain << ", " << BufferCount << ", " << Width << ", " << Height << ", " << NewFormat << ", " << SwapChainFlags << ")' ...";

	const auto it = sManagers.find(pSwapChain);
	const std::shared_ptr<ReShade::Runtime> runtime = it != sManagers.end() ? it->second : nullptr;

	if (runtime != nullptr)
	{
		sReferences.erase(pSwapChain);
		runtime->OnDelete();
	}

	HRESULT hr = ReHook::Call(&IDXGISwapChain_ResizeBuffers)(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

	if (SUCCEEDED(hr))
	{
		if (runtime != nullptr)
		{
			const ULONG ref = GetRefCount(pSwapChain);

			runtime->OnCreate(Width, Height);
			sReferences.insert(std::make_pair(pSwapChain, GetRefCount(pSwapChain) - ref));
		}
	}
	else
	{
		LOG(ERROR) << "'IDXGISwapChain::ResizeBuffers' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}

// IDXGISwapChain1
HRESULT STDMETHODCALLTYPE										IDXGISwapChain1_Present1(IDXGISwapChain1 *pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	static const auto trampoline = ReHook::Call(&IDXGISwapChain1_Present1);
	
	const auto it = sManagers.find(pSwapChain);

	if (it != sManagers.end())
	{
		const std::shared_ptr<ReShade::Runtime> &runtime = it->second;

		runtime->OnPostProcess();
		runtime->OnPresent();
	}

	return trampoline(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
}

// IDXGIFactory
HRESULT STDMETHODCALLTYPE										IDXGIFactory_CreateSwapChain(IDXGIFactory *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory::CreateSwapChain" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << ppSwapChain << ")' ...";

	assert(pDesc != nullptr);

	DXGI_SWAP_CHAIN_DESC desc = *pDesc;

	switch (desc.BufferDesc.Format)
	{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			LOG(WARNING) << "> Replacing 'DXGI_FORMAT_B8G8R8A8_UNORM' with 'DXGI_FORMAT_R8G8B8A8_UNORM' buffer format ...";
			desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			LOG(WARNING) << "> Replacing 'DXGI_FORMAT_B8G8R8A8_UNORM_SRGB' with 'DXGI_FORMAT_R8G8B8A8_UNORM_SRGB' buffer format ...";
			desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			break;
	}

	HRESULT hr = ReHook::Call(&IDXGIFactory_CreateSwapChain)(pFactory, pDevice, &desc, ppSwapChain);

	if (SUCCEEDED(hr))
	{
		#pragma region Skip uninteresting swapchains
		if ((pDesc->BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
		{
			LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";

			return hr;
		}

		RECT rect;
		GetWindowRect(pDesc->OutputWindow, &rect);

		const UINT width = (pDesc->BufferDesc.Width != 0) ? pDesc->BufferDesc.Width : (rect.right - rect.left);
		const UINT height = (pDesc->BufferDesc.Height != 0) ? pDesc->BufferDesc.Height : (rect.bottom - rect.top);

		if (width <= 100 || height <= 100)
		{
			LOG(WARNING) << "> Skipping swapchain due to low target size of " << width << 'x' << height << ".";

			return hr;
		}
		#pragma endregion

		ID3D10Device *deviceD3D10 = nullptr;
		ID3D11Device *deviceD3D11 = nullptr;
		IDXGISwapChain *swapchain = *ppSwapChain;

		swapchain->GetDesc(&desc);

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D10, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.BufferDesc.Width, desc.BufferDesc.Height);

				sManagers.insert(std::make_pair(swapchain, runtime));
				sReferences.insert(std::make_pair(swapchain, GetRefCount(swapchain) - 1));
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 10 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D11, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.BufferDesc.Width, desc.BufferDesc.Height);

				sManagers.insert(std::make_pair(swapchain, runtime));
				sReferences.insert(std::make_pair(swapchain, GetRefCount(swapchain) - 1));
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 11 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D11->Release();
		}
		else
		{
			LOG(WARNING) << "> Swapchain was created without a Direct3D device. Skipping ...";

			return hr;
		}

		ReHook::Register(VTable(swapchain, 2), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_Release));
		ReHook::Register(VTable(swapchain, 8), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_Present));
		ReHook::Register(VTable(swapchain, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_SetFullscreenState));
		ReHook::Register(VTable(swapchain, 13), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_ResizeBuffers));
	}
	else
	{
		LOG(WARNING) << "> 'IDXGIFactory::CreateSwapChain' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}

// IDXGIFactory2
HRESULT STDMETHODCALLTYPE										IDXGIFactory2_CreateSwapChainForHwnd(IDXGIFactory2 *pFactory, IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForHwnd" << "(" << pFactory << ", " << pDevice << ", " << hWnd << ", " << pDesc << ", " << pFullscreenDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";
	
	assert(pDesc != nullptr);

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

	switch (desc.Format)
	{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			LOG(WARNING) << "> Replacing 'DXGI_FORMAT_B8G8R8A8_UNORM' with 'DXGI_FORMAT_R8G8B8A8_UNORM' buffer format ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			LOG(WARNING) << "> Replacing 'DXGI_FORMAT_B8G8R8A8_UNORM_SRGB' with 'DXGI_FORMAT_R8G8B8A8_UNORM_SRGB' buffer format ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			break;
	}

	HRESULT hr = ReHook::Call(&IDXGIFactory2_CreateSwapChainForHwnd)(pFactory, pDevice, hWnd, &desc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

	if (SUCCEEDED(hr))
	{
		#pragma region Skip uninteresting swapchains
		if ((pDesc->BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
		{
			LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";

			return hr;
		}
		#pragma endregion

		ID3D10Device *deviceD3D10 = nullptr;
		ID3D11Device *deviceD3D11 = nullptr;
		IDXGISwapChain1 *swapchain = *ppSwapChain;

		swapchain->GetDesc1(&desc);

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D10, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				sManagers.insert(std::make_pair(swapchain, runtime));
				sReferences.insert(std::make_pair(swapchain, GetRefCount(swapchain) - 1));
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 10 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D11, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				sManagers.insert(std::make_pair(swapchain, runtime));
				sReferences.insert(std::make_pair(swapchain, GetRefCount(swapchain) - 1));
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 11 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D11->Release();
		}
		else
		{
			LOG(WARNING) << "> Swapchain was created without a Direct3D device. Skipping ...";

			return hr;
		}

		ReHook::Register(VTable(swapchain, 2), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_Release));
		ReHook::Register(VTable(swapchain, 8), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_Present));
		ReHook::Register(VTable(swapchain, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_SetFullscreenState));
		ReHook::Register(VTable(swapchain, 13), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_ResizeBuffers));
		ReHook::Register(VTable(swapchain, 22), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain1_Present1));
	}
	else
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForHwnd' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE										IDXGIFactory2_CreateSwapChainForCoreWindow(IDXGIFactory2 *pFactory, IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForCoreWindow" << "(" << pFactory << ", " << pDevice << ", " << pWindow << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	assert(pDesc != nullptr);

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

	switch (desc.Format)
	{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			LOG(WARNING) << "> Replacing 'DXGI_FORMAT_B8G8R8A8_UNORM' with 'DXGI_FORMAT_R8G8B8A8_UNORM' buffer format ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			LOG(WARNING) << "> Replacing 'DXGI_FORMAT_B8G8R8A8_UNORM_SRGB' with 'DXGI_FORMAT_R8G8B8A8_UNORM_SRGB' buffer format ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			break;
	}

	HRESULT hr = ReHook::Call(&IDXGIFactory2_CreateSwapChainForCoreWindow)(pFactory, pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain);

	if (SUCCEEDED(hr))
	{
		#pragma region Skip uninteresting swapchains
		if ((pDesc->BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
		{
			LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";

			return hr;
		}
		#pragma endregion

		ID3D10Device *deviceD3D10 = nullptr;
		ID3D11Device *deviceD3D11 = nullptr;
		IDXGISwapChain1 *swapchain = *ppSwapChain;

		swapchain->GetDesc1(&desc);

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D10, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				sManagers.insert(std::make_pair(swapchain, runtime));
				sReferences.insert(std::make_pair(swapchain, GetRefCount(swapchain) - 1));
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 10 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D11, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				sManagers.insert(std::make_pair(swapchain, runtime));
				sReferences.insert(std::make_pair(swapchain, GetRefCount(swapchain) - 1));
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 11 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D11->Release();
		}
		else
		{
			LOG(WARNING) << "> Swapchain was created without a Direct3D device. Skipping ...";

			return hr;
		}

		ReHook::Register(VTable(swapchain, 2), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_Release));
		ReHook::Register(VTable(swapchain, 8), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_Present));
		ReHook::Register(VTable(swapchain, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_SetFullscreenState));
		ReHook::Register(VTable(swapchain, 13), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_ResizeBuffers));
		ReHook::Register(VTable(swapchain, 22), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain1_Present1));
	}
	else
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForCoreWindow' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE										IDXGIFactory2_CreateSwapChainForComposition(IDXGIFactory2 *pFactory, IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForComposition" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	assert(pDesc != nullptr);

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

	switch (desc.Format)
	{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			LOG(WARNING) << "> Replacing 'DXGI_FORMAT_B8G8R8A8_UNORM' with 'DXGI_FORMAT_R8G8B8A8_UNORM' buffer format ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			LOG(WARNING) << "> Replacing 'DXGI_FORMAT_B8G8R8A8_UNORM_SRGB' with 'DXGI_FORMAT_R8G8B8A8_UNORM_SRGB' buffer format ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			break;
	}

	HRESULT hr = ReHook::Call(&IDXGIFactory2_CreateSwapChainForComposition)(pFactory, pDevice, &desc, pRestrictToOutput, ppSwapChain);

	if (SUCCEEDED(hr))
	{
		#pragma region Skip uninteresting swapchains
		if ((pDesc->BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
		{
			LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";

			return hr;
		}
		#pragma endregion

		ID3D10Device *deviceD3D10 = nullptr;
		ID3D11Device *deviceD3D11 = nullptr;
		IDXGISwapChain1 *swapchain = *ppSwapChain;

		swapchain->GetDesc1(&desc);

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D10, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				sManagers.insert(std::make_pair(swapchain, runtime));
				sReferences.insert(std::make_pair(swapchain, GetRefCount(swapchain) - 1));
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 10 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D11, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				sManagers.insert(std::make_pair(swapchain, runtime));
				sReferences.insert(std::make_pair(swapchain, GetRefCount(swapchain) - 1));
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 11 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D11->Release();
		}
		else
		{
			LOG(WARNING) << "> Swapchain was created without a Direct3D device. Skipping ...";

			return hr;
		}

		ReHook::Register(VTable(swapchain, 2), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_Release));
		ReHook::Register(VTable(swapchain, 8), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_Present));
		ReHook::Register(VTable(swapchain, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_SetFullscreenState));
		ReHook::Register(VTable(swapchain, 13), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain_ResizeBuffers));
		ReHook::Register(VTable(swapchain, 22), reinterpret_cast<ReHook::Hook::Function>(&IDXGISwapChain1_Present1));
	}
	else
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForComposition' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}

// D3DKMT
EXPORT NTSTATUS WINAPI											D3DKMTCreateAllocation(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTQueryResourceInfo(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTOpenResource(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTDestroyAllocation(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetAllocationPriority(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTQueryAllocationResidency(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTCreateDevice(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTDestroyDevice(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTCreateContext(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTDestroyContext(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTCreateSynchronizationObject(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTDestroySynchronizationObject(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTWaitForSynchronizationObject(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSignalSynchronizationObject(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTLock(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTUnlock(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetDisplayModeList(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetDisplayMode(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetMultisampleMethodList(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTPresent(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTRender(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetRuntimeData(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTQueryAdapterInfo(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTOpenAdapterFromHdc(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTCloseAdapter(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetSharedPrimaryHandle(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTEscape(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetVidPnSourceOwner(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTWaitForVerticalBlankEvent(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetGammaRamp(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetDeviceState(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetContextSchedulingPriority(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetContextSchedulingPriority(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetDisplayPrivateDriverFormat(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}

// DXGI
EXPORT HRESULT WINAPI											DXGID3D10CreateDevice(HMODULE hModule, IDXGIFactory *pFactory, IDXGIAdapter *pAdapter, UINT Flags, void *pUnknown, void **ppDevice)
{
	UNREFERENCED_PARAMETER(hModule);
	UNREFERENCED_PARAMETER(pFactory);
	UNREFERENCED_PARAMETER(pAdapter);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(pUnknown);
	UNREFERENCED_PARAMETER(ppDevice);

	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI											DXGID3D10CreateLayeredDevice(BYTE pUnknown[20])
{
	UNREFERENCED_PARAMETER(pUnknown);

	assert(false);

	return E_NOTIMPL;
}
EXPORT SIZE_T WINAPI											DXGID3D10GetLayeredDeviceSize(const void *pLayers, UINT NumLayers)
{
	UNREFERENCED_PARAMETER(pLayers);
	UNREFERENCED_PARAMETER(NumLayers);

	assert(false);

	return 0;
}
EXPORT HRESULT WINAPI											DXGID3D10RegisterLayers(const void *pLayers, UINT NumLayers)
{
	UNREFERENCED_PARAMETER(pLayers);
	UNREFERENCED_PARAMETER(NumLayers);

	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI											DXGIReportAdapterConfiguration(void)
{
	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI											DXGIDumpJournal(void)
{
	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI											OpenAdapter10(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI											OpenAdapter10_2(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return E_NOTIMPL;
}

EXPORT HRESULT WINAPI											CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	::StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory" << "(" << guid << ", " << ppFactory << ")' ...";
	
	HRESULT hr = ReHook::Call(&CreateDXGIFactory)(riid, ppFactory);

	if (SUCCEEDED(hr))
	{
		ReHook::Register(VTable(*ppFactory, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory_CreateSwapChain));
	}
	else
	{
		LOG(WARNING) << "> 'CreateDXGIFactory' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
EXPORT HRESULT WINAPI											CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	::StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory1" << "(" << guid << ", " << ppFactory << ")' ...";

	HRESULT hr = ReHook::Call(&CreateDXGIFactory1)(riid, ppFactory);

	if (SUCCEEDED(hr))
	{
		ReHook::Register(VTable(*ppFactory, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory_CreateSwapChain));
	}
	else
	{
		LOG(WARNING) << "> 'CreateDXGIFactory1' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
EXPORT HRESULT WINAPI											CreateDXGIFactory2(UINT flags, REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	::StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory2" << "(" << flags << ", " << guid << ", " << ppFactory << ")' ...";

	HRESULT hr = ReHook::Call(&CreateDXGIFactory2)(flags, riid, ppFactory);

	if (SUCCEEDED(hr))
	{
		ReHook::Register(VTable(*ppFactory, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory_CreateSwapChain));

		if (riid == __uuidof(IDXGIFactory2))
		{
			ReHook::Register(VTable(*ppFactory, 15), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory2_CreateSwapChainForHwnd));
			ReHook::Register(VTable(*ppFactory, 16), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
			ReHook::Register(VTable(*ppFactory, 24), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory2_CreateSwapChainForComposition));
		}
	}
	else
	{
		LOG(WARNING) << "> 'CreateDXGIFactory2' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}