/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_device_context.hpp"
#include "reshade_api_type_convert.hpp"

void reshade::d3d11::pipeline_impl::apply(ID3D11DeviceContext *ctx) const
{
	ctx->VSSetShader(vs.get(), nullptr, 0);
	ctx->HSSetShader(hs.get(), nullptr, 0);
	ctx->DSSetShader(ds.get(), nullptr, 0);
	ctx->GSSetShader(gs.get(), nullptr, 0);
	ctx->PSSetShader(ps.get(), nullptr, 0);
	ctx->IASetInputLayout(input_layout.get());
	ctx->IASetPrimitiveTopology(topology);
	ctx->OMSetBlendState(blend_state.get(), blend_constant, sample_mask);
	ctx->RSSetState(rasterizer_state.get());
	ctx->OMSetDepthStencilState(depth_stencil_state.get(), stencil_reference_value);
}

reshade::d3d11::command_list_impl::command_list_impl(device_impl *device, ID3D11CommandList *cmd_list) :
	api_object_impl(cmd_list), _device_impl(device)
{
#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_command_list>(this);
#endif
}
reshade::d3d11::command_list_impl::~command_list_impl()
{
#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_command_list>(this);
#endif
}

reshade::api::device *reshade::d3d11::command_list_impl::get_device()
{
	return _device_impl;
}

void reshade::d3d11::device_context_impl::barrier(uint32_t count, const api::resource *, const api::resource_usage *old_states, const api::resource_usage *new_states)
{
	bool transitions_away_from_shader_resource_usage = false;
	bool transitions_away_from_unordered_access_usage = false;
	for (UINT i = 0; i < count; ++i)
	{
		if ((old_states[i] & api::resource_usage::shader_resource) != api::resource_usage::undefined &&
			(new_states[i] & api::resource_usage::shader_resource) == api::resource_usage::undefined)
			transitions_away_from_shader_resource_usage = true;
		if ((old_states[i] & api::resource_usage::unordered_access) != api::resource_usage::undefined &&
			(new_states[i] & api::resource_usage::unordered_access) == api::resource_usage::undefined)
			transitions_away_from_unordered_access_usage = true;
	}

	// TODO: This should really only unbind the specific resources passed to this barrier command
	if (transitions_away_from_shader_resource_usage)
	{
		ID3D11ShaderResourceView *null_srv[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
		_orig->VSSetShaderResources(0, ARRAYSIZE(null_srv), null_srv);
		_orig->HSSetShaderResources(0, ARRAYSIZE(null_srv), null_srv);
		_orig->DSSetShaderResources(0, ARRAYSIZE(null_srv), null_srv);
		_orig->GSSetShaderResources(0, ARRAYSIZE(null_srv), null_srv);
		_orig->PSSetShaderResources(0, ARRAYSIZE(null_srv), null_srv);
		_orig->CSSetShaderResources(0, ARRAYSIZE(null_srv), null_srv);
	}
	if (transitions_away_from_unordered_access_usage)
	{
		const D3D_FEATURE_LEVEL feature_level = _device_impl->_orig->GetFeatureLevel();
		const UINT max_uav_bindings =
			feature_level >= D3D_FEATURE_LEVEL_11_1 ? D3D11_1_UAV_SLOT_COUNT :
			feature_level == D3D_FEATURE_LEVEL_11_0 ? D3D11_PS_CS_UAV_REGISTER_COUNT :
			feature_level >= D3D_FEATURE_LEVEL_10_0 ? D3D11_CS_4_X_UAV_REGISTER_COUNT : 0;

		ID3D11UnorderedAccessView *null_uav[D3D11_1_UAV_SLOT_COUNT] = {};
		_orig->CSSetUnorderedAccessViews(0, max_uav_bindings, null_uav, nullptr);
	}
}

void reshade::d3d11::device_context_impl::bind_pipeline(api::pipeline_type type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);

	switch (type)
	{
	case api::pipeline_type::compute:
		_orig->CSSetShader(reinterpret_cast<ID3D11ComputeShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_type::graphics:
		reinterpret_cast<pipeline_impl *>(pipeline.handle)->apply(_orig);
		break;
	case api::pipeline_type::graphics_vertex_shader:
		_orig->VSSetShader(reinterpret_cast<ID3D11VertexShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_type::graphics_hull_shader:
		_orig->HSSetShader(reinterpret_cast<ID3D11HullShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_type::graphics_domain_shader:
		_orig->DSSetShader(reinterpret_cast<ID3D11DomainShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_type::graphics_geometry_shader:
		_orig->GSSetShader(reinterpret_cast<ID3D11GeometryShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_type::graphics_pixel_shader:
		_orig->PSSetShader(reinterpret_cast<ID3D11PixelShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_type::graphics_blend_state:
		_orig->OMSetBlendState(reinterpret_cast<ID3D11BlendState *>(pipeline.handle), nullptr, D3D11_DEFAULT_SAMPLE_MASK);
		break;
	case api::pipeline_type::graphics_rasterizer_state:
		_orig->RSSetState(reinterpret_cast<ID3D11RasterizerState *>(pipeline.handle));
		break;
	case api::pipeline_type::graphics_depth_stencil_state:
		_orig->OMSetDepthStencilState(reinterpret_cast<ID3D11DepthStencilState *>(pipeline.handle), 0);
		break;
	default:
		assert(false);
		break;
	}
}
void reshade::d3d11::device_context_impl::bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values)
{
	for (UINT i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::pipeline_state::primitive_topology:
			_orig->IASetPrimitiveTopology(convert_primitive_topology(static_cast<api::primitive_topology>(values[i])));
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::d3d11::device_context_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	assert(first == 0);

	_orig->RSSetViewports(count, reinterpret_cast<const D3D11_VIEWPORT *>(viewports));
}
void reshade::d3d11::device_context_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	assert(first == 0);

	_orig->RSSetScissorRects(count, reinterpret_cast<const D3D10_RECT *>(rects));
}

void reshade::d3d11::device_context_impl::bind_samplers(api::shader_stage stage, uint32_t first, uint32_t count, const api::sampler *samplers)
{
	if (count > D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT)
	{
		assert(false);
		count = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D11SamplerState *sampler_ptrs[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		sampler_ptrs[i] = reinterpret_cast<ID3D11SamplerState *>(samplers[i].handle);
#else
	const auto sampler_ptrs = reinterpret_cast<ID3D11SamplerState *const *>(samplers);
#endif

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::hull) == api::shader_stage::hull)
		_orig->HSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::domain) == api::shader_stage::domain)
		_orig->DSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetSamplers(first, count, sampler_ptrs);
}
void reshade::d3d11::device_context_impl::bind_shader_resource_views(api::shader_stage stage, uint32_t first, uint32_t count, const api::resource_view *views)
{
	if (count > D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
	{
		assert(false);
		count = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D11ShaderResourceView *view_ptrs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		view_ptrs[i] = reinterpret_cast<ID3D11ShaderResourceView *>(views[i].handle);
#else
	const auto view_ptrs = reinterpret_cast<ID3D11ShaderResourceView *const *>(views);
#endif

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::hull) == api::shader_stage::hull)
		_orig->HSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::domain) == api::shader_stage::domain)
		_orig->DSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetShaderResources(first, count, view_ptrs);
}
void reshade::d3d11::device_context_impl::bind_unordered_access_views(api::shader_stage stage, uint32_t first, uint32_t count, const api::resource_view *views)
{
	if (count > D3D11_1_UAV_SLOT_COUNT)
	{
		assert(false);
		count = D3D11_1_UAV_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D11UnorderedAccessView *view_ptrs[D3D11_1_UAV_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		view_ptrs[i] = reinterpret_cast<ID3D11UnorderedAccessView *>(views[i].handle);
#else
	const auto view_ptrs = reinterpret_cast<ID3D11UnorderedAccessView *const *>(views);
#endif

	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, first, count, view_ptrs, nullptr);
	if ((stage & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetUnorderedAccessViews(first, count, view_ptrs, nullptr);
}
void reshade::d3d11::device_context_impl::bind_constant_buffers(api::shader_stage stage, uint32_t first, uint32_t count, const api::resource *buffers)
{
	if (count > D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
	{
		assert(false);
		count = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D11Buffer *buffer_ptrs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		buffer_ptrs[i] = reinterpret_cast<ID3D11Buffer *>(buffers[i].handle);
#else
	const auto buffer_ptrs = reinterpret_cast<ID3D11Buffer *const *>(buffers);
#endif

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::hull) == api::shader_stage::hull)
		_orig->HSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::domain) == api::shader_stage::domain)
		_orig->DSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetConstantBuffers(first, count, buffer_ptrs);
}

void reshade::d3d11::device_context_impl::push_constants(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, uint32_t first, uint32_t count, const void *values)
{
	assert(first == 0);

	if (count > _push_constants_size)
	{
		// Enlarge push constant buffer to fit new requirement
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = count * sizeof(uint32_t);
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		if (FAILED(_device_impl->_orig->CreateBuffer(&desc, nullptr, &_push_constants)))
		{
			LOG(ERROR) << "Failed to create push constant buffer!";
			return;
		}

		_device_impl->set_debug_name({ reinterpret_cast<uintptr_t>(_push_constants.get()) }, "Push constants");

		_push_constants_size = count;
	}

	const auto push_constants = _push_constants.get();

	// Discard the buffer to so driver can return a new memory region to avoid stalls
	if (D3D11_MAPPED_SUBRESOURCE mapped;
		SUCCEEDED(_orig->Map(push_constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		std::memcpy(static_cast<uint32_t *>(mapped.pData) + first, values, count * sizeof(uint32_t));
		_orig->Unmap(push_constants, 0);
	}

	const UINT push_constants_slot = layout.handle != 0 ?
		reinterpret_cast<pipeline_layout_impl *>(layout.handle)->shader_registers[layout_index] : 0;

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stage & api::shader_stage::hull) == api::shader_stage::hull)
		_orig->HSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stage & api::shader_stage::domain) == api::shader_stage::domain)
		_orig->DSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stage & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetConstantBuffers(push_constants_slot, 1, &push_constants);
}
void reshade::d3d11::device_context_impl::push_descriptors(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
{
	if (layout.handle != 0)
		first += reinterpret_cast<pipeline_layout_impl *>(layout.handle)->shader_registers[layout_index];

	switch (type)
	{
	case api::descriptor_type::sampler:
		bind_samplers(stage, first, count, static_cast<const api::sampler *>(descriptors));
		break;
	case api::descriptor_type::sampler_with_resource_view:
		assert(false);
		break;
	case api::descriptor_type::shader_resource_view:
		bind_shader_resource_views(stage, first, count, static_cast<const api::resource_view *>(descriptors));
		break;
	case api::descriptor_type::unordered_access_view:
		bind_unordered_access_views(stage, first, count, static_cast<const api::resource_view *>(descriptors));
		break;
	case api::descriptor_type::constant_buffer:
		bind_constant_buffers(stage, first, count, static_cast<const api::resource *>(descriptors));
		break;
	}
}
void reshade::d3d11::device_context_impl::bind_descriptor_sets(api::pipeline_type type, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)
{
	for (UINT i = 0; i < count; ++i)
	{
		const auto set_impl = reinterpret_cast<descriptor_set_impl *>(sets[i].handle);

		push_descriptors(
			type == api::pipeline_type::compute ? api::shader_stage::compute : api::shader_stage::all_graphics,
			layout,
			i + first,
			set_impl->type,
			0,
			static_cast<uint32_t>(set_impl->descriptors.size()),
			set_impl->descriptors.data());
	}
}

void reshade::d3d11::device_context_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	assert(offset <= std::numeric_limits<UINT>::max());
	assert(buffer.handle == 0 || index_size == 2 || index_size == 4);

	_orig->IASetIndexBuffer(reinterpret_cast<ID3D11Buffer *>(buffer.handle), index_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, static_cast<UINT>(offset));
}
void reshade::d3d11::device_context_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	if (count > D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
	{
		assert(false);
		count = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D11Buffer *buffer_ptrs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		buffer_ptrs[i] = reinterpret_cast<ID3D11Buffer *>(buffers[i].handle);
#else
	const auto buffer_ptrs = reinterpret_cast<ID3D11Buffer *const *>(buffers);
#endif

	UINT offsets_32[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		offsets_32[i] = static_cast<UINT>(offsets[i]);

	_orig->IASetVertexBuffers(first, count, buffer_ptrs, strides, offsets_32);
}

void reshade::d3d11::device_context_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	_orig->DrawInstanced(vertices, instances, first_vertex, first_instance);
}
void reshade::d3d11::device_context_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_orig->DrawIndexedInstanced(indices, instances, first_index, vertex_offset, first_instance);
}
void reshade::d3d11::device_context_impl::dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)
{
	_orig->Dispatch(num_groups_x, num_groups_y, num_groups_z);
}
void reshade::d3d11::device_context_impl::draw_or_dispatch_indirect(uint32_t type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	assert(offset <= std::numeric_limits<UINT>::max());

	switch (type)
	{
	case 1:
		for (UINT i = 0; i < draw_count; ++i)
			_orig->DrawInstancedIndirect(reinterpret_cast<ID3D11Buffer *>(buffer.handle), static_cast<UINT>(offset + i * stride));
		break;
	case 2:
		for (UINT i = 0; i < draw_count; ++i)
			_orig->DrawIndexedInstancedIndirect(reinterpret_cast<ID3D11Buffer *>(buffer.handle), static_cast<UINT>(offset + i * stride));
		break;
	case 3:
		for (UINT i = 0; i < draw_count; ++i)
			_orig->DispatchIndirect(reinterpret_cast<ID3D11Buffer *>(buffer.handle), static_cast<UINT>(offset + i * stride));
		break;
	}
}

void reshade::d3d11::device_context_impl::begin_render_pass(api::framebuffer fbo)
{
	assert(fbo.handle != 0);

	const auto fbo_impl = reinterpret_cast<const framebuffer_impl *>(fbo.handle);
	_orig->OMSetRenderTargets(fbo_impl->count, fbo_impl->rtv, fbo_impl->dsv);

	_current_fbo[0] = *fbo_impl;

	assert(!_has_open_render_pass);
	_has_open_render_pass = true;
}
void reshade::d3d11::device_context_impl::finish_render_pass()
{
	// Reset render targets
	_orig->OMSetRenderTargets(0, nullptr, nullptr);

	_current_fbo->count = 0;
	_current_fbo->dsv = nullptr;

	assert( _has_open_render_pass);
	_has_open_render_pass = false;
}

void reshade::d3d11::device_context_impl::copy_resource(api::resource src, api::resource dst)
{
	assert(src.handle != 0 && dst.handle != 0);

	_orig->CopyResource(reinterpret_cast<ID3D11Resource *>(dst.handle), reinterpret_cast<ID3D11Resource *>(src.handle));
}
void reshade::d3d11::device_context_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert(src_offset <= std::numeric_limits<UINT>::max() && dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	const D3D11_BOX src_box = { static_cast<UINT>(src_offset), 0, 0, static_cast<UINT>(src_offset + size), 1, 1 };

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D11Resource *>(dst.handle), 0, static_cast<UINT>(dst_offset), 0, 0,
		reinterpret_cast<ID3D11Resource *>(src.handle), 0, &src_box);
}
void reshade::d3d11::device_context_impl::copy_buffer_to_texture(api::resource, uint64_t, uint32_t, uint32_t, api::resource, uint32_t, const int32_t[6])
{
	assert(false);
}
void reshade::d3d11::device_context_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert((src_box == nullptr && dst_box == nullptr) || (src_box != nullptr && dst_box != nullptr &&
		(dst_box[3] - dst_box[0]) == (src_box[3] - src_box[0]) && // Blit between different region dimensions is not supported
		(dst_box[4] - dst_box[1]) == (src_box[4] - src_box[1]) &&
		(dst_box[5] - dst_box[2]) == (src_box[5] - src_box[2])));

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D11Resource *>(dst.handle), src_subresource, dst_box != nullptr ? dst_box[0] : 0, dst_box != nullptr ? dst_box[1] : 0, dst_box != nullptr ? dst_box[2] : 0,
		reinterpret_cast<ID3D11Resource *>(src.handle), dst_subresource, reinterpret_cast<const D3D11_BOX *>(src_box));
}
void reshade::d3d11::device_context_impl::copy_texture_to_buffer(api::resource, uint32_t, const int32_t[6], api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d11::device_context_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], api::format format)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert(src_box == nullptr && dst_offset == nullptr);

	_orig->ResolveSubresource(
		reinterpret_cast<ID3D11Resource *>(dst.handle), dst_subresource,
		reinterpret_cast<ID3D11Resource *>(src.handle), src_subresource, convert_format(format));
}

void reshade::d3d11::device_context_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);

	_orig->GenerateMips(reinterpret_cast<ID3D11ShaderResourceView *>(srv.handle));
}

void reshade::d3d11::device_context_impl::clear_attachments(api::format_aspect clear_flags, const float color[4], float depth, uint8_t stencil, uint32_t num_rects, const int32_t *)
{
	assert(num_rects == 0);
	assert(_has_open_render_pass);

	if ((clear_flags & api::format_aspect::color) != api::format_aspect::none)
		for (UINT i = 0; i < _current_fbo->count; ++i)
			_orig->ClearRenderTargetView(_current_fbo->rtv[i], color);
	if ((clear_flags & api::format_aspect::depth_stencil) != api::format_aspect::none)
		_orig->ClearDepthStencilView(_current_fbo->dsv, static_cast<UINT>(clear_flags) >> 1, depth, stencil);
}
void reshade::d3d11::device_context_impl::clear_depth_stencil_view(api::resource_view dsv, api::format_aspect clear_flags, float depth, uint8_t stencil, uint32_t num_rects, const int32_t *)
{
	assert(dsv.handle != 0 && num_rects == 0);

	_orig->ClearDepthStencilView(reinterpret_cast<ID3D11DepthStencilView *>(dsv.handle), static_cast<UINT>(clear_flags) >> 1, depth, stencil);
}
void reshade::d3d11::device_context_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t num_rects, const int32_t *)
{
	assert(rtv.handle != 0 && num_rects == 0);

	_orig->ClearRenderTargetView(reinterpret_cast<ID3D11RenderTargetView *>(rtv.handle), color);
}
void reshade::d3d11::device_context_impl::clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4], uint32_t num_rects, const int32_t *)
{
	assert(uav.handle != 0 && num_rects == 0);

	_orig->ClearUnorderedAccessViewUint(reinterpret_cast<ID3D11UnorderedAccessView *>(uav.handle), values);
}
void reshade::d3d11::device_context_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4], uint32_t num_rects, const int32_t *)
{
	assert(uav.handle != 0 && num_rects == 0);

	_orig->ClearUnorderedAccessViewFloat(reinterpret_cast<ID3D11UnorderedAccessView *>(uav.handle), values);
}

void reshade::d3d11::device_context_impl::begin_query(api::query_pool pool, api::query_type, uint32_t index)
{
	assert(pool.handle != 0);

	_orig->Begin(reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index].get());
}
void reshade::d3d11::device_context_impl::finish_query(api::query_pool pool, api::query_type, uint32_t index)
{
	assert(pool.handle != 0);

	_orig->End(reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index].get());
}
void reshade::d3d11::device_context_impl::copy_query_results(api::query_pool, api::query_type, uint32_t, uint32_t, api::resource, uint64_t, uint32_t)
{
	assert(false);
}

void reshade::d3d11::device_context_impl::add_debug_marker(const char *label, const float[4])
{
	if (_annotations == nullptr)
		return;

	const size_t label_len = strlen(label);
	std::wstring label_wide;
	label_wide.reserve(label_len + 1);
	utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

	_annotations->SetMarker(label_wide.c_str());
}
void reshade::d3d11::device_context_impl::begin_debug_marker(const char *label, const float[4])
{
	if (_annotations == nullptr)
		return;

	const size_t label_len = strlen(label);
	std::wstring label_wide;
	label_wide.reserve(label_len + 1);
	utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

	_annotations->BeginEvent(label_wide.c_str());
}
void reshade::d3d11::device_context_impl::finish_debug_marker()
{
	if (_annotations == nullptr)
		return;

	_annotations->EndEvent();
}
