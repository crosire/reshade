/*
 * Copyright (C) 2026 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if RESHADE_ADDON >= 2

struct D3D12_GET_CUDA_MERGED_TEXTURE_SAMPLER_OBJECT_PARAMS
{
	void *pNext;
	D3D12_CPU_DESCRIPTOR_HANDLE texDesc;
	D3D12_CPU_DESCRIPTOR_HANDLE smpDesc;
	UINT64 textureHandle;
};

struct D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_PARAMS
{
	void *pNext;
	enum D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_TYPE type;
	D3D12_CPU_DESCRIPTOR_HANDLE desc;
	UINT64 handle;
};

// See https://github.com/HansKristian-Work/vkd3d-proton/blob/master/include/vkd3d_device_vkd3d_ext.idl
HRESULT ID3D12DeviceExt_GetCudaTextureObject(IUnknown *device_ext, D3D12_CPU_DESCRIPTOR_HANDLE srv_handle, D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle, UINT32 *cuda_texture_handle);
HRESULT ID3D12DeviceExt_GetCudaSurfaceObject(IUnknown *device_ext, D3D12_CPU_DESCRIPTOR_HANDLE uav_handle, UINT32 *cuda_surface_handle);

HRESULT ID3D12DeviceExt2_GetCudaMergedTextureSamplerObject(IUnknown *device_ext, D3D12_GET_CUDA_MERGED_TEXTURE_SAMPLER_OBJECT_PARAMS *params);
HRESULT ID3D12DeviceExt2_GetCudaIndependentDescriptorObject(IUnknown *device_ext, D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_PARAMS *params);

#endif
