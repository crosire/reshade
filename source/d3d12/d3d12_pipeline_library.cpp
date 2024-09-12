/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON >= 2

#include "d3d12_device.hpp"
#include "d3d12_pipeline_library.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "com_utils.hpp"
#include "hook_manager.hpp"

HRESULT STDMETHODCALLTYPE ID3D12PipelineLibrary_LoadGraphicsPipeline(ID3D12PipelineLibrary *pPipelineLibrary, LPCWSTR pName, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	// Do not invoke 'create_pipeline' event, since it is not possible to modify the pipeline

	HRESULT hr = reshade::hooks::call(ID3D12PipelineLibrary_LoadGraphicsPipeline, reshade::hooks::vtable_from_instance(pPipelineLibrary) + 9)(pPipelineLibrary, pName, pDesc, riid, ppPipelineState);
	if (SUCCEEDED(hr))
	{
		assert(pDesc != nullptr && ppPipelineState != nullptr);

		com_ptr<ID3D12Device> device;
		pPipelineLibrary->GetDevice(IID_PPV_ARGS(&device));
		assert(device != nullptr);

		const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get());
		if (device_proxy != nullptr &&
			riid == __uuidof(ID3D12PipelineState))
		{
			device_proxy->invoke_create_and_init_pipeline_event(*pDesc, *reinterpret_cast<ID3D12PipelineState **>(ppPipelineState), hr, false);
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
HRESULT STDMETHODCALLTYPE ID3D12PipelineLibrary_LoadComputePipeline(ID3D12PipelineLibrary *pPipelineLibrary, LPCWSTR pName, const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	// Do not invoke 'create_pipeline' event, since it is not possible to modify the pipeline

	HRESULT hr = reshade::hooks::call(ID3D12PipelineLibrary_LoadComputePipeline, reshade::hooks::vtable_from_instance(pPipelineLibrary) + 10)(pPipelineLibrary, pName, pDesc, riid, ppPipelineState);
	if (SUCCEEDED(hr))
	{
		assert(pDesc != nullptr && ppPipelineState != nullptr);

		com_ptr<ID3D12Device> device;
		pPipelineLibrary->GetDevice(IID_PPV_ARGS(&device));
		assert(device != nullptr);

		const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get());
		if (device_proxy != nullptr &&
			riid == __uuidof(ID3D12PipelineState))
		{
			device_proxy->invoke_create_and_init_pipeline_event(*pDesc, *reinterpret_cast<ID3D12PipelineState **>(ppPipelineState), hr, false);
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

HRESULT STDMETHODCALLTYPE ID3D12PipelineLibrary1_LoadPipeline(ID3D12PipelineLibrary1 *pPipelineLibrary, LPCWSTR pName, const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	// Do not invoke 'create_pipeline' event, since it is not possible to modify the pipeline

	HRESULT hr = reshade::hooks::call(ID3D12PipelineLibrary1_LoadPipeline, reshade::hooks::vtable_from_instance(pPipelineLibrary) + 13)(pPipelineLibrary, pName, pDesc, riid, ppPipelineState);
	if (SUCCEEDED(hr))
	{
		assert(pDesc != nullptr && ppPipelineState != nullptr);

		com_ptr<ID3D12Device> device;
		pPipelineLibrary->GetDevice(IID_PPV_ARGS(&device));
		assert(device != nullptr);

		const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get());
		if (device_proxy != nullptr &&
			riid == __uuidof(ID3D12PipelineState))
		{
			device_proxy->invoke_create_and_init_pipeline_event(*pDesc, *reinterpret_cast<ID3D12PipelineState **>(ppPipelineState), hr, false);
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
