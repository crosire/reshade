/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "com_ptr.hpp"
#include <d3d11_1.h>

namespace reshade::d3d11
{
	class state_block
	{
	public:
		explicit state_block(ID3D11Device *device);
		~state_block();

		void capture(ID3D11DeviceContext *devicecontext);
		void apply_and_release();

	private:
		void release_all_device_objects();

		D3D_FEATURE_LEVEL _device_feature_level;
		com_ptr<ID3D11Device> _device;
		com_ptr<ID3D11DeviceContext> _device_context;
		ID3D11InputLayout *_ia_input_layout;
		D3D11_PRIMITIVE_TOPOLOGY _ia_primitive_topology;
		ID3D11Buffer *_ia_vertex_buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		UINT _ia_vertex_strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		UINT _ia_vertex_offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		ID3D11Buffer *_ia_index_buffer;
		DXGI_FORMAT _ia_index_format;
		UINT _ia_index_offset;
		ID3D11VertexShader *_vs;
		UINT _vs_num_class_instances;
		ID3D11ClassInstance *_vs_class_instances[256];
		ID3D11Buffer *_vs_constant_buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		ID3D11SamplerState *_vs_sampler_states[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
		ID3D11ShaderResourceView *_vs_shader_resources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		ID3D11HullShader *_hs;
		UINT _hs_num_class_instances;
		ID3D11ClassInstance *_hs_class_instances[256];
		ID3D11DomainShader *_ds;
		UINT _ds_num_class_instances;
		ID3D11ClassInstance *_ds_class_instances[256];
		ID3D11GeometryShader *_gs;
		UINT _gs_num_class_instances;
		ID3D11ClassInstance *_gs_class_instances[256];
		ID3D11RasterizerState *_rs_state;
		UINT _rs_num_viewports;
		D3D11_VIEWPORT _rs_viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		UINT _rs_num_scissor_rects;
		D3D11_RECT _rs_scissor_rects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		ID3D11PixelShader *_ps;
		UINT _ps_num_class_instances;
		ID3D11ClassInstance *_ps_class_instances[256];
		ID3D11Buffer *_ps_constant_buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		ID3D11SamplerState *_ps_sampler_states[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
		ID3D11ShaderResourceView *_ps_shader_resources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		ID3D11BlendState *_om_blend_state;
		FLOAT _om_blend_factor[4];
		UINT _om_sample_mask;
		ID3D11DepthStencilState *_om_depth_stencil_state;
		UINT _om_stencil_ref;
		ID3D11RenderTargetView *_om_render_targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		ID3D11DepthStencilView *_om_depth_stencil;
	};
}
