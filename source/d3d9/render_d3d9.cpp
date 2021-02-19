/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d9.hpp"

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
	assert(desc.samples == 1);

	convert_usage_to_d3d_usage(desc.usage, internal_desc.Usage);

	if (levels != nullptr)
		*levels = desc.levels;
}
void reshade::d3d9::convert_resource_desc(const resource_desc &desc, D3DSURFACE_DESC &internal_desc, UINT *levels)
{
	internal_desc.Width = desc.width;
	internal_desc.Height = desc.height;
	assert(desc.depth_or_layers == 1 || desc.depth_or_layers == 6 /* D3DRTYPE_CUBETEXTURE */);
	internal_desc.Format = static_cast<D3DFORMAT>(desc.format);

	if (desc.samples > 1)
		internal_desc.MultiSampleType = static_cast<D3DMULTISAMPLE_TYPE>(desc.samples);
	else
		internal_desc.MultiSampleType = D3DMULTISAMPLE_NONE;

	convert_usage_to_d3d_usage(desc.usage, internal_desc.Usage);

	if (levels != nullptr)
		*levels = desc.levels;
}
void reshade::d3d9::convert_resource_desc(const resource_desc &desc, D3DINDEXBUFFER_DESC &internal_desc)
{
	assert(desc.buffer_size <= std::numeric_limits<UINT>::max());
	internal_desc.Size = static_cast<UINT>(desc.buffer_size);

	convert_usage_to_d3d_usage(desc.usage, internal_desc.Usage);
}
void reshade::d3d9::convert_resource_desc(const resource_desc &desc, D3DVERTEXBUFFER_DESC &internal_desc)
{
	assert(desc.buffer_size <= std::numeric_limits<UINT>::max());
	internal_desc.Size = static_cast<UINT>(desc.buffer_size);

	convert_usage_to_d3d_usage(desc.usage, internal_desc.Usage);
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
resource_desc reshade::d3d9::convert_resource_desc(const D3DINDEXBUFFER_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.buffer_size = internal_desc.Size;
	convert_d3d_usage_to_usage(internal_desc.Usage, desc.usage);
	desc.usage |= resource_usage::index_buffer;
	return desc;
}
resource_desc reshade::d3d9::convert_resource_desc(const D3DVERTEXBUFFER_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.buffer_size = internal_desc.Size;
	convert_d3d_usage_to_usage(internal_desc.Usage, desc.usage);
	desc.usage |= resource_usage::vertex_buffer;
	return desc;
}

reshade::d3d9::device_impl::device_impl(IDirect3DDevice9 *device) :
	_device(device), _app_state(device)
{
	D3DCAPS9 caps = {};
	_device->GetDeviceCaps(&caps);
	D3DDEVICE_CREATION_PARAMETERS creation_params = {};
	_device->GetCreationParameters(&creation_params);

	_num_samplers = caps.MaxSimultaneousTextures;
	_num_simultaneous_rendertargets = caps.NumSimultaneousRTs;
	if (_num_simultaneous_rendertargets > 8)
		_num_simultaneous_rendertargets = 8;
	_behavior_flags = creation_params.BehaviorFlags;

#if RESHADE_ADDON
	// Load and initialize add-ons
	reshade::addon::load_addons();
#endif

	com_ptr<IDirect3DSwapChain9> swapchain;
	device->GetSwapChain(0, &swapchain);
	D3DPRESENT_PARAMETERS pp = {};
	swapchain->GetPresentParameters(&pp);
	on_after_reset(pp);
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
	// Do not call add-on events if this device was already reset before
	if (_copy_state == nullptr)
		return;

	// Force add-ons to release all resources associated with this device before performing reset
	RESHADE_ADDON_EVENT(destroy_command_queue, this);
	RESHADE_ADDON_EVENT(destroy_device, this);

	_app_state.release_state_block();
	_copy_state.reset();
}
void reshade::d3d9::device_impl::on_after_reset(const D3DPRESENT_PARAMETERS &pp)
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

	// Create state block object
	_app_state.init_state_block();

	RESHADE_ADDON_EVENT(init_device, this);
	RESHADE_ADDON_EVENT(init_command_queue, this);

#if RESHADE_ADDON
	// Communicate default state to add-ons
	if (pp.EnableAutoDepthStencil)
	{
		com_ptr<IDirect3DSurface9> auto_depth_stencil;
		_device->GetDepthStencilSurface(&auto_depth_stencil);

		const reshade::api::resource_view_handle dsv = get_resource_view_handle(auto_depth_stencil.get());
		RESHADE_ADDON_EVENT(set_render_targets_and_depth_stencil, this, 0, nullptr, dsv);
	}
#endif
}

bool reshade::d3d9::device_impl::check_format_support(uint32_t format, resource_usage usage)
{
	if ((usage & resource_usage::unordered_access) != 0)
		return false;

	com_ptr<IDirect3D9> d3d;
	_device->GetDirect3D(&d3d);
	D3DDEVICE_CREATION_PARAMETERS cp;
	_device->GetCreationParameters(&cp);

	DWORD d3d_usage = 0;
	convert_usage_to_d3d_usage(usage, d3d_usage);

	return SUCCEEDED(d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, d3d_usage, D3DRTYPE_TEXTURE, static_cast<D3DFORMAT>(format)));
}

bool reshade::d3d9::device_impl::check_resource_handle_valid(resource_handle resource)
{
	return resource.handle != 0 && _resources.has_object(reinterpret_cast<IDirect3DResource9 *>(resource.handle));
}
bool reshade::d3d9::device_impl::check_resource_view_handle_valid(resource_view_handle view)
{
	return check_resource_handle_valid({ view.handle });
}

bool reshade::d3d9::device_impl::create_resource(resource_type type, const resource_desc &desc, resource_usage, resource_handle *out_resource)
{
	DWORD d3d_usage = 0;
	convert_usage_to_d3d_usage(desc.usage, d3d_usage);

	switch (type)
	{
		case resource_type::buffer:
		{
			assert(desc.buffer_size <= std::numeric_limits<UINT>::max());

			if ((desc.usage & resource_usage::index_buffer) != 0)
			{
				// TODO: Index format
				if (IDirect3DIndexBuffer9 *resource;
					SUCCEEDED(_device->CreateIndexBuffer(static_cast<UINT>(desc.buffer_size), d3d_usage, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT, &resource, nullptr)))
				{
					register_resource(resource);
					*out_resource = { reinterpret_cast<uintptr_t>(resource) };
					return true;
				}
			}
			if ((desc.usage & resource_usage::vertex_buffer) != 0)
			{
				if (IDirect3DVertexBuffer9 *resource;
					SUCCEEDED(_device->CreateVertexBuffer(static_cast<UINT>(desc.buffer_size), d3d_usage, 0, D3DPOOL_DEFAULT, &resource, nullptr)))
				{
					register_resource(resource);
					*out_resource = { reinterpret_cast<uintptr_t>(resource) };
					return true;
				}
			}
			break;
		}
		case resource_type::texture_1d:
		case resource_type::texture_2d:
		{
			// Array or multisample textures are not supported in Direct3D 9
			if (desc.depth_or_layers == 1 && desc.samples == 1)
			{
				if (IDirect3DTexture9 *resource;
					SUCCEEDED(_device->CreateTexture(desc.width, desc.height, desc.levels, d3d_usage, static_cast<D3DFORMAT>(desc.format), D3DPOOL_DEFAULT, &resource, nullptr)))
				{
					register_resource(resource);
					*out_resource = { reinterpret_cast<uintptr_t>(resource) };
					return true;
				}
			}
			break;
		}
		case resource_type::texture_3d:
		{
			assert(desc.samples == 1); // 3D textures can never have multisampling
			if (IDirect3DVolumeTexture9 *resource;
				SUCCEEDED(_device->CreateVolumeTexture(desc.width, desc.height, desc.depth_or_layers, desc.levels, d3d_usage, static_cast<D3DFORMAT>(desc.format), D3DPOOL_DEFAULT, &resource, nullptr)))
			{
				register_resource(resource);
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
	// Views with a different format than the resource are not supported in Direct3D 9
	assert(desc.format == get_resource_desc(resource).format);

	assert(resource.handle != 0);
	const auto resource_object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	switch (resource_object->GetType())
	{
		case D3DRTYPE_SURFACE:
		{
			assert(desc.dimension == resource_view_dimension::texture_2d || desc.dimension == resource_view_dimension::texture_2d_multisample);
			assert(desc.first_level == 0 && (desc.levels == 1 || desc.levels == std::numeric_limits<uint32_t>::max()));
			assert(desc.first_layer == 0 && (desc.layers == 1 || desc.layers == std::numeric_limits<uint32_t>::max()));
			if (type == resource_view_type::depth_stencil || type == resource_view_type::render_target)
			{
				resource_object->AddRef();
				*out_view = { resource.handle };
				return true;
			}
			break;
		}
		case D3DRTYPE_TEXTURE:
		{
			assert(desc.dimension == resource_view_dimension::texture_2d || desc.dimension == resource_view_dimension::texture_2d_multisample);
			assert(desc.first_layer == 0 && (desc.layers == 1 || desc.layers == std::numeric_limits<uint32_t>::max()));
			if (type == resource_view_type::depth_stencil || type == resource_view_type::render_target)
			{
				assert(desc.levels == 1);
				if (IDirect3DSurface9 *surface = nullptr;
					SUCCEEDED(static_cast<IDirect3DTexture9 *>(resource_object)->GetSurfaceLevel(desc.first_level, &surface)))
				{
					*out_view = { reinterpret_cast<uintptr_t>(surface) };
					return true;
				}
			}
			else if (type == resource_view_type::shader_resource && desc.first_level == 0)
			{
				resource_object->AddRef();
				*out_view = { resource.handle };
				return true;
			}
			break;
		}
		case D3DRTYPE_CUBETEXTURE:
		{
			if (type == resource_view_type::depth_stencil || type == resource_view_type::render_target)
			{
				assert(desc.dimension == resource_view_dimension::texture_2d || desc.dimension == resource_view_dimension::texture_2d_multisample);
				assert(desc.levels == 1 && desc.layers == 1);
				if (IDirect3DSurface9 *surface = nullptr;
					SUCCEEDED(static_cast<IDirect3DCubeTexture9 *>(resource_object)->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(desc.first_layer), desc.first_level, &surface)))
				{
					*out_view = { reinterpret_cast<uintptr_t>(surface) };
					return true;
				}
			}
			else if (type == resource_view_type::shader_resource && desc.first_level == 0 && desc.first_layer == 0)
			{
				assert(desc.dimension == resource_view_dimension::texture_cube);
				resource_object->AddRef();
				*out_view = { resource.handle };
				return true;
			}
			break;
		}
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
	destroy_resource({ view.handle });
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

	// If unable to get container, just return the resource directly
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
		case D3DRTYPE_VERTEXBUFFER:
		{
			D3DVERTEXBUFFER_DESC internal_desc = {};
			static_cast<IDirect3DVertexBuffer9 *>(resource_object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
		case D3DRTYPE_INDEXBUFFER:
		{
			D3DINDEXBUFFER_DESC internal_desc = {};
			static_cast<IDirect3DIndexBuffer9 *>(resource_object)->GetDesc(&internal_desc);
			return convert_resource_desc(internal_desc);
		}
	}

	assert(false); // Not implemented
	return {};
}

void reshade::d3d9::device_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	assert(instances <= 1 && first_instance == 0);
	_device->DrawPrimitive(D3DPT_TRIANGLELIST, first_vertex, vertices / 3);
}
void reshade::d3d9::device_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	assert(instances <= 1 && first_instance == 0);
	_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertex_offset, 0, indices, first_index, indices / 3);
}

void reshade::d3d9::device_impl::copy_resource(resource_handle source, resource_handle destination)
{
	assert(source.handle != 0 && destination.handle != 0);
	const auto source_object = reinterpret_cast<IDirect3DResource9 *>(source.handle);
	const auto destination_object = reinterpret_cast<IDirect3DResource9 *>(destination.handle);

	switch (source_object->GetType() | (destination_object->GetType() << 4))
	{
		case D3DRTYPE_SURFACE | (D3DRTYPE_SURFACE << 4):
		{
			_device->StretchRect(static_cast<IDirect3DSurface9 *>(source_object), nullptr, static_cast<IDirect3DSurface9 *>(destination_object), nullptr, D3DTEXF_NONE);
			return;
		}
		case D3DRTYPE_SURFACE | (D3DRTYPE_TEXTURE << 4):
		{
			com_ptr<IDirect3DSurface9> target;
			static_cast<IDirect3DTexture9 *>(destination_object)->GetSurfaceLevel(0, &target);
			// Stretching from an offscreen color surface into a texture is supported, however the other way around is not
			_device->StretchRect(static_cast<IDirect3DSurface9 *>(source_object), nullptr, target.get(), nullptr, D3DTEXF_NONE);
			return;
		}
		case D3DRTYPE_TEXTURE | (D3DRTYPE_TEXTURE << 4):
		{
			_app_state.capture();

			// Perform copy using fullscreen triangle
			_copy_state->Apply();

			// TODO: This copies the first level only ...
			com_ptr<IDirect3DSurface9> target;
			static_cast<IDirect3DTexture9 *>(destination_object)->GetSurfaceLevel(0, &target);
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

			_app_state.apply_and_release();
			return;
		}
		case D3DRTYPE_TEXTURE | (D3DRTYPE_SURFACE << 4):
		{
			_app_state.capture();

			_copy_state->Apply();

			_device->SetTexture(0, static_cast<IDirect3DTexture9 *>(source_object));
			_device->SetRenderTarget(0, static_cast<IDirect3DSurface9 *>(destination_object));

			const float vertices[4][5] = {
				// x      y      z      tu     tv
				{ -1.0f,  1.0f,  0.0f,  0.0f,  0.0f },
				{  1.0f,  1.0f,  0.0f,  1.0f,  0.0f },
				{ -1.0f, -1.0f,  0.0f,  0.0f,  1.0f },
				{  1.0f, -1.0f,  0.0f,  1.0f,  1.0f },
			};
			_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(vertices[0]));

			_app_state.apply_and_release();
			return;
		}
	}

	assert(false); // Not implemented
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
