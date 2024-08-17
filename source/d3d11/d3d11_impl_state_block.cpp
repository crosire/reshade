/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d11_impl_state_block.hpp"

reshade::d3d11::state_block::state_block(com_ptr<ID3D11Device> device) :
	_device_feature_level(device->GetFeatureLevel())
{
#if RESHADE_D3D11_STATE_BLOCK_TYPE
	// Unfortunately this breaks RTSS, because calling 'ID3D11Device::CreateDeviceContextState' will expose the 'ID3D10Device' interface on the D3D11 device, making RTSS use its D3D10 renderer while the interface is locked out
	com_ptr<ID3D11Device1> device1;
	if (SUCCEEDED(device->QueryInterface(&device1)))
	{
		const UINT flags = (device->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED) ? D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED : 0;
		device1->CreateDeviceContextState(flags, &_device_feature_level, 1, D3D11_SDK_VERSION, __uuidof(ID3D11Device1), nullptr, &_state);
	}
#endif
}
reshade::d3d11::state_block::~state_block()
{
}

void reshade::d3d11::state_block::capture(ID3D11DeviceContext *device_context)
{
	assert(_device_context == nullptr);

	_device_context = device_context;

#if RESHADE_D3D11_STATE_BLOCK_TYPE
	com_ptr<ID3D11DeviceContext1> device_context1;
	if (_state != nullptr && _device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE && SUCCEEDED(_device_context->QueryInterface(&device_context1)))
	{
		device_context1->SwapDeviceContextState(_state.get(), &_captured_state);
		return;
	}
#endif

	_device_context->IAGetPrimitiveTopology(&_ia_primitive_topology);
	_device_context->IAGetInputLayout(&_ia_input_layout);

	if (_device_feature_level > D3D_FEATURE_LEVEL_10_0)
		_device_context->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, reinterpret_cast<ID3D11Buffer **>(_ia_vertex_buffers), _ia_vertex_strides, _ia_vertex_offsets);
	else
		_device_context->IAGetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, reinterpret_cast<ID3D11Buffer **>(_ia_vertex_buffers), _ia_vertex_strides, _ia_vertex_offsets);

	_device_context->IAGetIndexBuffer(&_ia_index_buffer, &_ia_index_format, &_ia_index_offset);

	_device_context->RSGetState(&_rs_state);
	_device_context->RSGetViewports(&_rs_num_viewports, nullptr);
	_device_context->RSGetViewports(&_rs_num_viewports, _rs_viewports);
	_device_context->RSGetScissorRects(&_rs_num_scissor_rects, nullptr);
	_device_context->RSGetScissorRects(&_rs_num_scissor_rects, _rs_scissor_rects);

	_vs_num_class_instances = ARRAYSIZE(_vs_class_instances);
	_device_context->VSGetShader(&_vs, reinterpret_cast<ID3D11ClassInstance **>(_vs_class_instances), &_vs_num_class_instances);
	_device_context->VSGetConstantBuffers(0, ARRAYSIZE(_vs_constant_buffers), reinterpret_cast<ID3D11Buffer **>(_vs_constant_buffers));
	_device_context->VSGetSamplers(0, ARRAYSIZE(_vs_sampler_states), reinterpret_cast<ID3D11SamplerState **>(_vs_sampler_states));
	_device_context->VSGetShaderResources(0, ARRAYSIZE(_vs_shader_resources), reinterpret_cast<ID3D11ShaderResourceView **>(_vs_shader_resources));

	if (_device_feature_level >= D3D_FEATURE_LEVEL_10_0)
	{
		if (_device_feature_level >= D3D_FEATURE_LEVEL_11_0)
		{
			_hs_num_class_instances = ARRAYSIZE(_hs_class_instances);
			_device_context->HSGetShader(&_hs, reinterpret_cast<ID3D11ClassInstance **>(_hs_class_instances), &_hs_num_class_instances);

			_ds_num_class_instances = ARRAYSIZE(_ds_class_instances);
			_device_context->DSGetShader(&_ds, reinterpret_cast<ID3D11ClassInstance **>(_ds_class_instances), &_ds_num_class_instances);
		}

		_gs_num_class_instances = ARRAYSIZE(_gs_class_instances);
		_device_context->GSGetShader(&_gs, reinterpret_cast<ID3D11ClassInstance **>(_gs_class_instances), &_gs_num_class_instances);
		_device_context->GSGetShaderResources(0, ARRAYSIZE(_gs_shader_resources), reinterpret_cast<ID3D11ShaderResourceView **>(_gs_shader_resources));
	}

	_ps_num_class_instances = ARRAYSIZE(_ps_class_instances);
	_device_context->PSGetShader(&_ps, reinterpret_cast<ID3D11ClassInstance **>(_ps_class_instances), &_ps_num_class_instances);
	_device_context->PSGetConstantBuffers(0, ARRAYSIZE(_ps_constant_buffers), reinterpret_cast<ID3D11Buffer **>(_ps_constant_buffers));
	_device_context->PSGetSamplers(0, ARRAYSIZE(_ps_sampler_states), reinterpret_cast<ID3D11SamplerState **>(_ps_sampler_states));
	_device_context->PSGetShaderResources(0, ARRAYSIZE(_ps_shader_resources), reinterpret_cast<ID3D11ShaderResourceView **>(_ps_shader_resources));

	_device_context->OMGetBlendState(&_om_blend_state, _om_blend_factor, &_om_sample_mask);
	_device_context->OMGetDepthStencilState(&_om_depth_stencil_state, &_om_stencil_ref);
	_device_context->OMGetRenderTargets(ARRAYSIZE(_om_render_targets), reinterpret_cast<ID3D11RenderTargetView **>(_om_render_targets), &_om_depth_stencil);

	if (_device_feature_level >= D3D_FEATURE_LEVEL_10_0)
	{
		_cs_num_class_instances = ARRAYSIZE(_cs_class_instances);
		_device_context->CSGetShader(&_cs, reinterpret_cast<ID3D11ClassInstance **>(_cs_class_instances), &_cs_num_class_instances);
		_device_context->CSGetConstantBuffers(0, ARRAYSIZE(_cs_constant_buffers), reinterpret_cast<ID3D11Buffer **>(_cs_constant_buffers));
		_device_context->CSGetSamplers(0, ARRAYSIZE(_cs_sampler_states), reinterpret_cast<ID3D11SamplerState **>(_cs_sampler_states));
		_device_context->CSGetShaderResources(0, ARRAYSIZE(_cs_shader_resources), reinterpret_cast<ID3D11ShaderResourceView **>(_cs_shader_resources));
		_device_context->CSGetUnorderedAccessViews(0,
			_device_feature_level >= D3D_FEATURE_LEVEL_11_1 ? D3D11_1_UAV_SLOT_COUNT :
			_device_feature_level == D3D_FEATURE_LEVEL_11_0 ? D3D11_PS_CS_UAV_REGISTER_COUNT : D3D11_CS_4_X_UAV_REGISTER_COUNT, reinterpret_cast<ID3D11UnorderedAccessView **>(_cs_unordered_access_views));
	}
}
void reshade::d3d11::state_block::apply_and_release()
{
#if RESHADE_D3D11_STATE_BLOCK_TYPE
	com_ptr<ID3D11DeviceContext1> device_context1;
	if (_state != nullptr && _device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE && SUCCEEDED(_device_context->QueryInterface(&device_context1)))
	{
		device_context1->SwapDeviceContextState(_captured_state.get(), nullptr);

		_captured_state.reset();
		_device_context.reset();
		return;
	}
#endif

	_device_context->IASetPrimitiveTopology(_ia_primitive_topology);
	_device_context->IASetInputLayout(_ia_input_layout.get());

	// With D3D_FEATURE_LEVEL_10_0 or less, the maximum number of IA Vertex Input Slots is 16
	// Starting with D3D_FEATURE_LEVEL_10_1 it is 32
	// See https://docs.microsoft.com/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-intro
	if (_device_feature_level > D3D_FEATURE_LEVEL_10_0)
		_device_context->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, reinterpret_cast<ID3D11Buffer *const *>(_ia_vertex_buffers), _ia_vertex_strides, _ia_vertex_offsets);
	else
		_device_context->IASetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, reinterpret_cast<ID3D11Buffer *const *>(_ia_vertex_buffers), _ia_vertex_strides, _ia_vertex_offsets);

	_device_context->IASetIndexBuffer(_ia_index_buffer.get(), _ia_index_format, _ia_index_offset);

	_device_context->RSSetState(_rs_state.get());
	_device_context->RSSetViewports(_rs_num_viewports, _rs_viewports);
	_device_context->RSSetScissorRects(_rs_num_scissor_rects, _rs_scissor_rects);

	_device_context->VSSetShader(_vs.get(), reinterpret_cast<ID3D11ClassInstance *const *>(_vs_class_instances), _vs_num_class_instances);
	_device_context->VSSetConstantBuffers(0, ARRAYSIZE(_vs_constant_buffers), reinterpret_cast<ID3D11Buffer *const *>(_vs_constant_buffers));
	_device_context->VSSetSamplers(0, ARRAYSIZE(_vs_sampler_states), reinterpret_cast<ID3D11SamplerState **>(_vs_sampler_states));
	_device_context->VSSetShaderResources(0, ARRAYSIZE(_vs_shader_resources), reinterpret_cast<ID3D11ShaderResourceView *const *>(_vs_shader_resources));

	if (_device_feature_level >= D3D_FEATURE_LEVEL_10_0)
	{
		if (_device_feature_level >= D3D_FEATURE_LEVEL_11_0)
		{
			_device_context->HSSetShader(_hs.get(), reinterpret_cast<ID3D11ClassInstance *const *>(_hs_class_instances), _hs_num_class_instances);
			_device_context->DSSetShader(_ds.get(), reinterpret_cast<ID3D11ClassInstance *const *>(_ds_class_instances), _ds_num_class_instances);
		}

		_device_context->GSSetShader(_gs.get(), reinterpret_cast<ID3D11ClassInstance *const *>(_gs_class_instances), _gs_num_class_instances);
		_device_context->GSSetShaderResources(0, ARRAYSIZE(_gs_shader_resources), reinterpret_cast<ID3D11ShaderResourceView *const *>(_gs_shader_resources));
	}

	_device_context->PSSetShader(_ps.get(), reinterpret_cast<ID3D11ClassInstance *const *>(_ps_class_instances), _ps_num_class_instances);
	_device_context->PSSetConstantBuffers(0, ARRAYSIZE(_ps_constant_buffers), reinterpret_cast<ID3D11Buffer *const *>(_ps_constant_buffers));
	_device_context->PSSetSamplers(0, ARRAYSIZE(_ps_sampler_states), reinterpret_cast<ID3D11SamplerState **>(_ps_sampler_states));
	_device_context->PSSetShaderResources(0, ARRAYSIZE(_ps_shader_resources), reinterpret_cast<ID3D11ShaderResourceView *const *>(_ps_shader_resources));

	_device_context->OMSetBlendState(_om_blend_state.get(), _om_blend_factor, _om_sample_mask);
	_device_context->OMSetDepthStencilState(_om_depth_stencil_state.get(), _om_stencil_ref);
	_device_context->OMSetRenderTargets(ARRAYSIZE(_om_render_targets), reinterpret_cast<ID3D11RenderTargetView *const *>(_om_render_targets), _om_depth_stencil.get());

	if (_device_feature_level >= D3D_FEATURE_LEVEL_10_0)
	{
		_device_context->CSSetShader(_cs.get(), reinterpret_cast<ID3D11ClassInstance *const *>(_cs_class_instances), _cs_num_class_instances);
		_device_context->CSSetConstantBuffers(0, ARRAYSIZE(_cs_constant_buffers), reinterpret_cast<ID3D11Buffer *const *>(_cs_constant_buffers));
		_device_context->CSSetSamplers(0, ARRAYSIZE(_cs_sampler_states), reinterpret_cast<ID3D11SamplerState **>(_cs_sampler_states));
		_device_context->CSSetShaderResources(0, ARRAYSIZE(_cs_shader_resources), reinterpret_cast<ID3D11ShaderResourceView *const *>(_cs_shader_resources));
		UINT uav_initial_counts[D3D11_1_UAV_SLOT_COUNT];
		FillMemory(uav_initial_counts, sizeof(uav_initial_counts), -1); // Keep the current offset
		_device_context->CSSetUnorderedAccessViews(0,
			_device_feature_level >= D3D_FEATURE_LEVEL_11_1 ? D3D11_1_UAV_SLOT_COUNT :
			_device_feature_level == D3D_FEATURE_LEVEL_11_0 ? D3D11_PS_CS_UAV_REGISTER_COUNT : D3D11_CS_4_X_UAV_REGISTER_COUNT, reinterpret_cast<ID3D11UnorderedAccessView *const *>(_cs_unordered_access_views), uav_initial_counts);
	}

	com_ptr<ID3D11Device> device;
	_device_context->GetDevice(&device);

	*this = state_block(std::move(device));
}
