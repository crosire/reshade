/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d11.hpp"
#include "dll_resources.hpp"

using namespace reshade::api;

static inline void convert_usage_to_bind_flags(const resource_usage usage, UINT &bind_flags)
{
	if ((usage & resource_usage::render_target) != 0)
		bind_flags |= D3D11_BIND_RENDER_TARGET;
	else
		bind_flags &= ~D3D11_BIND_RENDER_TARGET;

	if ((usage & resource_usage::depth_stencil) != 0)
		bind_flags |= D3D11_BIND_DEPTH_STENCIL;
	else
		bind_flags &= ~D3D11_BIND_DEPTH_STENCIL;

	if ((usage & resource_usage::shader_resource) != 0)
		bind_flags |= D3D11_BIND_SHADER_RESOURCE;
	else
		bind_flags &= ~D3D11_BIND_SHADER_RESOURCE;

	if ((usage & resource_usage::unordered_access) != 0)
		bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
	else
		bind_flags &= ~D3D11_BIND_UNORDERED_ACCESS;

	if ((usage & resource_usage::index_buffer) != 0)
		bind_flags |= D3D11_BIND_INDEX_BUFFER;
	else
		bind_flags &= ~D3D11_BIND_INDEX_BUFFER;

	if ((usage & resource_usage::vertex_buffer) != 0)
		bind_flags |= D3D11_BIND_VERTEX_BUFFER;
	else
		bind_flags &= ~D3D11_BIND_VERTEX_BUFFER;

	if ((usage & resource_usage::constant_buffer) != 0)
		bind_flags |= D3D11_BIND_CONSTANT_BUFFER;
	else
		bind_flags &= ~D3D11_BIND_CONSTANT_BUFFER;
}
static inline void convert_bind_flags_to_usage(const UINT bind_flags, resource_usage &usage)
{
	// Resources are generally copyable in D3D11
	usage |= resource_usage::copy_dest | resource_usage::copy_source;

	if ((bind_flags & D3D11_BIND_RENDER_TARGET) != 0)
		usage |= resource_usage::render_target;
	if ((bind_flags & D3D11_BIND_DEPTH_STENCIL) != 0)
		usage |= resource_usage::depth_stencil;
	if ((bind_flags & D3D11_BIND_SHADER_RESOURCE) != 0)
		usage |= resource_usage::shader_resource;
	if ((bind_flags & D3D11_BIND_UNORDERED_ACCESS) != 0)
		usage |= resource_usage::unordered_access;

	if ((bind_flags & D3D11_BIND_INDEX_BUFFER) != 0)
		usage |= resource_usage::index_buffer;
	if ((bind_flags & D3D11_BIND_VERTEX_BUFFER) != 0)
		usage |= resource_usage::vertex_buffer;
	if ((bind_flags & D3D11_BIND_CONSTANT_BUFFER) != 0)
		usage |= resource_usage::constant_buffer;
}

void reshade::d3d11::convert_resource_desc(const resource_desc &desc, D3D11_BUFFER_DESC &internal_desc)
{
	assert(desc.height == 0);
	internal_desc.ByteWidth = desc.width;
	convert_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
}
void reshade::d3d11::convert_resource_desc(const resource_desc &desc, D3D11_TEXTURE1D_DESC &internal_desc)
{
	internal_desc.Width = desc.width;
	assert(desc.height == 1);
	internal_desc.MipLevels = desc.levels;
	internal_desc.ArraySize = desc.depth_or_layers;
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.samples == 1);
	convert_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
}
void reshade::d3d11::convert_resource_desc(const resource_desc &desc, D3D11_TEXTURE2D_DESC &internal_desc)
{
	internal_desc.Width = desc.width;
	internal_desc.Height = desc.height;
	internal_desc.MipLevels = desc.levels;
	internal_desc.ArraySize = desc.depth_or_layers;
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	internal_desc.SampleDesc.Count = desc.samples;
	convert_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
}
void reshade::d3d11::convert_resource_desc(const resource_desc &desc, D3D11_TEXTURE3D_DESC &internal_desc)
{
	internal_desc.Width = desc.width;
	internal_desc.Height = desc.height;
	internal_desc.Depth = desc.depth_or_layers;
	internal_desc.MipLevels = desc.levels;
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.samples == 1);
	convert_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
}
resource_desc reshade::d3d11::convert_resource_desc(const D3D11_BUFFER_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.width = internal_desc.ByteWidth;
	convert_bind_flags_to_usage(internal_desc.BindFlags, desc.usage);
	return desc;
}
resource_desc reshade::d3d11::convert_resource_desc(const D3D11_TEXTURE1D_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.width = internal_desc.Width;
	desc.height = 1;
	assert(internal_desc.ArraySize <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(internal_desc.ArraySize);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.samples = 1;
	convert_bind_flags_to_usage(internal_desc.BindFlags, desc.usage);
	return desc;
}
resource_desc reshade::d3d11::convert_resource_desc(const D3D11_TEXTURE2D_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.width = internal_desc.Width;
	desc.height = internal_desc.Height;
	assert(internal_desc.ArraySize <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(internal_desc.ArraySize);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.samples = static_cast<uint16_t>(internal_desc.SampleDesc.Count);
	convert_bind_flags_to_usage(internal_desc.BindFlags, desc.usage);
	return desc;
}
resource_desc reshade::d3d11::convert_resource_desc(const D3D11_TEXTURE3D_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.width = internal_desc.Width;
	desc.height = internal_desc.Height;
	assert(internal_desc.Depth <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(internal_desc.Depth);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.samples = 1;
	convert_bind_flags_to_usage(internal_desc.BindFlags, desc.usage);
	return desc;
}

void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D11_DEPTH_STENCIL_VIEW_DESC::Flags
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.dimension != resource_view_dimension::buffer && desc.levels == 1);
	switch (desc.dimension) // Do not modifiy description in case dimension is 'resource_view_dimension::unknown'
	{
	case resource_view_dimension::texture_1d:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_1d_array:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_2d_array:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d_multisample:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_dimension::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.dimension != resource_view_dimension::buffer && desc.levels == 1);
	switch (desc.dimension) // Do not modifiy description in case dimension is 'resource_view_dimension::unknown'
	{
	case resource_view_dimension::texture_1d:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_1d_array:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_2d_array:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d_multisample:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_dimension::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_3d:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.first_layer;
		internal_desc.Texture3D.WSize = desc.layers;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_RENDER_TARGET_VIEW_DESC1 &internal_desc)
{
	if (desc.dimension == resource_view_dimension::texture_2d || desc.dimension == resource_view_dimension::texture_2d_array)
	{
		internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
		assert(desc.dimension != resource_view_dimension::buffer && desc.levels == 1);
		switch (desc.dimension)
		{
		case resource_view_dimension::texture_2d:
			internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			internal_desc.Texture2D.MipSlice = desc.first_level;
			// Missing fields: D3D11_TEX2D_RTV1::PlaneSlice
			break;
		case resource_view_dimension::texture_2d_array:
			internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			internal_desc.Texture2DArray.MipSlice = desc.first_level;
			internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
			internal_desc.Texture2DArray.ArraySize = desc.layers;
			break;
			// Missing fields: D3D11_TEX2D_ARRAY_RTV1::PlaneSlice
		}
	}
	else
	{
		convert_resource_view_desc(desc, reinterpret_cast<D3D11_RENDER_TARGET_VIEW_DESC &>(internal_desc));
	}
}
void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	switch (desc.dimension) // Do not modifiy description in case dimension is 'resource_view_dimension::unknown'
	{
	case resource_view_dimension::buffer:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		assert(desc.first_layer == 0 && desc.layers == 0);
		internal_desc.Buffer.FirstElement = desc.first_level;
		internal_desc.Buffer.NumElements = desc.levels;
		break;
	case resource_view_dimension::texture_1d:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MostDetailedMip = desc.first_level;
		internal_desc.Texture1D.MipLevels = desc.levels;
		break;
	case resource_view_dimension::texture_1d_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MostDetailedMip = desc.first_level;
		internal_desc.Texture1DArray.MipLevels = desc.levels;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MostDetailedMip = desc.first_level;
		internal_desc.Texture2D.MipLevels = desc.levels;
		break;
	case resource_view_dimension::texture_2d_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MostDetailedMip = desc.first_level;
		internal_desc.Texture2DArray.MipLevels = desc.levels;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d_multisample:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_dimension::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_3d:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MostDetailedMip = desc.first_level;
		internal_desc.Texture3D.MipLevels = desc.levels;
		break;
	case resource_view_dimension::texture_cube:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		internal_desc.TextureCube.MostDetailedMip = desc.first_level;
		internal_desc.TextureCube.MipLevels = desc.levels;
		break;
	case resource_view_dimension::texture_cube_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
		internal_desc.TextureCubeArray.MostDetailedMip = desc.first_level;
		internal_desc.TextureCubeArray.MipLevels = desc.levels;
		internal_desc.TextureCubeArray.First2DArrayFace = desc.first_layer;
		internal_desc.TextureCubeArray.NumCubes = desc.layers / 6;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_SHADER_RESOURCE_VIEW_DESC1 &internal_desc)
{
	if (desc.dimension == resource_view_dimension::texture_2d || desc.dimension == resource_view_dimension::texture_2d_array)
	{
		internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
		switch (desc.dimension)
		{
		case resource_view_dimension::texture_2d:
			internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			internal_desc.Texture2D.MostDetailedMip = desc.first_level;
			internal_desc.Texture2D.MipLevels = desc.levels;
			// Missing fields: D3D11_TEX2D_SRV1::PlaneSlice
			break;
		case resource_view_dimension::texture_2d_array:
			internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			internal_desc.Texture2DArray.MostDetailedMip = desc.first_level;
			internal_desc.Texture2DArray.MipLevels = desc.levels;
			internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
			internal_desc.Texture2DArray.ArraySize = desc.layers;
			break;
			// Missing fields: D3D11_TEX2D_ARRAY_SRV1::PlaneSlice
		}
	}
	else
	{
		convert_resource_view_desc(desc, reinterpret_cast<D3D11_SHADER_RESOURCE_VIEW_DESC &>(internal_desc));
	}
}
void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_UNORDERED_ACCESS_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.dimension == resource_view_dimension::buffer || desc.levels == 1);
	switch (desc.dimension) // Do not modifiy description in case dimension is 'resource_view_dimension::unknown'
	{
	case resource_view_dimension::buffer:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		assert(desc.first_layer == 0 && desc.layers == 0);
		internal_desc.Buffer.FirstElement = desc.first_level;
		internal_desc.Buffer.NumElements = desc.levels;
		break;
	case resource_view_dimension::texture_1d:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_1d_array:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_2d_array:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_3d:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.first_layer;
		internal_desc.Texture3D.WSize = desc.layers;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_UNORDERED_ACCESS_VIEW_DESC1 &internal_desc)
{
	if (desc.dimension == resource_view_dimension::texture_2d || desc.dimension == resource_view_dimension::texture_2d_array)
	{
		internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
		assert(desc.dimension == resource_view_dimension::buffer || desc.levels == 1);
		switch (desc.dimension)
		{
		case resource_view_dimension::texture_2d:
			internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			internal_desc.Texture2D.MipSlice = desc.first_level;
			// Missing fields: D3D11_TEX2D_UAV1::PlaneSlice
			break;
		case resource_view_dimension::texture_2d_array:
			internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
			internal_desc.Texture2DArray.MipSlice = desc.first_level;
			internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
			internal_desc.Texture2DArray.ArraySize = desc.layers;
			// Missing fields: D3D11_TEX2D_ARRAY_UAV1::PlaneSlice
			break;
		}
	}
	else
	{
		convert_resource_view_desc(desc, reinterpret_cast<D3D11_UNORDERED_ACCESS_VIEW_DESC &>(internal_desc));
	}
}
resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D11_DEPTH_STENCIL_VIEW_DESC::Flags
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.levels = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D11_DSV_DIMENSION_TEXTURE1D:
		desc.dimension = resource_view_dimension::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
		desc.dimension = resource_view_dimension::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2D:
		desc.dimension = resource_view_dimension::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
		desc.dimension = resource_view_dimension::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2DMS:
		desc.dimension = resource_view_dimension::texture_2d_multisample;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
		desc.dimension = resource_view_dimension::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	}
	return desc;
}
resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.levels = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D11_RTV_DIMENSION_TEXTURE1D:
		desc.dimension = resource_view_dimension::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
		desc.dimension = resource_view_dimension::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2D:
		desc.dimension = resource_view_dimension::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
		desc.dimension = resource_view_dimension::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2DMS:
		desc.dimension = resource_view_dimension::texture_2d_multisample;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
		desc.dimension = resource_view_dimension::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE3D:
		desc.dimension = resource_view_dimension::texture_3d;
		desc.first_level = internal_desc.Texture3D.MipSlice;
		desc.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.layers = internal_desc.Texture3D.WSize;
		break;
	}
	return desc;
}
resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_RENDER_TARGET_VIEW_DESC1 &internal_desc)
{
	if (internal_desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D || internal_desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
	{
		resource_view_desc desc = {};
		desc.format = static_cast<uint32_t>(internal_desc.Format);
		desc.levels = 1;
		switch (internal_desc.ViewDimension)
		{
		case D3D11_RTV_DIMENSION_TEXTURE2D:
			desc.dimension = resource_view_dimension::texture_2d;
			desc.first_level = internal_desc.Texture2D.MipSlice;
			// Missing fields: D3D11_TEX2D_RTV1::PlaneSlice
			break;
		case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
			desc.dimension = resource_view_dimension::texture_2d_array;
			desc.first_level = internal_desc.Texture2DArray.MipSlice;
			desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
			desc.layers = internal_desc.Texture2DArray.ArraySize;
			// Missing fields: D3D11_TEX2D_ARRAY_RTV1::PlaneSlice
			break;
		}
		return desc;
	}
	else
	{
		return convert_resource_view_desc(reinterpret_cast<const D3D11_RENDER_TARGET_VIEW_DESC &>(internal_desc));
	}
}
resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	switch (internal_desc.ViewDimension)
	{
	case D3D11_SRV_DIMENSION_BUFFER:
		desc.dimension = resource_view_dimension::buffer;
		desc.first_level = internal_desc.Buffer.FirstElement;
		desc.levels = internal_desc.Buffer.NumElements;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE1D:
		desc.dimension = resource_view_dimension::texture_1d;
		desc.first_level = internal_desc.Texture1D.MostDetailedMip;
		desc.levels = internal_desc.Texture1D.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
		desc.dimension = resource_view_dimension::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MostDetailedMip;
		desc.levels = internal_desc.Texture1DArray.MipLevels;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2D:
		desc.dimension = resource_view_dimension::texture_2d;
		desc.first_level = internal_desc.Texture2D.MostDetailedMip;
		desc.levels = internal_desc.Texture2D.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
		desc.dimension = resource_view_dimension::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MostDetailedMip;
		desc.levels = internal_desc.Texture2DArray.MipLevels;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2DMS:
		desc.dimension = resource_view_dimension::texture_2d_multisample;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
		desc.dimension = resource_view_dimension::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE3D:
		desc.dimension = resource_view_dimension::texture_3d;
		desc.first_level = internal_desc.Texture3D.MostDetailedMip;
		desc.levels = internal_desc.Texture3D.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURECUBE:
		desc.dimension = resource_view_dimension::texture_cube;
		desc.first_level = internal_desc.TextureCube.MostDetailedMip;
		desc.levels = internal_desc.TextureCube.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
		desc.dimension = resource_view_dimension::texture_cube_array;
		desc.first_level = internal_desc.TextureCubeArray.MostDetailedMip;
		desc.levels = internal_desc.TextureCubeArray.MipLevels;
		desc.first_layer = internal_desc.TextureCubeArray.First2DArrayFace;
		desc.layers = internal_desc.TextureCubeArray.NumCubes * 6;
		break;
	case D3D11_SRV_DIMENSION_BUFFEREX:
		// Do not set dimension to 'resource_view_dimension::buffer', so that flags field is kept later during conversion into the other direction
		desc.first_level = internal_desc.BufferEx.FirstElement;
		desc.levels = internal_desc.BufferEx.NumElements;
		// Missing fields: D3D11_BUFFEREX_SRV::Flags
		break;
	}
	return desc;
}
resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_SHADER_RESOURCE_VIEW_DESC1 &internal_desc)
{
	if (internal_desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D || internal_desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
	{
		resource_view_desc desc = {};
		desc.format = static_cast<uint32_t>(internal_desc.Format);
		switch (internal_desc.ViewDimension)
		{
		case D3D11_SRV_DIMENSION_TEXTURE2D:
			desc.dimension = resource_view_dimension::texture_2d;
			desc.first_level = internal_desc.Texture2D.MostDetailedMip;
			desc.levels = internal_desc.Texture2D.MipLevels;
			// Missing fields: D3D11_TEX2D_SRV1::PlaneSlice
			break;
		case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
			desc.dimension = resource_view_dimension::texture_2d_array;
			desc.first_level = internal_desc.Texture2DArray.MostDetailedMip;
			desc.levels = internal_desc.Texture2DArray.MipLevels;
			desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
			desc.layers = internal_desc.Texture2DArray.ArraySize;
			// Missing fields: D3D11_TEX2D_ARRAY_SRV1::PlaneSlice
			break;
		}
		return desc;
	}
	else
	{
		return convert_resource_view_desc(reinterpret_cast<const D3D11_SHADER_RESOURCE_VIEW_DESC &>(internal_desc));
	}
}
resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_UNORDERED_ACCESS_VIEW_DESC &internal_desc)
{
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.levels = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D11_UAV_DIMENSION_BUFFER:
		desc.dimension = resource_view_dimension::buffer;
		desc.first_level = internal_desc.Buffer.FirstElement;
		desc.levels = internal_desc.Buffer.NumElements;
		// Missing fields: D3D11_BUFFER_UAV::Flags
		break;
	case D3D11_UAV_DIMENSION_TEXTURE1D:
		desc.dimension = resource_view_dimension::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
		desc.dimension = resource_view_dimension::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE2D:
		desc.dimension = resource_view_dimension::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
		desc.dimension = resource_view_dimension::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE3D:
		desc.dimension = resource_view_dimension::texture_3d;
		desc.first_level = internal_desc.Texture3D.MipSlice;
		desc.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.layers = internal_desc.Texture3D.WSize;
		break;
	}
	return desc;
}
resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 &internal_desc)
{
	if (internal_desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D || internal_desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
	{
		resource_view_desc desc = {};
		desc.format = static_cast<uint32_t>(internal_desc.Format);
		desc.levels = 1;
		switch (internal_desc.ViewDimension)
		{
		case D3D11_UAV_DIMENSION_TEXTURE2D:
			desc.dimension = resource_view_dimension::texture_2d;
			desc.first_level = internal_desc.Texture2D.MipSlice;
			// Missing fields: D3D11_TEX2D_UAV1::PlaneSlice
			break;
		case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
			desc.dimension = resource_view_dimension::texture_2d_array;
			desc.first_level = internal_desc.Texture2DArray.MipSlice;
			desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
			desc.layers = internal_desc.Texture2DArray.ArraySize;
			// Missing fields: D3D11_TEX2D_ARRAY_UAV1::PlaneSlice
			break;
		}
		return desc;
	}
	else
	{
		return convert_resource_view_desc(reinterpret_cast<const D3D11_UNORDERED_ACCESS_VIEW_DESC &>(internal_desc));
	}
}

reshade::d3d11::device_impl::device_impl(ID3D11Device *device) :
	_orig(device)
{
	// Create copy states
	const resources::data_resource ps = resources::load_data_resource(IDR_COPY_PS);
	if (FAILED(_orig->CreatePixelShader(ps.data, ps.data_size, nullptr, &_copy_pixel_shader)))
		return;
	const resources::data_resource vs = resources::load_data_resource(IDR_FULLSCREEN_VS);
	if (FAILED(_orig->CreateVertexShader(vs.data, vs.data_size, nullptr, &_copy_vertex_shader)))
		return;

	{   D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

		if (FAILED(_orig->CreateSamplerState(&desc, &_copy_sampler_state)))
			return;
	}

#if RESHADE_ADDON
	// Load and initialize add-ons
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

bool reshade::d3d11::device_impl::check_format_support(uint32_t format, resource_usage usage) const
{
	UINT support = 0;
	if (FAILED(_orig->CheckFormatSupport(static_cast<DXGI_FORMAT>(format), &support)))
		return false;

	if ((usage & resource_usage::render_target) != 0 &&
		(support & D3D11_FORMAT_SUPPORT_RENDER_TARGET) == 0)
		return false;
	if ((usage & resource_usage::depth_stencil) != 0 &&
		(support & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) == 0)
		return false;
	if ((usage & resource_usage::shader_resource) != 0 &&
		(support & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) == 0)
		return false;
	if ((usage & resource_usage::unordered_access) != 0 &&
		(support & D3D11_FORMAT_SUPPORT_SHADER_LOAD) == 0)
		return false;
	if ((usage & (resource_usage::resolve_source | resource_usage::resolve_dest)) != 0 &&
		(support & D3D10_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE) == 0)
		return false;

	return true;
}

bool reshade::d3d11::device_impl::check_resource_handle_valid(resource_handle resource) const
{
	return resource.handle != 0 && _resources.has_object(reinterpret_cast<ID3D11Resource *>(resource.handle));
}
bool reshade::d3d11::device_impl::check_resource_view_handle_valid(resource_view_handle view) const
{
	return view.handle != 0 && _views.has_object(reinterpret_cast<ID3D11View *>(view.handle));
}

bool reshade::d3d11::device_impl::create_resource(resource_type type, const resource_desc &desc, resource_usage, resource_handle *out_resource)
{
	switch (type)
	{
		case resource_type::buffer:
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
		case resource_type::texture_1d:
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
		case resource_type::texture_2d:
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
		case resource_type::texture_3d:
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
bool reshade::d3d11::device_impl::create_resource_view(resource_handle resource, resource_view_type type, const resource_view_desc &desc, resource_view_handle *out_view)
{
	assert(resource.handle != 0);

	switch (type)
	{
		case resource_view_type::depth_stencil:
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
		case resource_view_type::render_target:
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
		case resource_view_type::shader_resource:
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
		case resource_view_type::unordered_access:
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

void reshade::d3d11::device_impl::destroy_resource(resource_handle resource)
{
	assert(resource.handle != 0);
	reinterpret_cast<ID3D11Resource *>(resource.handle)->Release();
}
void reshade::d3d11::device_impl::destroy_resource_view(resource_view_handle view)
{
	assert(view.handle != 0);
	reinterpret_cast<ID3D11View *>(view.handle)->Release();
}

void reshade::d3d11::device_impl::get_resource_from_view(resource_view_handle view, resource_handle *out_resource) const
{
	assert(view.handle != 0);
	com_ptr<ID3D11Resource> resource;
	reinterpret_cast<ID3D11View *>(view.handle)->GetResource(&resource);

	*out_resource = { reinterpret_cast<uintptr_t>(resource.get()) };
}

resource_desc reshade::d3d11::device_impl::get_resource_desc(resource_handle resource) const
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

reshade::d3d11::command_list_impl::command_list_impl(device_impl *device, ID3D11CommandList *list) :
	_device_impl(device), _orig(list)
{
	RESHADE_ADDON_EVENT(init_command_list, this);
}
reshade::d3d11::command_list_impl::~command_list_impl()
{
	RESHADE_ADDON_EVENT(destroy_command_list, this);
}

reshade::d3d11::device_context_impl::device_context_impl(device_impl *device, ID3D11DeviceContext *context) :
	_device_impl(device), _orig(context)
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

void reshade::d3d11::device_context_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	if (instances <= 1)
		_orig->Draw(vertices, first_vertex);
	else
		_orig->DrawInstanced(vertices, instances, first_vertex, first_instance);
}
void reshade::d3d11::device_context_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	if (instances <= 1)
		_orig->DrawIndexed(indices, first_index, vertex_offset);
	else
		_orig->DrawIndexedInstanced(indices, instances, first_index, vertex_offset, first_instance);
}

void reshade::d3d11::device_context_impl::copy_resource(resource_handle source, resource_handle destination)
{
	assert(source.handle != 0 && destination.handle != 0);
	_orig->CopyResource(reinterpret_cast<ID3D11Resource *>(destination.handle), reinterpret_cast<ID3D11Resource *>(source.handle));
}

void reshade::d3d11::device_context_impl::clear_depth_stencil_view(resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	assert(dsv.handle != 0);
	_orig->ClearDepthStencilView(reinterpret_cast<ID3D11DepthStencilView *>(dsv.handle), clear_flags, depth, stencil);
}
void reshade::d3d11::device_context_impl::clear_render_target_view(resource_view_handle rtv, const float color[4])
{
	assert(rtv.handle != 0);
	_orig->ClearRenderTargetView(reinterpret_cast<ID3D11RenderTargetView *>(rtv.handle), color);
}
