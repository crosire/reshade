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

	temp_mem<D3D12_RESOURCE_BARRIER> barriers(count);
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

	_orig->ResourceBarrier(count, barriers.p);
}

void reshade::d3d12::command_list_impl::begin_render_pass(uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds)
{
	assert(count <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

	com_ptr<ID3D12GraphicsCommandList4> cmd_list4;
	if (SUCCEEDED(_orig->QueryInterface(&cmd_list4)))
	{
		temp_mem<D3D12_RENDER_PASS_RENDER_TARGET_DESC, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> rt_desc(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			rt_desc[i].cpuDescriptor = { static_cast<SIZE_T>(rts[i].view.handle) };
			rt_desc[i].BeginningAccess.Type = convert_render_pass_load_op(rts[i].load_op);
			rt_desc[i].EndingAccess.Type = convert_render_pass_store_op(rts[i].store_op);

			if (rts[i].load_op == api::render_pass_load_op::clear)
			{
				rt_desc[i].BeginningAccess.Clear.ClearValue.Format = convert_format(_device_impl->get_resource_view_desc(rts[i].view).format);
				std::copy_n(rts[i].clear_color, 4, rt_desc[i].BeginningAccess.Clear.ClearValue.Color);
			}
		}

		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depth_stencil_desc = {};
		if (ds != nullptr && ds->view.handle != 0)
		{
			depth_stencil_desc.cpuDescriptor = { static_cast<SIZE_T>(ds->view.handle) };
			depth_stencil_desc.DepthBeginningAccess.Type = convert_render_pass_load_op(ds->depth_load_op);
			depth_stencil_desc.StencilBeginningAccess.Type = convert_render_pass_load_op(ds->stencil_load_op);
			depth_stencil_desc.DepthEndingAccess.Type = convert_render_pass_store_op(ds->depth_store_op);
			depth_stencil_desc.StencilEndingAccess.Type = convert_render_pass_store_op(ds->stencil_store_op);

			if (ds->depth_load_op == api::render_pass_load_op::clear)
			{
				depth_stencil_desc.DepthBeginningAccess.Clear.ClearValue.Format = convert_format(_device_impl->get_resource_view_desc(ds->view).format);
				depth_stencil_desc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = ds->clear_depth;
			}
			if (ds->stencil_load_op == api::render_pass_load_op::clear)
			{
				depth_stencil_desc.StencilBeginningAccess.Clear.ClearValue.Format = convert_format(_device_impl->get_resource_view_desc(ds->view).format);
				depth_stencil_desc.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = ds->clear_stencil;
			}
		}

		cmd_list4->BeginRenderPass(count, rt_desc.p, ds != nullptr ? &depth_stencil_desc : nullptr, D3D12_RENDER_PASS_FLAG_NONE);
	}
	else
	{
		temp_mem<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> rtv_handles(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			rtv_handles[i] = { static_cast<SIZE_T>(rts[i].view.handle) };

			if (rts[i].load_op == api::render_pass_load_op::clear)
				_orig->ClearRenderTargetView(rtv_handles[i], rts[i].clear_color, 0, nullptr);
		}

		D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_handle = {};
		if (ds != nullptr && ds->view.handle != 0)
		{
			depth_stencil_handle = { static_cast<SIZE_T>(ds->view.handle) };

			if (const UINT clear_flags = (ds->depth_load_op == api::render_pass_load_op::clear ? D3D12_CLEAR_FLAG_DEPTH : 0) | (ds->stencil_load_op == api::render_pass_load_op::clear ? D3D12_CLEAR_FLAG_STENCIL : 0))
				_orig->ClearDepthStencilView(depth_stencil_handle, static_cast<D3D12_CLEAR_FLAGS>(clear_flags), ds->clear_depth, ds->clear_stencil, 0, nullptr);
		}

		_orig->OMSetRenderTargets(count, rtv_handles.p, FALSE, ds != nullptr ? &depth_stencil_handle : nullptr);
	}
}
void reshade::d3d12::command_list_impl::end_render_pass()
{
	com_ptr<ID3D12GraphicsCommandList4> cmd_list4;
	if (SUCCEEDED(_orig->QueryInterface(&cmd_list4)))
	{
		cmd_list4->EndRenderPass();
	}
}
void reshade::d3d12::command_list_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	assert(count <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

#ifndef WIN64
	temp_mem<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> rtv_handles_mem(count);
	for (uint32_t i = 0; i < count; ++i)
		rtv_handles_mem[i] = { static_cast<SIZE_T>(rtvs[i].handle) };
	const auto rtv_handles = rtv_handles_mem.p;
#else
	const auto rtv_handles = reinterpret_cast<const D3D12_CPU_DESCRIPTOR_HANDLE *>(rtvs);
#endif
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = { static_cast<SIZE_T>(dsv.handle) };

	_orig->OMSetRenderTargets(count, rtv_handles, FALSE, dsv.handle != 0 ? &dsv_handle : nullptr);
}

void reshade::d3d12::command_list_impl::bind_pipeline(api::pipeline_stage type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);
	assert(type == api::pipeline_stage::all || type == api::pipeline_stage::all_compute || type == api::pipeline_stage::all_graphics);

	const auto pipeline_object = reinterpret_cast<ID3D12PipelineState *>(pipeline.handle);
	_orig->SetPipelineState(pipeline_object);

	pipeline_extra_data extra_data;
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
		case api::dynamic_state::stencil_reference_value:
			_orig->OMSetStencilRef(values[i]);
			break;
		case api::dynamic_state::primitive_topology:
			_orig->IASetPrimitiveTopology(convert_primitive_topology(static_cast<api::primitive_topology>(values[i])));
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::d3d12::command_list_impl::bind_viewports(uint32_t first, uint32_t count, const api::viewport *viewports)
{
	if (first != 0)
		return;

	_orig->RSSetViewports(count, reinterpret_cast<const D3D12_VIEWPORT *>(viewports));
}
void reshade::d3d12::command_list_impl::bind_scissor_rects(uint32_t first, uint32_t count, const api::rect *rects)
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
	assert(update.set.handle == 0 && update.binding == 0);

	D3D12_CPU_DESCRIPTOR_HANDLE base_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu;
	if (update.type != api::descriptor_type::sampler ?
		!_device_impl->_gpu_view_heap.allocate_transient(update.count, base_handle, base_handle_gpu) :
		!_device_impl->_gpu_sampler_heap.allocate_transient(update.count, base_handle, base_handle_gpu))
		return;

	const D3D12_DESCRIPTOR_HEAP_TYPE heap_type = convert_descriptor_type_to_heap_type(update.type);

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
		temp_mem<D3D12_CPU_DESCRIPTOR_HANDLE> src_handles(update.count);
		for (uint32_t k = 0; k < update.count; ++k)
			src_handles[k] = { static_cast<SIZE_T>(static_cast<const uint64_t *>(update.descriptors)[k]) };
		const UINT src_range_size = 1;

		_device_impl->_orig->CopyDescriptors(1, &base_handle, &update.count, update.count, src_handles.p, &src_range_size, convert_descriptor_type_to_heap_type(update.type));
#else
		temp_mem<UINT> src_range_sizes(update.count);
		std::fill(src_range_sizes.p, src_range_sizes.p + update.count, 1);

		_device_impl->_orig->CopyDescriptors(1, &base_handle, &update.count, update.count, static_cast<const D3D12_CPU_DESCRIPTOR_HANDLE *>(update.descriptors), src_range_sizes.p, convert_descriptor_type_to_heap_type(update.type));
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
#if RESHADE_ADDON && !RESHADE_LITE
		((sets[0].handle >> 24) & 0xFF) < 2) // Heap index 0 or 1
#else
		(_device_impl->_gpu_sampler_heap.contains(_device_impl->convert_to_original_gpu_descriptor_handle(sets[0])) || _device_impl->_gpu_view_heap.contains(_device_impl->convert_to_original_gpu_descriptor_handle(sets[0]))))
#endif
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
			_orig->SetComputeRootDescriptorTable(first + i, _device_impl->convert_to_original_gpu_descriptor_handle(sets[i]));
	}
	if (static_cast<uint32_t>(stages & api::shader_stage::all_graphics) != 0)
	{
		if (_current_root_signature[0] != root_signature)
		{
			_current_root_signature[0]  = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}

		for (uint32_t i = 0; i < count; ++i)
			_orig->SetGraphicsRootDescriptorTable(first + i, _device_impl->convert_to_original_gpu_descriptor_handle(sets[i]));
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
	assert(count <= D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);

	temp_mem<D3D12_VERTEX_BUFFER_VIEW, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> views(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto buffer_resource = reinterpret_cast<ID3D12Resource *>(buffers[i].handle);

		views[i].BufferLocation = buffer_resource->GetGPUVirtualAddress() + offsets[i];
		views[i].SizeInBytes = static_cast<UINT>(buffer_resource->GetDesc().Width - offsets[i]);
		views[i].StrideInBytes = strides[i];
	}

	_orig->IASetVertexBuffers(first, count, views.p);
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
void reshade::d3d12::command_list_impl::copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const api::subresource_box *dst_box)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	D3D12_RESOURCE_DESC res_desc = reinterpret_cast<ID3D12Resource *>(dst.handle)->GetDesc();

	D3D12_BOX src_box = {};
	if (dst_box != nullptr)
	{
		src_box.right = src_box.left + (dst_box->right - dst_box->left);
		src_box.bottom = src_box.top + (dst_box->bottom - dst_box->top);
		src_box.back = src_box.front + (dst_box->back - dst_box->front);
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
		&dst_copy_location, dst_box != nullptr ? dst_box->left : 0, dst_box != nullptr ? dst_box->top : 0, dst_box != nullptr ? dst_box->front : 0,
		&src_copy_location, &src_box);
}
void reshade::d3d12::command_list_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, const api::subresource_box *dst_box, api::filter_mode)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);
	assert((src_box == nullptr && dst_box == nullptr) || (src_box != nullptr && dst_box != nullptr &&
		(dst_box->right - dst_box->left) == (src_box->right - src_box->left) && // Blit between different region dimensions is not supported
		(dst_box->bottom - dst_box->top) == (src_box->bottom - src_box->top) &&
		(dst_box->back - dst_box->front) == (src_box->back - src_box->front)));

	D3D12_TEXTURE_COPY_LOCATION src_copy_location;
	src_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(src.handle);
	src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_copy_location.SubresourceIndex = src_subresource;

	D3D12_TEXTURE_COPY_LOCATION dst_copy_location;
	dst_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst_copy_location.SubresourceIndex = dst_subresource;

	_orig->CopyTextureRegion(
		&dst_copy_location, dst_box != nullptr ? dst_box->left : 0, dst_box != nullptr ? dst_box->top : 0, dst_box != nullptr ? dst_box->front : 0,
		&src_copy_location, reinterpret_cast<const D3D12_BOX *>(src_box));
}
void reshade::d3d12::command_list_impl::copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
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
void reshade::d3d12::command_list_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const api::rect *src_rect, api::resource dst, uint32_t dst_subresource, int32_t dst_x, int32_t dst_y, api::format format)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	com_ptr<ID3D12GraphicsCommandList1> cmd_list1;
	if (SUCCEEDED(_orig->QueryInterface(&cmd_list1)))
	{
		cmd_list1->ResolveSubresourceRegion(
			reinterpret_cast<ID3D12Resource *>(dst.handle), dst_subresource, dst_x, dst_y,
			reinterpret_cast<ID3D12Resource *>(src.handle), src_subresource, reinterpret_cast<D3D12_RECT *>(const_cast<api::rect *>(src_rect)), convert_format(format), D3D12_RESOLVE_MODE_MIN);
	}
	else
	{
		assert(src_rect == nullptr && dst_x == 0 && dst_y == 0);

		_orig->ResolveSubresource(
			reinterpret_cast<ID3D12Resource *>(dst.handle), dst_subresource,
			reinterpret_cast<ID3D12Resource *>(src.handle), src_subresource, convert_format(format));
	}
}

void reshade::d3d12::command_list_impl::clear_depth_stencil_view(api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const api::rect *rects)
{
	_has_commands = true;

	assert(dsv.handle != 0);

	_orig->ClearDepthStencilView(
		D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(dsv.handle) },
		static_cast<D3D12_CLEAR_FLAGS>((depth != nullptr ? D3D12_CLEAR_FLAG_DEPTH : 0) | (stencil != nullptr ? D3D12_CLEAR_FLAG_STENCIL : 0)), depth != nullptr ? *depth : 0.0f, stencil != nullptr ? *stencil : 0,
		rect_count, reinterpret_cast<const D3D12_RECT *>(rects));
}
void reshade::d3d12::command_list_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t rect_count, const api::rect *rects)
{
	_has_commands = true;

	assert(rtv.handle != 0);

	_orig->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(rtv.handle) }, color, rect_count, reinterpret_cast<const D3D12_RECT *>(rects));
}
void reshade::d3d12::command_list_impl::clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4], uint32_t rect_count, const api::rect *rects)
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
void reshade::d3d12::command_list_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4], uint32_t rect_count, const api::rect *rects)
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
