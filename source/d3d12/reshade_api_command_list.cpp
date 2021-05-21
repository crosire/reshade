/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "reshade_api_device.hpp"
#include "reshade_api_command_list.hpp"
#include "reshade_api_type_utils.hpp"
#include "reshade_api_format_utils.hpp"
#include <algorithm>

void encode_pix3blob(UINT64(&pix3blob)[64], const char *label, const float color[4])
{
	pix3blob[0] = (0x2ull /* PIXEvent_BeginEvent_NoArgs */ << 10);
	pix3blob[1] = 0xFF000000;
	if (color != nullptr)
		pix3blob[1] |= ((static_cast<DWORD>(color[0] * 255) & 0xFF) << 16) | ((static_cast<DWORD>(color[1] * 255) & 0xFF) << 8) | (static_cast<DWORD>(color[2] * 255) & 0xFF);
	pix3blob[2] = (8ull /* copyChunkSize */ << 55) | (1ull /* isANSI */ << 54);
	std::strncpy(reinterpret_cast<char *>(pix3blob + 3), label, sizeof(pix3blob) - (4 * sizeof(UINT64)));
	pix3blob[63] = 0;
}

reshade::d3d12::command_list_impl::command_list_impl(device_impl *device, ID3D12GraphicsCommandList *cmd_list) :
	api_object_impl(cmd_list), _device_impl(device), _has_commands(cmd_list != nullptr)
{
	if (_has_commands) // Do not call add-on event for immediate command list
		invoke_addon_event<addon_event::init_command_list>(this);
}
reshade::d3d12::command_list_impl::~command_list_impl()
{
	if (_has_commands)
		invoke_addon_event<addon_event::destroy_command_list>(this);
}

reshade::api::device *reshade::d3d12::command_list_impl::get_device()
{
	return _device_impl;
}

void reshade::d3d12::command_list_impl::barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states)
{
	if (count == 0)
		return;

	_has_commands = true;

	const auto barriers = static_cast<D3D12_RESOURCE_BARRIER *>(alloca(sizeof(D3D12_RESOURCE_BARRIER) * count));
	for (UINT i = 0; i < count; ++i)
	{
		if (old_states[i] == api::resource_usage::unordered_access && new_states[i] == api::resource_usage::unordered_access)
		{
			barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[i].UAV.pResource = reinterpret_cast<ID3D12Resource *>(resources[i].handle);
		}
		else
		{
			barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[i].Transition.pResource = reinterpret_cast<ID3D12Resource *>(resources[i].handle);
			barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barriers[i].Transition.StateBefore = convert_resource_usage_to_states(old_states[i]);
			barriers[i].Transition.StateAfter = convert_resource_usage_to_states(new_states[i]);
		}
	}

	_orig->ResourceBarrier(count, barriers);
}

void reshade::d3d12::command_list_impl::bind_pipeline(api::pipeline_type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);
	const auto pipeline_object = reinterpret_cast<ID3D12PipelineState *>(pipeline.handle);

	_orig->SetPipelineState(pipeline_object);

	pipeline_graphics_impl extra_data;
	UINT extra_data_size = sizeof(extra_data);
	if (SUCCEEDED(pipeline_object->GetPrivateData(pipeline_extra_data_guid, &extra_data_size, &extra_data)))
	{
		_orig->IASetPrimitiveTopology(extra_data.topology);
	}
}
void reshade::d3d12::command_list_impl::bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values)
{
	for (UINT i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::pipeline_state::stencil_reference_value:
			_orig->OMSetStencilRef(values[i]);
			break;
		case api::pipeline_state::blend_constant:
		{
			float blend_factor[4];
			blend_factor[0] = ((values[i]      ) & 0xFF) / 255.0f;
			blend_factor[1] = ((values[i] >>  4) & 0xFF) / 255.0f;
			blend_factor[2] = ((values[i] >>  8) & 0xFF) / 255.0f;
			blend_factor[3] = ((values[i] >> 12) & 0xFF) / 255.0f;

			_orig->OMSetBlendFactor(blend_factor);
			break;
		}
		case api::pipeline_state::primitive_topology:
			_orig->IASetPrimitiveTopology(convert_primitive_topology(static_cast<api::primitive_topology>(values[i])));
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::d3d12::command_list_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	assert(first == 0);

	_orig->RSSetViewports(count, reinterpret_cast<const D3D12_VIEWPORT *>(viewports));
}
void reshade::d3d12::command_list_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	assert(first == 0);

	_orig->RSSetScissorRects(count, reinterpret_cast<const D3D12_RECT *>(rects));
}

void reshade::d3d12::command_list_impl::push_constants(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, uint32_t first, uint32_t count, const void *values)
{
	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);
	if (stage == api::shader_stage::compute)
	{
		if (_current_root_signature[1] != root_signature)
		{
			_current_root_signature[1]  = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}

		_orig->SetComputeRoot32BitConstants(layout_index, count, values, first);
	}
	else
	{
		if (_current_root_signature[0] != root_signature)
		{
			_current_root_signature[0]  = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}

		_orig->SetGraphicsRoot32BitConstants(layout_index, count, values, first);
	}
}
void reshade::d3d12::command_list_impl::push_descriptors(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
{
	assert(first == 0);

	D3D12_CPU_DESCRIPTOR_HANDLE base_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu;
	if (type != api::descriptor_type::sampler ?
		!_device_impl->_gpu_view_heap.allocate_transient(first + count, base_handle, base_handle_gpu) :
		!_device_impl->_gpu_sampler_heap.allocate_transient(first + count, base_handle, base_handle_gpu))
		return;

	base_handle.ptr += first * _device_impl->_descriptor_handle_size[convert_descriptor_type_to_heap_type(type)];

	if (_current_descriptor_heaps[0] != _device_impl->_gpu_sampler_heap.get() ||
		_current_descriptor_heaps[1] != _device_impl->_gpu_view_heap.get())
	{
		ID3D12DescriptorHeap *const heaps[2] = { _device_impl->_gpu_sampler_heap.get(), _device_impl->_gpu_view_heap.get() };
		_orig->SetDescriptorHeaps(2, heaps);

		_current_descriptor_heaps[0] = heaps[0];
		_current_descriptor_heaps[1] = heaps[1];
	}

	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);
	if (stage == api::shader_stage::compute)
	{
		if (_current_root_signature[1] != root_signature)
		{
			_current_root_signature[1]  = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}
	}
	else
	{
		if (_current_root_signature[0] != root_signature)
		{
			_current_root_signature[0]  = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}
	}

#ifndef WIN64
	const UINT src_range_size = 1;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_handles(count);
	switch (type)
	{
	case api::descriptor_type::sampler:
		for (UINT i = 0; i < count; ++i)
		{
			src_handles[i] = { static_cast<SIZE_T>(static_cast<const api::sampler *>(descriptors)[i].handle) };
		}
		break;
	case api::descriptor_type::shader_resource_view:
	case api::descriptor_type::unordered_access_view:
		for (UINT i = 0; i < count; ++i)
		{
			src_handles[i] = { static_cast<SIZE_T>(static_cast<const api::resource_view *>(descriptors)[i].handle) };
		}
		break;
	case api::descriptor_type::constant_buffer:
		for (UINT i = 0; i < count; ++i)
		{
			src_handles[i] = { static_cast<SIZE_T>(static_cast<const api::resource *>(descriptors)[i].handle) };
		}
		break;
	}

	_device_impl->_orig->CopyDescriptors(1, &base_handle, &count, count, src_handles.data(), &src_range_size, convert_descriptor_type_to_heap_type(type));
#else
	std::vector<UINT> src_range_sizes(count, 1);
	_device_impl->_orig->CopyDescriptors(1, &base_handle, &count, count, static_cast<const D3D12_CPU_DESCRIPTOR_HANDLE *>(descriptors), src_range_sizes.data(), convert_descriptor_type_to_heap_type(type));
#endif

	if (stage == api::shader_stage::compute)
		_orig->SetComputeRootDescriptorTable(layout_index, base_handle_gpu);
	else
		_orig->SetGraphicsRootDescriptorTable(layout_index, base_handle_gpu);
}
void reshade::d3d12::command_list_impl::bind_descriptor_sets(api::pipeline_type type, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)
{
	if (_current_descriptor_heaps[0] != _device_impl->_gpu_sampler_heap.get() ||
		_current_descriptor_heaps[1] != _device_impl->_gpu_view_heap.get())
	{
		ID3D12DescriptorHeap *const heaps[2] = { _device_impl->_gpu_sampler_heap.get(), _device_impl->_gpu_view_heap.get() };
		_orig->SetDescriptorHeaps(2, heaps);

		_current_descriptor_heaps[0] = heaps[0];
		_current_descriptor_heaps[1] = heaps[1];
	}

	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);
	if (type == api::pipeline_type::compute)
	{
		if (_current_root_signature[1] != root_signature)
		{
			_current_root_signature[1]  = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}
	}
	else
	{
		if (_current_root_signature[0] != root_signature)
		{
			_current_root_signature[0]  = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}
	}

	for (uint32_t i = 0; i < count; ++i)
	{
		if (type == api::pipeline_type::compute)
			_orig->SetComputeRootDescriptorTable(i + first, D3D12_GPU_DESCRIPTOR_HANDLE { sets[i].handle });
		else
			_orig->SetGraphicsRootDescriptorTable(i + first, D3D12_GPU_DESCRIPTOR_HANDLE { sets[i].handle });
	}
}

void reshade::d3d12::command_list_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	if (buffer.handle != 0)
	{
		assert(index_size == 2 || index_size == 4);

		const auto buffer_ptr = reinterpret_cast<ID3D12Resource *>(buffer.handle);

		D3D12_INDEX_BUFFER_VIEW view;
		view.BufferLocation = buffer_ptr->GetGPUVirtualAddress() + offset;
		view.Format = index_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		view.SizeInBytes = static_cast<UINT>(buffer_ptr->GetDesc().Width - offset);

		_orig->IASetIndexBuffer(&view);
	}
	else
	{
		_orig->IASetIndexBuffer(nullptr);
	}
}
void reshade::d3d12::command_list_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	const auto views = static_cast<D3D12_VERTEX_BUFFER_VIEW *>(alloca(sizeof(D3D12_VERTEX_BUFFER_VIEW) * count));
	for (UINT i = 0; i < count; ++i)
	{
		const auto buffer_ptr = reinterpret_cast<ID3D12Resource *>(buffers[i].handle);

		views[i].BufferLocation = buffer_ptr->GetGPUVirtualAddress() + offsets[i];
		views[i].SizeInBytes = static_cast<UINT>(buffer_ptr->GetDesc().Width - offsets[i]);
		views[i].StrideInBytes = strides[i];
	}

	_orig->IASetVertexBuffers(first, count, views);
}

void reshade::d3d12::command_list_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	_has_commands = true;

	_orig->DrawInstanced(vertices, instances, first_vertex, first_instance);
}
void reshade::d3d12::command_list_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_has_commands = true;

	_orig->DrawIndexedInstanced(indices, instances, first_index, vertex_offset, first_instance);
}
void reshade::d3d12::command_list_impl::dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)
{
	_has_commands = true;

	_orig->Dispatch(num_groups_x, num_groups_y, num_groups_z);
}
void reshade::d3d12::command_list_impl::draw_or_dispatch_indirect(uint32_t, api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d12::command_list_impl::begin_render_pass(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	if (count > D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT)
	{
		assert(false);
		count = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	}

	com_ptr<ID3D12GraphicsCommandList4> cmd_list4;
	if (SUCCEEDED(_orig->QueryInterface(&cmd_list4)))
	{
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depth_stencil_desc = {};
		depth_stencil_desc.cpuDescriptor = { static_cast<SIZE_T>(dsv.handle) };
		depth_stencil_desc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
		depth_stencil_desc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		depth_stencil_desc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
		depth_stencil_desc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

		D3D12_RENDER_PASS_RENDER_TARGET_DESC render_target_desc[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
		for (UINT i = 0; i < count; ++i)
		{
			render_target_desc[i].cpuDescriptor = { static_cast<SIZE_T>(rtvs[i].handle) };
			render_target_desc[i].BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			render_target_desc[i].EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		}

		cmd_list4->BeginRenderPass(count, render_target_desc, dsv.handle != 0 ? &depth_stencil_desc : nullptr, D3D12_RENDER_PASS_FLAG_NONE);
	}
	else
	{
#ifndef WIN64
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
		for (UINT i = 0; i < count; ++i)
			rtv_handles[i] = { static_cast<SIZE_T>(rtvs[i].handle) };
#else
		const auto rtv_handles = reinterpret_cast<const D3D12_CPU_DESCRIPTOR_HANDLE *>(rtvs);
#endif
		const D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = { static_cast<SIZE_T>(dsv.handle) };

		_orig->OMSetRenderTargets(count, rtv_handles, FALSE, dsv.handle != 0 ? &dsv_handle : nullptr);
	}
}
void reshade::d3d12::command_list_impl::finish_render_pass()
{
	com_ptr<ID3D12GraphicsCommandList4> cmd_list4;
	if (SUCCEEDED(_orig->QueryInterface(&cmd_list4)))
	{
		cmd_list4->EndRenderPass();
	}
}

void reshade::d3d12::command_list_impl::copy_resource(api::resource src, api::resource dst)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	_orig->CopyResource(reinterpret_cast<ID3D12Resource *>(dst.handle), reinterpret_cast<ID3D12Resource *>(src.handle));
}
void reshade::d3d12::command_list_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	_orig->CopyBufferRegion(reinterpret_cast<ID3D12Resource *>(dst.handle), dst_offset, reinterpret_cast<ID3D12Resource *>(src.handle), src_offset, size);
}
void reshade::d3d12::command_list_impl::copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	const D3D12_RESOURCE_DESC res_desc = reinterpret_cast<ID3D12Resource *>(dst.handle)->GetDesc();

	D3D12_BOX src_box = {};
	if (dst_box != nullptr)
	{
		src_box.right = src_box.left + (dst_box[3] - dst_box[0]);
		src_box.bottom = src_box.top + (dst_box[4] - dst_box[1]);
		src_box.back = src_box.front + (dst_box[5] - dst_box[2]);
	}
	else
	{
		src_box.right = src_box.left + std::max(1u, static_cast<UINT>(res_desc.Width) >> (dst_subresource % res_desc.MipLevels));
		src_box.bottom = src_box.top + std::max(1u, res_desc.Height >> (dst_subresource % res_desc.MipLevels));
		src_box.back = src_box.front + (res_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? std::max(1u, static_cast<UINT>(res_desc.DepthOrArraySize) >> (dst_subresource % res_desc.MipLevels)) : 1u);
	}

	D3D12_TEXTURE_COPY_LOCATION src_copy_location;
	src_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(src.handle);
	src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src_copy_location.PlacedFootprint.Offset = src_offset;
	src_copy_location.PlacedFootprint.Footprint.Format = res_desc.Format;
	src_copy_location.PlacedFootprint.Footprint.Width = row_length != 0 ? row_length : src_box.right - src_box.left;
	src_copy_location.PlacedFootprint.Footprint.Height = slice_height != 0 ? slice_height : src_box.bottom - src_box.top;
	src_copy_location.PlacedFootprint.Footprint.Depth = src_box.back - src_box.front;
	src_copy_location.PlacedFootprint.Footprint.RowPitch = src_copy_location.PlacedFootprint.Footprint.Width * api::format_bpp(convert_format(res_desc.Format));
	src_copy_location.PlacedFootprint.Footprint.RowPitch = (src_copy_location.PlacedFootprint.Footprint.RowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);

	D3D12_TEXTURE_COPY_LOCATION dst_copy_location;
	dst_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst_copy_location.SubresourceIndex = dst_subresource;

	_orig->CopyTextureRegion(
		&dst_copy_location, dst_box != nullptr ? dst_box[0] : 0, dst_box != nullptr ? dst_box[1] : 0, dst_box != nullptr ? dst_box[2] : 0,
		&src_copy_location, &src_box);
}
void reshade::d3d12::command_list_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);
	assert((src_box == nullptr && dst_box == nullptr) || (src_box != nullptr && dst_box != nullptr &&
		(dst_box[3] - dst_box[0]) == (src_box[3] - src_box[0]) && // Blit between different region dimensions is not supported
		(dst_box[4] - dst_box[1]) == (src_box[4] - src_box[1]) &&
		(dst_box[5] - dst_box[2]) == (src_box[5] - src_box[2])));

	D3D12_TEXTURE_COPY_LOCATION src_copy_location;
	src_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(src.handle);
	src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_copy_location.SubresourceIndex = src_subresource;

	D3D12_TEXTURE_COPY_LOCATION dst_copy_location;
	dst_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst_copy_location.SubresourceIndex = dst_subresource;

	_orig->CopyTextureRegion(
		&dst_copy_location, dst_box != nullptr ? dst_box[0] : 0, dst_box != nullptr ? dst_box[1] : 0, dst_box != nullptr ? dst_box[2] : 0,
		&src_copy_location, reinterpret_cast<const D3D12_BOX *>(src_box));
}
void reshade::d3d12::command_list_impl::copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	const D3D12_RESOURCE_DESC res_desc = reinterpret_cast<ID3D12Resource *>(src.handle)->GetDesc();

	D3D12_TEXTURE_COPY_LOCATION src_copy_location;
	src_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(src.handle);
	src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_copy_location.SubresourceIndex = src_subresource;

	D3D12_TEXTURE_COPY_LOCATION dst_copy_location;
	dst_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_copy_location.PlacedFootprint.Offset = dst_offset;
	dst_copy_location.PlacedFootprint.Footprint.Format = res_desc.Format;
	dst_copy_location.PlacedFootprint.Footprint.Width = row_length != 0 ? row_length : std::max(1u, static_cast<UINT>(res_desc.Width) >> (src_subresource % res_desc.MipLevels));
	dst_copy_location.PlacedFootprint.Footprint.Height = slice_height != 0 ? slice_height : std::max(1u, res_desc.Height >> (src_subresource % res_desc.MipLevels));
	dst_copy_location.PlacedFootprint.Footprint.Depth = 1;
	dst_copy_location.PlacedFootprint.Footprint.RowPitch = dst_copy_location.PlacedFootprint.Footprint.Width * api::format_bpp(convert_format(res_desc.Format));
	dst_copy_location.PlacedFootprint.Footprint.RowPitch = (dst_copy_location.PlacedFootprint.Footprint.RowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);

	_orig->CopyTextureRegion(
		&dst_copy_location, 0, 0, 0,
		&src_copy_location, reinterpret_cast<const D3D12_BOX *>(src_box));
}
void reshade::d3d12::command_list_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], api::format format)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	if (src_box == nullptr && dst_offset == nullptr)
	{
		_orig->ResolveSubresource(
			reinterpret_cast<ID3D12Resource *>(dst.handle), dst_subresource,
			reinterpret_cast<ID3D12Resource *>(src.handle), src_subresource, convert_format(format));
	}
	else
	{
		com_ptr<ID3D12GraphicsCommandList1> cmd_list1;
		if (SUCCEEDED(_orig->QueryInterface(&cmd_list1)))
		{
			assert(src_box == nullptr || (src_box[2] == 0 && src_box[5] == 1));
			assert(dst_offset == nullptr || dst_offset[2] == 0);

			D3D12_RECT src_rect = { src_box[0], src_box[1], src_box[3], src_box[4] };

			cmd_list1->ResolveSubresourceRegion(
				reinterpret_cast<ID3D12Resource *>(dst.handle), dst_subresource, dst_offset != nullptr ? dst_offset[0] : 0, dst_offset != nullptr ? dst_offset[1] : 0,
				reinterpret_cast<ID3D12Resource *>(src.handle), src_subresource, &src_rect, convert_format(format), D3D12_RESOLVE_MODE_MIN);
		}
		else
		{
			assert(false);
		}
	}
}

void reshade::d3d12::command_list_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);
	api::resource resource_handle;
	_device_impl->get_resource_from_view(srv, &resource_handle);
	assert(resource_handle.handle != 0);

	const auto resource = reinterpret_cast<ID3D12Resource *>(resource_handle.handle);

	const D3D12_RESOURCE_DESC desc = resource->GetDesc();

	D3D12_CPU_DESCRIPTOR_HANDLE base_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu;
	if (!_device_impl->_gpu_view_heap.allocate_transient(desc.MipLevels * 2, base_handle, base_handle_gpu))
		return;

	for (uint32_t level = 0; level < desc.MipLevels; ++level, base_handle.ptr += _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV])
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
		srv_desc.Format = convert_format(api::format_to_default_typed(convert_format(desc.Format)));
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Texture2D.MostDetailedMip = level;
		srv_desc.Texture2D.PlaneSlice = 0;
		srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

		_device_impl->_orig->CreateShaderResourceView(resource, &srv_desc, base_handle);
	}
	for (uint32_t level = 1; level < desc.MipLevels; ++level, base_handle.ptr += _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV])
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		uav_desc.Format = convert_format(api::format_to_default_typed(convert_format(desc.Format)));
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice = level;
		uav_desc.Texture2D.PlaneSlice = 0;

		_device_impl->_orig->CreateUnorderedAccessView(resource, nullptr, &uav_desc, base_handle);
	}

	const auto view_heap = _device_impl->_gpu_view_heap.get();
	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap)
		_orig->SetDescriptorHeaps(1, &view_heap);

	_orig->SetComputeRootSignature(_device_impl->_mipmap_signature.get());
	_orig->SetPipelineState(_device_impl->_mipmap_pipeline.get());

	D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transition.Transition.pResource = resource;
	transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transition.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	transition.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	_orig->ResourceBarrier(1, &transition);

	for (uint32_t level = 1; level < desc.MipLevels; ++level)
	{
		const uint32_t width = std::max(1u, static_cast<uint32_t>(desc.Width) >> level);
		const uint32_t height = std::max(1u, desc.Height >> level);

		const float dimensions[2] = { 1.0f / width, 1.0f / height };
		_orig->SetComputeRoot32BitConstants(0, 2, dimensions, 0);

		// Bind next higher mipmap level as input
		_orig->SetComputeRootDescriptorTable(1, { base_handle_gpu.ptr + _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * (level - 1) });
		// There is no UAV for level 0, so substract one
		_orig->SetComputeRootDescriptorTable(2, { base_handle_gpu.ptr + _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * (desc.MipLevels + level - 1) });

		_orig->Dispatch(std::max(1u, (width + 7) / 8), std::max(1u, (height + 7) / 8), 1);

		// Wait for all accesses to be finished, since the result will be the input for the next mipmap
		D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_UAV };
		barrier.UAV.pResource = transition.Transition.pResource;
		_orig->ResourceBarrier(1, &barrier);
	}

	std::swap(transition.Transition.StateBefore, transition.Transition.StateAfter);
	_orig->ResourceBarrier(1, &transition);

	// Reset root signature and descriptor heaps
	_orig->SetComputeRootSignature(_current_root_signature[1]);

	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap && _current_descriptor_heaps[0] != nullptr)
		_orig->SetDescriptorHeaps(_current_descriptor_heaps[1] != nullptr ? 2 : 1, _current_descriptor_heaps);
}

void reshade::d3d12::command_list_impl::clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	_has_commands = true;

	assert(dsv.handle != 0);

	_orig->ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(dsv.handle) }, static_cast<D3D12_CLEAR_FLAGS>(clear_flags), depth, stencil, 0, nullptr);
}
void reshade::d3d12::command_list_impl::clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4])
{
	_has_commands = true;

	for (UINT i = 0; i < count; ++i)
	{
		assert(rtvs[i].handle != 0);

		_orig->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(rtvs[i].handle) }, color, 0, nullptr);
	}
}
void reshade::d3d12::command_list_impl::clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4])
{
	_has_commands = true;

	assert(uav.handle != 0);
	api::resource resource_handle;
	_device_impl->get_resource_from_view(uav, &resource_handle);
	assert(resource_handle.handle != 0);

	const auto resource = reinterpret_cast<ID3D12Resource *>(resource_handle.handle);

	D3D12_CPU_DESCRIPTOR_HANDLE table_base;
	D3D12_GPU_DESCRIPTOR_HANDLE table_base_gpu;
	if (!_device_impl->_gpu_view_heap.allocate_transient(1, table_base, table_base_gpu))
		return;

	const auto view_heap = _device_impl->_gpu_view_heap.get();
	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap)
		_orig->SetDescriptorHeaps(1, &view_heap);

	_device_impl->_orig->CreateUnorderedAccessView(resource, nullptr, nullptr, table_base);
	_orig->ClearUnorderedAccessViewUint(table_base_gpu, D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(uav.handle) }, resource, values, 0, nullptr);

	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap && _current_descriptor_heaps[0] != nullptr)
		_orig->SetDescriptorHeaps(_current_descriptor_heaps[1] != nullptr ? 2 : 1, _current_descriptor_heaps);
}
void reshade::d3d12::command_list_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4])
{
	_has_commands = true;

	assert(uav.handle != 0);
	api::resource resource_handle;
	_device_impl->get_resource_from_view(uav, &resource_handle);
	assert(resource_handle.handle != 0);

	const auto resource = reinterpret_cast<ID3D12Resource *>(resource_handle.handle);

	D3D12_CPU_DESCRIPTOR_HANDLE table_base;
	D3D12_GPU_DESCRIPTOR_HANDLE table_base_gpu;
	if (!_device_impl->_gpu_view_heap.allocate_transient(1, table_base, table_base_gpu))
		return;

	const auto view_heap = _device_impl->_gpu_view_heap.get();
	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap)
		_orig->SetDescriptorHeaps(1, &view_heap);

	_device_impl->_orig->CreateUnorderedAccessView(resource, nullptr, nullptr, table_base);
	_orig->ClearUnorderedAccessViewFloat(table_base_gpu, D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(uav.handle) }, resource, values, 0, nullptr);

	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap && _current_descriptor_heaps[0] != nullptr)
		_orig->SetDescriptorHeaps(_current_descriptor_heaps[1] != nullptr ? 2 : 1, _current_descriptor_heaps);
}

void reshade::d3d12::command_list_impl::begin_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	_has_commands = true;

	assert(pool.handle != 0);

	_orig->BeginQuery(reinterpret_cast<ID3D12QueryHeap *>(pool.handle), convert_query_type(type), index);
}
void reshade::d3d12::command_list_impl::finish_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	_has_commands = true;

	assert(pool.handle != 0);

	const auto heap_object = reinterpret_cast<ID3D12QueryHeap *>(pool.handle);
	const auto d3d_query_type = convert_query_type(type);
	_orig->EndQuery(heap_object, d3d_query_type, index);

	com_ptr<ID3D12Resource> readback_resource;
	UINT private_size = sizeof(ID3D12Resource *);
	if (SUCCEEDED(heap_object->GetPrivateData(pipeline_extra_data_guid, &private_size, &readback_resource)))
	{
		_orig->ResolveQueryData(reinterpret_cast<ID3D12QueryHeap *>(pool.handle), convert_query_type(type), index, 1, readback_resource.get(), index * sizeof(uint64_t));
	}
}
void reshade::d3d12::command_list_impl::copy_query_results(api::query_pool pool, api::query_type type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride)
{
	_has_commands = true;

	assert(pool.handle != 0);
	assert(stride == sizeof(uint64_t));

	_orig->ResolveQueryData(reinterpret_cast<ID3D12QueryHeap *>(pool.handle), convert_query_type(type), first, count, reinterpret_cast<ID3D12Resource *>(dst.handle), dst_offset);
}

void reshade::d3d12::command_list_impl::add_debug_marker(const char *label, const float color[4])
{
#if 0
	_orig->SetMarker(1, label, static_cast<UINT>(strlen(label)));
#else
	UINT64 pix3blob[64];
	encode_pix3blob(pix3blob, label, color);
	_orig->SetMarker(2, pix3blob, sizeof(pix3blob));
#endif
}
void reshade::d3d12::command_list_impl::begin_debug_marker(const char *label, const float color[4])
{
#if 0
	// Metadata is WINPIX_EVENT_ANSI_VERSION
	_orig->BeginEvent(1, label, static_cast<UINT>(strlen(label)));
#else
	UINT64 pix3blob[64];
	encode_pix3blob(pix3blob, label, color);
	// Metadata is WINPIX_EVENT_PIX3BLOB_VERSION
	_orig->BeginEvent(2, pix3blob, sizeof(pix3blob));
#endif
}
void reshade::d3d12::command_list_impl::finish_debug_marker()
{
	_orig->EndEvent();
}
