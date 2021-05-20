/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "render_d3d9.hpp"
#include "render_d3d9_utils.hpp"
#include <algorithm>

namespace
{
	struct pipeline_impl
	{
		com_ptr<IDirect3DStateBlock9> state_block;
		D3DPRIMITIVETYPE prim_type;
	};

	struct pipeline_layout_impl
	{
		std::vector<UINT> shader_registers;
	};

	struct query_pool_impl
	{
		reshade::api::query_type type;
		std::vector<com_ptr<IDirect3DQuery9>> queries;
	};

	struct descriptor_set_impl
	{
		reshade::api::descriptor_type type;
		std::vector<uint64_t> descriptors;
		std::vector<reshade::api::sampler_with_resource_view> sampler_with_resource_views;
	};

	struct descriptor_set_layout_impl
	{
		reshade::api::descriptor_range range;
	};

	inline bool convert_format_internal(reshade::api::format format, D3DFORMAT &internal_format)
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
			// This has to be mitiated in 'upload_texture_region' by flipping RGBA to BGRA
			internal_format = D3DFMT_A8R8G8B8;
			return true;
		}

		return internal_format != D3DFMT_UNKNOWN;
	}
}

reshade::d3d9::device_impl::device_impl(IDirect3DDevice9 *device) :
	api_object_impl(device), _caps(), _cp(), _backup_state(device)
{
	_orig->GetDirect3D(&_d3d);
	_orig->GetDeviceCaps(&_caps);
	_orig->GetCreationParameters(&_cp);

	// Maximum simultaneous number of render targets is typically 4 in D3D9
	assert(_caps.NumSimultaneousRTs <= 8);

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
	_default_input_stream.reset();
	_default_input_layout.reset();
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
	}

	if (FAILED(hr))
	{
		LOG(ERROR) << "Failed to create copy state block!";
		return;
	}

	if (!_backup_state.init_state_block())
	{
		LOG(ERROR) << "Failed to create backup state block!";
		return;
	}

	// Create input layout for vertex buffer which holds vertex indices
	{	const UINT max_vertices = 100;

		if (FAILED(_orig->CreateVertexBuffer(max_vertices * sizeof(float), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &_default_input_stream, nullptr)))
		{
			LOG(ERROR) << "Failed to create default input stream!";
			return;
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
			return;
		}
	}

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
		invoke_addon_event<addon_event::begin_render_pass>(this, 0, nullptr, api::resource_view { reinterpret_cast<uintptr_t>(auto_depth_stencil.get()) });
	}
	else
	{
		invoke_addon_event<addon_event::begin_render_pass>(this, 0, nullptr, api::resource_view { 0 });
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

bool reshade::d3d9::device_impl::check_capability(api::device_caps capability) const
{
	switch (capability)
	{
	case api::device_caps::compute_shader:
	case api::device_caps::geometry_shader:
	case api::device_caps::tessellation_shaders:
	case api::device_caps::dual_src_blend:
	case api::device_caps::independent_blend:
	case api::device_caps::logic_op:
	case api::device_caps::draw_instanced:
	case api::device_caps::draw_or_dispatch_indirect:
		return false;
	case api::device_caps::fill_mode_non_solid:
		return true;
	case api::device_caps::multi_viewport:
		return false;
	case api::device_caps::sampler_anisotropy:
	case api::device_caps::partial_push_constant_updates:
	case api::device_caps::partial_push_descriptor_updates:
	case api::device_caps::sampler_with_resource_view:
		return true;
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
		return true;
	case api::device_caps::copy_buffer_region:
	case api::device_caps::copy_buffer_to_texture:
	case api::device_caps::copy_query_results:
		return false;
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

bool reshade::d3d9::device_impl::check_resource_handle_valid(api::resource handle) const
{
	return handle.handle != 0 && _resources.has_object(reinterpret_cast<IDirect3DResource9 *>(handle.handle));
}
bool reshade::d3d9::device_impl::check_resource_view_handle_valid(api::resource_view handle) const
{
	return handle.handle != 0 && _resources.has_object(reinterpret_cast<IDirect3DResource9 *>(handle.handle));
}

bool reshade::d3d9::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out)
{
	const auto data = new DWORD[10];
	data[D3DSAMP_ADDRESSU - 1] = static_cast<DWORD>(desc.address_u);
	data[D3DSAMP_ADDRESSV - 1] = static_cast<DWORD>(desc.address_v);
	data[D3DSAMP_ADDRESSW - 1] = static_cast<DWORD>(desc.address_w);
	data[D3DSAMP_BORDERCOLOR - 1] = 0;
	data[D3DSAMP_MAGFILTER - 1] = 1 + ((static_cast<DWORD>(desc.filter) & 0x0C) >> 2);
	data[D3DSAMP_MINFILTER - 1] = 1 + ((static_cast<DWORD>(desc.filter) & 0x30) >> 4);
	data[D3DSAMP_MIPFILTER - 1] = 1 + ((static_cast<DWORD>(desc.filter) & 0x03));
	data[D3DSAMP_MIPMAPLODBIAS - 1] = *reinterpret_cast<const DWORD *>(&desc.mip_lod_bias);
	data[D3DSAMP_MAXMIPLEVEL - 1] = desc.min_lod > 0 ? static_cast<DWORD>(desc.min_lod) : 0;
	data[D3DSAMP_MAXANISOTROPY - 1] = static_cast<DWORD>(desc.max_anisotropy);

	*out = { reinterpret_cast<uintptr_t>(data) };
	return true;
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
						_resources.register_object(object.get());
						*out = { reinterpret_cast<uintptr_t>(object.release()) };

						if (initial_data != nullptr)
						{
							upload_buffer_region(initial_data->data, *out, 0, desc.buffer.size);
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
						_resources.register_object(object.get());
						*out = { reinterpret_cast<uintptr_t>(object.release()) };

						if (initial_data != nullptr)
						{
							upload_buffer_region(initial_data->data, *out, 0, desc.buffer.size);
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
			convert_resource_desc(desc, internal_desc, &levels);

			if (!convert_format_internal(desc.texture.format, internal_desc.Format))
				break;

			if ((desc.flags & api::resource_flags::cube_compatible) != api::resource_flags::cube_compatible)
			{
				if (com_ptr<IDirect3DTexture9> object;
					SUCCEEDED(_orig->CreateTexture(internal_desc.Width, internal_desc.Height, levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, &object, nullptr)))
				{
					_resources.register_object(object.get());
					*out = { reinterpret_cast<uintptr_t>(object.release()) };

					if (initial_data != nullptr)
					{
						// Cannot get level information for textures created with the 'generate_mipmaps' flag, so only upload to the base level
						if (levels == 0)
							levels  = 1;

						for (uint32_t subresource = 0; subresource < levels; ++subresource)
							upload_texture_region(initial_data[subresource], *out, subresource, nullptr);
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
					_resources.register_object(object.get());
					*out = { reinterpret_cast<uintptr_t>(object.release()) };

					if (initial_data != nullptr)
					{
						for (uint32_t subresource = 0; subresource < static_cast<uint32_t>(desc.texture.depth_or_layers) * desc.texture.levels; ++subresource)
							upload_texture_region(initial_data[subresource], *out, subresource, nullptr);
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
			convert_resource_desc(desc, internal_desc, &levels);

			if (!convert_format_internal(desc.texture.format, internal_desc.Format))
				break;

			if (com_ptr<IDirect3DVolumeTexture9> object;
				SUCCEEDED(_orig->CreateVolumeTexture(internal_desc.Width, internal_desc.Height, internal_desc.Depth, levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, &object, nullptr)))
			{
				_resources.register_object(object.get());
				*out = { reinterpret_cast<uintptr_t>(object.release()) };

				if (initial_data != nullptr)
				{
					for (uint32_t subresource = 0; subresource < static_cast<uint32_t>(desc.texture.depth_or_layers) * desc.texture.levels; ++subresource)
						upload_texture_region(initial_data[subresource], *out, subresource, nullptr);
				}

				return true;
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}
bool reshade::d3d9::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out)
{
	assert(resource.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	// Set the first bit in the handle to indicate whether this view is using a sRGB format
	// This requires the resource pointers to be at least 2-byte aligned, to ensure the first bit is always unused otherwise
	static_assert(alignof(IDirect3DResource9) >= 2);
	const uint64_t set_srgb_bit =
		desc.format == api::format::r8g8b8a8_unorm_srgb || desc.format == api::format::b8g8r8a8_unorm_srgb || desc.format == api::format::b8g8r8x8_unorm_srgb ||
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

bool reshade::d3d9::device_impl::create_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	// Check for unsupported states
	if (desc.type != api::pipeline_type::graphics ||
		desc.graphics.blend_state.num_viewports > 1 ||
		desc.graphics.blend_state.num_render_targets > _caps.NumSimultaneousRTs ||
		desc.graphics.multisample_state.alpha_to_coverage ||
		desc.graphics.hull_shader.handle != 0 ||
		desc.graphics.domain_shader.handle != 0 ||
		desc.graphics.geometry_shader.handle != 0 ||
		FAILED(_orig->BeginStateBlock()))
	{
		*out = { 0 };
		return false;
	}

	com_ptr<IDirect3DVertexDeclaration9> decl;
	if (desc.graphics.input_layout[0].format != api::format::unknown)
	{
		std::vector<D3DVERTEXELEMENT9> internal_elements;
		internal_elements.reserve(16 + 1);
		for (UINT i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
		{
			const auto &element = desc.graphics.input_layout[i];
			D3DVERTEXELEMENT9 &internal_element = internal_elements.emplace_back();

			assert(element.buffer_binding <= std::numeric_limits<WORD>::max());
			internal_element.Stream = static_cast<WORD>(element.buffer_binding);
			assert(element.offset <= std::numeric_limits<WORD>::max());
			internal_element.Offset = static_cast<WORD>(element.offset);

			switch (element.format)
			{
			default:
				assert(false);
				*out = { 0 };
				return false;
			case api::format::r8g8b8a8_uint:
				internal_element.Type = D3DDECLTYPE_UBYTE4;
				break;
			case api::format::r8g8b8a8_unorm:
				internal_element.Type = D3DDECLTYPE_UBYTE4N;
				break;
			case api::format::b8g8r8a8_unorm:
				internal_element.Type = D3DDECLTYPE_D3DCOLOR;
				break;
			case api::format::r10g10b10a2_uint:
				internal_element.Type = D3DDECLTYPE_UDEC3;
				break;
			case api::format::r10g10b10a2_unorm:
				internal_element.Type = D3DDECLTYPE_DEC3N;
				break;
			case api::format::r16g16_sint:
				internal_element.Type = D3DDECLTYPE_SHORT2;
				break;
			case api::format::r16g16_float:
				internal_element.Type = D3DDECLTYPE_FLOAT16_2;
				break;
			case api::format::r16g16_unorm:
				internal_element.Type = D3DDECLTYPE_USHORT2N;
				break;
			case api::format::r16g16_snorm:
				internal_element.Type = D3DDECLTYPE_SHORT2N;
				break;
			case api::format::r16g16b16a16_sint:
				internal_element.Type = D3DDECLTYPE_SHORT4;
				break;
			case api::format::r16g16b16a16_float:
				internal_element.Type = D3DDECLTYPE_FLOAT16_4;
				break;
			case api::format::r16g16b16a16_unorm:
				internal_element.Type = D3DDECLTYPE_USHORT4N;
				break;
			case api::format::r16g16b16a16_snorm:
				internal_element.Type = D3DDECLTYPE_SHORT4N;
				break;
			case api::format::r32_float:
				internal_element.Type = D3DDECLTYPE_FLOAT1;
				break;
			case api::format::r32g32_float:
				internal_element.Type = D3DDECLTYPE_FLOAT2;
				break;
			case api::format::r32g32b32_float:
				internal_element.Type = D3DDECLTYPE_FLOAT3;
				break;
			case api::format::r32g32b32a32_float:
				internal_element.Type = D3DDECLTYPE_FLOAT4;
				break;
			}

			if (strcmp(element.semantic, "POSITION") == 0)
				internal_element.Usage = D3DDECLUSAGE_POSITION;
			else if (strcmp(element.semantic, "BLENDWEIGHT") == 0)
				internal_element.Usage = D3DDECLUSAGE_BLENDWEIGHT;
			else if (strcmp(element.semantic, "BLENDINDICES") == 0)
				internal_element.Usage = D3DDECLUSAGE_BLENDINDICES;
			else if (strcmp(element.semantic, "NORMAL") == 0)
				internal_element.Usage = D3DDECLUSAGE_NORMAL;
			else if (strcmp(element.semantic, "PSIZE") == 0)
				internal_element.Usage = D3DDECLUSAGE_PSIZE;
			else if (strcmp(element.semantic, "TANGENT") == 0)
				internal_element.Usage = D3DDECLUSAGE_TANGENT;
			else if (strcmp(element.semantic, "BINORMAL") == 0)
				internal_element.Usage = D3DDECLUSAGE_BINORMAL;
			else if (strcmp(element.semantic, "TESSFACTOR") == 0)
				internal_element.Usage = D3DDECLUSAGE_TESSFACTOR;
			else if (strcmp(element.semantic, "POSITIONT") == 0)
				internal_element.Usage = D3DDECLUSAGE_POSITIONT;
			else if (strcmp(element.semantic, "COLOR") == 0)
				internal_element.Usage = D3DDECLUSAGE_COLOR;
			else if (strcmp(element.semantic, "FOG") == 0)
				internal_element.Usage = D3DDECLUSAGE_FOG;
			else if (strcmp(element.semantic, "DEPTH") == 0)
				internal_element.Usage = D3DDECLUSAGE_DEPTH;
			else if (strcmp(element.semantic, "SAMPLE") == 0)
				internal_element.Usage = D3DDECLUSAGE_SAMPLE;
			else
				internal_element.Usage = D3DDECLUSAGE_TEXCOORD;

			assert(element.semantic_index <= 256);
			internal_element.UsageIndex = static_cast<BYTE>(element.semantic_index);
		}

		internal_elements.push_back(D3DDECL_END());

		if (FAILED(_orig->CreateVertexDeclaration(internal_elements.data(), &decl)))
		{
			*out = { 0 };
			return false;
		}
	}

	_orig->SetPixelShader(reinterpret_cast<IDirect3DPixelShader9 *>(desc.graphics.pixel_shader.handle));
	_orig->SetVertexShader(reinterpret_cast<IDirect3DVertexShader9 *>(desc.graphics.vertex_shader.handle));

	if (decl != nullptr)
	{
		_orig->SetVertexDeclaration(decl.get());
	}
	else
	{
		// Setup default input (used to have a vertex ID in vertex shaders)
		_orig->SetStreamSource(0, _default_input_stream.get(), 0, sizeof(float));
		_orig->SetVertexDeclaration(_default_input_layout.get());
	}

	_orig->SetRenderState(D3DRS_ZENABLE,
		desc.graphics.depth_stencil_state.depth_test);
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
		desc.graphics.depth_stencil_state.stencil_test);
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
		desc.graphics.rasterizer_state.depth_clip);
	_orig->SetRenderState(D3DRS_LIGHTING, FALSE);
	_orig->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
	_orig->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
	_orig->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,
		desc.graphics.multisample_state.multisample);
	_orig->SetRenderState(D3DRS_MULTISAMPLEMASK,
		desc.graphics.multisample_state.sample_mask);
	_orig->SetRenderState(D3DRS_COLORWRITEENABLE,
		desc.graphics.blend_state.render_target_write_mask[0]);
	_orig->SetRenderState(D3DRS_BLENDOP,
		convert_blend_op(desc.graphics.blend_state.color_blend_op[0]));
	_orig->SetRenderState(D3DRS_SCISSORTESTENABLE,
		desc.graphics.rasterizer_state.scissor_test);
	_orig->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,
		*reinterpret_cast<const DWORD *>(&desc.graphics.rasterizer_state.slope_scaled_depth_bias));
	_orig->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE,
		desc.graphics.rasterizer_state.antialiased_line);
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
		static_cast<INT>(desc.graphics.rasterizer_state.depth_bias));
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
		const auto result = new pipeline_impl();
		result->prim_type = convert_primitive_topology(desc.graphics.rasterizer_state.topology);
		result->state_block = std::move(state_block);

		*out = { reinterpret_cast<uintptr_t>(result) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d9::device_impl::create_shader_module(api::shader_stage type, api::shader_format format, const char *entry_point, const void *code, size_t, api::shader_module *out)
{
	if (format == api::shader_format::dxbc)
	{
		assert(entry_point == nullptr);

		switch (type)
		{
		case api::shader_stage::vertex:
			if (com_ptr<IDirect3DVertexShader9> shader;
				SUCCEEDED(_orig->CreateVertexShader(static_cast<const DWORD *>(code), &shader)))
			{
				*out = { reinterpret_cast<uintptr_t>(shader.release()) };
				return true;
			}
			break;
		case api::shader_stage::pixel:
			if (com_ptr<IDirect3DPixelShader9> shader;
				SUCCEEDED(_orig->CreatePixelShader(static_cast<const DWORD *>(code), &shader)))
			{
				*out = { reinterpret_cast<uintptr_t>(shader.release()) };
				return true;
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}
bool reshade::d3d9::device_impl::create_pipeline_layout(uint32_t num_set_layouts, const api::descriptor_set_layout *set_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out)
{
	const auto layout_impl = new pipeline_layout_impl();
	layout_impl->shader_registers.resize(num_set_layouts + num_constant_ranges);

	for (UINT i = 0; i < num_set_layouts; ++i)
	{
		if (set_layouts[i].handle == 0)
			continue;

		layout_impl->shader_registers[i] = reinterpret_cast<descriptor_set_layout_impl *>(set_layouts[i].handle)->range.dx_shader_register;
	}

	for (UINT i = 0; i < num_constant_ranges; ++i)
	{
		layout_impl->shader_registers[num_set_layouts + i] = constant_ranges[i].offset / 4;
	}

	*out = { reinterpret_cast<uintptr_t>(layout_impl) };
	return true;
}
bool reshade::d3d9::device_impl::create_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, api::descriptor_set *out)
{
	const auto layout_impl = reinterpret_cast<descriptor_set_layout_impl *>(layout.handle);

	for (UINT i = 0; i < count; ++i)
	{
		const auto set = new descriptor_set_impl();
		set->type = layout_impl->range.type;

		if (layout_impl->range.type != api::descriptor_type::sampler_with_resource_view)
			set->descriptors.resize(layout_impl->range.count);
		else
			set->sampler_with_resource_views.resize(layout_impl->range.count);

		out[i] = { reinterpret_cast<uintptr_t>(set) };
	}

	return true;
}
bool reshade::d3d9::device_impl::create_descriptor_set_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool, api::descriptor_set_layout *out)
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

bool reshade::d3d9::device_impl::create_query_pool(api::query_type type, uint32_t count, api::query_pool *out)
{
	const auto result = new query_pool_impl();
	result->type = type;
	result->queries.resize(count);

	for (UINT i = 0; i < count; ++i)
	{
		if (FAILED(_orig->CreateQuery(convert_query_type(type), &result->queries[i])))
		{
			delete result;

			*out = { 0 };
			return false;
		}
	}

	*out = { reinterpret_cast<uintptr_t>(result) };
	return true;
}

void reshade::d3d9::device_impl::destroy_sampler(api::sampler handle)
{
	delete[] reinterpret_cast<DWORD *>(handle.handle);
}
void reshade::d3d9::device_impl::destroy_resource(api::resource handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d9::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle & ~1ull)->Release();
}

void reshade::d3d9::device_impl::destroy_pipeline(api::pipeline_type type, api::pipeline handle)
{
	assert(type == api::pipeline_type::graphics);

	delete reinterpret_cast<pipeline_impl *>(handle.handle);
}
void reshade::d3d9::device_impl::destroy_shader_module(api::shader_module handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d9::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	delete reinterpret_cast<pipeline_layout_impl *>(handle.handle);
}
void reshade::d3d9::device_impl::destroy_descriptor_sets(api::descriptor_set_layout, uint32_t count, const api::descriptor_set *sets)
{
	for (UINT i = 0; i < count; ++i)
		delete reinterpret_cast<descriptor_set_impl *>(sets[i].handle);
}
void reshade::d3d9::device_impl::destroy_descriptor_set_layout(api::descriptor_set_layout handle)
{
	delete reinterpret_cast<descriptor_set_layout_impl *>(handle.handle);
}

void reshade::d3d9::device_impl::destroy_query_pool(api::query_pool handle)
{
	delete reinterpret_cast<query_pool_impl *>(handle.handle);
}

void reshade::d3d9::device_impl::update_descriptor_sets(uint32_t num_updates, const api::descriptor_update *updates)
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
			set_impl->sampler_with_resource_views[updates[i].binding] = updates[i].descriptor;
			break;
		case api::descriptor_type::shader_resource_view:
			set_impl->descriptors[updates[i].binding] = updates[i].descriptor.view.handle;
			break;
		case api::descriptor_type::unordered_access_view:
		case api::descriptor_type::constant_buffer:
			assert(false);
			break;
		}
	}
}

bool reshade::d3d9::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **mapped_ptr)
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

	assert(resource.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	switch (object->GetType())
	{
	case D3DRTYPE_SURFACE:
		assert(subresource == 0);
		if (D3DLOCKED_RECT locked_rect;
			SUCCEEDED(static_cast<IDirect3DSurface9 *>(object)->LockRect(&locked_rect, nullptr, flags)))
		{
			*mapped_ptr = locked_rect.pBits;
			return true;
		}
		break;
	case D3DRTYPE_TEXTURE:
		if (D3DLOCKED_RECT locked_rect;
			SUCCEEDED(static_cast<IDirect3DTexture9 *>(object)->LockRect(subresource, &locked_rect, nullptr, flags)))
		{
			*mapped_ptr = locked_rect.pBits;
			return true;
		}
		break;
	case D3DRTYPE_VOLUMETEXTURE:
		if (D3DLOCKED_BOX locked_box;
			SUCCEEDED(static_cast<IDirect3DVolumeTexture9 *>(object)->LockBox(subresource, &locked_box, nullptr, flags)))
		{
			*mapped_ptr = locked_box.pBits;
			return true;
		}
		break;
	case D3DRTYPE_CUBETEXTURE: {
		const UINT levels = static_cast<IDirect3DCubeTexture9 *>(object)->GetLevelCount();

		if (D3DLOCKED_RECT locked_rect;
			SUCCEEDED(static_cast<IDirect3DCubeTexture9 *>(object)->LockRect(static_cast<D3DCUBEMAP_FACES>(subresource / levels), subresource % levels, &locked_rect, nullptr, flags)))
		{
			*mapped_ptr = locked_rect.pBits;
			return true;
		}
		break;
	}
	case D3DRTYPE_VERTEXBUFFER:
		assert(subresource == 0);
		return SUCCEEDED(static_cast<IDirect3DVertexBuffer9 *>(object)->Lock(0, 0, mapped_ptr, flags));
	case D3DRTYPE_INDEXBUFFER:
		assert(subresource == 0);
		return SUCCEEDED(static_cast<IDirect3DIndexBuffer9 *>(object)->Lock(0, 0, mapped_ptr, flags));
	}

	*mapped_ptr = nullptr;
	return false;
}
void reshade::d3d9::device_impl::unmap_resource(api::resource resource, uint32_t subresource)
{
	assert(resource.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(resource.handle);

	switch (object->GetType())
	{
	case D3DRTYPE_SURFACE:
		assert(subresource == 0);
		static_cast<IDirect3DSurface9 *>(object)->UnlockRect();
		return;
	case D3DRTYPE_TEXTURE:
		static_cast<IDirect3DTexture9 *>(object)->UnlockRect(subresource);
		return;
	case D3DRTYPE_VOLUMETEXTURE:
		static_cast<IDirect3DVolumeTexture9 *>(object)->UnlockBox(subresource);
		return;
	case D3DRTYPE_CUBETEXTURE: {
		const UINT levels = static_cast<IDirect3DCubeTexture9 *>(object)->GetLevelCount();

		static_cast<IDirect3DCubeTexture9 *>(object)->UnlockRect(static_cast<D3DCUBEMAP_FACES>(subresource / levels), subresource % levels);
		return;
	}
	case D3DRTYPE_VERTEXBUFFER:
		assert(subresource == 0);
		static_cast<IDirect3DVertexBuffer9 *>(object)->Unlock();
		return;
	case D3DRTYPE_INDEXBUFFER:
		assert(subresource == 0);
		static_cast<IDirect3DIndexBuffer9 *>(object)->Unlock();
		return;
	}
}

void reshade::d3d9::device_impl::upload_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(dst.handle != 0);
	assert(dst_offset <= std::numeric_limits<UINT>::max() && size <= std::numeric_limits<UINT>::max());

	auto object = reinterpret_cast<IDirect3DResource9 *>(dst.handle);

	switch (object->GetType())
	{
	case D3DRTYPE_VERTEXBUFFER:
		if (void *mapped_ptr = nullptr;
			SUCCEEDED(static_cast<IDirect3DVertexBuffer9 *>(object)->Lock(static_cast<UINT>(dst_offset), static_cast<UINT>(size), &mapped_ptr, 0)))
		{
			std::memcpy(mapped_ptr, data, static_cast<size_t>(size));
			static_cast<IDirect3DVertexBuffer9 *>(object)->Unlock();
		}
		return;
	case D3DRTYPE_INDEXBUFFER:
		if (void *mapped_ptr = nullptr;
			SUCCEEDED(static_cast<IDirect3DIndexBuffer9 *>(object)->Lock(static_cast<UINT>(dst_offset), static_cast<UINT>(size), &mapped_ptr, 0)))
		{
			std::memcpy(mapped_ptr, data, static_cast<size_t>(size));
			static_cast<IDirect3DIndexBuffer9 *>(object)->Unlock();
		}
		return;
	}

	assert(false); // Not implemented
}
void reshade::d3d9::device_impl::upload_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
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

void reshade::d3d9::device_impl::get_resource_from_view(api::resource_view view, api::resource *out_resource) const
{
	assert(view.handle != 0);
	auto object = reinterpret_cast<IDirect3DResource9 *>(view.handle & ~1ull);

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

bool reshade::d3d9::device_impl::get_query_results(api::query_pool heap, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(heap.handle != 0);
	const auto impl = reinterpret_cast<query_pool_impl *>(heap.handle);

	for (UINT i = 0; i < count; ++i)
	{
		if (impl->queries[i + first]->GetData(static_cast<uint8_t *>(results) + i * stride, stride, 0) != S_OK)
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

void reshade::d3d9::device_impl::bind_pipeline(api::pipeline_type type, api::pipeline pipeline)
{
	assert(type == api::pipeline_type::graphics && pipeline.handle != 0);
	const auto impl = reinterpret_cast<pipeline_impl *>(pipeline.handle);

	impl->state_block->Apply();
	_current_prim_type = impl->prim_type;
}
void reshade::d3d9::device_impl::bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values)
{
	for (UINT i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::pipeline_state::front_face_ccw:
		case api::pipeline_state::depth_bias_clamp:
		case api::pipeline_state::alpha_to_coverage:
			assert(false);
			break;
		case api::pipeline_state::primitive_topology:
			_current_prim_type = convert_primitive_topology(static_cast<api::primitive_topology>(values[i]));
			break;
		default:
			_orig->SetRenderState(static_cast<D3DRENDERSTATETYPE>(states[i]), values[i]);
			break;
		}
	}
}
void reshade::d3d9::device_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	assert(first == 0 && count == 1);

	D3DVIEWPORT9 d3d_viewport;
	d3d_viewport.X = static_cast<DWORD>(viewports[0]);
	d3d_viewport.Y = static_cast<DWORD>(viewports[1]);
	d3d_viewport.Width = static_cast<DWORD>(viewports[2]);
	d3d_viewport.Height = static_cast<DWORD>(viewports[3]);
	d3d_viewport.MinZ = viewports[4];
	d3d_viewport.MaxZ = viewports[5];

	_orig->SetViewport(&d3d_viewport);
}
void reshade::d3d9::device_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	assert(first == 0 && count == 1);

	_orig->SetScissorRect(reinterpret_cast<const RECT *>(rects));
}

void reshade::d3d9::device_impl::push_constants(api::shader_stage stage, api::pipeline_layout, uint32_t, uint32_t first, uint32_t count, const void *values)
{
	if ((stage & api::shader_stage::vertex) == api::shader_stage::vertex)
		_orig->SetVertexShaderConstantF(first / 4, static_cast<const float *>(values), count / 4);
	if ((stage & api::shader_stage::pixel) == api::shader_stage::pixel)
		_orig->SetPixelShaderConstantF(first / 4, static_cast<const float *>(values), count / 4);
}
void reshade::d3d9::device_impl::push_descriptors(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
{
	if ((stage & (api::shader_stage::vertex | api::shader_stage::pixel)) == (api::shader_stage::vertex | api::shader_stage::pixel))
	{
		// Call for each individual shader stage
		push_descriptors(api::shader_stage::vertex, layout, layout_index, type, first, count, descriptors);
		push_descriptors( api::shader_stage::pixel, layout, layout_index, type, first, count, descriptors);
		return;
	}

	if (layout.handle != 0)
		first += reinterpret_cast<pipeline_layout_impl *>(layout.handle)->shader_registers[layout_index];

	switch (stage)
	{
	default:
		assert(false);
		return;
	case api::shader_stage::vertex:
		// See https://docs.microsoft.com/windows/win32/direct3d9/vertex-textures-in-vs-3-0
		first += D3DVERTEXTEXTURESAMPLER0;
		if ((first + count) > D3DVERTEXTEXTURESAMPLER3) // The vertex engine only contains four texture sampler stages
			count = D3DVERTEXTEXTURESAMPLER3 - first;
		break;
	case api::shader_stage::pixel:
		break;
	}

	switch (type)
	{
	case api::descriptor_type::sampler:
		for (UINT i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler *>(descriptors)[i];

			if (descriptor.handle != 0)
				for (D3DSAMPLERSTATETYPE state = D3DSAMP_ADDRESSU; state <= D3DSAMP_MAXANISOTROPY; state = static_cast<D3DSAMPLERSTATETYPE>(state + 1))
					_orig->SetSamplerState(i + first, state, reinterpret_cast<const DWORD *>(descriptor.handle)[state - 1]);
		}
		break;
	case api::descriptor_type::sampler_with_resource_view:
		for (UINT i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(descriptors)[i];
			_orig->SetTexture(i + first, reinterpret_cast<IDirect3DBaseTexture9 *>(descriptor.view.handle & ~1ull));
			_orig->SetSamplerState(i + first, D3DSAMP_SRGBTEXTURE, descriptor.view.handle & 1);

			if (descriptor.sampler.handle != 0)
				for (D3DSAMPLERSTATETYPE state = D3DSAMP_ADDRESSU; state <= D3DSAMP_MAXANISOTROPY; state = static_cast<D3DSAMPLERSTATETYPE>(state + 1))
					_orig->SetSamplerState(i + first, state, reinterpret_cast<const DWORD *>(descriptor.sampler.handle)[state - 1]);
		}
		break;
	case api::descriptor_type::shader_resource_view:
		for (UINT i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(descriptors)[i];
			_orig->SetTexture(i + first, reinterpret_cast<IDirect3DBaseTexture9 *>(descriptor.handle & ~1ull));
			_orig->SetSamplerState(i + first, D3DSAMP_SRGBTEXTURE, descriptor.handle & 1);
		}
		break;
	case api::descriptor_type::unordered_access_view:
	case api::descriptor_type::constant_buffer:
		assert(false);
		break;
	}
}
void reshade::d3d9::device_impl::bind_descriptor_sets(api::pipeline_type type, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)
{
	assert(type == api::pipeline_type::graphics);

	for (UINT i = 0; i < count; ++i)
	{
		const auto set_impl = reinterpret_cast<descriptor_set_impl *>(sets[i].handle);

		if (set_impl->type != api::descriptor_type::sampler_with_resource_view)
		{
			push_descriptors(
				api::shader_stage::all_graphics,
				layout,
				i + first,
				set_impl->type,
				0,
				static_cast<uint32_t>(set_impl->descriptors.size()),
				set_impl->descriptors.data());
		}
		else
		{
			push_descriptors(
				api::shader_stage::all_graphics,
				layout,
				i + first,
				set_impl->type,
				0,
				static_cast<uint32_t>(set_impl->sampler_with_resource_views.size()),
				set_impl->sampler_with_resource_views.data());
		}
	}
}

void reshade::d3d9::device_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	assert(offset == 0);

#ifndef NDEBUG
	if (buffer.handle != 0)
	{
		assert(index_size == 2 || index_size == 4);

		D3DINDEXBUFFER_DESC desc;
		reinterpret_cast<IDirect3DIndexBuffer9 *>(buffer.handle)->GetDesc(&desc);
		assert(desc.Format == (index_size == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32));
	}
#else
	UNREFERENCED_PARAMETER(offset);
	UNREFERENCED_PARAMETER(index_size);
#endif

	_orig->SetIndices(reinterpret_cast<IDirect3DIndexBuffer9 *>(buffer.handle));
}
void reshade::d3d9::device_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	for (UINT i = 0; i < count; ++i)
	{
		assert(offsets == nullptr || offsets[i] <= std::numeric_limits<UINT>::max());

		_orig->SetStreamSource(i + first, reinterpret_cast<IDirect3DVertexBuffer9 *>(buffers[i].handle), offsets != nullptr ? static_cast<UINT>(offsets[i]) : 0, strides[i]);
	}
}

void reshade::d3d9::device_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	assert(instances == 1 && first_instance == 0);

	_orig->DrawPrimitive(_current_prim_type, first_vertex, calc_prim_from_vertex_count(_current_prim_type, vertices));
}
void reshade::d3d9::device_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	assert(instances == 1 && first_instance == 0);

	_orig->DrawIndexedPrimitive(_current_prim_type, vertex_offset, 0, 0xFFFF, first_index, calc_prim_from_vertex_count(_current_prim_type, indices));
}
void reshade::d3d9::device_impl::dispatch(uint32_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::d3d9::device_impl::draw_or_dispatch_indirect(uint32_t, api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d9::device_impl::begin_render_pass(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	assert(count <= _caps.NumSimultaneousRTs);

	for (UINT i = 0; i < count; ++i)
		_orig->SetRenderTarget(i, reinterpret_cast<IDirect3DSurface9 *>(rtvs[i].handle & ~1ull));
	for (UINT i = count; i < _caps.NumSimultaneousRTs; ++i)
		_orig->SetRenderTarget(i, nullptr);

	if (count != 0)
		_orig->SetRenderState(D3DRS_SRGBWRITEENABLE, rtvs[0].handle & 1);

	_orig->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9 *>(dsv.handle));
}
void reshade::d3d9::device_impl::end_render_pass()
{
}

void reshade::d3d9::device_impl::blit(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter)
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
void reshade::d3d9::device_impl::resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], api::format)
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
		src_box[3] = src_box[0] + std::max(1u, desc.texture.width >> src_subresource);
		src_box[4] = src_box[1] + std::max(1u, desc.texture.height >> src_subresource);
		src_box[5] = src_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.texture.depth_or_layers) >> src_subresource) : 1u);
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
		dst_box[3] = dst_box[0] + std::max(1u, desc.texture.width >> dst_subresource);
		dst_box[4] = dst_box[1] + std::max(1u, desc.texture.height >> dst_subresource);
		dst_box[5] = dst_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.texture.depth_or_layers) >> dst_subresource) : 1u);
	}

	blit(src, src_subresource, src_box, dst, dst_subresource, dst_box, api::texture_filter::min_mag_mip_point);
}
void reshade::d3d9::device_impl::copy_resource(api::resource src, api::resource dst)
{
	const api::resource_desc desc = get_resource_desc(src);

	if (desc.type == api::resource_type::buffer)
	{
		copy_buffer_region(src, 0, dst, 0, ~0llu);
	}
	else
	{
		for (uint32_t level = 0; level < desc.texture.levels; ++level)
		{
			const uint32_t subresource = level;

			copy_texture_region(src, subresource, nullptr, dst, subresource, nullptr, nullptr);
		}
	}
}
void reshade::d3d9::device_impl::copy_buffer_region(api::resource, uint64_t, api::resource, uint64_t, uint64_t)
{
	assert(false);
}
void reshade::d3d9::device_impl::copy_buffer_to_texture(api::resource, uint64_t, uint32_t, uint32_t, api::resource, uint32_t, const int32_t[6])
{
	assert(false);
}
void reshade::d3d9::device_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3])
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
		src_box[3] = src_box[0] + std::max(1u, desc.texture.width >> src_subresource);
		src_box[4] = src_box[1] + std::max(1u, desc.texture.height >> src_subresource);
		src_box[5] = src_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.texture.depth_or_layers) >> src_subresource) : 1u);
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
		dst_box[3] = dst_box[0] + std::max(1u, desc.texture.width >> dst_subresource);
		dst_box[4] = dst_box[1] + std::max(1u, desc.texture.height >> dst_subresource);
		dst_box[5] = dst_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.texture.depth_or_layers) >> dst_subresource) : 1u);
	}

	blit(src, src_subresource, src_box, dst, dst_subresource, dst_box, api::texture_filter::min_mag_mip_point);
}
void reshade::d3d9::device_impl::copy_texture_to_buffer(api::resource, uint32_t, const int32_t[6], api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d9::device_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);
	const auto texture = reinterpret_cast<IDirect3DBaseTexture9 *>(srv.handle & ~1ull);

	texture->SetAutoGenFilterType(D3DTEXF_LINEAR);
	texture->GenerateMipSubLevels();
}

void reshade::d3d9::device_impl::clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil)
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
void reshade::d3d9::device_impl::clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4])
{
	assert(color != nullptr);
#if 0
	assert(count <= _caps.NumSimultaneousRTs);

	_backup_state.capture();

	for (DWORD target = 0; target < count && target < _caps.NumSimultaneousRTs; ++target)
		_orig->SetRenderTarget(target, reinterpret_cast<IDirect3DSurface9 *>(rtvs[target].handle & ~1ull));
	for (DWORD target = count; target < _caps.NumSimultaneousRTs; ++target)
		_orig->SetRenderTarget(target, nullptr);

	_orig->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]), 0.0f, 0);

	_backup_state.apply_and_release();
#else
	for (UINT i = 0; i < count; ++i)
	{
		assert(rtvs[i].handle != 0);

		_orig->ColorFill(reinterpret_cast<IDirect3DSurface9 *>(rtvs[i].handle & ~1ull), nullptr, D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]));
	}
#endif
}
void reshade::d3d9::device_impl::clear_unordered_access_view_uint(api::resource_view, const uint32_t[4])
{
	assert(false);
}
void reshade::d3d9::device_impl::clear_unordered_access_view_float(api::resource_view, const float[4])
{
	assert(false);
}

void reshade::d3d9::device_impl::begin_query(api::query_pool pool, api::query_type, uint32_t index)
{
	assert(pool.handle != 0);
	reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index]->Issue(D3DISSUE_BEGIN);
}
void reshade::d3d9::device_impl::end_query(api::query_pool pool, api::query_type, uint32_t index)
{
	assert(pool.handle != 0);
	reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index]->Issue(D3DISSUE_END);
}
void reshade::d3d9::device_impl::copy_query_results(api::query_pool, api::query_type, uint32_t, uint32_t, api::resource, uint64_t, uint32_t)
{
	assert(false);
}

void reshade::d3d9::device_impl::begin_debug_marker(const char *label, const float color[4])
{
	const size_t label_len = strlen(label);
	std::wstring label_wide;
	label_wide.reserve(label_len + 1);
	utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

	D3DPERF_BeginEvent(color != nullptr ? D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]) : 0, label_wide.c_str());
}
void reshade::d3d9::device_impl::end_debug_marker()
{
	D3DPERF_EndEvent();
}
void reshade::d3d9::device_impl::insert_debug_marker(const char *label, const float color[4])
{
	const size_t label_len = strlen(label);
	std::wstring label_wide;
	label_wide.reserve(label_len + 1);
	utf8::unchecked::utf8to16(label, label + label_len, std::back_inserter(label_wide));

	D3DPERF_SetMarker(color != nullptr ? D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]) : 0, label_wide.c_str());
}
