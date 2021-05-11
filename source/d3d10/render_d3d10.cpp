/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d10.hpp"
#include "render_d3d10_utils.hpp"
#include <algorithm>

namespace
{
	struct pipeline_layout_impl
	{
		com_ptr<ID3D10Buffer> push_constants;
		UINT push_constants_binding;
	};

	struct pipeline_graphics_impl
	{
		com_ptr<ID3D10VertexShader> vs;
		com_ptr<ID3D10GeometryShader> gs;
		com_ptr<ID3D10PixelShader> ps;

		com_ptr<ID3D10InputLayout> input_layout;
		com_ptr<ID3D10BlendState> blend_state;
		com_ptr<ID3D10RasterizerState> rasterizer_state;
		com_ptr<ID3D10DepthStencilState> depth_stencil_state;

		D3D10_PRIMITIVE_TOPOLOGY topology;
		UINT sample_mask;
		UINT stencil_reference_value;
		FLOAT blend_constant[4];
	};

	const GUID vertex_shader_byte_code_guid = { 0xB2257A30, 0x4014, 0x46EA, { 0xBD, 0x88, 0xDE, 0xC2, 0x1D, 0xB6, 0xA0, 0x2B } };
}

reshade::d3d10::device_impl::device_impl(ID3D10Device1 *device) :
	api_object_impl(device)
{
#if RESHADE_ADDON
	addon::load_addons();
#endif

	invoke_addon_event<addon_event::init_device>(this);
	invoke_addon_event<addon_event::init_command_queue>(this);
}
reshade::d3d10::device_impl::~device_impl()
{
	invoke_addon_event<addon_event::destroy_command_queue>(this);
	invoke_addon_event<addon_event::destroy_device>(this);

#if RESHADE_ADDON
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
	case api::device_caps::tessellation_shaders:
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

bool reshade::d3d10::device_impl::check_resource_handle_valid(api::resource handle) const
{
	return handle.handle != 0 && _resources.has_object(reinterpret_cast<ID3D10Resource *>(handle.handle));
}
bool reshade::d3d10::device_impl::check_resource_view_handle_valid(api::resource_view handle) const
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
		return create_pipeline_graphics_all(desc, out);
	case api::pipeline_type::graphics_blend_state:
		return create_pipeline_graphics_blend_state(desc, out);
	case api::pipeline_type::graphics_rasterizer_state:
		return create_pipeline_graphics_rasterizer_state(desc, out);
	case api::pipeline_type::graphics_depth_stencil_state:
		return create_pipeline_graphics_depth_stencil_state(desc, out);
	}
}
bool reshade::d3d10::device_impl::create_pipeline_graphics_all(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (desc.graphics.hull_shader.handle != 0 ||
		desc.graphics.domain_shader.handle != 0 ||
		desc.graphics.num_dynamic_states != 0)
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

	std::vector<D3D10_INPUT_ELEMENT_DESC> internal_elements;
	internal_elements.reserve(16);
	for (UINT i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
	{
		const auto &element = desc.graphics.input_layout[i];
		D3D10_INPUT_ELEMENT_DESC &internal_element = internal_elements.emplace_back();

		internal_element.SemanticName = element.semantic;
		internal_element.SemanticIndex = element.semantic_index;
		internal_element.Format = convert_format(element.format);
		internal_element.InputSlot = element.buffer_binding;
		internal_element.AlignedByteOffset = element.offset;
		internal_element.InputSlotClass = element.instance_step_rate > 0 ? D3D10_INPUT_PER_INSTANCE_DATA : D3D10_INPUT_PER_VERTEX_DATA;
		internal_element.InstanceDataStepRate = element.instance_step_rate;
	}

	std::vector<uint8_t> bytecode;
	if (desc.graphics.vertex_shader.handle != 0)
	{
		const auto vertex_shader = reinterpret_cast<ID3D10VertexShader *>(desc.graphics.vertex_shader.handle);

		UINT bytecode_len = 0;
		vertex_shader->GetPrivateData(vertex_shader_byte_code_guid, &bytecode_len, nullptr);
		bytecode.resize(bytecode_len);
		vertex_shader->GetPrivateData(vertex_shader_byte_code_guid, &bytecode_len, bytecode.data());
	}

	com_ptr<ID3D10InputLayout> input_layout;
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

	state->vs = reinterpret_cast<ID3D10VertexShader *>(desc.graphics.vertex_shader.handle);
	state->gs = reinterpret_cast<ID3D10GeometryShader *>(desc.graphics.geometry_shader.handle);
	state->ps = reinterpret_cast<ID3D10PixelShader *>(desc.graphics.pixel_shader.handle);

	state->input_layout = std::move(input_layout);

	state->blend_state = reinterpret_cast<ID3D10BlendState *>(blend_state.handle);
	state->rasterizer_state = reinterpret_cast<ID3D10RasterizerState *>(rasterizer_state.handle);
	state->depth_stencil_state = reinterpret_cast<ID3D10DepthStencilState *>(depth_stencil_state.handle);

	state->topology = static_cast<D3D10_PRIMITIVE_TOPOLOGY>(desc.graphics.rasterizer_state.topology);
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
bool reshade::d3d10::device_impl::create_pipeline_graphics_blend_state(const api::pipeline_desc &desc, api::pipeline *out)
{
	D3D10_BLEND_DESC internal_desc;
	internal_desc.AlphaToCoverageEnable = desc.graphics.multisample_state.alpha_to_coverage;
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
	internal_desc.MultisampleEnable = desc.graphics.multisample_state.multisample;
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

bool reshade::d3d10::device_impl::create_shader_module(api::shader_stage type, api::shader_format format, const char *entry_point, const void *code, size_t code_size, api::shader_module *out)
{
	if (format == api::shader_format::dxbc)
	{
		assert(entry_point == nullptr);

		switch (type)
		{
		case api::shader_stage::vertex:
			if (com_ptr<ID3D10VertexShader> object;
				SUCCEEDED(_orig->CreateVertexShader(code, code_size, &object)))
			{
				assert(code_size <= std::numeric_limits<UINT>::max());
				object->SetPrivateData(vertex_shader_byte_code_guid, static_cast<UINT>(code_size), code);

				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		case api::shader_stage::geometry:
			if (com_ptr<ID3D10GeometryShader> object;
				SUCCEEDED(_orig->CreateGeometryShader(code, code_size, &object)))
			{
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		case api::shader_stage::pixel:
			if (com_ptr<ID3D10PixelShader> object;
				SUCCEEDED(_orig->CreatePixelShader(code, code_size, &object)))
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
bool reshade::d3d10::device_impl::create_pipeline_layout(uint32_t num_table_layouts, const api::descriptor_table_layout *table_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out)
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

		D3D10_BUFFER_DESC desc = {};
		desc.ByteWidth = constant_ranges[0].count * 4;
		desc.Usage = D3D10_USAGE_DYNAMIC;
		desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

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
bool reshade::d3d10::device_impl::create_descriptor_heap(uint32_t, uint32_t, const api::descriptor_heap_size *, api::descriptor_heap *out)
{
	assert(false);

	*out = { 0 };
	return false;
}
bool reshade::d3d10::device_impl::create_descriptor_tables(api::descriptor_heap, api::descriptor_table_layout, uint32_t, api::descriptor_table *out)
{
	assert(false);

	*out = { 0 };
	return false;
}
bool reshade::d3d10::device_impl::create_descriptor_table_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_table_layout *out)
{
	*out = { 0 };
	return push_descriptors;
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
		delete reinterpret_cast<pipeline_graphics_impl *>(handle.handle);
	else if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d10::device_impl::destroy_shader_module(api::shader_module handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d10::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	delete reinterpret_cast<pipeline_layout_impl *>(handle.handle);
}
void reshade::d3d10::device_impl::destroy_descriptor_heap(api::descriptor_heap)
{
	assert(false);
}
void reshade::d3d10::device_impl::destroy_descriptor_table_layout(api::descriptor_table_layout)
{
}

void reshade::d3d10::device_impl::update_descriptor_tables(uint32_t, const api::descriptor_update *)
{
	assert(false);
}

bool reshade::d3d10::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **mapped_ptr)
{
	D3D10_MAP flags = static_cast<D3D10_MAP>(0);
	switch (access)
	{
	case api::map_access::read_only:
		flags = D3D10_MAP_READ;
		break;
	case api::map_access::write_only:
		flags = D3D10_MAP_WRITE;
		break;
	case api::map_access::read_write:
		flags = D3D10_MAP_READ_WRITE;
		break;
	case api::map_access::write_discard:
		flags = D3D10_MAP_WRITE_DISCARD;
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
		return SUCCEEDED(static_cast<ID3D10Buffer *>(object)->Map(flags, 0, mapped_ptr));
	case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
		return SUCCEEDED(static_cast<ID3D10Texture1D *>(object)->Map(subresource, flags, 0, mapped_ptr));
	case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
		if (D3D10_MAPPED_TEXTURE2D mapped;
			SUCCEEDED(static_cast<ID3D10Texture2D *>(object)->Map(subresource, flags, 0, &mapped)))
		{
			*mapped_ptr = mapped.pData;
			return true;
		}
		break;
	case D3D10_RESOURCE_DIMENSION_TEXTURE3D:
		if (D3D10_MAPPED_TEXTURE3D mapped;
			SUCCEEDED(static_cast<ID3D10Texture3D *>(object)->Map(subresource, flags, 0, &mapped)))
		{
			*mapped_ptr = mapped.pData;
			return true;
		}
		break;
	}

	*mapped_ptr = 0;
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

void reshade::d3d10::device_impl::upload_buffer_region(api::resource dst, uint64_t dst_offset, const void *data, uint64_t size)
{
	assert(dst.handle != 0);
	assert(dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	const D3D10_BOX dst_box = { static_cast<UINT>(dst_offset), 0, 0, static_cast<UINT>(dst_offset + size), 1, 1 };
	_orig->UpdateSubresource(reinterpret_cast<ID3D10Resource *>(dst.handle), 0, &dst_box, data, static_cast<UINT>(size), static_cast<UINT>(size));
}
void reshade::d3d10::device_impl::upload_texture_region(api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], const void *data, uint32_t row_pitch, uint32_t slice_pitch)
{
	assert(dst.handle != 0);
	_orig->UpdateSubresource(reinterpret_cast<ID3D10Resource *>(dst.handle), dst_subresource, reinterpret_cast<const D3D10_BOX *>(dst_box), data, row_pitch, slice_pitch);
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
		default:
		{
			assert(false); // Not implemented
			return api::resource_desc {};
		}
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
}

void reshade::d3d10::device_impl::flush_immediate_command_list() const
{
	_orig->Flush();
}

void reshade::d3d10::device_impl::bind_pipeline(api::pipeline_type type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);

	switch (type)
	{
	case api::pipeline_type::graphics: {
		const auto state = reinterpret_cast<pipeline_graphics_impl *>(pipeline.handle);
		_orig->VSSetShader(state->vs.get());
		_orig->GSSetShader(state->gs.get());
		_orig->PSSetShader(state->ps.get());
		_orig->IASetInputLayout(state->input_layout.get());
		_orig->IASetPrimitiveTopology(state->topology);
		_orig->OMSetBlendState(state->blend_state.get(), state->blend_constant, state->sample_mask);
		_orig->RSSetState(state->rasterizer_state.get());
		_orig->OMSetDepthStencilState(state->depth_stencil_state.get(), state->stencil_reference_value);
		break;
	}
	case api::pipeline_type::graphics_blend_state:
		_orig->OMSetBlendState(reinterpret_cast<ID3D10BlendState *>(pipeline.handle), nullptr, D3D10_DEFAULT_SAMPLE_MASK);
		break;
	case api::pipeline_type::graphics_rasterizer_state:
		_orig->RSSetState(reinterpret_cast<ID3D10RasterizerState *>(pipeline.handle));
		break;
	case api::pipeline_type::graphics_depth_stencil_state:
		_orig->OMSetDepthStencilState(reinterpret_cast<ID3D10DepthStencilState *>(pipeline.handle), 0);
		break;
	default:
		assert(false);
		break;
	}
}
void reshade::d3d10::device_impl::bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values)
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
void reshade::d3d10::device_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	assert(first == 0);

	if (count > D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
	{
		assert(false);
		count = D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	}

	D3D10_VIEWPORT viewport_data[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	for (UINT i = 0, k = 0; i < count; ++i, k += 6)
	{
		viewport_data[i].TopLeftX = static_cast<INT>(viewports[k + 0]);
		viewport_data[i].TopLeftY = static_cast<INT>(viewports[k + 1]);
		viewport_data[i].Width = static_cast<UINT>(viewports[k + 2]);
		viewport_data[i].Height = static_cast<UINT>(viewports[k + 3]);
		viewport_data[i].MinDepth = viewports[k + 4];
		viewport_data[i].MaxDepth = viewports[k + 5];
	}

	_orig->RSSetViewports(count, viewport_data);
}
void reshade::d3d10::device_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	assert(first == 0);

	_orig->RSSetScissorRects(count, reinterpret_cast<const D3D10_RECT *>(rects));
}

void reshade::d3d10::device_impl::bind_samplers(api::shader_stage stage, uint32_t first, uint32_t count, const api::sampler *samplers)
{
	if (count > D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT)
	{
		assert(false);
		count = D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D10SamplerState *sampler_ptrs[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		sampler_ptrs[i] = reinterpret_cast<ID3D10SamplerState *>(samplers[i].handle);
#else
	const auto sampler_ptrs = reinterpret_cast<ID3D10SamplerState *const *>(samplers);
#endif

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetSamplers(first, count, sampler_ptrs);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetSamplers(first, count, sampler_ptrs);
}
void reshade::d3d10::device_impl::bind_shader_resource_views(api::shader_stage stage, uint32_t first, uint32_t count, const api::resource_view *views)
{
	if (count > D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
	{
		assert(false);
		count = D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D10ShaderResourceView *view_ptrs[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		view_ptrs[i] = reinterpret_cast<ID3D10ShaderResourceView *>(views[i].handle);
#else
	const auto view_ptrs = reinterpret_cast<ID3D10ShaderResourceView *const *>(views);
#endif

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetShaderResources(first, count, view_ptrs);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetShaderResources(first, count, view_ptrs);
}
void reshade::d3d10::device_impl::bind_constant_buffers(api::shader_stage stage, uint32_t first, uint32_t count, const api::resource *buffers)
{
	if (count > D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
	{
		assert(false);
		count = D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D10Buffer *buffer_ptrs[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		buffer_ptrs[i] = reinterpret_cast<ID3D10Buffer *>(buffers[i].handle);
#else
	const auto buffer_ptrs = reinterpret_cast<ID3D10Buffer *const *>(buffers);
#endif

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetConstantBuffers(first, count, buffer_ptrs);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetConstantBuffers(first, count, buffer_ptrs);
}

void reshade::d3d10::device_impl::push_constants(api::shader_stage stage, api::pipeline_layout layout, uint32_t, uint32_t first, uint32_t count, const uint32_t *values)
{
	assert(layout.handle != 0);
	pipeline_layout_impl *const layout_impl = reinterpret_cast<pipeline_layout_impl *>(layout.handle);
	const auto push_constants = layout_impl->push_constants.get();

	if (void *mapped_ptr;
		SUCCEEDED(push_constants->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped_ptr))) // TODO: No discard
	{
		std::memcpy(static_cast<uint32_t *>(mapped_ptr) + first, values, count * sizeof(uint32_t));

		push_constants->Unmap();
	}

	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->VSSetConstantBuffers(layout_impl->push_constants_binding, 1, &push_constants);
	if ((stage & api::shader_stage::geometry) == api::shader_stage::geometry)
		_orig->GSSetConstantBuffers(layout_impl->push_constants_binding, 1, &push_constants);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->PSSetConstantBuffers(layout_impl->push_constants_binding, 1, &push_constants);
}
void reshade::d3d10::device_impl::push_descriptors(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
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
		assert(false);
		break;
	case api::descriptor_type::constant_buffer:
		bind_constant_buffers(stage, first, count, static_cast<const api::resource *>(descriptors));
		break;
	}
}
void reshade::d3d10::device_impl::bind_descriptor_heaps(uint32_t, const api::descriptor_heap *)
{
	assert(false);
}
void reshade::d3d10::device_impl::bind_descriptor_tables(api::pipeline_type, api::pipeline_layout, uint32_t, uint32_t, const api::descriptor_table *)
{
	assert(false);
}

void reshade::d3d10::device_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	assert(offset <= std::numeric_limits<UINT>::max());
	assert(buffer.handle == 0 || index_size == 2 || index_size == 4);

	_orig->IASetIndexBuffer(reinterpret_cast<ID3D10Buffer *>(buffer.handle), index_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, static_cast<UINT>(offset));
}
void reshade::d3d10::device_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	if (count > D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
	{
		assert(false);
		count = D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
	}

#ifndef WIN64
	ID3D10Buffer *buffer_ptrs[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		buffer_ptrs[i] = reinterpret_cast<ID3D10Buffer *>(buffers[i].handle);
#else
	const auto buffer_ptrs = reinterpret_cast<ID3D10Buffer *const *>(buffers);
#endif

	UINT offsets_32[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		offsets_32[i] = static_cast<UINT>(offsets[i]);

	_orig->IASetVertexBuffers(first, count, buffer_ptrs, strides, offsets_32);
}

void reshade::d3d10::device_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	_orig->DrawInstanced(vertices, instances, first_vertex, first_instance);
}
void reshade::d3d10::device_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_orig->DrawIndexedInstanced(indices, instances, first_index, vertex_offset, first_instance);
}
void reshade::d3d10::device_impl::dispatch(uint32_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d10::device_impl::draw_or_dispatch_indirect(uint32_t, api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d10::device_impl::begin_render_pass(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	if (count > D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT)
	{
		assert(false);
		count = D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT;
	}

#ifndef WIN64
	ID3D10RenderTargetView *rtv_ptrs[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for (UINT i = 0; i < count; ++i)
		rtv_ptrs[i] = reinterpret_cast<ID3D10RenderTargetView *>(rtvs[i].handle);
#else
	const auto rtv_ptrs = reinterpret_cast<ID3D10RenderTargetView *const *>(rtvs);
#endif

	_orig->OMSetRenderTargets(count, rtv_ptrs, reinterpret_cast<ID3D10DepthStencilView *>(dsv.handle));
}
void reshade::d3d10::device_impl::end_render_pass()
{
	// Reset render targets
	_orig->OMSetRenderTargets(0, nullptr, nullptr);
}

void reshade::d3d10::device_impl::blit(api::resource, uint32_t, const int32_t[6], api::resource, uint32_t, const int32_t[6], api::texture_filter)
{
	assert(false);
}
void reshade::d3d10::device_impl::resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], api::format format)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert(src_offset == nullptr && dst_offset == nullptr && size == nullptr);

	_orig->ResolveSubresource(
		reinterpret_cast<ID3D10Resource *>(dst.handle), dst_subresource,
		reinterpret_cast<ID3D10Resource *>(src.handle), src_subresource, convert_format(format));
}
void reshade::d3d10::device_impl::copy_resource(api::resource src, api::resource dst)
{
	assert(src.handle != 0 && dst.handle != 0);

	_orig->CopyResource(reinterpret_cast<ID3D10Resource *>(dst.handle), reinterpret_cast<ID3D10Resource *>(src.handle));
}
void reshade::d3d10::device_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert(src_offset <= std::numeric_limits<UINT>::max() && dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	const D3D10_BOX src_box = { static_cast<UINT>(src_offset), 0, 0, static_cast<UINT>(src_offset + size), 1, 1 };

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D10Resource *>(dst.handle), 0, static_cast<UINT>(dst_offset), 0, 0,
		reinterpret_cast<ID3D10Resource *>(src.handle), 0, &src_box);
}
void reshade::d3d10::device_impl::copy_buffer_to_texture(api::resource, uint64_t, uint32_t, uint32_t, api::resource, uint32_t, const int32_t[6])
{
	assert(false);
}
void reshade::d3d10::device_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3])
{
	assert(src.handle != 0 && dst.handle != 0);

	D3D10_BOX src_box;
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
		const api::resource_desc desc = get_resource_desc(src);
		src_box.right = src_box.left + std::max(1u, desc.texture.width >> (src_subresource % desc.texture.levels));
		src_box.bottom = src_box.top + std::max(1u, desc.texture.height >> (src_subresource % desc.texture.levels));
		src_box.back = src_box.front + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.texture.depth_or_layers) >> (src_subresource % desc.texture.levels)) : 1u);
	}

	_orig->CopySubresourceRegion(
		reinterpret_cast<ID3D10Resource *>(dst.handle), src_subresource, dst_offset != nullptr ? dst_offset[0] : 0, dst_offset != nullptr ? dst_offset[1] : 0, dst_offset != nullptr ? dst_offset[2] : 0,
		reinterpret_cast<ID3D10Resource *>(src.handle), dst_subresource, &src_box);
}
void reshade::d3d10::device_impl::copy_texture_to_buffer(api::resource, uint32_t, const int32_t[6], api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d10::device_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);

	_orig->GenerateMips(reinterpret_cast<ID3D10ShaderResourceView *>(srv.handle));
}

void reshade::d3d10::device_impl::clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	assert(dsv.handle != 0);

	_orig->ClearDepthStencilView(reinterpret_cast<ID3D10DepthStencilView *>(dsv.handle), clear_flags, depth, stencil);
}
void reshade::d3d10::device_impl::clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4])
{
	for (UINT i = 0; i < count; ++i)
	{
		assert(rtvs[i].handle != 0);

		_orig->ClearRenderTargetView(reinterpret_cast<ID3D10RenderTargetView *>(rtvs[i].handle), color);
	}
}
void reshade::d3d10::device_impl::clear_unordered_access_view_uint(api::resource_view, const uint32_t[4])
{
	assert(false);
}
void reshade::d3d10::device_impl::clear_unordered_access_view_float(api::resource_view, const float[4])
{
	assert(false);
}

void reshade::d3d10::device_impl::begin_debug_event(const char *, const float[4])
{
}
void reshade::d3d10::device_impl::end_debug_event()
{
}
void reshade::d3d10::device_impl::insert_debug_marker(const char *, const float[4])
{
}
