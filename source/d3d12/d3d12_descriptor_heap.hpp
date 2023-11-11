/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if RESHADE_ADDON >= 2

struct D3D12Device;

struct DECLSPEC_UUID("8628AD68-6047-4D27-9D87-3E5F386E0231") D3D12DescriptorHeap final : ID3D12DescriptorHeap
{
	D3D12DescriptorHeap(ID3D12Device *device, ID3D12DescriptorHeap *original);

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
	#pragma region ID3D12DescriptorHeap
	D3D12_DESCRIPTOR_HEAP_DESC  STDMETHODCALLTYPE GetDesc() override;
	D3D12_CPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetCPUDescriptorHandleForHeapStart() override;
	D3D12_GPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetGPUDescriptorHandleForHeapStart() override;
	#pragma endregion

	bool check_and_upgrade_interface(REFIID riid);

	void initialize_descriptor_base_handle(size_t heap_index);

	ID3D12DescriptorHeap *const _orig;
	ULONG _ref = 1;
	ID3D12Device *const _device;

	D3D12_CPU_DESCRIPTOR_HANDLE _orig_base_cpu_handle = { 0 };
	D3D12_GPU_DESCRIPTOR_HANDLE _orig_base_gpu_handle = { 0 };
	D3D12_CPU_DESCRIPTOR_HANDLE _internal_base_cpu_handle = { 0 };
};

#endif
