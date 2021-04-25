/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d9.hpp"
#include "render_d3d9_utils.hpp"
#include <algorithm>

reshade::d3d9::device_impl::device_impl(IDirect3DDevice9 *device) :
	api_object_impl(device), _caps(), _cp(), _backup_state(device)
{
	_orig->GetDirect3D(&_d3d);
	_orig->GetDeviceCaps(&_caps);
	_orig->GetCreationParameters(&_cp);

	// Limit maximum simultaneous number of render targets to 8 (usually only 4 in D3D9 anyway)
	if (_caps.NumSimultaneousRTs > 8)
		_caps.NumSimultaneousRTs = 8;

#if RESHADE_ADDON
	addon::load_addons();
#endif

	com_ptr<IDirect3DSwapChain9> swapchain;
	device->GetSwapChain(0, &swapchain);
	assert(swapchain != nullptr); // There should always be an implicit swap chain

	D3DPRESENT_PARAMETERS pp = {};
	swapchain->GetPresentParameters(&pp);
	on_after_reset(pp);
}
reshade::d3d9::device_impl::~device_impl()
{
	on_reset();

#if RESHADE_ADDON
	addon::unload_addons();
#endif
}

void reshade::d3d9::device_impl::on_reset()
{
	// Do not call add-on events if initialization failed or this device was already reset
	if (_copy_state == nullptr)
		return;

	// Force add-ons to release all resources associated with this device before performing reset
	invoke_addon_event<addon_event::destroy_command_queue>(this);
	invoke_addon_event<addon_event::destroy_device>(this);

	_copy_state.reset();
	_backup_state.release_state_block();
}
void reshade::d3d9::device_impl::on_after_reset(const D3DPRESENT_PARAMETERS &pp)
{
	// Create state blocks used for resource copying
	HRESULT hr = _orig->BeginStateBlock();
	if (SUCCEEDED(hr))
	{
		_orig->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
		_orig->SetPixelShader(nullptr);
		_orig->SetVertexShader(nullptr);
		_orig->SetRenderState(D3DRS_ZENABLE, FALSE);
		_orig->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		_orig->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		_orig->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		_orig->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		_orig->SetRenderState(D3DRS_FOGENABLE, FALSE);
		_orig->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		_orig->SetRenderState(D3DRS_CLIPPING, FALSE);
		_orig->SetRenderState(D3DRS_LIGHTING, FALSE);
		_orig->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
		_orig->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
		_orig->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		_orig->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
		_orig->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		_orig->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		_orig->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		_orig->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		_orig->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		_orig->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		_orig->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		_orig->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		_orig->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
		_orig->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		_orig->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		_orig->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		_orig->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, 0);
		_orig->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0);
		_orig->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

		hr = _orig->EndStateBlock(&_copy_state);
		if (FAILED(hr))
			return;

		if (!_backup_state.init_state_block())
			return;
	}

	// Do not call add-on events if initialization failed
	if (_copy_state == nullptr)
		return;

	invoke_addon_event<addon_event::init_device>(this);
	invoke_addon_event<addon_event::init_command_queue>(this);

#if RESHADE_ADDON
	if (com_ptr<IDirect3DSurface9> auto_depth_stencil;
		pp.EnableAutoDepthStencil &&
		SUCCEEDED(_orig->GetDepthStencilSurface(&auto_depth_stencil)))
	{
		D3DSURFACE_DESC old_desc;
		auto_depth_stencil->GetDesc(&old_desc);
		D3DSURFACE_DESC new_desc = old_desc;

		invoke_addon_event<addon_event::create_resource>(
			[this, &auto_depth_stencil, &old_desc, &new_desc](api::device *, const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage) {
				if (desc.type != api::resource_type::surface || desc.heap != api::memory_heap::gpu_only || initial_data != nullptr)
					return false;
				convert_resource_desc(desc, new_desc);

				// Need to replace auto depth stencil if add-on modified the description
				if (com_ptr<IDirect3DSurface9> auto_depth_stencil_replacement;
					std::memcmp(&old_desc, &new_desc, sizeof(D3DSURFACE_DESC)) != 0 &&
					create_surface_replacement(new_desc, &auto_depth_stencil_replacement))
				{
					// The device will hold a reference to the surface after binding it, so can release this one afterwards
					_orig->SetDepthStencilSurface(auto_depth_stencil_replacement.get());

					auto_depth_stencil = std::move(auto_depth_stencil_replacement);
				}
				else
				{
					_resources.register_object(auto_depth_stencil.get());
				}
				return true;
			}, this, convert_resource_desc(old_desc, 1, _caps), nullptr, api::resource_usage::depth_stencil);

		// Communicate default state to add-ons
		invoke_addon_event<addon_event::bind_render_targets_and_depth_stencil>(this, 0, nullptr, api::resource_view_handle { reinterpret_cast<uintptr_t>(auto_depth_stencil.get()) });
	}
#else
	UNREFERENCED_PARAMETER(pp);
#endif
}

bool reshade::d3d9::device_impl::create_surface_replacement(const D3DSURFACE_DESC &new_desc, IDirect3DSurface9 **out_surface, HANDLE *out_shared_handle)
{
	com_ptr<IDirect3DTexture9> texture; // Surface will hold a reference to the created texture and keep it alive
	if (new_desc.MultiSampleType == D3DMULTISAMPLE_NONE &&
		SUCCEEDED(_orig->CreateTexture(new_desc.Width, new_desc.Height, 1, new_desc.Usage, new_desc.Format, new_desc.Pool, &texture, out_shared_handle)) &&
		SUCCEEDED(texture->GetSurfaceLevel(0, out_surface)))
	{
		_resources.register_object(texture.get());
		_resources.register_object(*out_surface);
		return true; // Successfully created replacement texture and got surface to it
	}
	return false;
}

bool reshade::d3d9::device_impl::check_format_support(uint32_t format, api::resource_usage usage) const
{
	if ((usage & api::resource_usage::unordered_access) != 0)
		return false;

	DWORD d3d_usage = 0;
	convert_resource_usage_to_d3d_usage(usage, d3d_usage);

	return SUCCEEDED(_d3d->CheckDeviceFormat(_cp.AdapterOrdinal, _cp.DeviceType, D3DFMT_X8R8G8B8, d3d_usage, D3DRTYPE_TEXTURE, static_cast<D3DFORMAT>(format)));
}

bool reshade::d3d9::device_impl::check_resource_handle_valid(api::resource_handle resource) const
{
	return resource.handle != 0 && _resources.has_object(reinterpret_cast<IDirect3DResource9 *>(resource.handle));
}
bool reshade::d3d9::device_impl::check_resource_view_handle_valid(api::resource_view_handle view) const
{
	return view.handle != 0 && _resources.has_object(reinterpret_cast<IDirect3DResource9 *>(view.handle));
}

bool reshade::d3d9::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler_handle *out_sampler)
{
	const auto data = new DWORD[12];
	data[D3DSAMP_ADDRESSU] = static_cast<DWORD>(desc.address_u);
	data[D3DSAMP_ADDRESSV] = static_cast<DWORD>(desc.address_v);
	data[D3DSAMP_ADDRESSW] = static_cast<DWORD>(desc.address_w);
	data[D3DSAMP_BORDERCOLOR] = 0;
	data[D3DSAMP_MAGFILTER] = 1 + ((static_cast<DWORD>(desc.filter) & 0x0C) >> 2);
	data[D3DSAMP_MINFILTER] = 1 + ((static_cast<DWORD>(desc.filter) & 0x30) >> 4);
	data[D3DSAMP_MIPFILTER] = 1 + ((static_cast<DWORD>(desc.filter) & 0x03));
	data[D3DSAMP_MIPMAPLODBIAS] = *reinterpret_cast<const DWORD *>(&desc.mip_lod_bias);
	data[D3DSAMP_MAXMIPLEVEL] = desc.min_lod > 0 ? static_cast<DWORD>(desc.min_lod) : 0;
	data[D3DSAMP_MAXANISOTROPY] = static_cast<DWORD>(desc.max_anisotropy);
	data[D3DSAMP_SRGBTEXTURE] = FALSE;

	*out_sampler = { reinterpret_cast<uintptr_t>(data) };
	return true;
}
bool reshade::d3d9::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage, api::resource_handle *out)
{
	if (initial_data != nullptr)
		return false;

	switch (desc.type)
	{
		case api::resource_type::buffer:
		{
			if (desc.usage == api::resource_usage::index_buffer)
			{
				D3DINDEXBUFFER_DESC internal_desc = {};
				convert_resource_desc(desc, internal_desc);

				// TODO: Index format
				IDirect3DIndexBuffer9 *object = nullptr;
				if (SUCCEEDED(_orig->CreateIndexBuffer(internal_desc.Size, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, &object, nullptr)))
				{
					_resources.register_object(object);
					*out = { reinterpret_cast<uintptr_t>(object) };
					return true;
				}
			}
			if (desc.usage == api::resource_usage::vertex_buffer)
			{
				D3DVERTEXBUFFER_DESC internal_desc = {};
				convert_resource_desc(desc, internal_desc);

				IDirect3DVertexBuffer9 *object = nullptr;
				if (SUCCEEDED(_orig->CreateVertexBuffer(internal_desc.Size, internal_desc.Usage, internal_desc.FVF, internal_desc.Pool, &object, nullptr)))
				{
					_resources.register_object(object);
					*out = { reinterpret_cast<uintptr_t>(object) };
					return true;
				}
			}
			break;
		}
		case api::resource_type::texture_1d:
		case api::resource_type::texture_2d:
		{
			// Array or multisample textures are not supported in Direct3D 9
			if (desc.depth_or_layers != 1 || desc.samples != 1)
				break;

			UINT levels = 0;
			D3DSURFACE_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc, &levels);

			IDirect3DTexture9 *object = nullptr;
			if (SUCCEEDED(_orig->CreateTexture(internal_desc.Width, internal_desc.Height, levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, &object, nullptr)))
			{
				_resources.register_object(object);
				*out = { reinterpret_cast<uintptr_t>(object) };
				return true;
			}
			break;
		}
		case api::resource_type::texture_3d:
		{
			// 3D textures can never have multisampling
			if (desc.samples != 1)
				break;

			UINT levels = 0;
			D3DVOLUME_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc, &levels);

			IDirect3DVolumeTexture9 *object = nullptr;
			if (SUCCEEDED(_orig->CreateVolumeTexture(internal_desc.Width, internal_desc.Height, internal_desc.Depth, levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, &object, nullptr)))
			{
				_resources.register_object(object);
				*out = { reinterpret_cast<uintptr_t>(object) };
				return true;
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}
bool reshade::d3d9::device_impl::create_resource_view(api::resource_handle resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view_handle *out)
{
	assert(resource.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	switch (object->GetType())
	{
		case D3DRTYPE_SURFACE:
		{
			assert(desc.type == api::resource_view_type::texture_2d || desc.type == api::resource_view_type::texture_2d_multisample);
			assert(desc.first_layer == 0 && (desc.layers == 1 || desc.layers == std::numeric_limits<uint32_t>::max()));

			if (usage_type == api::resource_usage::depth_stencil || usage_type == api::resource_usage::render_target)
			{
				if (desc.first_level != 0 || desc.levels != 1)
					break;

				D3DSURFACE_DESC internal_desc;
				static_cast<IDirect3DSurface9 *>(object)->GetDesc(&internal_desc);
				if (internal_desc.Format != static_cast<D3DFORMAT>(desc.format))
					break;

				object->AddRef();
				{
					*out = { reinterpret_cast<uintptr_t>(object) };
					return true;
				}
			}
			break;
		}
		case D3DRTYPE_TEXTURE:
		{
			assert(desc.type == api::resource_view_type::texture_2d || desc.type == api::resource_view_type::texture_2d_multisample);
			assert(desc.first_layer == 0 && (desc.layers == 1 || desc.layers == std::numeric_limits<uint32_t>::max()));

			if (usage_type == api::resource_usage::depth_stencil || usage_type == api::resource_usage::render_target)
			{
				if (desc.levels != 1)
					break;

				D3DSURFACE_DESC internal_desc;
				static_cast<IDirect3DTexture9 *>(object)->GetLevelDesc(desc.first_level, &internal_desc);
				if (internal_desc.Format != static_cast<D3DFORMAT>(desc.format))
					break;

				if (IDirect3DSurface9 *surface = nullptr;
					SUCCEEDED(static_cast<IDirect3DTexture9 *>(object)->GetSurfaceLevel(desc.first_level, &surface)))
				{
					*out = { reinterpret_cast<uintptr_t>(surface) };
					return true;
				}
			}
			else if (usage_type == api::resource_usage::shader_resource && desc.first_level == 0)
			{
				D3DSURFACE_DESC internal_desc;
				static_cast<IDirect3DTexture9 *>(object)->GetLevelDesc(0, &internal_desc);
				if (internal_desc.Format != static_cast<D3DFORMAT>(desc.format))
					break;

				object->AddRef();
				{
					*out = { reinterpret_cast<uintptr_t>(object) };
					return true;
				}
			}
			break;
		}
		case D3DRTYPE_CUBETEXTURE:
		{
			if (usage_type == api::resource_usage::depth_stencil || usage_type == api::resource_usage::render_target)
			{
				assert(desc.type == api::resource_view_type::texture_2d || desc.type == api::resource_view_type::texture_2d_multisample);

				if (desc.levels != 1 || desc.layers != 1)
					break;

				D3DSURFACE_DESC internal_desc;
				static_cast<IDirect3DCubeTexture9 *>(object)->GetLevelDesc(desc.first_level, &internal_desc);
				if (internal_desc.Format != static_cast<D3DFORMAT>(desc.format))
					break;

				if (IDirect3DSurface9 *surface = nullptr;
					SUCCEEDED(static_cast<IDirect3DCubeTexture9 *>(object)->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(desc.first_layer), desc.first_level, &surface)))
				{
					*out = { reinterpret_cast<uintptr_t>(surface) };
					return true;
				}
			}
			else if (usage_type == api::resource_usage::shader_resource && desc.first_level == 0 && desc.first_layer == 0)
			{
				assert(desc.type == api::resource_view_type::texture_cube);

				D3DSURFACE_DESC internal_desc;
				static_cast<IDirect3DCubeTexture9 *>(object)->GetLevelDesc(0, &internal_desc);
				if (internal_desc.Format != static_cast<D3DFORMAT>(desc.format))
					break;

				object->AddRef();
				{
					*out = { reinterpret_cast<uintptr_t>(object) };
					return true;
				}
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}

void reshade::d3d9::device_impl::destroy_sampler(api::sampler_handle sampler)
{
	assert(sampler.handle != 0);
	delete[] reinterpret_cast<DWORD *>(sampler.handle);
}
void reshade::d3d9::device_impl::destroy_resource(api::resource_handle resource)
{
	assert(resource.handle != 0);
	reinterpret_cast<IDirect3DResource9 *>(resource.handle)->Release();
}
void reshade::d3d9::device_impl::destroy_resource_view(api::resource_view_handle view)
{
	assert(view.handle != 0);
	reinterpret_cast<IDirect3DResource9 *>(view.handle)->Release();
}

void reshade::d3d9::device_impl::get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) const
{
	assert(view.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(view.handle);

	if (com_ptr<IDirect3DSurface9> surface;
		SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&surface))))
	{
		if (com_ptr<IDirect3DResource9> resource;
			SUCCEEDED(surface->GetContainer(IID_PPV_ARGS(&resource))))
		{
			*out_resource = { reinterpret_cast<uintptr_t>(resource.get()) };
			return;
		}
	}

	// If unable to get container, just return the resource directly
	*out_resource = { reinterpret_cast<uintptr_t>(object) };
}

reshade::api::resource_desc reshade::d3d9::device_impl::get_resource_desc(api::resource_handle resource) const
{
	assert(resource.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	switch (object->GetType())
	{
		default:
		{
			assert(false); // Not implemented
			return api::resource_desc {};
		}
		case D3DRTYPE_SURFACE:
		{
			D3DSURFACE_DESC internal_desc;
			static_cast<IDirect3DSurface9 *>(object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc, 1, _caps);
		}
		case D3DRTYPE_TEXTURE:
		{
			D3DSURFACE_DESC internal_desc;
			static_cast<IDirect3DTexture9 *>(object)->GetLevelDesc(0, &internal_desc);
			internal_desc.Type = D3DRTYPE_TEXTURE;
			return convert_resource_desc(internal_desc, static_cast<IDirect3DTexture9 *>(object)->GetLevelCount(), _caps);
		}
		case D3DRTYPE_VOLUMETEXTURE:
		{
			D3DVOLUME_DESC internal_desc;
			static_cast<IDirect3DVolumeTexture9 *>(object)->GetLevelDesc(0, &internal_desc);
			internal_desc.Type = D3DRTYPE_VOLUMETEXTURE;
			return convert_resource_desc(internal_desc, static_cast<IDirect3DVolumeTexture9 *>(object)->GetLevelCount());
		}
		case D3DRTYPE_CUBETEXTURE:
		{
			D3DSURFACE_DESC internal_desc;
			static_cast<IDirect3DCubeTexture9 *>(object)->GetLevelDesc(0, &internal_desc);
			internal_desc.Type = D3DRTYPE_CUBETEXTURE;
			return convert_resource_desc(internal_desc, static_cast<IDirect3DCubeTexture9 *>(object)->GetLevelCount(), _caps);
		}
		case D3DRTYPE_VERTEXBUFFER:
		{
			D3DVERTEXBUFFER_DESC internal_desc;
			static_cast<IDirect3DVertexBuffer9 *>(object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3DRTYPE_INDEXBUFFER:
		{
			D3DINDEXBUFFER_DESC internal_desc;
			static_cast<IDirect3DIndexBuffer9 *>(object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
	}
}

void reshade::d3d9::device_impl::blit(api::resource_handle src, uint32_t src_subresource, const int32_t src_box[6], api::resource_handle dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter)
{
	assert(src.handle != 0 && dst.handle != 0);

	const auto src_object = reinterpret_cast<IDirect3DResource9 *>(src.handle);
	const auto dst_object = reinterpret_cast<IDirect3DResource9 *>(dst.handle);

	RECT src_rect = {};
	if (src_box != nullptr)
	{
		src_rect.left = src_box[0];
		src_rect.top = src_box[1];
		assert(src_box[2] == 0);
		src_rect.right = src_box[3];
		src_rect.bottom = src_box[4];
		assert(src_box[5] == 1);
	}

	RECT dst_rect = {};
	if (dst_box != nullptr)
	{
		dst_rect.left = dst_box[0];
		dst_rect.top = dst_box[1];
		assert(dst_box[2] == 0);
		dst_rect.right = dst_box[3];
		dst_rect.bottom = dst_box[4];
		assert(dst_box[5] == 1);
	}

	D3DTEXTUREFILTERTYPE stretch_filter = D3DTEXF_NONE;
	switch (filter)
	{
	case api::texture_filter::min_mag_mip_point:
	case api::texture_filter::min_mag_point_mip_linear:
		stretch_filter = D3DTEXF_POINT;
		break;
	case api::texture_filter::min_mag_mip_linear:
	case api::texture_filter::min_mag_linear_mip_point:
		stretch_filter = D3DTEXF_LINEAR;
		break;
	}

	switch (src_object->GetType() | (dst_object->GetType() << 4))
	{
		case D3DRTYPE_SURFACE | (D3DRTYPE_SURFACE << 4):
		{
			assert(src_subresource == 0 && dst_subresource == 0);

			_orig->StretchRect(
				static_cast<IDirect3DSurface9 *>(src_object), src_box != nullptr ? &src_rect : nullptr,
				static_cast<IDirect3DSurface9 *>(dst_object), dst_box != nullptr ? &dst_rect : nullptr, stretch_filter);
			return;
		}
		case D3DRTYPE_SURFACE | (D3DRTYPE_TEXTURE << 4):
		{
			assert(src_subresource == 0);

			com_ptr<IDirect3DSurface9> destination_surface;
			static_cast<IDirect3DTexture9 *>(dst_object)->GetSurfaceLevel(dst_subresource, &destination_surface);

			_orig->StretchRect(
				static_cast<IDirect3DSurface9 *>(src_object), src_box != nullptr ? &src_rect : nullptr,
				destination_surface.get(), dst_box != nullptr ? &dst_rect : nullptr, stretch_filter);
			return;
		}
		case D3DRTYPE_TEXTURE | (D3DRTYPE_TEXTURE << 4):
		{
			// Capture and restore state, render targets, depth stencil surface and viewport (which all may change next)
			_backup_state.capture();

			// For some reason rendering below water acts up in Source Engine games if the active clip plane is not cleared to zero before executing any draw calls ...
			// Also copying with a fullscreen triangle rather than a quad of two triangles solves some artifacts that otherwise occur in the second triangle there as well. Not sure what is going on ...
			const float zero_clip_plane[4] = { 0, 0, 0, 0 };
			_orig->SetClipPlane(0, zero_clip_plane);

			// Perform copy using rasterization pipeline
			_copy_state->Apply();

			_orig->SetTexture(0, static_cast<IDirect3DTexture9 *>(src_object));
			_orig->SetSamplerState(0, D3DSAMP_MINFILTER, stretch_filter);
			_orig->SetSamplerState(0, D3DSAMP_MAGFILTER, stretch_filter);

			com_ptr<IDirect3DSurface9> destination_surface;
			static_cast<IDirect3DTexture9 *>(dst_object)->GetSurfaceLevel(dst_subresource, &destination_surface);
			_orig->SetRenderTarget(0, destination_surface.get());
			for (DWORD target = 1; target < _caps.NumSimultaneousRTs; ++target)
				_orig->SetRenderTarget(target, nullptr);
			_orig->SetDepthStencilSurface(nullptr);

			if (dst_box != nullptr)
			{
				D3DVIEWPORT9 viewport;
				viewport.X = dst_rect.left;
				viewport.Y = dst_rect.top;
				viewport.Width = dst_rect.right - dst_rect.left;
				viewport.Height = dst_rect.bottom - dst_rect.top;
				viewport.MinZ = 0.0f;
				viewport.MaxZ = 1.0f;

				_orig->SetViewport(&viewport);
			}

			float vertices[3][5] = {
				// x      y      z      tu     tv
				{ -1.0f,  1.0f,  0.0f,  0.0f,  0.0f },
				{ -1.0f, -3.0f,  0.0f,  0.0f,  2.0f },
				{  3.0f,  1.0f,  0.0f,  2.0f,  0.0f },
			};

			if (src_box != nullptr)
			{
				D3DSURFACE_DESC desc;
				destination_surface->GetDesc(&desc);

				vertices[0][3] = src_rect.left * 2.0f / desc.Width;
				vertices[0][4] = src_rect.top * 2.0f / desc.Height;
				vertices[1][3] = src_rect.left * 2.0f / desc.Width;
				vertices[1][4] = src_rect.bottom * 2.0f / desc.Height;
				vertices[2][3] = src_rect.right * 2.0f / desc.Width;
				vertices[2][4] = src_rect.top * 2.0f / desc.Height;
			}

			_orig->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, vertices, sizeof(vertices[0]));

			_backup_state.apply_and_release();
			return;
		}
		case D3DRTYPE_TEXTURE | (D3DRTYPE_SURFACE << 4):
		{
			assert(dst_subresource == 0);

			com_ptr<IDirect3DSurface9> source_surface;
			static_cast<IDirect3DTexture9 *>(src_object)->GetSurfaceLevel(src_subresource, &source_surface);

			_orig->StretchRect(
				source_surface.get(), src_box != nullptr ? &src_rect : nullptr,
				static_cast<IDirect3DSurface9 *>(dst_object), dst_box != nullptr ? &dst_rect : nullptr, stretch_filter);
			return;
		}
	}

	assert(false); // Not implemented
}
void reshade::d3d9::device_impl::resolve(api::resource_handle src, uint32_t src_subresource, const int32_t src_offset[3], api::resource_handle dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], uint32_t)
{
	int32_t src_box[6] = {};
	if (src_offset != nullptr)
		std::copy_n(src_offset, 3, src_box);

	if (size != nullptr)
	{
		src_box[3] = src_box[0] + size[0];
		src_box[4] = src_box[1] + size[1];
		src_box[5] = src_box[2] + size[2];
	}
	else
	{
		const api::resource_desc desc = get_resource_desc(src);
		src_box[3] = src_box[0] + std::max(1u, desc.width >> src_subresource);
		src_box[4] = src_box[1] + std::max(1u, desc.height >> src_subresource);
		src_box[5] = src_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.depth_or_layers) >> src_subresource) : 1u);
	}

	int32_t dst_box[6] = {};
	if (dst_offset != nullptr)
		std::copy_n(dst_offset, 3, dst_box);

	if (size != nullptr)
	{
		dst_box[3] = dst_box[0] + size[0];
		dst_box[4] = dst_box[1] + size[1];
		dst_box[5] = dst_box[2] + size[2];
	}
	else
	{
		const api::resource_desc desc = get_resource_desc(dst);
		dst_box[3] = dst_box[0] + std::max(1u, desc.width >> dst_subresource);
		dst_box[4] = dst_box[1] + std::max(1u, desc.height >> dst_subresource);
		dst_box[5] = dst_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.depth_or_layers) >> dst_subresource) : 1u);
	}

	blit(src, src_subresource, src_box, dst, dst_subresource, dst_box, api::texture_filter::min_mag_mip_point);
}
void reshade::d3d9::device_impl::copy_resource(api::resource_handle src, api::resource_handle dst)
{
	const api::resource_desc desc = get_resource_desc(src);

	if (desc.type == api::resource_type::buffer)
	{
		copy_buffer_region(src, 0, dst, 0, ~0llu);
	}
	else
	{
		for (uint32_t level = 0; level < desc.levels; ++level)
		{
			const uint32_t subresource = level;

			copy_texture_region(src, subresource, nullptr, dst, subresource, nullptr, nullptr);
		}
	}
}
void reshade::d3d9::device_impl::copy_buffer_region(api::resource_handle, uint64_t, api::resource_handle, uint64_t, uint64_t)
{
	assert(false);
}
void reshade::d3d9::device_impl::copy_buffer_to_texture(api::resource_handle, uint64_t, uint32_t, uint32_t, api::resource_handle, uint32_t, const int32_t[6])
{
	assert(false);
}
void reshade::d3d9::device_impl::copy_texture_region(api::resource_handle src, uint32_t src_subresource, const int32_t src_offset[3], api::resource_handle dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3])
{
	int32_t src_box[6] = {};
	if (src_offset != nullptr)
		std::copy_n(src_offset, 3, src_box);

	if (size != nullptr)
	{
		src_box[3] = src_box[0] + size[0];
		src_box[4] = src_box[1] + size[1];
		src_box[5] = src_box[2] + size[2];
	}
	else
	{
		const api::resource_desc desc = get_resource_desc(src);
		src_box[3] = src_box[0] + std::max(1u, desc.width >> src_subresource);
		src_box[4] = src_box[1] + std::max(1u, desc.height >> src_subresource);
		src_box[5] = src_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.depth_or_layers) >> src_subresource) : 1u);
	}

	int32_t dst_box[6] = {};
	if (dst_offset != nullptr)
		std::copy_n(dst_offset, 3, dst_box);

	if (size != nullptr)
	{
		dst_box[3] = dst_box[0] + size[0];
		dst_box[4] = dst_box[1] + size[1];
		dst_box[5] = dst_box[2] + size[2];
	}
	else
	{
		const api::resource_desc desc = get_resource_desc(dst);
		dst_box[3] = dst_box[0] + std::max(1u, desc.width >> dst_subresource);
		dst_box[4] = dst_box[1] + std::max(1u, desc.height >> dst_subresource);
		dst_box[5] = dst_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.depth_or_layers) >> dst_subresource) : 1u);
	}

	blit(src, src_subresource, src_box, dst, dst_subresource, dst_box, api::texture_filter::min_mag_mip_point);
}
void reshade::d3d9::device_impl::copy_texture_to_buffer(api::resource_handle, uint32_t, const int32_t[6], api::resource_handle, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d9::device_impl::clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	assert(dsv.handle != 0);

	_backup_state.capture();

	_orig->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9 *>(dsv.handle));

	_orig->Clear(
		0, nullptr,
		((clear_flags & 0x1) != 0 ? D3DCLEAR_ZBUFFER : 0) |
		((clear_flags & 0x2) != 0 ? D3DCLEAR_STENCIL : 0),
		0, depth, stencil);

	_backup_state.apply_and_release();
}
void reshade::d3d9::device_impl::clear_render_target_views(uint32_t count, const api::resource_view_handle *rtvs, const float color[4])
{
#if 0
	assert(count <= _caps.NumSimultaneousRTs);

	_backup_state.capture();

	for (DWORD target = 0; target < count && target < _caps.NumSimultaneousRTs; ++target)
		_orig->SetRenderTarget(target, reinterpret_cast<IDirect3DSurface9 *>(rtvs[target].handle));
	for (DWORD target = count; target < _caps.NumSimultaneousRTs; ++target)
		_orig->SetRenderTarget(target, nullptr);

	_orig->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]), 0.0f, 0);

	_backup_state.apply_and_release();
#else
	for (UINT i = 0; i < count; ++i)
	{
		assert(rtvs[i].handle != 0);

		_orig->ColorFill(reinterpret_cast<IDirect3DSurface9 *>(rtvs[i].handle), nullptr, D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]));
	}
#endif
}
