/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <d3d11_1.h>
#include "com_ptr.hpp"

#define RESHADE_D3D11_STATE_BLOCK_TYPE 0

namespace reshade::d3d11
{
	class state_block
	{
	public:
		explicit state_block(com_ptr<ID3D11Device> device);
		~state_block();

		void capture(ID3D11DeviceContext *device_context);
		void apply_and_release();

	private:
		D3D_FEATURE_LEVEL _device_feature_level;
		com_ptr<ID3D11DeviceContext> _device_context;
#if RESHADE_D3D11_STATE_BLOCK_TYPE
		com_ptr<ID3DDeviceContextState> _state;
		com_ptr<ID3DDeviceContextState> _captured_state;
#endif
		com_ptr<ID3D11InputLayout> _ia_input_layout;
		D3D11_PRIMITIVE_TOPOLOGY _ia_primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		com_ptr<ID3D11Buffer> _ia_vertex_buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		UINT _ia_vertex_strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
		UINT _ia_vertex_offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
		com_ptr<ID3D11Buffer> _ia_index_buffer;
		DXGI_FORMAT _ia_index_format = DXGI_FORMAT_UNKNOWN;
		UINT _ia_index_offset = 0;
		com_ptr<ID3D11VertexShader> _vs;
		UINT _vs_num_class_instances = 0;
		com_ptr<ID3D11ClassInstance> _vs_class_instances[256];
		com_ptr<ID3D11Buffer> _vs_constant_buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		com_ptr<ID3D11SamplerState> _vs_sampler_states[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
		com_ptr<ID3D11ShaderResourceView> _vs_shader_resources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		com_ptr<ID3D11HullShader> _hs;
		UINT _hs_num_class_instances = 0;
		com_ptr<ID3D11ClassInstance> _hs_class_instances[256];
		com_ptr<ID3D11DomainShader> _ds;
		UINT _ds_num_class_instances = 0;
		com_ptr<ID3D11ClassInstance> _ds_class_instances[256];
		com_ptr<ID3D11GeometryShader> _gs;
		UINT _gs_num_class_instances = 0;
		com_ptr<ID3D11ClassInstance> _gs_class_instances[256];
		com_ptr<ID3D11ShaderResourceView> _gs_shader_resources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		com_ptr<ID3D11RasterizerState> _rs_state;
		UINT _rs_num_viewports = 0;
		D3D11_VIEWPORT _rs_viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
		UINT _rs_num_scissor_rects = 0;
		D3D11_RECT _rs_scissor_rects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
		com_ptr<ID3D11PixelShader> _ps;
		UINT _ps_num_class_instances = 0;
		com_ptr<ID3D11ClassInstance> _ps_class_instances[256];
		com_ptr<ID3D11Buffer> _ps_constant_buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		com_ptr<ID3D11SamplerState> _ps_sampler_states[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
		com_ptr<ID3D11ShaderResourceView> _ps_shader_resources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		com_ptr<ID3D11BlendState> _om_blend_state;
		FLOAT _om_blend_factor[4] = { D3D11_DEFAULT_BLEND_FACTOR_RED, D3D11_DEFAULT_BLEND_FACTOR_BLUE, D3D11_DEFAULT_BLEND_FACTOR_GREEN, D3D11_DEFAULT_BLEND_FACTOR_ALPHA };
		UINT _om_sample_mask = D3D11_DEFAULT_SAMPLE_MASK;
		com_ptr<ID3D11DepthStencilState> _om_depth_stencil_state;
		UINT _om_stencil_ref = D3D11_DEFAULT_STENCIL_REFERENCE;
		com_ptr<ID3D11RenderTargetView> _om_render_targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		com_ptr<ID3D11DepthStencilView> _om_depth_stencil;
		UINT _cs_num_class_instances = 0;
		com_ptr<ID3D11ClassInstance> _cs_class_instances[256];
		com_ptr<ID3D11Buffer> _cs_constant_buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		com_ptr<ID3D11SamplerState> _cs_sampler_states[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
		com_ptr<ID3D11ShaderResourceView> _cs_shader_resources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		com_ptr<ID3D11UnorderedAccessView> _cs_unordered_access_views[D3D11_1_UAV_SLOT_COUNT];
		com_ptr<ID3D11ComputeShader> _cs;
	};
}
