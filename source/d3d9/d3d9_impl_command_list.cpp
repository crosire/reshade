/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d9_impl_device.hpp"
#include "d3d9_impl_type_convert.hpp"
#include <cstring> // std::strlen
#include <algorithm> // std::max
#include <utf8/unchecked.h>

static void convert_cube_uv_to_vec(D3DCUBEMAP_FACES face, float u, float v, float &x, float &y, float &z)
{
	u = 2.0f * u - 1.0f;
	v = 2.0f * v - 1.0f;

	switch (face)
	{
	case D3DCUBEMAP_FACE_POSITIVE_X:
		x =  1.0f;
		y =  v;
		z = -u;
		break;
	case D3DCUBEMAP_FACE_NEGATIVE_X:
		x = -1.0f;
		y =  v;
		z =  u;
		break;
	case D3DCUBEMAP_FACE_POSITIVE_Y:
		x =  u;
		y =  1.0f;
		z = -v;
		break;
	case D3DCUBEMAP_FACE_NEGATIVE_Y:
		x =  u;
		y = -1.0f;
		z =  v;
		break;
	case D3DCUBEMAP_FACE_POSITIVE_Z:
		x =  u;
		y =  v;
		z =  1.0f;
		break;
	case D3DCUBEMAP_FACE_NEGATIVE_Z:
		x = -u;
		y =  v;
		z = -1.0f;
		break;
	}
}

extern const RECT *convert_box_to_rect(const reshade::api::subresource_box *box, RECT &rect);

void reshade::d3d9::device_impl::begin_render_pass(uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds)
{
	DWORD clear_flags = 0;

	temp_mem<api::resource_view, 8> rtv_handles(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		rtv_handles[i] = rts[i].view;

		if (rts[i].load_op == api::render_pass_load_op::clear)
			clear_flags |= D3DCLEAR_TARGET; // This will clear all render targets, not just the current one ...
	}

	api::resource_view depth_stencil_handle = {};
	if (ds != nullptr && ds->view != 0)
	{
		depth_stencil_handle = ds->view;

		if (ds->depth_load_op == api::render_pass_load_op::clear)
			clear_flags |= D3DCLEAR_ZBUFFER;
		if (ds->stencil_load_op == api::render_pass_load_op::clear)
			clear_flags |= D3DCLEAR_STENCIL;
	}

	bind_render_targets_and_depth_stencil(count, rtv_handles.p, depth_stencil_handle);

	if (clear_flags != 0)
	{
		_orig->Clear(
			0, nullptr,
			clear_flags,
			rts != nullptr ? D3DCOLOR_COLORVALUE(rts->clear_color[0], rts->clear_color[1], rts->clear_color[2], rts->clear_color[3]) : 0,
			depth_stencil_handle != 0 ? ds->clear_depth : 0.0f, depth_stencil_handle != 0 ? ds->clear_stencil : 0);
	}
}
void reshade::d3d9::device_impl::end_render_pass()
{
	// This fixes artifacts in Dragon's Dogma: Dark Arisen for some reason (even though this render state is reset by the state block in 'swapchain_impl' as well)
	_orig->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
}
void reshade::d3d9::device_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	if (count != 0)
	{
		assert(count <= _caps.NumSimultaneousRTs);

		bool srgb_write_enable = false;

		for (DWORD i = 0; i < count; ++i)
		{
			_orig->SetRenderTarget(i, reinterpret_cast<IDirect3DSurface9 *>(rtvs[i].handle & ~1ull));

			if (rtvs[i].handle & 1ull)
				srgb_write_enable = true;
		}

		// Unset remaining render targets
		for (DWORD i = count; i < _caps.NumSimultaneousRTs; ++i)
		{
			_orig->SetRenderTarget(i, nullptr);
		}

		_orig->SetRenderState(D3DRS_SRGBWRITEENABLE, srgb_write_enable);
	}

	_orig->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9 *>(dsv.handle));
}

void reshade::d3d9::device_impl::bind_pipeline(api::pipeline_stage stages, api::pipeline pipeline)
{
	if (pipeline.handle & 1)
	{
		const auto pipeline_object = reinterpret_cast<pipeline_impl *>(pipeline.handle ^ 1);

		// This is a pipeline handle created with 'device_impl::create_pipeline', which does not support partial binding of its state
		assert(stages == api::pipeline_stage::all_graphics);
		pipeline_object->state_block->Apply();

		if ((stages & api::pipeline_stage::input_assembler) != 0 && pipeline_object->prim_type != 0)
			_current_prim_type = pipeline_object->prim_type;
		return;
	}

	switch (stages)
	{
	case api::pipeline_stage::vertex_shader:
		_orig->SetVertexShader(reinterpret_cast<IDirect3DVertexShader9 *>(pipeline.handle));
		break;
	case api::pipeline_stage::pixel_shader:
		_orig->SetPixelShader(reinterpret_cast<IDirect3DPixelShader9 *>(pipeline.handle));
		break;
	case api::pipeline_stage::input_assembler:
		_orig->SetVertexDeclaration(reinterpret_cast<IDirect3DVertexDeclaration9 *>(pipeline.handle));
		break;
	default:
		assert(false);
		break;
	}
}
void reshade::d3d9::device_impl::bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::dynamic_state::primitive_topology:
			_current_prim_type = convert_primitive_topology(static_cast<api::primitive_topology>(values[i]));
			break;
		case api::dynamic_state::front_counter_clockwise:
		case api::dynamic_state::depth_bias_clamp:
		case api::dynamic_state::logic_op_enable:
		case api::dynamic_state::logic_op:
		case api::dynamic_state::back_stencil_read_mask:
		case api::dynamic_state::back_stencil_write_mask:
		case api::dynamic_state::back_stencil_reference_value:
			assert(false);
			break;
		case api::dynamic_state::alpha_to_coverage_enable:
			// See "Advanced DX9 Capabilities for ATI Radeon Cards" document
			_orig->SetRenderState(D3DRS_POINTSIZE, values[i] ? MAKEFOURCC('A', '2', 'M', '1') : MAKEFOURCC('A', '2', 'M', '0'));

			_orig->SetRenderState(D3DRS_ADAPTIVETESS_Y, values[i] ? MAKEFOURCC('A', 'T', 'O', 'C') : 0);
			if (values[i])
				_orig->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
			break;
		case api::dynamic_state::color_blend_op:
		case api::dynamic_state::alpha_blend_op:
			_orig->SetRenderState(convert_dynamic_state(states[i]), convert_blend_op(static_cast<api::blend_op>(values[i])));
			break;
		case api::dynamic_state::source_color_blend_factor:
		case api::dynamic_state::dest_color_blend_factor:
		case api::dynamic_state::source_alpha_blend_factor:
		case api::dynamic_state::dest_alpha_blend_factor:
			_orig->SetRenderState(convert_dynamic_state(states[i]), convert_blend_factor(static_cast<api::blend_factor>(values[i])));
			break;
		case api::dynamic_state::fill_mode:
			_orig->SetRenderState(convert_dynamic_state(states[i]), convert_fill_mode(static_cast<api::fill_mode>(values[i])));
			break;
		case api::dynamic_state::cull_mode:
			_orig->SetRenderState(convert_dynamic_state(states[i]), convert_cull_mode(static_cast<api::cull_mode>(values[i]), false));
			break;
		case api::dynamic_state::depth_func:
		case api::dynamic_state::alpha_func:
		case api::dynamic_state::front_stencil_func:
		case api::dynamic_state::back_stencil_func:
			_orig->SetRenderState(convert_dynamic_state(states[i]), convert_compare_op(static_cast<api::compare_op>(values[i])));
			break;
		case api::dynamic_state::front_stencil_pass_op:
		case api::dynamic_state::front_stencil_fail_op:
		case api::dynamic_state::front_stencil_depth_fail_op:
		case api::dynamic_state::back_stencil_pass_op:
		case api::dynamic_state::back_stencil_fail_op:
		case api::dynamic_state::back_stencil_depth_fail_op:
			_orig->SetRenderState(convert_dynamic_state(states[i]), convert_stencil_op(static_cast<api::stencil_op>(values[i])));
			break;
		default:
			_orig->SetRenderState(convert_dynamic_state(states[i]), values[i]);
			break;
		}
	}
}
void reshade::d3d9::device_impl::bind_viewports(uint32_t first, uint32_t count, const api::viewport *viewports)
{
	if (count == 0)
		return;

	if (first != 0)
	{
		assert(false);
		return;
	}

	assert(count == 1 && viewports != nullptr);

	D3DVIEWPORT9 d3d_viewport;
	d3d_viewport.X = static_cast<DWORD>(viewports->x);
	d3d_viewport.Y = static_cast<DWORD>(viewports->y);
	d3d_viewport.Width = static_cast<DWORD>(viewports->width);
	d3d_viewport.Height = static_cast<DWORD>(viewports->height);
	d3d_viewport.MinZ = viewports->min_depth;
	d3d_viewport.MaxZ = viewports->max_depth;

	_orig->SetViewport(&d3d_viewport);
}
void reshade::d3d9::device_impl::bind_scissor_rects(uint32_t first, uint32_t count, const api::rect *rects)
{
	if (count == 0)
		return;

	if (first != 0)
	{
		assert(false);
		return;
	}

	assert(count == 1 && rects != nullptr);

	_orig->SetScissorRect(reinterpret_cast<const RECT *>(rects));
}

void reshade::d3d9::device_impl::push_constants(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const void *values)
{
	if (layout == 0 || reinterpret_cast<pipeline_layout_impl *>(layout.handle)->ranges[layout_param].binding == UINT32_MAX)
	{
		switch (layout_param)
		{
		case 2:
			assert(stages == api::shader_stage::vertex);
			break;
		case 3:
			assert(stages == api::shader_stage::vertex);
			_orig->SetVertexShaderConstantI(first / 4, static_cast<const int *>(values), count / 4);
			return;
		case 4:
			assert(stages == api::shader_stage::vertex);
			_orig->SetVertexShaderConstantB(first, static_cast<const BOOL *>(values), count);
			return;
		case 5:
			assert(stages == api::shader_stage::pixel);
			break;
		case 6:
			assert(stages == api::shader_stage::pixel);
			_orig->SetPixelShaderConstantI(first / 4, static_cast<const int *>(values), count / 4);
			return;
		case 7:
			assert(stages == api::shader_stage::pixel);
			_orig->SetPixelShaderConstantB(first, static_cast<const BOOL *>(values), count);
			return;
		}
	}

	assert((first % 4) == 0 && (count % 4) == 0);

	first /= 4;
	count /= 4;

	if ((stages & api::shader_stage::vertex) == api::shader_stage::vertex)
	{
		// https://learn.microsoft.com/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-vs-registers-vs-3-0#input-registers
		assert((first + count) <= _caps.MaxVertexShaderConst);
		_orig->SetVertexShaderConstantF(first, static_cast<const float *>(values), count);
	}
	if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
	{
		// https://learn.microsoft.com/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-ps-registers-ps-3-0#input-register-types
		// Technically limited based on the pixel shader version, but more seem to work on modern hardware and drivers, so do not check:
		//   assert((first + count) <= (_caps.PixelShaderVersion < D3DPS_VERSION(2, 0) ? 8 : _caps.PixelShaderVersion < D3DPS_VERSION(3, 0) ? 32 : 224));
		_orig->SetPixelShaderConstantF(first, static_cast<const float *>(values), count);
	}
}
void reshade::d3d9::device_impl::push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_table_update &update)
{
	assert(update.table == 0 && update.array_offset == 0);

	uint32_t first = update.binding;
	if (layout != 0)
	{
		const api::descriptor_range &range = reinterpret_cast<pipeline_layout_impl *>(layout.handle)->ranges[layout_param];

		assert(update.binding >= range.binding);
		first -= range.binding;
		first += range.dx_register_index;
		stages &= range.visibility;
	}

	// Set for each individual shader stage (pixel stage first, since vertex stage modifies the binding offset)
	constexpr api::shader_stage stages_to_iterate[] = { api::shader_stage::pixel, api::shader_stage::vertex };
	for (api::shader_stage stage : stages_to_iterate)
	{
		if ((stages & stage) == 0)
			continue;

		uint32_t count = update.count;
		if (stage == api::shader_stage::vertex)
		{
			// See https://docs.microsoft.com/windows/win32/direct3d9/vertex-textures-in-vs-3-0
			first += D3DVERTEXTEXTURESAMPLER0;
			if ((first + count) > (D3DVERTEXTEXTURESAMPLER3 + 1)) // The vertex engine only contains four texture sampler stages
				count = (D3DVERTEXTEXTURESAMPLER3 + 1) - first;
		}

		switch (update.type)
		{
		case api::descriptor_type::sampler:
			for (uint32_t i = 0; i < count; ++i)
			{
				const auto &descriptor = static_cast<const api::sampler *>(update.descriptors)[i];
				if (descriptor == 0)
					continue;

				const sampler_impl *const sampler_instance = reinterpret_cast<const sampler_impl *>(descriptor.handle);
				for (D3DSAMPLERSTATETYPE state = D3DSAMP_ADDRESSU; state <= D3DSAMP_MAXANISOTROPY; state = static_cast<D3DSAMPLERSTATETYPE>(state + 1))
					_orig->SetSamplerState(first + i, state, sampler_instance->state[state]);
			}
			break;
		case api::descriptor_type::sampler_with_resource_view:
			for (uint32_t i = 0; i < count; ++i)
			{
				const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(update.descriptors)[i];
				_orig->SetTexture(first + i, reinterpret_cast<IDirect3DBaseTexture9 *>(descriptor.view.handle & ~1ull));
				_orig->SetSamplerState(first + i, D3DSAMP_SRGBTEXTURE, descriptor.view.handle & 1);

				if (descriptor.sampler == 0)
					continue;

				const sampler_impl *const sampler_instance = reinterpret_cast<const sampler_impl *>(descriptor.sampler.handle);
				for (D3DSAMPLERSTATETYPE state = D3DSAMP_ADDRESSU; state <= D3DSAMP_MAXANISOTROPY; state = static_cast<D3DSAMPLERSTATETYPE>(state + 1))
					_orig->SetSamplerState(first + i, state, sampler_instance->state[state]);
			}
			break;
		case api::descriptor_type::shader_resource_view:
			for (uint32_t i = 0; i < count; ++i)
			{
				const auto &descriptor = static_cast<const api::resource_view *>(update.descriptors)[i];
				_orig->SetTexture(first + i, reinterpret_cast<IDirect3DBaseTexture9 *>(descriptor.handle & ~1ull));
				_orig->SetSamplerState(first + i, D3DSAMP_SRGBTEXTURE, descriptor.handle & 1);
			}
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::d3d9::device_impl::bind_descriptor_tables(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_table *tables)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto table_impl = reinterpret_cast<const descriptor_table_impl *>(tables[i].handle);

		push_descriptors(
			stages,
			layout,
			first + i,
			api::descriptor_table_update { {}, table_impl->base_binding, 0, table_impl->count, table_impl->type, table_impl->descriptors.data() });
	}
}

void reshade::d3d9::device_impl::bind_index_buffer(api::resource buffer, [[maybe_unused]] uint64_t offset, [[maybe_unused]] uint32_t index_size)
{
#ifndef NDEBUG
	assert(offset == 0);

	if (buffer != 0)
	{
		assert(index_size == 2 || index_size == 4);

		D3DINDEXBUFFER_DESC desc;
		reinterpret_cast<IDirect3DIndexBuffer9 *>(buffer.handle)->GetDesc(&desc);
		assert(desc.Format == (index_size >= 4 ? D3DFMT_INDEX32 : D3DFMT_INDEX16));
	}
#endif

	_orig->SetIndices(reinterpret_cast<IDirect3DIndexBuffer9 *>(buffer.handle));
}
void reshade::d3d9::device_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		assert(offsets == nullptr || offsets[i] <= std::numeric_limits<UINT>::max());

		_orig->SetStreamSource(first + i, reinterpret_cast<IDirect3DVertexBuffer9 *>(buffers[i].handle), offsets != nullptr ? static_cast<UINT>(offsets[i]) : 0, strides[i]);
	}
}
void reshade::d3d9::device_impl::bind_stream_output_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint64_t *, const api::resource *, const uint64_t *)
{
	assert(first == 0 && count == 1);
	assert(offsets == nullptr || offsets[0] <= std::numeric_limits<UINT>::max());

	_current_stream_output = reinterpret_cast<IDirect3DVertexBuffer9 *>(buffers[0].handle);
	_current_stream_output_offset = (offsets != nullptr) ? static_cast<UINT>(offsets[0]) : 0;
}

void reshade::d3d9::device_impl::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	assert(instance_count == 1 && first_instance == 0);
	assert(_current_prim_type != 0); // Need to bind a primitive topology before performing draw call

	if (_current_stream_output != nullptr)
	{
		com_ptr<IDirect3DVertexDeclaration9> decl;
		_orig->GetVertexDeclaration(&decl);

		_orig->ProcessVertices(first_vertex, _current_stream_output_offset, vertex_count, _current_stream_output, decl.get(), 0);
		return;
	}

	_orig->DrawPrimitive(_current_prim_type, first_vertex, calc_prim_from_vertex_count(_current_prim_type, vertex_count));
}
void reshade::d3d9::device_impl::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	assert(instance_count == 1 && first_instance == 0);
	assert(_current_prim_type != 0);

	// Estimate maximum vertex count based on the size of the current vertex buffer
	// This is needed for D3D9On12, which uses the vertex count to update deferred input buffers
	UINT vertex_count = 0;
	{
		com_ptr<IDirect3DVertexBuffer9> stream;
		UINT offset = 0, stride = 0;
		if (SUCCEEDED(_orig->GetStreamSource(0, &stream, &offset, &stride)) && stream != nullptr)
		{
			D3DVERTEXBUFFER_DESC desc;
			if (SUCCEEDED(stream->GetDesc(&desc)))
			{
				vertex_count = ((desc.Size - offset) / stride) - vertex_offset;
			}
		}
	}

	_orig->DrawIndexedPrimitive(_current_prim_type, vertex_offset, 0, vertex_count, first_index, calc_prim_from_vertex_count(_current_prim_type, index_count));
}
void reshade::d3d9::device_impl::dispatch(uint32_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d9::device_impl::dispatch_mesh(uint32_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d9::device_impl::dispatch_rays(api::resource, uint64_t, uint64_t, api::resource, uint64_t, uint64_t, uint64_t, api::resource, uint64_t, uint64_t, uint64_t, api::resource, uint64_t, uint64_t, uint64_t, uint32_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d9::device_impl::draw_or_dispatch_indirect(api::indirect_command, api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d9::device_impl::copy_resource(api::resource src, api::resource dst)
{
	const api::resource_desc desc = get_resource_desc(src);

	if (desc.type == api::resource_type::buffer)
	{
		copy_buffer_region(src, 0, dst, 0, UINT64_MAX);
	}
	else
	{
		for (uint32_t layer = 0; layer < desc.texture.depth_or_layers; ++layer)
		{
			for (uint32_t level = 0; level < desc.texture.levels; ++level)
			{
				const uint32_t subresource = level + layer * desc.texture.levels;

				copy_texture_region(src, subresource, nullptr, dst, subresource, nullptr, api::filter_mode::min_mag_mip_point);
			}
		}
	}
}
void reshade::d3d9::device_impl::copy_buffer_region(api::resource, uint64_t, api::resource, uint64_t, uint64_t)
{
	assert(false);
}
void reshade::d3d9::device_impl::copy_buffer_to_texture(api::resource, uint64_t, uint32_t, uint32_t, api::resource, uint32_t, const api::subresource_box *)
{
	assert(false);
}
void reshade::d3d9::device_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, const api::subresource_box *dst_box, api::filter_mode filter)
{
	assert(src != 0 && dst != 0);

	const auto src_object = reinterpret_cast<IDirect3DResource9 *>(src.handle);
	const auto dst_object = reinterpret_cast<IDirect3DResource9 *>(dst.handle);

	RECT src_rect, dst_rect;
	bool src_is_regular_texture = false;
	bool dst_is_regular_texture = false;

	// Default to no filtering if no stretching needs to be performed (prevents artifacts when copying depth data)
	D3DTEXTUREFILTERTYPE stretch_filter_type = D3DTEXF_POINT;
	if ((src_box != nullptr || dst_box != nullptr) && (filter == api::filter_mode::min_mag_mip_linear || filter == api::filter_mode::min_mag_linear_mip_point))
		stretch_filter_type = D3DTEXF_LINEAR;

	switch (IDirect3DResource9_GetType(src_object) | (IDirect3DResource9_GetType(dst_object) << 4))
	{
		case D3DRTYPE_SURFACE | (D3DRTYPE_SURFACE << 4): // Copy from surface to surface
		{
			assert(src_subresource == 0 && dst_subresource == 0);

			com_ptr<IDirect3DSurface9> src_surface = static_cast<IDirect3DSurface9 *>(src_object);
			com_ptr<IDirect3DSurface9> dst_surface = static_cast<IDirect3DSurface9 *>(dst_object);

			D3DSURFACE_DESC src_desc;
			IDirect3DSurface9_GetDesc(src_surface.get(), &src_desc);
			D3DSURFACE_DESC dst_desc;
			IDirect3DSurface9_GetDesc(dst_surface.get(), &dst_desc);

			if (dst_desc.Pool == D3DPOOL_DEFAULT && src_desc.Pool == D3DPOOL_SYSTEMMEM)
			{
				_orig->UpdateSurface(
					src_surface.get(), convert_box_to_rect(src_box, src_rect),
					dst_surface.get(), reinterpret_cast<const POINT *>(convert_box_to_rect(dst_box, dst_rect)));
				return;
			}
			if (dst_desc.Pool == D3DPOOL_SYSTEMMEM && src_desc.Pool == D3DPOOL_DEFAULT)
			{
				assert(src_box == nullptr && dst_box == nullptr);
				assert((src_desc.Usage & D3DUSAGE_RENDERTARGET) != 0);
				_orig->GetRenderTargetData(src_surface.get(), dst_surface.get());
				return;
			}

			assert(dst_desc.Pool == D3DPOOL_DEFAULT && src_desc.Pool == D3DPOOL_DEFAULT);

			_orig->StretchRect(
				src_surface.get(), convert_box_to_rect(src_box, src_rect),
				dst_surface.get(), convert_box_to_rect(dst_box, dst_rect), stretch_filter_type);
			return;
		}
		case D3DRTYPE_SURFACE | (D3DRTYPE_TEXTURE << 4): // Copy from surface to texture
		{
			dst_is_regular_texture = true;
			[[fallthrough]];
		}
		case D3DRTYPE_SURFACE | (D3DRTYPE_CUBETEXTURE << 4): // Copy from surface to cube texture
		{
			assert(src_subresource == 0);

			com_ptr<IDirect3DSurface9> src_surface = static_cast<IDirect3DSurface9 *>(src_object);

			D3DSURFACE_DESC src_desc;
			IDirect3DSurface9_GetDesc(src_surface.get(), &src_desc);

			if ((src_desc.Usage & D3DUSAGE_DEPTHSTENCIL) != 0 && dst_subresource == 0 && check_capability(api::device_caps::resolve_depth_stencil))
			{
				// Capture and restore state
				_backup_state.capture();

				_orig->SetTexture(0, static_cast<IDirect3DBaseTexture9 *>(dst_object));
				_orig->SetDepthStencilSurface(src_surface.get());

				// Dummy draw call, required to ensure the destination texture is set before the resolve operation is triggered below
				_orig->SetRenderState(D3DRS_ZENABLE, FALSE);
				_orig->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
				_orig->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
				const float dummy_point[3] = { 0.0, 0.0f, 0.0f };
				_orig->DrawPrimitiveUP(D3DPT_POINTLIST, 1, dummy_point, sizeof(dummy_point));

				// Trigger multisampled depth buffer resolve operation (see "Advanced DX9 Capabilities for ATI Radeon Cards" document)
				_orig->SetRenderState(D3DRS_POINTSIZE, 0x7FA05000 /* RESZ code */);

				_backup_state.apply_and_release();
				return;
			}

			const DWORD dst_level_count = IDirect3DBaseTexture9_GetLevelCount(static_cast<IDirect3DBaseTexture9 *>(dst_object));
			const DWORD dst_level = dst_subresource % dst_level_count;

			com_ptr<IDirect3DSurface9> dst_surface;
			if (FAILED(dst_is_regular_texture ?
					IDirect3DTexture9_GetSurfaceLevel(static_cast<IDirect3DTexture9 *>(dst_object), dst_level, &dst_surface) :
					IDirect3DCubeTexture9_GetCubeMapSurface(static_cast<IDirect3DCubeTexture9 *>(dst_object), static_cast<D3DCUBEMAP_FACES>(dst_subresource / dst_level_count), dst_level, &dst_surface)))
				break;

			D3DSURFACE_DESC dst_desc;
			IDirect3DSurface9_GetDesc(dst_surface.get(), &dst_desc);

			if (dst_desc.Pool == D3DPOOL_DEFAULT && src_desc.Pool == D3DPOOL_SYSTEMMEM)
			{
				_orig->UpdateSurface(
					src_surface.get(), convert_box_to_rect(src_box, src_rect),
					dst_surface.get(), reinterpret_cast<const POINT *>(convert_box_to_rect(dst_box, dst_rect)));
				return;
			}
			if (dst_desc.Pool == D3DPOOL_SYSTEMMEM && src_desc.Pool == D3DPOOL_DEFAULT)
			{
				assert(src_box == nullptr && dst_box == nullptr);
				assert((src_desc.Usage & D3DUSAGE_RENDERTARGET) != 0);
				_orig->GetRenderTargetData(src_surface.get(), dst_surface.get());
				return;
			}

			assert(dst_desc.Pool == D3DPOOL_DEFAULT && src_desc.Pool == D3DPOOL_DEFAULT);

			_orig->StretchRect(
				src_surface.get(), convert_box_to_rect(src_box, src_rect),
				dst_surface.get(), convert_box_to_rect(dst_box, dst_rect), stretch_filter_type);
			return;
		}
		case D3DRTYPE_TEXTURE | (D3DRTYPE_TEXTURE << 4): // Copy from texture to texture
		{
			src_is_regular_texture = true;
			[[fallthrough]];
		}
		case D3DRTYPE_CUBETEXTURE | (D3DRTYPE_TEXTURE << 4): // Copy from cube texture to texture
		{
			const DWORD src_level_count = IDirect3DBaseTexture9_GetLevelCount(static_cast<IDirect3DBaseTexture9 *>(src_object));
			const DWORD src_level = src_subresource % src_level_count;

			com_ptr<IDirect3DSurface9> src_surface;
			if (FAILED(src_is_regular_texture ?
					IDirect3DTexture9_GetSurfaceLevel(static_cast<IDirect3DTexture9 *>(src_object), src_level, &src_surface) :
					IDirect3DCubeTexture9_GetCubeMapSurface(static_cast<IDirect3DCubeTexture9 *>(src_object), static_cast<D3DCUBEMAP_FACES>(src_subresource / src_level_count), src_level, &src_surface)))
				break;

			com_ptr<IDirect3DSurface9> dst_surface;
			if (FAILED(IDirect3DTexture9_GetSurfaceLevel(static_cast<IDirect3DTexture9 *>(dst_object), dst_subresource, &dst_surface)))
				break;

			D3DSURFACE_DESC src_desc;
			IDirect3DSurface9_GetDesc(src_surface.get(), &src_desc);
			D3DSURFACE_DESC dst_desc;
			IDirect3DSurface9_GetDesc(dst_surface.get(), &dst_desc);

			com_ptr<IDirect3DSurface9> target_surface = dst_surface;

			if (dst_desc.Pool == D3DPOOL_DEFAULT && src_desc.Pool == D3DPOOL_SYSTEMMEM)
			{
				_orig->UpdateSurface(
					src_surface.get(), convert_box_to_rect(src_box, src_rect),
					dst_surface.get(), reinterpret_cast<const POINT *>(convert_box_to_rect(dst_box, dst_rect)));
				return;
			}
			if (dst_desc.Pool == D3DPOOL_SYSTEMMEM)
			{
				if (src_box == nullptr && dst_box == nullptr && src_desc.Pool == D3DPOOL_DEFAULT && (src_desc.Usage & D3DUSAGE_RENDERTARGET) != 0)
				{
					_orig->GetRenderTargetData(src_surface.get(), dst_surface.get());
					return;
				}
				else
				{
					dst_desc.Pool = D3DPOOL_DEFAULT;
					dst_desc.Usage = D3DUSAGE_RENDERTARGET;
					target_surface.reset();

					if (FAILED(create_surface_replacement(dst_desc, &target_surface)))
						break;
				}
			}

			assert(dst_desc.Pool == D3DPOOL_DEFAULT && (src_desc.Pool == D3DPOOL_DEFAULT || src_desc.Pool == D3DPOOL_MANAGED || src_desc.Pool == D3DPOOL_MANAGED_EX));

			if (_copy_state == nullptr)
				return;

			// Capture and restore state, render targets, depth-stencil surface and viewport (which all may change next)
			_backup_state.capture();

			// Perform copy using rasterization pipeline
			_copy_state->Apply();

			_orig->SetTexture(0, static_cast<IDirect3DBaseTexture9 *>(src_object));
			_orig->SetSamplerState(0, D3DSAMP_MINFILTER, stretch_filter_type);
			_orig->SetSamplerState(0, D3DSAMP_MAGFILTER, stretch_filter_type);
			_orig->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, src_level);

			assert((dst_desc.Usage & D3DUSAGE_RENDERTARGET) != 0);

			_orig->SetRenderTarget(0, target_surface.get());
			for (DWORD i = 1; i < _caps.NumSimultaneousRTs; ++i)
				_orig->SetRenderTarget(i, nullptr);
			_orig->SetDepthStencilSurface(nullptr);

			if (dst_box != nullptr)
			{
				convert_box_to_rect(dst_box, dst_rect);

				D3DVIEWPORT9 viewport;
				viewport.X = dst_rect.left;
				viewport.Y = dst_rect.top;
				viewport.Width = dst_rect.right - dst_rect.left;
				viewport.Height = dst_rect.bottom - dst_rect.top;
				viewport.MinZ = 0.0f;
				viewport.MaxZ = 1.0f;

				_orig->SetViewport(&viewport);

				dst_desc.Width = viewport.Width;
				dst_desc.Height = viewport.Height;
			}

			// Bake half-pixel offset into vertices
			const float half_pixel_offset[2] = {
				-1.0f / dst_desc.Width, 1.0f / dst_desc.Height
			};

			if (src_is_regular_texture)
			{
				_orig->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);

				float vertices[3][5] = {
					// x      y      z      tu     tv
					{ -1.0f,  1.0f,  0.0f,  0.0f,  0.0f },
					{  3.0f,  1.0f,  0.0f,  2.0f,  0.0f },
					{ -1.0f, -3.0f,  0.0f,  0.0f,  2.0f },
				};

				for (int v = 0; v < 3; ++v)
				{
					vertices[v][0] += half_pixel_offset[0];
					vertices[v][1] += half_pixel_offset[1];
				}

				if (src_box != nullptr)
				{
					vertices[0][3] = static_cast<float>(src_box->left  ) * 2.0f / dst_desc.Width;
					vertices[0][4] = static_cast<float>(src_box->top   ) * 2.0f / dst_desc.Height;
					vertices[1][3] = static_cast<float>(src_box->right ) * 2.0f / dst_desc.Width;
					vertices[1][4] = static_cast<float>(src_box->top   ) * 2.0f / dst_desc.Height;
					vertices[2][3] = static_cast<float>(src_box->left  ) * 2.0f / dst_desc.Width;
					vertices[2][4] = static_cast<float>(src_box->bottom) * 2.0f / dst_desc.Height;
				}

				_orig->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, vertices, sizeof(vertices[0]));
			}
			else
			{
				// See also https://docs.microsoft.com/windows/win32/direct3d9/cubic-environment-mapping
				_orig->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0));

				float vertices[4][6] = {
					// x      y      z      tx     ty     tz
					{ -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f },
					{  1.0f,  1.0f,  0.0f,  1.0f,  1.0f,  0.0f },
					{ -1.0f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f },
					{  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,  0.0f },
				};

				for (int v = 0; v < 4; ++v)
				{
					vertices[v][0] += half_pixel_offset[0];
					vertices[v][1] += half_pixel_offset[1];
				}

				if (src_box != nullptr)
				{
					vertices[0][3] = static_cast<float>(src_box->left  ) / dst_desc.Width;
					vertices[0][4] = static_cast<float>(src_box->bottom) / dst_desc.Height;
					vertices[1][3] = static_cast<float>(src_box->right ) / dst_desc.Width;
					vertices[1][4] = static_cast<float>(src_box->bottom) / dst_desc.Height;
					vertices[2][3] = static_cast<float>(src_box->left  ) / dst_desc.Width;
					vertices[2][4] = static_cast<float>(src_box->top   ) / dst_desc.Height;
					vertices[3][3] = static_cast<float>(src_box->right ) / dst_desc.Width;
					vertices[3][4] = static_cast<float>(src_box->top   ) / dst_desc.Height;
				}

				const auto face = static_cast<D3DCUBEMAP_FACES>(src_subresource / src_level_count);
				for (int i = 0; i < 4; ++i)
					convert_cube_uv_to_vec(face, vertices[i][3], vertices[i][4], vertices[i][3], vertices[i][4], vertices[i][5]);

				_orig->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(vertices[0]));
			}

			_backup_state.apply_and_release();

			if (target_surface != dst_surface)
			{
				assert(dst_box == nullptr);
				_orig->GetRenderTargetData(target_surface.get(), dst_surface.get());
			}
			return;
		}
		case D3DRTYPE_TEXTURE | (D3DRTYPE_SURFACE << 4): // Copy from texture to surface
		{
			src_is_regular_texture = true;
			[[fallthrough]];
		}
		case D3DRTYPE_CUBETEXTURE | (D3DRTYPE_SURFACE << 4): // Copy from cube texture to surface
		{
			assert(dst_subresource == 0);

			const DWORD src_level_count = IDirect3DBaseTexture9_GetLevelCount(static_cast<IDirect3DBaseTexture9 *>(src_object));
			const DWORD src_level = src_subresource % src_level_count;

			com_ptr<IDirect3DSurface9> src_surface;
			if (FAILED(src_is_regular_texture ?
					IDirect3DTexture9_GetSurfaceLevel(static_cast<IDirect3DTexture9 *>(src_object), src_level, &src_surface) :
					IDirect3DCubeTexture9_GetCubeMapSurface(static_cast<IDirect3DCubeTexture9 *>(src_object), static_cast<D3DCUBEMAP_FACES>(src_subresource / src_level_count), src_level, &src_surface)))
				break;

			com_ptr<IDirect3DSurface9> dst_surface = static_cast<IDirect3DSurface9 *>(dst_object);

			D3DSURFACE_DESC src_desc;
			IDirect3DSurface9_GetDesc(src_surface.get(), &src_desc);
			D3DSURFACE_DESC dst_desc;
			IDirect3DSurface9_GetDesc(dst_surface.get(), &dst_desc);

			com_ptr<IDirect3DSurface9> target_surface = dst_surface;

			if (dst_desc.Pool == D3DPOOL_DEFAULT && src_desc.Pool == D3DPOOL_SYSTEMMEM)
			{
				_orig->UpdateSurface(
					src_surface.get(), convert_box_to_rect(src_box, src_rect),
					dst_surface.get(), reinterpret_cast<const POINT *>(convert_box_to_rect(dst_box, dst_rect)));
				return;
			}
			if (dst_desc.Pool == D3DPOOL_SYSTEMMEM)
			{
				if (src_box == nullptr && dst_box == nullptr && src_desc.Pool == D3DPOOL_DEFAULT && (src_desc.Usage & D3DUSAGE_RENDERTARGET) != 0)
				{
					_orig->GetRenderTargetData(src_surface.get(), dst_surface.get());
					return;
				}
				else
				{
					dst_desc.Pool = D3DPOOL_DEFAULT;
					dst_desc.Usage = D3DUSAGE_RENDERTARGET;
					target_surface.reset();

					if (FAILED(create_surface_replacement(dst_desc, &target_surface)))
						break;
				}
			}

			assert(dst_desc.Pool == D3DPOOL_DEFAULT && src_desc.Pool == D3DPOOL_DEFAULT);

			_orig->StretchRect(
				src_surface.get(), convert_box_to_rect(src_box, src_rect),
				target_surface.get(), convert_box_to_rect(dst_box, dst_rect), stretch_filter_type);

			if (target_surface != dst_surface)
			{
				assert(dst_box == nullptr);
				_orig->GetRenderTargetData(target_surface.get(), dst_surface.get());
			}
			return;
		}
		case D3DRTYPE_VOLUMETEXTURE | (D3DRTYPE_VOLUMETEXTURE << 4): // Copy from volume texture to volume texture
		{
			if (src_subresource != 0 || dst_subresource != 0)
				return;

			com_ptr<IDirect3DVolumeTexture9> src_volume = static_cast<IDirect3DVolumeTexture9 *>(src_object);
			com_ptr<IDirect3DVolumeTexture9> dst_volume = static_cast<IDirect3DVolumeTexture9 *>(dst_object);

			D3DVOLUME_DESC src_desc;
			IDirect3DVolumeTexture9_GetLevelDesc(src_volume.get(), 0, &src_desc);
			D3DVOLUME_DESC dst_desc;
			IDirect3DVolumeTexture9_GetLevelDesc(dst_volume.get(), 0, &dst_desc);

			if (dst_desc.Pool == D3DPOOL_DEFAULT && src_desc.Pool == D3DPOOL_SYSTEMMEM)
			{
				_orig->UpdateTexture(src_volume.get(), dst_volume.get());
				return;
			}
			break;
		}
		case D3DRTYPE_CUBETEXTURE | (D3DRTYPE_CUBETEXTURE << 4): // Copy from cube texture to cube texture
		{
			const DWORD src_level_count = IDirect3DBaseTexture9_GetLevelCount(static_cast<IDirect3DBaseTexture9 *>(src_object));
			const DWORD src_level = src_subresource % src_level_count;

			const DWORD dst_level_count = IDirect3DBaseTexture9_GetLevelCount(static_cast<IDirect3DBaseTexture9 *>(dst_object));
			const DWORD dst_level = dst_subresource % dst_level_count;

			com_ptr<IDirect3DSurface9> src_surface;
			if (FAILED(IDirect3DCubeTexture9_GetCubeMapSurface(static_cast<IDirect3DCubeTexture9 *>(src_object), static_cast<D3DCUBEMAP_FACES>(src_subresource / src_level_count), src_level, &src_surface)))
				break;

			com_ptr<IDirect3DSurface9> dst_surface;
			if (FAILED(IDirect3DCubeTexture9_GetCubeMapSurface(static_cast<IDirect3DCubeTexture9 *>(dst_object), static_cast<D3DCUBEMAP_FACES>(dst_subresource / dst_level_count), dst_level, &dst_surface)))
				break;

			D3DSURFACE_DESC src_desc;
			IDirect3DSurface9_GetDesc(src_surface.get(), &src_desc);
			D3DSURFACE_DESC dst_desc;
			IDirect3DSurface9_GetDesc(dst_surface.get(), &dst_desc);

			if (dst_desc.Pool == D3DPOOL_DEFAULT && src_desc.Pool == D3DPOOL_SYSTEMMEM)
			{
				_orig->UpdateSurface(
					src_surface.get(), convert_box_to_rect(src_box, src_rect),
					dst_surface.get(), reinterpret_cast<const POINT *>(convert_box_to_rect(dst_box, dst_rect)));
				return;
			}
			if (dst_desc.Pool == D3DPOOL_SYSTEMMEM && src_desc.Pool == D3DPOOL_DEFAULT)
			{
				assert(src_box == nullptr && dst_box == nullptr);
				assert((src_desc.Usage & D3DUSAGE_RENDERTARGET) != 0);
				_orig->GetRenderTargetData(src_surface.get(), dst_surface.get());
				return;
			}

			assert(dst_desc.Pool == D3DPOOL_DEFAULT && src_desc.Pool == D3DPOOL_DEFAULT);

			_orig->StretchRect(
				src_surface.get(), convert_box_to_rect(src_box, src_rect),
				dst_surface.get(), convert_box_to_rect(dst_box, dst_rect), stretch_filter_type);
			return;
		}
	}

	assert(false); // Not implemented
}
void reshade::d3d9::device_impl::copy_texture_to_buffer(api::resource, uint32_t, const api::subresource_box *, api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d9::device_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, uint32_t dst_x, uint32_t dst_y, uint32_t dst_z, api::format)
{
	api::subresource_box dst_box;
	dst_box.left  = dst_x;
	dst_box.top   = dst_y;
	dst_box.front = dst_z;

	if (src_box != nullptr)
	{
		dst_box.right  = dst_x + src_box->width();
		dst_box.bottom = dst_y + src_box->height();
		dst_box.back   = dst_z + src_box->depth();
	}
	else
	{
		const api::resource_desc desc = get_resource_desc(dst);

		dst_box.right  = dst_x + std::max(1u, desc.texture.width >> (dst_subresource % desc.texture.levels));
		dst_box.bottom = dst_y + std::max(1u, desc.texture.height >> (dst_subresource % desc.texture.levels));
		dst_box.back   = dst_z + 1;
	}

	copy_texture_region(src, src_subresource, src_box, dst, dst_subresource, &dst_box, api::filter_mode::min_mag_mip_point);
}

void reshade::d3d9::device_impl::clear_depth_stencil_view(api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const api::rect *)
{
	assert(dsv != 0 && rect_count == 0); // Clearing rectangles is not supported

	com_ptr<IDirect3DSurface9> prev_surface;
	_orig->GetDepthStencilSurface(&prev_surface);
	if (prev_surface != reinterpret_cast<IDirect3DSurface9 *>(dsv.handle))
		_orig->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9 *>(dsv.handle));

	_orig->Clear(
		0, nullptr,
		(depth != nullptr ? D3DCLEAR_ZBUFFER : 0u) | (stencil != nullptr ? D3DCLEAR_STENCIL : 0u),
		0, depth != nullptr ? *depth : 0.0f, stencil != nullptr ? *stencil : 0);

	if (prev_surface != reinterpret_cast<IDirect3DSurface9 *>(dsv.handle))
		_orig->SetDepthStencilSurface(prev_surface.get());
}
void reshade::d3d9::device_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t rect_count, const api::rect *rects)
{
	assert(rtv != 0 && color != nullptr);

#if 1
	for (size_t i = 0; i < std::max(rect_count, 1u); ++i)
		_orig->ColorFill(reinterpret_cast<IDirect3DSurface9 *>(rtv.handle & ~1ull), reinterpret_cast<const RECT *>(rects + i * 4), D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]));
#else
	assert(rect_count == 0); // Clearing rectangles is not supported

	_backup_state.capture();

	_orig->SetRenderTarget(0, reinterpret_cast<IDirect3DSurface9 *>(rtv.handle & ~1ull));
	for (UINT i = 1; i < _caps.NumSimultaneousRTs; ++i)
		_orig->SetRenderTarget(i, nullptr);

	_orig->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]), 1.0f, 0);

	_backup_state.apply_and_release();
#endif
}
void reshade::d3d9::device_impl::clear_unordered_access_view_uint(api::resource_view, const uint32_t[4], uint32_t, const api::rect *)
{
	assert(false);
}
void reshade::d3d9::device_impl::clear_unordered_access_view_float(api::resource_view, const float[4], uint32_t, const api::rect *)
{
	assert(false);
}

void reshade::d3d9::device_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv != 0);

	const auto texture = reinterpret_cast<IDirect3DBaseTexture9 *>(srv.handle & ~1ull);

	texture->SetAutoGenFilterType(D3DTEXF_LINEAR);
	texture->GenerateMipSubLevels();
}

void reshade::d3d9::device_impl::begin_query(api::query_heap heap, api::query_type, uint32_t index)
{
	assert(heap != 0);

	reinterpret_cast<query_heap_impl *>(heap.handle)->queries[index]->Issue(D3DISSUE_BEGIN);
}
void reshade::d3d9::device_impl::end_query(api::query_heap heap, api::query_type, uint32_t index)
{
	assert(heap != 0);

	reinterpret_cast<query_heap_impl *>(heap.handle)->queries[index]->Issue(D3DISSUE_END);
}
void reshade::d3d9::device_impl::copy_query_heap_results(api::query_heap, api::query_type, uint32_t, uint32_t, api::resource, uint64_t, uint32_t)
{
	assert(false);
}

void reshade::d3d9::device_impl::copy_acceleration_structure(api::resource_view, api::resource_view, api::acceleration_structure_copy_mode)
{
	assert(false);
}
void reshade::d3d9::device_impl::build_acceleration_structure(api::acceleration_structure_type, api::acceleration_structure_build_flags, uint32_t, const api::acceleration_structure_build_input *, api::resource, uint64_t, api::resource_view, api::resource_view, api::acceleration_structure_build_mode)
{
	assert(false);
}
void reshade::d3d9::device_impl::query_acceleration_structures(uint32_t, const api::resource_view *, api::query_heap, api::query_type, uint32_t)
{
	assert(false);
}

void reshade::d3d9::device_impl::begin_debug_event(const char *label, const float color[4])
{
	assert(label != nullptr);

	const size_t label_len = std::strlen(label);
	std::wstring label_wide;
	label_wide.reserve(label_len + 1);
	utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

	D3DPERF_BeginEvent(color != nullptr ? D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]) : 0, label_wide.c_str());
}
void reshade::d3d9::device_impl::end_debug_event()
{
	D3DPERF_EndEvent();
}
void reshade::d3d9::device_impl::insert_debug_marker(const char *label, const float color[4])
{
	assert(label != nullptr);

	const size_t label_len = std::strlen(label);
	std::wstring label_wide;
	label_wide.reserve(label_len + 1);
	utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

	D3DPERF_SetMarker(color != nullptr ? D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]) : 0, label_wide.c_str());
}
