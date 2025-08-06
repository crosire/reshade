/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <cassert>
#include "dxgi_adapter.hpp"
#include "dxgi_factory.hpp"
#include "dxgi_output.hpp"
#include "dll_log.hpp"
#include "com_ptr.hpp"
#include "com_utils.hpp"

DXGIAdapter::DXGIAdapter(IDXGIAdapter *original)
	: _orig(original), _interface_version(0)
{
	assert(_orig != nullptr);

	// Add proxy object to the private data of the adapter, so that it can be retrieved again when only the original adapter is available
	DXGIAdapter *const adapter_proxy = this;
	_orig->SetPrivateData(__uuidof(DXGIAdapter), sizeof(adapter_proxy), &adapter_proxy);
}

DXGIAdapter::DXGIAdapter(IDXGIAdapter1 *original)
	: _orig(original), _interface_version(1)
{
	assert(_orig != nullptr);
	// Add proxy object to the private data of the adapter, so that it can be retrieved again when only the original adapter is available
	DXGIAdapter *const adapter_proxy = this;
	_orig->SetPrivateData(__uuidof(DXGIAdapter), sizeof(adapter_proxy), &adapter_proxy);
}

DXGIAdapter::~DXGIAdapter()
{
	_orig->SetPrivateData(__uuidof(DXGIAdapter), 0, nullptr); // Remove proxy object from the private data of the adapter
}

bool DXGIAdapter::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDXGIObject))
		return true;

	static constexpr IID iid_lookup[] = {
		__uuidof(IDXGIAdapter),  // 2411e7e1-12ac-4ccf-bd14-9798e8534dc0
		__uuidof(IDXGIAdapter1), // 29038f61-3839-4626-91fd-086879011a05
		__uuidof(IDXGIAdapter2), // 0AA1AE0A-FA0E-4B84-8644-E05FF8E5ACB5
		__uuidof(IDXGIAdapter3), // 645967A4-1392-4310-A798-8053CE3E93FD
		__uuidof(IDXGIAdapter4), // 3c8d99d1-4fbf-4181-a82c-af66bf7bd24e
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
		AddRef(); // Bump reference count to emulate QueryInterface
		*ppvObj = this;
		return S_OK;
	}
	return _orig->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE DXGIAdapter::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG STDMETHODCALLTYPE DXGIAdapter::Release()
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
	if (ref_orig != 0)
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
	HRESULT hr = _orig->GetParent(riid, ppParent);
	if (hr == S_OK)
	{
		IUnknown *const parent_unknown = static_cast<IUnknown *>(*ppParent);
		if (com_ptr<IDXGIFactory> factory;
			SUCCEEDED(parent_unknown->QueryInterface(&factory)))
		{
			auto factory_proxy = get_private_pointer_d3dx<DXGIFactory>(factory.get());
			if (factory_proxy == nullptr)
			{
				factory_proxy = new DXGIFactory(factory.get());
			}
			else
			{
				factory_proxy->_ref++;
			}

			if (factory_proxy->check_and_upgrade_interface(riid))
			{
#if RESHADE_VERBOSE_LOG
				reshade::log::message(reshade::log::level::debug, "DXGIAdapter::GetParent returning IDXGIFactory%hu object %p (%p).", factory_proxy->_interface_version, factory_proxy, factory_proxy->_orig);
#endif

				*ppParent = factory_proxy;
			}
			else // Do not hook object if we do not support the requested interface
			{
				reshade::log::message(reshade::log::level::warning, "Unknown interface %s in DXGIAdapter::GetParent.", reshade::log::iid_to_string(riid).c_str());

				delete factory_proxy; // Delete instead of release to keep reference count untouched
			}
		}
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIAdapter::EnumOutputs(UINT Output, IDXGIOutput **ppOutput)
{
	HRESULT hr = _orig->EnumOutputs(Output, ppOutput);
	if (hr == S_OK)
	{
		IDXGIOutput *const output = *ppOutput;
		auto output_proxy = get_private_pointer_d3dx<DXGIOutput>(output);
		if (output_proxy == nullptr)
		{
			output_proxy = new DXGIOutput(output);
		}
		else
		{
			output_proxy->_ref++;
		}
		*ppOutput = output_proxy;
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
	assert(_interface_version >= 3);
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
void STDMETHODCALLTYPE DXGIAdapter::UnregisterVideoMemoryBudgetChangeNotification(DWORD dwCookie)
{
	assert(_interface_version >= 3);
	static_cast<IDXGIAdapter3 *>(_orig)->UnregisterVideoMemoryBudgetChangeNotification(dwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIAdapter::GetDesc3(DXGI_ADAPTER_DESC3 *pDesc)
{
	assert(_interface_version >= 4);
	return static_cast<IDXGIAdapter4 *>(_orig)->GetDesc3(pDesc);
}

