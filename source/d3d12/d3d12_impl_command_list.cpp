/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d12_impl_device.hpp"
#include "d3d12_impl_command_list.hpp"
#include "d3d12_impl_type_convert.hpp"
#include <malloc.h>
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
	_current_fbo = new framebuffer_impl();

#if RESHADE_ADDON
	if (_has_commands) // Do not call add-on event for immediate command list
		invoke_addon_event<addon_event::init_command_list>(this);
#endif
}
reshade::d3d12::command_list_impl::~command_list_impl()
{
#if RESHADE_ADDON
	if (_has_commands)
		invoke_addon_event<addon_event::destroy_command_list>(this);
#endif

	delete _current_fbo;
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

	const auto barriers = static_cast<D3D12_RESOURCE_BARRIER *>(_malloca(count * sizeof(D3D12_RESOURCE_BARRIER)));

	for (uint32_t i = 0; i < count; ++i)
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

	_freea(barriers);
}

void reshade::d3d12::command_list_impl::begin_render_pass(api::render_pass pass, api::framebuffer fbo, uint32_t clear_value_count, const void *clear_values)
{
	assert(pass.handle != 0 && fbo.handle != 0);

	const auto fbo_impl = reinterpret_cast<const framebuffer_impl *>(fbo.handle);

	// It is not allowed to call "ClearRenderTargetView", "ClearDepthStencilView" etc. inside a render pass, which would break the "command_impl::clear_attachments" implementation, so use plain old "OMSetRenderTargets" instead of render pass API
	_orig->OMSetRenderTargets(fbo_impl->count, fbo_impl->rtv, fbo_impl->rtv_is_single_handle_to_range, fbo_impl->dsv.ptr != 0 ? &fbo_impl->dsv : nullptr);

	std::memcpy(_current_fbo, fbo_impl, sizeof(framebuffer_impl));

	if (clear_value_count == 0)
		return;

	for (const api::attachment_desc &attach : reinterpret_cast<const render_pass_impl *>(pass.handle)->attachments)
	{
		if (attach.type == api::attachment_type::color)
		{
			if (attach.color_or_depth_load_op == api::attachment_load_op::clear)
			{
				assert(clear_value_count != 0);

				_orig->ClearRenderTargetView(fbo_impl->rtv[attach.index], static_cast<const float *>(clear_values), 0, nullptr);
			}
		}
		else
		{
			if (const UINT clear_flags = ((attach.color_or_depth_load_op == api::attachment_load_op::clear) ? D3D12_CLEAR_FLAG_DEPTH : 0) | ((attach.stencil_load_op == api::attachment_load_op::clear) ? D3D12_CLEAR_FLAG_STENCIL : 0))
			{
				assert(clear_value_count != 0);

				_orig->ClearDepthStencilView(fbo_impl->dsv, static_cast<D3D12_CLEAR_FLAGS>(clear_flags),
					static_cast<const float *>(clear_values)[0],
					reinterpret_cast<const uint32_t &>(static_cast<const float *>(clear_values)[1]) & 0xFF, 0, nullptr);
			}
		}

		clear_values = static_cast<const float *>(clear_values) + 4;
		clear_value_count--;
	}
}
void reshade::d3d12::command_list_impl::end_render_pass()
{
	_current_fbo->count = 0;
	_current_fbo->dsv.ptr = 0;
}
void reshade::d3d12::command_list_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	if (count > D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT)
	{
		assert(false);
		count = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	}

#ifndef WIN64
	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for (uint32_t i = 0; i < count; ++i)
		rtv_handles[i] = { static_cast<SIZE_T>(rtvs[i].handle) };
#else
	const auto rtv_handles = reinterpret_cast<const D3D12_CPU_DESCRIPTOR_HANDLE *>(rtvs);
#endif
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = { static_cast<SIZE_T>(dsv.handle) };

	_orig->OMSetRenderTargets(count, rtv_handles, FALSE, dsv.handle != 0 ? &dsv_handle : nullptr);
}

void reshade::d3d12::command_list_impl::bind_pipeline(api::pipeline_stage type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);
	assert(type == api::pipeline_stage::all_compute || type == api::pipeline_stage::all_graphics);

	const auto pipeline_object = reinterpret_cast<ID3D12PipelineState *>(pipeline.handle);
	_orig->SetPipelineState(pipeline_object);

	pipeline_graphics_impl extra_data;
	UINT extra_data_size = sizeof(extra_data);
	if (type == api::pipeline_stage::all_graphics &&
		SUCCEEDED(pipeline_object->GetPrivateData(extra_data_guid, &extra_data_size, &extra_data)))
	{
		_orig->IASetPrimitiveTopology(extra_data.topology);
		_orig->OMSetBlendFactor(extra_data.blend_constant);
	}
}
void reshade::d3d12::command_list_impl::bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::dynamic_state::stencil_reference_value:
			_orig->OMSetStencilRef(values[i]);
			break;
		case api::dynamic_state::blend_constant:
		{
			float blend_factor[4];
			blend_factor[0] = ((values[i]      ) & 0xFF) / 255.0f;
			blend_factor[1] = ((values[i] >>  4) & 0xFF) / 255.0f;
			blend_factor[2] = ((values[i] >>  8) & 0xFF) / 255.0f;
			blend_factor[3] = ((values[i] >> 12) & 0xFF) / 255.0f;
			_orig->OMSetBlendFactor(blend_factor);
			break;
		}
		case api::dynamic_state::primitive_topology:
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
	if (first != 0)
		return;

	_orig->RSSetViewports(count, reinterpret_cast<const D3D12_VIEWPORT *>(viewports));
}
void reshade::d3d12::command_list_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	if (first != 0)
		return;

	_orig->RSSetScissorRects(count, reinterpret_cast<const D3D12_RECT *>(rects));
}

void reshade::d3d12::command_list_impl::push_constants(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const void *values)
{
	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);

	if (static_cast<uint32_t>(stages & api::shader_stage::all_compute) != 0)
	{
		if (_current_root_signature[1] != root_signature)
		{
			_current_root_signature[1]  = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}

		_orig->SetComputeRoot32BitConstants(layout_param, count, values, first);
	}
	if (static_cast<uint32_t>(stages & api::shader_stage::all_graphics) != 0)
	{
		if (_current_root_signature[0] != root_signature)
		{
			_current_root_signature[0]  = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}

		_orig->SetGraphicsRoot32BitConstants(layout_param, count, values, first);
	}
}
void reshade::d3d12::command_list_impl::push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_set_update &update)
{
	assert(update.set.handle == 0 && update.offset == 0);

	D3D12_CPU_DESCRIPTOR_HANDLE base_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu;
	if (update.type != api::descriptor_type::sampler ?
		!_device_impl->_gpu_view_heap.allocate_transient(update.offset + update.count, base_handle, base_handle_gpu) :
		!_device_impl->_gpu_sampler_heap.allocate_transient(update.offset + update.count, base_handle, base_handle_gpu))
		return;

	const D3D12_DESCRIPTOR_HEAP_TYPE heap_type = convert_descriptor_type_to_heap_type(update.type);

	base_handle = _device_impl->offset_descriptor_handle(base_handle, update.offset, heap_type);

	if (_current_descriptor_heaps[0] != _device_impl->_gpu_sampler_heap.get() ||
		_current_descriptor_heaps[1] != _device_impl->_gpu_view_heap.get())
	{
		ID3D12DescriptorHeap *const heaps[2] = { _device_impl->_gpu_sampler_heap.get(), _device_impl->_gpu_view_heap.get() };
		_orig->SetDescriptorHeaps(2, heaps);

		_current_descriptor_heaps[0] = heaps[0];
		_current_descriptor_heaps[1] = heaps[1];
	}

	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);

	if (static_cast<uint32_t>(stages & api::shader_stage::all_compute) != 0)
	{
		if (_current_root_signature[1] != root_signature)
		{
			_current_root_signature[1]  = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}
	}
	if (static_cast<uint32_t>(stages & api::shader_stage::all_graphics) != 0)
	{
		if (_current_root_signature[0] != root_signature)
		{
			_current_root_signature[0]  = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}
	}

	if (update.type == api::descriptor_type::constant_buffer)
	{
		for (uint32_t k = 0; k < update.count; ++k, base_handle.ptr += _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV])
		{
			const auto buffer_range = static_cast<const api::buffer_range *>(update.descriptors)[k];
			const auto buffer_resource = reinterpret_cast<ID3D12Resource *>(buffer_range.buffer.handle);

			D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc;
			view_desc.BufferLocation = buffer_resource->GetGPUVirtualAddress() + buffer_range.offset;
			view_desc.SizeInBytes = (buffer_range.size == UINT64_MAX) ? static_cast<UINT>(buffer_resource->GetDesc().Width) : static_cast<UINT>(buffer_range.size);

			_device_impl->_orig->CreateConstantBufferView(&view_desc, base_handle);
		}
	}
	else if (update.type == api::descriptor_type::sampler || update.type == api::descriptor_type::shader_resource_view || update.type == api::descriptor_type::unordered_access_view)
	{
#ifndef WIN64
		auto src_handles = static_cast<D3D12_CPU_DESCRIPTOR_HANDLE *>(_malloca(update.count * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE)));
		for (uint32_t k = 0; k < update.count; ++k)
			src_handles[k] = { static_cast<SIZE_T>(static_cast<const uint64_t *>(update.descriptors)[k]) };
		const UINT src_range_size = 1;

		_device_impl->_orig->CopyDescriptors(1, &base_handle, &update.count, update.count, src_handles, &src_range_size, convert_descriptor_type_to_heap_type(update.type));

		_freea(src_handles);
#else
		auto src_range_sizes = static_cast<UINT *>(_malloca(update.count * sizeof(UINT)));
		std::fill(src_range_sizes, src_range_sizes + update.count, 1);

		_device_impl->_orig->CopyDescriptors(1, &base_handle, &update.count, update.count, static_cast<const D3D12_CPU_DESCRIPTOR_HANDLE *>(update.descriptors), src_range_sizes, convert_descriptor_type_to_heap_type(update.type));

		_freea(src_range_sizes);
#endif
	}
	else
	{
		assert(false);
	}

	if (static_cast<uint32_t>(stages & api::shader_stage::all_compute) != 0)
		_orig->SetComputeRootDescriptorTable(layout_param, base_handle_gpu);
	if (static_cast<uint32_t>(stages & api::shader_stage::all_graphics) != 0)
		_orig->SetGraphicsRootDescriptorTable(layout_param, base_handle_gpu);
}
void reshade::d3d12::command_list_impl::bind_descriptor_sets(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)
{
	if (count == 0)
		return;

	assert(sets != nullptr);

	// Change descriptor heaps to internal ones if descriptor sets were allocated from them
	if ((_current_descriptor_heaps[0] != _device_impl->_gpu_sampler_heap.get() || _current_descriptor_heaps[1] != _device_impl->_gpu_view_heap.get()) &&
		(_device_impl->_gpu_sampler_heap.contains(D3D12_GPU_DESCRIPTOR_HANDLE { sets[0].handle }) || _device_impl->_gpu_view_heap.contains(D3D12_GPU_DESCRIPTOR_HANDLE { sets[0].handle })))
	{
		ID3D12DescriptorHeap *const heaps[2] = { _device_impl->_gpu_sampler_heap.get(), _device_impl->_gpu_view_heap.get() };
		_orig->SetDescriptorHeaps(2, heaps);

		_current_descriptor_heaps[0] = heaps[0];
		_current_descriptor_heaps[1] = heaps[1];
	}

	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);

	if (static_cast<uint32_t>(stages & api::shader_stage::all_compute) != 0)
	{
		if (_current_root_signature[1] != root_signature)
		{
			_current_root_signature[1]  = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}

		for (uint32_t i = 0; i < count; ++i)
			_orig->SetComputeRootDescriptorTable(first + i, D3D12_GPU_DESCRIPTOR_HANDLE { sets[i].handle });
	}
	if (static_cast<uint32_t>(stages & api::shader_stage::all_graphics) != 0)
	{
		if (_current_root_signature[0] != root_signature)
		{
			_current_root_signature[0]  = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}

		for (uint32_t i = 0; i < count; ++i)
			_orig->SetGraphicsRootDescriptorTable(first + i, D3D12_GPU_DESCRIPTOR_HANDLE { sets[i].handle });
	}
}

void reshade::d3d12::command_list_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	if (buffer.handle != 0)
	{
		assert(index_size == 2 || index_size == 4);

		const auto buffer_resource = reinterpret_cast<ID3D12Resource *>(buffer.handle);

		D3D12_INDEX_BUFFER_VIEW view;
		view.BufferLocation = buffer_resource->GetGPUVirtualAddress() + offset;
		view.Format = index_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		view.SizeInBytes = static_cast<UINT>(buffer_resource->GetDesc().Width - offset);

		_orig->IASetIndexBuffer(&view);
	}
	else
	{
		_orig->IASetIndexBuffer(nullptr);
	}
}
void reshade::d3d12::command_list_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	if (count > D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
	{
		assert(false);
		count = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
	}

	D3D12_VERTEX_BUFFER_VIEW views[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

	for (uint32_t i = 0; i < count; ++i)
	{
		const auto buffer_resource = reinterpret_cast<ID3D12Resource *>(buffers[i].handle);

		views[i].BufferLocation = buffer_resource->GetGPUVirtualAddress() + offsets[i];
		views[i].SizeInBytes = static_cast<UINT>(buffer_resource->GetDesc().Width - offsets[i]);
		views[i].StrideInBytes = strides[i];
	}

	_orig->IASetVertexBuffers(first, count, views);
}

void reshade::d3d12::command_list_impl::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	_has_commands = true;

	_orig->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}
void reshade::d3d12::command_list_impl::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_has_commands = true;

	_orig->DrawIndexedInstanced(index_count, instance_count, first_index, vertex_offset, first_instance);
}
void reshade::d3d12::command_list_impl::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	_has_commands = true;

	_orig->Dispatch(group_count_x, group_count_y, group_count_z);
}
void reshade::d3d12::command_list_impl::draw_or_dispatch_indirect(api::indirect_command, api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
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

	if (size == UINT64_MAX)
		size  = reinterpret_cast<ID3D12Resource *>(src.handle)->GetDesc().Width;

	_orig->CopyBufferRegion(reinterpret_cast<ID3D12Resource *>(dst.handle), dst_offset, reinterpret_cast<ID3D12Resource *>(src.handle), src_offset, size);
}
void reshade::d3d12::command_list_impl::copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	D3D12_RESOURCE_DESC res_desc = reinterpret_cast<ID3D12Resource *>(dst.handle)->GetDesc();

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

	if (row_length != 0)
		res_desc.Width = row_length;
	if (slice_height != 0)
		res_desc.Height = slice_height;

	D3D12_TEXTURE_COPY_LOCATION src_copy_location;
	src_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(src.handle);
	src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	_device_impl->_orig->GetCopyableFootprints(&res_desc, dst_subresource, 1, src_offset, &src_copy_location.PlacedFootprint, nullptr, nullptr, nullptr);

	D3D12_TEXTURE_COPY_LOCATION dst_copy_location;
	dst_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst_copy_location.SubresourceIndex = dst_subresource;

	_orig->CopyTextureRegion(
		&dst_copy_location, dst_box != nullptr ? dst_box[0] : 0, dst_box != nullptr ? dst_box[1] : 0, dst_box != nullptr ? dst_box[2] : 0,
		&src_copy_location, &src_box);
}
void reshade::d3d12::command_list_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::filter_mode)
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

	D3D12_RESOURCE_DESC res_desc = reinterpret_cast<ID3D12Resource *>(src.handle)->GetDesc();

	if (row_length != 0)
		res_desc.Width = row_length;
	if (slice_height != 0)
		res_desc.Height = slice_height;

	D3D12_TEXTURE_COPY_LOCATION src_copy_location;
	src_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(src.handle);
	src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_copy_location.SubresourceIndex = src_subresource;

	D3D12_TEXTURE_COPY_LOCATION dst_copy_location;
	dst_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	_device_impl->_orig->GetCopyableFootprints(&res_desc, src_subresource, 1, dst_offset, &dst_copy_location.PlacedFootprint, nullptr, nullptr, nullptr);

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

			D3D12_RECT src_rect = (src_box != nullptr) ? D3D12_RECT { src_box[0], src_box[1], src_box[3], src_box[4] } : D3D12_RECT {};

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

void reshade::d3d12::command_list_impl::clear_attachments(api::attachment_type clear_flags, const float color[4], float depth, uint8_t stencil, uint32_t rect_count, const int32_t *rects)
{
	_has_commands = true;

	if (static_cast<UINT>(clear_flags & (api::attachment_type::color)) != 0)
		for (UINT i = 0; i < _current_fbo->count; ++i)
			_orig->ClearRenderTargetView(_current_fbo->rtv[i], color, rect_count, reinterpret_cast<const D3D12_RECT *>(rects));
	if (static_cast<UINT>(clear_flags & (api::attachment_type::depth | api::attachment_type::stencil)) != 0)
		_orig->ClearDepthStencilView(_current_fbo->dsv, static_cast<D3D12_CLEAR_FLAGS>(static_cast<UINT>(clear_flags) >> 1), depth, stencil, rect_count, reinterpret_cast<const D3D12_RECT *>(rects));
}
void reshade::d3d12::command_list_impl::clear_depth_stencil_view(api::resource_view dsv, api::attachment_type clear_flags, float depth, uint8_t stencil, uint32_t rect_count, const int32_t *rects)
{
	_has_commands = true;

	assert(dsv.handle != 0);

	_orig->ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(dsv.handle) }, static_cast<D3D12_CLEAR_FLAGS>(clear_flags), depth, stencil, rect_count, reinterpret_cast<const D3D12_RECT *>(rects));
}
void reshade::d3d12::command_list_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t rect_count, const int32_t *rects)
{
	_has_commands = true;

	assert(rtv.handle != 0);

	_orig->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(rtv.handle) }, color, rect_count, reinterpret_cast<const D3D12_RECT *>(rects));
}
void reshade::d3d12::command_list_impl::clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4], uint32_t rect_count, const int32_t *rects)
{
	_has_commands = true;

	assert(uav.handle != 0);

	const auto resource = reinterpret_cast<ID3D12Resource *>(_device_impl->get_resource_from_view(uav).handle);
	assert(resource != nullptr);

	D3D12_CPU_DESCRIPTOR_HANDLE table_base;
	D3D12_GPU_DESCRIPTOR_HANDLE table_base_gpu;
	if (!_device_impl->_gpu_view_heap.allocate_transient(1, table_base, table_base_gpu))
		return;

	const auto view_heap = _device_impl->_gpu_view_heap.get();
	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap)
		_orig->SetDescriptorHeaps(1, &view_heap);

	_device_impl->_orig->CreateUnorderedAccessView(resource, nullptr, nullptr, table_base);
	_orig->ClearUnorderedAccessViewUint(table_base_gpu, D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(uav.handle) }, resource, values, rect_count, reinterpret_cast<const D3D12_RECT *>(rects));

	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap && _current_descriptor_heaps[0] != nullptr)
		_orig->SetDescriptorHeaps(_current_descriptor_heaps[1] != nullptr ? 2 : 1, _current_descriptor_heaps);
}
void reshade::d3d12::command_list_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4], uint32_t rect_count, const int32_t *rects)
{
	_has_commands = true;

	assert(uav.handle != 0);

	const auto resource = reinterpret_cast<ID3D12Resource *>(_device_impl->get_resource_from_view(uav).handle);
	assert(resource != nullptr);

	D3D12_CPU_DESCRIPTOR_HANDLE table_base;
	D3D12_GPU_DESCRIPTOR_HANDLE table_base_gpu;
	if (!_device_impl->_gpu_view_heap.allocate_transient(1, table_base, table_base_gpu))
		return;

	const auto view_heap = _device_impl->_gpu_view_heap.get();
	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap)
		_orig->SetDescriptorHeaps(1, &view_heap);

	_device_impl->_orig->CreateUnorderedAccessView(resource, nullptr, nullptr, table_base);
	_orig->ClearUnorderedAccessViewFloat(table_base_gpu, D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(uav.handle) }, resource, values, rect_count, reinterpret_cast<const D3D12_RECT *>(rects));

	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap && _current_descriptor_heaps[0] != nullptr)
		_orig->SetDescriptorHeaps(_current_descriptor_heaps[1] != nullptr ? 2 : 1, _current_descriptor_heaps);
}

void reshade::d3d12::command_list_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);

	const auto resource = reinterpret_cast<ID3D12Resource *>(_device_impl->get_resource_from_view(srv).handle);
	assert(resource != nullptr);

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
		_orig->SetComputeRootDescriptorTable(1, { base_handle_gpu.ptr + _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * static_cast<UINT64>(level - 1) });
		// There is no UAV for level 0, so substract one
		_orig->SetComputeRootDescriptorTable(2, { base_handle_gpu.ptr + _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * static_cast<UINT64>(desc.MipLevels + level - 1) });

		_orig->Dispatch(std::max(1u, (width + 7) / 8), std::max(1u, (height + 7) / 8), 1);

		// Wait for all accesses to be finished, since the result will be the input for the next mipmap
		D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_UAV };
		barrier.UAV.pResource = transition.Transition.pResource;
		_orig->ResourceBarrier(1, &barrier);
	}

	std::swap(transition.Transition.StateBefore, transition.Transition.StateAfter);
	_orig->ResourceBarrier(1, &transition);

	// Reset descriptor heaps
	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap && _current_descriptor_heaps[0] != nullptr)
		_orig->SetDescriptorHeaps(_current_descriptor_heaps[1] != nullptr ? 2 : 1, _current_descriptor_heaps);
}

void reshade::d3d12::command_list_impl::begin_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	_has_commands = true;

	assert(pool.handle != 0);

	_orig->BeginQuery(reinterpret_cast<ID3D12QueryHeap *>(pool.handle), convert_query_type(type), index);
}
void reshade::d3d12::command_list_impl::end_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	_has_commands = true;

	assert(pool.handle != 0);

	const auto heap_object = reinterpret_cast<ID3D12QueryHeap *>(pool.handle);
	const auto d3d_query_type = convert_query_type(type);
	_orig->EndQuery(heap_object, d3d_query_type, index);

	com_ptr<ID3D12Resource> readback_resource;
	UINT private_size = sizeof(ID3D12Resource *);
	if (SUCCEEDED(heap_object->GetPrivateData(extra_data_guid, &private_size, &readback_resource)))
	{
		_orig->ResolveQueryData(reinterpret_cast<ID3D12QueryHeap *>(pool.handle), convert_query_type(type), index, 1, readback_resource.get(), index * sizeof(uint64_t));
	}
}
void reshade::d3d12::command_list_impl::copy_query_pool_results(api::query_pool pool, api::query_type type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride)
{
	_has_commands = true;

	assert(pool.handle != 0);
	assert(stride == sizeof(uint64_t));

	_orig->ResolveQueryData(reinterpret_cast<ID3D12QueryHeap *>(pool.handle), convert_query_type(type), first, count, reinterpret_cast<ID3D12Resource *>(dst.handle), dst_offset);
}

void reshade::d3d12::command_list_impl::begin_debug_event(const char *label, const float color[4])
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
void reshade::d3d12::command_list_impl::end_debug_event()
{
	_orig->EndEvent();
}
void reshade::d3d12::command_list_impl::insert_debug_marker(const char *label, const float color[4])
{
#if 0
	_orig->SetMarker(1, label, static_cast<UINT>(strlen(label)));
#else
	UINT64 pix3blob[64];
	encode_pix3blob(pix3blob, label, color);
	_orig->SetMarker(2, pix3blob, sizeof(pix3blob));
#endif
}
