/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_GUI

#include "dll_resources.hpp"
#include "runtime_d3d10.hpp"
#include "runtime_d3d10_objects.hpp"
#include <imgui.h>

bool reshade::d3d10::runtime_impl::init_imgui_resources()
{
	if (_imgui.vs == nullptr)
	{
		const resources::data_resource vs_res = resources::load_data_resource(IDR_IMGUI_VS);
		if (FAILED(_device->CreateVertexShader(vs_res.data, vs_res.data_size, &_imgui.vs)))
			return false;

		const D3D10_INPUT_ELEMENT_DESC input_layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(ImDrawVert, pos), D3D10_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(ImDrawVert, uv ), D3D10_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(ImDrawVert, col), D3D10_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(_device->CreateInputLayout(input_layout, ARRAYSIZE(input_layout), vs_res.data, vs_res.data_size, &_imgui.layout)))
			return false;
	}

	if (_imgui.ps == nullptr)
	{
		const resources::data_resource ps_res = resources::load_data_resource(IDR_IMGUI_PS);
		if (FAILED(_device->CreatePixelShader(ps_res.data, ps_res.data_size, &_imgui.ps)))
			return false;
	}

	if (_imgui.cb == nullptr)
	{
		D3D10_BUFFER_DESC desc = {};
		desc.ByteWidth = 16 * sizeof(float);
		desc.Usage = D3D10_USAGE_IMMUTABLE;
		desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;

		// Setup orthographic projection matrix
		const float ortho_projection[16] = {
			2.0f / _width, 0.0f,   0.0f, 0.0f,
			0.0f, -2.0f / _height, 0.0f, 0.0f,
			0.0f,          0.0f,   0.5f, 0.0f,
			-1.f,          1.0f,   0.5f, 1.0f
		};

		D3D10_SUBRESOURCE_DATA initial_data = {};
		initial_data.pSysMem = ortho_projection;
		initial_data.SysMemPitch = sizeof(ortho_projection);

		if (FAILED(_device->CreateBuffer(&desc, &initial_data, &_imgui.cb)))
			return false;
	}

	if (_imgui.bs == nullptr)
	{
		D3D10_BLEND_DESC desc = {};
		desc.BlendEnable[0] = TRUE;
		desc.SrcBlend = D3D10_BLEND_SRC_ALPHA;
		desc.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
		desc.BlendOp = D3D10_BLEND_OP_ADD;
		desc.SrcBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
		desc.DestBlendAlpha = D3D10_BLEND_ZERO;
		desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
		desc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;

		if (FAILED(_device->CreateBlendState(&desc, &_imgui.bs)))
			return false;
	}

	if (_imgui.rs == nullptr)
	{
		D3D10_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D10_FILL_SOLID;
		desc.CullMode = D3D10_CULL_NONE;
		desc.ScissorEnable = TRUE;
		desc.DepthClipEnable = TRUE;

		if (FAILED(_device->CreateRasterizerState(&desc, &_imgui.rs)))
			return false;
	}

	if (_imgui.ds == nullptr)
	{
		D3D10_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable = FALSE;
		desc.StencilEnable = FALSE;

		if (FAILED(_device->CreateDepthStencilState(&desc, &_imgui.ds)))
			return false;
	}

	if (_imgui.ss == nullptr)
	{
		D3D10_SAMPLER_DESC desc = {};
		desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D10_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D10_TEXTURE_ADDRESS_WRAP;
		desc.ComparisonFunc = D3D10_COMPARISON_ALWAYS;

		if (FAILED(_device->CreateSamplerState(&desc, &_imgui.ss)))
			return false;
	}

	return true;
}

void reshade::d3d10::runtime_impl::render_imgui_draw_data(ImDrawData *draw_data)
{
	// Projection matrix resides in an immutable constant buffer, so cannot change display dimensions
	assert(draw_data->DisplayPos.x == 0 && draw_data->DisplaySize.x == _width);
	assert(draw_data->DisplayPos.y == 0 && draw_data->DisplaySize.y == _height);

	// Create and grow vertex/index buffers if needed
	if (_imgui.num_indices < draw_data->TotalIdxCount)
	{
		_imgui.indices.reset();
		_imgui.num_indices = draw_data->TotalIdxCount + 10000;

		D3D10_BUFFER_DESC desc = {};
		desc.Usage = D3D10_USAGE_DYNAMIC;
		desc.ByteWidth = _imgui.num_indices * sizeof(ImDrawIdx);
		desc.BindFlags = D3D10_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

		if (FAILED(_device->CreateBuffer(&desc, nullptr, &_imgui.indices)))
			return;
	}
	if (_imgui.num_vertices < draw_data->TotalVtxCount)
	{
		_imgui.vertices.reset();
		_imgui.num_vertices = draw_data->TotalVtxCount + 5000;

		D3D10_BUFFER_DESC desc = {};
		desc.Usage = D3D10_USAGE_DYNAMIC;
		desc.ByteWidth = _imgui.num_vertices * sizeof(ImDrawVert);
		desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

		if (FAILED(_device->CreateBuffer(&desc, nullptr, &_imgui.vertices)))
			return;
	}

	if (ImDrawIdx *idx_dst;
		SUCCEEDED(_imgui.indices->Map(D3D10_MAP_WRITE_DISCARD, 0, reinterpret_cast<void **>(&idx_dst))))
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			idx_dst += draw_list->IdxBuffer.Size;
		}

		_imgui.indices->Unmap();
	}
	if (ImDrawVert *vtx_dst;
		SUCCEEDED(_imgui.vertices->Map(D3D10_MAP_WRITE_DISCARD, 0, reinterpret_cast<void **>(&vtx_dst))))
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
			vtx_dst += draw_list->VtxBuffer.Size;
		}

		_imgui.vertices->Unmap();
	}

	// Setup render state and render draw lists
	_device->IASetInputLayout(_imgui.layout.get());
	_device->IASetIndexBuffer(_imgui.indices.get(), sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
	const UINT stride = sizeof(ImDrawVert), offset = 0;
	ID3D10Buffer *const vertex_buffers[] = { _imgui.vertices.get() };
	_device->IASetVertexBuffers(0, ARRAYSIZE(vertex_buffers), vertex_buffers, &stride, &offset);
	_device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_device->VSSetShader(_imgui.vs.get());
	ID3D10Buffer *const constant_buffers[] = { _imgui.cb.get() };
	_device->VSSetConstantBuffers(0, ARRAYSIZE(constant_buffers), constant_buffers);
	_device->GSSetShader(nullptr);
	_device->PSSetShader(_imgui.ps.get());
	ID3D10SamplerState *const samplers[] = { _imgui.ss.get() };
	_device->PSSetSamplers(0, ARRAYSIZE(samplers), samplers);
	_device->RSSetState(_imgui.rs.get());
	const D3D10_VIEWPORT viewport = { 0, 0, _width, _height, 0.0f, 1.0f };
	_device->RSSetViewports(1, &viewport);
	const FLOAT blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
	_device->OMSetBlendState(_imgui.bs.get(), blend_factor, D3D10_DEFAULT_SAMPLE_MASK);
	_device->OMSetDepthStencilState(_imgui.ds.get(), 0);
	ID3D10RenderTargetView *const render_targets[] = { _backbuffer_rtv[0].get() };
	_device->OMSetRenderTargets(ARRAYSIZE(render_targets), render_targets, nullptr);

	UINT vtx_offset = 0, idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; ++n)
	{
		const ImDrawList *const draw_list = draw_data->CmdLists[n];

		for (const ImDrawCmd &cmd : draw_list->CmdBuffer)
		{
			assert(cmd.TextureId != 0);
			assert(cmd.UserCallback == nullptr);

			const D3D10_RECT scissor_rect = {
				static_cast<LONG>(cmd.ClipRect.x),
				static_cast<LONG>(cmd.ClipRect.y),
				static_cast<LONG>(cmd.ClipRect.z),
				static_cast<LONG>(cmd.ClipRect.w)
			};
			_device->RSSetScissorRects(1, &scissor_rect);

			ID3D10ShaderResourceView *const texture_view =
				static_cast<const tex_data *>(cmd.TextureId)->srv[0].get();
			_device->PSSetShaderResources(0, 1, &texture_view);

			_device->DrawIndexed(cmd.ElemCount, cmd.IdxOffset + idx_offset, cmd.VtxOffset + vtx_offset);
		}

		idx_offset += draw_list->IdxBuffer.Size;
		vtx_offset += draw_list->VtxBuffer.Size;
	}
}

#endif
