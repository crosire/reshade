/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_impl_state_block.hpp"

reshade::d3d10::state_block::state_block(com_ptr<ID3D10Device> device) :
	_device(std::move(device))
{
}
reshade::d3d10::state_block::~state_block()
{
}

void reshade::d3d10::state_block::capture()
{
	_device->IAGetPrimitiveTopology(&_ia_primitive_topology);
	_device->IAGetInputLayout(&_ia_input_layout);

	_device->IAGetVertexBuffers(0, ARRAYSIZE(_ia_vertex_buffers), reinterpret_cast<ID3D10Buffer **>(_ia_vertex_buffers), _ia_vertex_strides, _ia_vertex_offsets);
	_device->IAGetIndexBuffer(&_ia_index_buffer, &_ia_index_format, &_ia_index_offset);

	_device->RSGetState(&_rs_state);
	_device->RSGetViewports(&_rs_num_viewports, nullptr);
	_device->RSGetViewports(&_rs_num_viewports, _rs_viewports);
	_device->RSGetScissorRects(&_rs_num_scissor_rects, nullptr);
	_device->RSGetScissorRects(&_rs_num_scissor_rects, _rs_scissor_rects);

	_device->VSGetShader(&_vs);
	_device->VSGetConstantBuffers(0, ARRAYSIZE(_vs_constant_buffers), reinterpret_cast<ID3D10Buffer **>(_vs_constant_buffers));
	_device->VSGetSamplers(0, ARRAYSIZE(_vs_sampler_states), reinterpret_cast<ID3D10SamplerState **>(_vs_sampler_states));
	_device->VSGetShaderResources(0, ARRAYSIZE(_vs_shader_resources), reinterpret_cast<ID3D10ShaderResourceView **>(_vs_shader_resources));

	_device->GSGetShader(&_gs);
	_device->GSGetShaderResources(0, ARRAYSIZE(_gs_shader_resources), reinterpret_cast<ID3D10ShaderResourceView **>(_gs_shader_resources));

	_device->PSGetShader(&_ps);
	_device->PSGetConstantBuffers(0, ARRAYSIZE(_ps_constant_buffers), reinterpret_cast<ID3D10Buffer **>(_ps_constant_buffers));
	_device->PSGetSamplers(0, ARRAYSIZE(_ps_sampler_states), reinterpret_cast<ID3D10SamplerState **>(_ps_sampler_states));
	_device->PSGetShaderResources(0, ARRAYSIZE(_ps_shader_resources), reinterpret_cast<ID3D10ShaderResourceView **>(_ps_shader_resources));

	_device->OMGetBlendState(&_om_blend_state, _om_blend_factor, &_om_sample_mask);
	_device->OMGetDepthStencilState(&_om_depth_stencil_state, &_om_stencil_ref);
	_device->OMGetRenderTargets(ARRAYSIZE(_om_render_targets), reinterpret_cast<ID3D10RenderTargetView **>(_om_render_targets), &_om_depth_stencil);
}
void reshade::d3d10::state_block::apply_and_release()
{
	_device->IASetPrimitiveTopology(_ia_primitive_topology);
	_device->IASetInputLayout(_ia_input_layout.get());

	_device->IASetVertexBuffers(0, ARRAYSIZE(_ia_vertex_buffers), reinterpret_cast<ID3D10Buffer *const *>(_ia_vertex_buffers), _ia_vertex_strides, _ia_vertex_offsets);
	_device->IASetIndexBuffer(_ia_index_buffer.get(), _ia_index_format, _ia_index_offset);

	_device->RSSetState(_rs_state.get());
	_device->RSSetViewports(_rs_num_viewports, _rs_viewports);
	_device->RSSetScissorRects(_rs_num_scissor_rects, _rs_scissor_rects);

	_device->VSSetShader(_vs.get());
	_device->VSSetConstantBuffers(0, ARRAYSIZE(_vs_constant_buffers), reinterpret_cast<ID3D10Buffer *const * >(_vs_constant_buffers));
	_device->VSSetSamplers(0, ARRAYSIZE(_vs_sampler_states), reinterpret_cast<ID3D10SamplerState *const *>(_vs_sampler_states));
	_device->VSSetShaderResources(0, ARRAYSIZE(_vs_shader_resources), reinterpret_cast<ID3D10ShaderResourceView *const *>(_vs_shader_resources));

	_device->GSSetShader(_gs.get());
	_device->GSSetShaderResources(0, ARRAYSIZE(_gs_shader_resources), reinterpret_cast<ID3D10ShaderResourceView *const *>(_gs_shader_resources));

	_device->PSSetShader(_ps.get());
	_device->PSSetConstantBuffers(0, ARRAYSIZE(_ps_constant_buffers), reinterpret_cast<ID3D10Buffer *const *>(_ps_constant_buffers));
	_device->PSSetSamplers(0, ARRAYSIZE(_ps_sampler_states), reinterpret_cast<ID3D10SamplerState *const *>(_ps_sampler_states));
	_device->PSSetShaderResources(0, ARRAYSIZE(_ps_shader_resources), reinterpret_cast<ID3D10ShaderResourceView *const *>(_ps_shader_resources));

	_device->OMSetBlendState(_om_blend_state.get(), _om_blend_factor, _om_sample_mask);
	_device->OMSetDepthStencilState(_om_depth_stencil_state.get(), _om_stencil_ref);
	_device->OMSetRenderTargets(ARRAYSIZE(_om_render_targets), reinterpret_cast<ID3D10RenderTargetView *const *>(_om_render_targets), _om_depth_stencil.get());

	*this = state_block(std::move(_device));
}
