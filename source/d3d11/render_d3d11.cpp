/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "render_d3d11.hpp"
#include "render_d3d11_utils.hpp"
#include <algorithm>

namespace
{
	struct pipeline_layout_impl
	{
		com_ptr<ID3D11Buffer> push_constants;
		UINT push_constants_binding;
	};

	struct pipeline_graphics_impl
	{
		com_ptr<ID3D11VertexShader> vs;
		com_ptr<ID3D11HullShader> hs;
		com_ptr<ID3D11DomainShader> ds;
		com_ptr<ID3D11GeometryShader> gs;
		com_ptr<ID3D11PixelShader> ps;

		com_ptr<ID3D11InputLayout> input_layout;
		com_ptr<ID3D11BlendState> blend_state;
		com_ptr<ID3D11RasterizerState> rasterizer_state;
		com_ptr<ID3D11DepthStencilState> depth_stencil_state;

		D3D11_PRIMITIVE_TOPOLOGY topology;
		UINT sample_mask;
		UINT stencil_reference_value;
		FLOAT blend_constant[4];

		void apply(ID3D11DeviceContext *ctx) const
		{
			ctx->VSSetShader(vs.get(), nullptr, 0);
			ctx->HSSetShader(hs.get(), nullptr, 0);
			ctx->DSSetShader(ds.get(), nullptr, 0);
			ctx->GSSetShader(gs.get(), nullptr, 0);
			ctx->PSSetShader(ps.get(), nullptr, 0);
			ctx->IASetInputLayout(input_layout.get());
			ctx->IASetPrimitiveTopology(topology);
			ctx->OMSetBlendState(blend_state.get(), blend_constant, sample_mask);
			ctx->RSSetState(rasterizer_state.get());
			ctx->OMSetDepthStencilState(depth_stencil_state.get(), stencil_reference_value);
		}
	};

	const GUID vertex_shader_byte_code_guid = { 0xB2257A30, 0x4014, 0x46EA, { 0xBD, 0x88, 0xDE, 0xC2, 0x1D, 0xB6, 0xA0, 0x2B } };
}

reshade::d3d11::device_impl::device_impl(ID3D11Device *device) :
	api_object_impl(device)
{
#if RESHADE_ADDON
	addon::load_addons();
#endif

	invoke_addon_event<reshade::addon_event::init_device>(this);
}
reshade::d3d11::device_impl::~device_impl()
{
	invoke_addon_event<reshade::addon_event::destroy_device>(this);

#if RESHADE_ADDON
	addon::unload_addons();
#endif
}

bool reshade::d3d11::device_impl::check_capability(api::device_caps capability) const
{
	D3D11_FEATURE_DATA_D3D11_OPTIONS options;

	switch (capability)
	{
	case api::device_caps::compute_shader:
		// Feature level 10 and 10.1 support a limited form of DirectCompute, but it does not have support for RWTexture2D, so is not particularly useful
		// See https://docs.microsoft.com/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-shader
		return _orig->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0;
	case api::device_caps::geometry_shader:
		return _orig->GetFeatureLevel() >= D3D_FEATURE_LEVEL_10_0;
	case api::device_caps::tessellation_shaders:
		return _orig->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0;
	case api::device_caps::dual_src_blend:
	case api::device_caps::independent_blend:
		return true;
	case api::device_caps::logic_op:
		_orig->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &options, sizeof(options));
		return options.OutputMergerLogicOp;
	case api::device_caps::draw_instanced:
	case api::device_caps::draw_or_dispatch_indirect:
	case api::device_caps::fill_mode_non_solid:
	case api::device_caps::multi_viewport:
	case api::device_caps::sampler_anisotropy:
	case api::device_caps::push_descriptors:
		return true;
	case api::device_caps::descriptor_tables:
	case api::device_caps::sampler_with_resource_view:
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
		return false;
	case api::device_caps::copy_buffer_region:
		return true;
	case api::device_caps::copy_buffer_to_texture:
		return false;
	default:
		return false;
	}
}
bool reshade::d3d11::device_impl::check_format_support(api::format format, api::resource_usage usage) const
{
	UINT support = 0;
	if (FAILED(_orig->CheckFormatSupport(convert_format(format), &support)))
		return false;

	if ((usage & api::resource_usage::depth_stencil) != api::resource_usage::undefined &&
		(support & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) == 0)
		return false;
	if ((usage & api::resource_usage::render_target) != api::resource_usage::undefined &&
		(support & D3D11_FORMAT_SUPPORT_RENDER_TARGET) == 0)
		return false;
	if ((usage & api::resource_usage::shader_resource) != api::resource_usage::undefined &&
		(support & (D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE)) == 0)
		return false;
	if ((usage & api::resource_usage::unordered_access) != api::resource_usage::undefined &&
		(support & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) == 0)
		return false;

	if ((usage & (api::resource_usage::resolve_source | api::resource_usage::resolve_dest)) != api::resource_usage::undefined &&
		(support & D3D10_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE) == 0)
		return false;

	return true;
}

bool reshade::d3d11::device_impl::check_resource_handle_valid(api::resource handle) const
{
	return handle.handle != 0 && _resources.has_object(reinterpret_cast<ID3D11Resource *>(handle.handle));
}
bool reshade::d3d11::device_impl::check_resource_view_handle_valid(api::resource_view handle) const
{
	return handle.handle != 0 && _views.has_object(reinterpret_cast<ID3D11View *>(handle.handle));
}

bool reshade::d3d11::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out)
{
	D3D11_SAMPLER_DESC internal_desc = {};
	convert_sampler_desc(desc, internal_desc);

	if (com_ptr<ID3D11SamplerState> object;
		SUCCEEDED(_orig->CreateSamplerState(&internal_desc, &object)))
	{
		*out = { reinterpret_cast<uintptr_t>(object.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d11::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage, api::resource *out)
{
	static_assert(sizeof(api::subresource_data) == sizeof(D3D11_SUBRESOURCE_DATA));

	switch (desc.type)
	{
		case api::resource_type::buffer:
		{
			D3D11_BUFFER_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D11Buffer> object;
				SUCCEEDED(_orig->CreateBuffer(&internal_desc, reinterpret_cast<const D3D11_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				_resources.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_1d:
		{
			D3D11_TEXTURE1D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D11Texture1D> object;
				SUCCEEDED(_orig->CreateTexture1D(&internal_desc, reinterpret_cast<const D3D11_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				_resources.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_2d:
		{
			D3D11_TEXTURE2D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D11Texture2D> object;
				SUCCEEDED(_orig->CreateTexture2D(&internal_desc, reinterpret_cast<const D3D11_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				_resources.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_3d:
		{
			D3D11_TEXTURE3D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D11Texture3D> object;
				SUCCEEDED(_orig->CreateTexture3D(&internal_desc, reinterpret_cast<const D3D11_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				_resources.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}
bool reshade::d3d11::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out)
{
	assert(resource.handle != 0);

	switch (usage_type)
	{
		case api::resource_usage::depth_stencil:
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D11DepthStencilView> object;
				SUCCEEDED(_orig->CreateDepthStencilView(reinterpret_cast<ID3D11Resource *>(resource.handle), &internal_desc, &object)))
			{
				_views.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_usage::render_target:
		{
			D3D11_RENDER_TARGET_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D11RenderTargetView> object;
				SUCCEEDED(_orig->CreateRenderTargetView(reinterpret_cast<ID3D11Resource *>(resource.handle), &internal_desc, &object)))
			{
				_views.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_usage::shader_resource:
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D11ShaderResourceView> object;
				SUCCEEDED(_orig->CreateShaderResourceView(reinterpret_cast<ID3D11Resource *>(resource.handle), &internal_desc, &object)))
			{
				_views.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_usage::unordered_access:
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D11UnorderedAccessView> object;
				SUCCEEDED(_orig->CreateUnorderedAccessView(reinterpret_cast<ID3D11Resource *>(resource.handle), &internal_desc, &object)))
			{
				_views.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}

bool reshade::d3d11::device_impl::create_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	switch (desc.type)
	{
	default:
		*out = { 0 };
		return false;
	case api::pipeline_type::compute:
		return create_pipeline_compute(desc, out);
	case api::pipeline_type::graphics:
		return create_pipeline_graphics_all(desc, out);
	case api::pipeline_type::graphics_blend_state:
		return create_pipeline_graphics_blend_state(desc, out);
	case api::pipeline_type::graphics_rasterizer_state:
		return create_pipeline_graphics_rasterizer_state(desc, out);
	case api::pipeline_type::graphics_depth_stencil_state:
		return create_pipeline_graphics_depth_stencil_state(desc, out);
	}
}
bool reshade::d3d11::device_impl::create_pipeline_compute(const api::pipeline_desc &desc, api::pipeline *out)
{
	const auto cs = reinterpret_cast<ID3D11ComputeShader *>(desc.compute.shader.handle);

	cs->AddRef();
	{
		*out = { reinterpret_cast<uintptr_t>(cs) };
		return true;
	}
}
bool reshade::d3d11::device_impl::create_pipeline_graphics_all(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (desc.graphics.num_dynamic_states != 0)
	{
		*out = { 0 };
		return false;
	}

	api::pipeline blend_state;
	if (!create_pipeline_graphics_blend_state(desc, &blend_state))
	{
		*out = { 0 };
		return false;
	}
	api::pipeline rasterizer_state;
	if (!create_pipeline_graphics_rasterizer_state(desc, &rasterizer_state))
	{
		*out = { 0 };
		destroy_pipeline(api::pipeline_type::graphics_blend_state, blend_state);
		return false;
	}
	api::pipeline depth_stencil_state;
	if (!create_pipeline_graphics_depth_stencil_state(desc, &depth_stencil_state))
	{
		*out = { 0 };
		destroy_pipeline(api::pipeline_type::graphics_blend_state, blend_state);
		destroy_pipeline(api::pipeline_type::graphics_rasterizer_state, rasterizer_state);
		return false;
	}

	std::vector<D3D11_INPUT_ELEMENT_DESC> internal_elements;
	internal_elements.reserve(16);
	for (UINT i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
	{
		const auto &element = desc.graphics.input_layout[i];
		D3D11_INPUT_ELEMENT_DESC &internal_element = internal_elements.emplace_back();

		internal_element.SemanticName = element.semantic;
		internal_element.SemanticIndex = element.semantic_index;
		internal_element.Format = convert_format(element.format);
		internal_element.InputSlot = element.buffer_binding;
		internal_element.AlignedByteOffset = element.offset;
		internal_element.InputSlotClass = element.instance_step_rate > 0 ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
		internal_element.InstanceDataStepRate = element.instance_step_rate;
	}

	std::vector<uint8_t> bytecode;
	if (desc.graphics.vertex_shader.handle != 0)
	{
		const auto vertex_shader = reinterpret_cast<ID3D11VertexShader *>(desc.graphics.vertex_shader.handle);

		UINT bytecode_len = 0;
		vertex_shader->GetPrivateData(vertex_shader_byte_code_guid, &bytecode_len, nullptr);
		bytecode.resize(bytecode_len);
		vertex_shader->GetPrivateData(vertex_shader_byte_code_guid, &bytecode_len, bytecode.data());
	}

	com_ptr<ID3D11InputLayout> input_layout;
	if (!internal_elements.empty() &&
		FAILED(_orig->CreateInputLayout(internal_elements.data(), static_cast<UINT>(internal_elements.size()), bytecode.data(), bytecode.size(), &input_layout)))
	{
		*out = { 0 };
		destroy_pipeline(api::pipeline_type::graphics_blend_state, blend_state);
		destroy_pipeline(api::pipeline_type::graphics_rasterizer_state, rasterizer_state);
		destroy_pipeline(api::pipeline_type::graphics_depth_stencil_state, depth_stencil_state);
		return false;
	}

	const auto state = new pipeline_graphics_impl();

	state->vs = reinterpret_cast<ID3D11VertexShader *>(desc.graphics.vertex_shader.handle);
	state->hs = reinterpret_cast<ID3D11HullShader *>(desc.graphics.hull_shader.handle);
	state->ds = reinterpret_cast<ID3D11DomainShader *>(desc.graphics.domain_shader.handle);
	state->gs = reinterpret_cast<ID3D11GeometryShader *>(desc.graphics.geometry_shader.handle);
	state->ps = reinterpret_cast<ID3D11PixelShader *>(desc.graphics.pixel_shader.handle);

	state->input_layout = std::move(input_layout);

	state->blend_state = reinterpret_cast<ID3D11BlendState *>(blend_state.handle);
	state->rasterizer_state = reinterpret_cast<ID3D11RasterizerState *>(rasterizer_state.handle);
	state->depth_stencil_state = reinterpret_cast<ID3D11DepthStencilState *>(depth_stencil_state.handle);

	state->topology = static_cast<D3D11_PRIMITIVE_TOPOLOGY>(desc.graphics.rasterizer_state.topology);
	state->sample_mask = desc.graphics.multisample_state.sample_mask;
	state->stencil_reference_value = desc.graphics.depth_stencil_state.stencil_reference_value;

	state->blend_constant[0] = ((desc.graphics.blend_state.blend_constant      ) & 0xFF) / 255.0f;
	state->blend_constant[1] = ((desc.graphics.blend_state.blend_constant >>  4) & 0xFF) / 255.0f;
	state->blend_constant[2] = ((desc.graphics.blend_state.blend_constant >>  8) & 0xFF) / 255.0f;
	state->blend_constant[3] = ((desc.graphics.blend_state.blend_constant >> 12) & 0xFF) / 255.0f;

	destroy_pipeline(api::pipeline_type::graphics_blend_state, blend_state);
	destroy_pipeline(api::pipeline_type::graphics_rasterizer_state, rasterizer_state);
	destroy_pipeline(api::pipeline_type::graphics_depth_stencil_state, depth_stencil_state);

	*out = { reinterpret_cast<uintptr_t>(state) };
	return true;
}
bool reshade::d3d11::device_impl::create_pipeline_graphics_blend_state(const api::pipeline_desc &desc, api::pipeline *out)
{
	D3D11_BLEND_DESC internal_desc;
	internal_desc.AlphaToCoverageEnable = desc.graphics.multisample_state.alpha_to_coverage;
	internal_desc.IndependentBlendEnable = TRUE;

	for (UINT i = 0; i < 8; ++i)
	{
		internal_desc.RenderTarget[i].BlendEnable = desc.graphics.blend_state.blend_enable[i];
		internal_desc.RenderTarget[i].SrcBlend = convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[i]);
		internal_desc.RenderTarget[i].DestBlend = convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[i]);
		internal_desc.RenderTarget[i].BlendOp = convert_blend_op(desc.graphics.blend_state.color_blend_op[i]);
		internal_desc.RenderTarget[i].SrcBlendAlpha = convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[i]);
		internal_desc.RenderTarget[i].DestBlendAlpha = convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[i]);
		internal_desc.RenderTarget[i].BlendOpAlpha = convert_blend_op(desc.graphics.blend_state.alpha_blend_op[i]);
		internal_desc.RenderTarget[i].RenderTargetWriteMask = desc.graphics.blend_state.render_target_write_mask[i];
	}

	if (com_ptr<ID3D11BlendState> object;
		SUCCEEDED(_orig->CreateBlendState(&internal_desc, &object)))
	{
		*out = { reinterpret_cast<uintptr_t>(object.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d11::device_impl::create_pipeline_graphics_rasterizer_state(const api::pipeline_desc &desc, api::pipeline *out)
{
	D3D11_RASTERIZER_DESC internal_desc;
	internal_desc.FillMode = convert_fill_mode(desc.graphics.rasterizer_state.fill_mode);
	internal_desc.CullMode = convert_cull_mode(desc.graphics.rasterizer_state.cull_mode);
	internal_desc.FrontCounterClockwise = desc.graphics.rasterizer_state.front_counter_clockwise;
	internal_desc.DepthBias = static_cast<INT>(desc.graphics.rasterizer_state.depth_bias);
	internal_desc.DepthBiasClamp = desc.graphics.rasterizer_state.depth_bias_clamp;
	internal_desc.SlopeScaledDepthBias = desc.graphics.rasterizer_state.slope_scaled_depth_bias;
	internal_desc.DepthClipEnable = desc.graphics.rasterizer_state.depth_clip;
	internal_desc.ScissorEnable = desc.graphics.rasterizer_state.scissor_test;
	internal_desc.MultisampleEnable = desc.graphics.multisample_state.multisample;
	internal_desc.AntialiasedLineEnable = desc.graphics.rasterizer_state.antialiased_line;

	if (com_ptr<ID3D11RasterizerState> object;
		SUCCEEDED(_orig->CreateRasterizerState(&internal_desc, &object)))
	{
		*out = { reinterpret_cast<uintptr_t>(object.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d11::device_impl::create_pipeline_graphics_depth_stencil_state(const api::pipeline_desc &desc, api::pipeline *out)
{
	D3D11_DEPTH_STENCIL_DESC internal_desc;
	internal_desc.DepthEnable = desc.graphics.depth_stencil_state.depth_test;
	internal_desc.DepthWriteMask = desc.graphics.depth_stencil_state.depth_write_mask ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	internal_desc.DepthFunc = convert_compare_op(desc.graphics.depth_stencil_state.depth_func);
	internal_desc.StencilEnable = desc.graphics.depth_stencil_state.stencil_test;
	internal_desc.StencilReadMask = desc.graphics.depth_stencil_state.stencil_read_mask;
	internal_desc.StencilWriteMask = desc.graphics.depth_stencil_state.stencil_write_mask;
	internal_desc.BackFace.StencilFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_fail_op);
	internal_desc.BackFace.StencilDepthFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_depth_fail_op);
	internal_desc.BackFace.StencilPassOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_pass_op);
	internal_desc.BackFace.StencilFunc = convert_compare_op(desc.graphics.depth_stencil_state.back_stencil_func);
	internal_desc.FrontFace.StencilFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_fail_op);
	internal_desc.FrontFace.StencilDepthFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_depth_fail_op);
	internal_desc.FrontFace.StencilPassOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_pass_op);
	internal_desc.FrontFace.StencilFunc = convert_compare_op(desc.graphics.depth_stencil_state.front_stencil_func);

	if (com_ptr<ID3D11DepthStencilState> object;
		SUCCEEDED(_orig->CreateDepthStencilState(&internal_desc, &object)))
	{
		*out = { reinterpret_cast<uintptr_t>(object.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}

bool reshade::d3d11::device_impl::create_shader_module(api::shader_stage type, api::shader_format format, const char *entry_point, const void *code, size_t code_size, api::shader_module *out)
{
	if (format == api::shader_format::dxbc)
	{
		assert(entry_point == nullptr);

		switch (type)
		{
		case api::shader_stage::vertex:
			if (com_ptr<ID3D11VertexShader> object;
				SUCCEEDED(_orig->CreateVertexShader(code, code_size, nullptr, &object)))
			{
				assert(code_size <= std::numeric_limits<UINT>::max());
				object->SetPrivateData(vertex_shader_byte_code_guid, static_cast<UINT>(code_size), code);

				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		case api::shader_stage::hull:
			if (com_ptr<ID3D11HullShader> object;
				SUCCEEDED(_orig->CreateHullShader(code, code_size, nullptr, &object)))
			{
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		case api::shader_stage::domain:
			if (com_ptr<ID3D11DomainShader> object;
				SUCCEEDED(_orig->CreateDomainShader(code, code_size, nullptr, &object)))
			{
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		case api::shader_stage::geometry:
			if (com_ptr<ID3D11GeometryShader> object;
				SUCCEEDED(_orig->CreateGeometryShader(code, code_size, nullptr, &object)))
			{
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		case api::shader_stage::pixel:
			if (com_ptr<ID3D11PixelShader> object;
				SUCCEEDED(_orig->CreatePixelShader(code, code_size, nullptr, &object)))
			{
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		case api::shader_stage::compute:
			if (com_ptr<ID3D11ComputeShader> object;
				SUCCEEDED(_orig->CreateComputeShader(code, code_size, nullptr, &object)))
			{
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}
bool reshade::d3d11::device_impl::create_pipeline_layout(uint32_t num_table_layouts, const api::descriptor_table_layout *table_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out)
{
	if (num_constant_ranges > 1)
	{
		*out = { 0 };
		return false;
	}

	const auto layout = new pipeline_layout_impl();

	if (num_constant_ranges == 1)
	{
		assert(constant_ranges[0].offset == 0);

		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = constant_ranges[0].count * 4;;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		if (FAILED(_orig->CreateBuffer(&desc, nullptr, &layout->push_constants)))
		{
			delete layout;

			*out = { 0 };
			return false;
		}

		layout->push_constants_binding = constant_ranges[0].dx_shader_register;
	}

	*out = { reinterpret_cast<uintptr_t>(layout) };
	return true;
}
bool reshade::d3d11::device_impl::create_descriptor_heap(uint32_t, uint32_t, const api::descriptor_heap_size *, api::descriptor_heap *out)
{
	assert(false);

	*out = { 0 };
	return false;
}
bool reshade::d3d11::device_impl::create_descriptor_tables(api::descriptor_heap, api::descriptor_table_layout, uint32_t, api::descriptor_table *out)
{
	assert(false);

	*out = { 0 };
	return false;
}
bool reshade::d3d11::device_impl::create_descriptor_table_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_table_layout *out)
{
	*out = { 0 };
	return push_descriptors;
}

void reshade::d3d11::device_impl::destroy_sampler(api::sampler handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d11::device_impl::destroy_resource(api::resource handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d11::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

void reshade::d3d11::device_impl::destroy_pipeline(api::pipeline_type type, api::pipeline handle)
{
	if (type == api::pipeline_type::graphics)
		delete reinterpret_cast<pipeline_graphics_impl *>(handle.handle);
	else if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d11::device_impl::destroy_shader_module(api::shader_module handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d11::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	delete reinterpret_cast<pipeline_layout_impl *>(handle.handle);
}
void reshade::d3d11::device_impl::destroy_descriptor_heap(api::descriptor_heap)
{
	assert(false);
}
void reshade::d3d11::device_impl::destroy_descriptor_table_layout(api::descriptor_table_layout)
{
}

void reshade::d3d11::device_impl::update_descriptor_tables(uint32_t, const api::descriptor_update *)
{
	assert(false);
}

bool reshade::d3d11::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **mapped_ptr)
{
	D3D11_MAP map_type = static_cast<D3D11_MAP>(0);
	switch (access)
	{
	case api::map_access::read_only:
		map_type = D3D11_MAP_READ;
		break;
	case api::map_access::write_only:
		map_type = D3D11_MAP_WRITE;
		break;
	case api::map_access::read_write:
		map_type = D3D11_MAP_READ_WRITE;
		break;
	case api::map_access::write_discard:
		map_type = D3D11_MAP_WRITE_DISCARD;
		break;
	}

	com_ptr<ID3D11DeviceContext> ctx;
	_orig->GetImmediateContext(&ctx);

	if (D3D11_MAPPED_SUBRESOURCE mapped_resource;
		SUCCEEDED(ctx->Map(reinterpret_cast<ID3D11Resource *>(resource.handle), subresource, map_type, 0, &mapped_resource)))
	{
		*mapped_ptr = mapped_resource.pData;
		return true;
	}
	else
	{
		*mapped_ptr = 0;
		return false;
	}
}
void reshade::d3d11::device_impl::unmap_resource(api::resource resource, uint32_t subresource)
{
	com_ptr<ID3D11DeviceContext> ctx;
	_orig->GetImmediateContext(&ctx);

	ctx->Unmap(reinterpret_cast<ID3D11Resource *>(resource.handle), subresource);
}

void reshade::d3d11::device_impl::upload_buffer_region(api::resource dst, uint64_t dst_offset, const void *data, uint64_t size)
{
	assert(dst.handle != 0);
	assert(dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	com_ptr<ID3D11DeviceContext> ctx;
	_orig->GetImmediateContext(&ctx);

	const D3D11_BOX dst_box = { static_cast<UINT>(dst_offset), 0, 0, static_cast<UINT>(dst_offset + size), 1, 1 };
	ctx->UpdateSubresource(reinterpret_cast<ID3D11Resource *>(dst.handle), 0, nullptr, data, static_cast<UINT>(size), static_cast<UINT>(size));
}
void reshade::d3d11::device_impl::upload_texture_region(api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], const void *data, uint32_t row_pitch, uint32_t slice_pitch)
{
	assert(dst.handle != 0);

	com_ptr<ID3D11DeviceContext> ctx;
	_orig->GetImmediateContext(&ctx);

	ctx->UpdateSubresource(reinterpret_cast<ID3D11Resource *>(dst.handle), dst_subresource, reinterpret_cast<const D3D11_BOX *>(dst_box), data, row_pitch, slice_pitch);
}

void reshade::d3d11::device_impl::get_resource_from_view(api::resource_view view, api::resource *out_resource) const
{
	assert(view.handle != 0);
	com_ptr<ID3D11Resource> resource;
	reinterpret_cast<ID3D11View *>(view.handle)->GetResource(&resource);

	*out_resource = { reinterpret_cast<uintptr_t>(resource.get()) };
}

reshade::api::resource_desc reshade::d3d11::device_impl::get_resource_desc(api::resource resource) const
{
	assert(resource.handle != 0);
	const auto resource_object = reinterpret_cast<ID3D11Resource *>(resource.handle);

	D3D11_RESOURCE_DIMENSION dimension;
	resource_object->GetType(&dimension);
	switch (dimension)
	{
		default:
		{
			assert(false); // Not implemented
			return api::resource_desc {};
		}
		case D3D11_RESOURCE_DIMENSION_BUFFER:
		{
			D3D11_BUFFER_DESC internal_desc;
			static_cast<ID3D11Buffer *>(resource_object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		{
			D3D11_TEXTURE1D_DESC internal_desc;
			static_cast<ID3D11Texture1D *>(resource_object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		{
			D3D11_TEXTURE2D_DESC internal_desc;
			static_cast<ID3D11Texture2D *>(resource_object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
		{
			D3D11_TEXTURE3D_DESC internal_desc;
			static_cast<ID3D11Texture3D *>(resource_object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
	}
}

void reshade::d3d11::device_impl::set_debug_name(api::resource resource, const char *name)
{
	const size_t debug_name_len = strlen(name);
	std::wstring debug_name_wide;
	debug_name_wide.reserve(debug_name_len + 1);
	utf8::unchecked::utf8to16(name, name + debug_name_len, std::back_inserter(debug_name_wide));

	// WKPDID_D3DDebugObjectNameW
	const GUID debug_object_name_guid = { 0x4cca5fd8, 0x921f, 0x42c8, { 0x85, 0x66, 0x70, 0xca, 0xf2, 0xa9, 0xb7, 0x41 } };
	reinterpret_cast<ID3D11Resource *>(resource.handle)->SetPrivateData(debug_object_name_guid, static_cast<UINT>(debug_name_len * sizeof(WCHAR)), name);
}

reshade::d3d11::command_list_impl::command_list_impl(device_impl *device, ID3D11CommandList *cmd_list) :
	api_object_impl(cmd_list), _device_impl(device)
{
	invoke_addon_event<addon_event::init_command_list>(this);
}
reshade::d3d11::command_list_impl::~command_list_impl()
{
	invoke_addon_event<addon_event::destroy_command_list>(this);
}

reshade::d3d11::device_context_impl::device_context_impl(device_impl *device, ID3D11DeviceContext *context) :
	api_object_impl(context), _device_impl(device)
{
	if (_orig->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
		invoke_addon_event<addon_event::init_command_list>(this);
	else
		invoke_addon_event<addon_event::init_command_queue>(this);
}
reshade::d3d11::device_context_impl::~device_context_impl()
{
	if (_orig->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
		invoke_addon_event<addon_event::destroy_command_list>(this);
	else
		invoke_addon_event<addon_event::destroy_command_queue>(this);
}

void reshade::d3d11::device_context_impl::flush_immediate_command_list() const
{
	assert(_orig->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

	_orig->Flush();
}

void reshade::d3d11::device_context_impl::bind_pipeline(api::pipeline_type type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);

	switch (type)
	{
	case api::pipeline_type::compute:
		_orig->CSSetShader(reinterpret_cast<ID3D11ComputeShader *>(pipeline.handle), nullptr, 0);
		break;
	case api::pipeline_type::graphics:
		reinterpret_cast<pipeline_graphics_impl *>(pipeline.handle)->apply(_orig);
		break;
	case api::pipeline_type::graphics_blend_state:
		_orig->OMSetBlendState(reinterpret_cast<ID3D11BlendState *>(pipeline.handle), nullptr, D3D11_DEFAULT_SAMPLE_MASK);
		break;
	case api::pipeline_type::graphics_rasterizer_state:
		_orig->RSSetState(reinterpret_cast<ID3D11RasterizerState *>(pipeline.handle));
		break;
	case api::pipeline_type::graphics_depth_stencil_state:
		_orig->OMSetDepthStencilState(reinterpret_cast<ID3D11DepthStencilState *>(pipeline.handle), 0);
		break;
	default:
		assert(false);
		break;
	}
}
void reshade::d3d11::device_context_impl::bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values)
{
	for (UINT i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::pipeline_state::primitive_topology:
			_orig->IASetPrimitiveTopology(convert_primitive_topology(static_cast<api::primitive_topology>(values[i])));
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::d3d11::device_context_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	assert(first == 0);

	_orig->RSSetViewports(count, reinterpret_cast<const D3D11_VIEWPORT *>(viewports));
}
void reshade::d3d11::device_context_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	assert(first == 0);

	_orig->RSSetScissorRects(count, reinterpret_cast<const D3D10_RECT *>(rects));
}

void reshade::d3d11::device_context_impl::bind_samplers(api::shader_stage stage, uint32_t first, uint32_t count, const api::sampler *samplers)
{
	if (count > D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT)
	{
		assert(false);
		count = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D11SamplerState *sampler_ptrs[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		sampler_ptrs[i] = reinterpret_cast<ID3D11SamplerState *>(samplers[i].handle);
#else
	const auto sampler_ptrs = reinterpret_cast<ID3D11SamplerState *const *>(samplers);
#endif

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::hull) == api::shader_stage::hull)
		_orig->HSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::domain) == api::shader_stage::domain)
		_orig->DSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetSamplers(first, count, sampler_ptrs);
}
void reshade::d3d11::device_context_impl::bind_shader_resource_views(api::shader_stage stage, uint32_t first, uint32_t count, const api::resource_view *views)
{
	if (count > D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
	{
		assert(false);
		count = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D11ShaderResourceView *view_ptrs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		view_ptrs[i] = reinterpret_cast<ID3D11ShaderResourceView *>(views[i].handle);
#else
	const auto view_ptrs = reinterpret_cast<ID3D11ShaderResourceView *const *>(views);
#endif

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::hull) == api::shader_stage::hull)
		_orig->HSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::domain) == api::shader_stage::domain)
		_orig->DSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetShaderResources(first, count, view_ptrs);
}
void reshade::d3d11::device_context_impl::bind_unordered_access_views(api::shader_stage stage, uint32_t first, uint32_t count, const api::resource_view *views)
{
	if (count > D3D11_1_UAV_SLOT_COUNT)
	{
		assert(false);
		count = D3D11_1_UAV_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D11UnorderedAccessView *view_ptrs[D3D11_1_UAV_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		view_ptrs[i] = reinterpret_cast<ID3D11UnorderedAccessView *>(views[i].handle);
#else
	const auto view_ptrs = reinterpret_cast<ID3D11UnorderedAccessView *const *>(views);
#endif

	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, first, count, view_ptrs, nullptr);
	if ((stage & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetUnorderedAccessViews(first, count, view_ptrs, nullptr);
}
void reshade::d3d11::device_context_impl::bind_constant_buffers(api::shader_stage stage, uint32_t first, uint32_t count, const api::resource *buffers)
{
	if (count > D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
	{
		assert(false);
		count = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D11Buffer *buffer_ptrs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		buffer_ptrs[i] = reinterpret_cast<ID3D11Buffer *>(buffers[i].handle);
#else
	const auto buffer_ptrs = reinterpret_cast<ID3D11Buffer *const *>(buffers);
#endif

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::hull) == api::shader_stage::hull)
		_orig->HSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::domain) == api::shader_stage::domain)
		_orig->DSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetConstantBuffers(first, count, buffer_ptrs);
}

void reshade::d3d11::device_context_impl::push_constants(api::shader_stage stage, api::pipeline_layout layout, uint32_t, uint32_t first, uint32_t count, const uint32_t *values)
{
	pipeline_layout_impl *const layout_impl = reinterpret_cast<pipeline_layout_impl *>(layout.handle);
	const auto push_constants = layout_impl->push_constants.get();

	if (D3D11_MAPPED_SUBRESOURCE mapped;
		SUCCEEDED(_orig->Map(push_constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) // TODO: No discard
	{
		assert(mapped.RowPitch <= count * sizeof(uint32_t));
		std::memcpy(static_cast<uint32_t *>(mapped.pData) + first, values, count * sizeof(uint32_t));

		_orig->Unmap(push_constants, 0);
	}

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetConstantBuffers(layout_impl->push_constants_binding, 1, &push_constants);
	if ((stage & api::shader_stage::hull) == api::shader_stage::hull)
		_orig->HSSetConstantBuffers(layout_impl->push_constants_binding, 1, &push_constants);
	if ((stage & api::shader_stage::domain) == api::shader_stage::domain)
		_orig->DSSetConstantBuffers(layout_impl->push_constants_binding, 1, &push_constants);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetConstantBuffers(layout_impl->push_constants_binding, 1, &push_constants);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetConstantBuffers(layout_impl->push_constants_binding, 1, &push_constants);
	if ((stage & api::shader_stage::compute) == api::shader_stage::compute)
		_orig->CSSetConstantBuffers(layout_impl->push_constants_binding, 1, &push_constants);
}
void reshade::d3d11::device_context_impl::push_descriptors(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
{
	// TODO: Binding indices are not handled correctly here (see layout index)

	switch (type)
	{
	case api::descriptor_type::sampler:
		bind_samplers(stage, first, count, static_cast<const api::sampler *>(descriptors));
		break;
	case api::descriptor_type::sampler_with_resource_view:
		assert(false);
		break;
	case api::descriptor_type::shader_resource_view:
		bind_shader_resource_views(stage, first, count, static_cast<const api::resource_view *>(descriptors));
		break;
	case api::descriptor_type::unordered_access_view:
		bind_unordered_access_views(stage, first, count, static_cast<const api::resource_view *>(descriptors));
		break;
	case api::descriptor_type::constant_buffer:
		bind_constant_buffers(stage, first, count, static_cast<const api::resource *>(descriptors));
		break;
	}
}
void reshade::d3d11::device_context_impl::bind_descriptor_heaps(uint32_t, const api::descriptor_heap *)
{
	assert(false);
}
void reshade::d3d11::device_context_impl::bind_descriptor_tables(api::pipeline_type, api::pipeline_layout, uint32_t, uint32_t, const api::descriptor_table *)
{
	assert(false);
}

void reshade::d3d11::device_context_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	assert(offset <= std::numeric_limits<UINT>::max());
	assert(buffer.handle == 0 || index_size == 2 || index_size == 4);

	_orig->IASetIndexBuffer(reinterpret_cast<ID3D11Buffer *>(buffer.handle), index_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, static_cast<UINT>(offset));
}
void reshade::d3d11::device_context_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	if (count > D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
	{
		assert(false);
		count = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D11Buffer *buffer_ptrs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		buffer_ptrs[i] = reinterpret_cast<ID3D11Buffer *>(buffers[i].handle);
#else
	const auto buffer_ptrs = reinterpret_cast<ID3D11Buffer *const *>(buffers);
#endif

	UINT offsets_32[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		offsets_32[i] = static_cast<UINT>(offsets[i]);

	_orig->IASetVertexBuffers(first, count, buffer_ptrs, strides, offsets_32);
}

void reshade::d3d11::device_context_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	_orig->DrawInstanced(vertices, instances, first_vertex, first_instance);
}
void reshade::d3d11::device_context_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_orig->DrawIndexedInstanced(indices, instances, first_index, vertex_offset, first_instance);
}
void reshade::d3d11::device_context_impl::dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)
{
	_orig->Dispatch(num_groups_x, num_groups_y, num_groups_z);
}
void reshade::d3d11::device_context_impl::draw_or_dispatch_indirect(uint32_t type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	assert(offset <= std::numeric_limits<UINT>::max());

	switch (type)
	{
	case 1:
		for (UINT i = 0; i < draw_count; ++i)
			_orig->DrawInstancedIndirect(reinterpret_cast<ID3D11Buffer *>(buffer.handle), static_cast<UINT>(offset + i * stride));
		break;
	case 2:
		for (UINT i = 0; i < draw_count; ++i)
			_orig->DrawIndexedInstancedIndirect(reinterpret_cast<ID3D11Buffer *>(buffer.handle), static_cast<UINT>(offset + i * stride));
		break;
	case 3:
		for (UINT i = 0; i < draw_count; ++i)
			_orig->DispatchIndirect(reinterpret_cast<ID3D11Buffer *>(buffer.handle), static_cast<UINT>(offset + i * stride));
		break;
	}
}

void reshade::d3d11::device_context_impl::begin_render_pass(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	if (count > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)
	{
		assert(false);
		count = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
	}

#ifndef WIN64
	ID3D11RenderTargetView *rtv_ptrs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for (UINT i = 0; i < count; ++i)
		rtv_ptrs[i] = reinterpret_cast<ID3D11RenderTargetView *>(rtvs[i].handle);
#else
	const auto rtv_ptrs = reinterpret_cast<ID3D11RenderTargetView *const *>(rtvs);
#endif

	_orig->OMSetRenderTargets(count, rtv_ptrs, reinterpret_cast<ID3D11DepthStencilView *>(dsv.handle));
}
void reshade::d3d11::device_context_impl::end_render_pass()
{
	// Reset render targets
	_orig->OMSetRenderTargets(0, nullptr, nullptr);
}

void reshade::d3d11::device_context_impl::blit(api::resource, uint32_t, const int32_t[6], api::resource, uint32_t, const int32_t[6], api::texture_filter)
{
	assert(false);
}
void reshade::d3d11::device_context_impl::resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], api::format format)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert(src_offset == nullptr && dst_offset == nullptr && size == nullptr);

	_orig->ResolveSubresource(
		reinterpret_cast<ID3D11Resource *>(dst.handle), dst_subresource,
		reinterpret_cast<ID3D11Resource *>(src.handle), src_subresource, convert_format(format));
}
void reshade::d3d11::device_context_impl::copy_resource(api::resource src, api::resource dst)
{
	assert(src.handle != 0 && dst.handle != 0);

	_orig->CopyResource(reinterpret_cast<ID3D11Resource *>(dst.handle), reinterpret_cast<ID3D11Resource *>(src.handle));
}
void reshade::d3d11::device_context_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert(src_offset <= std::numeric_limits<UINT>::max() && dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	const D3D11_BOX src_box = { static_cast<UINT>(src_offset), 0, 0, static_cast<UINT>(src_offset + size), 1, 1 };

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D11Resource *>(dst.handle), 0, static_cast<UINT>(dst_offset), 0, 0,
		reinterpret_cast<ID3D11Resource *>(src.handle), 0, &src_box);
}
void reshade::d3d11::device_context_impl::copy_buffer_to_texture(api::resource, uint64_t, uint32_t, uint32_t, api::resource, uint32_t, const int32_t[6])
{
	assert(false);
}
void reshade::d3d11::device_context_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3])
{
	assert(src.handle != 0 && dst.handle != 0);

	D3D11_BOX src_box;
	if (src_offset != nullptr)
	{
		src_box.left = src_offset[0];
		src_box.top = src_offset[1];
		src_box.front = src_offset[2];
	}
	else
	{
		src_box.left = 0;
		src_box.top = 0;
		src_box.front = 0;
	}

	if (size != nullptr)
	{
		src_box.right = src_box.left + size[0];
		src_box.bottom = src_box.top + size[1];
		src_box.back = src_box.front + size[2];
	}
	else
	{
		const api::resource_desc desc = _device_impl->get_resource_desc(src);
		src_box.right = src_box.left + std::max(1u, desc.texture.width >> (src_subresource % desc.texture.levels));
		src_box.bottom = src_box.top + std::max(1u, desc.texture.height >> (src_subresource % desc.texture.levels));
		src_box.back = src_box.front + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.texture.depth_or_layers) >> (src_subresource % desc.texture.levels)) : 1u);
	}

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D11Resource *>(dst.handle), src_subresource, dst_offset != nullptr ? dst_offset[0] : 0, dst_offset != nullptr ? dst_offset[1] : 0, dst_offset != nullptr ? dst_offset[2] : 0,
		reinterpret_cast<ID3D11Resource *>(src.handle), dst_subresource, &src_box);
}
void reshade::d3d11::device_context_impl::copy_texture_to_buffer(api::resource, uint32_t, const int32_t[6], api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d11::device_context_impl::clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	assert(dsv.handle != 0);

	_orig->ClearDepthStencilView(reinterpret_cast<ID3D11DepthStencilView *>(dsv.handle), clear_flags, depth, stencil);
}
void reshade::d3d11::device_context_impl::clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4])
{
	for (UINT i = 0; i < count; ++i)
	{
		assert(rtvs[i].handle != 0);

		_orig->ClearRenderTargetView(reinterpret_cast<ID3D11RenderTargetView *>(rtvs[i].handle), color);
	}
}
void reshade::d3d11::device_context_impl::clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4])
{
	assert(uav.handle != 0);

	_orig->ClearUnorderedAccessViewUint(reinterpret_cast<ID3D11UnorderedAccessView *>(uav.handle), values);
}
void reshade::d3d11::device_context_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4])
{
	assert(uav.handle != 0);

	_orig->ClearUnorderedAccessViewFloat(reinterpret_cast<ID3D11UnorderedAccessView *>(uav.handle), values);
}

void reshade::d3d11::device_context_impl::begin_debug_event(const char *label, const float[4])
{
	com_ptr<ID3DUserDefinedAnnotation> annotation;
	if (SUCCEEDED(_orig->QueryInterface(&annotation)))
	{
		const size_t label_len = strlen(label);
		std::wstring label_wide;
		label_wide.reserve(label_len + 1);
		utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

		annotation->BeginEvent(label_wide.c_str());
	}
}
void reshade::d3d11::device_context_impl::end_debug_event()
{
	com_ptr<ID3DUserDefinedAnnotation> annotation;
	if (SUCCEEDED(_orig->QueryInterface(&annotation)))
	{
		annotation->EndEvent();
	}
}
void reshade::d3d11::device_context_impl::insert_debug_marker(const char *label, const float[4])
{
	com_ptr<ID3DUserDefinedAnnotation> annotation;
	if (SUCCEEDED(_orig->QueryInterface(&annotation)))
	{
		const size_t label_len = strlen(label);
		std::wstring label_wide;
		label_wide.reserve(label_len + 1);
		utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

		annotation->SetMarker(label_wide.c_str());
	}
}
