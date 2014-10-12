#include "Log.hpp"
#include "Manager.hpp"
#include "HookManager.hpp"

#include <d3d9.h>
#include <unordered_map>

#undef IDirect3D9_CreateDevice
#undef IDirect3D9Ex_CreateDeviceEx
#undef IDirect3DDevice9_AddRef
#undef IDirect3DDevice9_Release
#undef IDirect3DDevice9_CreateAdditionalSwapChain
#undef IDirect3DDevice9_Reset
#undef IDirect3DDevice9_Present
#undef IDirect3DDevice9Ex_ResetEx
#undef IDirect3DDevice9Ex_PresentEx
#undef IDirect3DSwapChain9_AddRef
#undef IDirect3DSwapChain9_Release
#undef IDirect3DSwapChain9_Present

// -----------------------------------------------------------------------------------------------------

namespace
{
	std::unordered_map<IUnknown *, ReShade::Manager *>			sManagers;
	std::unordered_map<IUnknown *, ULONG>						sReferences;

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
			case D3DERR_NOTAVAILABLE:
				return "D3DERR_NOTAVAILABLE";
			case D3DERR_INVALIDCALL:
				return "D3DERR_INVALIDCALL";
			case D3DERR_INVALIDDEVICE:
				return "D3DERR_INVALIDDEVICE";
			case D3DERR_DEVICEHUNG:
				return "D3DERR_DEVICEHUNG";
			case D3DERR_DEVICELOST:
				return "D3DERR_DEVICELOST";
			case D3DERR_DEVICENOTRESET:
				return "D3DERR_DEVICENOTRESET";
			case D3DERR_WASSTILLDRAWING:
				return "D3DERR_WASSTILLDRAWING";
		}
	}
}
namespace ReShade
{
	extern std::shared_ptr<ReShade::EffectContext>				CreateEffectContext(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);
}

// -----------------------------------------------------------------------------------------------------

// IDirect3DSwapChain9
ULONG STDMETHODCALLTYPE											IDirect3DSwapChain9_Release(IDirect3DSwapChain9 *pSwapChain)
{
	static ReHook::Hook hook = ReHook::Find(&IDirect3DSwapChain9_Release);
	static const auto trampoline = ReHook::Call(&IDirect3DSwapChain9_Release);
	
	ULONG ref = trampoline(pSwapChain);

	const auto it = sReferences.find(pSwapChain);

	if (it != sReferences.end() && ref == it->second)
	{
		LOG(INFO) << "Redirecting final '" << "IDirect3DSwapChain9::Release" << "(" << pSwapChain << ")' ...";

		hook.Enable(false);
		pSwapChain->AddRef();
		
		delete sManagers.at(pSwapChain);
		sManagers.erase(pSwapChain);
		sReferences.erase(pSwapChain);

		ref = pSwapChain->Release();
		hook.Enable(true);
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE										IDirect3DSwapChain9_Present(IDirect3DSwapChain9 *pSwapChain, const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
	static const auto trampoline = ReHook::Call(&IDirect3DSwapChain9_Present);
	
	if (sManagers.count(pSwapChain))
	{
		sManagers.at(pSwapChain)->OnPostProcess();
		sManagers.at(pSwapChain)->OnPresent();
	}

	return trampoline(pSwapChain, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

// IDirect3DDevice9
ULONG STDMETHODCALLTYPE											IDirect3DDevice9_Release(IDirect3DDevice9 *pDevice)
{
	static ReHook::Hook hook = ReHook::Find(&IDirect3DDevice9_Release);
	static const auto trampoline = ReHook::Call(&IDirect3DDevice9_Release);
	
	ULONG ref = trampoline(pDevice);

	const auto it = sReferences.find(pDevice);

	if (it != sReferences.end() && ref == it->second)
	{
		LOG(INFO) << "Redirecting final '" << "IDirect3DDevice9::Release" << "(" << pDevice << ")' ...";

		hook.Enable(false);
		pDevice->AddRef();

		IDirect3DSwapChain9 *swapchain = nullptr;
		pDevice->GetSwapChain(0, &swapchain);

		delete sManagers.at(pDevice);
		sManagers.erase(pDevice);
		sManagers.erase(swapchain);
		sReferences.erase(pDevice);

		swapchain->Release();
		ref = pDevice->Release();
		hook.Enable(true);
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE										IDirect3DDevice9_CreateAdditionalSwapChain(IDirect3DDevice9 *pDevice, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain9 **ppSwapChain)
{
	HRESULT hr = ReHook::Call(&IDirect3DDevice9_CreateAdditionalSwapChain)(pDevice, pPresentationParameters, ppSwapChain);

	if (SUCCEEDED(hr))
	{
		#pragma region Skip uninteresting swapchains
		RECT rect;
		GetWindowRect(pPresentationParameters->hDeviceWindow, &rect);
			
		const UINT width = (pPresentationParameters->BackBufferWidth != 0) ? pPresentationParameters->BackBufferWidth : (rect.right - rect.left);
		const UINT height = (pPresentationParameters->BackBufferHeight != 0) ? pPresentationParameters->BackBufferHeight : (rect.bottom - rect.top);

		if (width <= 100 || height <= 100)
		{
			LOG(WARNING) << "> Low target size of " << width << 'x' << height << ". Skipping ...";

			return hr;
		}
		#pragma endregion
		
		IDirect3DDevice9 *device = nullptr;
		IDirect3DSwapChain9 *swapchain = *ppSwapChain;
		swapchain->GetDevice(&device);

		assert(device != nullptr);

		const std::shared_ptr<ReShade::EffectContext> context = ReShade::CreateEffectContext(device, swapchain);
		
		if (context != nullptr)
		{
			ReShade::Manager *manager = new ReShade::Manager(context);

			sManagers.insert(std::make_pair(swapchain, manager));
			sReferences.insert(std::make_pair(swapchain, GetRefCount(swapchain) - 1));
		}
		else
		{
			LOG(ERROR) << "Failed to initialize Direct3D 9 renderer on additional swapchain " << swapchain << "!";
		}
	}
	else
	{
		LOG(WARNING) << "> 'IDirect3DDevice9::CreateAdditionalSwapChain' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE										IDirect3DDevice9_Reset(IDirect3DDevice9 *pDevice, D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice9::Reset" << "(" << pDevice << ", " << pPresentationParameters << ")' ...";

	const auto it = sManagers.find(pDevice);
	ReShade::Manager *const manager = it != sManagers.end() ? it->second : nullptr;

	if (manager != nullptr)
	{
		sReferences.erase(pDevice);
		manager->OnDelete();
	}

	HRESULT hr = ReHook::Call(&IDirect3DDevice9_Reset)(pDevice, pPresentationParameters);

	if (SUCCEEDED(hr))
	{
		if (manager != nullptr)
		{
			const ULONG ref = GetRefCount(pDevice);
			
			manager->OnCreate();
			sReferences.insert(std::make_pair(pDevice, GetRefCount(pDevice) - ref));
		}
	}
	else
	{
		LOG(ERROR) << "'IDirect3DDevice9::Reset' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE										IDirect3DDevice9_Present(IDirect3DDevice9 *pDevice, const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion)
{
	static const auto trampoline = ReHook::Call(&IDirect3DDevice9_Present);
	
	const auto it = sManagers.find(pDevice);
	ReShade::Manager *const manager = it != sManagers.end() ? it->second : nullptr;

	if (manager != nullptr)
	{
		manager->OnPostProcess();
		manager->OnPresent();
	}

	return trampoline(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

// IDirect3DDevice9Ex
HRESULT STDMETHODCALLTYPE										IDirect3DDevice9Ex_ResetEx(IDirect3DDevice9Ex *pDevice, D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice9Ex::ResetEx" << "(" << pDevice << ", " << pPresentationParameters << ", " << pFullscreenDisplayMode << ")' ...";

	const auto it = sManagers.find(pDevice);
	ReShade::Manager *const manager = it != sManagers.end() ? it->second : nullptr;

	if (manager != nullptr)
	{
		sReferences.erase(pDevice);
		manager->OnDelete();
	}

	HRESULT hr = ReHook::Call(&IDirect3DDevice9Ex_ResetEx)(pDevice, pPresentationParameters, pFullscreenDisplayMode);

	if (SUCCEEDED(hr))
	{
		if (manager != nullptr)
		{
			const ULONG ref = GetRefCount(pDevice);

			manager->OnCreate();
			sReferences.insert(std::make_pair(pDevice, GetRefCount(pDevice) - ref));
		}
	}
	else
	{
		LOG(ERROR) << "'IDirect3DDevice9Ex::ResetEx' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE										IDirect3DDevice9Ex_PresentEx(IDirect3DDevice9Ex *pDevice, const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
	static const auto trampoline = ReHook::Call(&IDirect3DDevice9Ex_PresentEx);

	const auto it = sManagers.find(pDevice);
	ReShade::Manager *const manager = it != sManagers.end() ? it->second : nullptr;

	if (manager != nullptr)
	{
		manager->OnPostProcess();
		manager->OnPresent();
	}

	return trampoline(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

// IDirect3D9
HRESULT STDMETHODCALLTYPE										IDirect3D9_CreateDevice(IDirect3D9 *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DDevice9 **ppReturnedDeviceInterface)
{
	LOG(INFO) << "Redirecting '" << "IDirect3D9::CreateDevice" << "(" << pD3D << ", " << Adapter << ", " << DeviceType << ", " << hFocusWindow << ", " << BehaviorFlags << ", " << pPresentationParameters << ", " << ppReturnedDeviceInterface << ")' ...";

	HRESULT hr = ReHook::Call(&IDirect3D9_CreateDevice)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	if (SUCCEEDED(hr))
	{
		IDirect3DDevice9 *device = *ppReturnedDeviceInterface;
		IDirect3DSwapChain9 *swapchain = nullptr;
		device->GetSwapChain(0, &swapchain);

		assert(swapchain != nullptr);
		
		const std::shared_ptr<ReShade::EffectContext> context = ReShade::CreateEffectContext(device, swapchain);
		
		if (context != nullptr)
		{
			ReShade::Manager *manager = new ReShade::Manager(context);

			sManagers.insert(std::make_pair(device, manager));
			sManagers.insert(std::make_pair(swapchain, manager));
			sReferences.insert(std::make_pair(device, GetRefCount(device) - 1));
		}
		else
		{
			LOG(ERROR) << "Failed to initialize Direct3D 9 renderer on implicit swapchain " << swapchain << "!";
		}

		ReHook::Register(VTable(device, 2), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DDevice9_Release));
		ReHook::Register(VTable(device, 13), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DDevice9_CreateAdditionalSwapChain));
		ReHook::Register(VTable(device, 16), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DDevice9_Reset));
		ReHook::Register(VTable(device, 17), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DDevice9_Present));
		ReHook::Register(VTable(swapchain, 2), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DSwapChain9_Release));
		ReHook::Register(VTable(swapchain, 3), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DSwapChain9_Present));

		swapchain->Release();
	}
	else
	{
		LOG(WARNING) << "> 'IDirect3D9::CreateDevice' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}

// IDirect3D9Ex
HRESULT STDMETHODCALLTYPE										IDirect3D9Ex_CreateDeviceEx(IDirect3D9Ex *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode, IDirect3DDevice9Ex **ppReturnedDeviceInterface)
{
	LOG(INFO) << "Redirecting '" << "IDirect3D9Ex::CreateDeviceEx" << "(" << pD3D << ", " << Adapter << ", " << DeviceType << ", " << hFocusWindow << ", " << BehaviorFlags << ", " << pPresentationParameters << ", " << pFullscreenDisplayMode << ", " << ppReturnedDeviceInterface << ")' ...";

	HRESULT hr = ReHook::Call(&IDirect3D9Ex_CreateDeviceEx)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

	if (SUCCEEDED(hr))
	{
		IDirect3DDevice9Ex *device = *ppReturnedDeviceInterface;
		IDirect3DSwapChain9 *swapchain = nullptr;
		device->GetSwapChain(0, &swapchain);

		assert(swapchain != nullptr);

		const std::shared_ptr<ReShade::EffectContext> context = ReShade::CreateEffectContext(device, swapchain);
		
		if (context != nullptr)
		{
			ReShade::Manager *manager = new ReShade::Manager(context);

			sManagers.insert(std::make_pair(device, manager));
			sManagers.insert(std::make_pair(swapchain, manager));
			sReferences.insert(std::make_pair(device, GetRefCount(device) - 1));
		}
		else
		{
			LOG(ERROR) << "Failed to initialize Direct3D 9 renderer on implicit swapchain " << swapchain << "!";
		}

		ReHook::Register(VTable(device, 2), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DDevice9_Release));
		ReHook::Register(VTable(device, 13), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DDevice9_CreateAdditionalSwapChain));
		ReHook::Register(VTable(device, 16), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DDevice9_Reset));
		ReHook::Register(VTable(device, 17), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DDevice9_Present));
		ReHook::Register(VTable(device, 121), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DDevice9Ex_PresentEx));
		ReHook::Register(VTable(device, 132), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DDevice9Ex_ResetEx));
		ReHook::Register(VTable(swapchain, 2), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DSwapChain9_Release));
		ReHook::Register(VTable(swapchain, 3), reinterpret_cast<ReHook::Hook::Function>(&IDirect3DSwapChain9_Present));

		swapchain->Release();
	}
	else
	{
		LOG(WARNING) << "> 'IDirect3D9Ex::CreateDeviceEx' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}

// PIX
EXPORT int WINAPI												D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR wszName)
{
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);

	return 0;
}
EXPORT int WINAPI												D3DPERF_EndEvent(void)
{
	return 0;
}
EXPORT void WINAPI												D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR wszName)
{
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);
}
EXPORT void WINAPI												D3DPERF_SetRegion(D3DCOLOR col, LPCWSTR wszName)
{
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);
}
EXPORT BOOL WINAPI												D3DPERF_QueryRepeatFrame(void)
{
	return FALSE;
}
EXPORT void WINAPI												D3DPERF_SetOptions(DWORD dwOptions)
{
	UNREFERENCED_PARAMETER(dwOptions);

#ifdef _DEBUG
	static const auto trampoline = ReHook::Call(&D3DPERF_SetOptions);

	trampoline(0);
#endif
}
EXPORT DWORD WINAPI												D3DPERF_GetStatus(void)
{
	return 0;
}

// D3D9
EXPORT IDirect3D9 *WINAPI										Direct3DCreate9(UINT SDKVersion)
{
	LOG(INFO) << "Redirecting '" << "Direct3DCreate9" << "(" << SDKVersion << ")' ...";

	IDirect3D9 *res = ReHook::Call(&Direct3DCreate9)(SDKVersion);

	if (res != nullptr)
	{
		ReHook::Register(VTable(res, 16), reinterpret_cast<ReHook::Hook::Function>(&IDirect3D9_CreateDevice));
	}
	else
	{
		LOG(WARNING) << "> 'Direct3DCreate9' failed!";
	}

	return res;
}
EXPORT HRESULT WINAPI											Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **ppD3D)
{
	LOG(INFO) << "Redirecting '" << "Direct3DCreate9Ex" << "(" << SDKVersion << ", " << ppD3D << ")' ...";

	HRESULT hr = ReHook::Call(&Direct3DCreate9Ex)(SDKVersion, ppD3D);

	if (SUCCEEDED(hr))
	{
		ReHook::Register(VTable(*ppD3D, 16), reinterpret_cast<ReHook::Hook::Function>(&IDirect3D9_CreateDevice));
		ReHook::Register(VTable(*ppD3D, 20), reinterpret_cast<ReHook::Hook::Function>(&IDirect3D9Ex_CreateDeviceEx));
	}
	else
	{
		LOG(WARNING) << "> 'Direct3DCreate9Ex' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}