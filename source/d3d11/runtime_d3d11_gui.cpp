/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_GUI

#include "dll_resources.hpp"
#include "runtime_d3d11.hpp"
#include "runtime_d3d11_objects.hpp"
#include <imgui.h>

bool reshade::d3d11::runtime_impl::init_imgui_resources()
{
	if (_imgui.vs == nullptr)
	{
		const resources::data_resource vs_res = resources::load_data_resource(IDR_IMGUI_VS);
		if (FAILED(_device->CreateVertexShader(vs_res.data, vs_res.data_size, nullptr, &_imgui.vs)))
			return false;

		const D3D11_INPUT_ELEMENT_DESC input_layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(ImDrawVert, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(ImDrawVert, uv ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(ImDrawVert, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(_device->CreateInputLayout(input_layout, ARRAYSIZE(input_layout), vs_res.data, vs_res.data_size, &_imgui.layout)))
			return false;
	}

	if (_imgui.ps == nullptr)
	{
		const resources::data_resource ps_res = resources::load_data_resource(IDR_IMGUI_PS);
		if (FAILED(_device->CreatePixelShader(ps_res.data, ps_res.data_size, nullptr, &_imgui.ps)))
			return false;
	}

	if (_imgui.cb == nullptr)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = 16 * sizeof(float);
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		// Setup orthographic projection matrix
		const float ortho_projection[16] = {
			2.0f / _width, 0.0f,   0.0f, 0.0f,
			0.0f, -2.0f / _height, 0.0f, 0.0f,
			0.0f,          0.0f,   0.5f, 0.0f,
			-1.f,          1.0f,   0.5f, 1.0f
		};

		D3D11_SUBRESOURCE_DATA initial_data = {};
		initial_data.pSysMem = ortho_projection;
		initial_data.SysMemPitch = sizeof(ortho_projection);

		if (FAILED(_device->CreateBuffer(&desc, &initial_data, &_imgui.cb)))
			return false;
	}

	if (_imgui.bs == nullptr)
	{
		D3D11_BLEND_DESC desc = {};
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		if (FAILED(_device->CreateBlendState(&desc, &_imgui.bs)))
			return false;
	}

	if (_imgui.rs == nullptr)
	{
		D3D11_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_NONE;
		desc.ScissorEnable = TRUE;
		desc.DepthClipEnable = TRUE;

		if (FAILED(_device->CreateRasterizerState(&desc, &_imgui.rs)))
			return false;
	}

	if (_imgui.ds == nullptr)
	{
		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable = FALSE;
		desc.StencilEnable = FALSE;

		if (FAILED(_device->CreateDepthStencilState(&desc, &_imgui.ds)))
			return false;
	}

	if (_imgui.ss == nullptr)
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

		if (FAILED(_device->CreateSamplerState(&desc, &_imgui.ss)))
			return false;
	}

	return true;
}

void reshade::d3d11::runtime_impl::render_imgui_draw_data(ImDrawData *draw_data)
{
	// Projection matrix resides in an immutable constant buffer, so cannot change display dimensions
	assert(draw_data->DisplayPos.x == 0 && draw_data->DisplaySize.x == _width);
	assert(draw_data->DisplayPos.y == 0 && draw_data->DisplaySize.y == _height);

	// Create and grow vertex/index buffers if needed
	if (_imgui.num_indices < draw_data->TotalIdxCount)
	{
		_imgui.indices.reset();
		_imgui.num_indices = draw_data->TotalIdxCount + 10000;

		D3D11_BUFFER_DESC desc = {};
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = _imgui.num_indices * sizeof(ImDrawIdx);
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		if (FAILED(_device->CreateBuffer(&desc, nullptr, &_imgui.indices)))
			return;
	}
	if (_imgui.num_vertices < draw_data->TotalVtxCount)
	{
		_imgui.vertices.reset();
		_imgui.num_vertices = draw_data->TotalVtxCount + 5000;

		D3D11_BUFFER_DESC desc = {};
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = _imgui.num_vertices * sizeof(ImDrawVert);
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		if (FAILED(_device->CreateBuffer(&desc, nullptr, &_imgui.vertices)))
			return;
	}

	if (D3D11_MAPPED_SUBRESOURCE idx_resource;
		SUCCEEDED(_immediate_context->Map(_imgui.indices.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource)))
	{
		auto idx_dst = static_cast<ImDrawIdx *>(idx_resource.pData);
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			idx_dst += draw_list->IdxBuffer.Size;
		}

		_immediate_context->Unmap(_imgui.indices.get(), 0);
	}
	if (D3D11_MAPPED_SUBRESOURCE vtx_resource;
		SUCCEEDED(_immediate_context->Map(_imgui.vertices.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource)))
	{
		auto vtx_dst = static_cast<ImDrawVert *>(vtx_resource.pData);
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
			vtx_dst += draw_list->VtxBuffer.Size;
		}

		_immediate_context->Unmap(_imgui.vertices.get(), 0);
	}

	// Setup render state and render draw lists
	_immediate_context->IASetInputLayout(_imgui.layout.get());
	_immediate_context->IASetIndexBuffer(_imgui.indices.get(), sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
	const UINT stride = sizeof(ImDrawVert), offset = 0;
	ID3D11Buffer *const vertex_buffers[] = { _imgui.vertices.get() };
	_immediate_context->IASetVertexBuffers(0, ARRAYSIZE(vertex_buffers), vertex_buffers, &stride, &offset);
	_immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_immediate_context->VSSetShader(_imgui.vs.get(), nullptr, 0);
	ID3D11Buffer *const constant_buffers[] = { _imgui.cb.get() };
	_immediate_context->VSSetConstantBuffers(0, ARRAYSIZE(constant_buffers), constant_buffers);
	_immediate_context->HSSetShader(nullptr, nullptr, 0);
	_immediate_context->DSSetShader(nullptr, nullptr, 0);
	_immediate_context->GSSetShader(nullptr, nullptr, 0);
	_immediate_context->PSSetShader(_imgui.ps.get(), nullptr, 0);
	ID3D11SamplerState *const samplers[] = { _imgui.ss.get() };
	_immediate_context->PSSetSamplers(0, ARRAYSIZE(samplers), samplers);
	_immediate_context->RSSetState(_imgui.rs.get());
	const D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(_width), static_cast<FLOAT>(_height), 0.0f, 1.0f };
	_immediate_context->RSSetViewports(1, &viewport);
	const FLOAT blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	_immediate_context->OMSetBlendState(_imgui.bs.get(), blend_factor, D3D11_DEFAULT_SAMPLE_MASK);
	_immediate_context->OMSetDepthStencilState(_imgui.ds.get(), 0);
	ID3D11RenderTargetView *const render_targets[] = { _backbuffer_rtv[0].get() };
	_immediate_context->OMSetRenderTargets(ARRAYSIZE(render_targets), render_targets, nullptr);

	UINT vtx_offset = 0, idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; ++n)
	{
		const ImDrawList *const draw_list = draw_data->CmdLists[n];

		for (const ImDrawCmd &cmd : draw_list->CmdBuffer)
		{
			assert(cmd.TextureId != 0);
			assert(cmd.UserCallback == nullptr);

			const D3D11_RECT scissor_rect = {
				static_cast<LONG>(cmd.ClipRect.x),
				static_cast<LONG>(cmd.ClipRect.y),
				static_cast<LONG>(cmd.ClipRect.z),
				static_cast<LONG>(cmd.ClipRect.w)
			};
			_immediate_context->RSSetScissorRects(1, &scissor_rect);

			ID3D11ShaderResourceView *const texture_view =
				static_cast<const tex_data *>(cmd.TextureId)->srv[0].get();
			_immediate_context->PSSetShaderResources(0, 1, &texture_view);

			_immediate_context->DrawIndexed(cmd.ElemCount, cmd.IdxOffset + idx_offset, cmd.VtxOffset + vtx_offset);
		}

		idx_offset += draw_list->IdxBuffer.Size;
		vtx_offset += draw_list->VtxBuffer.Size;
	}
}

#endif
