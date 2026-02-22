/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON >= 2

#include "d3d12_device.hpp"
#include "d3d12_pipeline_library.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "com_utils.hpp"

D3D12PipelineLibrary::D3D12PipelineLibrary(D3D12Device *device, ID3D12PipelineLibrary *original) :
	_orig(original),
	_device(device)
{
	assert(_orig != nullptr && _device != nullptr);
}

bool D3D12PipelineLibrary::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) || // This is the IID_IUnknown identity object
		riid == __uuidof(ID3D12Object) ||
		riid == __uuidof(ID3D12DeviceChild))
		return true;

	static const IID iid_lookup[] = {
		__uuidof(ID3D12PipelineLibrary),
		__uuidof(ID3D12PipelineLibrary1),
	};

	for (unsigned int version = 0; version < ARRAYSIZE(iid_lookup); ++version)
	{
		if (riid != iid_lookup[version])
			continue;

		if (version > _interface_version)
		{
			IUnknown *new_interface = nullptr;
			if (FAILED(_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface))))
				return false;
#if RESHADE_VERBOSE_LOG
			reshade::log::message(reshade::log::level::debug, "Upgrading ID3D12PipelineLibrary%u object %p to ID3D12PipelineLibrary%u.", _interface_version, this, version);
#endif
			_orig->Release();
			_orig = static_cast<ID3D12PipelineLibrary *>(new_interface);
			_interface_version = version;
		}

		return true;
	}

	return false;
}

HRESULT STDMETHODCALLTYPE D3D12PipelineLibrary::QueryInterface(REFIID riid, void **ppvObj)
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
	if (riid == IID_UnwrappedObject)
	{
		_orig->AddRef();
		*ppvObj = _orig;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12PipelineLibrary::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D12PipelineLibrary::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
	{
		_orig->Release();
		return ref;
	}

	const auto orig = _orig;
	const auto interface_version = _interface_version;
#if 0
	reshade::log::message(reshade::log::level::debug, "Destroying ID3D12PipelineLibrary%u object %p (%p).", interface_version, this, orig);
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		reshade::log::message(reshade::log::level::warning, "Reference count for ID3D12PipelineLibrary%u object %p (%p) is inconsistent (%lu).", interface_version, this, orig, ref_orig);
	return 0;
}

HRESULT STDMETHODCALLTYPE D3D12PipelineLibrary::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12PipelineLibrary::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12PipelineLibrary::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}
HRESULT STDMETHODCALLTYPE D3D12PipelineLibrary::SetName(LPCWSTR Name)
{
	return _orig->SetName(Name);
}

HRESULT STDMETHODCALLTYPE D3D12PipelineLibrary::GetDevice(REFIID riid, void **ppvDevice)
{
	return _device->QueryInterface(riid, ppvDevice);
}

HRESULT STDMETHODCALLTYPE D3D12PipelineLibrary::StorePipeline(LPCWSTR pName, ID3D12PipelineState *pPipeline)
{
	return _orig->StorePipeline(pName, pPipeline);
}
HRESULT STDMETHODCALLTYPE D3D12PipelineLibrary::LoadGraphicsPipeline(LPCWSTR pName, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	// Do not invoke 'create_pipeline' event, since it is not possible to modify the pipeline

	HRESULT hr = _orig->LoadGraphicsPipeline(pName, pDesc, riid, ppPipelineState);
	if (SUCCEEDED(hr))
	{
		assert(pDesc != nullptr && ppPipelineState != nullptr);

		if (riid == __uuidof(ID3D12PipelineState))
		{
			_device->invoke_create_and_init_pipeline_event(*pDesc, *reinterpret_cast<ID3D12PipelineState **>(ppPipelineState), hr, false);
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		// 'E_INVALIDARG' is a common occurence indicating that the requested pipeline was not in the library
		reshade::log::message(reshade::log::level::warning, "ID3D12PipelineLibrary::LoadGraphicsPipeline failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12PipelineLibrary::LoadComputePipeline(LPCWSTR pName, const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	// Do not invoke 'create_pipeline' event, since it is not possible to modify the pipeline

	HRESULT hr = _orig->LoadComputePipeline(pName, pDesc, riid, ppPipelineState);
	if (SUCCEEDED(hr))
	{
		assert(pDesc != nullptr && ppPipelineState != nullptr);

		if (riid == __uuidof(ID3D12PipelineState))
		{
			_device->invoke_create_and_init_pipeline_event(*pDesc, *reinterpret_cast<ID3D12PipelineState **>(ppPipelineState), hr, false);
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12PipelineLibrary::LoadComputePipeline failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
SIZE_T  STDMETHODCALLTYPE D3D12PipelineLibrary::GetSerializedSize()
{
	return _orig->GetSerializedSize();
}
HRESULT STDMETHODCALLTYPE D3D12PipelineLibrary::Serialize(void *pData, SIZE_T DataSizeInBytes)
{
	return _orig->Serialize(pData, DataSizeInBytes);
}

HRESULT STDMETHODCALLTYPE D3D12PipelineLibrary::LoadPipeline(LPCWSTR pName, const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	assert(_interface_version >= 1);

	// Do not invoke 'create_pipeline' event, since it is not possible to modify the pipeline

	HRESULT hr = static_cast<ID3D12PipelineLibrary1 *>(_orig)->LoadPipeline(pName, pDesc, riid, ppPipelineState);
	if (SUCCEEDED(hr))
	{
		assert(pDesc != nullptr && ppPipelineState != nullptr);

		if (riid == __uuidof(ID3D12PipelineState))
		{
			_device->invoke_create_and_init_pipeline_event(*pDesc, *reinterpret_cast<ID3D12PipelineState **>(ppPipelineState), hr, false);
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12PipelineLibrary1::LoadPipeline failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}

#endif
