/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "state_block.hpp"

template <typename T>
static inline void safe_release(T *&object)
{
	if (object != nullptr)
	{
		object->Release();
		object = nullptr;
	}
}

reshade::d3d11::state_block::state_block(ID3D11Device *device)
{
	ZeroMemory(this, sizeof(*this));

	_device = device;
	_device_feature_level = device->GetFeatureLevel();
}
reshade::d3d11::state_block::~state_block()
{
	release_all_device_objects();
}

void reshade::d3d11::state_block::capture(ID3D11DeviceContext *devicecontext)
{
	_device_context = devicecontext;

	_device_context->IAGetPrimitiveTopology(&_ia_primitive_topology);
	_device_context->IAGetInputLayout(&_ia_input_layout);

	if (_device_feature_level > D3D_FEATURE_LEVEL_10_0)
		_device_context->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, _ia_vertex_buffers, _ia_vertex_strides, _ia_vertex_offsets);
	else
		_device_context->IAGetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, _ia_vertex_buffers, _ia_vertex_strides, _ia_vertex_offsets);

	_device_context->IAGetIndexBuffer(&_ia_index_buffer, &_ia_index_format, &_ia_index_offset);

	_device_context->RSGetState(&_rs_state);
	_device_context->RSGetViewports(&_rs_num_viewports, nullptr);
	_device_context->RSGetViewports(&_rs_num_viewports, _rs_viewports);
	_device_context->RSGetScissorRects(&_rs_num_scissor_rects, nullptr);
	_device_context->RSGetScissorRects(&_rs_num_scissor_rects, _rs_scissor_rects);

	_vs_num_class_instances = ARRAYSIZE(_vs_class_instances);
	_device_context->VSGetShader(&_vs, _vs_class_instances, &_vs_num_class_instances);
	_device_context->VSGetConstantBuffers(0, ARRAYSIZE(_vs_constant_buffers), _vs_constant_buffers);
	_device_context->VSGetSamplers(0, ARRAYSIZE(_vs_sampler_states), _vs_sampler_states);
	_device_context->VSGetShaderResources(0, ARRAYSIZE(_vs_shader_resources), _vs_shader_resources);

	if (_device_feature_level >= D3D_FEATURE_LEVEL_10_0)
	{
		if (_device_feature_level >= D3D_FEATURE_LEVEL_11_0)
		{
			_hs_num_class_instances = ARRAYSIZE(_hs_class_instances);
			_device_context->HSGetShader(&_hs, _hs_class_instances, &_hs_num_class_instances);

			_ds_num_class_instances = ARRAYSIZE(_ds_class_instances);
			_device_context->DSGetShader(&_ds, _ds_class_instances, &_ds_num_class_instances);
		}

		_gs_num_class_instances = ARRAYSIZE(_gs_class_instances);
		_device_context->GSGetShader(&_gs, _gs_class_instances, &_gs_num_class_instances);
	}

	_ps_num_class_instances = ARRAYSIZE(_ps_class_instances);
	_device_context->PSGetShader(&_ps, _ps_class_instances, &_ps_num_class_instances);
	_device_context->PSGetConstantBuffers(0, ARRAYSIZE(_ps_constant_buffers), _ps_constant_buffers);
	_device_context->PSGetSamplers(0, ARRAYSIZE(_ps_sampler_states), _ps_sampler_states);
	_device_context->PSGetShaderResources(0, ARRAYSIZE(_ps_shader_resources), _ps_shader_resources);

	_device_context->OMGetBlendState(&_om_blend_state, _om_blend_factor, &_om_sample_mask);
	_device_context->OMGetDepthStencilState(&_om_depth_stencil_state, &_om_stencil_ref);
	_device_context->OMGetRenderTargets(ARRAYSIZE(_om_render_targets), _om_render_targets, &_om_depth_stencil);

	if (_device_feature_level >= D3D_FEATURE_LEVEL_10_0)
	{
		_cs_num_class_instances = ARRAYSIZE(_cs_class_instances);
		_device_context->CSGetShader(&_cs, _cs_class_instances, &_cs_num_class_instances);
		_device_context->CSGetConstantBuffers(0, ARRAYSIZE(_cs_constant_buffers), _cs_constant_buffers);
		_device_context->CSGetSamplers(0, ARRAYSIZE(_cs_sampler_states), _cs_sampler_states);
		_device_context->CSGetShaderResources(0, ARRAYSIZE(_cs_shader_resources), _cs_shader_resources);
		_device_context->CSGetUnorderedAccessViews(0,
			_device_feature_level >= D3D_FEATURE_LEVEL_11_1 ? D3D11_1_UAV_SLOT_COUNT :
			_device_feature_level == D3D_FEATURE_LEVEL_11_0 ? D3D11_PS_CS_UAV_REGISTER_COUNT : D3D11_CS_4_X_UAV_REGISTER_COUNT, _cs_unordered_access_views);
	}
}
void reshade::d3d11::state_block::apply_and_release()
{
	_device_context->IASetPrimitiveTopology(_ia_primitive_topology);
	_device_context->IASetInputLayout(_ia_input_layout);

	// With D3D_FEATURE_LEVEL_10_0 or less, the maximum number of IA Vertex Input Slots is 16
	// Starting with D3D_FEATURE_LEVEL_10_1 it is 32
	// See https://docs.microsoft.com/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-intro
	if (_device_feature_level > D3D_FEATURE_LEVEL_10_0)
		_device_context->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, _ia_vertex_buffers, _ia_vertex_strides, _ia_vertex_offsets);
	else
		_device_context->IASetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, _ia_vertex_buffers, _ia_vertex_strides, _ia_vertex_offsets);

	_device_context->IASetIndexBuffer(_ia_index_buffer, _ia_index_format, _ia_index_offset);

	_device_context->RSSetState(_rs_state);
	_device_context->RSSetViewports(_rs_num_viewports, _rs_viewports);
	_device_context->RSSetScissorRects(_rs_num_scissor_rects, _rs_scissor_rects);

	_device_context->VSSetShader(_vs, _vs_class_instances, _vs_num_class_instances);
	_device_context->VSSetConstantBuffers(0, ARRAYSIZE(_vs_constant_buffers), _vs_constant_buffers);
	_device_context->VSSetSamplers(0, ARRAYSIZE(_vs_sampler_states), _vs_sampler_states);
	_device_context->VSSetShaderResources(0, ARRAYSIZE(_vs_shader_resources), _vs_shader_resources);

	if (_device_feature_level >= D3D_FEATURE_LEVEL_10_0)
	{
		if (_device_feature_level >= D3D_FEATURE_LEVEL_11_0)
		{
			_device_context->HSSetShader(_hs, _hs_class_instances, _hs_num_class_instances);
			_device_context->DSSetShader(_ds, _ds_class_instances, _ds_num_class_instances);
		}

		_device_context->GSSetShader(_gs, _gs_class_instances, _gs_num_class_instances);
	}

	_device_context->PSSetShader(_ps, _ps_class_instances, _ps_num_class_instances);
	_device_context->PSSetConstantBuffers(0, ARRAYSIZE(_ps_constant_buffers), _ps_constant_buffers);
	_device_context->PSSetSamplers(0, ARRAYSIZE(_ps_sampler_states), _ps_sampler_states);
	_device_context->PSSetShaderResources(0, ARRAYSIZE(_ps_shader_resources), _ps_shader_resources);

	_device_context->OMSetBlendState(_om_blend_state, _om_blend_factor, _om_sample_mask);
	_device_context->OMSetDepthStencilState(_om_depth_stencil_state, _om_stencil_ref);
	_device_context->OMSetRenderTargets(ARRAYSIZE(_om_render_targets), _om_render_targets, _om_depth_stencil);

	if (_device_feature_level >= D3D_FEATURE_LEVEL_10_0)
	{
		_device_context->CSSetShader(_cs, _cs_class_instances, _cs_num_class_instances);
		_device_context->CSSetConstantBuffers(0, ARRAYSIZE(_cs_constant_buffers), _cs_constant_buffers);
		_device_context->CSSetSamplers(0, ARRAYSIZE(_cs_sampler_states), _cs_sampler_states);
		_device_context->CSSetShaderResources(0, ARRAYSIZE(_cs_shader_resources), _cs_shader_resources);
		UINT uav_initial_counts[D3D11_1_UAV_SLOT_COUNT];
		std::memset(uav_initial_counts, -1, sizeof(uav_initial_counts)); // Keep the current offset
		_device_context->CSSetUnorderedAccessViews(0,
			_device_feature_level >= D3D_FEATURE_LEVEL_11_1 ? D3D11_1_UAV_SLOT_COUNT :
			_device_feature_level == D3D_FEATURE_LEVEL_11_0 ? D3D11_PS_CS_UAV_REGISTER_COUNT : D3D11_CS_4_X_UAV_REGISTER_COUNT, _cs_unordered_access_views, uav_initial_counts);
	}

	release_all_device_objects();

	_device_context.reset();
}

void reshade::d3d11::state_block::release_all_device_objects()
{
	safe_release(_ia_input_layout);
	for (auto &vertex_buffer : _ia_vertex_buffers)
		safe_release(vertex_buffer);
	safe_release(_ia_index_buffer);
	safe_release(_vs);
	for (UINT i = 0; i < _vs_num_class_instances; i++)
		safe_release(_vs_class_instances[i]);
	for (auto &constant_buffer : _vs_constant_buffers)
		safe_release(constant_buffer);
	for (auto &sampler_state : _vs_sampler_states)
		safe_release(sampler_state);
	for (auto &shader_resource : _vs_shader_resources)
		safe_release(shader_resource);
	safe_release(_hs);
	for (UINT i = 0; i < _hs_num_class_instances; i++)
		safe_release(_hs_class_instances[i]);
	safe_release(_ds);
	for (UINT i = 0; i < _ds_num_class_instances; i++)
		safe_release(_ds_class_instances[i]);
	safe_release(_gs);
	for (UINT i = 0; i < _gs_num_class_instances; i++)
		safe_release(_gs_class_instances[i]);
	safe_release(_rs_state);
	safe_release(_ps);
	for (UINT i = 0; i < _ps_num_class_instances; i++)
		safe_release(_ps_class_instances[i]);
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
	safe_release(_cs);
	for (UINT i = 0; i < _cs_num_class_instances; i++)
		safe_release(_cs_class_instances[i]);
	for (auto &constant_buffer : _cs_constant_buffers)
		safe_release(constant_buffer);
	for (auto &sampler_state : _cs_sampler_states)
		safe_release(sampler_state);
	for (auto &shader_resource : _cs_shader_resources)
		safe_release(shader_resource);
	for (auto &unordered_access_view : _cs_unordered_access_views)
		safe_release(unordered_access_view);
}
