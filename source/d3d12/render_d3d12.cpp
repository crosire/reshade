/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "render_d3d12.hpp"

using namespace reshade::api;

static inline D3D12_RESOURCE_STATES convert_resource_usage_to_states(resource_usage usage)
{
	auto result = static_cast<D3D12_RESOURCE_STATES>(usage);

	// Depth write state is mutually exclusive with other states, so remove it when read state is specified too
	if ((usage & resource_usage::depth_stencil) == resource_usage::depth_stencil)
	{
		result ^= D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	// The separate constant buffer state does not exist in D3D12, so replace it with the combined vertex/constant buffer one
	if ((usage & resource_usage::constant_buffer) != 0)
	{
		result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		result ^= static_cast<D3D12_RESOURCE_STATES>(resource_usage::constant_buffer);
	}

	return result;
}

void reshade::d3d12::convert_resource_desc(resource_type type, const resource_desc &desc, D3D12_RESOURCE_DESC &internal_desc)
{
	switch (type)
	{
	default:
		assert(false);
		internal_desc.Dimension = D3D12_RESOURCE_DIMENSION_UNKNOWN;
		break;
	case resource_type::buffer:
		internal_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		break;
	case resource_type::texture_1d:
		internal_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
		break;
	case resource_type::texture_2d:
		internal_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		break;
	case resource_type::texture_3d:
		internal_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		break;
	}

	if (type == resource_type::buffer)
	{
		internal_desc.Width = desc.width | (static_cast<uint64_t>(desc.height) << 32);
		internal_desc.Height = 1;
		internal_desc.DepthOrArraySize = 1;
		internal_desc.MipLevels = 1;
		internal_desc.SampleDesc.Count = 1;
	}
	else
	{
		internal_desc.Width = desc.width;
		internal_desc.Height = desc.height;
		internal_desc.DepthOrArraySize = desc.depth_or_layers;
		internal_desc.MipLevels = desc.levels;
		internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
		internal_desc.SampleDesc.Count = desc.samples;
	}

	if ((desc.usage & resource_usage::render_target) != 0)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	if ((desc.usage & resource_usage::depth_stencil) != 0)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	if ((desc.usage & resource_usage::shader_resource) == 0 && (internal_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	if ((desc.usage & resource_usage::unordered_access) != 0)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
}
std::pair<resource_type, resource_desc> reshade::d3d12::convert_resource_desc(const D3D12_RESOURCE_DESC &internal_desc)
{
	resource_type type;
	switch (internal_desc.Dimension)
	{
	default:
		assert(false);
		type = resource_type::unknown;
		break;
	case D3D12_RESOURCE_DIMENSION_BUFFER:
		type = resource_type::buffer;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		type = resource_type::texture_1d;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		type = resource_type::texture_2d;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		type = resource_type::texture_3d;
		break;
	}

	resource_desc desc = {};

	if (type == resource_type::buffer)
	{
		desc.width = internal_desc.Width & 0xFFFFFFFF;
		desc.height = (internal_desc.Width >> 32) & 0xFFFFFFFF;

		// Buffers may be of any type
		desc.usage |= resource_usage::vertex_buffer | resource_usage::index_buffer | resource_usage::constant_buffer;
	}
	else
	{
		assert(internal_desc.Width <= std::numeric_limits<uint32_t>::max());
		desc.width = static_cast<uint32_t>(internal_desc.Width);
		desc.height = internal_desc.Height;
		desc.levels = internal_desc.MipLevels;
		desc.depth_or_layers = internal_desc.DepthOrArraySize;
		desc.format = static_cast<uint32_t>(internal_desc.Format);
		desc.samples = static_cast<uint16_t>(internal_desc.SampleDesc.Count);
	}

	// Resources are generally copyable in D3D12
	desc.usage |= resource_usage::copy_dest | resource_usage::copy_source;

	if ((internal_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0)
		desc.usage |= resource_usage::render_target;
	if ((internal_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0)
		desc.usage |= resource_usage::depth_stencil;
	if ((internal_desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0)
		desc.usage |= resource_usage::shader_resource;
	if ((internal_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0)
		desc.usage |= resource_usage::unordered_access;

	return { type, desc };
}

void reshade::d3d12::convert_resource_view_desc(const resource_view_desc &desc, D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_DEPTH_STENCIL_VIEW_DESC::Flags
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.dimension != resource_view_dimension::buffer && desc.levels == 1);
	switch (desc.dimension) // Do not modifiy description in case dimension is 'resource_view_dimension::unknown'
	{
	case resource_view_dimension::texture_1d:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_1d_array:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_2d_array:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d_multisample:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_dimension::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	}
}
void reshade::d3d12::convert_resource_view_desc(const resource_view_desc &desc, D3D12_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.dimension != resource_view_dimension::buffer && desc.levels == 1);
	switch (desc.dimension) // Do not modifiy description in case dimension is 'resource_view_dimension::unknown'
	{
	case resource_view_dimension::texture_1d:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_1d_array:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_2d_array:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d_multisample:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_dimension::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_3d:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.first_layer;
		internal_desc.Texture3D.WSize = desc.layers;
		break;
	}
}
void reshade::d3d12::convert_resource_view_desc(const resource_view_desc &desc, D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_SHADER_RESOURCE_VIEW_DESC::Shader4ComponentMapping
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	switch (desc.dimension) // Do not modifiy description in case dimension is 'resource_view_dimension::unknown'
	{
	case resource_view_dimension::buffer:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		internal_desc.Buffer.FirstElement = desc.first_level | (static_cast<uint64_t>(desc.first_layer) << 32);
		assert(desc.layers == 0);
		internal_desc.Buffer.NumElements = desc.levels;
		// Missing fields: D3D12_BUFFER_SRV::StructureByteStride, D3D12_BUFFER_SRV::Flags
		break;
	case resource_view_dimension::texture_1d:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MostDetailedMip = desc.first_level;
		internal_desc.Texture1D.MipLevels = desc.levels;
		// Missing fields: D3D12_TEX1D_SRV::ResourceMinLODClamp
		break;
	case resource_view_dimension::texture_1d_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MostDetailedMip = desc.first_level;
		internal_desc.Texture1DArray.MipLevels = desc.levels;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		// Missing fields: D3D12_TEX1D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case resource_view_dimension::texture_2d:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MostDetailedMip = desc.first_level;
		internal_desc.Texture2D.MipLevels = desc.levels;
		// Missing fields: D3D12_TEX2D_SRV::PlaneSlice, D3D12_TEX2D_SRV::ResourceMinLODClamp
		break;
	case resource_view_dimension::texture_2d_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MostDetailedMip = desc.first_level;
		internal_desc.Texture2DArray.MipLevels = desc.levels;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		// Missing fields D3D12_TEX2D_ARRAY_SRV::PlaneSlice, D3D12_TEX2D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case resource_view_dimension::texture_2d_multisample:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_dimension::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_3d:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MostDetailedMip = desc.first_level;
		internal_desc.Texture3D.MipLevels = desc.levels;
		// Missing fields: D3D12_TEX3D_SRV::ResourceMinLODClamp
		break;
	case resource_view_dimension::texture_cube:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		internal_desc.TextureCube.MostDetailedMip = desc.first_level;
		internal_desc.TextureCube.MipLevels = desc.levels;
		// Missing fields: D3D12_TEXCUBE_SRV::ResourceMinLODClamp
		break;
	case resource_view_dimension::texture_cube_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
		internal_desc.TextureCubeArray.MostDetailedMip = desc.first_level;
		internal_desc.TextureCubeArray.MipLevels = desc.levels;
		internal_desc.TextureCubeArray.First2DArrayFace = desc.first_layer;
		internal_desc.TextureCubeArray.NumCubes = desc.layers / 6;
		// Missing fields: D3D12_TEXCUBE_ARRAY_SRV::ResourceMinLODClamp
		break;
	}
}
void reshade::d3d12::convert_resource_view_desc(const resource_view_desc &desc, D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.dimension == resource_view_dimension::buffer || desc.levels == 1);
	switch (desc.dimension) // Do not modifiy description in case dimension is 'resource_view_dimension::unknown'
	{
	case resource_view_dimension::buffer:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		internal_desc.Buffer.FirstElement = desc.first_level | (static_cast<uint64_t>(desc.first_layer) << 32);
		assert(desc.layers == 0);
		internal_desc.Buffer.NumElements = desc.levels;
		// Missing fields: D3D12_BUFFER_UAV::StructureByteStride, D3D12_BUFFER_UAV::CounterOffsetInBytes, D3D12_BUFFER_UAV::Flags
		break;
	case resource_view_dimension::texture_1d:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_1d_array:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_2d:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_dimension::texture_2d_array:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_dimension::texture_3d:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.first_layer;
		internal_desc.Texture3D.WSize = desc.layers;
		break;
	}
}
resource_view_desc reshade::d3d12::convert_resource_view_desc(const D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_DEPTH_STENCIL_VIEW_DESC::Flags
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.levels = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D12_DSV_DIMENSION_TEXTURE1D:
		desc.dimension = resource_view_dimension::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
		desc.dimension = resource_view_dimension::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2D:
		desc.dimension = resource_view_dimension::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
		desc.dimension = resource_view_dimension::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2DMS:
		desc.dimension = resource_view_dimension::texture_2d_multisample;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
		desc.dimension = resource_view_dimension::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	}
	return desc;
}
resource_view_desc reshade::d3d12::convert_resource_view_desc(const D3D12_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.levels = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D12_RTV_DIMENSION_TEXTURE1D:
		desc.dimension = resource_view_dimension::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
		desc.dimension = resource_view_dimension::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2D:
		desc.dimension = resource_view_dimension::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
		desc.dimension = resource_view_dimension::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2DMS:
		desc.dimension = resource_view_dimension::texture_2d_multisample;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
		desc.dimension = resource_view_dimension::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE3D:
		desc.dimension = resource_view_dimension::texture_3d;
		desc.first_level = internal_desc.Texture3D.MipSlice;
		desc.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.layers = internal_desc.Texture3D.WSize;
		break;
	}
	return desc;
}
resource_view_desc reshade::d3d12::convert_resource_view_desc(const D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_SHADER_RESOURCE_VIEW_DESC::Shader4ComponentMapping
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	switch (internal_desc.ViewDimension)
	{
	case D3D12_SRV_DIMENSION_BUFFER:
		desc.dimension = resource_view_dimension::buffer;
		desc.first_level = internal_desc.Buffer.FirstElement & 0xFFFFFFFF;
		desc.first_layer = (internal_desc.Buffer.FirstElement >> 32) & 0xFFFFFFFF;
		desc.levels = internal_desc.Buffer.NumElements;
		// Missing fields: D3D12_BUFFER_SRV::StructureByteStride, D3D12_BUFFER_SRV::Flags
		break;
	case D3D12_SRV_DIMENSION_TEXTURE1D:
		desc.dimension = resource_view_dimension::texture_1d;
		desc.first_level = internal_desc.Texture1D.MostDetailedMip;
		desc.levels = internal_desc.Texture1D.MipLevels;
		// Missing fields: D3D12_TEX1D_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
		desc.dimension = resource_view_dimension::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MostDetailedMip;
		desc.levels = internal_desc.Texture1DArray.MipLevels;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		// Missing fields: D3D12_TEX1D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2D:
		desc.dimension = resource_view_dimension::texture_2d;
		desc.first_level = internal_desc.Texture2D.MostDetailedMip;
		desc.levels = internal_desc.Texture2D.MipLevels;
		// Missing fields: D3D12_TEX2D_SRV::PlaneSlice, D3D12_TEX2D_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
		desc.dimension = resource_view_dimension::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MostDetailedMip;
		desc.levels = internal_desc.Texture2DArray.MipLevels;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		// Missing fields D3D12_TEX2D_ARRAY_SRV::PlaneSlice, D3D12_TEX2D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DMS:
		desc.dimension = resource_view_dimension::texture_2d_multisample;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
		desc.dimension = resource_view_dimension::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE3D:
		desc.dimension = resource_view_dimension::texture_3d;
		desc.first_level = internal_desc.Texture3D.MostDetailedMip;
		desc.levels = internal_desc.Texture3D.MipLevels;
		// Missing fields: D3D12_TEX3D_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURECUBE:
		desc.dimension = resource_view_dimension::texture_cube;
		desc.first_level = internal_desc.TextureCube.MostDetailedMip;
		desc.levels = internal_desc.TextureCube.MipLevels;
		// Missing fields: D3D12_TEXCUBE_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
		desc.dimension = resource_view_dimension::texture_cube_array;
		desc.first_level = internal_desc.TextureCubeArray.MostDetailedMip;
		desc.levels = internal_desc.TextureCubeArray.MipLevels;
		desc.first_layer = internal_desc.TextureCubeArray.First2DArrayFace;
		desc.layers = internal_desc.TextureCubeArray.NumCubes * 6;
		// Missing fields: D3D12_TEXCUBE_ARRAY_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
		desc.first_level = internal_desc.RaytracingAccelerationStructure.Location & 0xFFFFFFFF;
		desc.first_layer = (internal_desc.RaytracingAccelerationStructure.Location >> 32) & 0xFFFFFFFF;
		break;
	}
	return desc;
}
resource_view_desc reshade::d3d12::convert_resource_view_desc(const D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc)
{
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.levels = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D12_UAV_DIMENSION_BUFFER:
		desc.dimension = resource_view_dimension::buffer;
		desc.first_level = internal_desc.Buffer.FirstElement & 0xFFFFFFFF;
		desc.first_layer = (internal_desc.Buffer.FirstElement >> 32) & 0xFFFFFFFF;
		desc.levels = internal_desc.Buffer.NumElements;
		// Missing fields: D3D12_BUFFER_UAV::StructureByteStride, D3D12_BUFFER_UAV::CounterOffsetInBytes, D3D12_BUFFER_UAV::Flags
		break;
	case D3D12_UAV_DIMENSION_TEXTURE1D:
		desc.dimension = resource_view_dimension::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
		desc.dimension = resource_view_dimension::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE2D:
		desc.dimension = resource_view_dimension::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
		desc.dimension = resource_view_dimension::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE3D:
		desc.dimension = resource_view_dimension::texture_3d;
		desc.first_level = internal_desc.Texture3D.MipSlice;
		desc.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.layers = internal_desc.Texture3D.WSize;
		break;
	}
	return desc;
}

reshade::d3d12::device_impl::device_impl(ID3D12Device *device) :
	api_object_impl(device)
{
	for (UINT type = 0; type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++type)
	{
		_descriptor_handle_size[type] = device->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));

		D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
		heap_desc.Type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type);
		heap_desc.NumDescriptors = 32;
		_orig->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&_resource_view_pool[type]));
		_resource_view_pool_state[type].resize(heap_desc.NumDescriptors);
	}

#if RESHADE_ADDON
	reshade::addon::load_addons();
#endif

	RESHADE_ADDON_EVENT(init_device, this);
}
reshade::d3d12::device_impl::~device_impl()
{
	assert(_queues.empty()); // All queues should have been unregistered and destroyed by the application at this point

	RESHADE_ADDON_EVENT(destroy_device, this);

#if RESHADE_ADDON
	reshade::addon::unload_addons();
#endif
}

bool reshade::d3d12::device_impl::check_format_support(uint32_t format, resource_usage usage) const
{
	D3D12_FEATURE_DATA_FORMAT_SUPPORT feature = { static_cast<DXGI_FORMAT>(format) };
	if (FAILED(_orig->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &feature, sizeof(feature))))
		return false;

	if ((usage & resource_usage::render_target) != 0 &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) == 0)
		return false;
	if ((usage & resource_usage::depth_stencil) != 0 &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) == 0)
		return false;
	if ((usage & resource_usage::shader_resource) != 0 &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) == 0)
		return false;
	if ((usage & resource_usage::unordered_access) != 0 &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD) == 0)
		return false;
	if ((usage & (resource_usage::resolve_dest | resource_usage::resolve_source)) != 0 &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE) == 0)
		return false;

	return true;
}

bool reshade::d3d12::device_impl::check_resource_handle_valid(resource_handle resource) const
{
	return resource.handle != 0 && _resources.has_object(reinterpret_cast<ID3D12Resource *>(resource.handle));
}
bool reshade::d3d12::device_impl::check_resource_view_handle_valid(resource_view_handle view) const
{
	const std::lock_guard<std::mutex> lock(_mutex);
	return _views.find(view.handle) != _views.end();
}

bool reshade::d3d12::device_impl::create_resource(resource_type type, const resource_desc &desc, resource_usage initial_state, resource_handle *out_resource)
{
	assert((desc.usage & initial_state) == initial_state);

	D3D12_RESOURCE_DESC internal_desc = {};
	convert_resource_desc(type, desc, internal_desc);
	if (type == resource_type::buffer)
		internal_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	const D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_DEFAULT };

	if (ID3D12Resource *resource = nullptr;
		SUCCEEDED(_orig->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &internal_desc, convert_resource_usage_to_states(initial_state), nullptr, IID_PPV_ARGS(&resource))))
	{
		_resources.register_object(resource);
		*out_resource = { reinterpret_cast<uintptr_t>(resource) };
		return true;
	}
	else
	{
		*out_resource = { 0 };
		return false;
	}
}
bool reshade::d3d12::device_impl::create_resource_view(resource_handle resource, resource_view_type type, const resource_view_desc &desc, resource_view_handle *out_view)
{
	assert(resource.handle != 0);

	switch (type)
	{
		case resource_view_type::depth_stencil:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			if (descriptor_handle.ptr == 0)
				break; // No more space available in the descriptor heap

			D3D12_DEPTH_STENCIL_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			_orig->CreateDepthStencilView(reinterpret_cast<ID3D12Resource *>(resource.handle), &internal_desc, descriptor_handle);

			register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), descriptor_handle);
			*out_view = { descriptor_handle.ptr };
			return true;
		}
		case resource_view_type::render_target:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			if (descriptor_handle.ptr == 0)
				break;

			D3D12_RENDER_TARGET_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			_orig->CreateRenderTargetView(reinterpret_cast<ID3D12Resource *>(resource.handle), &internal_desc, descriptor_handle);

			register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), descriptor_handle);
			*out_view = { descriptor_handle.ptr };
			return true;
		}
		case resource_view_type::shader_resource:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			if (descriptor_handle.ptr == 0)
				break;

			D3D12_SHADER_RESOURCE_VIEW_DESC internal_desc = {};
			internal_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			convert_resource_view_desc(desc, internal_desc);

			_orig->CreateShaderResourceView(reinterpret_cast<ID3D12Resource *>(resource.handle), &internal_desc, descriptor_handle);

			register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), descriptor_handle);
			*out_view = { descriptor_handle.ptr };
			return true;
		}
		case resource_view_type::unordered_access:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			if (descriptor_handle.ptr == 0)
				break;

			D3D12_UNORDERED_ACCESS_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			_orig->CreateUnorderedAccessView(reinterpret_cast<ID3D12Resource *>(resource.handle), nullptr, &internal_desc, descriptor_handle);

			register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), descriptor_handle);
			*out_view = { descriptor_handle.ptr };
			return true;
		}
	}

	*out_view = { 0 };
	return false;
}

void reshade::d3d12::device_impl::destroy_resource(resource_handle resource)
{
	assert(resource.handle != 0);
	reinterpret_cast<ID3D12Resource *>(resource.handle)->Release();
}
void reshade::d3d12::device_impl::destroy_resource_view(resource_view_handle view)
{
	assert(view.handle != 0);

	const std::lock_guard<std::mutex> lock(_mutex);
	_views.erase(view.handle);

	// Mark free slot in the descriptor heap
	for (UINT type = 0; type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++type)
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE heap_start = _resource_view_pool[type]->GetCPUDescriptorHandleForHeapStart();
		if (view.handle >= heap_start.ptr && view.handle < (heap_start.ptr + _resource_view_pool_state[type].size() * _descriptor_handle_size[type]))
		{
			const size_t index = static_cast<size_t>(view.handle - heap_start.ptr) / _descriptor_handle_size[type];
			_resource_view_pool_state[type][index] = false;
			break;
		}
	}
}

void reshade::d3d12::device_impl::get_resource_from_view(resource_view_handle view, resource_handle *out_resource) const
{
	assert(view.handle != 0);

	const std::lock_guard<std::mutex> lock(_mutex);
	if (const auto it = _views.find(view.handle); it != _views.end())
		*out_resource = { reinterpret_cast<uintptr_t>(it->second) };
	else
		*out_resource = { 0 };
}

resource_desc reshade::d3d12::device_impl::get_resource_desc(resource_handle resource) const
{
	assert(resource.handle != 0);
	return convert_resource_desc(reinterpret_cast<ID3D12Resource *>(resource.handle)->GetDesc()).second;
}

void reshade::d3d12::device_impl::wait_idle() const
{
	com_ptr<ID3D12Fence> fence;
	if (FAILED(_orig->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
		return;

	const HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr)
		return;

	const std::lock_guard<std::mutex> lock(_mutex);

	UINT64 signal_value = 1;
	for (ID3D12CommandQueue *const queue : _queues)
	{
		queue->Signal(fence.get(), signal_value);
		fence->SetEventOnCompletion(signal_value, fence_event);
		WaitForSingleObject(fence_event, INFINITE);
		signal_value++;
	}

	CloseHandle(fence_event);
}

D3D12_CPU_DESCRIPTOR_HANDLE reshade::d3d12::device_impl::allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	std::vector<bool> &state = _resource_view_pool_state[type];
	// Find free entry in the descriptor heap
	const std::lock_guard<std::mutex> lock(_mutex);
	if (const auto it = std::find(state.begin(), state.end(), false);
		it != state.end() && _resource_view_pool[type] != nullptr)
	{
		const size_t index = it - state.begin();
		state[index] = true; // Mark this entry as being in use

		D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = _resource_view_pool[type]->GetCPUDescriptorHandleForHeapStart();
		descriptor_handle.ptr += index * _descriptor_handle_size[type];
		return descriptor_handle;
	}
	else
	{
		return D3D12_CPU_DESCRIPTOR_HANDLE { 0 };
	}
}

reshade::d3d12::command_list_impl::command_list_impl(device_impl *device, ID3D12GraphicsCommandList *cmd_list) :
	api_object_impl(cmd_list), _device_impl(device), _has_commands(cmd_list != nullptr)
{
	if (_has_commands) // Do not call add-on event for immediate command list
	{
		RESHADE_ADDON_EVENT(init_command_list, this);
	}
}
reshade::d3d12::command_list_impl::~command_list_impl()
{
	if (_has_commands)
	{
		RESHADE_ADDON_EVENT(destroy_command_list, this);
	}
}

void reshade::d3d12::command_list_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	_has_commands = true;

	_orig->DrawInstanced(vertices, instances, first_vertex, first_instance);
}
void reshade::d3d12::command_list_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_has_commands = true;

	_orig->DrawIndexedInstanced(indices, instances, first_index, vertex_offset, first_instance);
}

void reshade::d3d12::command_list_impl::copy_resource(resource_handle source, resource_handle destination)
{
	_has_commands = true;

	assert(source.handle != 0 && destination.handle != 0);
	_orig->CopyResource(reinterpret_cast<ID3D12Resource *>(destination.handle), reinterpret_cast<ID3D12Resource *>(source.handle));
}

void reshade::d3d12::command_list_impl::transition_state(resource_handle resource, resource_usage old_layout, resource_usage new_layout)
{
	_has_commands = true;

	assert(resource.handle != 0);

	D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transition.Transition.pResource = reinterpret_cast<ID3D12Resource *>(resource.handle);
	transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transition.Transition.StateBefore = convert_resource_usage_to_states(old_layout);
	transition.Transition.StateAfter = convert_resource_usage_to_states(new_layout);

	_orig->ResourceBarrier(1, &transition);
}

void reshade::d3d12::command_list_impl::clear_depth_stencil_view(resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	_has_commands = true;

	assert(dsv.handle != 0);
	_orig->ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(dsv.handle) }, static_cast<D3D12_CLEAR_FLAGS>(clear_flags), depth, stencil, 0, nullptr);
}
void reshade::d3d12::command_list_impl::clear_render_target_view(resource_view_handle rtv, const float color[4])
{
	_has_commands = true;

	assert(rtv.handle != 0);
	_orig->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(rtv.handle) }, color, 0, nullptr);
}

reshade::d3d12::command_list_immediate_impl::command_list_immediate_impl(device_impl *device) :
	command_list_impl(device, nullptr)
{
	// Create multiple command allocators to buffer for multiple frames
	for (UINT i = 0; i < NUM_COMMAND_FRAMES; ++i)
	{
		if (FAILED(_device_impl->_orig->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence[i]))))
			return;
		if (FAILED(_device_impl->_orig->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmd_alloc[i]))))
			return;
	}

	// Create and open the command list for recording
	if (SUCCEEDED(_device_impl->_orig->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmd_alloc[_cmd_index].get(), nullptr, IID_PPV_ARGS(&_orig))))
		_orig->SetName(L"ReShade command list");

	// Create auto-reset event and fences for synchronization
	_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}
reshade::d3d12::command_list_immediate_impl::~command_list_immediate_impl()
{
	if (_orig != nullptr)
		_orig->Release();
	if (_fence_event != nullptr)
		CloseHandle(_fence_event);

	// Signal to 'command_list_impl' destructor that this is an immediate command list
	_has_commands = false;
}

bool reshade::d3d12::command_list_immediate_impl::flush(ID3D12CommandQueue *queue)
{
	if (!_has_commands)
		return true;

	if (const HRESULT hr = _orig->Close(); FAILED(hr))
	{
		LOG(ERROR) << "Failed to close immediate command list! HRESULT is " << hr << '.';

		// A command list that failed to close can never be reset, so destroy it and create a new one
		_device_impl->wait_idle();
		_orig->Release(); _orig = nullptr;
		if (SUCCEEDED(_device_impl->_orig->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmd_alloc[_cmd_index].get(), nullptr, IID_PPV_ARGS(&_orig))))
			_orig->SetName(L"ReShade command list");
		return false;
	}

	ID3D12CommandList *const cmd_lists[] = { _orig };
	queue->ExecuteCommandLists(ARRAYSIZE(cmd_lists), cmd_lists);

	if (const UINT64 sync_value = _fence_value[_cmd_index] + NUM_COMMAND_FRAMES;
		SUCCEEDED(queue->Signal(_fence[_cmd_index].get(), sync_value)))
		_fence_value[_cmd_index] = sync_value;

	// Continue with next command list now that the current one was submitted
	_cmd_index = (_cmd_index + 1) % NUM_COMMAND_FRAMES;

	// Make sure all commands for the next command allocator have finished executing before reseting it
	if (_fence[_cmd_index]->GetCompletedValue() < _fence_value[_cmd_index])
	{
		if (SUCCEEDED(_fence[_cmd_index]->SetEventOnCompletion(_fence_value[_cmd_index], _fence_event)))
			WaitForSingleObject(_fence_event, INFINITE); // Event is automatically reset after this wait is released
	}

	// Reset command allocator before using it this frame again
	_cmd_alloc[_cmd_index]->Reset();

	// Reset command list using current command allocator and put it into the recording state
	return SUCCEEDED(_orig->Reset(_cmd_alloc[_cmd_index].get(), nullptr));
}
bool reshade::d3d12::command_list_immediate_impl::flush_and_wait(ID3D12CommandQueue *queue)
{
	// Index is updated during flush below, so keep track of the current one to wait on
	const UINT cmd_index_to_wait_on = _cmd_index;

	if (!flush(queue))
		return false;

	if (FAILED(_fence[cmd_index_to_wait_on]->SetEventOnCompletion(_fence_value[cmd_index_to_wait_on], _fence_event)))
		return false;
	return WaitForSingleObject(_fence_event, INFINITE) == WAIT_OBJECT_0;
}

reshade::d3d12::command_queue_impl::command_queue_impl(device_impl *device, ID3D12CommandQueue *queue) :
	api_object_impl(queue), _device_impl(device)
{
	// Register queue to device
	{	const std::lock_guard<std::mutex> lock(_device_impl->_mutex);
		_device_impl->_queues.push_back(_orig);
	}

	// Only create an immediate command list for graphics queues (since the implemented commands do not work on other queue types)
	if (queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
		_immediate_cmd_list = new command_list_immediate_impl(device);

	RESHADE_ADDON_EVENT(init_command_queue, this);
}
reshade::d3d12::command_queue_impl::~command_queue_impl()
{
	RESHADE_ADDON_EVENT(destroy_command_queue, this);

	delete _immediate_cmd_list;

	// Unregister queue from device
	{	const std::lock_guard<std::mutex> lock(_device_impl->_mutex);
		_device_impl->_queues.erase(std::find(_device_impl->_queues.begin(), _device_impl->_queues.end(), _orig));
	}
}

void reshade::d3d12::command_queue_impl::flush_immediate_command_list() const
{
	if (_immediate_cmd_list != nullptr)
		_immediate_cmd_list->flush(_orig);
}
