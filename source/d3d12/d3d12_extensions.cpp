/*
 * Copyright (C) 2026 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON >= 2

#include "d3d12_device.hpp"
#include "d3d12_extensions.hpp"
#include "com_utils.hpp"
#include "hook_manager.hpp"

static D3D12_CPU_DESCRIPTOR_HANDLE convert_to_original_cpu_descriptor_handle(const D3D12Device *device_proxy, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	// These hooks are installed on the vtable shared by all vkd3d-proton device objects, so they also receive handles from descriptor heaps that were created on the original device rather than through the proxy (e.g. by code using the native device via the add-on API)
	// Those were never converted to the internal format, so have to be passed through unchanged
	return device_proxy->is_internal_cpu_descriptor_handle(handle) ? device_proxy->convert_to_original_cpu_descriptor_handle(handle) : handle;
}

HRESULT ID3D12DeviceExt_GetCudaTextureObject(IUnknown *device_ext, D3D12_CPU_DESCRIPTOR_HANDLE srv_handle, D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle, UINT32 *cuda_texture_handle)
{
	com_ptr<ID3D12Device> device;
	device_ext->QueryInterface(IID_PPV_ARGS(&device));
	assert(device != nullptr);

	if (const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get()))
	{
		srv_handle = convert_to_original_cpu_descriptor_handle(device_proxy, srv_handle);
		sampler_handle = convert_to_original_cpu_descriptor_handle(device_proxy, sampler_handle);
	}

	return reshade::hooks::call(ID3D12DeviceExt_GetCudaTextureObject, reshade::hooks::vtable_from_instance(device_ext) + 7)(device_ext, srv_handle, sampler_handle, cuda_texture_handle);
}

HRESULT ID3D12DeviceExt_GetCudaSurfaceObject(IUnknown *device_ext, D3D12_CPU_DESCRIPTOR_HANDLE uav_handle, UINT32 *cuda_surface_handle)
{
	com_ptr<ID3D12Device> device;
	device_ext->QueryInterface(IID_PPV_ARGS(&device));
	assert(device != nullptr);

	if (const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get()))
	{
		uav_handle = convert_to_original_cpu_descriptor_handle(device_proxy, uav_handle);
	}

	return reshade::hooks::call(ID3D12DeviceExt_GetCudaSurfaceObject, reshade::hooks::vtable_from_instance(device_ext) + 8)(device_ext, uav_handle, cuda_surface_handle);
}

HRESULT ID3D12DeviceExt2_GetCudaMergedTextureSamplerObject(IUnknown *device_ext, D3D12_GET_CUDA_MERGED_TEXTURE_SAMPLER_OBJECT_PARAMS *params)
{
	com_ptr<ID3D12Device> device;
	device_ext->QueryInterface(IID_PPV_ARGS(&device));
	assert(device != nullptr);

	if (const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get()))
	{
		params->texDesc = convert_to_original_cpu_descriptor_handle(device_proxy, params->texDesc);
		params->smpDesc = convert_to_original_cpu_descriptor_handle(device_proxy, params->smpDesc);
	}

	return reshade::hooks::call(ID3D12DeviceExt2_GetCudaMergedTextureSamplerObject, reshade::hooks::vtable_from_instance(device_ext) + 14)(device_ext, params);
}

HRESULT ID3D12DeviceExt2_GetCudaIndependentDescriptorObject(IUnknown *device_ext, D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_PARAMS *params)
{
	com_ptr<ID3D12Device> device;
	device_ext->QueryInterface(IID_PPV_ARGS(&device));
	assert(device != nullptr);

	if (const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get()))
	{
		params->desc = convert_to_original_cpu_descriptor_handle(device_proxy, params->desc);
	}

	return reshade::hooks::call(ID3D12DeviceExt2_GetCudaIndependentDescriptorObject, reshade::hooks::vtable_from_instance(device_ext) + 15)(device_ext, params);
}

#endif
