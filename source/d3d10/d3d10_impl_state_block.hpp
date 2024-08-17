/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <d3d10_1.h>
#include "com_ptr.hpp"

namespace reshade::d3d10
{
	class state_block
	{
	public:
		explicit state_block(com_ptr<ID3D10Device> device);
		~state_block();

		void capture();
		void apply_and_release();

	private:
		com_ptr<ID3D10Device> _device;
		com_ptr<ID3D10InputLayout> _ia_input_layout;
		D3D10_PRIMITIVE_TOPOLOGY _ia_primitive_topology = D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED;
		com_ptr<ID3D10Buffer> _ia_vertex_buffers[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		UINT _ia_vertex_strides[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
		UINT _ia_vertex_offsets[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
		com_ptr<ID3D10Buffer> _ia_index_buffer;
		DXGI_FORMAT _ia_index_format = DXGI_FORMAT_UNKNOWN;
		UINT _ia_index_offset = 0;
		com_ptr<ID3D10VertexShader> _vs;
		com_ptr<ID3D10Buffer> _vs_constant_buffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		com_ptr<ID3D10SamplerState> _vs_sampler_states[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
		com_ptr<ID3D10ShaderResourceView> _vs_shader_resources[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		com_ptr<ID3D10GeometryShader> _gs;
		com_ptr<ID3D10ShaderResourceView> _gs_shader_resources[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		com_ptr<ID3D10RasterizerState> _rs_state;
		UINT _rs_num_viewports = 0;
		D3D10_VIEWPORT _rs_viewports[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
		UINT _rs_num_scissor_rects = 0;
		D3D10_RECT _rs_scissor_rects[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
		com_ptr<ID3D10PixelShader> _ps;
		com_ptr<ID3D10Buffer> _ps_constant_buffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		com_ptr<ID3D10SamplerState> _ps_sampler_states[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
		com_ptr<ID3D10ShaderResourceView> _ps_shader_resources[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		com_ptr<ID3D10BlendState> _om_blend_state;
		FLOAT _om_blend_factor[4] = { D3D10_DEFAULT_BLEND_FACTOR_RED, D3D10_DEFAULT_BLEND_FACTOR_BLUE, D3D10_DEFAULT_BLEND_FACTOR_GREEN, D3D10_DEFAULT_BLEND_FACTOR_ALPHA };
		UINT _om_sample_mask = D3D10_DEFAULT_SAMPLE_MASK;
		com_ptr<ID3D10DepthStencilState> _om_depth_stencil_state;
		UINT _om_stencil_ref = D3D10_DEFAULT_STENCIL_REFERENCE;
		com_ptr<ID3D10RenderTargetView> _om_render_targets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
		com_ptr<ID3D10DepthStencilView> _om_depth_stencil;
	};
}
