/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d11_impl_device.hpp"
#include "d3d11_impl_device_context.hpp"
#include "d3d11_impl_type_convert.hpp"
#include "dll_log.hpp"
#include <cstring> // std::memcpy, std::strlen
#include <algorithm> // std::find
#include <utf8/unchecked.h>

void reshade::d3d11::pipeline_impl::apply(ID3D11DeviceContext *ctx, api::pipeline_stage stages) const
{
	if ((stages & api::pipeline_stage::vertex_shader) != 0)
		ctx->VSSetShader(vs.get(), nullptr, 0);
	if ((stages & api::pipeline_stage::hull_shader) != 0)
		ctx->HSSetShader(hs.get(), nullptr, 0);
	if ((stages & api::pipeline_stage::domain_shader) != 0)
		ctx->DSSetShader(ds.get(), nullptr, 0);
	if ((stages & api::pipeline_stage::geometry_shader) != 0)
		ctx->GSSetShader(gs.get(), nullptr, 0);
	if ((stages & api::pipeline_stage::pixel_shader) != 0)
		ctx->PSSetShader(ps.get(), nullptr, 0);

	if ((stages & api::pipeline_stage::input_assembler) != 0)
	{
		ctx->IASetInputLayout(input_layout.get());
		if (topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
			ctx->IASetPrimitiveTopology(topology);
	}

	if ((stages & api::pipeline_stage::rasterizer) != 0)
		ctx->RSSetState(rasterizer_state.get());

	if ((stages & api::pipeline_stage::depth_stencil) != 0)
		ctx->OMSetDepthStencilState(depth_stencil_state.get(), stencil_reference_value);

	if ((stages & api::pipeline_stage::output_merger) != 0)
		ctx->OMSetBlendState(blend_state.get(), blend_constant, sample_mask);
}

reshade::d3d11::command_list_impl::command_list_impl(device_impl *device, ID3D11CommandList *cmd_list) :
	api_object_impl(cmd_list),
	_device_impl(device)
{
}

reshade::api::device *reshade::d3d11::command_list_impl::get_device()
{
	return _device_impl;
}

void reshade::d3d11::device_context_impl::barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states)
{
	uint32_t transitions_away_from_shader_resource_usage = 0;
	temp_mem<ID3D11Resource *> shader_resource_resources(count);
	uint32_t transitions_away_from_unordered_access_usage = 0;
	temp_mem<ID3D11Resource *> unordered_access_resources(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		if ((old_states[i] & api::resource_usage::shader_resource) != 0 && (new_states[i] & api::resource_usage::shader_resource) == 0 &&
			// Ignore transitions to copy source or destination states, since those are not affected by the current SRV bindings
			(new_states[i] & (api::resource_usage::depth_stencil | api::resource_usage::render_target)) != 0)
			shader_resource_resources[transitions_away_from_shader_resource_usage++] = reinterpret_cast<ID3D11Resource *>(resources[i].handle);
		if ((old_states[i] & api::resource_usage::unordered_access) != 0 && (new_states[i] & api::resource_usage::unordered_access) == 0)
			unordered_access_resources[transitions_away_from_unordered_access_usage++] = reinterpret_cast<ID3D11Resource *>(resources[i].handle);
	}

	if (transitions_away_from_shader_resource_usage != 0)
	{
#if 1
#define UNBIND_SHADER_RESOURCE_VIEWS(stage) \
		bool update_##stage = false; \
		com_ptr<ID3D11ShaderResourceView> srvs_##stage[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]; \
		_orig->stage##GetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, reinterpret_cast<ID3D11ShaderResourceView **>(srvs_##stage)); \
		for (UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i) \
		{ \
			if (srvs_##stage[i] != nullptr) \
			{ \
				com_ptr<ID3D11Resource> resource; \
				srvs_##stage[i]->GetResource(&resource); \
				if (std::find(shader_resource_resources.p, shader_resource_resources.p + transitions_away_from_shader_resource_usage, resource) != shader_resource_resources.p + transitions_away_from_shader_resource_usage) \
				{ \
					srvs_##stage[i].reset(); \
					update_##stage = true; \
				} \
			} \
		} \
		if (update_##stage) \
			_orig->stage##SetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, reinterpret_cast<ID3D11ShaderResourceView *const *>(srvs_##stage));
#else
#define UNBIND_SHADER_RESOURCE_VIEWS(stage) \
		ID3D11ShaderResourceView *null_srvs_##stage[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {}; \
		_orig->stage##SetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, null_srvs_##stage);
#endif

		UNBIND_SHADER_RESOURCE_VIEWS(VS);
#if 0
		// Not currently covered by state block (see d3d11_impl_state_block.cpp)
		UNBIND_SHADER_RESOURCE_VIEWS(HS);
		UNBIND_SHADER_RESOURCE_VIEWS(DS);
#endif
		UNBIND_SHADER_RESOURCE_VIEWS(GS);
		UNBIND_SHADER_RESOURCE_VIEWS(PS);
		UNBIND_SHADER_RESOURCE_VIEWS(CS);

#undef UNBIND_SHADER_RESOURCE_VIEWS
	}
	if (transitions_away_from_unordered_access_usage != 0)
	{
		const D3D_FEATURE_LEVEL feature_level = _device_impl->_orig->GetFeatureLevel();
		const UINT max_uav_bindings =
			feature_level >= D3D_FEATURE_LEVEL_11_1 ? D3D11_1_UAV_SLOT_COUNT :
			feature_level == D3D_FEATURE_LEVEL_11_0 ? D3D11_PS_CS_UAV_REGISTER_COUNT :
			feature_level >= D3D_FEATURE_LEVEL_10_0 ? D3D11_CS_4_X_UAV_REGISTER_COUNT : 0;

#if 1
#define UNBIND_UNORDERED_ACCESS_VIEWS(stage) \
		bool update_##stage = false; \
		com_ptr<ID3D11UnorderedAccessView> uavs_##stage[D3D11_1_UAV_SLOT_COUNT]; \
		_orig->stage##GetUnorderedAccessViews(0, max_uav_bindings, reinterpret_cast<ID3D11UnorderedAccessView **>(uavs_##stage)); \
		for (UINT i = 0; i < max_uav_bindings; ++i) \
		{ \
			if (uavs_##stage[i] != nullptr) \
			{ \
				com_ptr<ID3D11Resource> resource; \
				uavs_##stage[i]->GetResource(&resource); \
				if (std::find(unordered_access_resources.p, unordered_access_resources.p + transitions_away_from_unordered_access_usage, resource) != unordered_access_resources.p + transitions_away_from_unordered_access_usage) \
				{ \
					uavs_##stage[i].reset(); \
					update_##stage = true; \
				} \
			} \
		} \
		if (update_##stage) \
			_orig->stage##SetUnorderedAccessViews(0, max_uav_bindings, reinterpret_cast<ID3D11UnorderedAccessView *const *>(uavs_##stage), nullptr);
#else
#define UNBIND_UNORDERED_ACCESS_VIEWS(stage) \
		ID3D11UnorderedAccessView *null_uavs_##stage[D3D11_1_UAV_SLOT_COUNT] = {}; \
		_orig->CSSetUnorderedAccessViews(0, max_uav_bindings, null_uavs_##stage, nullptr);
#endif

		UNBIND_UNORDERED_ACCESS_VIEWS(CS);

#undef UNBIND_UNORDERED_ACCESS_VIEWS
	}
}

void reshade::d3d11::device_context_impl::begin_render_pass(uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds)
{
	assert(count <= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);

	temp_mem<api::resource_view, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> rtv_handles(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		rtv_handles[i] = rts[i].view;

		if (rts[i].load_op == api::render_pass_load_op::clear)
			_orig->ClearRenderTargetView(reinterpret_cast<ID3D11RenderTargetView *>(rtv_handles[i].handle), rts[i].clear_color);
	}

	api::resource_view depth_stencil_handle = {};
	if (ds != nullptr && ds->view != 0)
	{
		depth_stencil_handle = ds->view;

		if (const UINT clear_flags = (ds->depth_load_op == api::render_pass_load_op::clear ? D3D11_CLEAR_DEPTH : 0u) | (ds->stencil_load_op == api::render_pass_load_op::clear ? D3D11_CLEAR_STENCIL : 0u))
			_orig->ClearDepthStencilView(reinterpret_cast<ID3D11DepthStencilView *>(depth_stencil_handle.handle), clear_flags, ds->clear_depth, ds->clear_stencil);
	}

	bind_render_targets_and_depth_stencil(count, rtv_handles.p, depth_stencil_handle);
}
void reshade::d3d11::device_context_impl::end_render_pass()
{
	// Reset render targets
	_orig->OMSetRenderTargets(0, nullptr, nullptr);
}
void reshade::d3d11::device_context_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	assert(count <= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);

#ifndef _WIN64
	temp_mem<ID3D11RenderTargetView *, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> rtv_ptrs_mem(count);
	for (uint32_t i = 0; i < count; ++i)
		rtv_ptrs_mem[i] = reinterpret_cast<ID3D11RenderTargetView *>(rtvs[i].handle);
	const auto rtv_ptrs = rtv_ptrs_mem.p;
#else
	const auto rtv_ptrs = reinterpret_cast<ID3D11RenderTargetView *const *>(rtvs);
#endif

	_orig->OMSetRenderTargets(count, rtv_ptrs, reinterpret_cast<ID3D11DepthStencilView *>(dsv.handle));
}

void reshade::d3d11::device_context_impl::bind_pipeline(api::pipeline_stage stages, api::pipeline pipeline)
{
	if (pipeline.handle & 1)
	{
		// This is a pipeline handle created with 'device_impl::create_pipeline', which can only contain graphics stages
		assert((stages & api::pipeline_stage::all_graphics) != 0);
		reinterpret_cast<pipeline_impl *>(pipeline.handle ^ 1)->apply(_orig, stages);
		return;
	}

	switch (stages)
	{
	case api::pipeline_stage::vertex_shader:
		_orig->VSSetShader(reinterpret_cast<ID3D11VertexShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_stage::hull_shader:
		_orig->HSSetShader(reinterpret_cast<ID3D11HullShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_stage::domain_shader:
		_orig->DSSetShader(reinterpret_cast<ID3D11DomainShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_stage::geometry_shader:
	case api::pipeline_stage::geometry_shader | api::pipeline_stage::stream_output:
		_orig->GSSetShader(reinterpret_cast<ID3D11GeometryShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_stage::pixel_shader:
		_orig->PSSetShader(reinterpret_cast<ID3D11PixelShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_stage::compute_shader:
		_orig->CSSetShader(reinterpret_cast<ID3D11ComputeShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_stage::input_assembler:
		_orig->IASetInputLayout(reinterpret_cast<ID3D11InputLayout *>(pipeline.handle));
		break;
	case api::pipeline_stage::rasterizer:
		_orig->RSSetState(reinterpret_cast<ID3D11RasterizerState *>(pipeline.handle));
		break;
	case api::pipeline_stage::depth_stencil:
		_orig->OMSetDepthStencilState(reinterpret_cast<ID3D11DepthStencilState *>(pipeline.handle), 0);
		break;
	case api::pipeline_stage::output_merger:
		_orig->OMSetBlendState(reinterpret_cast<ID3D11BlendState *>(pipeline.handle), nullptr, D3D11_DEFAULT_SAMPLE_MASK);
		break;
	case api::pipeline_stage::all:
		if (pipeline == 0)
		{
			_orig->ClearState();
			break;
		}
		[[fallthrough]];
	default:
		assert(false);
		break;
	}
}
void reshade::d3d11::device_context_impl::bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::dynamic_state::primitive_topology:
			_orig->IASetPrimitiveTopology(convert_primitive_topology(static_cast<api::primitive_topology>(values[i])));
			break;
		case api::dynamic_state::blend_constant:
			if (const float blend_constant[4] = { ((values[i]) & 0xFF) / 255.0f, ((values[i] >> 4) & 0xFF) / 255.0f, ((values[i] >> 8) & 0xFF) / 255.0f, ((values[i] >> 12) & 0xFF) / 255.0f };
				i + 1 < count &&
				states[i + 1] == api::dynamic_state::sample_mask)
			{
				com_ptr<ID3D11BlendState> state;
				_orig->OMGetBlendState(&state, nullptr, nullptr);
				_orig->OMSetBlendState(state.get(), blend_constant, values[i + 1]);
				i += 1;
			}
			else
			{
				com_ptr<ID3D11BlendState> state;
				UINT sample_mask = D3D11_DEFAULT_SAMPLE_MASK;
				_orig->OMGetBlendState(&state, nullptr, &sample_mask);
				_orig->OMSetBlendState(state.get(), blend_constant, sample_mask);
			}
			break;
		case api::dynamic_state::sample_mask:
			{
				com_ptr<ID3D11BlendState> state;
				float blend_constant[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				_orig->OMGetBlendState(&state, blend_constant, nullptr);
				_orig->OMSetBlendState(state.get(), blend_constant, values[i]);
			}
			break;
		case api::dynamic_state::front_stencil_reference_value:
			{
				com_ptr<ID3D11DepthStencilState> state;
				_orig->OMGetDepthStencilState(&state, nullptr);
				_orig->OMSetDepthStencilState(state.get(), values[i]);
			}
			break;
		case api::dynamic_state::back_stencil_reference_value:
			// Ignore
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::d3d11::device_context_impl::bind_viewports(uint32_t first, uint32_t count, const api::viewport *viewports)
{
	if (first != 0)
	{
		assert(false);
		return;
	}

	_orig->RSSetViewports(count, reinterpret_cast<const D3D11_VIEWPORT *>(viewports));
}
void reshade::d3d11::device_context_impl::bind_scissor_rects(uint32_t first, uint32_t count, const api::rect *rects)
{
	if (first != 0)
	{
		assert(false);
		return;
	}

	_orig->RSSetScissorRects(count, reinterpret_cast<const D3D11_RECT *>(rects));
}

void reshade::d3d11::device_context_impl::bind_samplers(api::shader_stage stages, uint32_t first, uint32_t count, const api::sampler *samplers)
{
	assert(count <= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT);

#ifndef _WIN64
	temp_mem<ID3D11SamplerState *, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> sampler_ptrs_mem(count);
	for (uint32_t i = 0; i < count; ++i)
		sampler_ptrs_mem[i] = reinterpret_cast<ID3D11SamplerState *>(samplers[i].handle);
	const auto sampler_ptrs = sampler_ptrs_mem.p;
#else
	const auto sampler_ptrs = reinterpret_cast<ID3D11SamplerState *const *>(samplers);
#endif

	if ((stages & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetSamplers(first, count, sampler_ptrs);
	if ((stages & api::shader_stage::hull) == api::shader_stage::hull)
		_orig->HSSetSamplers(first, count, sampler_ptrs);
	if ((stages & api::shader_stage::domain) == api::shader_stage::domain)
		_orig->DSSetSamplers(first, count, sampler_ptrs);
	if ((stages & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetSamplers(first, count, sampler_ptrs);
	if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetSamplers(first, count, sampler_ptrs);
	if ((stages & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetSamplers(first, count, sampler_ptrs);
}
void reshade::d3d11::device_context_impl::bind_shader_resource_views(api::shader_stage stages, uint32_t first, uint32_t count, const api::resource_view *views)
{
	assert(count <= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);

#ifndef _WIN64
	temp_mem<ID3D11ShaderResourceView *, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> view_ptrs_mem(count);
	for (uint32_t i = 0; i < count; ++i)
		view_ptrs_mem[i] = reinterpret_cast<ID3D11ShaderResourceView *>(views[i].handle);
	const auto view_ptrs = view_ptrs_mem.p;
#else
	const auto view_ptrs = reinterpret_cast<ID3D11ShaderResourceView *const *>(views);
#endif

	if ((stages & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetShaderResources(first, count, view_ptrs);
	if ((stages & api::shader_stage::hull) == api::shader_stage::hull)
		_orig->HSSetShaderResources(first, count, view_ptrs);
	if ((stages & api::shader_stage::domain) == api::shader_stage::domain)
		_orig->DSSetShaderResources(first, count, view_ptrs);
	if ((stages & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetShaderResources(first, count, view_ptrs);
	if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetShaderResources(first, count, view_ptrs);
	if ((stages & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetShaderResources(first, count, view_ptrs);
}
void reshade::d3d11::device_context_impl::bind_unordered_access_views(api::shader_stage stages, uint32_t first, uint32_t count, const api::resource_view *views)
{
	assert(count <= D3D11_1_UAV_SLOT_COUNT);

#ifndef _WIN64
	temp_mem<ID3D11UnorderedAccessView *, D3D11_1_UAV_SLOT_COUNT> view_ptrs_mem(count);
	for (uint32_t i = 0; i < count; ++i)
		view_ptrs_mem[i] = reinterpret_cast<ID3D11UnorderedAccessView *>(views[i].handle);
	const auto view_ptrs = view_ptrs_mem.p;
#else
	const auto view_ptrs = reinterpret_cast<ID3D11UnorderedAccessView *const *>(views);
#endif

	if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr, first, count, view_ptrs, nullptr);
	if ((stages & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetUnorderedAccessViews(first, count, view_ptrs, nullptr);
}
void reshade::d3d11::device_context_impl::bind_constant_buffers(api::shader_stage stages, uint32_t first, uint32_t count, const api::buffer_range *buffer_ranges)
{
	assert(count <= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

	bool whole_range = true;
	temp_mem<ID3D11Buffer *, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> buffer_ptrs(count);
	temp_mem<UINT, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> first_constant(count);
	temp_mem<UINT, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> constant_count(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		buffer_ptrs[i] = reinterpret_cast<ID3D11Buffer *>(buffer_ranges[i].buffer.handle);

		assert(buffer_ranges[i].offset <= std::numeric_limits<UINT>::max() && (buffer_ranges[i].offset % (16 * 16)) == 0);
		first_constant[i] = static_cast<UINT>(buffer_ranges[i].offset / 16);

		if (buffer_ranges[i].size != UINT64_MAX)
		{
			whole_range = false;
			assert(buffer_ranges[i].size <= std::numeric_limits<UINT>::max() && (buffer_ranges[i].size % (16 * 16)) == 0);
			constant_count[i] = static_cast<UINT>(buffer_ranges[i].size / 16);
		}
		else
		{
			if (buffer_ptrs[i] != nullptr)
			{
				D3D11_BUFFER_DESC desc;
				buffer_ptrs[i]->GetDesc(&desc);
				constant_count[i] = desc.ByteWidth / 16;
			}
			else
			{
				constant_count[i] = 0;
			}
		}
	}

	com_ptr<ID3D11DeviceContext1> context1;
	if (whole_range ||
		FAILED(_orig->QueryInterface(&context1)))
	{
		if ((stages & api::shader_stage::vertex) == api::shader_stage::vertex)
			_orig->VSSetConstantBuffers(first, count, buffer_ptrs.p);
		if ((stages & api::shader_stage::hull) == api::shader_stage::hull)
			_orig->HSSetConstantBuffers(first, count, buffer_ptrs.p);
		if ((stages & api::shader_stage::domain) == api::shader_stage::domain)
			_orig->DSSetConstantBuffers(first, count, buffer_ptrs.p);
		if ((stages & api::shader_stage::geometry) == api::shader_stage::geometry)
			_orig->GSSetConstantBuffers(first, count, buffer_ptrs.p);
		if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
			_orig->PSSetConstantBuffers(first, count, buffer_ptrs.p);
		if ((stages & api::shader_stage::compute) == api::shader_stage::compute)
			_orig->CSSetConstantBuffers(first, count, buffer_ptrs.p);
	}
	else
	{
		if ((stages & api::shader_stage::vertex) == api::shader_stage::vertex)
			context1->VSSetConstantBuffers1(first, count, buffer_ptrs.p, first_constant.p, constant_count.p);
		if ((stages & api::shader_stage::hull) == api::shader_stage::hull)
			context1->HSSetConstantBuffers1(first, count, buffer_ptrs.p, first_constant.p, constant_count.p);
		if ((stages & api::shader_stage::domain) == api::shader_stage::domain)
			context1->DSSetConstantBuffers1(first, count, buffer_ptrs.p, first_constant.p, constant_count.p);
		if ((stages & api::shader_stage::geometry) == api::shader_stage::geometry)
			context1->GSSetConstantBuffers1(first, count, buffer_ptrs.p, first_constant.p, constant_count.p);
		if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
			context1->PSSetConstantBuffers1(first, count, buffer_ptrs.p, first_constant.p, constant_count.p);
		if ((stages & api::shader_stage::compute) == api::shader_stage::compute)
			context1->CSSetConstantBuffers1(first, count, buffer_ptrs.p, first_constant.p, constant_count.p);
	}
}

void reshade::d3d11::device_context_impl::push_constants(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const void *values)
{
	if (count == 0)
		return;

	count += first;

	if (count > _push_constants_data.size())
	{
		_push_constants_data.resize(count);

		// Enlarge push constant buffer to fit new requirement
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = ((count * sizeof(uint32_t)) + 15) & ~15; // Align to 16 bytes
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		_push_constants.reset();

		if (FAILED(_device_impl->_orig->CreateBuffer(&desc, nullptr, &_push_constants)))
		{
			_push_constants_data.clear();

			LOG(ERROR) << "Failed to create push constant buffer!";
			return;
		}

		_device_impl->set_resource_name({ reinterpret_cast<uintptr_t>(_push_constants.get()) }, "Push constants");
	}

	std::memcpy(_push_constants_data.data() + first, values, (count - first) * sizeof(uint32_t));

	ID3D11Buffer *const push_constants = _push_constants.get();

	// Discard the buffer so driver can return a new memory region to avoid stalls
	if (D3D11_MAPPED_SUBRESOURCE mapped;
		SUCCEEDED(_orig->Map(push_constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		std::memcpy(mapped.pData, _push_constants_data.data(), _push_constants_data.size() * sizeof(uint32_t));
		_orig->Unmap(push_constants, 0);
	}

	uint32_t push_constants_slot = 0;
	if (layout != 0)
	{
		const api::descriptor_range &range = reinterpret_cast<pipeline_layout_impl *>(layout.handle)->ranges[layout_param];

		push_constants_slot = range.dx_register_index;
		stages &= range.visibility;
	}

	if ((stages & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stages & api::shader_stage::hull) == api::shader_stage::hull)
		_orig->HSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stages & api::shader_stage::domain) == api::shader_stage::domain)
		_orig->DSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stages & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stages & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetConstantBuffers(push_constants_slot, 1, &push_constants);
	if ((stages & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetConstantBuffers(push_constants_slot, 1, &push_constants);
}
void reshade::d3d11::device_context_impl::push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_table_update &update)
{
	assert(update.table == 0 && update.array_offset == 0);

	uint32_t first = update.binding;
	if (layout != 0)
	{
		const api::descriptor_range &range = reinterpret_cast<pipeline_layout_impl *>(layout.handle)->ranges[layout_param];

		assert(update.binding >= range.binding);
		first -= range.binding;
		first += range.dx_register_index;
		stages &= range.visibility;
	}

	switch (update.type)
	{
	case api::descriptor_type::sampler:
		bind_samplers(stages, first, update.count, static_cast<const api::sampler *>(update.descriptors));
		break;
	case api::descriptor_type::buffer_shader_resource_view:
	case api::descriptor_type::texture_shader_resource_view:
		bind_shader_resource_views(stages, first, update.count, static_cast<const api::resource_view *>(update.descriptors));
		break;
	case api::descriptor_type::buffer_unordered_access_view:
	case api::descriptor_type::texture_unordered_access_view:
		bind_unordered_access_views(stages, first, update.count, static_cast<const api::resource_view *>(update.descriptors));
		break;
	case api::descriptor_type::constant_buffer:
		bind_constant_buffers(stages, first, update.count, static_cast<const api::buffer_range *>(update.descriptors));
		break;
	default:
		assert(false);
		break;
	}
}
void reshade::d3d11::device_context_impl::bind_descriptor_tables(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_table *tables)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto table_impl = reinterpret_cast<const descriptor_table_impl *>(tables[i].handle);

		push_descriptors(
			stages,
			layout,
			first + i,
			api::descriptor_table_update { {}, table_impl->base_binding, 0, table_impl->count, table_impl->type, table_impl->descriptors.data() });
	}
}

void reshade::d3d11::device_context_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	assert(offset <= std::numeric_limits<UINT>::max());
	assert(buffer == 0 || index_size == 2 || index_size == 4);

	_orig->IASetIndexBuffer(reinterpret_cast<ID3D11Buffer *>(buffer.handle), index_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, static_cast<UINT>(offset));
}
void reshade::d3d11::device_context_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	assert(count <= D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);

#ifndef _WIN64
	temp_mem<ID3D11Buffer *, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> buffer_ptrs_mem(count);
	for (uint32_t i = 0; i < count; ++i)
		buffer_ptrs_mem[i] = reinterpret_cast<ID3D11Buffer *>(buffers[i].handle);
	const auto buffer_ptrs = buffer_ptrs_mem.p;
#else
	const auto buffer_ptrs = reinterpret_cast<ID3D11Buffer *const *>(buffers);
#endif

	temp_mem<UINT, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> offsets_32(count);
	for (uint32_t i = 0; i < count; ++i)
		offsets_32[i] = static_cast<UINT>(offsets[i]);

	_orig->IASetVertexBuffers(first, count, buffer_ptrs, strides, offsets_32.p);
}
void reshade::d3d11::device_context_impl::bind_stream_output_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint64_t *, const api::resource *, const uint64_t *)
{
	assert(first == 0 && count <= D3D11_SO_BUFFER_SLOT_COUNT);

#ifndef _WIN64
	temp_mem<ID3D11Buffer *, D3D11_SO_BUFFER_SLOT_COUNT> buffer_ptrs_mem(count);
	for (uint32_t i = 0; i < count; ++i)
		buffer_ptrs_mem[i] = reinterpret_cast<ID3D11Buffer *>(buffers[i].handle);
	const auto buffer_ptrs = buffer_ptrs_mem.p;
#else
	const auto buffer_ptrs = reinterpret_cast<ID3D11Buffer *const *>(buffers);
#endif

	temp_mem<UINT, D3D11_SO_BUFFER_SLOT_COUNT> offsets_32(count);
	for (uint32_t i = 0; i < count; ++i)
		offsets_32[i] = static_cast<UINT>(offsets[i]);

	_orig->SOSetTargets(count, buffer_ptrs, offsets_32.p);
}

void reshade::d3d11::device_context_impl::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	_orig->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}
void reshade::d3d11::device_context_impl::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_orig->DrawIndexedInstanced(index_count, instance_count, first_index, vertex_offset, first_instance);
}
void reshade::d3d11::device_context_impl::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	_orig->Dispatch(group_count_x, group_count_y, group_count_z);
}
void reshade::d3d11::device_context_impl::dispatch_mesh(uint32_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d11::device_context_impl::dispatch_rays(api::resource, uint64_t, uint64_t, api::resource, uint64_t, uint64_t, uint64_t, api::resource, uint64_t, uint64_t, uint64_t, api::resource, uint64_t, uint64_t, uint64_t, uint32_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d11::device_context_impl::draw_or_dispatch_indirect(api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	assert(offset <= std::numeric_limits<UINT>::max());

	switch (type)
	{
	case api::indirect_command::draw:
		for (UINT i = 0; i < draw_count; ++i)
			_orig->DrawInstancedIndirect(reinterpret_cast<ID3D11Buffer *>(buffer.handle), static_cast<UINT>(offset) + i * stride);
		break;
	case api::indirect_command::draw_indexed:
		for (UINT i = 0; i < draw_count; ++i)
			_orig->DrawIndexedInstancedIndirect(reinterpret_cast<ID3D11Buffer *>(buffer.handle), static_cast<UINT>(offset) + i * stride);
		break;
	case api::indirect_command::dispatch:
		for (UINT i = 0; i < draw_count; ++i)
			_orig->DispatchIndirect(reinterpret_cast<ID3D11Buffer *>(buffer.handle), static_cast<UINT>(offset) + i * stride);
		break;
	default:
		assert(false);
		break;
	}
}

void reshade::d3d11::device_context_impl::copy_resource(api::resource src, api::resource dst)
{
	assert(src != 0 && dst != 0);

	_orig->CopyResource(reinterpret_cast<ID3D11Resource *>(dst.handle), reinterpret_cast<ID3D11Resource *>(src.handle));
}
void reshade::d3d11::device_context_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(src != 0 && dst != 0);

	if (UINT64_MAX == size)
	{
		D3D11_BUFFER_DESC desc;
		reinterpret_cast<ID3D11Buffer *>(src.handle)->GetDesc(&desc);
		size = desc.ByteWidth;
	}

	assert(src_offset <= std::numeric_limits<UINT>::max() && dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	const D3D11_BOX src_box = { static_cast<UINT>(src_offset), 0, 0, static_cast<UINT>(src_offset + size), 1, 1 };

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D11Resource *>(dst.handle), 0, static_cast<UINT>(dst_offset), 0, 0,
		reinterpret_cast<ID3D11Resource *>(src.handle), 0, &src_box);
}
void reshade::d3d11::device_context_impl::copy_buffer_to_texture(api::resource, uint64_t, uint32_t, uint32_t, api::resource, uint32_t, const api::subresource_box *)
{
	assert(false);
}
void reshade::d3d11::device_context_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, const api::subresource_box *dst_box, api::filter_mode)
{
	assert(src != 0 && dst != 0);
	// Blit between different region dimensions is not supported
	assert((src_box == nullptr && dst_box == nullptr) || (src_box != nullptr && dst_box != nullptr && dst_box->width() == src_box->width() && dst_box->height() == src_box->height() && dst_box->depth() == src_box->depth()));

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D11Resource *>(dst.handle), dst_subresource, dst_box != nullptr ? dst_box->left : 0, dst_box != nullptr ? dst_box->top : 0, dst_box != nullptr ? dst_box->front : 0,
		reinterpret_cast<ID3D11Resource *>(src.handle), src_subresource, reinterpret_cast<const D3D11_BOX *>(src_box));
}
void reshade::d3d11::device_context_impl::copy_texture_to_buffer(api::resource, uint32_t, const api::subresource_box *, api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d11::device_context_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, int32_t dst_x, int32_t dst_y, int32_t dst_z, api::format format)
{
	assert(src != 0 && dst != 0);
	assert(src_box == nullptr && dst_x == 0 && dst_y == 0 && dst_z == 0);

	_orig->ResolveSubresource(
		reinterpret_cast<ID3D11Resource *>(dst.handle), dst_subresource,
		reinterpret_cast<ID3D11Resource *>(src.handle), src_subresource, convert_format(format));
}

void reshade::d3d11::device_context_impl::clear_depth_stencil_view(api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const api::rect *)
{
	assert(dsv != 0 && rect_count == 0); // Clearing rectangles is not supported

	_orig->ClearDepthStencilView(
		reinterpret_cast<ID3D11DepthStencilView *>(dsv.handle),
		(depth != nullptr ? D3D11_CLEAR_DEPTH : 0u) | (stencil != nullptr ? D3D11_CLEAR_STENCIL : 0u), depth != nullptr ? *depth : 0.0f, stencil != nullptr ? *stencil : 0);
}
void reshade::d3d11::device_context_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t rect_count, const api::rect *)
{
	assert(rtv != 0 && rect_count == 0); // Clearing rectangles is not supported

	_orig->ClearRenderTargetView(reinterpret_cast<ID3D11RenderTargetView *>(rtv.handle), color);
}
void reshade::d3d11::device_context_impl::clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4], uint32_t rect_count, const api::rect *)
{
	assert(uav != 0 && rect_count == 0); // Clearing rectangles is not supported

	_orig->ClearUnorderedAccessViewUint(reinterpret_cast<ID3D11UnorderedAccessView *>(uav.handle), values);
}
void reshade::d3d11::device_context_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4], uint32_t rect_count, const api::rect *)
{
	assert(uav != 0 && rect_count == 0); // Clearing rectangles is not supported

	_orig->ClearUnorderedAccessViewFloat(reinterpret_cast<ID3D11UnorderedAccessView *>(uav.handle), values);
}

void reshade::d3d11::device_context_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv != 0);

	_orig->GenerateMips(reinterpret_cast<ID3D11ShaderResourceView *>(srv.handle));
}

void reshade::d3d11::device_context_impl::begin_query(api::query_heap heap, api::query_type, uint32_t index)
{
	assert(heap != 0);

	_orig->Begin(reinterpret_cast<query_heap_impl *>(heap.handle)->queries[index].get());
}
void reshade::d3d11::device_context_impl::end_query(api::query_heap heap, api::query_type, uint32_t index)
{
	assert(heap != 0);

	_orig->End(reinterpret_cast<query_heap_impl *>(heap.handle)->queries[index].get());
}
void reshade::d3d11::device_context_impl::copy_query_heap_results(api::query_heap, api::query_type, uint32_t, uint32_t, api::resource, uint64_t, uint32_t)
{
	assert(false);
}

void reshade::d3d11::device_context_impl::copy_acceleration_structure(api::resource_view, api::resource_view, api::acceleration_structure_copy_mode)
{
	assert(false);
}
void reshade::d3d11::device_context_impl::build_acceleration_structure(api::acceleration_structure_type, api::acceleration_structure_build_flags, uint32_t, const api::acceleration_structure_build_input *, api::resource, uint64_t, api::resource_view, api::resource_view, api::acceleration_structure_build_mode)
{
	assert(false);
}

void reshade::d3d11::device_context_impl::begin_debug_event(const char *label, const float[4])
{
	assert(label != nullptr);

	if (_annotations == nullptr)
		return;

	const size_t label_len = std::strlen(label);
	std::wstring label_wide;
	label_wide.reserve(label_len + 1);
	utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

	_annotations->BeginEvent(label_wide.c_str());
}
void reshade::d3d11::device_context_impl::end_debug_event()
{
	if (_annotations == nullptr)
		return;

	_annotations->EndEvent();
}
void reshade::d3d11::device_context_impl::insert_debug_marker(const char *label, const float[4])
{
	assert(label != nullptr);

	if (_annotations == nullptr)
		return;

	const size_t label_len = std::strlen(label);
	std::wstring label_wide;
	label_wide.reserve(label_len + 1);
	utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

	_annotations->SetMarker(label_wide.c_str());
}
