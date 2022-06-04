/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON && !RESHADE_ADDON_LITE

#include "d3d12_device.hpp"
#include "d3d12_resource.hpp"
#include "d3d12_impl_type_convert.hpp"
#include "com_utils.hpp"
#include "hook_manager.hpp"

using reshade::d3d12::to_handle;

// Monster Hunter Rise calls 'ID3D12Device::CopyDescriptorsSimple' on a device queried from a resource
// This crashes if that device pointer is not pointing to the proxy device, due to our modified descriptor handles, so need to make sure that is the case
HRESULT STDMETHODCALLTYPE ID3D12Resource_GetDevice(ID3D12Resource *pResource, REFIID riid, void **ppvDevice)
{
	const HRESULT hr = reshade::hooks::call(ID3D12Resource_GetDevice, vtable_from_instance(pResource) + 7)(pResource, riid, ppvDevice);
	if (SUCCEEDED(hr))
	{
		if (const auto device_proxy = get_private_pointer<D3D12Device>(static_cast<ID3D12Object *>(*ppvDevice)))
		{
			*ppvDevice = device_proxy;
			InterlockedIncrement(&device_proxy->_ref);
		}
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE ID3D12Resource_Map(ID3D12Resource *pResource, UINT Subresource, const D3D12_RANGE *pReadRange, void **ppData)
{
	const HRESULT hr = reshade::hooks::call(ID3D12Resource_Map, vtable_from_instance(pResource) + 8)(pResource, Subresource, pReadRange, ppData);
	if (FAILED(hr))
		return hr;

	com_ptr<ID3D12Device> device;
	pResource->GetDevice(IID_PPV_ARGS(&device));

	const auto device_proxy = get_private_pointer<D3D12Device>(device.get());
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
				reshade::api::map_access::read_write,
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
				reshade::api::map_access::read_write,
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
				reshade::api::map_access::read_write,
				nullptr);
		}
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE ID3D12Resource_Unmap(ID3D12Resource *pResource, UINT Subresource, const D3D12_RANGE *pWrittenRange)
{
	com_ptr<ID3D12Device> device;
	pResource->GetDevice(IID_PPV_ARGS(&device));

	const auto device_proxy = get_private_pointer<D3D12Device>(device.get());
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

	return reshade::hooks::call(ID3D12Resource_Unmap, vtable_from_instance(pResource) + 9)(pResource, Subresource, pWrittenRange);
}

#endif
