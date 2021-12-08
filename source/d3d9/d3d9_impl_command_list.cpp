/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d9_impl_device.hpp"
#include "d3d9_impl_type_convert.hpp"
#include <algorithm>
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
	if (count > _caps.NumSimultaneousRTs)
	{
		assert(false);
		count = _caps.NumSimultaneousRTs;
	}

	DWORD clear_flags = 0;
	api::resource_view rtv_handles[8], depth_stencil_handle = {};

	for (uint32_t i = 0; i < count; ++i)
	{
		rtv_handles[i] = rts[i].view;

		if (rts[i].load_op == api::render_pass_load_op::clear)
			clear_flags |= D3DCLEAR_TARGET;
	}

	if (ds != nullptr)
	{
		depth_stencil_handle = ds->view;

		if (ds->depth_load_op == api::render_pass_load_op::clear)
			clear_flags |= D3DCLEAR_ZBUFFER;
		if (ds->stencil_load_op == api::render_pass_load_op::clear)
			clear_flags |= D3DCLEAR_STENCIL;
	}

	bind_render_targets_and_depth_stencil(count, rtv_handles, depth_stencil_handle);

	if (clear_flags != 0)
		_orig->Clear(
			0, nullptr,
			clear_flags,
			clear_flags & D3DCLEAR_TARGET  ? D3DCOLOR_COLORVALUE(rts->clear_color[0], rts->clear_color[1], rts->clear_color[2], rts->clear_color[3]) : 0,
			clear_flags & D3DCLEAR_ZBUFFER ? ds->clear_depth : 0.0f,
			clear_flags & D3DCLEAR_STENCIL ? ds->clear_stencil : 0);
}
void reshade::d3d9::device_impl::end_render_pass()
{
}
void reshade::d3d9::device_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	assert(count <= _caps.NumSimultaneousRTs);

	bool srgb_write_enable = false;

	for (DWORD i = 0; i < count; ++i)
	{
		_orig->SetRenderTarget(i, reinterpret_cast<IDirect3DSurface9 *>(rtvs[i].handle & ~1ull));

		if (rtvs[i].handle & 1ull)
			srgb_write_enable = true;
	}

	for (DWORD i = count; i < _caps.NumSimultaneousRTs; ++i)
		_orig->SetRenderTarget(i, nullptr);

	_orig->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9 *>(dsv.handle));

	_orig->SetRenderState(D3DRS_SRGBWRITEENABLE, srgb_write_enable);
}

void reshade::d3d9::device_impl::bind_pipeline(api::pipeline_stage type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);

	switch (type)
	{
	case api::pipeline_stage::all_graphics:
		assert(pipeline.handle & 1);
		reinterpret_cast<pipeline_impl *>(pipeline.handle ^ 1)->state_block->Apply();
		_current_prim_type = reinterpret_cast<pipeline_impl *>(pipeline.handle ^ 1)->prim_type;
		break;
	case api::pipeline_stage::input_assembler:
		_orig->SetVertexDeclaration(reinterpret_cast<IDirect3DVertexDeclaration9 *>(pipeline.handle));
		break;
	case api::pipeline_stage::vertex_shader:
		_orig->SetVertexShader(reinterpret_cast<IDirect3DVertexShader9 *>(pipeline.handle));
		break;
	case api::pipeline_stage::pixel_shader:
		_orig->SetPixelShader(reinterpret_cast<IDirect3DPixelShader9 *>(pipeline.handle));
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
		case api::dynamic_state::alpha_to_coverage_enable:
		case api::dynamic_state::logic_op_enable:
		case api::dynamic_state::logic_op:
			assert(false);
			break;
		default:
			_orig->SetRenderState(static_cast<D3DRENDERSTATETYPE>(states[i]), values[i]);
			break;
		}
	}
}
void reshade::d3d9::device_impl::bind_viewports(uint32_t first, uint32_t count, const api::viewport *viewports)
{
	if (first != 0 || count == 0)
		return;

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
	if (first != 0 || count == 0)
		return;

	assert(count == 1 && rects != nullptr);

	_orig->SetScissorRect(reinterpret_cast<const RECT *>(rects));
}

void reshade::d3d9::device_impl::push_constants(api::shader_stage stages, api::pipeline_layout, uint32_t, uint32_t first, uint32_t count, const void *values)
{
	if ((stages & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->SetVertexShaderConstantF(first / 4, static_cast<const float *>(values), count / 4);
	if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->SetPixelShaderConstantF(first / 4, static_cast<const float *>(values), count / 4);
}
void reshade::d3d9::device_impl::push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_set_update &update)
{
	assert(update.set.handle == 0);

	uint32_t first = update.offset, count = update.count;
	if (layout.handle != 0 && layout != global_pipeline_layout)
		first = reinterpret_cast<pipeline_layout_impl *>(layout.handle)->shader_registers[layout_param];

	// Set for each individual shader stage (pixel stage first, since vertex stage modifies the binding offset)
	constexpr api::shader_stage stages_to_iterate[] = { api::shader_stage::pixel, api::shader_stage::vertex };
	for (api::shader_stage stage : stages_to_iterate)
	{
		if ((stages & stage) != stage)
			continue;

		if (stage == api::shader_stage::vertex)
		{
			// See https://docs.microsoft.com/windows/win32/direct3d9/vertex-textures-in-vs-3-0
			first += D3DVERTEXTEXTURESAMPLER0;
			if ((first + count) > D3DVERTEXTEXTURESAMPLER3) // The vertex engine only contains four texture sampler stages
				count = D3DVERTEXTEXTURESAMPLER3 - first;
		}

		switch (update.type)
		{
		case api::descriptor_type::sampler:
			for (uint32_t i = 0; i < count; ++i)
			{
				const auto &descriptor = static_cast<const api::sampler *>(update.descriptors)[i];

				if (descriptor.handle != 0)
				{
					const auto sampler_instance = reinterpret_cast<const sampler_impl *>(descriptor.handle);

					for (D3DSAMPLERSTATETYPE state = D3DSAMP_ADDRESSU; state <= D3DSAMP_MAXANISOTROPY; state = static_cast<D3DSAMPLERSTATETYPE>(state + 1))
						_orig->SetSamplerState(first + i, state, sampler_instance->state[state]);
				}
			}
			break;
		case api::descriptor_type::sampler_with_resource_view:
			for (uint32_t i = 0; i < count; ++i)
			{
				const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(update.descriptors)[i];
				_orig->SetTexture(i + first, reinterpret_cast<IDirect3DBaseTexture9 *>(descriptor.view.handle & ~1ull));
				_orig->SetSamplerState(i + first, D3DSAMP_SRGBTEXTURE, descriptor.view.handle & 1);

				if (descriptor.sampler.handle != 0)
				{
					const auto sampler_instance = reinterpret_cast<const sampler_impl *>(descriptor.sampler.handle);

					for (D3DSAMPLERSTATETYPE state = D3DSAMP_ADDRESSU; state <= D3DSAMP_MAXANISOTROPY; state = static_cast<D3DSAMPLERSTATETYPE>(state + 1))
						_orig->SetSamplerState(first + i, state, sampler_instance->state[state]);
				}
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
void reshade::d3d9::device_impl::bind_descriptor_sets(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto set_impl = reinterpret_cast<const descriptor_set_impl *>(sets[i].handle);

		push_descriptors(
			stages,
			layout,
			first + i,
			api::descriptor_set_update(0, set_impl->count, set_impl->type, set_impl->descriptors.data()));
	}
}

void reshade::d3d9::device_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	assert(offset == 0);

#ifndef NDEBUG
	if (buffer.handle != 0)
	{
		assert(index_size == 2 || index_size == 4);

		D3DINDEXBUFFER_DESC desc;
		reinterpret_cast<IDirect3DIndexBuffer9 *>(buffer.handle)->GetDesc(&desc);
		assert(desc.Format == (index_size == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32));
	}
#else
	UNREFERENCED_PARAMETER(offset);
	UNREFERENCED_PARAMETER(index_size);
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

void reshade::d3d9::device_impl::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	assert(instance_count == 1 && first_instance == 0);

	_orig->DrawPrimitive(_current_prim_type, first_vertex, calc_prim_from_vertex_count(_current_prim_type, vertex_count));
}
void reshade::d3d9::device_impl::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	assert(instance_count == 1 && first_instance == 0);

	_orig->DrawIndexedPrimitive(_current_prim_type, vertex_offset, 0, 0xFFFF, first_index, calc_prim_from_vertex_count(_current_prim_type, index_count));
}
void reshade::d3d9::device_impl::dispatch(uint32_t, uint32_t, uint32_t)
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
	assert(src.handle != 0 && dst.handle != 0);

	const auto src_object = reinterpret_cast<IDirect3DResource9 *>(src.handle);
	const auto dst_object = reinterpret_cast<IDirect3DResource9 *>(dst.handle);

	RECT src_rect, dst_rect;
	bool src_is_regular_texture = false;
	bool dst_is_regular_texture = false;

	// Default to no filtering if no stretching needs to be performed (prevents artifacts when copying depth data)
	D3DTEXTUREFILTERTYPE stretch_filter_type = D3DTEXF_POINT;
	if ((src_box != nullptr || dst_box != nullptr) && (filter == api::filter_mode::min_mag_mip_linear || filter == api::filter_mode::min_mag_linear_mip_point))
		stretch_filter_type = D3DTEXF_LINEAR;

	switch (src_object->GetType() | (dst_object->GetType() << 4))
	{
		case D3DRTYPE_SURFACE | (D3DRTYPE_SURFACE << 4):
		{
			assert(src_subresource == 0 && dst_subresource == 0);

			com_ptr<IDirect3DSurface9> src_surface = static_cast<IDirect3DSurface9 *>(src_object);
			com_ptr<IDirect3DSurface9> dst_surface = static_cast<IDirect3DSurface9 *>(dst_object);

			D3DSURFACE_DESC src_desc;
			src_surface->GetDesc(&src_desc);
			D3DSURFACE_DESC dst_desc;
			dst_surface->GetDesc(&dst_desc);

			if (src_desc.Pool == D3DPOOL_DEFAULT && dst_desc.Pool == D3DPOOL_SYSTEMMEM)
			{
				assert(src_box == nullptr && dst_box == nullptr);
				assert((src_desc.Usage & D3DUSAGE_RENDERTARGET) != 0);
				_orig->GetRenderTargetData(src_surface.get(), dst_surface.get());
				return;
			}
			if (src_desc.Pool == D3DPOOL_SYSTEMMEM && dst_desc.Pool == D3DPOOL_DEFAULT)
			{
				_orig->UpdateSurface(
					src_surface.get(), convert_box_to_rect(src_box, src_rect),
					dst_surface.get(), reinterpret_cast<const POINT *>(convert_box_to_rect(dst_box, dst_rect)));
				return;
			}

			assert(src_desc.Pool == D3DPOOL_DEFAULT && dst_desc.Pool == D3DPOOL_DEFAULT);

			_orig->StretchRect(
				src_surface.get(), convert_box_to_rect(src_box, src_rect),
				dst_surface.get(), convert_box_to_rect(dst_box, dst_rect), stretch_filter_type);
			return;
		}
		case D3DRTYPE_SURFACE | (D3DRTYPE_TEXTURE << 4):
		{
			dst_is_regular_texture = true;
			[[fallthrough]];
		}
		case D3DRTYPE_SURFACE | (D3DRTYPE_CUBETEXTURE << 4):
		{
			assert(src_subresource == 0);

			const DWORD dst_level_count = static_cast<IDirect3DBaseTexture9 *>(dst_object)->GetLevelCount();
			const DWORD dst_level = dst_subresource % dst_level_count;

			com_ptr<IDirect3DSurface9> src_surface = static_cast<IDirect3DSurface9 *>(src_object);

			com_ptr<IDirect3DSurface9> dst_surface;
			if (FAILED(dst_is_regular_texture ?
					static_cast<IDirect3DTexture9 *>(dst_object)->GetSurfaceLevel(dst_level, &dst_surface) :
					static_cast<IDirect3DCubeTexture9 *>(dst_object)->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(dst_subresource / dst_level_count), dst_level, &dst_surface)))
				break;

			D3DSURFACE_DESC src_desc;
			src_surface->GetDesc(&src_desc);
			D3DSURFACE_DESC dst_desc;
			dst_surface->GetDesc(&dst_desc);

			if (src_desc.Pool == D3DPOOL_DEFAULT && dst_desc.Pool == D3DPOOL_SYSTEMMEM)
			{
				assert(src_box == nullptr && dst_box == nullptr);
				assert((src_desc.Usage & D3DUSAGE_RENDERTARGET) != 0);
				_orig->GetRenderTargetData(src_surface.get(), dst_surface.get());
				return;
			}
			if (src_desc.Pool == D3DPOOL_SYSTEMMEM && dst_desc.Pool == D3DPOOL_DEFAULT)
			{
				_orig->UpdateSurface(
					src_surface.get(), convert_box_to_rect(src_box, src_rect),
					dst_surface.get(), reinterpret_cast<const POINT *>(convert_box_to_rect(dst_box, dst_rect)));
				return;
			}

			assert(src_desc.Pool == D3DPOOL_DEFAULT && dst_desc.Pool == D3DPOOL_DEFAULT);

			_orig->StretchRect(
				src_surface.get(), convert_box_to_rect(src_box, src_rect),
				dst_surface.get(), convert_box_to_rect(dst_box, dst_rect), stretch_filter_type);
			return;
		}
		case D3DRTYPE_TEXTURE | (D3DRTYPE_TEXTURE << 4):
		{
			src_is_regular_texture = true;
			[[fallthrough]];
		}
		case D3DRTYPE_CUBETEXTURE | (D3DRTYPE_TEXTURE << 4):
		{
			const DWORD src_level_count = static_cast<IDirect3DBaseTexture9 *>(src_object)->GetLevelCount();
			const DWORD src_level = src_subresource % src_level_count;

			com_ptr<IDirect3DSurface9> src_surface;
			if (FAILED(src_is_regular_texture ?
					static_cast<IDirect3DTexture9 *>(src_object)->GetSurfaceLevel(src_level, &src_surface) :
					static_cast<IDirect3DCubeTexture9 *>(src_object)->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(src_subresource / src_level_count), src_level, &src_surface)))
				break;

			com_ptr<IDirect3DSurface9> dst_surface;
			if (FAILED(static_cast<IDirect3DTexture9 *>(dst_object)->GetSurfaceLevel(dst_subresource, &dst_surface)))
				break;

			D3DSURFACE_DESC src_desc;
			src_surface->GetDesc(&src_desc);
			D3DSURFACE_DESC dst_desc;
			dst_surface->GetDesc(&dst_desc);

			com_ptr<IDirect3DSurface9> target_surface = dst_surface;

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
			else if (src_desc.Pool == D3DPOOL_SYSTEMMEM && dst_desc.Pool == D3DPOOL_DEFAULT)
			{
				_orig->UpdateSurface(
					src_surface.get(), convert_box_to_rect(src_box, src_rect),
					dst_surface.get(), reinterpret_cast<const POINT *>(convert_box_to_rect(dst_box, dst_rect)));
				return;
			}

			assert((src_desc.Pool == D3DPOOL_DEFAULT || src_desc.Pool == D3DPOOL_MANAGED) && dst_desc.Pool == D3DPOOL_DEFAULT);

			// Capture and restore state, render targets, depth stencil surface and viewport (which all may change next)
			_backup_state.capture();

			// For some reason rendering below water acts up in Source Engine games if the active clip plane is not cleared to zero before executing any draw calls ...
			// Also copying with a fullscreen triangle rather than a quad of two triangles solves some artifacts that otherwise occur in the second triangle there as well. Not sure what is going on ...
			const float zero_clip_plane[4] = { 0, 0, 0, 0 };
			_orig->SetClipPlane(0, zero_clip_plane);

			// Perform copy using rasterization pipeline
			_copy_state->Apply();

			_orig->SetTexture(0, static_cast<IDirect3DBaseTexture9 *>(src_object));
			_orig->SetSamplerState(0, D3DSAMP_MINFILTER, stretch_filter_type);
			_orig->SetSamplerState(0, D3DSAMP_MAGFILTER, stretch_filter_type);
			_orig->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, src_level);

			assert((dst_desc.Usage & D3DUSAGE_RENDERTARGET) != 0);

			_orig->SetRenderTarget(0, target_surface.get());
			for (DWORD target = 1; target < _caps.NumSimultaneousRTs; ++target)
				_orig->SetRenderTarget(target, nullptr);
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
			}

			if (src_is_regular_texture)
			{
				_orig->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);

				float vertices[3][5] = {
					// x      y      z      tu     tv
					{ -1.0f,  1.0f,  0.0f,  0.0f,  0.0f },
					{  3.0f,  1.0f,  0.0f,  2.0f,  0.0f },
					{ -1.0f, -3.0f,  0.0f,  0.0f,  2.0f },
				};

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
		case D3DRTYPE_TEXTURE | (D3DRTYPE_SURFACE << 4):
		{
			src_is_regular_texture = true;
			[[fallthrough]];
		}
		case D3DRTYPE_CUBETEXTURE | (D3DRTYPE_SURFACE << 4) :
		{
			assert(dst_subresource == 0);

			const DWORD src_level_count = static_cast<IDirect3DBaseTexture9 *>(src_object)->GetLevelCount();
			const DWORD src_level = src_subresource % src_level_count;

			com_ptr<IDirect3DSurface9> src_surface;
			if (FAILED(src_is_regular_texture ?
					static_cast<IDirect3DTexture9 *>(src_object)->GetSurfaceLevel(src_level, &src_surface) :
					static_cast<IDirect3DCubeTexture9 *>(src_object)->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(src_subresource / src_level_count), src_level, &src_surface)))
				break;

			com_ptr<IDirect3DSurface9> dst_surface = static_cast<IDirect3DSurface9 *>(dst_object);

			D3DSURFACE_DESC src_desc;
			src_surface->GetDesc(&src_desc);
			D3DSURFACE_DESC dst_desc;
			dst_surface->GetDesc(&dst_desc);

			com_ptr<IDirect3DSurface9> target_surface = dst_surface;

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
			else if (src_desc.Pool == D3DPOOL_SYSTEMMEM && dst_desc.Pool == D3DPOOL_DEFAULT)
			{
				_orig->UpdateSurface(
					src_surface.get(), convert_box_to_rect(src_box, src_rect),
					dst_surface.get(), reinterpret_cast<const POINT *>(convert_box_to_rect(dst_box, dst_rect)));
				return;
			}

			assert(src_desc.Pool == D3DPOOL_DEFAULT && dst_desc.Pool == D3DPOOL_DEFAULT);

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
	}

	assert(false); // Not implemented
}
void reshade::d3d9::device_impl::copy_texture_to_buffer(api::resource, uint32_t, const api::subresource_box *, api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d9::device_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const api::rect *src_rect, api::resource dst, uint32_t dst_subresource, int32_t dst_x, int32_t dst_y, api::format)
{
	api::subresource_box src_box;
	src_box.front = 0;
	src_box.back = 1;
	api::subresource_box dst_box;
	dst_box.front = 0;
	dst_box.back = 1;

	if (src_rect != nullptr)
	{
		src_box.left = src_rect->left;
		src_box.top = src_rect->top;
		src_box.right = src_rect->right;
		src_box.bottom = src_rect->bottom;

		dst_box.left = dst_x;
		dst_box.top = dst_y;
		dst_box.right = dst_x + src_box.right - src_box.left;
		dst_box.bottom = dst_y + src_box.bottom - src_box.top;
	}
	else
	{
		const api::resource_desc desc = get_resource_desc(dst);

		src_box.left = 0;
		src_box.top = 0;
		src_box.right = std::max(1u, desc.texture.width >> dst_subresource);
		src_box.bottom = std::max(1u, desc.texture.height >> dst_subresource);

		dst_box.left = dst_x;
		dst_box.top = dst_y;
		dst_box.right = dst_x + src_box.right;
		dst_box.bottom = dst_y + src_box.bottom;
	}

	copy_texture_region(src, src_subresource, &src_box, dst, dst_subresource, &dst_box, api::filter_mode::min_mag_mip_point);
}

void reshade::d3d9::device_impl::clear_depth_stencil_view(api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const api::rect *)
{
	assert(dsv.handle != 0 && rect_count == 0); // Clearing rectangles is not supported

	_backup_state.capture();

	_orig->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9 *>(dsv.handle));

	_orig->Clear(
		0, nullptr,
		(depth != nullptr ? D3DCLEAR_ZBUFFER : 0) | (stencil != nullptr ? D3DCLEAR_STENCIL : 0), 0, depth != nullptr ? *depth : 0.0f, stencil != nullptr ? *stencil : 0);

	_backup_state.apply_and_release();
}
void reshade::d3d9::device_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t rect_count, const api::rect *rects)
{
	assert(rtv.handle != 0 && color != nullptr);

#if 1
	for (uint32_t i = 0; i < std::max(rect_count, 1u); ++i)
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
	assert(srv.handle != 0);

	const auto texture = reinterpret_cast<IDirect3DBaseTexture9 *>(srv.handle & ~1ull);

	texture->SetAutoGenFilterType(D3DTEXF_LINEAR);
	texture->GenerateMipSubLevels();
}

void reshade::d3d9::device_impl::begin_query(api::query_pool pool, api::query_type, uint32_t index)
{
	assert(pool.handle != 0);

	reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index]->Issue(D3DISSUE_BEGIN);
}
void reshade::d3d9::device_impl::end_query(api::query_pool pool, api::query_type, uint32_t index)
{
	assert(pool.handle != 0);

	reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index]->Issue(D3DISSUE_END);
}
void reshade::d3d9::device_impl::copy_query_pool_results(api::query_pool, api::query_type, uint32_t, uint32_t, api::resource, uint64_t, uint32_t)
{
	assert(false);
}

void reshade::d3d9::device_impl::begin_debug_event(const char *label, const float color[4])
{
	const size_t label_len = strlen(label);
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
	const size_t label_len = strlen(label);
	std::wstring label_wide;
	label_wide.reserve(label_len + 1);
	utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

	D3DPERF_SetMarker(color != nullptr ? D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]) : 0, label_wide.c_str());
}
