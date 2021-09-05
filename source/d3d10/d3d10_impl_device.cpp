/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "d3d10_impl_device.hpp"
#include "d3d10_impl_type_convert.hpp"
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
	load_addons();

	create_global_pipeline_layout();

	invoke_addon_event<addon_event::init_device>(this);
	invoke_addon_event<addon_event::init_command_list>(this);
	invoke_addon_event<addon_event::init_command_queue>(this);
#endif
}
reshade::d3d10::device_impl::~device_impl()
{
#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_command_queue>(this);
	invoke_addon_event<addon_event::destroy_command_list>(this);
	invoke_addon_event<addon_event::destroy_device>(this);

	destroy_global_pipeline_layout();

	unload_addons();
#endif
}

#if RESHADE_ADDON
void reshade::d3d10::device_impl::create_global_pipeline_layout()
{
	// Create global pipeline layout that is used for all application descriptor events
	api::descriptor_range push_descriptors = {};
	api::pipeline_layout_param layout_params[3] = {};

	push_descriptors.type = api::descriptor_type::sampler;
	push_descriptors.array_size = D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT;
	push_descriptors.visibility = api::shader_stage::all;
	layout_params[0].type = api::pipeline_layout_param_type::push_descriptors;
	create_descriptor_set_layout(1, &push_descriptors, true, &layout_params[0].descriptor_layout);
	push_descriptors.type = api::descriptor_type::shader_resource_view;
	push_descriptors.array_size = D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	push_descriptors.visibility = api::shader_stage::all;
	layout_params[1].type = api::pipeline_layout_param_type::push_descriptors;
	create_descriptor_set_layout(1, &push_descriptors, true, &layout_params[1].descriptor_layout);
	push_descriptors.type = api::descriptor_type::constant_buffer;
	push_descriptors.array_size = D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	push_descriptors.visibility = api::shader_stage::all;
	layout_params[2].type = api::pipeline_layout_param_type::push_descriptors;
	create_descriptor_set_layout(1, &push_descriptors, true, &layout_params[2].descriptor_layout);

	create_pipeline_layout(3, layout_params, &_global_pipeline_layout);
}
void reshade::d3d10::device_impl::destroy_global_pipeline_layout()
{
	const std::vector<api::pipeline_layout_param> &layout_params = reinterpret_cast<pipeline_layout_impl *>(_global_pipeline_layout.handle)->params;

	destroy_descriptor_set_layout(layout_params[0].descriptor_layout);
	destroy_descriptor_set_layout(layout_params[1].descriptor_layout);
	destroy_descriptor_set_layout(layout_params[2].descriptor_layout);

	destroy_pipeline_layout(_global_pipeline_layout);
}
#endif

bool reshade::d3d10::device_impl::check_capability(api::device_caps capability) const
{
	switch (capability)
	{
	case api::device_caps::compute_shader:
		return false;
	case api::device_caps::geometry_shader:
		return _orig->GetFeatureLevel() >= D3D_FEATURE_LEVEL_10_0;
	case api::device_caps::hull_and_domain_shader:
	case api::device_caps::logic_op:
		return false;
	case api::device_caps::dual_src_blend:
		return true;
	case api::device_caps::independent_blend: // Supported in D3D10.1
	case api::device_caps::fill_mode_non_solid:
	case api::device_caps::bind_render_targets_and_depth_stencil:
	case api::device_caps::multi_viewport:
		return true;
	case api::device_caps::partial_push_constant_updates:
		return false;
	case api::device_caps::partial_push_descriptor_updates:
	case api::device_caps::draw_instanced:
		return true;
	case api::device_caps::draw_or_dispatch_indirect:
		return false;
	case api::device_caps::copy_buffer_region:
		return true;
	case api::device_caps::copy_buffer_to_texture:
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
	case api::device_caps::copy_query_pool_results:
		return false;
	case api::device_caps::sampler_compare:
	case api::device_caps::sampler_anisotropic:
		return true;
	case api::device_caps::sampler_with_resource_view:
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
void reshade::d3d10::device_impl::destroy_sampler(api::sampler handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
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
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}
void reshade::d3d10::device_impl::destroy_resource(api::resource handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

bool reshade::d3d10::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out)
{
	if (resource.handle == 0)
	{
		*out = { 0 };
		return false;
	}

	switch (usage_type)
	{
		case api::resource_usage::depth_stencil:
		{
			D3D10_DEPTH_STENCIL_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D10DepthStencilView> object;
				SUCCEEDED(_orig->CreateDepthStencilView(reinterpret_cast<ID3D10Resource *>(resource.handle), &internal_desc, &object)))
			{
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
				*out = { reinterpret_cast<uintptr_t>(object.release()) };
				return true;
			}
			break;
		}
		case api::resource_usage::shader_resource:
		{
			D3D10_SHADER_RESOURCE_VIEW_DESC1 internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D10ShaderResourceView1> object;
				SUCCEEDED(_orig->CreateShaderResourceView1(reinterpret_cast<ID3D10Resource *>(resource.handle), &internal_desc, &object)))
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
void reshade::d3d10::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

bool reshade::d3d10::device_impl::create_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	switch (desc.type)
	{
	case api::pipeline_stage::all_graphics:
		return create_graphics_pipeline(desc, out);
	case api::pipeline_stage::input_assembler:
		return create_input_layout(desc, out);
	case api::pipeline_stage::vertex_shader:
		return create_vertex_shader(desc, out);
	case api::pipeline_stage::geometry_shader:
		return create_geometry_shader(desc, out);
	case api::pipeline_stage::pixel_shader:
		return create_pixel_shader(desc, out);
	case api::pipeline_stage::rasterizer:
		return create_rasterizer_state(desc, out);
	case api::pipeline_stage::depth_stencil:
		return create_depth_stencil_state(desc, out);
	case api::pipeline_stage::output_merger:
		return create_blend_state(desc, out);
	default:
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d10::device_impl::create_graphics_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (desc.graphics.hull_shader.code_size != 0 ||
		desc.graphics.domain_shader.code_size != 0 ||
		desc.graphics.blend_state.logic_op_enable[0] ||
		desc.graphics.topology == api::primitive_topology::triangle_fan ||
		desc.graphics.dynamic_states[0] != api::dynamic_state::unknown)
	{
		*out = { 0 };
		return false;
	}

#define create_state_object(name, type, extra_check) \
	api::pipeline name##_handle = { 0 }; \
	if (extra_check && !create_##name(desc, &name##_handle)) { \
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
#undef create_state_object

	const auto impl = new pipeline_impl();

	impl->vs = std::move(vertex_shader);
	impl->gs = std::move(geometry_shader);
	impl->ps = std::move(pixel_shader);

	impl->input_layout = std::move(input_layout);

	impl->blend_state = std::move(blend_state);
	impl->rasterizer_state = std::move(rasterizer_state);
	impl->depth_stencil_state = std::move(depth_stencil_state);

	impl->topology = static_cast<D3D10_PRIMITIVE_TOPOLOGY>(desc.graphics.topology);
	impl->sample_mask = desc.graphics.sample_mask;
	impl->stencil_reference_value = desc.graphics.depth_stencil_state.stencil_reference_value;

	impl->blend_constant[0] = ((desc.graphics.blend_state.blend_constant      ) & 0xFF) / 255.0f;
	impl->blend_constant[1] = ((desc.graphics.blend_state.blend_constant >>  4) & 0xFF) / 255.0f;
	impl->blend_constant[2] = ((desc.graphics.blend_state.blend_constant >>  8) & 0xFF) / 255.0f;
	impl->blend_constant[3] = ((desc.graphics.blend_state.blend_constant >> 12) & 0xFF) / 255.0f;

	*out = { reinterpret_cast<uintptr_t>(impl) };
	return true;
}
bool reshade::d3d10::device_impl::create_input_layout(const api::pipeline_desc &desc, api::pipeline *out)
{
	std::vector<D3D10_INPUT_ELEMENT_DESC> internal_elements;
	convert_pipeline_desc(desc, internal_elements);

	if (com_ptr<ID3D10InputLayout> object;
		internal_elements.empty() || // Empty input layout is valid, but generates a warning, so just return success and a zero handle
		SUCCEEDED(_orig->CreateInputLayout(internal_elements.data(), static_cast<UINT>(internal_elements.size()), desc.graphics.vertex_shader.code, desc.graphics.vertex_shader.code_size, &object)))
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
bool reshade::d3d10::device_impl::create_vertex_shader(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (com_ptr<ID3D10VertexShader> object;
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
bool reshade::d3d10::device_impl::create_geometry_shader(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (com_ptr<ID3D10GeometryShader> object;
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
bool reshade::d3d10::device_impl::create_pixel_shader(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (com_ptr<ID3D10PixelShader> object;
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
bool reshade::d3d10::device_impl::create_blend_state(const api::pipeline_desc &desc, api::pipeline *out)
{
	D3D10_BLEND_DESC1 internal_desc = {};
	convert_pipeline_desc(desc, internal_desc);

	if (com_ptr<ID3D10BlendState1> object;
		SUCCEEDED(_orig->CreateBlendState1(&internal_desc, &object)))
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
bool reshade::d3d10::device_impl::create_rasterizer_state(const api::pipeline_desc &desc, api::pipeline *out)
{
	D3D10_RASTERIZER_DESC internal_desc = {};
	convert_pipeline_desc(desc, internal_desc);

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
bool reshade::d3d10::device_impl::create_depth_stencil_state(const api::pipeline_desc &desc, api::pipeline *out)
{
	D3D10_DEPTH_STENCIL_DESC internal_desc = {};
	convert_pipeline_desc(desc, internal_desc);

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
void reshade::d3d10::device_impl::destroy_pipeline(api::pipeline_stage type, api::pipeline handle)
{
	if (type == api::pipeline_stage::all_graphics)
		delete reinterpret_cast<pipeline_impl *>(handle.handle);
	else if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

bool reshade::d3d10::device_impl::create_pipeline_layout(uint32_t count, const api::pipeline_layout_param *params, api::pipeline_layout *out)
{
	bool success = true;

	const auto impl = new pipeline_layout_impl();
	impl->params.assign(params, params + count);
	impl->shader_registers.resize(count);

	for (uint32_t i = 0; i < count && success; ++i)
	{
		if (params[i].type != api::pipeline_layout_param_type::push_constants)
		{
			const auto set_layout_impl = reinterpret_cast<const descriptor_set_layout_impl *>(params[i].descriptor_layout.handle);

			impl->shader_registers[i] = set_layout_impl->range.dx_register_index;
		}
		else
		{
			if (params[i].push_constants.dx_register_space != 0)
				success = false;

			impl->shader_registers[i] = params[i].push_constants.dx_register_index;
		}
	}

	if (success)
	{
		*out = { reinterpret_cast<uintptr_t>(impl) };
		return true;
	}
	else
	{
		delete impl;

		*out = { 0 };
		return false;
	}
}
void reshade::d3d10::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	delete reinterpret_cast<pipeline_layout_impl *>(handle.handle);
}

bool reshade::d3d10::device_impl::create_descriptor_set_layout(uint32_t count, const api::descriptor_range *bindings, bool, api::descriptor_set_layout *out)
{
	bool success = true;
	api::descriptor_range merged_range = count ? bindings[0] : api::descriptor_range {};

	for (uint32_t i = 1; i < count && success; ++i)
	{
		if (bindings[i].type != merged_range.type || bindings[i].dx_register_space != 0)
			success = false;

		if (bindings[i].offset >= merged_range.offset)
		{
			const uint32_t distance = bindings[i].offset - merged_range.offset;

			if ((bindings[i].dx_register_index - merged_range.dx_register_index) != distance)
				success = false;

			merged_range.array_size += distance;
			merged_range.visibility |= bindings[i].visibility;
		}
		else
		{
			const uint32_t distance = merged_range.offset - bindings[i].offset;

			if ((merged_range.dx_register_index - bindings[i].dx_register_index) != distance)
				success = false;

			merged_range.offset = bindings[i].offset;
			merged_range.binding = bindings[i].binding;
			merged_range.dx_register_index = bindings[i].dx_register_index;
			merged_range.array_size += distance;
			merged_range.visibility |= bindings[i].visibility;
		}
	}

	if (success)
	{
		const auto impl = new descriptor_set_layout_impl();
		impl->range = merged_range;

		*out = { reinterpret_cast<uintptr_t>(impl) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
void reshade::d3d10::device_impl::destroy_descriptor_set_layout(api::descriptor_set_layout handle)
{
	delete reinterpret_cast<descriptor_set_layout_impl *>(handle.handle);
}

bool reshade::d3d10::device_impl::create_query_pool(api::query_type type, uint32_t size, api::query_pool *out)
{
	const auto impl = new query_pool_impl();
	impl->queries.resize(size);

	D3D10_QUERY_DESC internal_desc = {};
	internal_desc.Query = convert_query_type(type);

	for (UINT i = 0; i < size; ++i)
	{
		if (FAILED(_orig->CreateQuery(&internal_desc, &impl->queries[i])))
		{
			delete impl;

			*out = { 0 };
			return false;
		}
	}

	*out = { reinterpret_cast<uintptr_t>(impl) };
	return true;
}
void reshade::d3d10::device_impl::destroy_query_pool(api::query_pool handle)
{
	delete reinterpret_cast<query_pool_impl *>(handle.handle);
}

bool reshade::d3d10::device_impl::create_render_pass(const api::render_pass_desc &, api::render_pass *out)
{
	*out = { 0 };
	return true;
}
void reshade::d3d10::device_impl::destroy_render_pass(api::render_pass handle)
{
	assert(handle.handle == 0);
}

bool reshade::d3d10::device_impl::create_framebuffer(const api::framebuffer_desc &desc, api::framebuffer *out)
{
	const auto impl = new framebuffer_impl();
	impl->dsv = reinterpret_cast<ID3D10DepthStencilView *>(desc.depth_stencil.handle);
	for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT && desc.render_targets[i].handle != 0; ++i, ++impl->count)
		impl->rtv[i] = reinterpret_cast<ID3D10RenderTargetView *>(desc.render_targets[i].handle);

	*out = { reinterpret_cast<uintptr_t>(impl) };
	return true;
}
void reshade::d3d10::device_impl::destroy_framebuffer(api::framebuffer handle)
{
	delete reinterpret_cast<framebuffer_impl *>(handle.handle);
}

bool reshade::d3d10::device_impl::create_descriptor_sets(uint32_t count, const api::descriptor_set_layout *layouts, api::descriptor_set *out)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto set_layout_impl = reinterpret_cast<const descriptor_set_layout_impl *>(layouts[i].handle);
		assert(set_layout_impl != nullptr);

		const auto impl = new descriptor_set_impl();
		impl->type = set_layout_impl->range.type;
		impl->count = set_layout_impl->range.array_size;

		switch (impl->type)
		{
		case api::descriptor_type::sampler:
		case api::descriptor_type::shader_resource_view:
			impl->descriptors.resize(impl->count * 1);
			break;
		case api::descriptor_type::constant_buffer:
			impl->descriptors.resize(impl->count * 3);
			break;
		default:
			assert(false);
			break;
		}

		out[i] = { reinterpret_cast<uintptr_t>(impl) };
	}

	return true;
}
void reshade::d3d10::device_impl::destroy_descriptor_sets(uint32_t count, const api::descriptor_set *sets)
{
	for (uint32_t i = 0; i < count; ++i)
		delete reinterpret_cast<descriptor_set_impl *>(sets[i].handle);
}

bool reshade::d3d10::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access access, api::subresource_data *out_data)
{
	static_assert(sizeof(api::subresource_data) == sizeof(D3D10_MAPPED_TEXTURE3D));

	D3D10_MAP map_type = static_cast<D3D10_MAP>(0);
	switch (access)
	{
	case api::map_access::read_only:
		map_type = D3D10_MAP_READ;
		break;
	case api::map_access::write_only:
		// Use no overwrite flag to simulate D3D12 behavior of there only being one allocation that backs a buffer (instead of the runtime managing multiple ones behind the scenes)
		map_type = D3D10_MAP_WRITE_NO_OVERWRITE;
		break;
	case api::map_access::read_write:
		map_type = D3D10_MAP_READ_WRITE;
		break;
	case api::map_access::write_discard:
		map_type = D3D10_MAP_WRITE_DISCARD;
		break;
	}

	assert(out_data != nullptr);
	out_data->data = nullptr;
	out_data->row_pitch = 0;
	out_data->slice_pitch = 0;

	assert(resource.handle != 0);
	const auto object = reinterpret_cast<ID3D10Resource *>(resource.handle);

	D3D10_RESOURCE_DIMENSION dimension;
	object->GetType(&dimension);
	switch (dimension)
	{
	case D3D10_RESOURCE_DIMENSION_BUFFER:
		assert(subresource == 0);
		return SUCCEEDED(static_cast<ID3D10Buffer *>(object)->Map(map_type, 0, &out_data->data));
	case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
		return SUCCEEDED(static_cast<ID3D10Texture1D *>(object)->Map(subresource, map_type, 0, &out_data->data));
	case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
		return SUCCEEDED(static_cast<ID3D10Texture2D *>(object)->Map(subresource, map_type, 0, reinterpret_cast<D3D10_MAPPED_TEXTURE2D *>(out_data)));
	case D3D10_RESOURCE_DIMENSION_TEXTURE3D:
		return SUCCEEDED(static_cast<ID3D10Texture3D *>(object)->Map(subresource, map_type, 0, reinterpret_cast<D3D10_MAPPED_TEXTURE3D *>(out_data)));
	default:
		return false;
	}
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

void reshade::d3d10::device_impl::update_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(dst.handle != 0);
	assert(dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	const D3D10_BOX dst_box = { static_cast<UINT>(dst_offset), 0, 0, static_cast<UINT>(dst_offset + size), 1, 1 };
	_orig->UpdateSubresource(reinterpret_cast<ID3D10Resource *>(dst.handle), 0, dst_offset != 0 ? &dst_box : nullptr, data, static_cast<UINT>(size), 0);
}
void reshade::d3d10::device_impl::update_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	assert(dst.handle != 0);

	_orig->UpdateSubresource(reinterpret_cast<ID3D10Resource *>(dst.handle), dst_subresource, reinterpret_cast<const D3D10_BOX *>(dst_box), data.data, data.row_pitch, data.slice_pitch);
}

void reshade::d3d10::device_impl::update_descriptor_sets(uint32_t count, const api::descriptor_set_update *updates)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto impl = reinterpret_cast<descriptor_set_impl *>(updates[i].set.handle);

		const api::descriptor_set_update &update = updates[i];

		switch (update.type)
		{
		case api::descriptor_type::sampler:
		case api::descriptor_type::shader_resource_view:
			std::memcpy(&impl->descriptors[update.offset * 1], update.descriptors, update.count * sizeof(uint64_t) * 1);
			break;
		case api::descriptor_type::sampler_with_resource_view:
		case api::descriptor_type::unordered_access_view:
			assert(false);
			break;
		case api::descriptor_type::constant_buffer:
			std::memcpy(&impl->descriptors[update.offset * 3], update.descriptors, update.count * sizeof(uint64_t) * 3);
			break;
		}
	}
}

bool reshade::d3d10::device_impl::get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(pool.handle != 0);
	const auto impl = reinterpret_cast<query_pool_impl *>(pool.handle);

	for (uint32_t i = 0; i < count; ++i)
	{
		if (FAILED(impl->queries[first + i]->GetData(static_cast<uint8_t *>(results) + i * stride, stride, D3D10_ASYNC_GETDATA_DONOTFLUSH)))
			return false;
	}

	return true;
}

void reshade::d3d10::device_impl::set_resource_name(api::resource resource, const char *name)
{
	assert(resource.handle != 0);

	constexpr GUID debug_object_name_guid = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00} }; // WKPDID_D3DDebugObjectName
	reinterpret_cast<ID3D10Resource *>(resource.handle)->SetPrivateData(debug_object_name_guid, static_cast<UINT>(strlen(name)), name);
}

void reshade::d3d10::device_impl::get_pipeline_layout_desc(api::pipeline_layout layout, uint32_t *count, api::pipeline_layout_param *params) const
{
	assert(layout.handle != 0 && count != nullptr);
	const auto layout_impl = reinterpret_cast<const pipeline_layout_impl *>(layout.handle);

	if (params != nullptr)
	{
		*count = std::min(*count, static_cast<uint32_t>(layout_impl->params.size()));
		std::memcpy(params, layout_impl->params.data(), *count * sizeof(api::pipeline_layout_param));
	}
	else
	{
		*count = static_cast<uint32_t>(layout_impl->params.size());
	}
}
void reshade::d3d10::device_impl::get_descriptor_pool_offset(api::descriptor_set, api::descriptor_pool *pool, uint32_t *offset) const
{
	*pool = { 0 };
	*offset = 0; // Unsupported
}
void reshade::d3d10::device_impl::get_descriptor_set_layout_desc(api::descriptor_set_layout layout, uint32_t *count, api::descriptor_range *bindings) const
{
	assert(layout.handle != 0 && count != nullptr);
	const auto layout_impl = reinterpret_cast<descriptor_set_layout_impl *>(layout.handle);

	if (bindings != nullptr)
	{
		if (*count != 0)
		{
			*count = 1;
			*bindings = layout_impl->range;
		}
	}
	else
	{
		*count = 1;
	}
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
reshade::api::resource      reshade::d3d10::device_impl::get_resource_from_view(api::resource_view view) const
{
	assert(view.handle != 0);
	com_ptr<ID3D10Resource> resource;
	reinterpret_cast<ID3D10View *>(view.handle)->GetResource(&resource);

	return { reinterpret_cast<uintptr_t>(resource.get()) };
}

reshade::api::resource_view reshade::d3d10::device_impl::get_framebuffer_attachment(api::framebuffer fbo, api::attachment_type type, uint32_t index) const
{
	assert(fbo.handle != 0);
	const auto fbo_impl = reinterpret_cast<const framebuffer_impl *>(fbo.handle);

	if (type == api::attachment_type::color)
	{
		if (index < fbo_impl->count)
		{
			return { reinterpret_cast<uintptr_t>(fbo_impl->rtv[index]) };
		}
	}
	else
	{
		if (fbo_impl->dsv != nullptr)
		{
			return { reinterpret_cast<uintptr_t>(fbo_impl->dsv) };
		}
	}

	return { 0 };
}
