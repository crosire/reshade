/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_type_convert.hpp"
#include <algorithm>

void reshade::d3d9::device_impl::bind_pipeline(api::pipeline_type type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);

	switch (type)
	{
	case api::pipeline_type::graphics:
		reinterpret_cast<pipeline_impl *>(pipeline.handle)->state_block->Apply();
		_current_prim_type = reinterpret_cast<pipeline_impl *>(pipeline.handle)->prim_type;
		break;
	case api::pipeline_type::graphics_vertex_shader:
		_orig->SetVertexShader(reinterpret_cast<IDirect3DVertexShader9 *>(pipeline.handle));
		break;
	case api::pipeline_type::graphics_pixel_shader:
		_orig->SetPixelShader(reinterpret_cast<IDirect3DPixelShader9 *>(pipeline.handle));
		break;
	default:
		assert(false);
		break;
	}
}
void reshade::d3d9::device_impl::bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values)
{
	for (UINT i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::pipeline_state::front_face_ccw:
		case api::pipeline_state::depth_bias_clamp:
		case api::pipeline_state::alpha_to_coverage:
			assert(false);
			break;
		case api::pipeline_state::primitive_topology:
			_current_prim_type = convert_primitive_topology(static_cast<api::primitive_topology>(values[i]));
			break;
		default:
			_orig->SetRenderState(static_cast<D3DRENDERSTATETYPE>(states[i]), values[i]);
			break;
		}
	}
}
void reshade::d3d9::device_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	assert(first == 0 && count == 1);

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
	assert(first == 0 && count == 1);

	_orig->SetScissorRect(reinterpret_cast<const RECT *>(rects));
}

void reshade::d3d9::device_impl::push_constants(api::shader_stage stage, api::pipeline_layout, uint32_t, uint32_t first, uint32_t count, const void *values)
{
	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->SetVertexShaderConstantF(first / 4, static_cast<const float *>(values), count / 4);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->SetPixelShaderConstantF(first / 4, static_cast<const float *>(values), count / 4);
}
void reshade::d3d9::device_impl::push_descriptors(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
{
	if ((stage & (api::shader_stage::vertex | api::shader_stage::pixel)) == (api::shader_stage::vertex | api::shader_stage::pixel))
	{
		// Call for each individual shader stage
		push_descriptors(api::shader_stage::vertex, layout, layout_index, type, first, count, descriptors);
		push_descriptors( api::shader_stage::pixel, layout, layout_index, type, first, count, descriptors);
		return;
	}

	if (layout.handle != 0)
		first += reinterpret_cast<pipeline_layout_impl *>(layout.handle)->shader_registers[layout_index];

	switch (stage)
	{
	default:
		assert(false);
		return;
	case api::shader_stage::vertex:
		// See https://docs.microsoft.com/windows/win32/direct3d9/vertex-textures-in-vs-3-0
		first += D3DVERTEXTEXTURESAMPLER0;
		if ((first + count) > D3DVERTEXTEXTURESAMPLER3) // The vertex engine only contains four texture sampler stages
			count = D3DVERTEXTEXTURESAMPLER3 - first;
		break;
	case api::shader_stage::pixel:
		break;
	}

	switch (type)
	{
	case api::descriptor_type::sampler:
		for (UINT i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler *>(descriptors)[i];

			if (descriptor.handle != 0)
				for (D3DSAMPLERSTATETYPE state = D3DSAMP_ADDRESSU; state <= D3DSAMP_MAXANISOTROPY; state = static_cast<D3DSAMPLERSTATETYPE>(state + 1))
					_orig->SetSamplerState(i + first, state, reinterpret_cast<const DWORD *>(descriptor.handle)[state - 1]);
		}
		break;
	case api::descriptor_type::sampler_with_resource_view:
		for (UINT i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(descriptors)[i];
			_orig->SetTexture(i + first, reinterpret_cast<IDirect3DBaseTexture9 *>(descriptor.view.handle & ~1ull));
			_orig->SetSamplerState(i + first, D3DSAMP_SRGBTEXTURE, descriptor.view.handle & 1);

			if (descriptor.sampler.handle != 0)
				for (D3DSAMPLERSTATETYPE state = D3DSAMP_ADDRESSU; state <= D3DSAMP_MAXANISOTROPY; state = static_cast<D3DSAMPLERSTATETYPE>(state + 1))
					_orig->SetSamplerState(i + first, state, reinterpret_cast<const DWORD *>(descriptor.sampler.handle)[state - 1]);
		}
		break;
	case api::descriptor_type::shader_resource_view:
		for (UINT i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(descriptors)[i];
			_orig->SetTexture(i + first, reinterpret_cast<IDirect3DBaseTexture9 *>(descriptor.handle & ~1ull));
			_orig->SetSamplerState(i + first, D3DSAMP_SRGBTEXTURE, descriptor.handle & 1);
		}
		break;
	case api::descriptor_type::unordered_access_view:
	case api::descriptor_type::constant_buffer:
		assert(false);
		break;
	}
}
void reshade::d3d9::device_impl::bind_descriptor_sets(api::pipeline_type type, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)
{
	assert(type == api::pipeline_type::graphics);

	for (UINT i = 0; i < count; ++i)
	{
		const auto set_impl = reinterpret_cast<descriptor_set_impl *>(sets[i].handle);

		if (set_impl->type != api::descriptor_type::sampler_with_resource_view)
		{
			push_descriptors(
				api::shader_stage::all_graphics,
				layout,
				i + first,
				set_impl->type,
				0,
				static_cast<uint32_t>(set_impl->descriptors.size()),
				set_impl->descriptors.data());
		}
		else
		{
			push_descriptors(
				api::shader_stage::all_graphics,
				layout,
				i + first,
				set_impl->type,
				0,
				static_cast<uint32_t>(set_impl->sampler_with_resource_views.size()),
				set_impl->sampler_with_resource_views.data());
		}
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
	for (UINT i = 0; i < count; ++i)
	{
		assert(offsets == nullptr || offsets[i] <= std::numeric_limits<UINT>::max());

		_orig->SetStreamSource(i + first, reinterpret_cast<IDirect3DVertexBuffer9 *>(buffers[i].handle), offsets != nullptr ? static_cast<UINT>(offsets[i]) : 0, strides[i]);
	}
}

void reshade::d3d9::device_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	assert(instances == 1 && first_instance == 0);

	_orig->DrawPrimitive(_current_prim_type, first_vertex, calc_prim_from_vertex_count(_current_prim_type, vertices));
}
void reshade::d3d9::device_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	assert(instances == 1 && first_instance == 0);

	_orig->DrawIndexedPrimitive(_current_prim_type, vertex_offset, 0, 0xFFFF, first_index, calc_prim_from_vertex_count(_current_prim_type, indices));
}
void reshade::d3d9::device_impl::dispatch(uint32_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d9::device_impl::draw_or_dispatch_indirect(uint32_t, api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d9::device_impl::begin_render_pass(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	assert(count <= _caps.NumSimultaneousRTs);

	for (UINT i = 0; i < count; ++i)
		_orig->SetRenderTarget(i, reinterpret_cast<IDirect3DSurface9 *>(rtvs[i].handle & ~1ull));
	for (UINT i = count; i < _caps.NumSimultaneousRTs; ++i)
		_orig->SetRenderTarget(i, nullptr);

	if (count != 0)
		_orig->SetRenderState(D3DRS_SRGBWRITEENABLE, rtvs[0].handle & 1);

	_orig->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9 *>(dsv.handle));
}
void reshade::d3d9::device_impl::finish_render_pass()
{
}

void reshade::d3d9::device_impl::copy_resource(api::resource src, api::resource dst)
{
	const api::resource_desc desc = get_resource_desc(src);

	if (desc.type == api::resource_type::buffer)
	{
		copy_buffer_region(src, 0, dst, 0, ~0llu);
	}
	else
	{
		for (uint32_t layer = 0; layer < desc.texture.depth_or_layers; ++layer)
		{
			for (uint32_t level = 0; level < desc.texture.levels; ++level)
			{
				const uint32_t subresource = level + layer * desc.texture.levels;

				copy_texture_region(src, subresource, nullptr, dst, subresource, nullptr, api::texture_filter::min_mag_mip_point);
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
void reshade::d3d9::device_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[3], api::texture_filter filter)
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

	D3DTEXTUREFILTERTYPE stretch_filter = D3DTEXF_NONE;
	switch (filter)
	{
	case api::texture_filter::min_mag_mip_point:
	case api::texture_filter::min_mag_point_mip_linear:
		stretch_filter = D3DTEXF_POINT;
		break;
	case api::texture_filter::min_mag_mip_linear:
	case api::texture_filter::min_mag_linear_mip_point:
		stretch_filter = D3DTEXF_LINEAR;
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
				static_cast<IDirect3DSurface9 *>(dst_object), dst_box != nullptr ? &dst_rect : nullptr, stretch_filter);
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
				dst_surface.get(), dst_box != nullptr ? &dst_rect : nullptr, stretch_filter);
			return;
		}
		case D3DRTYPE_TEXTURE | (D3DRTYPE_TEXTURE << 4):
		{
			// Capture and restore state, render targets, depth stencil surface and viewport (which all may change next)
			_backup_state.capture();

			// For some reason rendering below water acts up in Source Engine games if the active clip plane is not cleared to zero before executing any draw calls ...
			// Also copying with a fullscreen triangle rather than a quad of two triangles solves some artifacts that otherwise occur in the second triangle there as well. Not sure what is going on ...
			const float zero_clip_plane[4] = { 0, 0, 0, 0 };
			_orig->SetClipPlane(0, zero_clip_plane);

			// Perform copy using rasterization pipeline
			_copy_state->Apply();

			_orig->SetTexture(0, static_cast<IDirect3DTexture9 *>(src_object));
			_orig->SetSamplerState(0, D3DSAMP_MINFILTER, stretch_filter);
			_orig->SetSamplerState(0, D3DSAMP_MAGFILTER, stretch_filter);

			com_ptr<IDirect3DSurface9> dst_surface;
			static_cast<IDirect3DTexture9 *>(dst_object)->GetSurfaceLevel(dst_subresource, &dst_surface);
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
				{ -1.0f, -3.0f,  0.0f,  0.0f,  2.0f },
				{  3.0f,  1.0f,  0.0f,  2.0f,  0.0f },
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
				static_cast<IDirect3DSurface9 *>(dst_object), dst_box != nullptr ? &dst_rect : nullptr, stretch_filter);
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

	copy_texture_region(src, src_subresource, src_box, dst, dst_subresource, dst_box, api::texture_filter::min_mag_mip_point);
}

void reshade::d3d9::device_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);
	const auto texture = reinterpret_cast<IDirect3DBaseTexture9 *>(srv.handle & ~1ull);

	texture->SetAutoGenFilterType(D3DTEXF_LINEAR);
	texture->GenerateMipSubLevels();
}

void reshade::d3d9::device_impl::clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	assert(dsv.handle != 0);

	_backup_state.capture();

	_orig->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9 *>(dsv.handle));

	_orig->Clear(
		0, nullptr,
		((clear_flags & 0x1) != 0 ? D3DCLEAR_ZBUFFER : 0) |
		((clear_flags & 0x2) != 0 ? D3DCLEAR_STENCIL : 0),
		0, depth, stencil);

	_backup_state.apply_and_release();
}
void reshade::d3d9::device_impl::clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4])
{
	assert(color != nullptr);
#if 0
	assert(count <= _caps.NumSimultaneousRTs);

	_backup_state.capture();

	for (DWORD target = 0; target < count && target < _caps.NumSimultaneousRTs; ++target)
		_orig->SetRenderTarget(target, reinterpret_cast<IDirect3DSurface9 *>(rtvs[target].handle & ~1ull));
	for (DWORD target = count; target < _caps.NumSimultaneousRTs; ++target)
		_orig->SetRenderTarget(target, nullptr);

	_orig->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]), 0.0f, 0);

	_backup_state.apply_and_release();
#else
	for (UINT i = 0; i < count; ++i)
	{
		assert(rtvs[i].handle != 0);

		_orig->ColorFill(reinterpret_cast<IDirect3DSurface9 *>(rtvs[i].handle & ~1ull), nullptr, D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]));
	}
#endif
}
void reshade::d3d9::device_impl::clear_unordered_access_view_uint(api::resource_view, const uint32_t[4])
{
	assert(false);
}
void reshade::d3d9::device_impl::clear_unordered_access_view_float(api::resource_view, const float[4])
{
	assert(false);
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
void reshade::d3d9::device_impl::copy_query_results(api::query_pool, api::query_type, uint32_t, uint32_t, api::resource, uint64_t, uint32_t)
{
	assert(false);
}

void reshade::d3d9::device_impl::add_debug_marker(const char *label, const float color[4])
{
	const size_t label_len = strlen(label);
	std::wstring label_wide;
	label_wide.reserve(label_len + 1);
	utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

	D3DPERF_SetMarker(color != nullptr ? D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]) : 0, label_wide.c_str());
}
void reshade::d3d9::device_impl::begin_debug_marker(const char *label, const float color[4])
{
	const size_t label_len = strlen(label);
	std::wstring label_wide;
	label_wide.reserve(label_len + 1);
	utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

	D3DPERF_BeginEvent(color != nullptr ? D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]) : 0, label_wide.c_str());
}
void reshade::d3d9::device_impl::finish_debug_marker()
{
	D3DPERF_EndEvent();
}
