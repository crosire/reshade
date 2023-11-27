/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON >= 2

#include "d3d12_device.hpp"
#include "d3d12_pipeline_library.hpp"
#include "d3d12_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "com_utils.hpp"
#include "hook_manager.hpp"
#include "addon_manager.hpp"

using reshade::d3d12::to_handle;

HRESULT STDMETHODCALLTYPE ID3D12PipelineLibrary_LoadGraphicsPipeline(ID3D12PipelineLibrary *pPipelineLibrary, LPCWSTR pName, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	// Do not invoke 'create_pipeline' event, since it is not possible to modify the pipeline

	const HRESULT hr = reshade::hooks::call(ID3D12PipelineLibrary_LoadGraphicsPipeline, reshade::hooks::vtable_from_instance(pPipelineLibrary) + 9)(pPipelineLibrary, pName, pDesc, riid, ppPipelineState);
	if (SUCCEEDED(hr))
	{
		assert(pDesc != nullptr && ppPipelineState != nullptr);

		com_ptr<ID3D12Device> device;
		pPipelineLibrary->GetDevice(IID_PPV_ARGS(&device));

		const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get());
		if (device_proxy != nullptr)
		{
			if (riid == __uuidof(ID3D12PipelineState))
			{
				const auto pipeline = static_cast<ID3D12PipelineState *>(*ppPipelineState);

				const D3D12_GRAPHICS_PIPELINE_STATE_DESC &internal_desc = *pDesc;

				reshade::api::shader_desc vs_desc = reshade::d3d12::convert_shader_desc(internal_desc.VS);
				reshade::api::shader_desc ps_desc = reshade::d3d12::convert_shader_desc(internal_desc.PS);
				reshade::api::shader_desc ds_desc = reshade::d3d12::convert_shader_desc(internal_desc.DS);
				reshade::api::shader_desc hs_desc = reshade::d3d12::convert_shader_desc(internal_desc.HS);
				reshade::api::shader_desc gs_desc = reshade::d3d12::convert_shader_desc(internal_desc.GS);
				auto stream_output_desc = reshade::d3d12::convert_stream_output_desc(internal_desc.StreamOutput);
				auto blend_desc = reshade::d3d12::convert_blend_desc(internal_desc.BlendState);
				auto rasterizer_desc = reshade::d3d12::convert_rasterizer_desc(internal_desc.RasterizerState);
				auto depth_stencil_desc = reshade::d3d12::convert_depth_stencil_desc(internal_desc.DepthStencilState);
				auto input_layout = reshade::d3d12::convert_input_layout_desc(internal_desc.InputLayout.NumElements, internal_desc.InputLayout.pInputElementDescs);
				reshade::api::primitive_topology topology = reshade::d3d12::convert_primitive_topology_type(internal_desc.PrimitiveTopologyType);

				reshade::api::format depth_stencil_format = reshade::d3d12::convert_format(internal_desc.DSVFormat);
				assert(internal_desc.NumRenderTargets <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
				reshade::api::format render_target_formats[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
				for (UINT i = 0; i < internal_desc.NumRenderTargets; ++i)
					render_target_formats[i] = reshade::d3d12::convert_format(internal_desc.RTVFormats[i]);

				uint32_t sample_mask = internal_desc.SampleMask;
				uint32_t sample_count = internal_desc.SampleDesc.Count;

				std::vector<reshade::api::dynamic_state> dynamic_states = {
					reshade::api::dynamic_state::primitive_topology,
					reshade::api::dynamic_state::blend_constant,
					reshade::api::dynamic_state::front_stencil_reference_value,
					reshade::api::dynamic_state::back_stencil_reference_value
				};
				if (internal_desc.Flags & D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS)
				{
					dynamic_states.push_back(reshade::api::dynamic_state::depth_bias);
					dynamic_states.push_back(reshade::api::dynamic_state::depth_bias_clamp);
					dynamic_states.push_back(reshade::api::dynamic_state::depth_bias_slope_scaled);
				}

				const reshade::api::pipeline_subobject subobjects[] = {
					{ reshade::api::pipeline_subobject_type::vertex_shader, 1, &vs_desc },
					{ reshade::api::pipeline_subobject_type::pixel_shader, 1, &ps_desc },
					{ reshade::api::pipeline_subobject_type::domain_shader, 1, &ds_desc },
					{ reshade::api::pipeline_subobject_type::hull_shader, 1, &hs_desc },
					{ reshade::api::pipeline_subobject_type::geometry_shader, 1, &gs_desc },
					{ reshade::api::pipeline_subobject_type::stream_output_state, 1, &stream_output_desc },
					{ reshade::api::pipeline_subobject_type::blend_state, 1, &blend_desc },
					{ reshade::api::pipeline_subobject_type::sample_mask, 1, &sample_mask },
					{ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_desc },
					{ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc },
					{ reshade::api::pipeline_subobject_type::input_layout, static_cast<uint32_t>(input_layout.size()), input_layout.data() },
					{ reshade::api::pipeline_subobject_type::primitive_topology, 1, &topology },
					{ reshade::api::pipeline_subobject_type::render_target_formats, internal_desc.NumRenderTargets, render_target_formats },
					{ reshade::api::pipeline_subobject_type::depth_stencil_format, 1, &depth_stencil_format },
					{ reshade::api::pipeline_subobject_type::sample_count, 1, &sample_count },
					{ reshade::api::pipeline_subobject_type::dynamic_pipeline_states, static_cast<uint32_t>(dynamic_states.size()), dynamic_states.data() },
				};

				reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(device_proxy, to_handle(internal_desc.pRootSignature), static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

				if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
				{
					register_destruction_callback_d3dx(pipeline, [device_proxy, pipeline]() {
						reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(device_proxy, to_handle(pipeline));
					});
				}
			}
			else
			{
				LOG(WARN) << "Unknown interface " << riid << " in " << "ID3D12PipelineLibrary::LoadGraphicsPipeline" << '.';
			}
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		// 'E_INVALIDARG' is a common occurence indicating that the requested pipeline was not in the library
		LOG(WARN) << "ID3D12PipelineLibrary::LoadGraphicsPipeline" << " failed with error code " << hr << '.';
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE ID3D12PipelineLibrary_LoadComputePipeline(ID3D12PipelineLibrary *pPipelineLibrary, LPCWSTR pName, const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	// Do not invoke 'create_pipeline' event, since it is not possible to modify the pipeline

	const HRESULT hr = reshade::hooks::call(ID3D12PipelineLibrary_LoadComputePipeline, reshade::hooks::vtable_from_instance(pPipelineLibrary) + 10)(pPipelineLibrary, pName, pDesc, riid, ppPipelineState);
	if (SUCCEEDED(hr))
	{
		assert(pDesc != nullptr && ppPipelineState != nullptr);

		com_ptr<ID3D12Device> device;
		pPipelineLibrary->GetDevice(IID_PPV_ARGS(&device));

		const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get());
		if (device_proxy != nullptr)
		{
			if (riid == __uuidof(ID3D12PipelineState))
			{
				const auto pipeline = static_cast<ID3D12PipelineState *>(*ppPipelineState);

				const D3D12_COMPUTE_PIPELINE_STATE_DESC &internal_desc = *pDesc;

				reshade::api::shader_desc cs_desc = reshade::d3d12::convert_shader_desc(internal_desc.CS);

				const reshade::api::pipeline_subobject subobjects[] = {
					{ reshade::api::pipeline_subobject_type::compute_shader, 1, &cs_desc }
				};

				reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(device_proxy, to_handle(internal_desc.pRootSignature), static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

				if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
				{
					register_destruction_callback_d3dx(pipeline, [device_proxy, pipeline]() {
						reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(device_proxy, to_handle(pipeline));
					});
				}
			}
			else
			{
				LOG(WARN) << "Unknown interface " << riid << " in " << "ID3D12PipelineLibrary::LoadComputePipeline" << '.';
			}
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		LOG(WARN) << "ID3D12PipelineLibrary::LoadComputePipeline" << " failed with error code " << hr << '.';
	}
#endif

	return hr;
}

HRESULT STDMETHODCALLTYPE ID3D12PipelineLibrary1_LoadPipeline(ID3D12PipelineLibrary1 *pPipelineLibrary, LPCWSTR pName, const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	// Do not invoke 'create_pipeline' event, since it is not possible to modify the pipeline

	const HRESULT hr = reshade::hooks::call(ID3D12PipelineLibrary1_LoadPipeline, reshade::hooks::vtable_from_instance(pPipelineLibrary) + 13)(pPipelineLibrary, pName, pDesc, riid, ppPipelineState);
	if (SUCCEEDED(hr))
	{
		assert(pDesc != nullptr && ppPipelineState != nullptr);

		com_ptr<ID3D12Device> device;
		pPipelineLibrary->GetDevice(IID_PPV_ARGS(&device));

		const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get());
		if (device_proxy != nullptr)
		{
			if (riid == __uuidof(ID3D12PipelineState))
			{
				const auto pipeline = static_cast<ID3D12PipelineState *>(*ppPipelineState);

				reshade::api::pipeline_layout layout = {};

				reshade::api::shader_desc vs_desc, ps_desc, ds_desc, hs_desc, gs_desc, cs_desc, as_desc, ms_desc;
				reshade::api::stream_output_desc stream_output_desc;
				reshade::api::blend_desc blend_desc;
				reshade::api::rasterizer_desc rasterizer_desc;
				reshade::api::depth_stencil_desc depth_stencil_desc;
				std::vector<reshade::api::input_element> input_layout;
				reshade::api::primitive_topology topology;
				reshade::api::format depth_stencil_format;
				reshade::api::format render_target_formats[8];
				uint32_t sample_mask;
				uint32_t sample_count;

				std::vector<reshade::api::pipeline_subobject> subobjects;

				for (uintptr_t p = reinterpret_cast<uintptr_t>(pDesc->pPipelineStateSubobjectStream); p < (reinterpret_cast<uintptr_t>(pDesc->pPipelineStateSubobjectStream) + pDesc->SizeInBytes);)
				{
					switch (*reinterpret_cast<const D3D12_PIPELINE_STATE_SUBOBJECT_TYPE *>(p))
					{
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:
						layout = to_handle(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE *>(p)->data);
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:
						vs_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_VS *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::vertex_shader, 1, &vs_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_VS);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:
						ps_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_PS *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::pixel_shader, 1, &ps_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_PS);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:
						ds_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_DS *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::domain_shader, 1, &ds_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_DS);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:
						hs_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_HS *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::hull_shader, 1, &hs_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_HS);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:
						gs_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_GS *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::geometry_shader, 1, &gs_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_GS);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:
						cs_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_CS *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::compute_shader, 1, &cs_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_CS);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:
						stream_output_desc = reshade::d3d12::convert_stream_output_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_STREAM_OUTPUT *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::stream_output_state, 1, &stream_output_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_STREAM_OUTPUT);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND:
						blend_desc = reshade::d3d12::convert_blend_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_BLEND_DESC *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::blend_state, 1, &blend_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_BLEND_DESC);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:
						sample_mask = reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_SAMPLE_MASK *>(p)->data;
						subobjects.push_back({ reshade::api::pipeline_subobject_type::sample_mask, 1, &sample_mask });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_SAMPLE_MASK);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER:
						rasterizer_desc = reshade::d3d12::convert_rasterizer_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RASTERIZER *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_RASTERIZER);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:
						depth_stencil_desc = reshade::d3d12::convert_depth_stencil_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:
						input_layout = reshade::d3d12::convert_input_layout_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_INPUT_LAYOUT *>(p)->data.NumElements, reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_INPUT_LAYOUT *>(p)->data.pInputElementDescs);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::input_layout, static_cast<uint32_t>(input_layout.size()), input_layout.data() });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_INPUT_LAYOUT);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_IB_STRIP_CUT_VALUE);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:
						topology = reshade::d3d12::convert_primitive_topology_type(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::primitive_topology, 1, &topology });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS:
						assert(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS *>(p)->data.NumRenderTargets <= 8);
						for (UINT i = 0; i < reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS *>(p)->data.NumRenderTargets; ++i)
							render_target_formats[i] = reshade::d3d12::convert_format(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS *>(p)->data.RTFormats[i]);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::render_target_formats, reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS *>(p)->data.NumRenderTargets, render_target_formats });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:
						depth_stencil_format = reshade::d3d12::convert_format(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_format, 1, &depth_stencil_format });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:
						sample_count = reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_SAMPLE_DESC *>(p)->data.Count;
						subobjects.push_back({ reshade::api::pipeline_subobject_type::sample_count, 1, &sample_count });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_SAMPLE_DESC);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_NODE_MASK);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_CACHED_PSO);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_FLAGS);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:
						depth_stencil_desc = reshade::d3d12::convert_depth_stencil_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:
						assert(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_VIEW_INSTANCING *>(p)->data.ViewInstanceCount <= D3D12_MAX_VIEW_INSTANCE_COUNT);
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_VIEW_INSTANCING);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:
						as_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_AS *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::amplification_shader, 1, &as_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_AS);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:
						ms_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_MS *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::mesh_shader, 1, &ms_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_MS);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2:
						depth_stencil_desc = reshade::d3d12::convert_depth_stencil_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL2 *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL2);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER1:
						rasterizer_desc = reshade::d3d12::convert_rasterizer_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RASTERIZER1 *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_RASTERIZER1);
						continue;
					case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER2:
						rasterizer_desc = reshade::d3d12::convert_rasterizer_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RASTERIZER2 *>(p)->data);
						subobjects.push_back({ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_desc });
						p += sizeof(D3D12_PIPELINE_STATE_STREAM_RASTERIZER2);
						continue;
					default:
						// Unknown sub-object type, break out of the loop
						assert(false);
					}
					break;
				}

				reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(device_proxy, layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), to_handle(pipeline));

				if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
				{
					register_destruction_callback_d3dx(pipeline, [device_proxy, pipeline]() {
						reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(device_proxy, to_handle(pipeline));
					});
				}
			}
			else
			{
				LOG(WARN) << "Unknown interface " << riid << " in " << "ID3D12PipelineLibrary1::LoadPipeline" << '.';
			}
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		LOG(WARN) << "ID3D12PipelineLibrary1::LoadPipeline" << " failed with error code " << hr << '.';
	}
#endif

	return hr;
}

#endif
