/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if RESHADE_ADDON && !RESHADE_ADDON_LITE

#include <d3d12.h>

struct D3D12Device;

struct DECLSPEC_UUID("2DCF7A2D-3824-4E6A-9F53-FE7C7D8B633D") D3D12PipelineLibrary final : ID3D12PipelineLibrary1
{
	D3D12PipelineLibrary(D3D12Device *device, ID3D12PipelineLibrary *original);

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region ID3D12Object
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) override;
	HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name) override;
	#pragma endregion
	#pragma region ID3D12DeviceChild
	HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **ppvDevice) override;
	#pragma endregion
	#pragma region ID3D12PipelineLibrary
	HRESULT STDMETHODCALLTYPE StorePipeline( LPCWSTR pName, ID3D12PipelineState *pPipeline) override;
	HRESULT STDMETHODCALLTYPE LoadGraphicsPipeline(LPCWSTR pName, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState) override;
	HRESULT STDMETHODCALLTYPE LoadComputePipeline(LPCWSTR pName, const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState) override;
	SIZE_T  STDMETHODCALLTYPE GetSerializedSize() override;
	HRESULT STDMETHODCALLTYPE Serialize(void *pData, SIZE_T DataSizeInBytes) override;
	#pragma endregion
	#pragma region ID3D12PipelineLibrary1
	HRESULT STDMETHODCALLTYPE LoadPipeline(LPCWSTR pName, const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc, REFIID riid, void **ppPipelineState) override;
	#pragma endregion

	bool check_and_upgrade_interface(REFIID riid);

	ID3D12PipelineLibrary *_orig;
	ULONG _ref = 1;
	unsigned int _interface_version = 0;
	D3D12Device *const _device;
};

#endif
