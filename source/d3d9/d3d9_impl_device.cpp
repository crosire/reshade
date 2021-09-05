/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "com_utils.hpp"
#include "d3d9_impl_device.hpp"
#include "d3d9_impl_type_convert.hpp"
#include <algorithm>

static inline bool convert_format_internal(reshade::api::format format, D3DFORMAT &internal_format)
{
	if (format == reshade::api::format::r8_typeless || format == reshade::api::format::r8_unorm ||
		format == reshade::api::format::r8g8_typeless || format == reshade::api::format::r8g8_unorm)
	{
		// Use 4-component format so that unused components are returned as zero and alpha as one (to match behavior from other APIs)
		internal_format = D3DFMT_X8R8G8B8;
		return true;
	}
	if (format == reshade::api::format::r8g8b8a8_typeless || format == reshade::api::format::r8g8b8a8_unorm || format == reshade::api::format::r8g8b8a8_unorm_srgb)
	{
		// Use 'D3DFMT_A8R8G8B8' instead of 'D3DFMT_A8B8G8R8', since the later is not supported well
		// This has to be mitiated in 'update_texture_region' by flipping RGBA to BGRA
		internal_format = D3DFMT_A8R8G8B8;
		return true;
	}

	return internal_format != D3DFMT_UNKNOWN;
}

reshade::d3d9::device_impl::device_impl(IDirect3DDevice9 *device) :
	api_object_impl(device), _caps(), _cp(), _backup_state(device)
{
	_orig->GetDirect3D(&_d3d);
	_orig->GetDeviceCaps(&_caps);
	_orig->GetCreationParameters(&_cp);

	// Maximum simultaneous number of render targets is typically 4 in D3D9
	assert(_caps.NumSimultaneousRTs <= ARRAYSIZE(framebuffer_impl::rtv));

#if RESHADE_ADDON
	load_addons();

	create_global_pipeline_layout();
#endif

	com_ptr<IDirect3DSwapChain9> swapchain;
	device->GetSwapChain(0, &swapchain);
	assert(swapchain != nullptr); // There should always be an implicit swap chain

	D3DPRESENT_PARAMETERS pp = {};
	swapchain->GetPresentParameters(&pp);

	on_init(pp);
}
reshade::d3d9::device_impl::~device_impl()
{
	on_reset();

#if RESHADE_ADDON
	destroy_global_pipeline_layout();

	unload_addons();
#endif
}

bool reshade::d3d9::device_impl::on_init(const D3DPRESENT_PARAMETERS &pp)
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
	}

	if (FAILED(hr))
	{
		LOG(ERROR) << "Failed to create copy pipeline!";
		return false;
	}

	if (!_backup_state.init_state_block())
	{
		LOG(ERROR) << "Failed to create backup state block!";
		return false;
	}

	// Create input layout for vertex buffer which holds vertex indices
	{
		const UINT max_vertices = 100; // TODO: Make this configurable or automatically resize when encountering larger draw calls

		if (FAILED(_orig->CreateVertexBuffer(max_vertices * sizeof(float), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &_default_input_stream, nullptr)))
		{
			LOG(ERROR) << "Failed to create default input stream!";
			return false;
		}

		if (float *data;
			SUCCEEDED(_default_input_stream->Lock(0, max_vertices * sizeof(float), reinterpret_cast<void **>(&data), 0)))
		{
			for (UINT i = 0; i < max_vertices; ++i)
				data[i] = static_cast<float>(i);
			_default_input_stream->Unlock();
		}

		const D3DVERTEXELEMENT9 declaration[] = {
			{ 0, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			D3DDECL_END()
		};

		if (FAILED(_orig->CreateVertexDeclaration(declaration, &_default_input_layout)))
		{
			LOG(ERROR) << "Failed to create default vertex declaration!";
			return false;
		}
	}

#if RESHADE_ADDON
	// Reserve some space for samplers to avoid reallocation during a frame
	_cached_sampler_states.reserve(32);

	invoke_addon_event<addon_event::init_device>(this);
	invoke_addon_event<addon_event::init_command_list>(this);
	invoke_addon_event<addon_event::init_command_queue>(this);

	if (com_ptr<IDirect3DSurface9> auto_depth_stencil;
		pp.EnableAutoDepthStencil &&
		SUCCEEDED(_orig->GetDepthStencilSurface(&auto_depth_stencil)))
	{
		auto desc = get_resource_desc(api::resource { reinterpret_cast<uintptr_t>(auto_depth_stencil.get()) });

		if (invoke_addon_event<addon_event::create_resource>(this, desc, nullptr, api::resource_usage::depth_stencil))
		{
			D3DSURFACE_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc, nullptr, _caps);

			// Need to replace auto depth stencil if an add-on modified the description
			if (com_ptr<IDirect3DSurface9> auto_depth_stencil_replacement;
				SUCCEEDED(create_surface_replacement(internal_desc, &auto_depth_stencil_replacement)))
			{
				// The device will hold a reference to the surface after binding it, so can release this one afterwards
				_orig->SetDepthStencilSurface(auto_depth_stencil_replacement.get());

				auto_depth_stencil = std::move(auto_depth_stencil_replacement);
			}
		}

		// In case surface was replaced with a texture resource
		const api::resource resource = get_resource_from_view(reshade::api::resource_view { reinterpret_cast<uintptr_t>(auto_depth_stencil.get()) });

		invoke_addon_event<addon_event::init_resource>(this, desc, nullptr, api::resource_usage::depth_stencil, resource);
		invoke_addon_event<addon_event::init_resource_view>(this, resource, api::resource_usage::depth_stencil, api::resource_view_desc(desc.texture.format), api::resource_view { reinterpret_cast<uintptr_t>(auto_depth_stencil.get()) });

		register_destruction_callback_d3d9(reinterpret_cast<IDirect3DResource9 *>(resource.handle), [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, resource);
		});
		register_destruction_callback_d3d9(auto_depth_stencil.get(), [this, resource_view = auto_depth_stencil.get()]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource_view) });
		}, reinterpret_cast<uintptr_t>(auto_depth_stencil.get()) == resource.handle ? 1 : 0);

		// Communicate default state to add-ons
		invoke_addon_event<addon_event::bind_render_targets_and_depth_stencil>(this, 0, nullptr, reshade::api::resource_view { reinterpret_cast<uintptr_t>(auto_depth_stencil.get()) });
	}
#else
	UNREFERENCED_PARAMETER(pp);
#endif

	return true;
}
void reshade::d3d9::device_impl::on_reset()
{
	// Do not call add-on events if initialization failed or this device was already reset
	if (_copy_state == nullptr)
		return;

	// Release backup state before invoking device destroy event, since it may still hold references to resources and releasing it may therefore invoke resource destroy events
	// And all resource destroy events should have happened before the device destroy event
	_backup_state.release_state_block();

#if RESHADE_ADDON
	for (const auto [hash, handle] : _cached_sampler_states)
	{
		invoke_addon_event<addon_event::destroy_sampler>(this, handle);
		destroy_sampler(handle);
	}

	_cached_sampler_states.clear();

	// Force add-ons to release all resources associated with this device before performing reset
	invoke_addon_event<addon_event::destroy_command_queue>(this);
	invoke_addon_event<addon_event::destroy_command_list>(this);
	invoke_addon_event<addon_event::destroy_device>(this);
#endif

	_copy_state.reset();
	_default_input_stream.reset();
	_default_input_layout.reset();
}

#if RESHADE_ADDON
void reshade::d3d9::device_impl::create_global_pipeline_layout()
{
	// Create global pipeline layout that is used for all application descriptor events
	api::descriptor_range push_descriptors = {};
	api::pipeline_layout_param layout_params[8] = {};

	// See https://docs.microsoft.com/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-vs-registers-vs-3-0
	layout_params[2].type = api::pipeline_layout_param_type::push_constants;
	layout_params[2].push_constants.count = _caps.MaxVertexShaderConst * 4; // c#
	layout_params[2].push_constants.visibility = api::shader_stage::vertex;
	layout_params[3].type = api::pipeline_layout_param_type::push_constants;
	layout_params[3].push_constants.count = 16 * 4; // i#
	layout_params[3].push_constants.visibility = api::shader_stage::vertex;
	layout_params[4].type = api::pipeline_layout_param_type::push_constants;
	layout_params[4].push_constants.count = 16; // b#
	layout_params[4].push_constants.visibility = api::shader_stage::vertex;

	push_descriptors.type = api::descriptor_type::sampler_with_resource_view;
	push_descriptors.array_size = 4; // s#, Vertex shaders only support 4 sampler slots (D3DVERTEXTEXTURESAMPLER0 - D3DVERTEXTEXTURESAMPLER3)
	push_descriptors.visibility = api::shader_stage::vertex;
	layout_params[0].type = api::pipeline_layout_param_type::push_descriptors;
	create_descriptor_set_layout(1, &push_descriptors, true, &layout_params[0].descriptor_layout);

	// See https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-ps-registers-ps-3-0
	layout_params[5].type = api::pipeline_layout_param_type::push_constants;
	layout_params[5].push_constants.count = 224 * 4; // c#
	layout_params[5].push_constants.visibility = api::shader_stage::pixel;
	layout_params[6].type = api::pipeline_layout_param_type::push_constants;
	layout_params[6].push_constants.count = 16 * 4; // i#
	layout_params[6].push_constants.visibility = api::shader_stage::pixel;
	layout_params[7].type = api::pipeline_layout_param_type::push_constants;
	layout_params[7].push_constants.count = 16; // b#
	layout_params[7].push_constants.visibility = api::shader_stage::pixel;

	push_descriptors.type = api::descriptor_type::sampler_with_resource_view;
	push_descriptors.array_size = _caps.MaxSimultaneousTextures; // s#
	push_descriptors.visibility = api::shader_stage::pixel;
	layout_params[1].type = api::pipeline_layout_param_type::push_descriptors;
	create_descriptor_set_layout(1, &push_descriptors, true, &layout_params[1].descriptor_layout);

	create_pipeline_layout(8, layout_params, &_global_pipeline_layout);
}
void reshade::d3d9::device_impl::destroy_global_pipeline_layout()
{
	const std::vector<api::pipeline_layout_param> &layout_params = reinterpret_cast<pipeline_layout_impl *>(_global_pipeline_layout.handle)->params;

	destroy_descriptor_set_layout(layout_params[0].descriptor_layout);
	destroy_descriptor_set_layout(layout_params[1].descriptor_layout);

	destroy_pipeline_layout(_global_pipeline_layout);
}
#endif

bool reshade::d3d9::device_impl::check_capability(api::device_caps capability) const
{
	switch (capability)
	{
	case api::device_caps::compute_shader:
	case api::device_caps::geometry_shader:
	case api::device_caps::hull_and_domain_shader:
	case api::device_caps::logic_op:
	case api::device_caps::dual_src_blend:
	case api::device_caps::independent_blend:
		return false;
	case api::device_caps::fill_mode_non_solid:
	case api::device_caps::bind_render_targets_and_depth_stencil:
		return true;
	case api::device_caps::multi_viewport:
		return false;
	case api::device_caps::partial_push_constant_updates:
	case api::device_caps::partial_push_descriptor_updates:
		return true;
	case api::device_caps::draw_instanced:
	case api::device_caps::draw_or_dispatch_indirect:
	case api::device_caps::copy_buffer_region:
	case api::device_caps::copy_buffer_to_texture:
		return false;
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
		return true;
	case api::device_caps::copy_query_pool_results:
	case api::device_caps::sampler_compare:
		return false;
	case api::device_caps::sampler_anisotropic:
	case api::device_caps::sampler_with_resource_view:
		return true;
	default:
		return false;
	}
}
bool reshade::d3d9::device_impl::check_format_support(api::format format, api::resource_usage usage) const
{
	if ((usage & api::resource_usage::unordered_access) != api::resource_usage::undefined)
		return false;

	DWORD d3d_usage = 0;
	convert_resource_usage_to_d3d_usage(usage, d3d_usage);

	const D3DFORMAT d3d_format = convert_format(format);
	return d3d_format != D3DFMT_UNKNOWN && SUCCEEDED(_d3d->CheckDeviceFormat(_cp.AdapterOrdinal, _cp.DeviceType, D3DFMT_X8R8G8B8, d3d_usage, D3DRTYPE_TEXTURE, d3d_format));
}

bool reshade::d3d9::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out)
{
	// Comparison sampling is not supported in D3D9
	if ((static_cast<uint32_t>(desc.filter) & 0x80) != 0)
	{
		*out = { 0 };
		return false;
	}

	const auto impl = new sampler_impl();
	impl->state[D3DSAMP_ADDRESSU] = static_cast<DWORD>(desc.address_u);
	impl->state[D3DSAMP_ADDRESSV] = static_cast<DWORD>(desc.address_v);
	impl->state[D3DSAMP_ADDRESSW] = static_cast<DWORD>(desc.address_w);
	impl->state[D3DSAMP_BORDERCOLOR] = 0;
	impl->state[D3DSAMP_MAGFILTER] = ((static_cast<DWORD>(desc.filter) & 0x0C) >> 2) + 1;
	impl->state[D3DSAMP_MINFILTER] = ((static_cast<DWORD>(desc.filter) & 0x30) >> 4) + 1;
	impl->state[D3DSAMP_MIPFILTER] = ((static_cast<DWORD>(desc.filter) & 0x03)     ) + 1;
	impl->state[D3DSAMP_MIPMAPLODBIAS] = *reinterpret_cast<const DWORD *>(&desc.mip_lod_bias);
	impl->state[D3DSAMP_MAXMIPLEVEL] = desc.min_lod > 0 ? static_cast<DWORD>(desc.min_lod) : 0;
	impl->state[D3DSAMP_MAXANISOTROPY] = static_cast<DWORD>(desc.max_anisotropy);

	if (desc.filter == api::filter_type::anisotropic)
	{
		impl->state[D3DSAMP_MINFILTER] = D3DTEXF_ANISOTROPIC;
		impl->state[D3DSAMP_MAGFILTER] = D3DTEXF_ANISOTROPIC;
	}

	*out = { reinterpret_cast<uintptr_t>(impl) };
	return true;
}
void reshade::d3d9::device_impl::destroy_sampler(api::sampler handle)
{
	delete reinterpret_cast<sampler_impl *>(handle.handle);
}

bool reshade::d3d9::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage, api::resource *out)
{
	switch (desc.type)
	{
		case api::resource_type::buffer:
		{
			switch (desc.usage & (api::resource_usage::index_buffer | api::resource_usage::vertex_buffer | api::resource_usage::constant_buffer))
			{
				case api::resource_usage::index_buffer:
				{
					D3DINDEXBUFFER_DESC internal_desc = {};
					convert_resource_desc(desc, internal_desc);
					internal_desc.Format = D3DFMT_INDEX16; // TODO: Index format

					if (com_ptr<IDirect3DIndexBuffer9> object;
						SUCCEEDED(_orig->CreateIndexBuffer(internal_desc.Size, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, &object, nullptr)))
					{
						*out = { reinterpret_cast<uintptr_t>(object.release()) };

						if (initial_data != nullptr)
						{
							update_buffer_region(initial_data->data, *out, 0, desc.buffer.size);
						}
						return true;
					}
					break;
				}
				case api::resource_usage::vertex_buffer:
				{
					D3DVERTEXBUFFER_DESC internal_desc = {};
					convert_resource_desc(desc, internal_desc);

					if (com_ptr<IDirect3DVertexBuffer9> object;
						SUCCEEDED(_orig->CreateVertexBuffer(internal_desc.Size, internal_desc.Usage, internal_desc.FVF, internal_desc.Pool, &object, nullptr)))
					{
						*out = { reinterpret_cast<uintptr_t>(object.release()) };

						if (initial_data != nullptr)
						{
							update_buffer_region(initial_data->data, *out, 0, desc.buffer.size);
						}
						return true;
					}
					break;
				}
			}
			break;
		}
		case api::resource_type::texture_1d:
		case api::resource_type::texture_2d:
		{
			// Array or multisample textures are not supported in Direct3D 9
			if (desc.texture.depth_or_layers != 1 || desc.texture.samples != 1)
				break;

			UINT levels = 0;
			D3DSURFACE_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc, &levels, _caps);

			if (!convert_format_internal(desc.texture.format, internal_desc.Format))
				break;

			if ((desc.flags & api::resource_flags::cube_compatible) != api::resource_flags::cube_compatible)
			{
				if (com_ptr<IDirect3DTexture9> object;
					SUCCEEDED(_orig->CreateTexture(internal_desc.Width, internal_desc.Height, levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, &object, nullptr)))
				{
					*out = { reinterpret_cast<uintptr_t>(object.release()) };

					if (initial_data != nullptr)
					{
						// Cannot get level information for textures created with the 'generate_mipmaps' flag, so only upload to the base level
						if (levels == 0)
							levels  = 1;

						for (uint32_t subresource = 0; subresource < levels; ++subresource)
							update_texture_region(initial_data[subresource], *out, subresource, nullptr);
					}
					return true;
				}
			}
			else
			{
				assert(internal_desc.Width == internal_desc.Height);

				if (com_ptr<IDirect3DCubeTexture9> object;
					SUCCEEDED(_orig->CreateCubeTexture(internal_desc.Width, levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, &object, nullptr)))
				{
					*out = { reinterpret_cast<uintptr_t>(object.release()) };

					if (initial_data != nullptr)
					{
						for (uint32_t subresource = 0; subresource < static_cast<uint32_t>(desc.texture.depth_or_layers) * desc.texture.levels; ++subresource)
							update_texture_region(initial_data[subresource], *out, subresource, nullptr);
					}
					return true;
				}
			}
			break;
		}
		case api::resource_type::texture_3d:
		{
			// 3D textures can never have multisampling
			if (desc.texture.samples != 1)
				break;

			UINT levels = 0;
			D3DVOLUME_DESC internal_desc = {};
			convert_resource_desc(desc, internal_desc, &levels, _caps);

			if (!convert_format_internal(desc.texture.format, internal_desc.Format))
				break;

			if (com_ptr<IDirect3DVolumeTexture9> object;
				SUCCEEDED(_orig->CreateVolumeTexture(internal_desc.Width, internal_desc.Height, internal_desc.Depth, levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, &object, nullptr)))
			{
				*out = { reinterpret_cast<uintptr_t>(object.release()) };

				if (initial_data != nullptr)
				{
					for (uint32_t subresource = 0; subresource < static_cast<uint32_t>(desc.texture.depth_or_layers) * desc.texture.levels; ++subresource)
						update_texture_region(initial_data[subresource], *out, subresource, nullptr);
				}

				return true;
			}
			break;
		}
		case api::resource_type::surface:
		{
			// Surfaces can never be arrays or have mipmap levels
			if (desc.texture.depth_or_layers != 1 || desc.texture.levels != 1)
				break;

			assert(initial_data == nullptr);

			switch (desc.usage & (api::resource_usage::depth_stencil | api::resource_usage::render_target))
			{
				case api::resource_usage::depth_stencil:
				case api::resource_usage::depth_stencil_read:
				case api::resource_usage::depth_stencil_write:
				{
					D3DSURFACE_DESC internal_desc = {};
					convert_resource_desc(desc, internal_desc, nullptr, _caps);
					assert(internal_desc.Usage == D3DUSAGE_DEPTHSTENCIL);

					if (com_ptr<IDirect3DSurface9> object;
						((desc.usage & reshade::api::resource_usage::shader_resource) != reshade::api::resource_usage::undefined) ?
						SUCCEEDED(create_surface_replacement(internal_desc, &object, nullptr)) :
						SUCCEEDED(_orig->CreateDepthStencilSurface(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.MultiSampleType, internal_desc.MultiSampleQuality, FALSE, &object, nullptr)))
					{
						*out = { reinterpret_cast<uintptr_t>(object.release()) };
						return true;
					}
					break;
				}
				case api::resource_usage::render_target:
				{
					D3DSURFACE_DESC internal_desc = {};
					convert_resource_desc(desc, internal_desc, nullptr, _caps);
					assert(internal_desc.Usage == D3DUSAGE_RENDERTARGET);

					if (com_ptr<IDirect3DSurface9> object;
						((desc.usage & reshade::api::resource_usage::shader_resource) != reshade::api::resource_usage::undefined) ?
						SUCCEEDED(create_surface_replacement(internal_desc, &object, nullptr)) :
						SUCCEEDED(_orig->CreateRenderTarget(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.MultiSampleType, internal_desc.MultiSampleQuality, FALSE, &object, nullptr)))
					{
						*out = { reinterpret_cast<uintptr_t>(object.release()) };
						return true;
					}
					break;
				}
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}
void reshade::d3d9::device_impl::destroy_resource(api::resource handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

bool reshade::d3d9::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out)
{
	if (resource.handle == 0)
	{
		*out = { 0 };
		return false;
	}

	auto object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	// Set the first bit in the handle to indicate whether this view is using a sRGB format
	// This requires the resource pointers to be at least 2-byte aligned, to ensure the first bit is always unused otherwise
	static_assert(alignof(IDirect3DResource9) >= 2);
	const uint64_t set_srgb_bit =
		desc.format == api::format::r8g8b8a8_unorm_srgb || desc.format == api::format::r8g8b8x8_unorm_srgb ||
		desc.format == api::format::b8g8r8a8_unorm_srgb || desc.format == api::format::b8g8r8x8_unorm_srgb ||
		desc.format == api::format::bc1_unorm_srgb || desc.format == api::format::bc2_unorm_srgb || desc.format == api::format::bc3_unorm_srgb;

	switch (object->GetType())
	{
		case D3DRTYPE_SURFACE:
		{
			assert(desc.type == api::resource_view_type::texture_2d || desc.type == api::resource_view_type::texture_2d_multisample);
			assert(desc.texture.first_layer == 0 && (desc.texture.layers == 1 || desc.texture.layers == std::numeric_limits<uint32_t>::max()));

			if (usage_type == api::resource_usage::depth_stencil || usage_type == api::resource_usage::render_target)
			{
				if (desc.texture.first_level != 0 || desc.texture.levels != 1)
					break;

				D3DSURFACE_DESC internal_desc;
				static_cast<IDirect3DSurface9 *>(object)->GetDesc(&internal_desc);
				if (D3DFORMAT view_format = convert_format(desc.format);
					!convert_format_internal(desc.format, view_format) ||
					internal_desc.Format != view_format)
					break;

				object->AddRef();
				{
					*out = { reinterpret_cast<uintptr_t>(object) | set_srgb_bit };
					return true;
				}
			}
			break;
		}
		case D3DRTYPE_TEXTURE:
		{
			assert(desc.type == api::resource_view_type::texture_2d || desc.type == api::resource_view_type::texture_2d_multisample);
			assert(desc.texture.first_layer == 0 && (desc.texture.layers == 1 || desc.texture.layers == std::numeric_limits<uint32_t>::max()));

			if (usage_type == api::resource_usage::depth_stencil || usage_type == api::resource_usage::render_target)
			{
				if (desc.texture.levels != 1)
					break;

				D3DSURFACE_DESC internal_desc;
				static_cast<IDirect3DTexture9 *>(object)->GetLevelDesc(desc.texture.first_level, &internal_desc);
				if (D3DFORMAT view_format = convert_format(desc.format);
					!convert_format_internal(desc.format, view_format) ||
					internal_desc.Format != view_format)
					break;

				if (com_ptr<IDirect3DSurface9> surface;
					SUCCEEDED(static_cast<IDirect3DTexture9 *>(object)->GetSurfaceLevel(desc.texture.first_level, &surface)))
				{
					*out = { reinterpret_cast<uintptr_t>(surface.release()) | set_srgb_bit };
					return true;
				}
			}
			else if (usage_type == api::resource_usage::shader_resource && desc.texture.first_level == 0)
			{
				D3DSURFACE_DESC internal_desc;
				static_cast<IDirect3DTexture9 *>(object)->GetLevelDesc(0, &internal_desc);
				if (D3DFORMAT view_format = convert_format(desc.format);
					!convert_format_internal(desc.format, view_format) ||
					internal_desc.Format != view_format)
					break;

				object->AddRef();
				{
					*out = { reinterpret_cast<uintptr_t>(object) | set_srgb_bit };
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

				if (desc.texture.levels != 1 || desc.texture.layers != 1)
					break;

				D3DSURFACE_DESC internal_desc;
				static_cast<IDirect3DCubeTexture9 *>(object)->GetLevelDesc(desc.texture.first_level, &internal_desc);
				if (D3DFORMAT view_format = convert_format(desc.format);
					!convert_format_internal(desc.format, view_format) ||
					internal_desc.Format != view_format)
					break;

				if (com_ptr<IDirect3DSurface9> surface;
					SUCCEEDED(static_cast<IDirect3DCubeTexture9 *>(object)->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(desc.texture.first_layer), desc.texture.first_level, &surface)))
				{
					*out = { reinterpret_cast<uintptr_t>(surface.release()) | set_srgb_bit };
					return true;
				}
			}
			else if (usage_type == api::resource_usage::shader_resource && desc.texture.first_level == 0 && desc.texture.first_layer == 0)
			{
				assert(desc.type == api::resource_view_type::texture_cube);

				D3DSURFACE_DESC internal_desc;
				static_cast<IDirect3DCubeTexture9 *>(object)->GetLevelDesc(0, &internal_desc);
				if (D3DFORMAT view_format = convert_format(desc.format);
					!convert_format_internal(desc.format, view_format) ||
					internal_desc.Format != view_format)
					break;

				object->AddRef();
				{
					*out = { reinterpret_cast<uintptr_t>(object) | set_srgb_bit };
					return true;
				}
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}
void reshade::d3d9::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle & ~1ull)->Release();
}

bool reshade::d3d9::device_impl::create_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	switch (desc.type)
	{
	case api::pipeline_stage::all_graphics:
		return create_graphics_pipeline(desc, out);
	case api::pipeline_stage::input_assembler:
		return create_input_layout(desc, out);
	case api::pipeline_stage::vertex_shader:
		return create_vertex_shader(desc, out);
	case api::pipeline_stage::pixel_shader:
		return create_pixel_shader(desc, out);
	default:
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d9::device_impl::create_graphics_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	// Check for unsupported states
	if (desc.graphics.hull_shader.code_size != 0 ||
		desc.graphics.domain_shader.code_size != 0 ||
		desc.graphics.geometry_shader.code_size != 0 ||
		desc.graphics.blend_state.alpha_to_coverage_enable ||
		desc.graphics.blend_state.logic_op_enable[0] ||
		desc.graphics.topology > api::primitive_topology::triangle_fan ||
		desc.graphics.viewport_count > 1)
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

	create_state_object(vertex_shader, IDirect3DVertexShader9, desc.graphics.vertex_shader.code_size != 0);
	create_state_object(pixel_shader, IDirect3DPixelShader9, desc.graphics.pixel_shader.code_size != 0);

	create_state_object(input_layout, IDirect3DVertexDeclaration9, true);
#undef create_state_object

	if (FAILED(_orig->BeginStateBlock()))
	{
		*out = { 0 };
		return false;
	}

	_orig->SetVertexShader(vertex_shader.get());
	_orig->SetPixelShader(pixel_shader.get());

	if (input_layout != nullptr)
	{
		_orig->SetVertexDeclaration(input_layout.get());
	}
	else
	{
		// Setup default input (used to have a vertex ID in vertex shaders)
		_orig->SetStreamSource(0, _default_input_stream.get(), 0, sizeof(float));
		_orig->SetVertexDeclaration(_default_input_layout.get());
	}

	_orig->SetRenderState(D3DRS_ZENABLE,
		desc.graphics.depth_stencil_state.depth_enable);
	_orig->SetRenderState(D3DRS_FILLMODE,
		convert_fill_mode(desc.graphics.rasterizer_state.fill_mode));
	_orig->SetRenderState(D3DRS_ZWRITEENABLE,
		desc.graphics.depth_stencil_state.depth_write_mask);
	_orig->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	_orig->SetRenderState(D3DRS_LASTPIXEL, TRUE);
	_orig->SetRenderState(D3DRS_SRCBLEND,
		convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[0]));
	_orig->SetRenderState(D3DRS_DESTBLEND,
		convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[0]));
	_orig->SetRenderState(D3DRS_CULLMODE,
		convert_cull_mode(desc.graphics.rasterizer_state.cull_mode, desc.graphics.rasterizer_state.front_counter_clockwise));
	_orig->SetRenderState(D3DRS_ZFUNC,
		convert_compare_op(desc.graphics.depth_stencil_state.depth_func));
	_orig->SetRenderState(D3DRS_DITHERENABLE, FALSE);
	_orig->SetRenderState(D3DRS_ALPHABLENDENABLE,
		desc.graphics.blend_state.blend_enable[0]);
	_orig->SetRenderState(D3DRS_FOGENABLE, FALSE);
	_orig->SetRenderState(D3DRS_STENCILENABLE,
		desc.graphics.depth_stencil_state.stencil_enable);
	_orig->SetRenderState(D3DRS_STENCILZFAIL,
		convert_stencil_op(desc.graphics.rasterizer_state.front_counter_clockwise ? desc.graphics.depth_stencil_state.back_stencil_depth_fail_op : desc.graphics.depth_stencil_state.front_stencil_depth_fail_op));
	_orig->SetRenderState(D3DRS_STENCILFAIL,
		convert_stencil_op(desc.graphics.rasterizer_state.front_counter_clockwise ? desc.graphics.depth_stencil_state.back_stencil_fail_op : desc.graphics.depth_stencil_state.front_stencil_fail_op));
	_orig->SetRenderState(D3DRS_STENCILPASS,
		convert_stencil_op(desc.graphics.rasterizer_state.front_counter_clockwise ? desc.graphics.depth_stencil_state.back_stencil_pass_op : desc.graphics.depth_stencil_state.front_stencil_pass_op));
	_orig->SetRenderState(D3DRS_STENCILFUNC,
		convert_compare_op(desc.graphics.rasterizer_state.front_counter_clockwise ? desc.graphics.depth_stencil_state.back_stencil_func : desc.graphics.depth_stencil_state.front_stencil_func));
	_orig->SetRenderState(D3DRS_STENCILREF,
		desc.graphics.depth_stencil_state.stencil_reference_value);
	_orig->SetRenderState(D3DRS_STENCILMASK,
		desc.graphics.depth_stencil_state.stencil_read_mask);
	_orig->SetRenderState(D3DRS_STENCILWRITEMASK,
		desc.graphics.depth_stencil_state.stencil_write_mask);
	_orig->SetRenderState(D3DRS_CLIPPING,
		desc.graphics.rasterizer_state.depth_clip_enable);
	_orig->SetRenderState(D3DRS_LIGHTING, FALSE);
	_orig->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
	_orig->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
	_orig->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,
		desc.graphics.rasterizer_state.multisample_enable);
	_orig->SetRenderState(D3DRS_MULTISAMPLEMASK,
		desc.graphics.sample_mask);
	_orig->SetRenderState(D3DRS_COLORWRITEENABLE,
		desc.graphics.blend_state.render_target_write_mask[0]);
	_orig->SetRenderState(D3DRS_BLENDOP,
		convert_blend_op(desc.graphics.blend_state.color_blend_op[0]));
	_orig->SetRenderState(D3DRS_SCISSORTESTENABLE,
		desc.graphics.rasterizer_state.scissor_enable);
	_orig->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,
		*reinterpret_cast<const DWORD *>(&desc.graphics.rasterizer_state.slope_scaled_depth_bias));
	_orig->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE,
		desc.graphics.rasterizer_state.antialiased_line_enable);
	_orig->SetRenderState(D3DRS_ENABLEADAPTIVETESSELLATION, FALSE);
	_orig->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE,
		desc.graphics.rasterizer_state.cull_mode == api::cull_mode::none && (
		desc.graphics.depth_stencil_state.front_stencil_fail_op != desc.graphics.depth_stencil_state.back_stencil_fail_op ||
		desc.graphics.depth_stencil_state.front_stencil_depth_fail_op != desc.graphics.depth_stencil_state.back_stencil_depth_fail_op ||
		desc.graphics.depth_stencil_state.front_stencil_pass_op != desc.graphics.depth_stencil_state.back_stencil_pass_op ||
		desc.graphics.depth_stencil_state.front_stencil_func != desc.graphics.depth_stencil_state.back_stencil_func));
	_orig->SetRenderState(D3DRS_CCW_STENCILZFAIL,
		convert_stencil_op(desc.graphics.rasterizer_state.front_counter_clockwise ? desc.graphics.depth_stencil_state.front_stencil_depth_fail_op : desc.graphics.depth_stencil_state.back_stencil_depth_fail_op));
	_orig->SetRenderState(D3DRS_CCW_STENCILFAIL,
		convert_stencil_op(desc.graphics.rasterizer_state.front_counter_clockwise ? desc.graphics.depth_stencil_state.front_stencil_fail_op : desc.graphics.depth_stencil_state.back_stencil_fail_op));
	_orig->SetRenderState(D3DRS_CCW_STENCILPASS,
		convert_stencil_op(desc.graphics.rasterizer_state.front_counter_clockwise ? desc.graphics.depth_stencil_state.front_stencil_pass_op : desc.graphics.depth_stencil_state.back_stencil_pass_op));
	_orig->SetRenderState(D3DRS_CCW_STENCILFUNC,
		convert_compare_op(desc.graphics.rasterizer_state.front_counter_clockwise ? desc.graphics.depth_stencil_state.front_stencil_func : desc.graphics.depth_stencil_state.back_stencil_func));
	_orig->SetRenderState(D3DRS_COLORWRITEENABLE1,
		desc.graphics.blend_state.render_target_write_mask[1]);
	_orig->SetRenderState(D3DRS_COLORWRITEENABLE2,
		desc.graphics.blend_state.render_target_write_mask[2]);
	_orig->SetRenderState(D3DRS_COLORWRITEENABLE3,
		desc.graphics.blend_state.render_target_write_mask[3]);
	_orig->SetRenderState(D3DRS_BLENDFACTOR,
		desc.graphics.blend_state.blend_constant);
	_orig->SetRenderState(D3DRS_DEPTHBIAS,
		*reinterpret_cast<const DWORD *>(&desc.graphics.rasterizer_state.depth_bias));
	_orig->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
	_orig->SetRenderState(D3DRS_SRCBLENDALPHA,
		convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[0]));
	_orig->SetRenderState(D3DRS_DESTBLENDALPHA,
		convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[0]));
	_orig->SetRenderState(D3DRS_BLENDOPALPHA,
		convert_blend_op(desc.graphics.blend_state.alpha_blend_op[0]));

	if (com_ptr<IDirect3DStateBlock9> state_block;
		SUCCEEDED(_orig->EndStateBlock(&state_block)))
	{
		const auto impl = new pipeline_impl();
		impl->prim_type = static_cast<D3DPRIMITIVETYPE>(desc.graphics.topology);
		impl->state_block = std::move(state_block);

		*out = { reinterpret_cast<uintptr_t>(impl) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d9::device_impl::create_input_layout(const api::pipeline_desc &desc, api::pipeline *out)
{
	std::vector<D3DVERTEXELEMENT9> internal_elements;
	convert_pipeline_desc(desc, internal_elements);

	if (com_ptr<IDirect3DVertexDeclaration9> object;
		internal_elements.size() == 1 || SUCCEEDED(_orig->CreateVertexDeclaration(internal_elements.data(), &object)))
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
bool reshade::d3d9::device_impl::create_vertex_shader(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (com_ptr<IDirect3DVertexShader9> object;
		SUCCEEDED(_orig->CreateVertexShader(static_cast<const DWORD *>(desc.graphics.vertex_shader.code), &object)))
	{
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
bool reshade::d3d9::device_impl::create_pixel_shader(const api::pipeline_desc &desc, api::pipeline *out)
{
	if (com_ptr<IDirect3DPixelShader9> object;
		SUCCEEDED(_orig->CreatePixelShader(static_cast<const DWORD *>(desc.graphics.pixel_shader.code), &object)))
	{
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
void reshade::d3d9::device_impl::destroy_pipeline(api::pipeline_stage type, api::pipeline handle)
{
	if (type == api::pipeline_stage::all_graphics)
		delete reinterpret_cast<pipeline_impl *>(handle.handle);
	else if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

bool reshade::d3d9::device_impl::create_pipeline_layout(uint32_t count, const api::pipeline_layout_param *params, api::pipeline_layout *out)
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
			impl->shader_registers[i] = params[i].push_constants.offset / 4;
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
void reshade::d3d9::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	delete reinterpret_cast<pipeline_layout_impl *>(handle.handle);
}

bool reshade::d3d9::device_impl::create_descriptor_set_layout(uint32_t count, const api::descriptor_range *bindings, bool, api::descriptor_set_layout *out)
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
void reshade::d3d9::device_impl::destroy_descriptor_set_layout(api::descriptor_set_layout handle)
{
	delete reinterpret_cast<descriptor_set_layout_impl *>(handle.handle);
}

bool reshade::d3d9::device_impl::create_query_pool(api::query_type type, uint32_t size, api::query_pool *out)
{
	const auto impl = new query_pool_impl();
	impl->type = type;
	impl->queries.resize(size);

	const D3DQUERYTYPE internal_type = convert_query_type(type);

	for (UINT i = 0; i < size; ++i)
	{
		if (FAILED(_orig->CreateQuery(internal_type, &impl->queries[i])))
		{
			delete impl;

			*out = { 0 };
			return false;
		}
	}

	*out = { reinterpret_cast<uintptr_t>(impl) };
	return true;
}
void reshade::d3d9::device_impl::destroy_query_pool(api::query_pool handle)
{
	delete reinterpret_cast<query_pool_impl *>(handle.handle);
}

bool reshade::d3d9::device_impl::create_render_pass(const api::render_pass_desc &, api::render_pass *out)
{
	*out = { 0 };
	return true;
}
void reshade::d3d9::device_impl::destroy_render_pass(api::render_pass handle)
{
	assert(handle.handle == 0);
}

bool reshade::d3d9::device_impl::create_framebuffer(const api::framebuffer_desc &desc, api::framebuffer *out)
{
	const auto impl = new framebuffer_impl();

	for (DWORD i = 0; i < 8 && desc.render_targets[i].handle != 0; ++i)
	{
		if (i >= _caps.NumSimultaneousRTs)
		{
			delete impl;

			*out = { 0 };
			return false;
		}

		impl->rtv[i] = reinterpret_cast<IDirect3DSurface9 *>(desc.render_targets[i].handle & ~1ull);
		impl->srgb_write_enable |= (desc.render_targets[i].handle & 1);
	}

	impl->dsv = reinterpret_cast<IDirect3DSurface9 *>(desc.depth_stencil.handle);

	*out = { reinterpret_cast<uintptr_t>(impl) };
	return true;
}
void reshade::d3d9::device_impl::destroy_framebuffer(api::framebuffer handle)
{
	delete reinterpret_cast<framebuffer_impl *>(handle.handle);
}

bool reshade::d3d9::device_impl::create_descriptor_sets(uint32_t count, const api::descriptor_set_layout *layouts, api::descriptor_set *out)
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
		case api::descriptor_type::sampler_with_resource_view:
			impl->descriptors.resize(impl->count * 2);
			break;
		default:
			assert(false);
			break;
		}

		out[i] = { reinterpret_cast<uintptr_t>(impl) };
	}

	return true;
}
void reshade::d3d9::device_impl::destroy_descriptor_sets(uint32_t count, const api::descriptor_set *sets)
{
	for (uint32_t i = 0; i < count; ++i)
		delete reinterpret_cast<descriptor_set_impl *>(sets[i].handle);
}

bool reshade::d3d9::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access access, api::subresource_data *out_data)
{
	DWORD flags = 0;
	switch (access)
	{
	case api::map_access::read_only:
		flags = D3DLOCK_READONLY;
		break;
	case api::map_access::write_discard:
		flags = D3DLOCK_DISCARD;
		break;
	}

	assert(out_data != nullptr);
	out_data->data = nullptr;
	out_data->row_pitch = 0;
	out_data->slice_pitch = 0;

	assert(resource.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	switch (object->GetType())
	{
		case D3DRTYPE_SURFACE:
		{
			assert(subresource == 0);

			if (D3DLOCKED_RECT locked_rect;
				SUCCEEDED(static_cast<IDirect3DSurface9 *>(object)->LockRect(&locked_rect, nullptr, flags)))
			{
				out_data->data = locked_rect.pBits;
				out_data->row_pitch = locked_rect.Pitch;
				return true;
			}
			break;
		}
		case D3DRTYPE_TEXTURE:
		{
			if (D3DLOCKED_RECT locked_rect;
				SUCCEEDED(static_cast<IDirect3DTexture9 *>(object)->LockRect(subresource, &locked_rect, nullptr, flags)))
			{
				out_data->data = locked_rect.pBits;
				out_data->row_pitch = locked_rect.Pitch;
				return true;
			}
			break;
		}
		case D3DRTYPE_VOLUMETEXTURE:
		{
			if (D3DLOCKED_BOX locked_box;
				SUCCEEDED(static_cast<IDirect3DVolumeTexture9 *>(object)->LockBox(subresource, &locked_box, nullptr, flags)))
			{
				out_data->data = locked_box.pBits;
				out_data->row_pitch = locked_box.RowPitch;
				out_data->slice_pitch = locked_box.SlicePitch;
				return true;
			}
			break;
		}
		case D3DRTYPE_CUBETEXTURE:
		{
			const UINT levels = static_cast<IDirect3DCubeTexture9 *>(object)->GetLevelCount();

			if (D3DLOCKED_RECT locked_rect;
				SUCCEEDED(static_cast<IDirect3DCubeTexture9 *>(object)->LockRect(static_cast<D3DCUBEMAP_FACES>(subresource / levels), subresource % levels, &locked_rect, nullptr, flags)))
			{
				out_data->data = locked_rect.pBits;
				out_data->row_pitch = locked_rect.Pitch;
				return true;
			}
			break;
		}
		case D3DRTYPE_VERTEXBUFFER:
		{
			assert(subresource == 0);

			return SUCCEEDED(static_cast<IDirect3DVertexBuffer9 *>(object)->Lock(0, 0, &out_data->data, flags));
		}
		case D3DRTYPE_INDEXBUFFER:
		{
			assert(subresource == 0);

			return SUCCEEDED(static_cast<IDirect3DIndexBuffer9 *>(object)->Lock(0, 0, &out_data->data, flags));
		}
	}

	return false;
}
void reshade::d3d9::device_impl::unmap_resource(api::resource resource, uint32_t subresource)
{
	assert(resource.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	switch (object->GetType())
	{
		case D3DRTYPE_SURFACE:
		{
			assert(subresource == 0);

			static_cast<IDirect3DSurface9 *>(object)->UnlockRect();
			break;
		}
		case D3DRTYPE_TEXTURE:
		{
			static_cast<IDirect3DTexture9 *>(object)->UnlockRect(subresource);
			break;
		}
		case D3DRTYPE_VOLUMETEXTURE:
		{
			static_cast<IDirect3DVolumeTexture9 *>(object)->UnlockBox(subresource);
			break;
		}
		case D3DRTYPE_CUBETEXTURE:
		{
			const UINT levels = static_cast<IDirect3DCubeTexture9 *>(object)->GetLevelCount();

			static_cast<IDirect3DCubeTexture9 *>(object)->UnlockRect(static_cast<D3DCUBEMAP_FACES>(subresource / levels), subresource % levels);
			break;
		}
		case D3DRTYPE_VERTEXBUFFER:
		{
			assert(subresource == 0);

			static_cast<IDirect3DVertexBuffer9 *>(object)->Unlock();
			break;
		}
		case D3DRTYPE_INDEXBUFFER:
		{
			assert(subresource == 0);

			static_cast<IDirect3DIndexBuffer9 *>(object)->Unlock();
			break;
		}
	}
}

void reshade::d3d9::device_impl::update_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(dst.handle != 0);
	assert(dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());
	auto object = reinterpret_cast<IDirect3DResource9 *>(dst.handle);

	switch (object->GetType())
	{
		case D3DRTYPE_VERTEXBUFFER:
		{
			if (void *mapped_ptr = nullptr;
				SUCCEEDED(static_cast<IDirect3DVertexBuffer9 *>(object)->Lock(static_cast<UINT>(dst_offset), static_cast<UINT>(size), &mapped_ptr, 0)))
			{
				std::memcpy(mapped_ptr, data, static_cast<size_t>(size));
				static_cast<IDirect3DVertexBuffer9 *>(object)->Unlock();
			}
			return;
		}
		case D3DRTYPE_INDEXBUFFER:
		{
			if (void *mapped_ptr = nullptr;
				SUCCEEDED(static_cast<IDirect3DIndexBuffer9 *>(object)->Lock(static_cast<UINT>(dst_offset), static_cast<UINT>(size), &mapped_ptr, 0)))
			{
				std::memcpy(mapped_ptr, data, static_cast<size_t>(size));
				static_cast<IDirect3DIndexBuffer9 *>(object)->Unlock();
			}
			return;
		}
	}

	assert(false); // Not implemented
}
void reshade::d3d9::device_impl::update_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	assert(dst.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(dst.handle);

	switch (object->GetType())
	{
		case D3DRTYPE_TEXTURE:
		{
			D3DSURFACE_DESC desc = {}; // Get D3D texture format
			static_cast<IDirect3DTexture9 *>(object)->GetLevelDesc(dst_subresource, &desc);

			UINT width = dst_box != nullptr ? dst_box[3] - dst_box[0] : desc.Width;
			UINT height = dst_box != nullptr ? dst_box[4] - dst_box[1] : desc.Height;
			const bool use_systemmem_texture = static_cast<IDirect3DTexture9 *>(object)->GetLevelCount() == 1 && dst_box == nullptr;

			com_ptr<IDirect3DTexture9> intermediate;
			if (FAILED(_orig->CreateTexture(width, height, 1, use_systemmem_texture ? 0 : D3DUSAGE_DYNAMIC, desc.Format, use_systemmem_texture ? D3DPOOL_SYSTEMMEM : D3DPOOL_DEFAULT, &intermediate, nullptr)))
			{
				LOG(ERROR) << "Failed to create upload buffer!";
				LOG(DEBUG) << "> Details: Width = " << width << ", Height = " << height << ", Levels = " << "1" << ", Usage = " << (use_systemmem_texture ? "0" : "D3DUSAGE_DYNAMIC") << ", Format = " << desc.Format;
				return;
			}

			D3DLOCKED_RECT locked_rect;
			if (FAILED(intermediate->LockRect(0, &locked_rect, nullptr, 0)))
				return;
			auto mapped_data = static_cast<uint8_t *>(locked_rect.pBits);
			auto upload_data = static_cast<const uint8_t *>(data.data);

			// If format is one of these two, assume they were overwritten by 'convert_format_internal', so handle them accordingly
			// TODO: Maybe store the original format as user data in the resource to avoid this hack
			if (desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_X8R8G8B8)
			{
				for (uint32_t y = 0; y < height; ++y, mapped_data += locked_rect.Pitch, upload_data += data.row_pitch)
				{
					switch (data.row_pitch / width)
					{
					case 1: // This is likely actually a r8 texture
						for (uint32_t x = 0, i = 0; x < width * 4; x += 4, i += 1)
							mapped_data[x + 0] = 0, // Set green and blue channel to zero
							mapped_data[x + 1] = 0,
							mapped_data[x + 2] = upload_data[i],
							mapped_data[x + 3] = 0xFF;
						break;
					case 2: // This is likely actually a r8g8 texture
						for (uint32_t x = 0, i = 0; x < width * 4; x += 4, i += 2)
							mapped_data[x + 0] = 0, // Set blue channel to zero
							mapped_data[x + 1] = upload_data[i + 1],
							mapped_data[x + 2] = upload_data[i + 0],
							mapped_data[x + 3] = 0xFF;
						break;
					case 4: // This is likely actually a r8g8b8a8 texture
					default:
						for (uint32_t x = 0, i = 0; x < width * 4; x += 4, i += 4)
							mapped_data[x + 0] = upload_data[i + 2], // Flip RGBA input to BGRA
							mapped_data[x + 1] = upload_data[i + 1],
							mapped_data[x + 2] = upload_data[i + 0],
							mapped_data[x + 3] = upload_data[i + 3];
						break;
					}
				}
			}
			else
			{
				for (uint32_t y = 0; y < height; ++y, mapped_data += locked_rect.Pitch, upload_data += data.row_pitch)
				{
					std::memcpy(mapped_data, upload_data, std::min(data.row_pitch, static_cast<uint32_t>(locked_rect.Pitch)));
				}
			}

			intermediate->UnlockRect(0);

			if (use_systemmem_texture)
			{
				assert(dst_subresource == 0);

				_orig->UpdateTexture(intermediate.get(), static_cast<IDirect3DTexture9 *>(object));
			}
			else
			{
				RECT dst_rect;
				if (dst_box != nullptr)
				{
					dst_rect.left = dst_box[0];
					dst_rect.top = dst_box[1];
					assert(dst_box[2] == 0);
					dst_rect.right = dst_box[3];
					dst_rect.bottom = dst_box[4];
					assert(dst_box[5] == 1);
				}

				com_ptr<IDirect3DSurface9> src_surface;
				intermediate->GetSurfaceLevel(0, &src_surface);
				com_ptr<IDirect3DSurface9> dst_surface;
				static_cast<IDirect3DTexture9 *>(object)->GetSurfaceLevel(dst_subresource, &dst_surface);

				_orig->StretchRect(src_surface.get(), nullptr, dst_surface.get(), dst_box != nullptr ? &dst_rect : nullptr, D3DTEXF_NONE);
			}
			return;
		}
	}

	assert(false); // Not implemented
}

void reshade::d3d9::device_impl::update_descriptor_sets(uint32_t count, const api::descriptor_set_update *updates)
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
			std::memcpy(&impl->descriptors[update.offset * 2], update.descriptors, update.count * sizeof(uint64_t) * 2);
			break;
		case api::descriptor_type::unordered_access_view:
		case api::descriptor_type::constant_buffer:
			assert(false);
			break;
		}
	}
}

bool reshade::d3d9::device_impl::get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(pool.handle != 0);
	const auto impl = reinterpret_cast<query_pool_impl *>(pool.handle);

	for (uint32_t i = 0; i < count; ++i)
	{
		if (impl->queries[first + i]->GetData(static_cast<uint8_t *>(results) + i * stride, stride, 0) != S_OK)
			return false;

		if (impl->type == api::query_type::timestamp)
		{
			assert(stride >= sizeof(uint64_t));
			// D3D9 timestamp queries seem to always have a resolution of 10ns (D3DQUERYTYPE_TIMESTAMPFREQ returns 100000000), so convert that to nanoseconds for consistency with other APIs
			*reinterpret_cast<uint64_t *>(static_cast<uint8_t *>(results) + i * stride) *= 10;
		}
	}

	return true;
}

void reshade::d3d9::device_impl::get_pipeline_layout_desc(api::pipeline_layout layout, uint32_t *count, api::pipeline_layout_param *params) const
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
void reshade::d3d9::device_impl::get_descriptor_pool_offset(api::descriptor_set, api::descriptor_pool *pool, uint32_t *offset) const
{
	*pool = { 0 };
	*offset = 0; // Unsupported
}
void reshade::d3d9::device_impl::get_descriptor_set_layout_desc(api::descriptor_set_layout layout, uint32_t *count, api::descriptor_range *bindings) const
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

reshade::api::resource_desc reshade::d3d9::device_impl::get_resource_desc(api::resource resource) const
{
	assert(resource.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	switch (object->GetType())
	{
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

	assert(false); // Not implemented
	return api::resource_desc {};
}
reshade::api::resource      reshade::d3d9::device_impl::get_resource_from_view(api::resource_view view) const
{
	assert(view.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(view.handle & ~1ull);

	if (com_ptr<IDirect3DSurface9> surface;
		SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&surface))))
	{
		if (com_ptr<IDirect3DResource9> resource;
			SUCCEEDED(surface->GetContainer(IID_PPV_ARGS(&resource))))
		{
			return { reinterpret_cast<uintptr_t>(resource.get()) };
		}
	}

	// If unable to get container, just return the resource directly
	return { reinterpret_cast<uintptr_t>(object) };
}
reshade::api::resource      reshade::d3d9::device_impl::get_resource_from_view(api::resource_view view, uint32_t *subresource) const
{
	auto object = reinterpret_cast<IDirect3DResource9 *>(get_resource_from_view(view).handle);
	auto view_surface = reinterpret_cast<IDirect3DSurface9 *>(view.handle & ~1ull);

	switch (object->GetType())
	{
		case D3DRTYPE_TEXTURE:
		{
			const auto texture = static_cast<IDirect3DTexture9 *>(object);

			for (DWORD level = 0, levels = texture->GetLevelCount(); level < levels; ++level)
			{
				com_ptr<IDirect3DSurface9> surface;
				if (SUCCEEDED(texture->GetSurfaceLevel(level, &surface)) && surface == view_surface)
				{
					*subresource = level;
					return { reinterpret_cast<uint64_t>(object) };
				}
			}
			break;
		}
		case D3DRTYPE_CUBETEXTURE:
		{
			const auto texture = static_cast<IDirect3DCubeTexture9 *>(object);

			for (DWORD level = 0, levels = texture->GetLevelCount(); level < levels; ++level)
			{
				for (D3DCUBEMAP_FACES face = D3DCUBEMAP_FACE_POSITIVE_X; face <= D3DCUBEMAP_FACE_NEGATIVE_Z; face = static_cast<D3DCUBEMAP_FACES>(face + 1))
				{
					com_ptr<IDirect3DSurface9> surface;
					if (SUCCEEDED(texture->GetCubeMapSurface(face, level, &surface)) && surface == view_surface)
					{
						*subresource = level;
						return { reinterpret_cast<uint64_t>(object) };
					}
				}
			}
			break;
		}
	}

	*subresource = 0;
	return { reinterpret_cast<uint64_t>(object) };
}

reshade::api::resource_view reshade::d3d9::device_impl::get_framebuffer_attachment(api::framebuffer fbo, api::attachment_type type, uint32_t index) const
{
	assert(fbo.handle != 0);
	const auto fbo_impl = reinterpret_cast<const framebuffer_impl *>(fbo.handle);

	if (type == api::attachment_type::color)
	{
		if (index < _caps.NumSimultaneousRTs && fbo_impl->rtv[index] != nullptr)
		{
			const uint64_t set_srgb_bit = fbo_impl->srgb_write_enable;

			return { reinterpret_cast<uintptr_t>(fbo_impl->rtv[index]) | set_srgb_bit };
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

HRESULT reshade::d3d9::device_impl::create_surface_replacement(const D3DSURFACE_DESC &new_desc, IDirect3DSurface9 **out_surface, HANDLE *out_shared_handle) const
{
	// Cannot create multisampled textures
	if (new_desc.MultiSampleType != D3DMULTISAMPLE_NONE)
		return D3DERR_INVALIDCALL;

	// Surface will hold a reference to the created texture and keep it alive
	com_ptr<IDirect3DTexture9> texture;

	HRESULT hr = _orig->CreateTexture(new_desc.Width, new_desc.Height, 1, new_desc.Usage, new_desc.Format, new_desc.Pool, &texture, out_shared_handle);
	if (SUCCEEDED(hr))
		hr = texture->GetSurfaceLevel(0, out_surface);
	return hr;
}

#if RESHADE_ADDON
reshade::api::sampler reshade::d3d9::device_impl::get_current_sampler_state(DWORD slot)
{
	// Get current sampler state
	DWORD state_data[12];
	for (D3DSAMPLERSTATETYPE state = D3DSAMP_ADDRESSU; state <= D3DSAMP_SRGBTEXTURE; state = static_cast<D3DSAMPLERSTATETYPE>(state + 1))
		_orig->GetSamplerState(slot, state, &state_data[state]);

	// Generate hash for sampler state description
	size_t state_hash = 2166136261;
	for (int i = 0; i < 12; ++i)
		state_hash = (state_hash * 16777619) ^ state_data[i];

	// Find or create a sampler handle for this sampler state
	if (const auto it = _cached_sampler_states.find(state_hash); it != _cached_sampler_states.end())
		return it->second;

	api::sampler_desc desc = {};
	desc.filter = state_data[D3DSAMP_MINFILTER] == D3DTEXF_ANISOTROPIC || state_data[D3DSAMP_MAGFILTER] == D3DTEXF_ANISOTROPIC ?
		api::filter_type::anisotropic : static_cast<api::filter_type>(
			((state_data[D3DSAMP_MINFILTER] >= D3DTEXF_LINEAR ? 1 : 0) << 4) |
			((state_data[D3DSAMP_MAGFILTER] >= D3DTEXF_LINEAR ? 1 : 0) << 2) |
			((state_data[D3DSAMP_MIPFILTER] >= D3DTEXF_LINEAR ? 1 : 0)));
	desc.address_u = static_cast<api::texture_address_mode>(state_data[D3DSAMP_ADDRESSU]);
	desc.address_v = static_cast<api::texture_address_mode>(state_data[D3DSAMP_ADDRESSV]);
	desc.address_w = static_cast<api::texture_address_mode>(state_data[D3DSAMP_ADDRESSW]);
	desc.mip_lod_bias = *reinterpret_cast<const float *>(  &state_data[D3DSAMP_MIPMAPLODBIAS]);
	desc.min_lod = static_cast<float>(state_data[D3DSAMP_MAXMIPLEVEL]);
	desc.max_lod = std::numeric_limits<float>::max();
	desc.max_anisotropy = static_cast<float>(state_data[D3DSAMP_MAXANISOTROPY]);

	api::sampler handle;
	create_sampler(desc, &handle);
	_cached_sampler_states.emplace(state_hash, handle);

	invoke_addon_event<addon_event::init_sampler>(this, desc, handle);

	return handle;
}
#endif
