/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_type_utils.hpp"
#include <algorithm>

reshade::d3d10::device_impl::device_impl(ID3D10Device1 *device) :
	api_object_impl(device)
{
	// Create copy pipeline
	{
		D3D10_SAMPLER_DESC desc = {};
		desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;

		const resources::data_resource vs = resources::load_data_resource(IDR_FULLSCREEN_VS);
		const resources::data_resource ps = resources::load_data_resource(IDR_COPY_PS);
		if (FAILED(_orig->CreateVertexShader(vs.data, vs.data_size, &_copy_vert_shader)) ||
			FAILED(_orig->CreatePixelShader(ps.data, ps.data_size, &_copy_pixel_shader)) ||
			FAILED(_orig->CreateSamplerState(&desc, &_copy_sampler_state)))
		{
			LOG(ERROR) << "Failed to create copy pipeline!";
		}
	}

#if RESHADE_ADDON
	addon::load_addons();

	invoke_addon_event<addon_event::init_device>(this);
	invoke_addon_event<addon_event::init_command_queue>(this);
#endif
}
reshade::d3d10::device_impl::~device_impl()
{
#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_command_queue>(this);
	invoke_addon_event<addon_event::destroy_device>(this);

	addon::unload_addons();
#endif
}

bool reshade::d3d10::device_impl::check_capability(api::device_caps capability) const
{
	switch (capability)
	{
	case api::device_caps::compute_shader:
		return false;
	case api::device_caps::geometry_shader:
		return _orig->GetFeatureLevel() >= D3D_FEATURE_LEVEL_10_0;
	case api::device_caps::hull_and_domain_shader:
		return false;
	case api::device_caps::dual_src_blend:
		return true;
	case api::device_caps::independent_blend: // Only supports independent 'BlendEnable' and 'RenderTargetWriteMask', so report as unsupported
	case api::device_caps::logic_op:
		return false;
	case api::device_caps::draw_instanced:
		return true;
	case api::device_caps::draw_or_dispatch_indirect:
		return false;
	case api::device_caps::fill_mode_non_solid:
	case api::device_caps::multi_viewport:
		return true;
	case api::device_caps::partial_push_constant_updates:
		return false;
	case api::device_caps::partial_push_descriptor_updates:
	case api::device_caps::sampler_compare_op:
	case api::device_caps::sampler_anisotropic_filtering:
		return true;
	case api::device_caps::sampler_with_resource_view:
		return false;
	case api::device_caps::copy_buffer_region:
		return true;
	case api::device_caps::copy_buffer_to_texture:
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
	case api::device_caps::copy_query_results:
		return false;
	default:
		return false;
	}
}
bool reshade::d3d10::device_impl::check_format_support(api::format format, api::resource_usage usage) const
{
	if ((usage & api::resource_usage::unordered_access) != api::resource_usage::undefined)
		return false;

	UINT support = 0;
	if (FAILED(_orig->CheckFormatSupport(convert_format(format), &support)))
		return false;

	if ((usage & api::resource_usage::depth_stencil) != api::resource_usage::undefined &&
		(support & D3D10_FORMAT_SUPPORT_DEPTH_STENCIL) == 0)
		return false;
	if ((usage & api::resource_usage::render_target) != api::resource_usage::undefined &&
		(support & D3D10_FORMAT_SUPPORT_RENDER_TARGET) == 0)
		return false;
	if ((usage & api::resource_usage::shader_resource) != api::resource_usage::undefined &&
		(support & (D3D10_FORMAT_SUPPORT_SHADER_LOAD | D3D10_FORMAT_SUPPORT_SHADER_SAMPLE)) == 0)
		return false;

	if ((usage & (api::resource_usage::resolve_source | api::resource_usage::resolve_dest)) != api::resource_usage::undefined &&
		(support & D3D10_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE) == 0)
		return false;

	return true;
}

bool reshade::d3d10::device_impl::is_resource_handle_valid(api::resource handle) const
{
	return handle.handle != 0 && _resources.has_object(reinterpret_cast<ID3D10Resource *>(handle.handle));
}
bool reshade::d3d10::device_impl::is_resource_view_handle_valid(api::resource_view handle) const
{
	return handle.handle != 0 && _views.has_object(reinterpret_cast<ID3D10View *>(handle.handle));
}

bool reshade::d3d10::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out)
{
	D3D10_SAMPLER_DESC internal_desc = {};
	convert_sampler_desc(desc, internal_desc);

	if (com_ptr<ID3D10SamplerState> object;
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
bool reshade::d3d10::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage, api::resource *out)
{
	static_assert(sizeof(api::subresource_data) == sizeof(D3D10_SUBRESOURCE_DATA));

	switch (desc.type)
	{
		case api::resource_type::buffer:
		{
			D3D10_BUFFER_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D10Buffer> object;
				SUCCEEDED(_orig->CreateBuffer(&internal_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				_resources.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_1d:
		{
			D3D10_TEXTURE1D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D10Texture1D> object;
				SUCCEEDED(_orig->CreateTexture1D(&internal_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				_resources.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_2d:
		{
			D3D10_TEXTURE2D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D10Texture2D> object;
				SUCCEEDED(_orig->CreateTexture2D(&internal_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				_resources.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_3d:
		{
			D3D10_TEXTURE3D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D10Texture3D> object;
				SUCCEEDED(_orig->CreateTexture3D(&internal_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), &object)))
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
bool reshade::d3d10::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out)
{
	assert(resource.handle != 0);

	switch (usage_type)
	{
		case api::resource_usage::depth_stencil:
		{
			D3D10_DEPTH_STENCIL_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D10DepthStencilView> object;
				SUCCEEDED(_orig->CreateDepthStencilView(reinterpret_cast<ID3D10Resource *>(resource.handle), &internal_desc, &object)))
			{
				_views.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_usage::render_target:
		{
			D3D10_RENDER_TARGET_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D10RenderTargetView> object;
				SUCCEEDED(_orig->CreateRenderTargetView(reinterpret_cast<ID3D10Resource *>(resource.handle), &internal_desc, &object)))
			{
				_views.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_usage::shader_resource:
		{
			D3D10_SHADER_RESOURCE_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D10ShaderResourceView> object;
				SUCCEEDED(_orig->CreateShaderResourceView(reinterpret_cast<ID3D10Resource *>(resource.handle), &internal_desc, &object)))
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

bool reshade::d3d10::device_impl::create_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	switch (desc.type)
	{
	default:
		*out = { 0 };
		return false;
	case api::pipeline_type::graphics:
		return create_pipeline_graphics(desc, out);
	case api::pipeline_type::graphics_vertex_shader:
		return create_pipeline_graphics_vertex_shader(desc, out);
	case api::pipeline_type::graphics_geometry_shader:
		return create_pipeline_graphics_geometry_shader(desc, out);
	case api::pipeline_type::graphics_pixel_shader:
		return create_pipeline_graphics_pixel_shader(desc, out);
	case api::pipeline_type::graphics_input_layout:
		return create_pipeline_graphics_input_layout(desc, out);
	case api::pipeline_type::graphics_blend_state:
		return create_pipeline_graphics_blend_state(desc, out);
	case api::pipeline_type::graphics_rasterizer_state:
		return create_pipeline_graphics_rasterizer_state(desc, out);
	case api::pipeline_type::graphics_depth_stencil_state:
		return create_pipeline_graphics_depth_stencil_state(desc, out);
	}
}
bool reshade::d3d10::device_impl::create_pipeline_graphics(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (desc.graphics.hull_shader.code_size != 0 ||
		desc.graphics.domain_shader.code_size != 0 ||
		desc.graphics.blend_state.logic_op_enable[0] ||
		desc.graphics.num_dynamic_states != 0)
	{
		*out = { 0 };
		return false;
	}

#define create_state_object(name, type, extra_check) \
	api::pipeline name##_handle = { 0 }; \
	if (extra_check && !create_pipeline_graphics_##name(desc, &name##_handle)) { \
		*out = { 0 }; \
		return false; \
	} \
	com_ptr<type> name(reinterpret_cast<type *>(name##_handle.handle), true)

	create_state_object(vertex_shader, ID3D10VertexShader, desc.graphics.vertex_shader.code_size != 0);
	create_state_object(geometry_shader, ID3D10GeometryShader, desc.graphics.geometry_shader.code_size != 0);
	create_state_object(pixel_shader, ID3D10PixelShader, desc.graphics.pixel_shader.code_size != 0);

	create_state_object(input_layout, ID3D10InputLayout, true);
	create_state_object(blend_state, ID3D10BlendState, true);
	create_state_object(rasterizer_state, ID3D10RasterizerState, true);
	create_state_object(depth_stencil_state, ID3D10DepthStencilState, true);

	const auto state = new pipeline_impl();

	state->vs = std::move(vertex_shader);
	state->gs = std::move(geometry_shader);
	state->ps = std::move(pixel_shader);

	state->input_layout = std::move(input_layout);

	state->blend_state = std::move(blend_state);
	state->rasterizer_state = std::move(rasterizer_state);
	state->depth_stencil_state = std::move(depth_stencil_state);

	state->topology = static_cast<D3D10_PRIMITIVE_TOPOLOGY>(desc.graphics.topology);
	state->sample_mask = desc.graphics.sample_mask;
	state->stencil_reference_value = desc.graphics.depth_stencil_state.stencil_reference_value;

	state->blend_constant[0] = ((desc.graphics.blend_state.blend_constant      ) & 0xFF) / 255.0f;
	state->blend_constant[1] = ((desc.graphics.blend_state.blend_constant >>  4) & 0xFF) / 255.0f;
	state->blend_constant[2] = ((desc.graphics.blend_state.blend_constant >>  8) & 0xFF) / 255.0f;
	state->blend_constant[3] = ((desc.graphics.blend_state.blend_constant >> 12) & 0xFF) / 255.0f;

	*out = { reinterpret_cast<uintptr_t>(state) };
	return true;
}

bool reshade::d3d10::device_impl::create_pipeline_graphics_vertex_shader(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (com_ptr<ID3D10VertexShader> object;
		desc.graphics.vertex_shader.format == api::shader_format::dxbc &&
		SUCCEEDED(_orig->CreateVertexShader(desc.graphics.vertex_shader.code, desc.graphics.vertex_shader.code_size, &object)))
	{
		assert(desc.graphics.vertex_shader.entry_point == nullptr);
		assert(desc.graphics.vertex_shader.num_spec_constants == 0);

		*out = { reinterpret_cast<uintptr_t>(object.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d10::device_impl::create_pipeline_graphics_geometry_shader(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (com_ptr<ID3D10GeometryShader> object;
		desc.graphics.geometry_shader.format == api::shader_format::dxbc &&
		SUCCEEDED(_orig->CreateGeometryShader(desc.graphics.geometry_shader.code, desc.graphics.geometry_shader.code_size, &object)))
	{
		assert(desc.graphics.geometry_shader.entry_point == nullptr);
		assert(desc.graphics.geometry_shader.num_spec_constants == 0);

		*out = { reinterpret_cast<uintptr_t>(object.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d10::device_impl::create_pipeline_graphics_pixel_shader(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (com_ptr<ID3D10PixelShader> object;
		desc.graphics.pixel_shader.format == api::shader_format::dxbc &&
		SUCCEEDED(_orig->CreatePixelShader(desc.graphics.pixel_shader.code, desc.graphics.pixel_shader.code_size, &object)))
	{
		assert(desc.graphics.pixel_shader.entry_point == nullptr);
		assert(desc.graphics.pixel_shader.num_spec_constants == 0);

		*out = { reinterpret_cast<uintptr_t>(object.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d10::device_impl::create_pipeline_graphics_input_layout(const api::pipeline_desc &desc, api::pipeline *out)
{
	std::vector<D3D10_INPUT_ELEMENT_DESC> internal_elements;
	internal_elements.reserve(16);

	for (UINT i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
	{
		const api::input_layout_element &element = desc.graphics.input_layout[i];

		D3D10_INPUT_ELEMENT_DESC &internal_element = internal_elements.emplace_back();
		internal_element.SemanticName = element.semantic;
		internal_element.SemanticIndex = element.semantic_index;
		internal_element.Format = convert_format(element.format);
		internal_element.InputSlot = element.buffer_binding;
		internal_element.AlignedByteOffset = element.offset;
		internal_element.InputSlotClass = element.instance_step_rate > 0 ? D3D10_INPUT_PER_INSTANCE_DATA : D3D10_INPUT_PER_VERTEX_DATA;
		internal_element.InstanceDataStepRate = element.instance_step_rate;
	}

	if (com_ptr<ID3D10InputLayout> object;
		internal_elements.empty() || SUCCEEDED(_orig->CreateInputLayout(internal_elements.data(), static_cast<UINT>(internal_elements.size()), desc.graphics.vertex_shader.code, desc.graphics.vertex_shader.code_size, &object)))
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
bool reshade::d3d10::device_impl::create_pipeline_graphics_blend_state(const api::pipeline_desc &desc, api::pipeline *out)
{
	D3D10_BLEND_DESC internal_desc;
	internal_desc.AlphaToCoverageEnable = desc.graphics.blend_state.alpha_to_coverage;
	internal_desc.SrcBlend = convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[0]);
	internal_desc.DestBlend = convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[0]);
	internal_desc.BlendOp = convert_blend_op(desc.graphics.blend_state.color_blend_op[0]);
	internal_desc.SrcBlendAlpha = convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[0]);
	internal_desc.DestBlendAlpha = convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[0]);
	internal_desc.BlendOpAlpha = convert_blend_op(desc.graphics.blend_state.alpha_blend_op[0]);

	for (UINT i = 0; i < 8; ++i)
	{
		internal_desc.BlendEnable[i] = desc.graphics.blend_state.blend_enable[i];
		internal_desc.RenderTargetWriteMask[i] = desc.graphics.blend_state.render_target_write_mask[i];
	}

	if (com_ptr<ID3D10BlendState> object;
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
bool reshade::d3d10::device_impl::create_pipeline_graphics_rasterizer_state(const api::pipeline_desc &desc, api::pipeline *out)
{
	D3D10_RASTERIZER_DESC internal_desc;
	internal_desc.FillMode = convert_fill_mode(desc.graphics.rasterizer_state.fill_mode);
	internal_desc.CullMode = convert_cull_mode(desc.graphics.rasterizer_state.cull_mode);
	internal_desc.FrontCounterClockwise = desc.graphics.rasterizer_state.front_counter_clockwise;
	internal_desc.DepthBias = static_cast<INT>(desc.graphics.rasterizer_state.depth_bias);
	internal_desc.DepthBiasClamp = desc.graphics.rasterizer_state.depth_bias_clamp;
	internal_desc.SlopeScaledDepthBias = desc.graphics.rasterizer_state.slope_scaled_depth_bias;
	internal_desc.DepthClipEnable = desc.graphics.rasterizer_state.depth_clip;
	internal_desc.ScissorEnable = desc.graphics.rasterizer_state.scissor_test;
	internal_desc.MultisampleEnable = desc.graphics.rasterizer_state.multisample;
	internal_desc.AntialiasedLineEnable = desc.graphics.rasterizer_state.antialiased_line;

	if (com_ptr<ID3D10RasterizerState> object;
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
bool reshade::d3d10::device_impl::create_pipeline_graphics_depth_stencil_state(const api::pipeline_desc &desc, api::pipeline *out)
{
	D3D10_DEPTH_STENCIL_DESC internal_desc;
	internal_desc.DepthEnable = desc.graphics.depth_stencil_state.depth_test;
	internal_desc.DepthWriteMask = desc.graphics.depth_stencil_state.depth_write_mask ? D3D10_DEPTH_WRITE_MASK_ALL : D3D10_DEPTH_WRITE_MASK_ZERO;
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

	if (com_ptr<ID3D10DepthStencilState> object;
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

bool reshade::d3d10::device_impl::create_pipeline_layout(uint32_t num_set_layouts, const api::descriptor_set_layout *set_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out)
{
	if (num_constant_ranges > 1)
	{
		*out = { 0 };
		return false;
	}

	const auto layout_impl = new pipeline_layout_impl();
	layout_impl->shader_registers.resize(num_set_layouts + num_constant_ranges);

	for (UINT i = 0; i < num_set_layouts; ++i)
	{
		if (set_layouts[i].handle == 0)
			continue;

		layout_impl->shader_registers[i] = reinterpret_cast<descriptor_set_layout_impl *>(set_layouts[i].handle)->range.dx_shader_register;
	}

	if (num_constant_ranges == 1)
	{
		assert(constant_ranges[0].offset == 0);
		layout_impl->shader_registers[num_set_layouts] = constant_ranges[0].dx_shader_register;
	}

	*out = { reinterpret_cast<uintptr_t>(layout_impl) };
	return true;
}
bool reshade::d3d10::device_impl::create_descriptor_set_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool, api::descriptor_set_layout *out)
{
	// Can only have descriptors of a single type in a descriptor set
	if (num_ranges != 1)
	{
		*out = { 0 };
		return false;
	}

	const auto layout_impl = new descriptor_set_layout_impl();
	layout_impl->range = ranges[0];

	*out = { reinterpret_cast<uintptr_t>(layout_impl) };
	return true;
}
bool reshade::d3d10::device_impl::create_query_pool(api::query_type type, uint32_t count, api::query_pool *out)
{
	const auto result = new query_pool_impl();
	result->queries.resize(count);

	for (UINT i = 0; i < count; ++i)
	{
		D3D10_QUERY_DESC internal_desc = {};
		internal_desc.Query = convert_query_type(type);

		if (FAILED(_orig->CreateQuery(&internal_desc, &result->queries[i])))
		{
			delete result;

			*out = { 0 };
			return false;
		}
	}

	*out = { reinterpret_cast<uintptr_t>(result) };
	return true;
}
bool reshade::d3d10::device_impl::create_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, api::descriptor_set *out)
{
	const auto layout_impl = reinterpret_cast<descriptor_set_layout_impl *>(layout.handle);

	for (UINT i = 0; i < count; ++i)
	{
		const auto set = new descriptor_set_impl();
		set->type = layout_impl->range.type;
		set->descriptors.resize(layout_impl->range.count);

		out[i] = { reinterpret_cast<uintptr_t>(set) };
	}

	return true;
}

void reshade::d3d10::device_impl::destroy_sampler(api::sampler handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d10::device_impl::destroy_resource(api::resource handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d10::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

void reshade::d3d10::device_impl::destroy_pipeline(api::pipeline_type type, api::pipeline handle)
{
	if (type == api::pipeline_type::graphics)
		delete reinterpret_cast<pipeline_impl *>(handle.handle);
	else if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d10::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	delete reinterpret_cast<pipeline_layout_impl *>(handle.handle);
}
void reshade::d3d10::device_impl::destroy_descriptor_set_layout(api::descriptor_set_layout handle)
{
	delete reinterpret_cast<descriptor_set_layout_impl *>(handle.handle);
}
void reshade::d3d10::device_impl::destroy_query_pool(api::query_pool handle)
{
	delete reinterpret_cast<query_pool_impl *>(handle.handle);
}
void reshade::d3d10::device_impl::destroy_descriptor_sets(api::descriptor_set_layout, uint32_t count, const api::descriptor_set *sets)
{
	for (UINT i = 0; i < count; ++i)
		delete reinterpret_cast<descriptor_set_impl *>(sets[i].handle);
}

void reshade::d3d10::device_impl::update_descriptor_sets(uint32_t num_updates, const api::descriptor_update *updates)
{
	for (UINT i = 0; i < num_updates; ++i)
	{
		const auto set_impl = reinterpret_cast<descriptor_set_impl *>(updates[i].set.handle);

		switch (updates[i].type)
		{
		case api::descriptor_type::sampler:
			set_impl->descriptors[updates[i].binding] = updates[i].descriptor.sampler.handle;
			break;
		case api::descriptor_type::sampler_with_resource_view:
			assert(false);
			break;
		case api::descriptor_type::shader_resource_view:
			set_impl->descriptors[updates[i].binding] = updates[i].descriptor.view.handle;
			break;
		case api::descriptor_type::unordered_access_view:
			assert(false);
			break;
		case api::descriptor_type::constant_buffer:
			set_impl->descriptors[updates[i].binding] = updates[i].descriptor.resource.handle;
			break;
		}
	}
}

bool reshade::d3d10::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **data)
{
	D3D10_MAP map_type = static_cast<D3D10_MAP>(0);
	switch (access)
	{
	case api::map_access::read_only:
		map_type = D3D10_MAP_READ;
		break;
	case api::map_access::write_only:
		map_type = D3D10_MAP_WRITE;
		break;
	case api::map_access::read_write:
		map_type = D3D10_MAP_READ_WRITE;
		break;
	case api::map_access::write_discard:
		map_type = D3D10_MAP_WRITE_DISCARD;
		break;
	}

	assert(resource.handle != 0);
	const auto object = reinterpret_cast<ID3D10Resource *>(resource.handle);

	D3D10_RESOURCE_DIMENSION dimension;
	object->GetType(&dimension);
	switch (dimension)
	{
	case D3D10_RESOURCE_DIMENSION_BUFFER:
		assert(subresource == 0);
		return SUCCEEDED(static_cast<ID3D10Buffer *>(object)->Map(map_type, 0, data));
	case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
		return SUCCEEDED(static_cast<ID3D10Texture1D *>(object)->Map(subresource, map_type, 0, data));
	case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
		if (D3D10_MAPPED_TEXTURE2D mapped;
			SUCCEEDED(static_cast<ID3D10Texture2D *>(object)->Map(subresource, map_type, 0, &mapped)))
		{
			*data = mapped.pData;
			return true;
		}
		break;
	case D3D10_RESOURCE_DIMENSION_TEXTURE3D:
		if (D3D10_MAPPED_TEXTURE3D mapped;
			SUCCEEDED(static_cast<ID3D10Texture3D *>(object)->Map(subresource, map_type, 0, &mapped)))
		{
			*data = mapped.pData;
			return true;
		}
		break;
	}

	*data = 0;
	return false;
}
void reshade::d3d10::device_impl::unmap_resource(api::resource resource, uint32_t subresource)
{
	assert(resource.handle != 0);
	const auto object = reinterpret_cast<ID3D10Resource *>(resource.handle);

	D3D10_RESOURCE_DIMENSION dimension;
	object->GetType(&dimension);
	switch (dimension)
	{
	case D3D10_RESOURCE_DIMENSION_BUFFER:
		assert(subresource == 0);
		static_cast<ID3D10Buffer *>(object)->Unmap();
		break;
	case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
		static_cast<ID3D10Texture1D *>(object)->Unmap(subresource);
		break;
	case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
		static_cast<ID3D10Texture2D *>(object)->Unmap(subresource);
		break;
	case D3D10_RESOURCE_DIMENSION_TEXTURE3D:
		static_cast<ID3D10Texture3D *>(object)->Unmap(subresource);
		break;
	}
}

void reshade::d3d10::device_impl::upload_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(dst.handle != 0);
	assert(dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	const D3D10_BOX dst_box = { static_cast<UINT>(dst_offset), 0, 0, static_cast<UINT>(dst_offset + size), 1, 1 };
	_orig->UpdateSubresource(reinterpret_cast<ID3D10Resource *>(dst.handle), 0, dst_offset != 0 ? &dst_box : nullptr, data, static_cast<UINT>(size), 0);
}
void reshade::d3d10::device_impl::upload_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	assert(dst.handle != 0);

	_orig->UpdateSubresource(reinterpret_cast<ID3D10Resource *>(dst.handle), dst_subresource, reinterpret_cast<const D3D10_BOX *>(dst_box), data.data, data.row_pitch, data.slice_pitch);
}

void reshade::d3d10::device_impl::get_resource_from_view(api::resource_view view, api::resource *out_resource) const
{
	assert(view.handle != 0);
	com_ptr<ID3D10Resource> resource;
	reinterpret_cast<ID3D10View *>(view.handle)->GetResource(&resource);

	*out_resource = { reinterpret_cast<uintptr_t>(resource.get()) };
}

reshade::api::resource_desc reshade::d3d10::device_impl::get_resource_desc(api::resource resource) const
{
	assert(resource.handle != 0);
	const auto object = reinterpret_cast<ID3D10Resource *>(resource.handle);

	D3D10_RESOURCE_DIMENSION dimension;
	object->GetType(&dimension);
	switch (dimension)
	{
		case D3D10_RESOURCE_DIMENSION_BUFFER:
		{
			D3D10_BUFFER_DESC internal_desc;
			static_cast<ID3D10Buffer *>(object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
		{
			D3D10_TEXTURE1D_DESC internal_desc;
			static_cast<ID3D10Texture1D *>(object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
		{
			D3D10_TEXTURE2D_DESC internal_desc;
			static_cast<ID3D10Texture2D *>(object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3D10_RESOURCE_DIMENSION_TEXTURE3D:
		{
			D3D10_TEXTURE3D_DESC internal_desc;
			static_cast<ID3D10Texture3D *>(object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
	}

	assert(false); // Not implemented
	return api::resource_desc {};
}

bool reshade::d3d10::device_impl::get_query_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	const auto impl = reinterpret_cast<query_pool_impl *>(pool.handle);

	for (UINT i = 0; i < count; ++i)
	{
		if (FAILED(impl->queries[i + first]->GetData(static_cast<uint8_t *>(results) + i * stride, stride, D3D10_ASYNC_GETDATA_DONOTFLUSH)))
			return false;
	}

	return true;
}

void reshade::d3d10::device_impl::set_debug_name(api::resource resource, const char *name)
{
	const GUID debug_object_name_guid = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00} }; // WKPDID_D3DDebugObjectName
	reinterpret_cast<ID3D10Resource *>(resource.handle)->SetPrivateData(debug_object_name_guid, static_cast<UINT>(strlen(name)), name);
}
