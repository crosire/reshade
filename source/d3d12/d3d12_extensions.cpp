/*
 * Copyright (C) 2026 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON >= 2

#include "d3d12_device.hpp"
#include "d3d12_extensions.hpp"
#include "d3d12_impl_type_convert.hpp"
#include <cstddef>

namespace
{
	template <typename T>
	T get_vtable_entry(IUnknown *object, std::size_t index)
	{
		return reinterpret_cast<T>((*reinterpret_cast<void ***>(object))[index]);
	}
}

D3D12DeviceExt::D3D12DeviceExt(D3D12Device *device) :
	_parent_device(device)
{
	assert(_parent_device != nullptr);
}
D3D12DeviceExt::~D3D12DeviceExt()
{
	if (_orig != nullptr)
		_orig->Release();
}

HRESULT D3D12DeviceExt::check_and_upgrade_interface(REFIID riid)
{
	static constexpr IID iid_lookup[] = {
		IID_ID3D12DeviceExt,
		IID_ID3D12DeviceExt1,
		IID_ID3D12DeviceExt2,
		IID_ID3D12DeviceExt3,
		IID_ID3D12DeviceExt4,
		IID_ID3D12DeviceExt5,
	};

	for (unsigned short version = 0; version < sizeof(iid_lookup) / sizeof(iid_lookup[0]); ++version)
	{
		if (riid != iid_lookup[version])
			continue;

		if (_orig == nullptr || version > _interface_version)
		{
			IUnknown *new_interface = nullptr;
			const HRESULT hr = _parent_device->_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface));
			if (FAILED(hr))
				return hr;

			if (_orig != nullptr)
				_orig->Release();

			_orig = new_interface;
			_interface_version = version;
		}

		return S_OK;
	}

	return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE D3D12DeviceExt::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == IID_ID3D12DeviceExt ||
		riid == IID_ID3D12DeviceExt1 ||
		riid == IID_ID3D12DeviceExt2 ||
		riid == IID_ID3D12DeviceExt3 ||
		riid == IID_ID3D12DeviceExt4 ||
		riid == IID_ID3D12DeviceExt5)
	{
		const HRESULT hr = check_and_upgrade_interface(riid);
		if (FAILED(hr))
		{
			*ppvObj = nullptr;
			return hr;
		}

		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	// Preserve the identity of the parent D3D12 device proxy for IUnknown and all other interfaces.
	return _parent_device->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE D3D12DeviceExt::AddRef()
{
	return _parent_device->AddRef();
}
ULONG STDMETHODCALLTYPE D3D12DeviceExt::Release()
{
	return _parent_device->Release();
}

HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetVulkanHandles(void *vk_instance, void *vk_physical_device, void *vk_device)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, void *, void *, void *);
	return get_vtable_entry<function_type>(_orig, 3)(_orig, vk_instance, vk_physical_device, vk_device);
}
BOOL STDMETHODCALLTYPE D3D12DeviceExt::GetExtensionSupport(UINT extension)
{
	using function_type = BOOL (STDMETHODCALLTYPE *)(IUnknown *, UINT);
	return get_vtable_entry<function_type>(_orig, 4)(_orig, extension);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::CreateCubinComputeShaderWithName(const void *cubin_data, UINT32 cubin_size, UINT32 block_x, UINT32 block_y, UINT32 block_z, const char *shader_name, void **handle)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, const void *, UINT32, UINT32, UINT32, UINT32, const char *, void **);
	return get_vtable_entry<function_type>(_orig, 5)(_orig, cubin_data, cubin_size, block_x, block_y, block_z, shader_name, handle);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::DestroyCubinComputeShader(void *handle)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, void *);
	return get_vtable_entry<function_type>(_orig, 6)(_orig, handle);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetCudaTextureObject(D3D12_CPU_DESCRIPTOR_HANDLE srv_handle, D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle, UINT32 *cuda_texture_handle)
{
	srv_handle = _parent_device->convert_to_original_cpu_descriptor_handle(srv_handle);
	sampler_handle = _parent_device->convert_to_original_cpu_descriptor_handle(sampler_handle);

	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE, UINT32 *);
	return get_vtable_entry<function_type>(_orig, 7)(_orig, srv_handle, sampler_handle, cuda_texture_handle);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetCudaSurfaceObject(D3D12_CPU_DESCRIPTOR_HANDLE uav_handle, UINT32 *cuda_surface_handle)
{
	uav_handle = _parent_device->convert_to_original_cpu_descriptor_handle(uav_handle);

	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, D3D12_CPU_DESCRIPTOR_HANDLE, UINT32 *);
	return get_vtable_entry<function_type>(_orig, 8)(_orig, uav_handle, cuda_surface_handle);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::CaptureUAVInfo(void *uav_info)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, void *);
	return get_vtable_entry<function_type>(_orig, 9)(_orig, uav_info);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::CreateResourceFromBorrowedHandle(const D3D12_RESOURCE_DESC1 *desc, UINT64 vk_handle, ID3D12Resource **resource)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, const D3D12_RESOURCE_DESC1 *, UINT64, ID3D12Resource **);
	return get_vtable_entry<function_type>(_orig, 10)(_orig, desc, vk_handle, resource);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetVulkanQueueInfoEx(ID3D12CommandQueue *queue, void *vk_queue, UINT32 *vk_queue_index, UINT32 *vk_queue_flags, UINT32 *vk_queue_family)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, ID3D12CommandQueue *, void *, UINT32 *, UINT32 *, UINT32 *);
	return get_vtable_entry<function_type>(_orig, 11)(_orig, queue, vk_queue, vk_queue_index, vk_queue_flags, vk_queue_family);
}
BOOL STDMETHODCALLTYPE D3D12DeviceExt::SupportsCubin64bit()
{
	using function_type = BOOL (STDMETHODCALLTYPE *)(IUnknown *);
	return get_vtable_entry<function_type>(_orig, 12)(_orig);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::CreateCubinComputeShaderExV2(void *params)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, void *);
	return get_vtable_entry<function_type>(_orig, 13)(_orig, params);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetCudaMergedTextureSamplerObject(D3D12_GET_CUDA_MERGED_TEXTURE_SAMPLER_OBJECT_PARAMS *params)
{
	params->texDesc = _parent_device->convert_to_original_cpu_descriptor_handle(params->texDesc);
	params->smpDesc = _parent_device->convert_to_original_cpu_descriptor_handle(params->smpDesc);

	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, D3D12_GET_CUDA_MERGED_TEXTURE_SAMPLER_OBJECT_PARAMS *);
	return get_vtable_entry<function_type>(_orig, 14)(_orig, params);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetCudaIndependentDescriptorObject(D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_PARAMS *params)
{
	params->desc = _parent_device->convert_to_original_cpu_descriptor_handle(params->desc);

	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_PARAMS *);
	return get_vtable_entry<function_type>(_orig, 15)(_orig, params);
}
BOOL STDMETHODCALLTYPE D3D12DeviceExt::SupportsAGSExtension(UINT ags_extension)
{
	using function_type = BOOL (STDMETHODCALLTYPE *)(IUnknown *, UINT);
	return get_vtable_entry<function_type>(_orig, 16)(_orig, ags_extension);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::SetAGSUAVSlot(UINT uav_slot)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, UINT);
	return get_vtable_entry<function_type>(_orig, 17)(_orig, uav_slot);
}
BOOL STDMETHODCALLTYPE D3D12DeviceExt::IsNvShaderExtnOpCodeSupported(UINT32 op_code)
{
	using function_type = BOOL (STDMETHODCALLTYPE *)(IUnknown *, UINT32);
	return get_vtable_entry<function_type>(_orig, 18)(_orig, op_code);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::SetNvShaderExtnSlotSpace(UINT32 uav_slot, UINT32 uav_space, BOOL local_thread)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, UINT32, UINT32, BOOL);
	return get_vtable_entry<function_type>(_orig, 19)(_orig, uav_slot, uav_space, local_thread);
}
BOOL STDMETHODCALLTYPE D3D12DeviceExt::SetCreatePipelineStateFlagsNVAPI(UINT pipeline_state_flags)
{
	using function_type = BOOL (STDMETHODCALLTYPE *)(IUnknown *, UINT);
	return get_vtable_entry<function_type>(_orig, 20)(_orig, pipeline_state_flags);
}

#endif
