/*
 * Copyright (C) 2025 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dxgi_adapter.hpp"
#include "dll_log.hpp"
#include "com_utils.hpp"

DXGIAdapter::DXGIAdapter(IDXGIFactory *factory, IDXGIAdapter  *original) :
	_orig(original),
	_interface_version(0),
	_parent_factory(factory)
{
	assert(_orig != nullptr && _parent_factory != nullptr);
	_parent_factory->AddRef();

	// Add proxy object to the private data of the adapter, so that it can be retrieved again when only the original adapter is available
	DXGIAdapter *const adapter_proxy = this;
	_orig->SetPrivateData(__uuidof(DXGIAdapter), sizeof(adapter_proxy), &adapter_proxy);
}
DXGIAdapter::DXGIAdapter(IDXGIFactory *factory, IDXGIAdapter1 *original) :
	DXGIAdapter(factory, static_cast<IDXGIAdapter *>(original))
{
	_interface_version = 1;
}
DXGIAdapter::~DXGIAdapter()
{
	// Remove pointer to this proxy object from the private data of the adapter
	_orig->SetPrivateData(__uuidof(DXGIAdapter), 0, nullptr);

	_parent_factory->Release();
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

	// Interface ID to query the original object from a proxy object
	constexpr GUID IID_UnwrappedObject = { 0x7f2c9a11, 0x3b4e, 0x4d6a, { 0x81, 0x2f, 0x5e, 0x9c, 0xd3, 0x7a, 0x1b, 0x42 } }; // {7F2C9A11-3B4E-4D6A-812F-5E9CD37A1B42}
	if (riid == IID_UnwrappedObject)
	{
		_orig->AddRef();
		*ppvObj = _orig;
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
#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Destroying IDXGIAdapter%hu object %p (%p).", interface_version, this, orig);
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig > 1) // One reference can still be held by a DXGI device
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
	return _parent_factory->QueryInterface(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE DXGIAdapter::EnumOutputs(UINT Output, IDXGIOutput **ppOutput)
{
	return _orig->EnumOutputs(Output, ppOutput);
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
void    STDMETHODCALLTYPE DXGIAdapter::UnregisterHardwareContentProtectionTeardownStatus(DWORD dwCookie)
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

