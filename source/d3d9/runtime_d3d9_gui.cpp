/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_GUI

#include "dll_log.hpp"
#include "runtime_d3d9.hpp"
#include "runtime_d3d9_objects.hpp"
#include <imgui.h>

bool reshade::d3d9::runtime_d3d9::init_imgui_resources()
{
	HRESULT hr = _device->BeginStateBlock();
	if (SUCCEEDED(hr))
	{
		_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
		_device->SetPixelShader(nullptr);
		_device->SetVertexShader(nullptr);
		_device->SetRenderState(D3DRS_ZENABLE, false);
		_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		_device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
		_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		_device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
		_device->SetRenderState(D3DRS_FOGENABLE, false);
		_device->SetRenderState(D3DRS_STENCILENABLE, false);
		_device->SetRenderState(D3DRS_CLIPPING, false);
		_device->SetRenderState(D3DRS_LIGHTING, false);
		_device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
		_device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);
		_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		_device->SetRenderState(D3DRS_SRGBWRITEENABLE, false);
		_device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
		_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		_device->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
		_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		_device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		_device->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, 0);
		_device->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0);
		_device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);

		hr = _device->EndStateBlock(&_imgui.state);
	}

	if (FAILED(hr))
	{
		LOG(ERROR) << "Failed to create state block! HRESULT is " << hr << '.';
		return false;
	}

	return true;
}

void reshade::d3d9::runtime_d3d9::render_imgui_draw_data(ImDrawData *draw_data)
{
	// Fixed-function vertex layout
	struct ImDrawVert9
	{
		float x, y, z;
		D3DCOLOR col;
		float u, v;
	};

	// Create and grow vertex/index buffers if needed
	if (_imgui.num_indices < draw_data->TotalIdxCount)
	{
		_imgui.indices.reset();
		_imgui.num_indices = draw_data->TotalIdxCount + 10000;

		if (FAILED(_device->CreateIndexBuffer(_imgui.num_indices * sizeof(ImDrawIdx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &_imgui.indices, nullptr)))
			return;
	}
	if (_imgui.num_vertices < draw_data->TotalVtxCount)
	{
		_imgui.vertices.reset();
		_imgui.num_vertices = draw_data->TotalVtxCount + 5000;

		if (FAILED(_device->CreateVertexBuffer(_imgui.num_vertices * sizeof(ImDrawVert9), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, D3DPOOL_DEFAULT, &_imgui.vertices, nullptr)))
			return;
	}

	if (ImDrawIdx *idx_dst;
		SUCCEEDED(_imgui.indices->Lock(0, draw_data->TotalIdxCount * sizeof(ImDrawIdx), reinterpret_cast<void **>(&idx_dst), D3DLOCK_DISCARD)))
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.size() * sizeof(ImDrawIdx));
			idx_dst += draw_list->IdxBuffer.size();
		}

		_imgui.indices->Unlock();
	}
	if (ImDrawVert9 *vtx_dst;
		SUCCEEDED(_imgui.vertices->Lock(0, draw_data->TotalVtxCount * sizeof(ImDrawVert9), reinterpret_cast<void **>(&vtx_dst), D3DLOCK_DISCARD)))
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];

			for (const ImDrawVert &vtx : draw_list->VtxBuffer)
			{
				vtx_dst->x = vtx.pos.x;
				vtx_dst->y = vtx.pos.y;
				vtx_dst->z = 0.0f;

				// RGBA --> ARGB
				vtx_dst->col = (vtx.col & 0xFF00FF00) | ((vtx.col & 0xFF0000) >> 16) | ((vtx.col & 0xFF) << 16);

				vtx_dst->u = vtx.uv.x;
				vtx_dst->v = vtx.uv.y;

				++vtx_dst; // Next vertex
			}
		}

		_imgui.vertices->Unlock();
	}

	// Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
	_imgui.state->Apply();
	_device->SetIndices(_imgui.indices.get());
	_device->SetStreamSource(0, _imgui.vertices.get(), 0, sizeof(ImDrawVert9));
	_device->SetRenderTarget(0, _backbuffer_resolved.get());

	// Clear unused bindings
	for (unsigned int i = 0; i < _device_impl->_num_samplers; i++)
		_device->SetTexture(i, nullptr);
	for (unsigned int i = 1; i < _device_impl->_num_simultaneous_rendertargets; i++)
		_device->SetRenderTarget(i, nullptr);
	_device->SetDepthStencilSurface(nullptr);

	const D3DMATRIX identity_mat = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	const D3DMATRIX ortho_projection = {
		2.0f / draw_data->DisplaySize.x, 0.0f,   0.0f, 0.0f,
		0.0f, -2.0f / draw_data->DisplaySize.y,  0.0f, 0.0f,
		0.0f,                            0.0f,   0.5f, 0.0f,
		-(2 * draw_data->DisplayPos.x + draw_data->DisplaySize.x + 1.0f) / draw_data->DisplaySize.x, // Bake half-pixel offset into projection matrix
		+(2 * draw_data->DisplayPos.y + draw_data->DisplaySize.y + 1.0f) / draw_data->DisplaySize.y, 0.5f, 1.0f
	};

	_device->SetTransform(D3DTS_VIEW, &identity_mat);
	_device->SetTransform(D3DTS_WORLD, &identity_mat);
	_device->SetTransform(D3DTS_PROJECTION, &ortho_projection);

	UINT vtx_offset = 0, idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList *const draw_list = draw_data->CmdLists[n];

		for (const ImDrawCmd &cmd : draw_list->CmdBuffer)
		{
			assert(cmd.TextureId != 0);
			assert(cmd.UserCallback == nullptr);

			const RECT scissor_rect = {
				static_cast<LONG>(cmd.ClipRect.x - draw_data->DisplayPos.x),
				static_cast<LONG>(cmd.ClipRect.y - draw_data->DisplayPos.y),
				static_cast<LONG>(cmd.ClipRect.z - draw_data->DisplayPos.x),
				static_cast<LONG>(cmd.ClipRect.w - draw_data->DisplayPos.y)
			};
			_device->SetScissorRect(&scissor_rect);

			_device->SetTexture(0, static_cast<const tex_data *>(cmd.TextureId)->texture.get());

			_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, cmd.VtxOffset + vtx_offset, 0, draw_list->VtxBuffer.Size, cmd.IdxOffset + idx_offset, cmd.ElemCount / 3);

		}

		idx_offset += draw_list->IdxBuffer.Size;
		vtx_offset += draw_list->VtxBuffer.Size;
	}
}

#endif
