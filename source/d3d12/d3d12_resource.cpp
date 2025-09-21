/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d12_device.hpp"
#include "d3d12_resource.hpp"
#include "com_utils.hpp"
#include "hook_manager.hpp"

extern std::shared_mutex g_adapter_mutex;

// Monster Hunter Rise calls 'ID3D12Device::CopyDescriptorsSimple' on a device queried from a resource
// This crashes if that device pointer is not pointing to the proxy device, due to our modified descriptor handles, so need to make sure that is the case
HRESULT STDMETHODCALLTYPE ID3D12Resource_GetDevice(ID3D12Resource *pResource, REFIID riid, void **ppvDevice)
{
	const HRESULT hr = reshade::hooks::call(ID3D12Resource_GetDevice, reshade::hooks::vtable_from_instance(pResource) + 7)(pResource, riid, ppvDevice);
	if (FAILED(hr))
		return hr;

	const auto device = static_cast<ID3D12Device *>(*ppvDevice);
	assert(device != nullptr);

	const std::unique_lock<std::shared_mutex> lock(g_adapter_mutex);

	const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device);
	if (device_proxy != nullptr && device_proxy->_orig == device)
	{
		InterlockedIncrement(&device_proxy->_ref);
		*ppvDevice = device_proxy;
	}

	return hr;
}

#if RESHADE_ADDON >= 2

#include "d3d12_impl_type_convert.hpp"
#include "addon_manager.hpp"

using reshade::d3d12::to_handle;

HRESULT STDMETHODCALLTYPE ID3D12Resource_Map(ID3D12Resource *pResource, UINT Subresource, const D3D12_RANGE *pReadRange, void **ppData)
{
	const HRESULT hr = reshade::hooks::call(ID3D12Resource_Map, reshade::hooks::vtable_from_instance(pResource) + 8)(pResource, Subresource, pReadRange, ppData);
	if (FAILED(hr))
		return hr;

	com_ptr<ID3D12Device> device;
	pResource->GetDevice(IID_PPV_ARGS(&device));
	assert(device != nullptr);

	const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get());
	if (device_proxy != nullptr)
	{
		const D3D12_RESOURCE_DESC desc = pResource->GetDesc();

		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			assert(Subresource == 0);

			reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
				device_proxy,
				to_handle(pResource),
				0,
				std::numeric_limits<uint64_t>::max(),
				pReadRange != nullptr && pReadRange->End <= pReadRange->Begin ? reshade::api::map_access::write_only : reshade::api::map_access::read_write,
				ppData);
		}
		else if (ppData != nullptr)
		{
			reshade::api::subresource_data data;
			data.data = *ppData;

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
			device->GetCopyableFootprints(&desc, Subresource, 1, 0, &layout, &data.slice_pitch, nullptr, nullptr);
			data.row_pitch = layout.Footprint.RowPitch;
			data.slice_pitch *= layout.Footprint.RowPitch;

			reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
				device_proxy,
				to_handle(pResource),
				Subresource,
				nullptr,
				pReadRange != nullptr && pReadRange->End <= pReadRange->Begin ? reshade::api::map_access::write_only : reshade::api::map_access::read_write,
				&data);

			*ppData = data.data;
		}
		else
		{
			reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
				device_proxy,
				to_handle(pResource),
				Subresource,
				nullptr,
				pReadRange != nullptr && pReadRange->End <= pReadRange->Begin ? reshade::api::map_access::write_only : reshade::api::map_access::read_write,
				nullptr);
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE ID3D12Resource_Unmap(ID3D12Resource *pResource, UINT Subresource, const D3D12_RANGE *pWrittenRange)
{
	com_ptr<ID3D12Device> device;
	pResource->GetDevice(IID_PPV_ARGS(&device));
	assert(device != nullptr);

	const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get());
	if (device_proxy != nullptr)
	{
		const D3D12_RESOURCE_DESC desc = pResource->GetDesc();

		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			assert(Subresource == 0);

			reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(device_proxy, to_handle(pResource));
		}
		else
		{
			reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(device_proxy, to_handle(pResource), Subresource);
		}
	}

	return reshade::hooks::call(ID3D12Resource_Unmap, reshade::hooks::vtable_from_instance(pResource) + 9)(pResource, Subresource, pWrittenRange);
}

#endif
