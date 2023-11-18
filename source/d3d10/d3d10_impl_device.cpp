/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_impl_device.hpp"
#include "d3d10_impl_type_convert.hpp"
#include "d3d10_resource_call_vtable.inl"
#include <algorithm>
#include <utf8/unchecked.h>

reshade::d3d10::device_impl::device_impl(ID3D10Device1 *device) :
	api_object_impl(device)
{
}

reshade::api::device_properties reshade::d3d10::device_impl::get_properties() const
{
	api::device_properties props;
	props.api_version = _orig->GetFeatureLevel();

	if (com_ptr<IDXGIDevice> dxgi_device;
		SUCCEEDED(_orig->QueryInterface(&dxgi_device)))
	{
		if (com_ptr<IDXGIAdapter> dxgi_adapter;
			SUCCEEDED(dxgi_device->GetAdapter(&dxgi_adapter)))
		{
			LARGE_INTEGER umd_version;
			if (SUCCEEDED(dxgi_adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd_version)))
			{
				props.driver_version = LOWORD(umd_version.LowPart) + (HIWORD(umd_version.LowPart) % 10) * 10000;
			}

			DXGI_ADAPTER_DESC adapter_desc;
			if (SUCCEEDED(dxgi_adapter->GetDesc(&adapter_desc)))
			{
				props.vendor_id = adapter_desc.VendorId;
				props.device_id = adapter_desc.DeviceId;

				static_assert(std::size(props.description) >= std::size(adapter_desc.Description));
				utf8::unchecked::utf16to8(adapter_desc.Description, adapter_desc.Description + std::size(adapter_desc.Description), props.description);
			}
		}
	}

	return props;
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
	case api::device_caps::logic_op:
		return false;
	case api::device_caps::dual_source_blend:
	case api::device_caps::independent_blend: // Supported in D3D10.1
	case api::device_caps::fill_mode_non_solid:
		return true;
	case api::device_caps::conservative_rasterization:
		return false;
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
	case api::device_caps::copy_query_heap_results:
		return false;
	case api::device_caps::sampler_compare:
	case api::device_caps::sampler_anisotropic:
		return true;
	case api::device_caps::sampler_with_resource_view:
		return false;
	case api::device_caps::shared_resource:
		return true;
	case api::device_caps::shared_resource_nt_handle:
	case api::device_caps::resolve_depth_stencil:
	case api::device_caps::shared_fence:
	case api::device_caps::shared_fence_nt_handle:
	default:
		return false;
	}
}
bool reshade::d3d10::device_impl::check_format_support(api::format format, api::resource_usage usage) const
{
	if ((usage & api::resource_usage::unordered_access) != 0)
		return false;

	UINT support = 0;
	if (FAILED(_orig->CheckFormatSupport(convert_format(format), &support)))
		return false;

	if ((usage & api::resource_usage::depth_stencil) != 0 && (support & D3D10_FORMAT_SUPPORT_DEPTH_STENCIL) == 0)
		return false;
	if ((usage & api::resource_usage::render_target) != 0 && (support & D3D10_FORMAT_SUPPORT_RENDER_TARGET) == 0)
		return false;
	if ((usage & api::resource_usage::shader_resource) != 0 && (support & (D3D10_FORMAT_SUPPORT_SHADER_LOAD | D3D10_FORMAT_SUPPORT_SHADER_SAMPLE)) == 0)
		return false;

	if ((usage & (api::resource_usage::resolve_source | api::resource_usage::resolve_dest)) != 0 && (support & D3D10_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE) == 0)
		return false;

	return true;
}

bool reshade::d3d10::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out_handle)
{
	D3D10_SAMPLER_DESC internal_desc = {};
	convert_sampler_desc(desc, internal_desc);

	if (com_ptr<ID3D10SamplerState> object;
		SUCCEEDED(_orig->CreateSamplerState(&internal_desc, &object)))
	{
		*out_handle = to_handle(object.release());
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
void reshade::d3d10::device_impl::destroy_sampler(api::sampler handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

static bool get_shared_resource(ID3D10Resource *object, HANDLE *shared_handle)
{
	com_ptr<IDXGIResource> object_dxgi;
	return SUCCEEDED(object->QueryInterface(&object_dxgi)) && SUCCEEDED(object_dxgi->GetSharedHandle(shared_handle));
}
static bool open_shared_resource(HANDLE shared_handle, ID3D10Device *device, REFIID iid, void **out_object)
{
	return SUCCEEDED(device->OpenSharedResource(shared_handle, iid, out_object));
}

bool reshade::d3d10::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage, api::resource *out_handle, HANDLE *shared_handle)
{
	*out_handle = { 0 };

	const bool is_shared = (desc.flags & api::resource_flags::shared) != 0;
	if (is_shared)
	{
		// NT handles are not supported
		if (shared_handle == nullptr || (desc.flags & reshade::api::resource_flags::shared_nt_handle) != 0)
			return false;

		if (*shared_handle != nullptr)
		{
			assert(initial_data == nullptr);

			if (com_ptr<ID3D10Resource> object;
				open_shared_resource(*shared_handle, _orig, IID_PPV_ARGS(&object)))
			{
				*out_handle = to_handle(object.release());
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	switch (desc.type)
	{
		case api::resource_type::buffer:
		{
			D3D10_BUFFER_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D10Buffer> object;
				SUCCEEDED(_orig->CreateBuffer(&internal_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), &object)))
			{
				if (is_shared && !get_shared_resource(object.get(), shared_handle))
					break;

				*out_handle = to_handle(object.release());
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
				if (is_shared && !get_shared_resource(object.get(), shared_handle))
					break;

				*out_handle = to_handle(object.release());
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
				if (is_shared && !get_shared_resource(object.get(), shared_handle))
					break;

				*out_handle = to_handle(object.release());
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
				if (is_shared && !get_shared_resource(object.get(), shared_handle))
					break;

				*out_handle = to_handle(object.release());
				return true;
			}
			break;
		}
	}

	return false;
}
void reshade::d3d10::device_impl::destroy_resource(api::resource handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
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

bool reshade::d3d10::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out_handle)
{
	*out_handle = { 0 };

	if (resource.handle == 0)
		return false;

	// Cannot create a resource view with a typeless format
	assert(desc.format != api::format_to_typeless(desc.format) || api::format_to_typeless(desc.format) == api::format_to_default_typed(desc.format));

	switch (usage_type)
	{
		case api::resource_usage::depth_stencil:
		{
			D3D10_DEPTH_STENCIL_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D10DepthStencilView> object;
				SUCCEEDED(_orig->CreateDepthStencilView(reinterpret_cast<ID3D10Resource *>(resource.handle), desc.type != api::resource_view_type::unknown ? &internal_desc : nullptr, &object)))
			{
				*out_handle = to_handle(object.release());
				return true;
			}
			break;
		}
		case api::resource_usage::render_target:
		{
			D3D10_RENDER_TARGET_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D10RenderTargetView> object;
				SUCCEEDED(_orig->CreateRenderTargetView(reinterpret_cast<ID3D10Resource *>(resource.handle), desc.type != api::resource_view_type::unknown ? &internal_desc : nullptr, &object)))
			{
				*out_handle = to_handle(object.release());
				return true;
			}
			break;
		}
		case api::resource_usage::shader_resource:
		{
			D3D10_SHADER_RESOURCE_VIEW_DESC1 internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D10ShaderResourceView1> object;
				SUCCEEDED(_orig->CreateShaderResourceView1(reinterpret_cast<ID3D10Resource *>(resource.handle), desc.type != api::resource_view_type::unknown ? &internal_desc : nullptr, &object)))
			{
				*out_handle = to_handle(object.release());
				return true;
			}
			break;
		}
	}

	return false;
}
void reshade::d3d10::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

reshade::api::resource reshade::d3d10::device_impl::get_resource_from_view(api::resource_view view) const
{
	assert(view.handle != 0);

	com_ptr<ID3D10Resource> resource;
	reinterpret_cast<ID3D10View *>(view.handle)->GetResource(&resource);

	return to_handle(resource.get());
}
reshade::api::resource_view_desc reshade::d3d10::device_impl::get_resource_view_desc(api::resource_view view) const
{
	assert(view.handle != 0);

	if (com_ptr<ID3D10RenderTargetView> object;
		SUCCEEDED(reinterpret_cast<IUnknown *>(view.handle)->QueryInterface(&object)))
	{
		D3D10_RENDER_TARGET_VIEW_DESC internal_desc;
		object->GetDesc(&internal_desc);
		return convert_resource_view_desc(internal_desc);
	}
	if (com_ptr<ID3D10DepthStencilView> object;
		SUCCEEDED(reinterpret_cast<IUnknown *>(view.handle)->QueryInterface(&object)))
	{
		D3D10_DEPTH_STENCIL_VIEW_DESC internal_desc;
		object->GetDesc(&internal_desc);
		return convert_resource_view_desc(internal_desc);
	}
	if (com_ptr<ID3D10ShaderResourceView1> object;
		SUCCEEDED(reinterpret_cast<IUnknown *>(view.handle)->QueryInterface(&object)))
	{
		D3D10_SHADER_RESOURCE_VIEW_DESC1 internal_desc;
		object->GetDesc1(&internal_desc);
		return convert_resource_view_desc(internal_desc);
	}
	if (com_ptr<ID3D10ShaderResourceView> object;
		SUCCEEDED(reinterpret_cast<IUnknown *>(view.handle)->QueryInterface(&object)))
	{
		D3D10_SHADER_RESOURCE_VIEW_DESC internal_desc;
		object->GetDesc(&internal_desc);
		return convert_resource_view_desc(internal_desc);
	}

	assert(false); // Not implemented
	return api::resource_view_desc();
}

bool reshade::d3d10::device_impl::map_buffer_region(api::resource resource, uint64_t offset, uint64_t, api::map_access access, void **out_data)
{
	if (out_data == nullptr)
		return false;

	assert(resource.handle != 0);

	D3D10_BUFFER_DESC internal_desc = {};
	reinterpret_cast<ID3D10Buffer *>(resource.handle)->GetDesc(&internal_desc);

	D3D10_MAP map_type = convert_access_flags(access);
	if ((internal_desc.BindFlags & (D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_INDEX_BUFFER)) != 0 && (internal_desc.BindFlags & D3D10_BIND_CONSTANT_BUFFER) == 0 && map_type == D3D10_MAP_WRITE)
		// Use no overwrite flag to simulate D3D12 behavior of there only being one allocation that backs a buffer (instead of the runtime managing multiple ones behind the scenes)
		map_type = D3D10_MAP_WRITE_NO_OVERWRITE;

	if (SUCCEEDED(ID3D10Buffer_Map(reinterpret_cast<ID3D10Buffer *>(resource.handle), map_type, 0, out_data)))
	{
		*out_data = static_cast<uint8_t *>(*out_data) + offset;
		return true;
	}
	else
	{
		return false;
	}
}
void reshade::d3d10::device_impl::unmap_buffer_region(api::resource resource)
{
	assert(resource.handle != 0);

	ID3D10Buffer_Unmap(reinterpret_cast<ID3D10Buffer *>(resource.handle));
}
bool reshade::d3d10::device_impl::map_texture_region(api::resource resource, uint32_t subresource, const api::subresource_box *box, api::map_access access, api::subresource_data *out_data)
{
	if (out_data == nullptr)
		return false;

	out_data->data = nullptr;
	out_data->row_pitch = 0;
	out_data->slice_pitch = 0;

	// Mapping a subset of a texture is not supported
	if (box != nullptr)
		return false;

	assert(resource.handle != 0);

	const auto object = reinterpret_cast<ID3D10Resource *>(resource.handle);

	D3D10_RESOURCE_DIMENSION dimension;
	object->GetType(&dimension);
	switch (dimension)
	{
	case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
		return SUCCEEDED(ID3D10Texture1D_Map(static_cast<ID3D10Texture1D *>(object), subresource, convert_access_flags(access), 0, &out_data->data));
	case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
		static_assert(sizeof(api::subresource_data) >= sizeof(D3D10_MAPPED_TEXTURE2D));
		return SUCCEEDED(ID3D10Texture2D_Map(static_cast<ID3D10Texture2D *>(object), subresource, convert_access_flags(access), 0, reinterpret_cast<D3D10_MAPPED_TEXTURE2D *>(out_data)));
	case D3D10_RESOURCE_DIMENSION_TEXTURE3D:
		static_assert(sizeof(api::subresource_data) == sizeof(D3D10_MAPPED_TEXTURE3D));
		return SUCCEEDED(ID3D10Texture3D_Map(static_cast<ID3D10Texture3D *>(object), subresource, convert_access_flags(access), 0, reinterpret_cast<D3D10_MAPPED_TEXTURE3D *>(out_data)));
	}

	return false;
}
void reshade::d3d10::device_impl::unmap_texture_region(api::resource resource, uint32_t subresource)
{
	assert(resource.handle != 0);

	const auto object = reinterpret_cast<ID3D10Resource *>(resource.handle);

	D3D10_RESOURCE_DIMENSION dimension;
	object->GetType(&dimension);
	switch (dimension)
	{
	case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
		ID3D10Texture1D_Unmap(static_cast<ID3D10Texture1D *>(object), subresource);
		break;
	case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
		ID3D10Texture2D_Unmap(static_cast<ID3D10Texture2D *>(object), subresource);
		break;
	case D3D10_RESOURCE_DIMENSION_TEXTURE3D:
		ID3D10Texture3D_Unmap(static_cast<ID3D10Texture3D *>(object), subresource);
		break;
	}
}

void reshade::d3d10::device_impl::update_buffer_region(const void *data, api::resource resource, uint64_t offset, uint64_t size)
{
	assert(resource.handle != 0);
	assert(data != nullptr);
	assert(offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	const D3D10_BOX box = { static_cast<UINT>(offset), 0, 0, static_cast<UINT>(offset + size), 1, 1 };

	_orig->UpdateSubresource(reinterpret_cast<ID3D10Resource *>(resource.handle), 0, offset != 0 ? &box : nullptr, data, static_cast<UINT>(size), 0);
}
void reshade::d3d10::device_impl::update_texture_region(const api::subresource_data &data, api::resource resource, uint32_t subresource, const api::subresource_box *box)
{
	assert(resource.handle != 0);
	assert(data.data != nullptr);

	_orig->UpdateSubresource(reinterpret_cast<ID3D10Resource *>(resource.handle), subresource, reinterpret_cast<const D3D10_BOX *>(box), data.data, data.row_pitch, data.slice_pitch);
}

bool reshade::d3d10::device_impl::create_pipeline(api::pipeline_layout, uint32_t subobject_count, const api::pipeline_subobject *subobjects, api::pipeline *out_handle)
{
	api::shader_desc vertex_shader_desc = {};
	com_ptr<ID3D10VertexShader> vertex_shader;
	com_ptr<ID3D10GeometryShader> geometry_shader;
	com_ptr<ID3D10PixelShader> pixel_shader;
	api::pipeline_subobject input_layout_desc = {};
	com_ptr<ID3D10BlendState> blend_state;
	com_ptr<ID3D10RasterizerState> rasterizer_state;
	com_ptr<ID3D10DepthStencilState> depth_stencil_state;
	api::primitive_topology topology = api::primitive_topology::triangle_list;
	uint32_t sample_mask = UINT32_MAX;
	uint32_t stencil_reference_value = 0;
	float blend_constant[4] = {};

	if (subobject_count == 1)
	{
		if (subobjects->count != 0)
		{
			switch (subobjects->type)
			{
			case api::pipeline_subobject_type::vertex_shader:
				assert(subobjects->count == 1);
				return create_vertex_shader(*static_cast<const api::shader_desc *>(subobjects->data), out_handle);
			case api::pipeline_subobject_type::geometry_shader:
				assert(subobjects->count == 1);
				return create_geometry_shader(*static_cast<const api::shader_desc *>(subobjects->data), out_handle);
			case api::pipeline_subobject_type::pixel_shader:
				assert(subobjects->count == 1);
				return create_pixel_shader(*static_cast<const api::shader_desc *>(subobjects->data), out_handle);
			case api::pipeline_subobject_type::blend_state:
				assert(subobjects->count == 1);
				return create_blend_state(*static_cast<const api::blend_desc *>(subobjects->data), out_handle);
			case api::pipeline_subobject_type::rasterizer_state:
				assert(subobjects->count == 1);
				return create_rasterizer_state(*static_cast<const api::rasterizer_desc *>(subobjects->data), out_handle);
			case api::pipeline_subobject_type::depth_stencil_state:
				assert(subobjects->count == 1);
				return create_depth_stencil_state(*static_cast<const api::depth_stencil_desc *>(subobjects->data), out_handle);
			default:
				assert(false);
				break;
			}
		}

		goto exit_failure;
	}
	else
	{
		for (uint32_t i = 0; i < subobject_count; ++i)
		{
			if (subobjects[i].count == 0)
				continue;

			api::pipeline temp;

			switch (subobjects[i].type)
			{
			case api::pipeline_subobject_type::vertex_shader:
				assert(subobjects[i].count == 1);
				if (static_cast<const api::shader_desc *>(subobjects[i].data)->code_size == 0)
					break;
				vertex_shader_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
				if (subobject_count == 2 && subobjects[!i].type == api::pipeline_subobject_type::input_layout)
					break; // Special case for creating an input layout, where the vertex shader pipeline subobject only points to the input signature byte code
				if (!create_vertex_shader(*static_cast<const api::shader_desc *>(subobjects[i].data), &temp))
					goto exit_failure;
				vertex_shader = com_ptr<ID3D10VertexShader>(reinterpret_cast<ID3D10VertexShader *>(temp.handle), true);
				break;
			case api::pipeline_subobject_type::hull_shader:
			case api::pipeline_subobject_type::domain_shader:
				assert(subobjects[i].count == 1);
				if (static_cast<const api::shader_desc *>(subobjects[i].data)->code_size == 0)
					break;
				goto exit_failure;
			case api::pipeline_subobject_type::geometry_shader:
				assert(subobjects[i].count == 1);
				if (static_cast<const api::shader_desc *>(subobjects[i].data)->code_size == 0)
					break;
				if (!create_geometry_shader(*static_cast<const api::shader_desc *>(subobjects[i].data), &temp))
					goto exit_failure;
				geometry_shader = com_ptr<ID3D10GeometryShader>(reinterpret_cast<ID3D10GeometryShader *>(temp.handle), true);
				break;
			case api::pipeline_subobject_type::pixel_shader:
				assert(subobjects[i].count == 1);
				if (static_cast<const api::shader_desc *>(subobjects[i].data)->code_size == 0)
					break;
				if (!create_pixel_shader(*static_cast<const api::shader_desc *>(subobjects[i].data), &temp))
					goto exit_failure;
				pixel_shader = com_ptr<ID3D10PixelShader>(reinterpret_cast<ID3D10PixelShader *>(temp.handle), true);
				break;
			case api::pipeline_subobject_type::compute_shader:
				assert(subobjects[i].count == 1);
				if (static_cast<const api::shader_desc *>(subobjects[i].data)->code_size == 0)
					break;
				goto exit_failure; // Not supported
			case api::pipeline_subobject_type::input_layout:
				input_layout_desc = subobjects[i];
				break;
			case api::pipeline_subobject_type::stream_output_state:
				assert(subobjects[i].count == 1);
				goto exit_failure; // Not implemented
			case api::pipeline_subobject_type::blend_state:
				assert(subobjects[i].count == 1);
				if (!create_blend_state(*static_cast<const api::blend_desc *>(subobjects[i].data), &temp))
					goto exit_failure;
				blend_state = com_ptr<ID3D10BlendState>(reinterpret_cast<ID3D10BlendState *>(temp.handle), true);
				std::copy_n(static_cast<const api::blend_desc *>(subobjects[i].data)->blend_constant, 4, blend_constant);
				break;
			case api::pipeline_subobject_type::rasterizer_state:
				assert(subobjects[i].count == 1);
				if (!create_rasterizer_state(*static_cast<const api::rasterizer_desc *>(subobjects[i].data), &temp))
					goto exit_failure;
				rasterizer_state = com_ptr<ID3D10RasterizerState>(reinterpret_cast<ID3D10RasterizerState *>(temp.handle), true);
				break;
			case api::pipeline_subobject_type::depth_stencil_state:
				assert(subobjects[i].count == 1);
				if (!create_depth_stencil_state(*static_cast<const api::depth_stencil_desc *>(subobjects[i].data), &temp))
					goto exit_failure;
				depth_stencil_state = com_ptr<ID3D10DepthStencilState>(reinterpret_cast<ID3D10DepthStencilState *>(temp.handle), true);
				stencil_reference_value = static_cast<const api::depth_stencil_desc *>(subobjects[i].data)->front_stencil_reference_value;
				break;
			case api::pipeline_subobject_type::primitive_topology:
				assert(subobjects[i].count == 1);
				topology = *static_cast<const api::primitive_topology *>(subobjects[i].data);
				if (topology == api::primitive_topology::triangle_fan)
					goto exit_failure;
				break;
			case api::pipeline_subobject_type::depth_stencil_format:
			case api::pipeline_subobject_type::render_target_formats:
				break; // Ignored
			case api::pipeline_subobject_type::sample_mask:
				assert(subobjects[i].count == 1);
				sample_mask = *static_cast<const uint32_t *>(subobjects[i].data);
				break;
			case api::pipeline_subobject_type::sample_count:
			case api::pipeline_subobject_type::viewport_count:
				assert(subobjects[i].count == 1);
				break; // Ignored
			case api::pipeline_subobject_type::dynamic_pipeline_states:
				for (uint32_t k = 0; k < subobjects[i].count; ++k)
					if (static_cast<const api::dynamic_state *>(subobjects[i].data)[k] != api::dynamic_state::primitive_topology)
						goto exit_failure;
				break;
			case api::pipeline_subobject_type::max_vertex_count:
				assert(subobjects[i].count == 1);
				break; // Ignored
			default:
				assert(false);
				goto exit_failure;
			}
		}
	}

	api::pipeline input_layout;
	if (!create_input_layout(input_layout_desc.count, static_cast<const api::input_element *>(input_layout_desc.data), vertex_shader_desc, &input_layout))
		goto exit_failure;

	pipeline_impl *const impl = new pipeline_impl();

	impl->vs = std::move(vertex_shader);
	impl->gs = std::move(geometry_shader);
	impl->ps = std::move(pixel_shader);

	impl->input_layout = com_ptr<ID3D10InputLayout>(reinterpret_cast<ID3D10InputLayout *>(input_layout.handle), true);

	impl->blend_state = std::move(blend_state);
	impl->rasterizer_state = std::move(rasterizer_state);
	impl->depth_stencil_state = std::move(depth_stencil_state);

	impl->topology = convert_primitive_topology(topology);
	impl->sample_mask = sample_mask;
	impl->stencil_reference_value = stencil_reference_value;

	std::copy_n(blend_constant, 4, impl->blend_constant);

	// Set first bit to identify this as a 'pipeline_impl' handle for 'destroy_pipeline'
	static_assert(alignof(pipeline_impl) >= 2);

	*out_handle = { reinterpret_cast<uintptr_t>(impl) | 1 };
	return true;

exit_failure:
	*out_handle = { 0 };
	return false;
}
bool reshade::d3d10::device_impl::create_input_layout(uint32_t count, const api::input_element *desc, const api::shader_desc &signature, api::pipeline *out_handle)
{
	static_assert(alignof(ID3D10InputLayout) >= 2);

	std::vector<D3D10_INPUT_ELEMENT_DESC> internal_elements;
	convert_input_layout_desc(count, desc, internal_elements);

	if (com_ptr<ID3D10InputLayout> object;
		internal_elements.empty() || // Empty input layout is valid, but generates a warning, so just return success and a zero handle
		SUCCEEDED(_orig->CreateInputLayout(internal_elements.data(), static_cast<UINT>(internal_elements.size()), signature.code, signature.code_size, &object)))
	{
		*out_handle = to_handle(object.release());
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
bool reshade::d3d10::device_impl::create_vertex_shader(const api::shader_desc &desc, api::pipeline *out_handle)
{
	static_assert(alignof(ID3D10VertexShader) >= 2);

	assert(desc.entry_point == nullptr);
	assert(desc.spec_constants == 0);

	if (com_ptr<ID3D10VertexShader> object;
		SUCCEEDED(_orig->CreateVertexShader(desc.code, desc.code_size, &object)))
	{
		*out_handle = to_handle(object.release());
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
bool reshade::d3d10::device_impl::create_geometry_shader(const api::shader_desc &desc, api::pipeline *out_handle)
{
	static_assert(alignof(ID3D10GeometryShader) >= 2);

	assert(desc.entry_point == nullptr);
	assert(desc.spec_constants == 0);

	if (com_ptr<ID3D10GeometryShader> object;
		SUCCEEDED(_orig->CreateGeometryShader(desc.code, desc.code_size, &object)))
	{
		*out_handle = to_handle(object.release());
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
bool reshade::d3d10::device_impl::create_pixel_shader(const api::shader_desc &desc, api::pipeline *out_handle)
{
	static_assert(alignof(ID3D10PixelShader) >= 2);

	assert(desc.entry_point == nullptr);
	assert(desc.spec_constants == 0);

	if (com_ptr<ID3D10PixelShader> object;
		SUCCEEDED(_orig->CreatePixelShader(desc.code, desc.code_size, &object)))
	{
		*out_handle = to_handle(object.release());
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
bool reshade::d3d10::device_impl::create_blend_state(const api::blend_desc &desc, api::pipeline *out_handle)
{
	if (desc.logic_op_enable[0])
	{
		*out_handle = { 0 };
		return false;
	}

	static_assert(alignof(ID3D10BlendState1) >= 2);

	D3D10_BLEND_DESC1 internal_desc = {};
	convert_blend_desc(desc, internal_desc);

	if (com_ptr<ID3D10BlendState1> object;
		SUCCEEDED(_orig->CreateBlendState1(&internal_desc, &object)))
	{
		*out_handle = to_handle(object.release());
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
bool reshade::d3d10::device_impl::create_rasterizer_state(const api::rasterizer_desc &desc, api::pipeline *out_handle)
{
	if (desc.conservative_rasterization)
	{
		*out_handle = { 0 };
		return false;
	}

	static_assert(alignof(ID3D10RasterizerState) >= 2);

	D3D10_RASTERIZER_DESC internal_desc = {};
	convert_rasterizer_desc(desc, internal_desc);

	if (com_ptr<ID3D10RasterizerState> object;
		SUCCEEDED(_orig->CreateRasterizerState(&internal_desc, &object)))
	{
		*out_handle = to_handle(object.release());
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
bool reshade::d3d10::device_impl::create_depth_stencil_state(const api::depth_stencil_desc &desc, api::pipeline *out_handle)
{
	static_assert(alignof(ID3D10DepthStencilState) >= 2);

	D3D10_DEPTH_STENCIL_DESC internal_desc = {};
	convert_depth_stencil_desc(desc, internal_desc);

	if (com_ptr<ID3D10DepthStencilState> object;
		SUCCEEDED(_orig->CreateDepthStencilState(&internal_desc, &object)))
	{
		*out_handle = to_handle(object.release());
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
void reshade::d3d10::device_impl::destroy_pipeline(api::pipeline handle)
{
	if (handle.handle & 1) // See 'device_impl::create_pipeline'
		delete reinterpret_cast<pipeline_impl *>(handle.handle ^ 1);
	else if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

bool reshade::d3d10::device_impl::create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_handle)
{
	*out_handle = { 0 };

	std::vector<api::descriptor_range> ranges(param_count);

	for (uint32_t i = 0; i < param_count; ++i)
	{
		api::descriptor_range &merged_range = ranges[i];

		switch (params[i].type)
		{
		case api::pipeline_layout_param_type::descriptor_table:
		case api::pipeline_layout_param_type::push_descriptors_with_ranges:
			if (params[i].descriptor_table.count == 0)
				return false;

			merged_range = params[i].descriptor_table.ranges[0];
			if (merged_range.count == UINT32_MAX || merged_range.array_size > 1 || merged_range.dx_register_space != 0)
				return false;

			for (uint32_t k = 1; k < params[i].descriptor_table.count; ++k)
			{
				const api::descriptor_range &range = params[i].descriptor_table.ranges[k];

				if (range.type != merged_range.type || range.count == UINT32_MAX || range.array_size > 1 || range.dx_register_space != merged_range.dx_register_space)
					return false;

				if (range.binding >= merged_range.binding)
				{
					const uint32_t distance = range.binding - merged_range.binding;

					if ((range.dx_register_index - merged_range.dx_register_index) != distance)
						return false;
					assert(merged_range.count <= distance);

					merged_range.count = distance + range.count;
					merged_range.visibility |= range.visibility;
				}
				else
				{
					const uint32_t distance = merged_range.binding - range.binding;

					if ((merged_range.dx_register_index - range.dx_register_index) != distance)
						return false;
					assert(range.count <= distance);

					merged_range.binding = range.binding;
					merged_range.dx_register_index = range.dx_register_index;
					merged_range.count += distance;
					merged_range.visibility |= range.visibility;
				}
			}
			break;
		case api::pipeline_layout_param_type::push_descriptors:
			merged_range = params[i].push_descriptors;
			if (merged_range.dx_register_space != 0)
				return false;
			break;
		case api::pipeline_layout_param_type::push_constants:
			merged_range.dx_register_index = params[i].push_constants.dx_register_index;
			merged_range.dx_register_space = params[i].push_constants.dx_register_space;
			if (merged_range.dx_register_space != 0)
				return false;
			break;
		}
	}

	const auto impl = new pipeline_layout_impl();
	impl->ranges = std::move(ranges);

	*out_handle = { reinterpret_cast<uintptr_t>(impl) };
	return true;
}
void reshade::d3d10::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	assert(handle != global_pipeline_layout);

	delete reinterpret_cast<pipeline_layout_impl *>(handle.handle);
}

bool reshade::d3d10::device_impl::allocate_descriptor_tables(uint32_t count, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_table *out_tables)
{
	const auto layout_impl = reinterpret_cast<const pipeline_layout_impl *>(layout.handle);

	if (layout_impl != nullptr)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto table_impl = new descriptor_table_impl();
			table_impl->type = layout_impl->ranges[layout_param].type;
			table_impl->count = layout_impl->ranges[layout_param].count;
			table_impl->base_binding = layout_impl->ranges[layout_param].binding;

			switch (table_impl->type)
			{
			case api::descriptor_type::sampler:
			case api::descriptor_type::shader_resource_view:
				table_impl->descriptors.resize(table_impl->count * 1);
				break;
			case api::descriptor_type::constant_buffer:
				table_impl->descriptors.resize(table_impl->count * 3);
				break;
			default:
				assert(false);
				break;
			}

			out_tables[i] = { reinterpret_cast<uintptr_t>(table_impl) };
		}

		return true;
	}
	else
	{
		for (uint32_t i = 0; i < count; ++i)
			out_tables[i] = { 0 };

		return false;
	}
}
void reshade::d3d10::device_impl::free_descriptor_tables(uint32_t count, const api::descriptor_table *sets)
{
	for (uint32_t i = 0; i < count; ++i)
		delete reinterpret_cast<descriptor_table_impl *>(sets[i].handle);
}

void reshade::d3d10::device_impl::get_descriptor_heap_offset(api::descriptor_table table, uint32_t binding, uint32_t array_offset, api::descriptor_heap *heap, uint32_t *offset) const
{
	assert(table.handle != 0 && array_offset == 0 && heap != nullptr && offset != nullptr);

	*heap = { 0 }; // Not implemented
	*offset = binding;
}

void reshade::d3d10::device_impl::copy_descriptor_tables(uint32_t count, const api::descriptor_table_copy *copies)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const api::descriptor_table_copy &copy = copies[i];

		const auto src_table_impl = reinterpret_cast<descriptor_table_impl *>(copy.source_table.handle);
		const auto dst_table_impl = reinterpret_cast<descriptor_table_impl *>(copy.dest_table.handle);
		assert(src_table_impl != nullptr && dst_table_impl != nullptr && src_table_impl->type == dst_table_impl->type);

		const uint32_t dst_binding = copy.dest_binding - dst_table_impl->base_binding;
		assert(dst_binding < dst_table_impl->count && copy.count <= (dst_table_impl->count - dst_binding));
		const uint32_t src_binding = copy.source_binding - src_table_impl->base_binding;
		assert(src_binding < src_table_impl->count && copy.count <= (src_table_impl->count - src_binding));

		assert(copy.dest_array_offset == 0 && copy.source_array_offset == 0);

		switch (src_table_impl->type)
		{
		case api::descriptor_type::sampler:
		case api::descriptor_type::shader_resource_view:
			std::memcpy(&dst_table_impl->descriptors[dst_binding * 1], &src_table_impl->descriptors[src_binding * 1], copy.count * sizeof(uint64_t) * 1);
			break;
		case api::descriptor_type::constant_buffer:
			std::memcpy(&dst_table_impl->descriptors[dst_binding * 3], &src_table_impl->descriptors[src_binding * 3], copy.count * sizeof(uint64_t) * 3);
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::d3d10::device_impl::update_descriptor_tables(uint32_t count, const api::descriptor_table_update *updates)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const api::descriptor_table_update &update = updates[i];

		const auto table_impl = reinterpret_cast<descriptor_table_impl *>(update.table.handle);
		assert(table_impl != nullptr && table_impl->type == update.type);

		const uint32_t update_binding = update.binding - table_impl->base_binding;
		assert(update_binding < table_impl->count && update.count <= (table_impl->count - update_binding));

		assert(update.array_offset == 0);

		switch (update.type)
		{
		case api::descriptor_type::sampler:
		case api::descriptor_type::shader_resource_view:
			std::memcpy(&table_impl->descriptors[update_binding * 1], update.descriptors, update.count * sizeof(uint64_t) * 1);
			break;
		case api::descriptor_type::constant_buffer:
			std::memcpy(&table_impl->descriptors[update_binding * 3], update.descriptors, update.count * sizeof(uint64_t) * 3);
			break;
		default:
			assert(false);
			break;
		}
	}
}

bool reshade::d3d10::device_impl::create_query_heap(api::query_type type, uint32_t size, api::query_heap *out_handle)
{
	const auto impl = new query_heap_impl();
	impl->queries.resize(size);

	D3D10_QUERY_DESC internal_desc = {};
	internal_desc.Query = convert_query_type(type);

	for (uint32_t i = 0; i < size; ++i)
	{
		if (FAILED(_orig->CreateQuery(&internal_desc, &impl->queries[i])))
		{
			delete impl;

			*out_handle = { 0 };
			return false;
		}
	}

	*out_handle = { reinterpret_cast<uintptr_t>(impl) };
	return true;
}
void reshade::d3d10::device_impl::destroy_query_heap(api::query_heap handle)
{
	delete reinterpret_cast<query_heap_impl *>(handle.handle);
}

bool reshade::d3d10::device_impl::get_query_heap_results(api::query_heap heap, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(heap.handle != 0);

	const auto impl = reinterpret_cast<query_heap_impl *>(heap.handle);

	for (size_t i = 0; i < count; ++i)
	{
		// May return 'S_FALSE' if the data is not yet available
		if (impl->queries[first + i]->GetData(static_cast<uint8_t *>(results) + i * stride, stride, D3D10_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
			return false;
	}

	return true;
}

// WKPDID_D3DDebugObjectName
static constexpr GUID s_debug_object_name_guid = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00} };

void reshade::d3d10::device_impl::set_resource_name(api::resource handle, const char *name)
{
	assert(handle.handle != 0);

	reinterpret_cast<ID3D10DeviceChild *>(handle.handle)->SetPrivateData(s_debug_object_name_guid, static_cast<UINT>(std::strlen(name)), name);
}
void reshade::d3d10::device_impl::set_resource_view_name(api::resource_view handle, const char *name)
{
	assert(handle.handle != 0);

	reinterpret_cast<ID3D10DeviceChild *>(handle.handle)->SetPrivateData(s_debug_object_name_guid, static_cast<UINT>(std::strlen(name)), name);
}

bool reshade::d3d10::device_impl::create_fence(uint64_t initial_value, api::fence_flags flags, api::fence *out_handle, HANDLE *shared_handle)
{
	*out_handle = { 0 };

	const bool is_shared = (flags & api::fence_flags::shared) != 0;
	if (is_shared)
	{
		// NT handles are not supported
		if (shared_handle == nullptr || (flags & reshade::api::fence_flags::shared_nt_handle) != 0)
			return false;

		if (*shared_handle != nullptr)
		{
			if (com_ptr<IDXGIKeyedMutex> object;
				open_shared_resource(*shared_handle, _orig, IID_PPV_ARGS(&object)))
			{
				*out_handle = to_handle(object.release());
				return true;
			}
		}

		return false;
	}

	const auto impl = new fence_impl();
	impl->current_value = initial_value;

	D3D10_QUERY_DESC internal_desc = {};
	internal_desc.Query = D3D10_QUERY_EVENT;

	for (size_t i = 0; i < std::size(impl->event_queries); ++i)
	{
		if (FAILED(_orig->CreateQuery(&internal_desc, &impl->event_queries[i])))
		{
			delete impl;
			return false;
		}
	}

	// Set first bit to identify this as a 'fence_impl' handle for 'destroy_fence'
	static_assert(alignof(fence_impl) >= 2);

	*out_handle = { reinterpret_cast<uintptr_t>(impl) | 1 };
	return true;
}
void reshade::d3d10::device_impl::destroy_fence(api::fence handle)
{
	if (handle.handle & 1) // See 'device_impl::create_fence'
		delete reinterpret_cast<fence_impl *>(handle.handle ^ 1);
	else if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

uint64_t reshade::d3d10::device_impl::get_completed_fence_value(api::fence fence) const
{
	if (fence.handle & 1)
	{
		const auto impl = reinterpret_cast<fence_impl *>(fence.handle ^ 1);

		for (uint64_t value = impl->current_value, offset = 0; value > 0 && offset < std::size(impl->event_queries); --value, ++offset)
		{
			if (impl->event_queries[value % std::size(impl->event_queries)]->GetData(nullptr, 0, 0) == S_OK)
				return value;
		}

		return 0;
	}

	assert(false);
	return 0;
}

bool reshade::d3d10::device_impl::wait(api::fence fence, uint64_t value, uint64_t timeout)
{
	DWORD timeout_ms = (timeout == UINT64_MAX) ? INFINITE : (timeout / 1000000) & 0xFFFFFFFF;

	if (fence.handle & 1)
	{
		const auto impl = reinterpret_cast<fence_impl *>(fence.handle ^ 1);
		if (value > impl->current_value)
			return false;

		while (true)
		{
			const HRESULT hr = impl->event_queries[value % std::size(impl->event_queries)]->GetData(nullptr, 0, 0);
			if (hr == S_OK)
				return true;
			if (hr != S_FALSE)
				break;

			if (timeout_ms != INFINITE)
			{
				if (timeout_ms == 0)
					break;
				timeout_ms -= 1;
			}

			Sleep(1);
		}

		return false;
	}

	if (com_ptr<IDXGIKeyedMutex> keyed_mutex;
		SUCCEEDED(reinterpret_cast<IUnknown *>(fence.handle)->QueryInterface(&keyed_mutex)))
	{
		return SUCCEEDED(keyed_mutex->AcquireSync(value, timeout_ms));
	}

	return false;
}
bool reshade::d3d10::device_impl::signal(api::fence fence, uint64_t value)
{
	if (fence.handle & 1)
	{
		const auto impl = reinterpret_cast<fence_impl *>(fence.handle ^ 1);
		if (value < impl->current_value)
			return false;
		impl->current_value = value;

		return impl->event_queries[value % std::size(impl->event_queries)]->End(), true;
	}

	if (com_ptr<IDXGIKeyedMutex> keyed_mutex;
		SUCCEEDED(reinterpret_cast<IUnknown *>(fence.handle)->QueryInterface(&keyed_mutex)))
	{
		return SUCCEEDED(keyed_mutex->ReleaseSync(value));
	}

	return false;
}
