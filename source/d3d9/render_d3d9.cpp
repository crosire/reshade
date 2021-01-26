/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d9.hpp"
#include <algorithm>

using namespace reshade::api;

static void convert_usage_to_d3d_usage(const resource_usage usage, DWORD &d3d_usage)
{
	// Copying textures is implemented using the rasterization pipeline (see 'device_impl::copy_resource' implementation), so needs render target usage too
	if ((usage & (resource_usage::render_target | resource_usage::copy_dest | resource_usage::copy_source)) != 0)
		d3d_usage |= D3DUSAGE_RENDERTARGET;
	else
		d3d_usage &= ~D3DUSAGE_RENDERTARGET;

	if ((usage & (resource_usage::depth_stencil)) != 0)
		d3d_usage |= D3DUSAGE_DEPTHSTENCIL;
	else
		d3d_usage &= ~D3DUSAGE_DEPTHSTENCIL;

	// Unordered access is not supported in D3D9
	assert((usage & resource_usage::unordered_access) == 0);
}
static void convert_d3d_usage_to_usage(const DWORD d3d_usage, resource_usage &usage)
{
	if (d3d_usage & D3DUSAGE_RENDERTARGET)
		usage |= resource_usage::render_target | resource_usage::copy_dest | resource_usage::copy_source;
	if (d3d_usage & D3DUSAGE_DEPTHSTENCIL)
		usage |= resource_usage::depth_stencil;
}

void reshade::d3d9::convert_resource_desc(const resource_desc &desc, D3DVOLUME_DESC &internal_desc, UINT *levels)
{
	internal_desc.Width = desc.width;
	internal_desc.Height = desc.height;
	internal_desc.Depth = desc.depth_or_layers;
	internal_desc.Format = static_cast<D3DFORMAT>(desc.format);
	assert(desc.samples <= 1);

	convert_usage_to_d3d_usage(desc.usage, internal_desc.Usage);

	if (levels != nullptr)
		*levels = desc.levels;
}
void reshade::d3d9::convert_resource_desc(const resource_desc &desc, D3DSURFACE_DESC &internal_desc, UINT *levels)
{
	internal_desc.Width = desc.width;
	internal_desc.Height = desc.height;
	assert(desc.depth_or_layers <= 1 || desc.depth_or_layers == 6 /* D3DRTYPE_CUBETEXTURE */);
	internal_desc.Format = static_cast<D3DFORMAT>(desc.format);

	if (desc.samples > 1)
		internal_desc.MultiSampleType = static_cast<D3DMULTISAMPLE_TYPE>(desc.samples);
	else
		internal_desc.MultiSampleType = D3DMULTISAMPLE_NONE;

	convert_usage_to_d3d_usage(desc.usage, internal_desc.Usage);

	if (levels != nullptr)
		*levels = desc.levels;
}
resource_desc reshade::d3d9::convert_resource_desc(const D3DVOLUME_DESC &internal_desc, UINT levels)
{
	assert(internal_desc.Type == D3DRTYPE_VOLUME || internal_desc.Type == D3DRTYPE_VOLUMETEXTURE);

	resource_desc desc = {};
	desc.width = internal_desc.Width;
	desc.height = internal_desc.Height;
	assert(internal_desc.Depth <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(internal_desc.Depth);
	assert(levels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(levels);
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.samples = 1;

	convert_d3d_usage_to_usage(internal_desc.Usage, desc.usage);
	if (internal_desc.Type == D3DRTYPE_VOLUMETEXTURE)
		desc.usage |= resource_usage::shader_resource;

	return desc;
}
resource_desc reshade::d3d9::convert_resource_desc(const D3DSURFACE_DESC &internal_desc, UINT levels)
{
	assert(internal_desc.Type == D3DRTYPE_SURFACE || internal_desc.Type == D3DRTYPE_TEXTURE || internal_desc.Type == D3DRTYPE_CUBETEXTURE);

	resource_desc desc = {};
	desc.width = internal_desc.Width;
	desc.height = internal_desc.Height;
	desc.depth_or_layers = internal_desc.Type == D3DRTYPE_CUBETEXTURE ? 6 : 1;
	assert(levels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(levels);
	desc.format = static_cast<uint32_t>(internal_desc.Format);

	if (internal_desc.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES)
		desc.samples = static_cast<uint16_t>(internal_desc.MultiSampleType);
	else
		desc.samples = 1;

	convert_d3d_usage_to_usage(internal_desc.Usage, desc.usage);
	if (internal_desc.Type == D3DRTYPE_TEXTURE || internal_desc.Type == D3DRTYPE_CUBETEXTURE)
		desc.usage |= resource_usage::shader_resource;

	return desc;
}

void reshade::d3d9::convert_resource_view_desc(const resource_view_desc &desc, D3DSURFACE_DESC &internal_desc)
{
	assert(desc.dimension == resource_view_dimension::texture_2d || desc.dimension == resource_view_dimension::texture_2d_multisample);

	internal_desc.Format = static_cast<D3DFORMAT>(desc.format);
}
resource_view_desc reshade::d3d9::convert_resource_view_desc(const D3DSURFACE_DESC &internal_desc)
{
	resource_view_desc desc = {};
	desc.dimension = internal_desc.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES ? resource_view_dimension::texture_2d_multisample : resource_view_dimension::texture_2d;
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.first_level = 0;
	desc.levels = 1;
	desc.first_layer = 0;
	desc.layers = 1;

	return desc;
}

reshade::d3d9::device_impl::device_impl(IDirect3DDevice9 *device) :
	_device(device), _backup_state(device)
{
	com_ptr<IDirect3D9> d3d;
	_device->GetDirect3D(&d3d);

	D3DCAPS9 caps = {};
	_device->GetDeviceCaps(&caps);
	D3DDEVICE_CREATION_PARAMETERS creation_params = {};
	_device->GetCreationParameters(&creation_params);

	_num_samplers = caps.MaxSimultaneousTextures;
	_num_simultaneous_rendertargets = std::min(caps.NumSimultaneousRTs, static_cast<DWORD>(8));
	_behavior_flags = creation_params.BehaviorFlags;

#if RESHADE_ADDON
	// Load and initialize add-ons
	reshade::addon::load_addons();
#endif

	on_after_reset();
}
reshade::d3d9::device_impl::~device_impl()
{
	on_reset();

#if RESHADE_ADDON
	reshade::addon::unload_addons();
#endif
}

void reshade::d3d9::device_impl::on_reset()
{
	// Force add-ons to release all resources associated with this device before performing reset
	RESHADE_ADDON_EVENT(destroy_command_queue, this);
	RESHADE_ADDON_EVENT(destroy_device, this);

	_copy_state.reset();
	_backup_state.release_state_block();
}
void reshade::d3d9::device_impl::on_after_reset()
{
	HRESULT hr = _device->BeginStateBlock();
	if (SUCCEEDED(hr))
	{
		_device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
		_device->SetPixelShader(nullptr);
		_device->SetVertexShader(nullptr);
		_device->SetRenderState(D3DRS_ZENABLE, false);
		_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		_device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
		_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
		_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
		_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		_device->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
		_device->SetRenderState(D3DRS_FOGENABLE, false);
		_device->SetRenderState(D3DRS_STENCILENABLE, false);
		_device->SetRenderState(D3DRS_CLIPPING, false);
		_device->SetRenderState(D3DRS_LIGHTING, false);
		_device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
		_device->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
		_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		_device->SetRenderState(D3DRS_SRGBWRITEENABLE, false);
		_device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
		_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		_device->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
		_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		_device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		_device->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, 0);
		_device->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0);
		_device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);

		hr = _device->EndStateBlock(&_copy_state);
	}

	_backup_state.init_state_block();

	RESHADE_ADDON_EVENT(init_device, this);
	RESHADE_ADDON_EVENT(init_command_queue, this);
}

bool reshade::d3d9::device_impl::check_format_support(uint32_t format, resource_usage usage)
{
	com_ptr<IDirect3D9> d3d;
	_device->GetDirect3D(&d3d);
	D3DDEVICE_CREATION_PARAMETERS cp;
	_device->GetCreationParameters(&cp);

	DWORD d3d_usage = 0;
	convert_usage_to_d3d_usage(usage, d3d_usage);

	return SUCCEEDED(d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, d3d_usage, D3DRTYPE_TEXTURE, static_cast<D3DFORMAT>(format)));
}

bool reshade::d3d9::device_impl::is_resource_valid(resource_handle resource)
{
	return resource.handle != 0 && _resources.has_object(reinterpret_cast<IDirect3DResource9 *>(resource.handle));
}
bool reshade::d3d9::device_impl::is_resource_view_valid(resource_view_handle view)
{
	return view.handle != 0 && _resources.has_object(reinterpret_cast<IDirect3DResource9 *>(view.handle));
}

bool reshade::d3d9::device_impl::create_resource(resource_type type, const resource_desc &desc, resource_handle *out_resource)
{
	DWORD d3d_usage = 0;
	convert_usage_to_d3d_usage(desc.usage, d3d_usage);

	switch (type)
	{
		case resource_type::texture_1d:
		case resource_type::texture_2d:
		{
			if (IDirect3DTexture9 *resource;
				SUCCEEDED(_device->CreateTexture(desc.width, desc.height, desc.levels, d3d_usage, static_cast<D3DFORMAT>(desc.format), D3DPOOL_DEFAULT, &resource, nullptr)))
			{
				_resources.register_object(resource);
				*out_resource = { reinterpret_cast<uintptr_t>(resource) };
				return true;
			}
			break;
		}
		case resource_type::texture_3d:
		{
			if (IDirect3DVolumeTexture9 *resource;
				SUCCEEDED(_device->CreateVolumeTexture(desc.width, desc.height, desc.depth_or_layers, desc.levels, d3d_usage, static_cast<D3DFORMAT>(desc.format), D3DPOOL_DEFAULT, &resource, nullptr)))
			{
				_resources.register_object(resource);
				*out_resource = { reinterpret_cast<uintptr_t>(resource) };
				return true;
			}
			break;
		}
	}

	*out_resource = { 0 };
	return false;
}
bool reshade::d3d9::device_impl::create_resource_view(resource_handle resource, resource_view_type type, const resource_view_desc &desc, resource_view_handle *out_view)
{
	assert(resource.handle != 0);
	const auto resource_object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	const D3DRESOURCETYPE resource_type = resource_object->GetType();
	if (resource_type == D3DRTYPE_TEXTURE || resource_type == D3DRTYPE_VOLUMETEXTURE || resource_type == D3DRTYPE_CUBETEXTURE)
	{
		if (type == resource_view_type::shader_resource)
		{
			resource_object->AddRef();
			*out_view = { resource.handle };
			return true;
		}
		else if (type == resource_view_type::depth_stencil || type == resource_view_type::render_target)
		{
			assert(desc.levels <= 1);
			if (IDirect3DSurface9 *surface = nullptr;
				SUCCEEDED(static_cast<IDirect3DTexture9 *>(resource_object)->GetSurfaceLevel(desc.first_level, &surface)))
			{
				*out_view = { reinterpret_cast<uintptr_t>(surface) };
				return true;
			}
		}
	}
	else if (resource_type == D3DRTYPE_SURFACE && (type == resource_view_type::depth_stencil || type == resource_view_type::render_target))
	{
		resource_object->AddRef();
		*out_view = { resource.handle };
		return true;
	}

	*out_view = { 0 };
	return false;
}

void reshade::d3d9::device_impl::destroy_resource(resource_handle resource)
{
	assert(resource.handle != 0);
	reinterpret_cast<IDirect3DResource9 *>(resource.handle)->Release();
}
void reshade::d3d9::device_impl::destroy_resource_view(resource_view_handle view)
{
	assert(view.handle != 0);
	reinterpret_cast<IDirect3DResource9 *>(view.handle)->Release();
}

void reshade::d3d9::device_impl::get_resource_from_view(resource_view_handle view, resource_handle *out_resource)
{
	assert(view.handle != 0);
	const auto resource_object = reinterpret_cast<IDirect3DResource9 *>(view.handle);

	if (com_ptr<IDirect3DSurface9> surface;
		SUCCEEDED(resource_object->QueryInterface(IID_PPV_ARGS(&surface))))
	{
		if (com_ptr<IDirect3DResource9> resource;
			SUCCEEDED(surface->GetContainer(IID_PPV_ARGS(&resource))))
		{
			*out_resource = { reinterpret_cast<uintptr_t>(resource.get()) };
			return;
		}
	}

	// If unable to get container, just return the surface directly
	*out_resource = { view.handle };
}

resource_desc reshade::d3d9::device_impl::get_resource_desc(resource_handle resource)
{
	assert(resource.handle != 0);
	const auto resource_object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	switch (resource_object->GetType())
	{
		case D3DRTYPE_SURFACE:
		{
			D3DSURFACE_DESC internal_desc = {};
			static_cast<IDirect3DSurface9 *>(resource_object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3DRTYPE_TEXTURE:
		{
			D3DSURFACE_DESC internal_desc = {};
			static_cast<IDirect3DTexture9 *>(resource_object)->GetLevelDesc(0, &internal_desc);
			internal_desc.Type = D3DRTYPE_TEXTURE;
			return convert_resource_desc(internal_desc, static_cast<IDirect3DTexture9 *>(resource_object)->GetLevelCount());
		}
		case D3DRTYPE_VOLUMETEXTURE:
		{
			D3DVOLUME_DESC internal_desc = {};
			static_cast<IDirect3DVolumeTexture9 *>(resource_object)->GetLevelDesc(0, &internal_desc);
			internal_desc.Type = D3DRTYPE_VOLUMETEXTURE;
			return convert_resource_desc(internal_desc, static_cast<IDirect3DVolumeTexture9 *>(resource_object)->GetLevelCount());
		}
		case D3DRTYPE_CUBETEXTURE:
		{
			D3DSURFACE_DESC internal_desc = {};
			static_cast<IDirect3DCubeTexture9 *>(resource_object)->GetLevelDesc(0, &internal_desc);
			internal_desc.Type = D3DRTYPE_CUBETEXTURE;
			return convert_resource_desc(internal_desc, static_cast<IDirect3DCubeTexture9 *>(resource_object)->GetLevelCount());
		}
	}

	assert(false);
	return {};
}

void reshade::d3d9::device_impl::clear_depth_stencil_view(resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	com_ptr<IDirect3DSurface9> depth_stencil;
	_device->GetDepthStencilSurface(&depth_stencil);

	_device->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9 *>(dsv.handle));

	_device->Clear(
		0, nullptr,
		((clear_flags & 0x1) != 0 ? D3DCLEAR_ZBUFFER : 0) |
		((clear_flags & 0x2) != 0 ? D3DCLEAR_STENCIL : 0),
		0, depth, stencil);

	_device->SetDepthStencilSurface(depth_stencil.get());
}
void reshade::d3d9::device_impl::clear_render_target_view(resource_view_handle rtv, const float color[4])
{
	D3DVIEWPORT9 viewport = {};
	_device->GetViewport(&viewport);
	com_ptr<IDirect3DSurface9> render_targets[8];
	for (DWORD target = 0; target < _num_simultaneous_rendertargets; ++target)
		_device->GetRenderTarget(target, &render_targets[target]);

	_device->SetRenderTarget(0, reinterpret_cast<IDirect3DSurface9 *>(rtv.handle));
	for (DWORD target = 1; target < _num_simultaneous_rendertargets; ++target)
		_device->SetRenderTarget(target, nullptr);

	_device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]), 0.0f, 0);

	for (DWORD target = 0; target < _num_simultaneous_rendertargets; ++target)
		_device->SetRenderTarget(target, render_targets[target].get());
	_device->SetViewport(&viewport);
}

void reshade::d3d9::device_impl::copy_resource(resource_handle source, resource_handle dest)
{
	assert(source.handle != 0 && dest.handle != 0);
	const auto dest_object = reinterpret_cast<IDirect3DResource9 *>(dest.handle);
	const auto source_object = reinterpret_cast<IDirect3DResource9 *>(source.handle);

	const D3DRESOURCETYPE type = dest_object->GetType();
	if (type != source_object->GetType())
		return; // Copying is only supported between resources of the same type

	if (type == D3DRTYPE_TEXTURE)
	{
		_backup_state.capture();

		// Perform copy using fullscreen triangle
		_copy_state->Apply();

		com_ptr<IDirect3DSurface9> target;
		static_cast<IDirect3DTexture9 *>(dest_object)->GetSurfaceLevel(0, &target);
		_device->SetTexture(0, static_cast<IDirect3DTexture9 *>(source_object));
		_device->SetRenderTarget(0, target.get());

		const float vertices[4][5] = {
			// x      y      z      tu     tv
			{ -1.0f,  1.0f,  0.0f,  0.0f,  0.0f },
			{  1.0f,  1.0f,  0.0f,  1.0f,  0.0f },
			{ -1.0f, -1.0f,  0.0f,  0.0f,  1.0f },
			{  1.0f, -1.0f,  0.0f,  1.0f,  1.0f },
		};
		_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(vertices[0]));

		_backup_state.apply_and_release();
		return;
	}
	if (type == D3DRTYPE_SURFACE)
	{
		_device->StretchRect(static_cast<IDirect3DSurface9 *>(source_object), nullptr, static_cast<IDirect3DSurface9 *>(dest_object), nullptr, D3DTEXF_NONE);
		return;
	}

	assert(false);
}
