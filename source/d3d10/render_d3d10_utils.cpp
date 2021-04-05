/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d10.hpp"
#include "render_d3d10_utils.hpp"

using namespace reshade::api;

static inline void convert_usage_to_bind_flags(const resource_usage usage, UINT &bind_flags)
{
	if ((usage & resource_usage::render_target) != 0)
		bind_flags |= D3D10_BIND_RENDER_TARGET;
	else
		bind_flags &= ~D3D10_BIND_RENDER_TARGET;

	if ((usage & resource_usage::depth_stencil) != 0)
		bind_flags |= D3D10_BIND_DEPTH_STENCIL;
	else
		bind_flags &= ~D3D10_BIND_DEPTH_STENCIL;

	if ((usage & resource_usage::shader_resource) != 0)
		bind_flags |= D3D10_BIND_SHADER_RESOURCE;
	else
		bind_flags &= ~D3D10_BIND_SHADER_RESOURCE;

	assert((usage & resource_usage::unordered_access) == 0);

	if ((usage & resource_usage::index_buffer) != 0)
		bind_flags |= D3D10_BIND_INDEX_BUFFER;
	else
		bind_flags &= ~D3D10_BIND_INDEX_BUFFER;

	if ((usage & resource_usage::vertex_buffer) != 0)
		bind_flags |= D3D10_BIND_VERTEX_BUFFER;
	else
		bind_flags &= ~D3D10_BIND_VERTEX_BUFFER;

	if ((usage & resource_usage::constant_buffer) != 0)
		bind_flags |= D3D10_BIND_CONSTANT_BUFFER;
	else
		bind_flags &= ~D3D10_BIND_CONSTANT_BUFFER;
}
static inline void convert_bind_flags_to_usage(const UINT bind_flags, resource_usage &usage)
{
	// Resources are generally copyable in D3D10
	usage |= resource_usage::copy_dest | resource_usage::copy_source;

	if ((bind_flags & D3D10_BIND_RENDER_TARGET) != 0)
		usage |= resource_usage::render_target;
	if ((bind_flags & D3D10_BIND_DEPTH_STENCIL) != 0)
		usage |= resource_usage::depth_stencil;
	if ((bind_flags & D3D10_BIND_SHADER_RESOURCE) != 0)
		usage |= resource_usage::shader_resource;

	if ((bind_flags & D3D10_BIND_INDEX_BUFFER) != 0)
		usage |= resource_usage::index_buffer;
	if ((bind_flags & D3D10_BIND_VERTEX_BUFFER) != 0)
		usage |= resource_usage::vertex_buffer;
	if ((bind_flags & D3D10_BIND_CONSTANT_BUFFER) != 0)
		usage |= resource_usage::constant_buffer;
}

void reshade::d3d10::convert_memory_usage(memory_usage memory, D3D10_USAGE &usage, UINT &cpu_access_flags)
{
	switch (memory)
	{
	default:
	case memory_usage::gpu_only:
		usage = D3D10_USAGE_DEFAULT;
		break;
	case memory_usage::cpu_to_gpu:
		usage = D3D10_USAGE_DYNAMIC;
		cpu_access_flags |= D3D10_CPU_ACCESS_WRITE;
		break;
	case memory_usage::gpu_to_cpu:
		usage = D3D10_USAGE_STAGING;
		cpu_access_flags |= D3D10_CPU_ACCESS_READ;
		break;
	case memory_usage::cpu_only:
		usage = D3D10_USAGE_STAGING;
		cpu_access_flags |= D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE;
		break;
	}
}
memory_usage  reshade::d3d10::convert_memory_usage(D3D10_USAGE usage)
{
	switch (usage)
	{
	default:
	case D3D10_USAGE_DEFAULT:
	case D3D10_USAGE_IMMUTABLE:
		return memory_usage::gpu_only;
	case D3D10_USAGE_DYNAMIC:
		return memory_usage::cpu_to_gpu;
	case D3D10_USAGE_STAGING:
		return memory_usage::gpu_to_cpu;
	}
}

void reshade::d3d10::convert_resource_desc(const resource_desc &desc, D3D10_BUFFER_DESC &internal_desc)
{
	assert(desc.type == resource_type::buffer);
	assert(desc.size <= std::numeric_limits<UINT>::max());
	internal_desc.ByteWidth = static_cast<UINT>(desc.size);
	convert_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
	convert_memory_usage(desc.mem_usage, internal_desc.Usage, internal_desc.CPUAccessFlags);
}
void reshade::d3d10::convert_resource_desc(const resource_desc &desc, D3D10_TEXTURE1D_DESC &internal_desc)
{
	assert(desc.type == resource_type::texture_1d);
	internal_desc.Width = desc.width;
	assert(desc.height == 1);
	internal_desc.MipLevels = desc.levels;
	internal_desc.ArraySize = desc.depth_or_layers;
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.samples == 1);
	convert_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
	convert_memory_usage(desc.mem_usage, internal_desc.Usage, internal_desc.CPUAccessFlags);
}
void reshade::d3d10::convert_resource_desc(const resource_desc &desc, D3D10_TEXTURE2D_DESC &internal_desc)
{
	assert(desc.type == resource_type::texture_2d);
	internal_desc.Width = desc.width;
	internal_desc.Height = desc.height;
	internal_desc.MipLevels = desc.levels;
	internal_desc.ArraySize = desc.depth_or_layers;
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	internal_desc.SampleDesc.Count = desc.samples;
	convert_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
	convert_memory_usage(desc.mem_usage, internal_desc.Usage, internal_desc.CPUAccessFlags);
}
void reshade::d3d10::convert_resource_desc(const resource_desc &desc, D3D10_TEXTURE3D_DESC &internal_desc)
{
	assert(desc.type == resource_type::texture_3d);
	internal_desc.Width = desc.width;
	internal_desc.Height = desc.height;
	internal_desc.Depth = desc.depth_or_layers;
	internal_desc.MipLevels = desc.levels;
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.samples == 1);
	convert_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
	convert_memory_usage(desc.mem_usage, internal_desc.Usage, internal_desc.CPUAccessFlags);
}
resource_desc reshade::d3d10::convert_resource_desc(const D3D10_BUFFER_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.type = resource_type::buffer;
	desc.size = internal_desc.ByteWidth;
	convert_bind_flags_to_usage(internal_desc.BindFlags, desc.usage);
	desc.mem_usage = convert_memory_usage(internal_desc.Usage);
	return desc;
}
resource_desc reshade::d3d10::convert_resource_desc(const D3D10_TEXTURE1D_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.type = resource_type::texture_1d;
	desc.width = internal_desc.Width;
	desc.height = 1;
	assert(internal_desc.ArraySize <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(internal_desc.ArraySize);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.samples = 1;
	convert_bind_flags_to_usage(internal_desc.BindFlags, desc.usage);
	desc.mem_usage = convert_memory_usage(internal_desc.Usage);
	return desc;
}
resource_desc reshade::d3d10::convert_resource_desc(const D3D10_TEXTURE2D_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.type = resource_type::texture_2d;
	desc.width = internal_desc.Width;
	desc.height = internal_desc.Height;
	assert(internal_desc.ArraySize <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(internal_desc.ArraySize);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.samples = static_cast<uint16_t>(internal_desc.SampleDesc.Count);
	convert_bind_flags_to_usage(internal_desc.BindFlags, desc.usage);
	desc.usage |= desc.samples > 1 ? resource_usage::resolve_source : resource_usage::resolve_dest;
	desc.mem_usage = convert_memory_usage(internal_desc.Usage);
	return desc;
}
resource_desc reshade::d3d10::convert_resource_desc(const D3D10_TEXTURE3D_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.type = resource_type::texture_3d;
	desc.width = internal_desc.Width;
	desc.height = internal_desc.Height;
	assert(internal_desc.Depth <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(internal_desc.Depth);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.samples = 1;
	convert_bind_flags_to_usage(internal_desc.BindFlags, desc.usage);
	desc.mem_usage = convert_memory_usage(internal_desc.Usage);
	return desc;
}

void reshade::d3d10::convert_resource_view_desc(const resource_view_desc &desc, D3D10_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.type != resource_view_type::buffer && desc.levels == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	}
}
void reshade::d3d10::convert_resource_view_desc(const resource_view_desc &desc, D3D10_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.type != resource_view_type::buffer && desc.levels == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.first_layer;
		internal_desc.Texture3D.WSize = desc.layers;
		break;
	}
}
void reshade::d3d10::convert_resource_view_desc(const resource_view_desc &desc, D3D10_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case resource_view_type::buffer:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
		assert(desc.offset <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.FirstElement = static_cast<UINT>(desc.offset);
		assert(desc.size <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.NumElements = static_cast<UINT>(desc.size);
		break;
	case resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MostDetailedMip = desc.first_level;
		internal_desc.Texture1D.MipLevels = desc.levels;
		break;
	case resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MostDetailedMip = desc.first_level;
		internal_desc.Texture1DArray.MipLevels = desc.levels;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MostDetailedMip = desc.first_level;
		internal_desc.Texture2D.MipLevels = desc.levels;
		break;
	case resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MostDetailedMip = desc.first_level;
		internal_desc.Texture2DArray.MipLevels = desc.levels;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MostDetailedMip = desc.first_level;
		internal_desc.Texture3D.MipLevels = desc.levels;
		break;
	case resource_view_type::texture_cube:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
		internal_desc.TextureCube.MostDetailedMip = desc.first_level;
		internal_desc.TextureCube.MipLevels = desc.levels;
		break;
	}
}
void reshade::d3d10::convert_resource_view_desc(const resource_view_desc &desc, D3D10_SHADER_RESOURCE_VIEW_DESC1 &internal_desc)
{
	if (desc.type == resource_view_type::texture_cube_array)
	{
		internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
		internal_desc.ViewDimension = D3D10_1_SRV_DIMENSION_TEXTURECUBEARRAY;
		internal_desc.TextureCubeArray.MostDetailedMip = desc.first_level;
		internal_desc.TextureCubeArray.MipLevels = desc.levels;
		internal_desc.TextureCubeArray.First2DArrayFace = desc.first_layer;
		if (desc.layers == 0xFFFFFFFF)
			internal_desc.TextureCubeArray.NumCubes = 0xFFFFFFFF;
		else
			internal_desc.TextureCubeArray.NumCubes = desc.layers / 6;
	}
	else
	{
		convert_resource_view_desc(desc, reinterpret_cast<D3D10_SHADER_RESOURCE_VIEW_DESC &>(internal_desc));
	}
}
resource_view_desc reshade::d3d10::convert_resource_view_desc(const D3D10_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.levels = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D10_DSV_DIMENSION_TEXTURE1D:
		desc.type = resource_view_type::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE1DARRAY:
		desc.type = resource_view_type::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2D:
		desc.type = resource_view_type::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2DARRAY:
		desc.type = resource_view_type::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2DMS:
		desc.type = resource_view_type::texture_2d_multisample;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = resource_view_type::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	}
	return desc;
}
resource_view_desc reshade::d3d10::convert_resource_view_desc(const D3D10_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.levels = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D10_RTV_DIMENSION_TEXTURE1D:
		desc.type = resource_view_type::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE1DARRAY:
		desc.type = resource_view_type::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2D:
		desc.type = resource_view_type::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2DARRAY:
		desc.type = resource_view_type::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2DMS:
		desc.type = resource_view_type::texture_2d_multisample;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = resource_view_type::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE3D:
		desc.type = resource_view_type::texture_3d;
		desc.first_level = internal_desc.Texture3D.MipSlice;
		desc.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.layers = internal_desc.Texture3D.WSize;
		break;
	}
	return desc;
}
resource_view_desc reshade::d3d10::convert_resource_view_desc(const D3D10_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	switch (internal_desc.ViewDimension)
	{
	case D3D10_SRV_DIMENSION_BUFFER:
		desc.type = resource_view_type::buffer;
		desc.offset = internal_desc.Buffer.FirstElement;
		desc.size = internal_desc.Buffer.NumElements;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE1D:
		desc.type = resource_view_type::texture_1d;
		desc.first_level = internal_desc.Texture1D.MostDetailedMip;
		desc.levels = internal_desc.Texture1D.MipLevels;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE1DARRAY:
		desc.type = resource_view_type::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MostDetailedMip;
		desc.levels = internal_desc.Texture1DArray.MipLevels;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2D:
		desc.type = resource_view_type::texture_2d;
		desc.first_level = internal_desc.Texture2D.MostDetailedMip;
		desc.levels = internal_desc.Texture2D.MipLevels;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2DARRAY:
		desc.type = resource_view_type::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MostDetailedMip;
		desc.levels = internal_desc.Texture2DArray.MipLevels;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2DMS:
		desc.type = resource_view_type::texture_2d_multisample;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = resource_view_type::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE3D:
		desc.type = resource_view_type::texture_3d;
		desc.first_level = internal_desc.Texture3D.MostDetailedMip;
		desc.levels = internal_desc.Texture3D.MipLevels;
		break;
	case D3D10_SRV_DIMENSION_TEXTURECUBE:
		desc.type = resource_view_type::texture_cube;
		desc.first_level = internal_desc.TextureCube.MostDetailedMip;
		desc.levels = internal_desc.TextureCube.MipLevels;
		break;
	}
	return desc;
}
resource_view_desc reshade::d3d10::convert_resource_view_desc(const D3D10_SHADER_RESOURCE_VIEW_DESC1 &internal_desc)
{
	if (internal_desc.ViewDimension == D3D10_1_SRV_DIMENSION_TEXTURECUBEARRAY)
	{
		resource_view_desc desc = {};
		desc.type = resource_view_type::texture_cube_array;
		desc.format = static_cast<uint32_t>(internal_desc.Format);
		desc.first_level = internal_desc.TextureCubeArray.MostDetailedMip;
		desc.levels = internal_desc.TextureCubeArray.MipLevels;
		desc.first_layer = internal_desc.TextureCubeArray.First2DArrayFace;
		if (internal_desc.TextureCubeArray.NumCubes == 0xFFFFFFFF)
			desc.layers = 0xFFFFFFFF;
		else
			desc.layers = internal_desc.TextureCubeArray.NumCubes * 6;
		return desc;
	}
	else
	{
		return convert_resource_view_desc(reinterpret_cast<const D3D10_SHADER_RESOURCE_VIEW_DESC &>(internal_desc));
	}
}
