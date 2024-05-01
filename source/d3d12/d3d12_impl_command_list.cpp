/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d12_impl_device.hpp"
#include "d3d12_impl_command_list.hpp"
#include "d3d12_impl_type_convert.hpp"
#include "dll_log.hpp"
#include <algorithm>

// IID_ID3D12GraphicsCommandListExt
static constexpr GUID s_command_list_ex_guid = { 0x77a86b09, 0x2bea, 0x4801, { 0xb8, 0x9a, 0x37, 0x64, 0x8e, 0x10, 0x4a, 0xf1 } };

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
	api_object_impl(cmd_list),
	_device_impl(device)
{
	if (_orig != nullptr)
		on_init();
}

void reshade::d3d12::command_list_impl::on_init()
{
	com_ptr<ID3D12GraphicsCommandList4> cmd_list4;
	com_ptr<IUnknown> cmd_list_ex;
	if (SUCCEEDED(_orig->QueryInterface(&cmd_list4)))
	{
		_orig->Release();
		_orig = cmd_list4.release();
		_supports_ray_tracing = true;

		// VKD3D does not implement 'ID3D12GraphicsCommandList4::BeginRenderPass' despite anouncing support for the 'ID3D12GraphicsCommandList4' interface as of VKD3D v2.6
		// This means we cannot use the 'BeginRenderPass' code path if we are running VKD3D, which next line tries to detect by querying for 'IID_ID3D12GraphicsCommandListExt' support
		if (_orig->QueryInterface(s_command_list_ex_guid, reinterpret_cast<void **>(&cmd_list_ex)) != S_OK)
			_supports_render_passes = true;
	}
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

	uint32_t k = 0;
	temp_mem<D3D12_RESOURCE_BARRIER> barriers(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		if (resources[i].handle == 0)
		{
			D3D12_GLOBAL_BARRIER barrier = {};
			barrier.SyncBefore = D3D12_BARRIER_SYNC_ALL;
			barrier.SyncAfter = D3D12_BARRIER_SYNC_ALL;
			barrier.AccessBefore = convert_usage_to_access(old_states[i]);
			barrier.AccessAfter = convert_usage_to_access(new_states[i]);

			D3D12_BARRIER_GROUP barrier_group = {};
			barrier_group.Type = D3D12_BARRIER_TYPE_GLOBAL;
			barrier_group.NumBarriers = 1;
			barrier_group.pGlobalBarriers = &barrier;

			com_ptr<ID3D12GraphicsCommandList7> cmd_list7;
			if (SUCCEEDED(_orig->QueryInterface(&cmd_list7)))
				cmd_list7->Barrier(1, &barrier_group);
			else
				assert(false);
			continue;
		}

		if (old_states[i] == api::resource_usage::unordered_access && new_states[i] == api::resource_usage::unordered_access)
		{
			barriers[k].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barriers[k].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[k].UAV.pResource = reinterpret_cast<ID3D12Resource *>(resources[i].handle);
		}
		else if (old_states[i] == api::resource_usage::undefined && new_states[i] == api::resource_usage::general)
		{
			barriers[k].Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
			barriers[k].Aliasing.pResourceBefore = nullptr;
			barriers[k].Aliasing.pResourceAfter = reinterpret_cast<ID3D12Resource *>(resources[i].handle);
		}
		else
		{
			barriers[k].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barriers[k].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[k].Transition.pResource = reinterpret_cast<ID3D12Resource *>(resources[i].handle);
			barriers[k].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barriers[k].Transition.StateBefore = convert_usage_to_resource_states(old_states[i]);
			barriers[k].Transition.StateAfter = convert_usage_to_resource_states(new_states[i]);
		}

		k++;
	}

	_orig->ResourceBarrier(k, barriers.p);
}

void reshade::d3d12::command_list_impl::begin_render_pass(uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds)
{
	_has_commands = true;

	assert(count <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

	if (_supports_render_passes)
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

		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depth_stencil_desc;
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

		static_cast<ID3D12GraphicsCommandList4 *>(_orig)->BeginRenderPass(count, rt_desc.p, ds != nullptr && ds->view.handle != 0 ? &depth_stencil_desc : nullptr, D3D12_RENDER_PASS_FLAG_NONE);
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

		D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_handle;
		if (ds != nullptr && ds->view.handle != 0)
		{
			depth_stencil_handle = { static_cast<SIZE_T>(ds->view.handle) };

			if (const UINT clear_flags = (ds->depth_load_op == api::render_pass_load_op::clear ? D3D12_CLEAR_FLAG_DEPTH : 0) | (ds->stencil_load_op == api::render_pass_load_op::clear ? D3D12_CLEAR_FLAG_STENCIL : 0))
				_orig->ClearDepthStencilView(depth_stencil_handle, static_cast<D3D12_CLEAR_FLAGS>(clear_flags), ds->clear_depth, ds->clear_stencil, 0, nullptr);
		}

		_orig->OMSetRenderTargets(count, rtv_handles.p, FALSE, ds != nullptr && ds->view.handle != 0 ? &depth_stencil_handle : nullptr);
	}
}
void reshade::d3d12::command_list_impl::end_render_pass()
{
	assert(_has_commands);

	if (_supports_render_passes)
	{
		static_cast<ID3D12GraphicsCommandList4 *>(_orig)->EndRenderPass();
	}
}
void reshade::d3d12::command_list_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	assert(count <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

#ifndef _WIN64
	temp_mem<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> rtv_handles_mem(count);
	for (uint32_t i = 0; i < count; ++i)
		rtv_handles_mem[i] = { static_cast<SIZE_T>(rtvs[i].handle) };
	const auto rtv_handles = rtv_handles_mem.p;
#else
	const auto rtv_handles = reinterpret_cast<const D3D12_CPU_DESCRIPTOR_HANDLE *>(rtvs);
#endif
	const D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_handle = { static_cast<SIZE_T>(dsv.handle) };

	_orig->OMSetRenderTargets(count, rtv_handles, FALSE, dsv.handle != 0 ? &depth_stencil_handle : nullptr);
}

void reshade::d3d12::command_list_impl::bind_pipeline(api::pipeline_stage stages, api::pipeline pipeline)
{
	// Cannot bind state to individual pipeline stages
	assert(stages == api::pipeline_stage::all || stages == api::pipeline_stage::all_compute || stages == api::pipeline_stage::all_graphics || stages == api::pipeline_stage::all_ray_tracing);

	if (stages == api::pipeline_stage::all_ray_tracing)
	{
		const auto pipeline_object = reinterpret_cast<ID3D12StateObject *>(pipeline.handle);

		if (_supports_ray_tracing)
			static_cast<ID3D12GraphicsCommandList4 *>(_orig)->SetPipelineState1(pipeline_object);
		else
			assert(false);
	}
	else
	{
		const auto pipeline_object = reinterpret_cast<ID3D12PipelineState *>(pipeline.handle);
		_orig->SetPipelineState(pipeline_object);

		pipeline_extra_data extra_data;
		UINT extra_data_size = sizeof(extra_data);
		if (stages == api::pipeline_stage::all_graphics &&
			pipeline_object != nullptr &&
			SUCCEEDED(pipeline_object->GetPrivateData(extra_data_guid, &extra_data_size, &extra_data)))
		{
			_orig->IASetPrimitiveTopology(extra_data.topology);
			_orig->OMSetBlendFactor(extra_data.blend_constant);
		}
	}
}
void reshade::d3d12::command_list_impl::bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::dynamic_state::primitive_topology:
			_orig->IASetPrimitiveTopology(convert_primitive_topology(static_cast<api::primitive_topology>(values[i])));
			break;
		case api::dynamic_state::blend_constant:
		{
			const float blend_constant[4] = { ((values[i]) & 0xFF) / 255.0f, ((values[i] >> 4) & 0xFF) / 255.0f, ((values[i] >> 8) & 0xFF) / 255.0f, ((values[i] >> 12) & 0xFF) / 255.0f };
			_orig->OMSetBlendFactor(blend_constant);
			break;
		}
		case api::dynamic_state::front_stencil_reference_value:
			_orig->OMSetStencilRef(values[i]);
			break;
		case api::dynamic_state::back_stencil_reference_value:
			// OMSetFrontAndBackStencilRef
			assert(false);
			break;
		case api::dynamic_state::depth_bias:
		case api::dynamic_state::depth_bias_clamp:
		case api::dynamic_state::depth_bias_slope_scaled:
			// RSSetDepthBias
			assert(false);
			break;
		case api::dynamic_state::ray_tracing_pipeline_stack_size:
			// ID3D12StateObjectProperties *props = ...;
			// props->SetPipelineStackSize(values[i]);
			assert(false);
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
	{
		assert(false);
		return;
	}

	_orig->RSSetViewports(count, reinterpret_cast<const D3D12_VIEWPORT *>(viewports));
}
void reshade::d3d12::command_list_impl::bind_scissor_rects(uint32_t first, uint32_t count, const api::rect *rects)
{
	if (first != 0)
	{
		assert(false);
		return;
	}

	_orig->RSSetScissorRects(count, reinterpret_cast<const D3D12_RECT *>(rects));
}

void reshade::d3d12::command_list_impl::push_constants(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const void *values)
{
	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);

	if ((stages & (api::shader_stage::all_compute | api::shader_stage::all_ray_tracing)) != 0)
	{
		if (root_signature != _current_root_signature[1])
		{
			_current_root_signature[1] = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}

		_orig->SetComputeRoot32BitConstants(layout_param, count, values, first);
	}
	if ((stages & api::shader_stage::all_graphics) != 0)
	{
		if (root_signature != _current_root_signature[0])
		{
			_current_root_signature[0] = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}

		_orig->SetGraphicsRoot32BitConstants(layout_param, count, values, first);
	}
}
void reshade::d3d12::command_list_impl::push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_table_update &update)
{
	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);

	if ((stages & (api::shader_stage::all_compute | api::shader_stage::all_ray_tracing)) != 0)
	{
		if (root_signature != _current_root_signature[1])
		{
			_current_root_signature[1] = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}
	}
	if ((stages & api::shader_stage::all_graphics) != 0)
	{
		if (root_signature != _current_root_signature[0])
		{
			_current_root_signature[0] = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}
	}

	assert(update.table.handle == 0 && update.array_offset == 0);

	const D3D12_DESCRIPTOR_HEAP_TYPE heap_type = convert_descriptor_type_to_heap_type(update.type);

	if (update.binding == 0 && update.count == 1 &&
		heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV &&
		update.type != api::descriptor_type::texture_shader_resource_view &&
		update.type != api::descriptor_type::texture_unordered_access_view)
	{
		if (update.type == api::descriptor_type::constant_buffer)
		{
			const auto &view_range = *static_cast<const api::buffer_range *>(update.descriptors);

			const D3D12_GPU_VIRTUAL_ADDRESS view_address = reinterpret_cast<ID3D12Resource *>(view_range.buffer.handle)->GetGPUVirtualAddress() + view_range.offset;
			assert(view_address != 0);

			if ((stages & (api::shader_stage::all_compute | api::shader_stage::all_ray_tracing)) != 0)
				_orig->SetComputeRootConstantBufferView(layout_param, view_address);
			if ((stages & api::shader_stage::all_graphics) != 0)
				_orig->SetGraphicsRootConstantBufferView(layout_param, view_address);
		}
		else if (update.type == api::descriptor_type::buffer_shader_resource_view || update.type == api::descriptor_type::acceleration_structure)
		{
			const D3D12_GPU_VIRTUAL_ADDRESS view_address = _device_impl->get_resource_view_gpu_address(*static_cast<const api::resource_view *>(update.descriptors));
			assert(view_address != 0);

			if ((stages & (api::shader_stage::all_compute | api::shader_stage::all_ray_tracing)) != 0)
				_orig->SetComputeRootShaderResourceView(layout_param, view_address);
			if ((stages & api::shader_stage::all_graphics) != 0)
				_orig->SetGraphicsRootShaderResourceView(layout_param, view_address);
		}
		else if (update.type == api::descriptor_type::buffer_unordered_access_view)
		{
			const D3D12_GPU_VIRTUAL_ADDRESS view_address = _device_impl->get_resource_view_gpu_address(*static_cast<const api::resource_view *>(update.descriptors));
			assert(view_address != 0);

			if ((stages & (api::shader_stage::all_compute | api::shader_stage::all_ray_tracing)) != 0)
				_orig->SetComputeRootUnorderedAccessView(layout_param, view_address);
			if ((stages & api::shader_stage::all_graphics) != 0)
				_orig->SetGraphicsRootShaderResourceView(layout_param, view_address);
		}
		return;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE base_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu;
	if (update.type != api::descriptor_type::sampler ?
		!_device_impl->_gpu_view_heap.allocate_transient(update.binding + update.count, base_handle, base_handle_gpu) :
		!_device_impl->_gpu_sampler_heap.allocate_transient(update.binding + update.count, base_handle, base_handle_gpu))
	{
		LOG(ERROR) << "Failed to allocate " << update.count << " transient descriptor handle(s) of type " << static_cast<uint32_t>(update.type) << '!';
		return;
	}

	// Add base descriptor offset (these descriptors stay unusued)
	base_handle = _device_impl->offset_descriptor_handle(base_handle, update.binding, heap_type);

	if (update.type == api::descriptor_type::constant_buffer)
	{
		for (uint32_t k = 0; k < update.count; ++k, base_handle = _device_impl->offset_descriptor_handle(base_handle, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
		{
			const auto &view_range = static_cast<const api::buffer_range *>(update.descriptors)[k];

			D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc;
			view_desc.BufferLocation = reinterpret_cast<ID3D12Resource *>(view_range.buffer.handle)->GetGPUVirtualAddress() + view_range.offset;
			view_desc.SizeInBytes = static_cast<UINT>(view_range.size == UINT64_MAX ? reinterpret_cast<ID3D12Resource *>(view_range.buffer.handle)->GetDesc().Width - view_range.offset : view_range.size);

			_device_impl->_orig->CreateConstantBufferView(&view_desc, base_handle);
		}
	}
	else if (update.type == api::descriptor_type::sampler ||
		update.type == api::descriptor_type::buffer_shader_resource_view ||
		update.type == api::descriptor_type::buffer_unordered_access_view ||
		update.type == api::descriptor_type::texture_shader_resource_view ||
		update.type == api::descriptor_type::texture_unordered_access_view)
	{
#ifndef _WIN64
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
	else if (update.type == api::descriptor_type::acceleration_structure)
	{
		for (uint32_t k = 0; k < update.count; ++k, base_handle = _device_impl->offset_descriptor_handle(base_handle, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC view_desc;
			view_desc.Format = DXGI_FORMAT_UNKNOWN;
			view_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
			view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			view_desc.RaytracingAccelerationStructure.Location = static_cast<const api::resource_view *>(update.descriptors)[k].handle;

			_device_impl->_orig->CreateShaderResourceView(nullptr, &view_desc, base_handle);
		}
	}
	else
	{
		assert(false);
		return;
	}

	if (_current_descriptor_heaps[0] != _device_impl->_gpu_sampler_heap.get() ||
		_current_descriptor_heaps[1] != _device_impl->_gpu_view_heap.get())
	{
		ID3D12DescriptorHeap *const heaps[2] = { _device_impl->_gpu_sampler_heap.get(), _device_impl->_gpu_view_heap.get() };
		std::copy_n(heaps, 2, _current_descriptor_heaps);
		_orig->SetDescriptorHeaps(2, heaps);
	}

#ifndef NDEBUG
	pipeline_layout_extra_data extra_data;
	UINT extra_data_size = sizeof(extra_data);
	if (SUCCEEDED(root_signature->GetPrivateData(extra_data_guid, &extra_data_size, &extra_data)))
	{
		assert(heap_type == extra_data.ranges[layout_param].first);
		assert(update.binding + update.count <= extra_data.ranges[layout_param].second);
	}
#endif

	if ((stages & (api::shader_stage::all_compute | api::shader_stage::all_ray_tracing)) != 0)
		_orig->SetComputeRootDescriptorTable(layout_param, base_handle_gpu);
	if ((stages & api::shader_stage::all_graphics) != 0)
		_orig->SetGraphicsRootDescriptorTable(layout_param, base_handle_gpu);
}
void reshade::d3d12::command_list_impl::bind_descriptor_tables(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_table *tables)
{
	assert(tables != nullptr || count == 0);

	// Change current descriptor heaps to the ones the descriptor tables were allocated from
	ID3D12DescriptorHeap *heaps[2];
	std::copy_n(_current_descriptor_heaps, 2, heaps);

	for (uint32_t i = 0; i < count; ++i)
	{
		ID3D12DescriptorHeap *const heap = _device_impl->get_descriptor_heap(tables[i]);
		if (heap == nullptr)
			continue;

		for (uint32_t k = 0; k < 2; ++k)
		{
			if (heaps[k] == heap)
				break;

			if (heaps[k] == nullptr || heaps[k]->GetDesc().Type == heap->GetDesc().Type)
			{
				// Cannot bind descriptor tables from different descriptor heaps
				assert(heaps[k] == _current_descriptor_heaps[k]);

				heaps[k] = heap;
				break;
			}
		}
	}

#if RESHADE_ADDON >= 2
	if ((heaps[0] == _previous_descriptor_heaps[0] || heaps[0] == _previous_descriptor_heaps[1] || heaps[1] == _previous_descriptor_heaps[0] || heaps[1] == _previous_descriptor_heaps[1] || count == 0) &&
		_previous_descriptor_heaps[0] != nullptr && _previous_descriptor_heaps[1] != nullptr &&
		_previous_descriptor_heaps[0] != _current_descriptor_heaps[0] && _previous_descriptor_heaps[1] != _current_descriptor_heaps[1])
	{
		// Attempt to keep combination of descriptor heaps set by the application if one of them is restored
		// An application may set both descriptor heaps, but then only bind descriptor tables allocated from one of them, causing add-ons to be unable to restore the other descriptor heap
		std::copy_n(_previous_descriptor_heaps, 2, heaps);
	}
#endif

	if (_current_descriptor_heaps[0] != heaps[0] || _current_descriptor_heaps[1] != heaps[1] || count == 0) // Force descriptor heap update when there are no tables
	{
		std::copy_n(heaps, 2, _current_descriptor_heaps);
		if (heaps[0] != nullptr)
			_orig->SetDescriptorHeaps(heaps[1] != nullptr ? 2 : 1, heaps);
	}

	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);

	if ((stages & (api::shader_stage::all_compute | api::shader_stage::all_ray_tracing)) != 0)
	{
		if (root_signature != _current_root_signature[1] || count == 0) // Force root signature update when there are no tables
		{
			_current_root_signature[1] = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}

		for (uint32_t i = 0; i < count; ++i)
			_orig->SetComputeRootDescriptorTable(first + i, _device_impl->convert_to_original_gpu_descriptor_handle(tables[i]));
	}
	if ((stages & api::shader_stage::all_graphics) != 0)
	{
		if (root_signature != _current_root_signature[0] || count == 0)
		{
			_current_root_signature[0] = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}

		for (uint32_t i = 0; i < count; ++i)
			_orig->SetGraphicsRootDescriptorTable(first + i, _device_impl->convert_to_original_gpu_descriptor_handle(tables[i]));
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
		view.Format = (index_size == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
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
		const auto offset = (offsets != nullptr ? offsets[i] : 0);

		views[i].BufferLocation = buffer_resource->GetGPUVirtualAddress() + offset;
		views[i].SizeInBytes = static_cast<UINT>(buffer_resource->GetDesc().Width - offset);
		views[i].StrideInBytes = strides[i];
	}

	_orig->IASetVertexBuffers(first, count, views.p);
}
void reshade::d3d12::command_list_impl::bind_stream_output_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint64_t *max_sizes, const api::resource *counter_buffers, const uint64_t *counter_offsets)
{
	assert(count <= D3D12_SO_BUFFER_SLOT_COUNT);

	temp_mem<D3D12_STREAM_OUTPUT_BUFFER_VIEW, D3D12_SO_BUFFER_SLOT_COUNT> views(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto buffer_resource = reinterpret_cast<ID3D12Resource *>(buffers[i].handle);
		const auto offset = (offsets != nullptr) ? offsets[i] : 0;
		const auto counter_buffer_resource = reinterpret_cast<ID3D12Resource *>(counter_buffers[i].handle);
		const auto counter_offset = (counter_offsets != nullptr) ? counter_offsets[i] : 0;

		views[i].BufferLocation = buffer_resource->GetGPUVirtualAddress() + offset;
		views[i].SizeInBytes = (max_sizes != nullptr && max_sizes[i] != UINT64_MAX) ? max_sizes[0] : 0;
		views[i].BufferFilledSizeLocation = counter_buffer_resource->GetGPUVirtualAddress() + counter_offset;
	}

	_orig->SOSetTargets(first, count, views.p);
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
void reshade::d3d12::command_list_impl::dispatch_mesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	_has_commands = true;

	com_ptr<ID3D12GraphicsCommandList6> cmd_list6;
	if (SUCCEEDED(_orig->QueryInterface(&cmd_list6)))
		cmd_list6->DispatchMesh(group_count_x, group_count_y, group_count_z);
	else
		assert(false);
}
void reshade::d3d12::command_list_impl::dispatch_rays(api::resource raygen, uint64_t raygen_offset, uint64_t raygen_size, api::resource miss, uint64_t miss_offset, uint64_t miss_size, uint64_t miss_stride, api::resource hit_group, uint64_t hit_group_offset, uint64_t hit_group_size, uint64_t hit_group_stride, api::resource callable, uint64_t callable_offset, uint64_t callable_size, uint64_t callable_stride, uint32_t width, uint32_t height, uint32_t depth)
{
	_has_commands = true;

	if (_supports_ray_tracing)
	{
		D3D12_DISPATCH_RAYS_DESC desc;
		desc.RayGenerationShaderRecord.StartAddress = (raygen.handle != 0 ? reinterpret_cast<ID3D12Resource *>(raygen.handle)->GetGPUVirtualAddress() : 0) + raygen_offset;
		desc.RayGenerationShaderRecord.SizeInBytes = raygen_size;
		desc.MissShaderTable.StartAddress = (miss.handle != 0 ? reinterpret_cast<ID3D12Resource *>(miss.handle)->GetGPUVirtualAddress() : 0) + miss_offset;
		desc.MissShaderTable.SizeInBytes = miss_size;
		desc.MissShaderTable.StrideInBytes = miss_stride;
		desc.HitGroupTable.StartAddress = (hit_group.handle != 0 ? reinterpret_cast<ID3D12Resource *>(hit_group.handle)->GetGPUVirtualAddress() : 0) + hit_group_offset;
		desc.HitGroupTable.SizeInBytes = hit_group_size;
		desc.HitGroupTable.StrideInBytes = hit_group_stride;
		desc.CallableShaderTable.StartAddress = (callable.handle != 0 ? reinterpret_cast<ID3D12Resource *>(callable.handle)->GetGPUVirtualAddress() : 0) + callable_offset;
		desc.CallableShaderTable.SizeInBytes = callable_size;
		desc.CallableShaderTable.StrideInBytes = callable_stride;
		desc.Width = width;
		desc.Height = height;
		desc.Depth = depth;

		static_cast<ID3D12GraphicsCommandList4 *>(_orig)->DispatchRays(&desc);
	}
	else
	{
		assert(false);
	}
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

	if (UINT64_MAX == size)
		size = reinterpret_cast<ID3D12Resource *>(src.handle)->GetDesc().Width;

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
		src_box.right = src_box.left + dst_box->width();
		src_box.bottom = src_box.top + dst_box->height();
		src_box.back = src_box.front + dst_box->depth();
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
	// Blit between different region dimensions is not supported
	assert((src_box == nullptr && dst_box == nullptr) || (src_box != nullptr && dst_box != nullptr && dst_box->width() == src_box->width() && dst_box->height() == src_box->height() && dst_box->depth() == src_box->depth()));

	D3D12_RESOURCE_DESC src_desc = reinterpret_cast<ID3D12Resource *>(src.handle)->GetDesc();
	D3D12_RESOURCE_DESC dst_desc = reinterpret_cast<ID3D12Resource *>(dst.handle)->GetDesc();

	D3D12_TEXTURE_COPY_LOCATION src_copy_location;
	src_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(src.handle);
	if (src_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src_copy_location.PlacedFootprint.Offset = 0;

		UINT extra_data_size = sizeof(src_copy_location.PlacedFootprint.Footprint);
		if (FAILED(src_copy_location.pResource->GetPrivateData(extra_data_guid, &extra_data_size, &src_copy_location.PlacedFootprint.Footprint)))
		{
			assert(dst_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER);

			_device_impl->_orig->GetCopyableFootprints(&dst_desc, dst_subresource, 1, 0, &src_copy_location.PlacedFootprint, nullptr, nullptr, nullptr);
		}
	}
	else
	{
		src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src_copy_location.SubresourceIndex = src_subresource;
	}

	D3D12_TEXTURE_COPY_LOCATION dst_copy_location;
	dst_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	if (dst_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst_copy_location.PlacedFootprint.Offset = 0;

		UINT extra_data_size = sizeof(dst_copy_location.PlacedFootprint.Footprint);
		if (FAILED(dst_copy_location.pResource->GetPrivateData(extra_data_guid, &extra_data_size, &dst_copy_location.PlacedFootprint.Footprint)))
		{
			assert(src_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER);

			_device_impl->_orig->GetCopyableFootprints(&src_desc, src_subresource, 1, 0, &dst_copy_location.PlacedFootprint, nullptr, nullptr, nullptr);
		}
	}
	else
	{
		dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst_copy_location.SubresourceIndex = dst_subresource;
	}

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
void reshade::d3d12::command_list_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, int32_t dst_x, int32_t dst_y, int32_t dst_z, api::format format)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	com_ptr<ID3D12GraphicsCommandList1> cmd_list1;
	if (SUCCEEDED(_orig->QueryInterface(&cmd_list1)))
	{
		assert(dst_z == 0);

		D3D12_RECT src_rect;
		if (src_box != nullptr)
		{
			src_rect.left = src_box->left;
			src_rect.top = src_box->top;
			src_rect.right = src_box->right;
			src_rect.bottom = src_box->back;

			assert((src_box->back - src_box->front) <= 1);
		}

		D3D12_RESOLVE_MODE resolve_mode;
		switch (format)
		{
		default:
			// MIN or MAX can be used with any render target or depth-stencil format
			resolve_mode = D3D12_RESOLVE_MODE_MIN;
			break;
		case api::format::r8_unorm:
		case api::format::r8_snorm:
		case api::format::r8g8_unorm:
		case api::format::r8g8_snorm:
		case api::format::r8g8b8a8_unorm:
		case api::format::r8g8b8a8_unorm_srgb:
		case api::format::r8g8b8a8_snorm:
		case api::format::b8g8r8a8_unorm:
		case api::format::b8g8r8a8_unorm_srgb:
		case api::format::b8g8r8x8_unorm:
		case api::format::b8g8r8x8_unorm_srgb:
		case api::format::r10g10b10a2_unorm:
		case api::format::r16_unorm:
		case api::format::r16_snorm:
		case api::format::r16_float:
		case api::format::r16g16_unorm:
		case api::format::r16g16_snorm:
		case api::format::r16g16_float:
		case api::format::r16g16b16a16_unorm:
		case api::format::r16g16b16a16_snorm:
		case api::format::r16g16b16a16_float:
		case api::format::r32_float:
		case api::format::r32g32_float:
		case api::format::r32g32b32_float:
		case api::format::r32g32b32a32_float:
		case api::format::d16_unorm:
		case api::format::d32_float:
		case api::format::r24_unorm_x8_uint:
		case api::format::r32_float_x8_uint:
			// AVERAGE can be used with any non-integer format (see https://microsoft.github.io/DirectX-Specs/d3d/ProgrammableSamplePositions.html#resolvesubresourceregion)
			resolve_mode = D3D12_RESOLVE_MODE_AVERAGE;
			break;
		}

		cmd_list1->ResolveSubresourceRegion(
			reinterpret_cast<ID3D12Resource *>(dst.handle), dst_subresource, dst_x, dst_y,
			reinterpret_cast<ID3D12Resource *>(src.handle), src_subresource, src_box != nullptr ? &src_rect : nullptr, convert_format(format), resolve_mode);
	}
	else
	{
		assert(src_box == nullptr && dst_x == 0 && dst_y == 0 && dst_z == 0);

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
	const api::resource resource = _device_impl->get_resource_from_view(uav);
	assert(resource.handle != 0);

	D3D12_CPU_DESCRIPTOR_HANDLE table_base;
	D3D12_GPU_DESCRIPTOR_HANDLE table_base_gpu;
	if (!_device_impl->_gpu_view_heap.allocate_transient(1, table_base, table_base_gpu))
	{
		LOG(ERROR) << "Failed to allocate " << 1 << " transient descriptor handle(s) of type " << static_cast<uint32_t>(api::descriptor_type::unordered_access_view) << '!';
		return;
	}

	ID3D12DescriptorHeap *const view_heap = _device_impl->_gpu_view_heap.get();
	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap)
		_orig->SetDescriptorHeaps(1, &view_heap);

	D3D12_UNORDERED_ACCESS_VIEW_DESC internal_desc = {};
	convert_resource_view_desc(_device_impl->get_resource_view_desc(uav), internal_desc);
	_device_impl->_orig->CreateUnorderedAccessView(reinterpret_cast<ID3D12Resource *>(resource.handle), nullptr, &internal_desc, table_base);
	_orig->ClearUnorderedAccessViewUint(table_base_gpu, D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(uav.handle) }, reinterpret_cast<ID3D12Resource *>(resource.handle), values, rect_count, reinterpret_cast<const D3D12_RECT *>(rects));

	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap && _current_descriptor_heaps[0] != nullptr)
		_orig->SetDescriptorHeaps(_current_descriptor_heaps[1] != nullptr ? 2 : 1, _current_descriptor_heaps);
}
void reshade::d3d12::command_list_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4], uint32_t rect_count, const api::rect *rects)
{
	_has_commands = true;

	assert(uav.handle != 0);
	const api::resource resource = _device_impl->get_resource_from_view(uav);
	assert(resource.handle != 0);

	D3D12_CPU_DESCRIPTOR_HANDLE table_base;
	D3D12_GPU_DESCRIPTOR_HANDLE table_base_gpu;
	if (!_device_impl->_gpu_view_heap.allocate_transient(1, table_base, table_base_gpu))
	{
		LOG(ERROR) << "Failed to allocate " << 1 << " transient descriptor handle(s) of type " << static_cast<uint32_t>(api::descriptor_type::unordered_access_view) << '!';
		return;
	}

	ID3D12DescriptorHeap *const view_heap = _device_impl->_gpu_view_heap.get();
	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap)
		_orig->SetDescriptorHeaps(1, &view_heap);

	D3D12_UNORDERED_ACCESS_VIEW_DESC internal_desc = {};
	convert_resource_view_desc(_device_impl->get_resource_view_desc(uav), internal_desc);
	_device_impl->_orig->CreateUnorderedAccessView(reinterpret_cast<ID3D12Resource *>(resource.handle), nullptr, &internal_desc, table_base);
	_orig->ClearUnorderedAccessViewFloat(table_base_gpu, D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(uav.handle) }, reinterpret_cast<ID3D12Resource *>(resource.handle), values, rect_count, reinterpret_cast<const D3D12_RECT *>(rects));

	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap && _current_descriptor_heaps[0] != nullptr)
		_orig->SetDescriptorHeaps(_current_descriptor_heaps[1] != nullptr ? 2 : 1, _current_descriptor_heaps);
}

void reshade::d3d12::command_list_impl::generate_mipmaps(api::resource_view srv)
{
	if (_device_impl->_mipmap_pipeline == nullptr)
		return;

	_has_commands = true;

	assert(srv.handle != 0);
	const api::resource resource = _device_impl->get_resource_from_view(srv);
	assert(resource.handle != 0);

	const D3D12_RESOURCE_DESC desc = reinterpret_cast<ID3D12Resource *>(resource.handle)->GetDesc();

	const api::resource_view_desc view_desc = _device_impl->get_resource_view_desc(srv);
	const uint32_t level_count = std::min(view_desc.texture.level_count, static_cast<uint32_t>(desc.MipLevels));

	D3D12_CPU_DESCRIPTOR_HANDLE base_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu;
	if (!_device_impl->_gpu_view_heap.allocate_transient(level_count * 2, base_handle, base_handle_gpu))
	{
		LOG(ERROR) << "Failed to allocate " << (level_count * 2) << " transient descriptor handle(s) of type " << static_cast<uint32_t>(api::descriptor_type::shader_resource_view) << " and " << static_cast<uint32_t>(api::descriptor_type::unordered_access_view) << '!';
		return;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE sampler_handle_gpu;
	if (!_device_impl->_gpu_sampler_heap.allocate_transient(1, sampler_handle, sampler_handle_gpu))
	{
		LOG(ERROR) << "Failed to allocate " << 1 << " transient descriptor handle(s) of type " << static_cast<uint32_t>(api::descriptor_type::sampler) << '!';
		return;
	}

	D3D12_SAMPLER_DESC sampler_desc = {};
	sampler_desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

	_device_impl->_orig->CreateSampler(&sampler_desc, sampler_handle);

	for (uint32_t level = view_desc.texture.first_level; level < view_desc.texture.first_level + level_count;
			++level, base_handle = _device_impl->offset_descriptor_handle(base_handle, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
		srv_desc.Format = convert_format(api::format_to_default_typed(view_desc.format, 0));
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Texture2D.MostDetailedMip = level;
		srv_desc.Texture2D.PlaneSlice = 0;
		srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

		_device_impl->_orig->CreateShaderResourceView(reinterpret_cast<ID3D12Resource *>(resource.handle), &srv_desc, base_handle);
	}
	for (uint32_t level = view_desc.texture.first_level + 1; level < view_desc.texture.first_level + level_count;
			++level, base_handle = _device_impl->offset_descriptor_handle(base_handle, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		uav_desc.Format = convert_format(api::format_to_default_typed(view_desc.format, 0));
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice = level;
		uav_desc.Texture2D.PlaneSlice = 0;

		_device_impl->_orig->CreateUnorderedAccessView(reinterpret_cast<ID3D12Resource *>(resource.handle), nullptr, &uav_desc, base_handle);
	}

	if (_current_descriptor_heaps[0] != _device_impl->_gpu_sampler_heap.get() ||
		_current_descriptor_heaps[1] != _device_impl->_gpu_view_heap.get())
	{
		ID3D12DescriptorHeap *const heaps[2] = { _device_impl->_gpu_sampler_heap.get(), _device_impl->_gpu_view_heap.get() };
		std::copy_n(heaps, 2, _current_descriptor_heaps);
		_orig->SetDescriptorHeaps(2, heaps);
	}

	_orig->SetComputeRootSignature(_device_impl->_mipmap_signature.get());
	_orig->SetPipelineState(_device_impl->_mipmap_pipeline.get());

	_current_root_signature[1] = nullptr;

	_orig->SetComputeRootDescriptorTable(3, sampler_handle_gpu);

	D3D12_RESOURCE_BARRIER barriers[2];
	barriers[0] = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	barriers[0].Transition.pResource = reinterpret_cast<ID3D12Resource *>(resource.handle);
	barriers[1] = { D3D12_RESOURCE_BARRIER_TYPE_UAV };
	barriers[1].UAV.pResource = reinterpret_cast<ID3D12Resource *>(resource.handle);

	for (uint32_t level = view_desc.texture.first_level + 1; level < view_desc.texture.first_level + level_count; ++level)
	{
		barriers[0].Transition.Subresource = level;

		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		_orig->ResourceBarrier(1, barriers);

		const uint32_t width = std::max(1u, static_cast<uint32_t>(desc.Width) >> level);
		const uint32_t height = std::max(1u, desc.Height >> level);

		const float dimensions[2] = { 1.0f / width, 1.0f / height };
		_orig->SetComputeRoot32BitConstants(0, 2, dimensions, 0);

		// Bind next higher mipmap level as input
		_orig->SetComputeRootDescriptorTable(1, _device_impl->offset_descriptor_handle(base_handle_gpu, level - 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		// There is no UAV for level 0, so substract one
		_orig->SetComputeRootDescriptorTable(2, _device_impl->offset_descriptor_handle(base_handle_gpu, level_count + level - 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		_orig->Dispatch(std::max(1u, (width + 7) / 8), std::max(1u, (height + 7) / 8), 1);

		// Wait for all accesses to be finished, since the result will be the input for the next mipmap
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		_orig->ResourceBarrier(2, barriers);
	}
}

void reshade::d3d12::command_list_impl::begin_query(api::query_heap heap, api::query_type type, uint32_t index)
{
	_has_commands = true;

	assert(heap.handle != 0);

	_orig->BeginQuery(reinterpret_cast<ID3D12QueryHeap *>(heap.handle), convert_query_type(type), index);
}
void reshade::d3d12::command_list_impl::end_query(api::query_heap heap, api::query_type type, uint32_t index)
{
	_has_commands = true;

	assert(heap.handle != 0);

	_orig->EndQuery(reinterpret_cast<ID3D12QueryHeap *>(heap.handle), convert_query_type(type), index);
}
void reshade::d3d12::command_list_impl::copy_query_heap_results(api::query_heap heap, api::query_type type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride)
{
	_has_commands = true;

	assert(heap.handle != 0);
	assert(stride == sizeof(uint64_t));

	_orig->ResolveQueryData(reinterpret_cast<ID3D12QueryHeap *>(heap.handle), convert_query_type(type), first, count, reinterpret_cast<ID3D12Resource *>(dst.handle), dst_offset);
}

void reshade::d3d12::command_list_impl::copy_acceleration_structure(api::resource_view source, api::resource_view dest, api::acceleration_structure_copy_mode mode)
{
	_has_commands = true;

	if (_supports_ray_tracing)
		static_cast<ID3D12GraphicsCommandList4 *>(_orig)->CopyRaytracingAccelerationStructure(dest.handle, source.handle, convert_acceleration_structure_copy_mode(mode));
	else
		assert(false);
}
void reshade::d3d12::command_list_impl::build_acceleration_structure(api::acceleration_structure_type type, api::acceleration_structure_build_flags flags, uint32_t input_count, const api::acceleration_structure_build_input *inputs, api::resource scratch, uint64_t scratch_offset, api::resource_view source, api::resource_view dest, api::acceleration_structure_build_mode mode)
{
	_has_commands = true;

	if (_supports_ray_tracing)
	{
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
		desc.DestAccelerationStructureData = dest.handle;
		desc.Inputs.Type = static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE>(type);
		desc.Inputs.Flags = convert_acceleration_structure_build_flags(flags, mode);
		desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		desc.SourceAccelerationStructureData = source.handle;
		desc.ScratchAccelerationStructureData = (scratch.handle != 0 ? reinterpret_cast<ID3D12Resource *>(scratch.handle)->GetGPUVirtualAddress() : 0) + scratch_offset;

		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometries(input_count);

		if (type == api::acceleration_structure_type::top_level)
		{
			assert(input_count == 1 && inputs->type == api::acceleration_structure_build_input_type::instances);

			desc.Inputs.NumDescs = inputs->instances.count;
			desc.Inputs.DescsLayout = inputs->instances.array_of_pointers ? D3D12_ELEMENTS_LAYOUT_ARRAY_OF_POINTERS : D3D12_ELEMENTS_LAYOUT_ARRAY;
			desc.Inputs.InstanceDescs = (inputs->instances.buffer.handle != 0 ? reinterpret_cast<ID3D12Resource *>(inputs->instances.buffer.handle)->GetGPUVirtualAddress() : 0) + inputs->instances.offset;
		}
		else
		{
			for (uint32_t i = 0; i < input_count; ++i)
				convert_acceleration_structure_build_input(inputs[i], geometries[i]);

			desc.Inputs.NumDescs = static_cast<UINT>(geometries.size());
			desc.Inputs.pGeometryDescs = geometries.data();
		}

		static_cast<ID3D12GraphicsCommandList4 *>(_orig)->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	}
	else
	{
		assert(false);
	}
}

void reshade::d3d12::command_list_impl::begin_debug_event(const char *label, const float color[4])
{
	assert(label != nullptr);

#if 0
	// Metadata is WINPIX_EVENT_ANSI_VERSION
	_orig->BeginEvent(1, label, static_cast<UINT>(std::strlen(label)));
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
	assert(label != nullptr);

#if 0
	_orig->SetMarker(1, label, static_cast<UINT>(std::strlen(label)));
#else
	UINT64 pix3blob[64];
	encode_pix3blob(pix3blob, label, color);
	_orig->SetMarker(2, pix3blob, sizeof(pix3blob));
#endif
}
