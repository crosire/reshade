/*
 * Copyright (C) 2025 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dxgi_factory.hpp"
#include "dxgi_adapter.hpp"
#include "dxgi_device.hpp"
#include "dll_log.hpp"
#include "com_utils.hpp"

DXGIFactory::DXGIFactory(IDXGIFactory *original) :
	_orig(original),
	_interface_version(0)
{
	assert(_orig != nullptr);

	// Add proxy object to the private data of the factory, so that it can be retrieved again when only the original factory is available
	DXGIFactory *const factory_proxy = this;
	_orig->SetPrivateData(__uuidof(DXGIFactory), sizeof(factory_proxy), &factory_proxy);
}
DXGIFactory::~DXGIFactory()
{
	// Remove pointer to this proxy object from the private data of the factory
	_orig->SetPrivateData(__uuidof(DXGIFactory), 0, nullptr);
}

bool DXGIFactory::check_and_proxy_interface(REFIID riid, void **object)
{
	IDXGIFactory *factory = nullptr;
	if (SUCCEEDED(static_cast<IUnknown *>(*object)->QueryInterface(&factory)))
	{
		DXGIFactory *factory_proxy = get_private_pointer_d3dx<DXGIFactory>(factory);
		if (factory_proxy)
		{
			factory_proxy->_ref++;
		}
		else
		{
			factory_proxy = new DXGIFactory(factory);
			factory_proxy->_temporary = true;
		}

		factory->Release();

		if (factory_proxy->check_and_upgrade_interface(riid))
		{
			*object = factory_proxy;
			return true;
		}
		else // Do not hook object if we do not support the requested interface
		{
			delete factory_proxy; // Delete instead of release to keep reference count untouched
		}
	}

	return false;
}

bool DXGIFactory::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDXGIObject))
		return true;

	static constexpr IID iid_lookup[] = {
		__uuidof(IDXGIFactory),
		__uuidof(IDXGIFactory1),
		__uuidof(IDXGIFactory2),
		__uuidof(IDXGIFactory3),
		__uuidof(IDXGIFactory4),
		__uuidof(IDXGIFactory5),
		__uuidof(IDXGIFactory6),
		__uuidof(IDXGIFactory7),
	};

	for (unsigned short version = 0; version < ARRAYSIZE(iid_lookup); ++version)
	{
		if (riid != iid_lookup[version])
			continue;

		if (version > _interface_version)
		{
			IUnknown *new_interface = nullptr;
			if (FAILED(_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface))))
				return false;
#if RESHADE_VERBOSE_LOG
			if (!_temporary)
				reshade::log::message(reshade::log::level::debug, "Upgrading IDXGIFactory%hu object %p to IDXGIFactory%hu.", _interface_version, this, version);
#endif
			_orig->Release();
			_orig = static_cast<IDXGIFactory *>(new_interface);
			_interface_version = version;
		}

		return true;
	}

	return false;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	if (riid == ID_IDeviceChildParent)
	{
		_orig->AddRef();
		*ppvObj = _orig;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE DXGIFactory::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE DXGIFactory::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
	{
		_orig->Release();
		return ref;
	}

	const auto orig = _orig;
	const auto interface_version = _interface_version;
	const bool temporary = _temporary;
#if RESHADE_VERBOSE_LOG
	if (!temporary)
		reshade::log::message(reshade::log::level::debug, "Destroying IDXGIFactory%hu object %p (%p).", interface_version, this, orig);
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (!temporary && ref_orig != 0) // Verify internal reference count
		reshade::log::message(reshade::log::level::warning, "Reference count for IDXGIFactory%hu object %p (%p) is inconsistent (%lu).", interface_version, this, orig, ref_orig);
	return 0;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
{
	return _orig->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::GetParent(REFIID riid, void **ppParent)
{
	return _orig->GetParent(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapters(UINT Adapter, IDXGIAdapter **ppAdapter)
{
	const HRESULT hr = _orig->EnumAdapters(Adapter, ppAdapter);
	if (SUCCEEDED(hr))
		DXGIAdapter::check_and_proxy_interface(ppAdapter);
	return hr;
}
HRESULT STDMETHODCALLTYPE DXGIFactory::MakeWindowAssociation(HWND WindowHandle, UINT Flags)
{
	return _orig->MakeWindowAssociation(WindowHandle, Flags);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::GetWindowAssociation(HWND *pWindowHandle)
{
	return _orig->GetWindowAssociation(pWindowHandle);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChain(IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
	return IDXGIFactory_CreateSwapChain_Impl(this, pDevice, pDesc, ppSwapChain,
		[](IDXGIFactory *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain) -> HRESULT {
			return static_cast<DXGIFactory *>(pFactory)->_orig->CreateSwapChain(pDevice, pDesc, ppSwapChain);
		});
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSoftwareAdapter(HMODULE Module, IDXGIAdapter **ppAdapter)
{
	const HRESULT hr = _orig->CreateSoftwareAdapter(Module, ppAdapter);
	if (SUCCEEDED(hr))
		DXGIAdapter::check_and_proxy_interface(ppAdapter);
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapters1(UINT Adapter, IDXGIAdapter1 **ppAdapter)
{
	assert(_interface_version >= 1);
	const HRESULT hr = static_cast<IDXGIFactory1 *>(_orig)->EnumAdapters1(Adapter, ppAdapter);
	if (SUCCEEDED(hr))
		DXGIAdapter::check_and_proxy_interface(ppAdapter);
	return hr;
}
BOOL    STDMETHODCALLTYPE DXGIFactory::IsCurrent()
{
	assert(_interface_version >= 1);
	return static_cast<IDXGIFactory1 *>(_orig)->IsCurrent();
}

BOOL    STDMETHODCALLTYPE DXGIFactory::IsWindowedStereoEnabled()
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->IsWindowedStereoEnabled();
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChainForHwnd(IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	assert(_interface_version >= 2);
	return IDXGIFactory2_CreateSwapChainForHwnd_Impl(this, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain,
		[](IDXGIFactory2 *pFactory, IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) -> HRESULT {
			return static_cast<IDXGIFactory2 *>(static_cast<DXGIFactory *>(pFactory)->_orig)->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
		});
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChainForCoreWindow(IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	assert(_interface_version >= 2);
	return IDXGIFactory2_CreateSwapChainForCoreWindow_Impl(this, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain,
		[](IDXGIFactory2 *pFactory, IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) -> HRESULT {
			return static_cast<IDXGIFactory2 *>(static_cast<DXGIFactory *>(pFactory)->_orig)->CreateSwapChainForCoreWindow(pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
		});
}
HRESULT STDMETHODCALLTYPE DXGIFactory::GetSharedResourceAdapterLuid(HANDLE hResource, LUID *pLuid)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->GetSharedResourceAdapterLuid(hResource, pLuid);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterStereoStatusWindow(HWND WindowHandle, UINT wMsg, DWORD *pdwCookie)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->RegisterStereoStatusWindow(WindowHandle, wMsg, pdwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterStereoStatusEvent(HANDLE hEvent, DWORD *pdwCookie)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->RegisterStereoStatusEvent(hEvent, pdwCookie);
}
void    STDMETHODCALLTYPE DXGIFactory::UnregisterStereoStatus(DWORD dwCookie)
{
	assert(_interface_version >= 2);
	static_cast<IDXGIFactory2 *>(_orig)->UnregisterStereoStatus(dwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterOcclusionStatusWindow(HWND WindowHandle, UINT wMsg, DWORD *pdwCookie)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterOcclusionStatusEvent(HANDLE hEvent, DWORD *pdwCookie)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIFactory2 *>(_orig)->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
}
void    STDMETHODCALLTYPE DXGIFactory::UnregisterOcclusionStatus(DWORD dwCookie)
{
	assert(_interface_version >= 2);
	static_cast<IDXGIFactory2 *>(_orig)->UnregisterOcclusionStatus(dwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChainForComposition(IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	assert(_interface_version >= 2);
	return IDXGIFactory2_CreateSwapChainForComposition_Impl(this, pDevice, pDesc, pRestrictToOutput, ppSwapChain,
		[](IDXGIFactory2 *pFactory, IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain) -> HRESULT {
			return static_cast<IDXGIFactory2 *>(static_cast<DXGIFactory *>(pFactory)->_orig)->CreateSwapChainForComposition(pDevice, pDesc, pRestrictToOutput, ppSwapChain);
		});
}

UINT    STDMETHODCALLTYPE DXGIFactory::GetCreationFlags()
{
	assert(_interface_version >= 3);
	return static_cast<IDXGIFactory3 *>(_orig)->GetCreationFlags();
}

HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapterByLuid(LUID AdapterLuid, REFIID riid, void **ppvAdapter)
{
	assert(_interface_version >= 4);
	const HRESULT hr = static_cast<IDXGIFactory4 *>(_orig)->EnumAdapterByLuid(AdapterLuid, riid, ppvAdapter);
	if (SUCCEEDED(hr))
	{
		if (DXGIAdapter::check_and_proxy_interface(riid, ppvAdapter))
		{
#if RESHADE_VERBOSE_LOG
			const auto adapter_proxy = static_cast<DXGIAdapter *>(*ppvAdapter);
			reshade::log::message(reshade::log::level::debug, "IDXGIFactory4::EnumAdapterByLuid returning IDXGIAdapter%hu object %p (%p).", adapter_proxy->_interface_version, adapter_proxy, adapter_proxy->_orig);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in IDXGIFactory4::EnumAdapterByLuid.", reshade::log::iid_to_string(riid).c_str());
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE DXGIFactory::EnumWarpAdapter(REFIID riid, void **ppvAdapter)
{
	assert(_interface_version >= 4);
	const HRESULT hr = static_cast<IDXGIFactory4 *>(_orig)->EnumWarpAdapter(riid, ppvAdapter);
	if (SUCCEEDED(hr))
	{
		if (DXGIAdapter::check_and_proxy_interface(riid, ppvAdapter))
		{
#if RESHADE_VERBOSE_LOG
			const auto adapter_proxy = static_cast<DXGIAdapter *>(*ppvAdapter);
			reshade::log::message(reshade::log::level::debug, "IDXGIFactory4::EnumWarpAdapter returning IDXGIAdapter%hu object %p (%p).", adapter_proxy->_interface_version, adapter_proxy, adapter_proxy->_orig);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in IDXGIFactory4::EnumWarpAdapter.", reshade::log::iid_to_string(riid).c_str());
		}
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::CheckFeatureSupport(DXGI_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize)
{
	assert(_interface_version >= 5);
	return static_cast<IDXGIFactory5 *>(_orig)->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}

HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapterByGpuPreference(UINT Adapter, DXGI_GPU_PREFERENCE GpuPreference, REFIID riid, void **ppvAdapter)
{
	assert(_interface_version >= 6);
	const HRESULT hr = static_cast<IDXGIFactory6 *>(_orig)->EnumAdapterByGpuPreference(Adapter, GpuPreference, riid, ppvAdapter);
	if (SUCCEEDED(hr))
	{
		if (DXGIAdapter::check_and_proxy_interface(riid, ppvAdapter))
		{
#if RESHADE_VERBOSE_LOG
			const auto adapter_proxy = static_cast<DXGIAdapter *>(*ppvAdapter);
			reshade::log::message(reshade::log::level::debug, "IDXGIFactory6::EnumAdapterByGpuPreference returning IDXGIAdapter%hu object %p (%p).", adapter_proxy->_interface_version, adapter_proxy, adapter_proxy->_orig);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in IDXGIFactory6::EnumAdapterByGpuPreference.", reshade::log::iid_to_string(riid).c_str());
		}
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterAdaptersChangedEvent(HANDLE hEvent, DWORD *pdwCookie)
{
	assert(_interface_version >= 7);
	return static_cast<IDXGIFactory7 *>(_orig)->RegisterAdaptersChangedEvent(hEvent, pdwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::UnregisterAdaptersChangedEvent(DWORD dwCookie)
{
	assert(_interface_version >= 7);
	return static_cast<IDXGIFactory7 *>(_orig)->UnregisterAdaptersChangedEvent(dwCookie);
}
