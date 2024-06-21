/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_impl_state_block.hpp"

template <typename T>
inline void safe_release(T *&object)
{
	if (object != nullptr)
	{
		object->Release();
		object = nullptr;
	}
}

reshade::d3d10::state_block::state_block(ID3D10Device *device)
{
	ZeroMemory(this, sizeof(*this));

	_device = device;
}
reshade::d3d10::state_block::~state_block()
{
	release_all_device_objects();
}

void reshade::d3d10::state_block::capture()
{
	_device->IAGetPrimitiveTopology(&_ia_primitive_topology);
	_device->IAGetInputLayout(&_ia_input_layout);

	_device->IAGetVertexBuffers(0, ARRAYSIZE(_ia_vertex_buffers), _ia_vertex_buffers, _ia_vertex_strides, _ia_vertex_offsets);
	_device->IAGetIndexBuffer(&_ia_index_buffer, &_ia_index_format, &_ia_index_offset);

	_device->RSGetState(&_rs_state);
	_device->RSGetViewports(&_rs_num_viewports, nullptr);
	_device->RSGetViewports(&_rs_num_viewports, _rs_viewports);
	_device->RSGetScissorRects(&_rs_num_scissor_rects, nullptr);
	_device->RSGetScissorRects(&_rs_num_scissor_rects, _rs_scissor_rects);

	_device->VSGetShader(&_vs);
	_device->VSGetConstantBuffers(0, ARRAYSIZE(_vs_constant_buffers), _vs_constant_buffers);
	_device->VSGetSamplers(0, ARRAYSIZE(_vs_sampler_states), _vs_sampler_states);
	_device->VSGetShaderResources(0, ARRAYSIZE(_vs_shader_resources), _vs_shader_resources);

	_device->GSGetShader(&_gs);
	_device->GSGetShaderResources(0, ARRAYSIZE(_gs_shader_resources), _gs_shader_resources);

	_device->PSGetShader(&_ps);
	_device->PSGetConstantBuffers(0, ARRAYSIZE(_ps_constant_buffers), _ps_constant_buffers);
	_device->PSGetSamplers(0, ARRAYSIZE(_ps_sampler_states), _ps_sampler_states);
	_device->PSGetShaderResources(0, ARRAYSIZE(_ps_shader_resources), _ps_shader_resources);

	_device->OMGetBlendState(&_om_blend_state, _om_blend_factor, &_om_sample_mask);
	_device->OMGetDepthStencilState(&_om_depth_stencil_state, &_om_stencil_ref);
	_device->OMGetRenderTargets(ARRAYSIZE(_om_render_targets), _om_render_targets, &_om_depth_stencil);
}
void reshade::d3d10::state_block::apply_and_release()
{
	_device->IASetPrimitiveTopology(_ia_primitive_topology);
	_device->IASetInputLayout(_ia_input_layout);

	_device->IASetVertexBuffers(0, ARRAYSIZE(_ia_vertex_buffers), _ia_vertex_buffers, _ia_vertex_strides, _ia_vertex_offsets);
	_device->IASetIndexBuffer(_ia_index_buffer, _ia_index_format, _ia_index_offset);

	_device->RSSetState(_rs_state);
	_device->RSSetViewports(_rs_num_viewports, _rs_viewports);
	_device->RSSetScissorRects(_rs_num_scissor_rects, _rs_scissor_rects);

	_device->VSSetShader(_vs);
	_device->VSSetConstantBuffers(0, ARRAYSIZE(_vs_constant_buffers), _vs_constant_buffers);
	_device->VSSetSamplers(0, ARRAYSIZE(_vs_sampler_states), _vs_sampler_states);
	_device->VSSetShaderResources(0, ARRAYSIZE(_vs_shader_resources), _vs_shader_resources);

	_device->GSSetShader(_gs);
	_device->GSSetShaderResources(0, ARRAYSIZE(_gs_shader_resources), _gs_shader_resources);

	_device->PSSetShader(_ps);
	_device->PSSetConstantBuffers(0, ARRAYSIZE(_ps_constant_buffers), _ps_constant_buffers);
	_device->PSSetSamplers(0, ARRAYSIZE(_ps_sampler_states), _ps_sampler_states);
	_device->PSSetShaderResources(0, ARRAYSIZE(_ps_shader_resources), _ps_shader_resources);

	_device->OMSetBlendState(_om_blend_state, _om_blend_factor, _om_sample_mask);
	_device->OMSetDepthStencilState(_om_depth_stencil_state, _om_stencil_ref);
	_device->OMSetRenderTargets(ARRAYSIZE(_om_render_targets), _om_render_targets, _om_depth_stencil);

	release_all_device_objects();
}

void reshade::d3d10::state_block::release_all_device_objects()
{
	safe_release(_ia_input_layout);
	for (auto &vertex_buffer : _ia_vertex_buffers)
		safe_release(vertex_buffer);
	safe_release(_ia_index_buffer);
	safe_release(_vs);
	for (auto &constant_buffer : _vs_constant_buffers)
		safe_release(constant_buffer);
	for (auto &sampler_state : _vs_sampler_states)
		safe_release(sampler_state);
	for (auto &shader_resource : _vs_shader_resources)
		safe_release(shader_resource);
	safe_release(_gs);
	for (auto &shader_resource : _gs_shader_resources)
		safe_release(shader_resource);
	safe_release(_rs_state);
	safe_release(_ps);
	for (auto &constant_buffer : _ps_constant_buffers)
		safe_release(constant_buffer);
	for (auto &sampler_state : _ps_sampler_states)
		safe_release(sampler_state);
	for (auto &shader_resource : _ps_shader_resources)
		safe_release(shader_resource);
	safe_release(_om_blend_state);
	safe_release(_om_depth_stencil_state);
	for (auto &render_target : _om_render_targets)
		safe_release(render_target);
	safe_release(_om_depth_stencil);
}
