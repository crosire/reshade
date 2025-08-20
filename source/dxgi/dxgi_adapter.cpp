/*
 * Copyright (C) 2025 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dxgi_output.hpp"
#include "dxgi_factory.hpp"
#include "dxgi_adapter.hpp"
#include "dll_log.hpp"
#include "com_utils.hpp"

DXGIAdapter::DXGIAdapter(IDXGIAdapter  *original) :
	_orig(original),
	_interface_version(0)
{
	assert(_orig != nullptr);

	// Add proxy object to the private data of the adapter, so that it can be retrieved again when only the original adapter is available
	DXGIAdapter *const adapter_proxy = this;
	_orig->SetPrivateData(__uuidof(DXGIAdapter), sizeof(adapter_proxy), &adapter_proxy);
}
DXGIAdapter::DXGIAdapter(IDXGIAdapter1 *original) :
	_orig(original),
	_interface_version(1)
{
	assert(_orig != nullptr);

	DXGIAdapter *const adapter_proxy = this;
	_orig->SetPrivateData(__uuidof(DXGIAdapter), sizeof(adapter_proxy), &adapter_proxy);
}
DXGIAdapter::~DXGIAdapter()
{
	// Remove pointer to this proxy object from the private data of the adapter
	_orig->SetPrivateData(__uuidof(DXGIAdapter), 0, nullptr);
}

bool DXGIAdapter::check_and_proxy_interface(REFIID riid, void **object)
{
	IDXGIAdapter *adapter = nullptr;
	if (SUCCEEDED(static_cast<IUnknown *>(*object)->QueryInterface(&adapter)))
	{
		DXGIAdapter *adapter_proxy = get_private_pointer_d3dx<DXGIAdapter>(adapter);
		if (adapter_proxy)
		{
			adapter_proxy->_ref++;
		}
		else
		{
			adapter_proxy = new DXGIAdapter(adapter);
			adapter_proxy->_temporary = true;
		}

		adapter->Release();

		if (adapter_proxy->check_and_upgrade_interface(riid))
		{
			*object = adapter_proxy;
			return true;
		}
		else // Do not hook object if we do not support the requested interface
		{
			delete adapter_proxy; // Delete instead of release to keep reference count untouched
		}
	}

	return false;
}

bool DXGIAdapter::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDXGIObject))
		return true;

	static constexpr IID iid_lookup[] = {
		__uuidof(IDXGIAdapter),  // {2411E7E1-12AC-4CCF-BD14-9798E8534dC0}
		__uuidof(IDXGIAdapter1), // {29038F61-3839-4626-91FD-086879011A05}
		__uuidof(IDXGIAdapter2), // {0AA1AE0A-FA0E-4B84-8644-E05FF8E5ACB5}
		__uuidof(IDXGIAdapter3), // {645967A4-1392-4310-A798-8053CE3E93FD}
		__uuidof(IDXGIAdapter4), // {3C8D99D1-4FBF-4181-A82C-AF66BF7BD24E}
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
				reshade::log::message(reshade::log::level::debug, "Upgrading IDXGIAdapter%hu object %p to IDXGIAdapter%hu.", _interface_version, this, version);
#endif
			_orig->Release();
			_orig = static_cast<IDXGIAdapter *>(new_interface);
			_interface_version = version;
		}

		return true;
	}

	return false;
}


HRESULT STDMETHODCALLTYPE DXGIAdapter::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE DXGIAdapter::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE DXGIAdapter::Release()
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
		reshade::log::message(reshade::log::level::debug, "Destroying IDXGIAdapter%hu object %p (%p).", interface_version, this, orig);
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (!temporary && ref_orig != 0)
		reshade::log::message(reshade::log::level::warning, "Reference count for IDXGIAdapter%hu object %p (%p) is inconsistent (%lu).", interface_version, this, orig, ref_orig);
	return 0;
}

HRESULT STDMETHODCALLTYPE DXGIAdapter::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIAdapter::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
{
	return _orig->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE DXGIAdapter::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIAdapter::GetParent(REFIID riid, void **ppParent)
{
	const HRESULT hr = _orig->GetParent(riid, ppParent);
	if (SUCCEEDED(hr))
	{
		if (DXGIFactory::check_and_proxy_interface(riid, ppParent))
		{
#if RESHADE_VERBOSE_LOG
			const auto factory_proxy = static_cast<DXGIFactory *>(*ppParent);
			reshade::log::message(reshade::log::level::debug, "IDXGIAdapter::GetParent returning IDXGIFactory%hu object %p (%p).", factory_proxy->_interface_version, factory_proxy, factory_proxy->_orig);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in IDXGIAdapter::GetParent.", reshade::log::iid_to_string(riid).c_str());
		}
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIAdapter::EnumOutputs(UINT Output, IDXGIOutput **ppOutput)
{
	const HRESULT hr = _orig->EnumOutputs(Output, ppOutput);
	if (SUCCEEDED(hr))
	{
		DXGIOutput::check_and_proxy_interface(ppOutput);
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE DXGIAdapter::GetDesc(DXGI_ADAPTER_DESC *pDesc)
{
	return _orig->GetDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGIAdapter::CheckInterfaceSupport(REFGUID InterfaceName, LARGE_INTEGER *pUMDVersion)
{
	return _orig->CheckInterfaceSupport(InterfaceName, pUMDVersion);
}

HRESULT STDMETHODCALLTYPE DXGIAdapter::GetDesc1(DXGI_ADAPTER_DESC1 *pDesc)
{
	assert(_interface_version >= 1);
	return static_cast<IDXGIAdapter1 *>(_orig)->GetDesc1(pDesc);
}

HRESULT STDMETHODCALLTYPE DXGIAdapter::GetDesc2(DXGI_ADAPTER_DESC2 *pDesc)
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIAdapter2 *>(_orig)->GetDesc2(pDesc);
}

HRESULT STDMETHODCALLTYPE DXGIAdapter::RegisterHardwareContentProtectionTeardownStatusEvent(HANDLE hEvent, DWORD *pdwCookie)
{
	assert(_interface_version >= 3);
	return static_cast<IDXGIAdapter3 *>(_orig)->RegisterHardwareContentProtectionTeardownStatusEvent(hEvent, pdwCookie);
}
void STDMETHODCALLTYPE DXGIAdapter::UnregisterHardwareContentProtectionTeardownStatus(DWORD dwCookie)
{
	assert(_interface_version >= 3);
	static_cast<IDXGIAdapter3 *>(_orig)->UnregisterHardwareContentProtectionTeardownStatus(dwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIAdapter::QueryVideoMemoryInfo(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup, DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo)
{
	// assert(_interface_version >= 3); // Grand Theft Auto V Enhanced Edition incorrectly calls this on a 'IDXGIAdapter' object
	return static_cast<IDXGIAdapter3 *>(_orig)->QueryVideoMemoryInfo(NodeIndex, MemorySegmentGroup, pVideoMemoryInfo);
}
HRESULT STDMETHODCALLTYPE DXGIAdapter::SetVideoMemoryReservation(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup, UINT64 Reservation)
{
	assert(_interface_version >= 3);
	return static_cast<IDXGIAdapter3 *>(_orig)->SetVideoMemoryReservation(NodeIndex, MemorySegmentGroup, Reservation);
}
HRESULT STDMETHODCALLTYPE DXGIAdapter::RegisterVideoMemoryBudgetChangeNotificationEvent(HANDLE hEvent, DWORD *pdwCookie)
{
	assert(_interface_version >= 3);
	return static_cast<IDXGIAdapter3 *>(_orig)->RegisterVideoMemoryBudgetChangeNotificationEvent(hEvent, pdwCookie);
}
void    STDMETHODCALLTYPE DXGIAdapter::UnregisterVideoMemoryBudgetChangeNotification(DWORD dwCookie)
{
	assert(_interface_version >= 3);
	static_cast<IDXGIAdapter3 *>(_orig)->UnregisterVideoMemoryBudgetChangeNotification(dwCookie);
}

HRESULT STDMETHODCALLTYPE DXGIAdapter::GetDesc3(DXGI_ADAPTER_DESC3 *pDesc)
{
	assert(_interface_version >= 4);
	return static_cast<IDXGIAdapter4 *>(_orig)->GetDesc3(pDesc);
}

