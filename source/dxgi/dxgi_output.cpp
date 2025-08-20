/*
 * Copyright (C) 2025 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dxgi_output.hpp"
#include "dxgi_adapter.hpp"
#include "dll_log.hpp"
#include "com_utils.hpp"

DXGIOutput::DXGIOutput(IDXGIOutput *original) :
	_orig(original),
	_interface_version(0)
{
	assert(_orig != nullptr);

	// Add proxy object to the private data of the output, so that it can be retrieved again when only the original output is available
	DXGIOutput *const output_proxy = this;
	_orig->SetPrivateData(__uuidof(DXGIOutput), sizeof(output_proxy), &output_proxy);
}
DXGIOutput::~DXGIOutput()
{
	// Remove pointer to this proxy object from the private data of the output
	_orig->SetPrivateData(__uuidof(DXGIOutput), 0, nullptr);
}

bool DXGIOutput::check_and_proxy_interface(REFIID riid, void **object)
{
	IDXGIOutput *output = nullptr;
	if (SUCCEEDED(static_cast<IUnknown *>(*object)->QueryInterface(&output)))
	{
		DXGIOutput *output_proxy = get_private_pointer_d3dx<DXGIOutput>(output);
		if (output_proxy)
		{
			output_proxy->_ref++;
		}
		else
		{
			output_proxy = new DXGIOutput(output);
			output_proxy->_temporary = true;
		}

		output->Release();

		if (output_proxy->check_and_upgrade_interface(riid))
		{
			*object = output_proxy;
			return true;
		}
		else // Do not hook object if we do not support the requested interface
		{
			delete output_proxy; // Delete instead of release to keep reference count untouched
		}
	}

	return false;
}

bool DXGIOutput::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDXGIObject))
		return true;

	static constexpr IID iid_lookup[] = {
		__uuidof(IDXGIOutput),  // {80A07424-AB52-42EB-833C-0C42FD282D98}
		__uuidof(IDXGIOutput1), // {00CDDEA8-939B-4B83-A340-A685226666CC}
		__uuidof(IDXGIOutput2), // {595E39D1-2724-4663-99B1-DA969DE28364}
		__uuidof(IDXGIOutput3), // {8A6BB301-7E7E-41F4-A8E7-7F4E7F1E3C9F}
		__uuidof(IDXGIOutput4), // {DC7DCA35-2196-414D-9F53-617884032A60}
		__uuidof(IDXGIOutput5), // {80A07424-AB52-42EB-833C-0C42FD282D98}
		__uuidof(IDXGIOutput6), // {068346E8-AAEC-4B84-ADD7-137F513F77A1}
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
				reshade::log::message(reshade::log::level::debug, "Upgrading IDXGIOutput%hu object %p to IDXGIOutput%hu.", _interface_version, this, version);
#endif
			_orig->Release();
			_orig = static_cast<IDXGIOutput *>(new_interface);
			_interface_version = version;
		}
		return true;
	}

	return false;
}

HRESULT STDMETHODCALLTYPE DXGIOutput::QueryInterface(REFIID riid, void **ppvObj)
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
ULONG   STDMETHODCALLTYPE DXGIOutput::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE DXGIOutput::Release()
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
		reshade::log::message(reshade::log::level::debug, "Destroying IDXGIOutput%hu object %p (%p).", interface_version, this, orig);
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (!temporary && ref_orig != 0)
		reshade::log::message(reshade::log::level::warning, "Reference count for IDXGIOutput%hu object %p (%p) is inconsistent (%lu).", interface_version, this, orig, ref_orig);
	return 0;
}

HRESULT STDMETHODCALLTYPE DXGIOutput::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
{
	return _orig->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::GetParent(REFIID riid, void **ppParent)
{
	const HRESULT hr = _orig->GetParent(riid, ppParent);
	if (SUCCEEDED(hr))
	{
		if (DXGIAdapter::check_and_proxy_interface(riid, ppParent))
		{
#if RESHADE_VERBOSE_LOG
			const auto adapter_proxy = static_cast<DXGIAdapter *>(*ppParent);
			reshade::log::message(reshade::log::level::debug, "IDXGIOutput::GetParent returning IDXGIAdapter%hu object %p (%p).", adapter_proxy->_interface_version, adapter_proxy, adapter_proxy->_orig);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in IDXGIOutput::GetParent.", reshade::log::iid_to_string(riid).c_str());
		}
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIOutput::GetDesc(DXGI_OUTPUT_DESC *pDesc)
{
	return _orig->GetDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::GetDisplayModeList(DXGI_FORMAT EnumFormat, UINT Flags, UINT *pNumModes, DXGI_MODE_DESC *pDesc)
{
	return _orig->GetDisplayModeList(EnumFormat, Flags, pNumModes, pDesc);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::FindClosestMatchingMode(const DXGI_MODE_DESC *pModeToMatch, DXGI_MODE_DESC *pClosestMatch, IUnknown *pConcernedDevice)
{
	return _orig->FindClosestMatchingMode(pModeToMatch, pClosestMatch, pConcernedDevice);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::WaitForVBlank()
{
	return _orig->WaitForVBlank();
}
HRESULT STDMETHODCALLTYPE DXGIOutput::TakeOwnership(IUnknown *pDevice, BOOL Exclusive)
{
	return _orig->TakeOwnership(pDevice, Exclusive);
}
void    STDMETHODCALLTYPE DXGIOutput::ReleaseOwnership()
{
	_orig->ReleaseOwnership();
}
HRESULT STDMETHODCALLTYPE DXGIOutput::GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps)
{
	return _orig->GetGammaControlCapabilities(pGammaCaps);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::SetGammaControl(const DXGI_GAMMA_CONTROL *pArray)
{
	return _orig->SetGammaControl(pArray);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::GetGammaControl(DXGI_GAMMA_CONTROL *pArray)
{
	return _orig->GetGammaControl(pArray);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::SetDisplaySurface(IDXGISurface *pScanoutSurface)
{
	return _orig->SetDisplaySurface(pScanoutSurface);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::GetDisplaySurfaceData(IDXGISurface *pDestination)
{
	return _orig->GetDisplaySurfaceData(pDestination);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats)
{
	return _orig->GetFrameStatistics(pStats);
}

HRESULT STDMETHODCALLTYPE DXGIOutput::GetDisplayModeList1(DXGI_FORMAT EnumFormat, UINT Flags, UINT *pNumModes, DXGI_MODE_DESC1 *pDesc)
{
	assert(_interface_version >= 1);
	return static_cast<IDXGIOutput1 *>(_orig)->GetDisplayModeList1(EnumFormat, Flags, pNumModes, pDesc);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::FindClosestMatchingMode1(const DXGI_MODE_DESC1 *pModeToMatch, DXGI_MODE_DESC1 *pClosestMatch, IUnknown *pConcernedDevice)
{
	assert(_interface_version >= 1);
	return static_cast<IDXGIOutput1 *>(_orig)->FindClosestMatchingMode1(pModeToMatch, pClosestMatch, pConcernedDevice);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::GetDisplaySurfaceData1(IDXGIResource *pDestination)
{
	assert(_interface_version >= 1);
	return static_cast<IDXGIOutput1 *>(_orig)->GetDisplaySurfaceData1(pDestination);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::DuplicateOutput(IUnknown *pDevice, IDXGIOutputDuplication **ppOutputDuplication)
{
	assert(_interface_version >= 1);
	return static_cast<IDXGIOutput1 *>(_orig)->DuplicateOutput(pDevice, ppOutputDuplication);
}

BOOL    STDMETHODCALLTYPE DXGIOutput::SupportsOverlays()
{
	assert(_interface_version >= 2);
	return static_cast<IDXGIOutput2 *>(_orig)->SupportsOverlays();
}

HRESULT STDMETHODCALLTYPE DXGIOutput::CheckOverlaySupport(DXGI_FORMAT EnumFormat, IUnknown *pConcernedDevice, UINT *pFlags)
{
	assert(_interface_version >= 3);
	return static_cast<IDXGIOutput3 *>(_orig)->CheckOverlaySupport(EnumFormat, pConcernedDevice, pFlags);
}

HRESULT STDMETHODCALLTYPE DXGIOutput::CheckOverlayColorSpaceSupport(DXGI_FORMAT Format, DXGI_COLOR_SPACE_TYPE ColorSpace, IUnknown *pConcernedDevice, UINT *pFlags)
{
	assert(_interface_version >= 4);
	return static_cast<IDXGIOutput4 *>(_orig)->CheckOverlayColorSpaceSupport(Format, ColorSpace, pConcernedDevice, pFlags);
}

HRESULT STDMETHODCALLTYPE DXGIOutput::DuplicateOutput1(IUnknown *pDevice, UINT Flags, UINT SupportedFormatsCount, const DXGI_FORMAT *pSupportedFormats, IDXGIOutputDuplication **ppOutputDuplication)
{
	assert(_interface_version >= 5);
	return static_cast<IDXGIOutput5 *>(_orig)->DuplicateOutput1(pDevice, Flags, SupportedFormatsCount, pSupportedFormats, ppOutputDuplication);
}

HRESULT STDMETHODCALLTYPE DXGIOutput::GetDesc1(DXGI_OUTPUT_DESC1 *pDesc)
{
	assert(_interface_version >= 6);
	return static_cast<IDXGIOutput6 *>(_orig)->GetDesc1(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGIOutput::CheckHardwareCompositionSupport(UINT *pFlags)
{
	assert(_interface_version >= 6);
	return static_cast<IDXGIOutput6 *>(_orig)->CheckHardwareCompositionSupport(pFlags);
}
