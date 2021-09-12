/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d9_impl_device.hpp"
#include "d3d9_impl_type_convert.hpp"
#include <algorithm>
#include <utf8/unchecked.h>

void reshade::d3d9::device_impl::begin_render_pass(api::render_pass, api::framebuffer fbo)
{
	assert(fbo.handle != 0);
	const auto fbo_impl = reinterpret_cast<const framebuffer_impl *>(fbo.handle);

	for (DWORD i = 0; i < _caps.NumSimultaneousRTs; ++i)
		_orig->SetRenderTarget(i, fbo_impl->rtv[i]);

	_orig->SetRenderState(D3DRS_SRGBWRITEENABLE, fbo_impl->srgb_write_enable);

	_orig->SetDepthStencilSurface(fbo_impl->dsv);
}
void reshade::d3d9::device_impl::finish_render_pass()
{
}
void reshade::d3d9::device_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	assert(count <= _caps.NumSimultaneousRTs);

	for (DWORD i = 0; i < count; ++i)
		_orig->SetRenderTarget(i, reinterpret_cast<IDirect3DSurface9 *>(rtvs[i].handle));
	for (DWORD i = count; i < _caps.NumSimultaneousRTs; ++i)
		_orig->SetRenderTarget(i, nullptr);

	_orig->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9 *>(dsv.handle));
}

void reshade::d3d9::device_impl::bind_pipeline(api::pipeline_stage type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);

	switch (type)
	{
	case api::pipeline_stage::all_graphics:
		reinterpret_cast<pipeline_impl *>(pipeline.handle)->state_block->Apply();
		_current_prim_type = reinterpret_cast<pipeline_impl *>(pipeline.handle)->prim_type;
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
			_current_prim_type = static_cast<D3DPRIMITIVETYPE>(values[i]);
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
void reshade::d3d9::device_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	if (count == 0)
		return;

	assert(first == 0 && count == 1 && viewports != nullptr);

	D3DVIEWPORT9 d3d_viewport;
	d3d_viewport.X = static_cast<DWORD>(viewports[0]);
	d3d_viewport.Y = static_cast<DWORD>(viewports[1]);
	d3d_viewport.Width = static_cast<DWORD>(viewports[2]);
	d3d_viewport.Height = static_cast<DWORD>(viewports[3]);
	d3d_viewport.MinZ = viewports[4];
	d3d_viewport.MaxZ = viewports[5];

	_orig->SetViewport(&d3d_viewport);
}
void reshade::d3d9::device_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	if (count == 0)
		return;

	assert(first == 0 && count == 1 && rects != nullptr);

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
	if (layout.handle != 0)
		first += reinterpret_cast<pipeline_layout_impl *>(layout.handle)->shader_registers[layout_param];

	// Set for each individual shader stage (pixel stage first, since vertex stage modifies the the binding offset)
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
					const auto sampler_impl = reinterpret_cast<const struct sampler_impl *>(descriptor.handle);

					for (D3DSAMPLERSTATETYPE state = D3DSAMP_ADDRESSU; state <= D3DSAMP_MAXANISOTROPY; state = static_cast<D3DSAMPLERSTATETYPE>(state + 1))
						_orig->SetSamplerState(first + i, state, sampler_impl->state[state]);
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
					const auto sampler_impl = reinterpret_cast<const struct sampler_impl *>(descriptor.sampler.handle);

					for (D3DSAMPLERSTATETYPE state = D3DSAMP_ADDRESSU; state <= D3DSAMP_MAXANISOTROPY; state = static_cast<D3DSAMPLERSTATETYPE>(state + 1))
						_orig->SetSamplerState(first + i, state, sampler_impl->state[state]);
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
	assert(sets != nullptr);

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
		copy_buffer_region(src, 0, dst, 0, std::numeric_limits<uint64_t>::max());
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
void reshade::d3d9::device_impl::copy_buffer_to_texture(api::resource, uint64_t, uint32_t, uint32_t, api::resource, uint32_t, const int32_t[6])
{
	assert(false);
}
void reshade::d3d9::device_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[3], api::filter_mode filter)
{
	assert(src.handle != 0 && dst.handle != 0);
	const auto src_object = reinterpret_cast<IDirect3DResource9 *>(src.handle);
	const auto dst_object = reinterpret_cast<IDirect3DResource9 *>(dst.handle);

	RECT src_rect = {};
	if (src_box != nullptr)
	{
		src_rect.left = src_box[0];
		src_rect.top = src_box[1];
		assert(src_box[2] == 0);
		src_rect.right = src_box[3];
		src_rect.bottom = src_box[4];
		assert(src_box[5] == 1);
	}

	RECT dst_rect = {};
	if (dst_box != nullptr)
	{
		dst_rect.left = dst_box[0];
		dst_rect.top = dst_box[1];
		assert(dst_box[2] == 0);
		dst_rect.right = dst_box[3];
		dst_rect.bottom = dst_box[4];
		assert(dst_box[5] == 1);
	}

	D3DTEXTUREFILTERTYPE stretch_filter_type = D3DTEXF_NONE;
	switch (filter)
	{
	case api::filter_mode::min_mag_mip_point:
	case api::filter_mode::min_mag_point_mip_linear:
		// Default to no filtering if no stretching needs to be performed (prevents artifacts when copying depth data)
		if (src_box != nullptr || dst_box != nullptr)
			stretch_filter_type = D3DTEXF_POINT;
		break;
	case api::filter_mode::min_mag_mip_linear:
	case api::filter_mode::min_mag_linear_mip_point:
			stretch_filter_type = D3DTEXF_LINEAR;
		break;
	}

	switch (src_object->GetType() | (dst_object->GetType() << 4))
	{
		case D3DRTYPE_SURFACE | (D3DRTYPE_SURFACE << 4):
		{
			assert(src_subresource == 0 && dst_subresource == 0);

			if (src_box == nullptr && dst_box == nullptr)
			{
				D3DSURFACE_DESC src_desc;
				static_cast<IDirect3DSurface9 *>(src_object)->GetDesc(&src_desc);
				D3DSURFACE_DESC dst_desc;
				static_cast<IDirect3DSurface9 *>(dst_object)->GetDesc(&dst_desc);

				if (src_desc.Pool == D3DPOOL_DEFAULT && dst_desc.Pool == D3DPOOL_SYSTEMMEM)
				{
					_orig->GetRenderTargetData(static_cast<IDirect3DSurface9 *>(src_object), static_cast<IDirect3DSurface9 *>(dst_object));
					return;
				}
			}

			_orig->StretchRect(
				static_cast<IDirect3DSurface9 *>(src_object), src_box != nullptr ? &src_rect : nullptr,
				static_cast<IDirect3DSurface9 *>(dst_object), dst_box != nullptr ? &dst_rect : nullptr, stretch_filter_type);
			return;
		}
		case D3DRTYPE_SURFACE | (D3DRTYPE_TEXTURE << 4):
		{
			assert(src_subresource == 0);

			com_ptr<IDirect3DSurface9> dst_surface;
			static_cast<IDirect3DTexture9 *>(dst_object)->GetSurfaceLevel(dst_subresource, &dst_surface);

			if (src_box == nullptr && dst_box == nullptr)
			{
				D3DSURFACE_DESC src_desc;
				static_cast<IDirect3DSurface9 *>(src_object)->GetDesc(&src_desc);
				D3DSURFACE_DESC dst_desc;
				dst_surface->GetDesc(&dst_desc);

				if (src_desc.Pool == D3DPOOL_DEFAULT && dst_desc.Pool == D3DPOOL_SYSTEMMEM)
				{
					_orig->GetRenderTargetData(static_cast<IDirect3DSurface9 *>(src_object), dst_surface.get());
					return;
				}
			}

			_orig->StretchRect(
				static_cast<IDirect3DSurface9 *>(src_object), src_box != nullptr ? &src_rect : nullptr,
				dst_surface.get(), dst_box != nullptr ? &dst_rect : nullptr, stretch_filter_type);
			return;
		}
		case D3DRTYPE_TEXTURE | (D3DRTYPE_TEXTURE << 4):
		{
			com_ptr<IDirect3DSurface9> dst_surface;
			static_cast<IDirect3DTexture9 *>(dst_object)->GetSurfaceLevel(dst_subresource, &dst_surface);

			if (src_box == nullptr && dst_box == nullptr)
			{
				D3DSURFACE_DESC src_desc;
				static_cast<IDirect3DTexture9 *>(src_object)->GetLevelDesc(src_subresource, &src_desc);
				D3DSURFACE_DESC dst_desc;
				dst_surface->GetDesc(&dst_desc);

				if (src_desc.Pool == D3DPOOL_DEFAULT && dst_desc.Pool == D3DPOOL_SYSTEMMEM)
				{
					com_ptr<IDirect3DSurface9> src_surface;
					static_cast<IDirect3DTexture9 *>(src_object)->GetSurfaceLevel(src_subresource, &src_surface);

					_orig->GetRenderTargetData(src_surface.get(), dst_surface.get());
					return;
				}
			}

			// Capture and restore state, render targets, depth stencil surface and viewport (which all may change next)
			_backup_state.capture();

			// For some reason rendering below water acts up in Source Engine games if the active clip plane is not cleared to zero before executing any draw calls ...
			// Also copying with a fullscreen triangle rather than a quad of two triangles solves some artifacts that otherwise occur in the second triangle there as well. Not sure what is going on ...
			const float zero_clip_plane[4] = { 0, 0, 0, 0 };
			_orig->SetClipPlane(0, zero_clip_plane);

			// Perform copy using rasterization pipeline
			_copy_state->Apply();

			_orig->SetTexture(0, static_cast<IDirect3DTexture9 *>(src_object));
			_orig->SetSamplerState(0, D3DSAMP_MINFILTER, stretch_filter_type);
			_orig->SetSamplerState(0, D3DSAMP_MAGFILTER, stretch_filter_type);

			_orig->SetRenderTarget(0, dst_surface.get());
			for (DWORD target = 1; target < _caps.NumSimultaneousRTs; ++target)
				_orig->SetRenderTarget(target, nullptr);
			_orig->SetDepthStencilSurface(nullptr);

			if (dst_box != nullptr)
			{
				D3DVIEWPORT9 viewport;
				viewport.X = dst_rect.left;
				viewport.Y = dst_rect.top;
				viewport.Width = dst_rect.right - dst_rect.left;
				viewport.Height = dst_rect.bottom - dst_rect.top;
				viewport.MinZ = 0.0f;
				viewport.MaxZ = 1.0f;

				_orig->SetViewport(&viewport);
			}

			float vertices[3][5] = {
				// x      y      z      tu     tv
				{ -1.0f,  1.0f,  0.0f,  0.0f,  0.0f },
				{  3.0f,  1.0f,  0.0f,  2.0f,  0.0f },
				{ -1.0f, -3.0f,  0.0f,  0.0f,  2.0f },
			};

			if (src_box != nullptr)
			{
				D3DSURFACE_DESC desc;
				dst_surface->GetDesc(&desc);

				vertices[0][3] = src_rect.left * 2.0f / desc.Width;
				vertices[0][4] = src_rect.top * 2.0f / desc.Height;
				vertices[1][3] = src_rect.left * 2.0f / desc.Width;
				vertices[1][4] = src_rect.bottom * 2.0f / desc.Height;
				vertices[2][3] = src_rect.right * 2.0f / desc.Width;
				vertices[2][4] = src_rect.top * 2.0f / desc.Height;
			}

			_orig->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, vertices, sizeof(vertices[0]));

			_backup_state.apply_and_release();
			return;
		}
		case D3DRTYPE_TEXTURE | (D3DRTYPE_SURFACE << 4):
		{
			assert(dst_subresource == 0);

			com_ptr<IDirect3DSurface9> src_surface;
			static_cast<IDirect3DTexture9 *>(src_object)->GetSurfaceLevel(src_subresource, &src_surface);

			if (src_box == nullptr && dst_box == nullptr)
			{
				D3DSURFACE_DESC src_desc;
				src_surface->GetDesc(&src_desc);
				D3DSURFACE_DESC dst_desc;
				static_cast<IDirect3DSurface9 *>(dst_object)->GetDesc(&dst_desc);

				if (src_desc.Pool == D3DPOOL_DEFAULT && dst_desc.Pool == D3DPOOL_SYSTEMMEM)
				{
					_orig->GetRenderTargetData(src_surface.get(), static_cast<IDirect3DSurface9 *>(dst_object));
					return;
				}
			}

			_orig->StretchRect(
				src_surface.get(), src_box != nullptr ? &src_rect : nullptr,
				static_cast<IDirect3DSurface9 *>(dst_object), dst_box != nullptr ? &dst_rect : nullptr, stretch_filter_type);
			return;
		}
	}

	assert(false); // Not implemented
}
void reshade::d3d9::device_impl::copy_texture_to_buffer(api::resource, uint32_t, const int32_t[6], api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d9::device_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], api::format)
{
	int32_t dst_box[6] = {};
	if (dst_offset != nullptr)
		std::copy_n(dst_offset, 3, dst_box);

	if (src_box != nullptr)
	{
		dst_box[3] = dst_box[0] + src_box[3] - src_box[0];
		dst_box[4] = dst_box[1] + src_box[4] - src_box[1];
		dst_box[5] = dst_box[2] + src_box[5] - src_box[2];
	}
	else
	{
		const api::resource_desc desc = get_resource_desc(dst);
		dst_box[3] = dst_box[0] + std::max(1u, desc.texture.width >> dst_subresource);
		dst_box[4] = dst_box[1] + std::max(1u, desc.texture.height >> dst_subresource);
		dst_box[5] = dst_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.texture.depth_or_layers) >> dst_subresource) : 1u);
	}

	copy_texture_region(src, src_subresource, src_box, dst, dst_subresource, dst_box, api::filter_mode::min_mag_mip_point);
}

void reshade::d3d9::device_impl::clear_attachments(api::attachment_type clear_flags, const float color[4], float depth, uint8_t stencil, uint32_t rect_count, const int32_t *rects)
{
	_orig->Clear(rect_count, reinterpret_cast<const D3DRECT *>(rects), static_cast<DWORD>(clear_flags), color != nullptr ? D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]) : 0, depth, stencil);
}
void reshade::d3d9::device_impl::clear_depth_stencil_view(api::resource_view dsv, api::attachment_type clear_flags, float depth, uint8_t stencil, uint32_t rect_count, const int32_t *)
{
	assert(dsv.handle != 0 && rect_count == 0);

	_backup_state.capture();

	_orig->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9 *>(dsv.handle));

	_orig->Clear(0, nullptr, static_cast<DWORD>(clear_flags), 0, depth, stencil);

	_backup_state.apply_and_release();
}
void reshade::d3d9::device_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t rect_count, const int32_t *rects)
{
	assert(rtv.handle != 0 && color != nullptr);

	for (uint32_t i = 0; i < std::max(rect_count, 1u); ++i)
		_orig->ColorFill(reinterpret_cast<IDirect3DSurface9 *>(rtv.handle & ~1ull), reinterpret_cast<const RECT *>(rects + i * 4), D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]));
}
void reshade::d3d9::device_impl::clear_unordered_access_view_uint(api::resource_view, const uint32_t[4], uint32_t, const int32_t *)
{
	assert(false);
}
void reshade::d3d9::device_impl::clear_unordered_access_view_float(api::resource_view, const float[4], uint32_t, const int32_t *)
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
void reshade::d3d9::device_impl::finish_query(api::query_pool pool, api::query_type, uint32_t index)
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
void reshade::d3d9::device_impl::finish_debug_event()
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
