/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "com_ptr.hpp"
#include <d3d10_1.h>

namespace reshade::d3d10
{
	class state_block
	{
	public:
		explicit state_block(ID3D10Device *device);
		~state_block();

		void capture();
		void apply_and_release();

	private:
		void release_all_device_objects();

		com_ptr<ID3D10Device> _device;
		ID3D10InputLayout *_ia_input_layout;
		D3D10_PRIMITIVE_TOPOLOGY _ia_primitive_topology;
		ID3D10Buffer *_ia_vertex_buffers[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		UINT _ia_vertex_strides[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		UINT _ia_vertex_offsets[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		ID3D10Buffer *_ia_index_buffer;
		DXGI_FORMAT _ia_index_format;
		UINT _ia_index_offset;
		ID3D10VertexShader *_vs;
		ID3D10Buffer *_vs_constant_buffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		ID3D10SamplerState *_vs_sampler_states[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
		ID3D10ShaderResourceView *_vs_shader_resources[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		ID3D10GeometryShader *_gs;
		ID3D10RasterizerState *_rs_state;
		UINT _rs_num_viewports;
		D3D10_VIEWPORT _rs_viewports[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		UINT _rs_num_scissor_rects;
		D3D10_RECT _rs_scissor_rects[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		ID3D10PixelShader *_ps;
		ID3D10Buffer *_ps_constant_buffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		ID3D10SamplerState *_ps_sampler_states[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
		ID3D10ShaderResourceView *_ps_shader_resources[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		ID3D10BlendState *_om_blend_state;
		FLOAT _om_blend_factor[4];
		UINT _om_sample_mask;
		ID3D10DepthStencilState *_om_depth_stencil_state;
		UINT _om_stencil_ref;
		ID3D10RenderTargetView *_om_render_targets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
		ID3D10DepthStencilView *_om_depth_stencil;
	};
}
