/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d9.hpp"
#include "render_d3d9_utils.hpp"

using namespace reshade::api;

memory_usage reshade::d3d9::convert_d3d_pool_to_memory_usage(D3DPOOL pool)
{
	switch (pool)
	{
	default:
	case D3DPOOL_DEFAULT:
		return memory_usage::gpu_only;
	case D3DPOOL_MANAGED:
	case D3DPOOL_SYSTEMMEM:
		return memory_usage::cpu_to_gpu;
	case D3DPOOL_SCRATCH:
		return memory_usage::cpu_only;
	}
}

void reshade::d3d9::convert_resource_usage_to_d3d_usage(resource_usage usage, DWORD &d3d_usage)
{
	// Copying textures is implemented using the rasterization pipeline (see 'device_impl::copy_resource' implementation), so needs render target usage
	// When the destination in 'IDirect3DDevice9::StretchRect' is a texture surface, it too has to have render target usage (see https://docs.microsoft.com/windows/win32/api/d3d9helper/nf-d3d9helper-idirect3ddevice9-stretchrect)
	if ((usage & (resource_usage::render_target | resource_usage::copy_dest | resource_usage::resolve_dest)) != 0)
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
void reshade::d3d9::convert_d3d_usage_to_resource_usage(DWORD d3d_usage, resource_usage &usage)
{
	if (d3d_usage & D3DUSAGE_RENDERTARGET)
		usage |= resource_usage::render_target;
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

	convert_resource_usage_to_d3d_usage(desc.usage, internal_desc.Usage);

	if (levels != nullptr)
		*levels = desc.levels;
	else
		assert(desc.levels == 1);
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

	convert_resource_usage_to_d3d_usage(desc.usage, internal_desc.Usage);

	if (levels != nullptr)
		*levels = desc.levels;
	else
		assert(desc.levels == 1);
}
void reshade::d3d9::convert_resource_desc(const resource_desc &desc, D3DINDEXBUFFER_DESC &internal_desc)
{
	assert(desc.size <= std::numeric_limits<UINT>::max());
	internal_desc.Size = static_cast<UINT>(desc.size);
	assert((desc.usage & (resource_usage::vertex_buffer | resource_usage::index_buffer)) == resource_usage::index_buffer);
	convert_resource_usage_to_d3d_usage(desc.usage, internal_desc.Usage);
}
void reshade::d3d9::convert_resource_desc(const resource_desc &desc, D3DVERTEXBUFFER_DESC &internal_desc)
{
	assert(desc.size <= std::numeric_limits<UINT>::max());
	internal_desc.Size = static_cast<UINT>(desc.size);
	assert((desc.usage & (resource_usage::vertex_buffer | resource_usage::index_buffer)) == resource_usage::vertex_buffer);
	convert_resource_usage_to_d3d_usage(desc.usage, internal_desc.Usage);
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

	convert_d3d_usage_to_resource_usage(internal_desc.Usage, desc.usage);
	if (internal_desc.Type == D3DRTYPE_VOLUMETEXTURE)
		desc.usage |= resource_usage::shader_resource;

	return desc;
}
resource_desc reshade::d3d9::convert_resource_desc(const D3DSURFACE_DESC &internal_desc, UINT levels, const D3DCAPS9 &caps)
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

	convert_d3d_usage_to_resource_usage(internal_desc.Usage, desc.usage);
	if (internal_desc.Type == D3DRTYPE_TEXTURE || internal_desc.Type == D3DRTYPE_CUBETEXTURE)
		desc.usage |= resource_usage::shader_resource;

	// Copying is restricted by limitations of 'IDirect3DDevice9::StretchRect' (see https://docs.microsoft.com/windows/win32/api/d3d9helper/nf-d3d9helper-idirect3ddevice9-stretchrect)
	// or performing copy between two textures using rasterization pipeline (see 'device_impl::copy_resource' implementation)
	if (internal_desc.Pool == D3DPOOL_DEFAULT && (internal_desc.Type == D3DRTYPE_SURFACE || (internal_desc.Type == D3DRTYPE_TEXTURE && (caps.Caps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES) != 0)))
	{
		switch (static_cast<DWORD>(internal_desc.Format))
		{
		default:
			desc.usage |= resource_usage::copy_source;
			if (internal_desc.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES)
				desc.usage |= resource_usage::resolve_source;
			if (internal_desc.Usage & D3DUSAGE_RENDERTARGET)
				desc.usage |= resource_usage::copy_dest | resource_usage::resolve_dest;
			break;
		case D3DFMT_DXT1:
		case D3DFMT_DXT2:
		case D3DFMT_DXT3:
		case D3DFMT_DXT4:
		case D3DFMT_DXT5:
			// Stretching is not supported if either surface is in a compressed format
			break;
		case D3DFMT_D16_LOCKABLE:
		case D3DFMT_D32:
		case D3DFMT_D15S1:
		case D3DFMT_D24S8:
		case D3DFMT_D24X8:
		case D3DFMT_D24X4S4:
		case D3DFMT_D16:
		case D3DFMT_D32F_LOCKABLE:
		case D3DFMT_D24FS8:
		case D3DFMT_D32_LOCKABLE:
		case D3DFMT_S8_LOCKABLE:
			// Stretching depth stencil surfaces is extremly limited (does not support copying from surface to texture for example), so just do not allow it
			assert(internal_desc.Usage & D3DUSAGE_DEPTHSTENCIL);
			break;
		case MAKEFOURCC('N', 'U', 'L', 'L'):
			// Special render target format that has no memory attached, so cannot be copied
			break;
		}
	}

	return desc;
}
resource_desc reshade::d3d9::convert_resource_desc(const D3DINDEXBUFFER_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.size = internal_desc.Size;
	convert_d3d_usage_to_resource_usage(internal_desc.Usage, desc.usage);
	desc.usage |= resource_usage::index_buffer;
	return desc;
}
resource_desc reshade::d3d9::convert_resource_desc(const D3DVERTEXBUFFER_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.size = internal_desc.Size;
	convert_d3d_usage_to_resource_usage(internal_desc.Usage, desc.usage);
	desc.usage |= resource_usage::vertex_buffer;
	return desc;
}
