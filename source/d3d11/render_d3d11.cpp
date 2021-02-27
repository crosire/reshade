/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d11.hpp"
#include "render_d3d11_utils.hpp"

reshade::d3d11::device_impl::device_impl(ID3D11Device *device) :
	api_object_impl(device)
{
#if RESHADE_ADDON
	reshade::addon::load_addons();
#endif

	RESHADE_ADDON_EVENT(init_device, this);
}
reshade::d3d11::device_impl::~device_impl()
{
	RESHADE_ADDON_EVENT(destroy_device, this);

#if RESHADE_ADDON
	reshade::addon::unload_addons();
#endif
}

bool reshade::d3d11::device_impl::check_format_support(uint32_t format, api::resource_usage usage) const
{
	UINT support = 0;
	if (FAILED(_orig->CheckFormatSupport(static_cast<DXGI_FORMAT>(format), &support)))
		return false;

	if ((usage & api::resource_usage::render_target) != 0 &&
		(support & D3D11_FORMAT_SUPPORT_RENDER_TARGET) == 0)
		return false;
	if ((usage & api::resource_usage::depth_stencil) != 0 &&
		(support & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) == 0)
		return false;
	if ((usage & api::resource_usage::shader_resource) != 0 &&
		(support & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) == 0)
		return false;
	if ((usage & api::resource_usage::unordered_access) != 0 &&
		(support & D3D11_FORMAT_SUPPORT_SHADER_LOAD) == 0)
		return false;
	if ((usage & (api::resource_usage::resolve_source | api::resource_usage::resolve_dest)) != 0 &&
		(support & D3D10_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE) == 0)
		return false;

	return true;
}

bool reshade::d3d11::device_impl::check_resource_handle_valid(api::resource_handle resource) const
{
	return resource.handle != 0 && _resources.has_object(reinterpret_cast<ID3D11Resource *>(resource.handle));
}
bool reshade::d3d11::device_impl::check_resource_view_handle_valid(api::resource_view_handle view) const
{
	return view.handle != 0 && _views.has_object(reinterpret_cast<ID3D11View *>(view.handle));
}

bool reshade::d3d11::device_impl::create_resource(api::resource_type type, const api::resource_desc &desc, api::resource_usage, api::resource_handle *out_resource)
{
	switch (type)
	{
		case api::resource_type::buffer:
		{
			D3D11_BUFFER_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D11Buffer> resource;
				SUCCEEDED(_orig->CreateBuffer(&internal_desc, nullptr, &resource)))
			{
				_resources.register_object(resource.get());
				*out_resource = { reinterpret_cast<uintptr_t>(resource.release()) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_1d:
		{
			D3D11_TEXTURE1D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D11Texture1D> resource;
				SUCCEEDED(_orig->CreateTexture1D(&internal_desc, nullptr, &resource)))
			{
				_resources.register_object(resource.get());
				*out_resource = { reinterpret_cast<uintptr_t>(resource.release()) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_2d:
		{
			D3D11_TEXTURE2D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D11Texture2D> resource;
				SUCCEEDED(_orig->CreateTexture2D(&internal_desc, nullptr, &resource)))
			{
				_resources.register_object(resource.get());
				*out_resource = { reinterpret_cast<uintptr_t>(resource.release()) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_3d:
		{
			D3D11_TEXTURE3D_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc);

			if (com_ptr<ID3D11Texture3D> resource;
				SUCCEEDED(_orig->CreateTexture3D(&internal_desc, nullptr, &resource)))
			{
				_resources.register_object(resource.get());
				*out_resource = { reinterpret_cast<uintptr_t>(resource.release()) };
				return true;
			}
			break;
		}
	}

	*out_resource = { 0 };
	return false;
}
bool reshade::d3d11::device_impl::create_resource_view(api::resource_handle resource, api::resource_view_type type, const api::resource_view_desc &desc, api::resource_view_handle *out_view)
{
	assert(resource.handle != 0);

	switch (type)
	{
		case api::resource_view_type::depth_stencil:
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D11DepthStencilView> view;
				SUCCEEDED(_orig->CreateDepthStencilView(reinterpret_cast<ID3D11Resource *>(resource.handle), &internal_desc, &view)))
			{
				_views.register_object(view.get());
				*out_view = { reinterpret_cast<uintptr_t>(view.release()) };
				return true;
			}
			break;
		}
		case api::resource_view_type::render_target:
		{
			D3D11_RENDER_TARGET_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D11RenderTargetView> view;
				SUCCEEDED(_orig->CreateRenderTargetView(reinterpret_cast<ID3D11Resource *>(resource.handle), &internal_desc, &view)))
			{
				_views.register_object(view.get());
				*out_view = { reinterpret_cast<uintptr_t>(view.release()) };
				return true;
			}
			break;
		}
		case api::resource_view_type::shader_resource:
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D11ShaderResourceView> view;
				SUCCEEDED(_orig->CreateShaderResourceView(reinterpret_cast<ID3D11Resource *>(resource.handle), &internal_desc, &view)))
			{
				_views.register_object(view.get());
				*out_view = { reinterpret_cast<uintptr_t>(view.release()) };
				return true;
			}
			break;
		}
		case api::resource_view_type::unordered_access:
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			if (com_ptr<ID3D11UnorderedAccessView> view;
				SUCCEEDED(_orig->CreateUnorderedAccessView(reinterpret_cast<ID3D11Resource *>(resource.handle), &internal_desc, &view)))
			{
				_views.register_object(view.get());
				*out_view = { reinterpret_cast<uintptr_t>(view.release()) };
				return true;
			}
			break;
		}
	}

	*out_view = { 0 };
	return false;
}

void reshade::d3d11::device_impl::destroy_resource(api::resource_handle resource)
{
	assert(resource.handle != 0);
	reinterpret_cast<ID3D11Resource *>(resource.handle)->Release();
}
void reshade::d3d11::device_impl::destroy_resource_view(api::resource_view_handle view)
{
	assert(view.handle != 0);
	reinterpret_cast<ID3D11View *>(view.handle)->Release();
}

void reshade::d3d11::device_impl::get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) const
{
	assert(view.handle != 0);
	com_ptr<ID3D11Resource> resource;
	reinterpret_cast<ID3D11View *>(view.handle)->GetResource(&resource);

	*out_resource = { reinterpret_cast<uintptr_t>(resource.get()) };
}

reshade::api::resource_desc reshade::d3d11::device_impl::get_resource_desc(api::resource_handle resource) const
{
	assert(resource.handle != 0);
	const auto resource_object = reinterpret_cast<ID3D11Resource *>(resource.handle);

	D3D11_RESOURCE_DIMENSION dimension;
	resource_object->GetType(&dimension);
	switch (dimension)
	{
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

	assert(false); // Not implemented
	return {};
}

reshade::d3d11::command_list_impl::command_list_impl(device_impl *device, ID3D11CommandList *cmd_list) :
	api_object_impl(cmd_list), _device_impl(device)
{
	RESHADE_ADDON_EVENT(init_command_list, this);
}
reshade::d3d11::command_list_impl::~command_list_impl()
{
	RESHADE_ADDON_EVENT(destroy_command_list, this);
}

reshade::d3d11::device_context_impl::device_context_impl(device_impl *device, ID3D11DeviceContext *context) :
	api_object_impl(context), _device_impl(device)
{
	if (_orig->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
	{
		RESHADE_ADDON_EVENT(init_command_list, this);
	}
	else
	{
		RESHADE_ADDON_EVENT(init_command_queue, this);
	}
}
reshade::d3d11::device_context_impl::~device_context_impl()
{
	if (_orig->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
	{
		RESHADE_ADDON_EVENT(destroy_command_list, this);
	}
	else
	{
		RESHADE_ADDON_EVENT(destroy_command_queue, this);
	}
}

void reshade::d3d11::device_context_impl::flush_immediate_command_list() const
{
	assert(_orig->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);
	_orig->Flush();
}

void reshade::d3d11::device_context_impl::copy_resource(api::resource_handle src, api::resource_handle dst)
{
	assert(src.handle != 0 && dst.handle != 0);
	_orig->CopyResource(reinterpret_cast<ID3D11Resource *>(dst.handle), reinterpret_cast<ID3D11Resource *>(src.handle));
}

void reshade::d3d11::device_context_impl::clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	assert(dsv.handle != 0);
	_orig->ClearDepthStencilView(reinterpret_cast<ID3D11DepthStencilView *>(dsv.handle), clear_flags, depth, stencil);
}
void reshade::d3d11::device_context_impl::clear_render_target_view(api::resource_view_handle rtv, const float color[4])
{
	assert(rtv.handle != 0);
	_orig->ClearRenderTargetView(reinterpret_cast<ID3D11RenderTargetView *>(rtv.handle), color);
}
