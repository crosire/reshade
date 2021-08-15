/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d10_impl_device.hpp"
#include "d3d10_impl_type_convert.hpp"
#include <algorithm>

void reshade::d3d10::pipeline_impl::apply(ID3D10Device *ctx) const
{
	ctx->VSSetShader(vs.get());
	ctx->GSSetShader(gs.get());
	ctx->PSSetShader(ps.get());
	ctx->IASetInputLayout(input_layout.get());
	ctx->IASetPrimitiveTopology(topology);
	ctx->OMSetBlendState(blend_state.get(), blend_constant, sample_mask);
	ctx->RSSetState(rasterizer_state.get());
	ctx->OMSetDepthStencilState(depth_stencil_state.get(), stencil_reference_value);
}

void reshade::d3d10::device_impl::barrier(uint32_t count, const api::resource *, const api::resource_usage *old_states, const api::resource_usage *new_states)
{
	bool transitions_away_from_shader_resource_usage = false;
	for (uint32_t i = 0; i < count; ++i)
	{
		if ((old_states[i] & api::resource_usage::shader_resource) != api::resource_usage::undefined &&
			(new_states[i] & api::resource_usage::shader_resource) == api::resource_usage::undefined)
			transitions_away_from_shader_resource_usage = true;
	}

	// TODO: This should really only unbind the specific resources passed to this barrier command
	if (transitions_away_from_shader_resource_usage)
	{
		ID3D10ShaderResourceView *null_srv[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
		_orig->VSSetShaderResources(0, ARRAYSIZE(null_srv), null_srv);
		_orig->GSSetShaderResources(0, ARRAYSIZE(null_srv), null_srv);
		_orig->PSSetShaderResources(0, ARRAYSIZE(null_srv), null_srv);
	}
}

void reshade::d3d10::device_impl::begin_render_pass(api::render_pass, api::framebuffer fbo)
{
	assert(fbo.handle != 0);
	const auto fbo_impl = reinterpret_cast<const framebuffer_impl *>(fbo.handle);
	_orig->OMSetRenderTargets(fbo_impl->count, fbo_impl->rtv, fbo_impl->dsv);
}
void reshade::d3d10::device_impl::finish_render_pass()
{
	// Reset render targets
	_orig->OMSetRenderTargets(0, nullptr, nullptr);
}
void reshade::d3d10::device_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	if (count > D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT)
	{
		assert(false);
		count = D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT;
	}

#ifndef WIN64
	ID3D10RenderTargetView *rtv_ptrs[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for (uint32_t i = 0; i < count; ++i)
		rtv_ptrs[i] = reinterpret_cast<ID3D10RenderTargetView *>(rtvs[i].handle);
#else
	const auto rtv_ptrs = reinterpret_cast<ID3D10RenderTargetView *const *>(rtvs);
#endif

	_orig->OMSetRenderTargets(count, rtv_ptrs, reinterpret_cast<ID3D10DepthStencilView *>(dsv.handle));
}

void reshade::d3d10::device_impl::bind_pipeline(api::pipeline_stage type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);

	switch (type)
	{
	case api::pipeline_stage::all_graphics:
		reinterpret_cast<pipeline_impl *>(pipeline.handle)->apply(_orig);
		break;
	case api::pipeline_stage::input_assembler:
		_orig->IASetInputLayout(reinterpret_cast<ID3D10InputLayout *>(pipeline.handle));
		break;
	case api::pipeline_stage::vertex_shader:
		_orig->VSSetShader(reinterpret_cast<ID3D10VertexShader *>(pipeline.handle));
		break;
	case api::pipeline_stage::geometry_shader:
		_orig->GSSetShader(reinterpret_cast<ID3D10GeometryShader *>(pipeline.handle));
		break;
	case api::pipeline_stage::pixel_shader:
		_orig->PSSetShader(reinterpret_cast<ID3D10PixelShader *>(pipeline.handle));
		break;
	case api::pipeline_stage::rasterizer:
		_orig->RSSetState(reinterpret_cast<ID3D10RasterizerState *>(pipeline.handle));
		break;
	case api::pipeline_stage::depth_stencil:
		_orig->OMSetDepthStencilState(reinterpret_cast<ID3D10DepthStencilState *>(pipeline.handle), 0);
		break;
	case api::pipeline_stage::output_merger:
		_orig->OMSetBlendState(reinterpret_cast<ID3D10BlendState *>(pipeline.handle), nullptr, D3D10_DEFAULT_SAMPLE_MASK);
		break;
	default:
		assert(false);
		break;
	}
}
void reshade::d3d10::device_impl::bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::dynamic_state::primitive_topology:
			_orig->IASetPrimitiveTopology(static_cast<D3D10_PRIMITIVE_TOPOLOGY>(values[i]));
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::d3d10::device_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	assert(first == 0);

	if (count > D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
	{
		assert(false);
		count = D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	}

	D3D10_VIEWPORT viewport_data[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	for (uint32_t i = 0, k = 0; i < count; ++i, k += 6)
	{
		viewport_data[i].TopLeftX = static_cast<INT>(viewports[k + 0]);
		viewport_data[i].TopLeftY = static_cast<INT>(viewports[k + 1]);
		viewport_data[i].Width = static_cast<UINT>(viewports[k + 2]);
		viewport_data[i].Height = static_cast<UINT>(viewports[k + 3]);
		viewport_data[i].MinDepth = viewports[k + 4];
		viewport_data[i].MaxDepth = viewports[k + 5];
	}

	_orig->RSSetViewports(count, viewport_data);
}
void reshade::d3d10::device_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	assert(first == 0);

	_orig->RSSetScissorRects(count, reinterpret_cast<const D3D10_RECT *>(rects));
}

void reshade::d3d10::device_impl::bind_samplers(api::shader_stage stages, uint32_t first, uint32_t count, const api::sampler *samplers)
{
	if (count > D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT)
	{
		assert(false);
		count = D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D10SamplerState *sampler_ptrs[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for (uint32_t i = 0; i < count; ++i)
		sampler_ptrs[i] = reinterpret_cast<ID3D10SamplerState *>(samplers[i].handle);
#else
	const auto sampler_ptrs = reinterpret_cast<ID3D10SamplerState *const *>(samplers);
#endif

	if ((stages & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetSamplers(first, count, sampler_ptrs);
	if ((stages & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetSamplers(first, count, sampler_ptrs);
	if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetSamplers(first, count, sampler_ptrs);
}
void reshade::d3d10::device_impl::bind_shader_resource_views(api::shader_stage stages, uint32_t first, uint32_t count, const api::resource_view *views)
{
	if (count > D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
	{
		assert(false);
		count = D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D10ShaderResourceView *view_ptrs[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for (uint32_t i = 0; i < count; ++i)
		view_ptrs[i] = reinterpret_cast<ID3D10ShaderResourceView *>(views[i].handle);
#else
	const auto view_ptrs = reinterpret_cast<ID3D10ShaderResourceView *const *>(views);
#endif

	if ((stages & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetShaderResources(first, count, view_ptrs);
	if ((stages & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetShaderResources(first, count, view_ptrs);
	if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetShaderResources(first, count, view_ptrs);
}
void reshade::d3d10::device_impl::bind_constant_buffers(api::shader_stage stages, uint32_t first, uint32_t count, const api::buffer_range *buffer_ranges)
{
	if (count > D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
	{
		assert(false);
		count = D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	}

	ID3D10Buffer *buffer_ptrs[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for (uint32_t i = 0; i < count; ++i)
	{
		buffer_ptrs[i] = reinterpret_cast<ID3D10Buffer *>(buffer_ranges[i].buffer.handle);
		assert(buffer_ranges[i].offset == 0 && buffer_ranges[i].size == std::numeric_limits<uint64_t>::max());
	}

	if ((stages & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stages & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetConstantBuffers(first, count, buffer_ptrs);
}

void reshade::d3d10::device_impl::push_constants(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const void *values)
{
	assert(first == 0);

	if (count > _push_constants_size)
	{
		// Enlarge push constant buffer to fit new requirement
		D3D10_BUFFER_DESC desc = {};
		desc.ByteWidth = count * sizeof(uint32_t);
		desc.Usage = D3D10_USAGE_DYNAMIC;
		desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

		if (FAILED(_orig->CreateBuffer(&desc, nullptr, &_push_constants)))
		{
			LOG(ERROR) << "Failed to create push constant buffer!";
			return;
		}

		set_resource_name({ reinterpret_cast<uintptr_t>(_push_constants.get()) }, "Push constants");

		_push_constants_size = count;
	}

	const auto push_constants = _push_constants.get();

	// Discard the buffer to so driver can return a new memory region to avoid stalls
	if (uint32_t *mapped_data;
		SUCCEEDED(push_constants->Map(D3D10_MAP_WRITE_DISCARD, 0, reinterpret_cast<void **>(&mapped_data))))
	{
		std::memcpy(mapped_data + first, values, count * sizeof(uint32_t));
		push_constants->Unmap();
	}

	const UINT push_constants_slot = layout.handle != 0 ?
		reinterpret_cast<pipeline_layout_impl *>(layout.handle)->shader_registers[layout_param] : 0;

	if ((stages & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stages & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetConstantBuffers(push_constants_slot, 1, &push_constants);
}
void reshade::d3d10::device_impl::push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
{
	if (layout.handle != 0)
		first += reinterpret_cast<pipeline_layout_impl *>(layout.handle)->shader_registers[layout_param];

	switch (type)
	{
	case api::descriptor_type::sampler:
		bind_samplers(stages, first, count, static_cast<const api::sampler *>(descriptors));
		break;
	case api::descriptor_type::shader_resource_view:
		bind_shader_resource_views(stages, first, count, static_cast<const api::resource_view *>(descriptors));
		break;
	case api::descriptor_type::constant_buffer:
		bind_constant_buffers(stages, first, count, static_cast<const api::buffer_range *>(descriptors));
		break;
	default:
		assert(false);
		break;
	}
}
void reshade::d3d10::device_impl::bind_descriptor_sets(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets, const uint32_t *offsets)
{
	assert(sets != nullptr);

	for (uint32_t i = 0; i < count; ++i)
	{
		const auto set_impl = reinterpret_cast<const descriptor_set_impl *>(sets[i].handle);
		const auto set_offset = (offsets != nullptr) ? offsets[i] : 0;

		push_descriptors(
			stages,
			layout,
			first + i,
			set_impl->type,
			0,
			set_impl->count - set_offset,
			set_impl->descriptors.data() + set_offset * (set_impl->descriptors.size() / set_impl->count));
	}
}

void reshade::d3d10::device_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	assert(offset <= std::numeric_limits<UINT>::max());
	assert(buffer.handle == 0 || index_size == 2 || index_size == 4);

	_orig->IASetIndexBuffer(reinterpret_cast<ID3D10Buffer *>(buffer.handle), index_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, static_cast<UINT>(offset));
}
void reshade::d3d10::device_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	if (count > D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
	{
		assert(false);
		count = D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D10Buffer *buffer_ptrs[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (uint32_t i = 0; i < count; ++i)
		buffer_ptrs[i] = reinterpret_cast<ID3D10Buffer *>(buffers[i].handle);
#else
	const auto buffer_ptrs = reinterpret_cast<ID3D10Buffer *const *>(buffers);
#endif

	UINT offsets_32[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (uint32_t i = 0; i < count; ++i)
		offsets_32[i] = static_cast<UINT>(offsets[i]);

	_orig->IASetVertexBuffers(first, count, buffer_ptrs, strides, offsets_32);
}

void reshade::d3d10::device_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	_orig->DrawInstanced(vertices, instances, first_vertex, first_instance);
}
void reshade::d3d10::device_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_orig->DrawIndexedInstanced(indices, instances, first_index, vertex_offset, first_instance);
}
void reshade::d3d10::device_impl::dispatch(uint32_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d10::device_impl::draw_or_dispatch_indirect(api::indirect_command, api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d10::device_impl::copy_resource(api::resource src, api::resource dst)
{
	assert(src.handle != 0 && dst.handle != 0);

	_orig->CopyResource(reinterpret_cast<ID3D10Resource *>(dst.handle), reinterpret_cast<ID3D10Resource *>(src.handle));
}
void reshade::d3d10::device_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(src.handle != 0 && dst.handle != 0);

	if (size == std::numeric_limits<uint64_t>::max())
	{
		D3D10_BUFFER_DESC desc;
		reinterpret_cast<ID3D10Buffer *>(src.handle)->GetDesc(&desc);
		size  = desc.ByteWidth;
	}

	assert(src_offset <= std::numeric_limits<UINT>::max() && dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	const D3D10_BOX src_box = { static_cast<UINT>(src_offset), 0, 0, static_cast<UINT>(src_offset + size), 1, 1 };

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D10Resource *>(dst.handle), 0, static_cast<UINT>(dst_offset), 0, 0,
		reinterpret_cast<ID3D10Resource *>(src.handle), 0, &src_box);
}
void reshade::d3d10::device_impl::copy_buffer_to_texture(api::resource, uint64_t, uint32_t, uint32_t, api::resource, uint32_t, const int32_t[6])
{
	assert(false);
}
void reshade::d3d10::device_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::filter_type)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert((src_box == nullptr && dst_box == nullptr) || (src_box != nullptr && dst_box != nullptr &&
		(dst_box[3] - dst_box[0]) == (src_box[3] - src_box[0]) && // Blit between different region dimensions is not supported
		(dst_box[4] - dst_box[1]) == (src_box[4] - src_box[1]) &&
		(dst_box[5] - dst_box[2]) == (src_box[5] - src_box[2])));

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D10Resource *>(dst.handle), src_subresource, dst_box != nullptr ? dst_box[0] : 0, dst_box != nullptr ? dst_box[1] : 0, dst_box != nullptr ? dst_box[2] : 0,
		reinterpret_cast<ID3D10Resource *>(src.handle), dst_subresource, reinterpret_cast<const D3D10_BOX *>(src_box));
}
void reshade::d3d10::device_impl::copy_texture_to_buffer(api::resource, uint32_t, const int32_t[6], api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d10::device_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], api::format format)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert(src_box == nullptr && dst_offset == nullptr);

	_orig->ResolveSubresource(
		reinterpret_cast<ID3D10Resource *>(dst.handle), dst_subresource,
		reinterpret_cast<ID3D10Resource *>(src.handle), src_subresource, convert_format(format));
}

void reshade::d3d10::device_impl::clear_attachments(api::attachment_type clear_flags, const float color[4], float depth, uint8_t stencil, uint32_t num_rects, const int32_t *)
{
	assert(num_rects == 0);

	com_ptr<ID3D10DepthStencilView> dsv;
	com_ptr<ID3D10RenderTargetView> rtv[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
	_orig->OMGetRenderTargets(ARRAYSIZE(rtv), reinterpret_cast<ID3D10RenderTargetView **>(rtv), &dsv);

	if (static_cast<UINT>(clear_flags & (api::attachment_type::color)) != 0)
		for (UINT i = 0; i < ARRAYSIZE(rtv) && rtv[i] != nullptr; ++i)
			_orig->ClearRenderTargetView(rtv[i].get(), color);
	if (static_cast<UINT>(clear_flags & (api::attachment_type::depth | api::attachment_type::stencil)) != 0 && dsv != nullptr)
		_orig->ClearDepthStencilView(dsv.get(), static_cast<UINT>(clear_flags) >> 1, depth, stencil);
}
void reshade::d3d10::device_impl::clear_depth_stencil_view(api::resource_view dsv, api::attachment_type clear_flags, float depth, uint8_t stencil, uint32_t num_rects, const int32_t *)
{
	assert(dsv.handle != 0 && num_rects == 0);

	_orig->ClearDepthStencilView(reinterpret_cast<ID3D10DepthStencilView *>(dsv.handle), static_cast<UINT>(clear_flags) >> 1, depth, stencil);
}
void reshade::d3d10::device_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t num_rects, const int32_t *)
{
	assert(rtv.handle != 0 && num_rects == 0);

	_orig->ClearRenderTargetView(reinterpret_cast<ID3D10RenderTargetView *>(rtv.handle), color);
}
void reshade::d3d10::device_impl::clear_unordered_access_view_uint(api::resource_view, const uint32_t[4], uint32_t, const int32_t *)
{
	assert(false);
}
void reshade::d3d10::device_impl::clear_unordered_access_view_float(api::resource_view, const float[4], uint32_t, const int32_t *)
{
	assert(false);
}

void reshade::d3d10::device_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);

	_orig->GenerateMips(reinterpret_cast<ID3D10ShaderResourceView *>(srv.handle));
}

void reshade::d3d10::device_impl::begin_query(api::query_pool pool, api::query_type, uint32_t index)
{
	assert(pool.handle != 0);

	reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index]->Begin();
}
void reshade::d3d10::device_impl::finish_query(api::query_pool pool, api::query_type, uint32_t index)
{
	assert(pool.handle != 0);

	reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index]->End();
}
void reshade::d3d10::device_impl::copy_query_pool_results(api::query_pool, api::query_type, uint32_t, uint32_t, api::resource, uint64_t, uint32_t)
{
	assert(false);
}
