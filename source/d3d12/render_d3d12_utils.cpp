/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d12.hpp"
#include "render_d3d12_utils.hpp"

using namespace reshade::api;

D3D12_RESOURCE_STATES reshade::d3d12::convert_resource_usage_to_states(reshade::api::resource_usage usage)
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

void reshade::d3d12::convert_resource_desc(const resource_desc &desc, D3D12_RESOURCE_DESC &internal_desc, D3D12_HEAP_PROPERTIES &heap_props)
{
	switch (desc.type)
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

	if (desc.type == resource_type::buffer)
	{
		internal_desc.Width = desc.size;
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

	switch (desc.heap)
	{
	case memory_heap::gpu_only:
		heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
		break;
	case memory_heap::cpu_only:
	case memory_heap::cpu_to_gpu:
		heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
		break;
	case memory_heap::gpu_to_cpu:
		heap_props.Type = D3D12_HEAP_TYPE_READBACK;
		break;
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
resource_desc reshade::d3d12::convert_resource_desc(const D3D12_RESOURCE_DESC &internal_desc, const D3D12_HEAP_PROPERTIES &heap_props)
{
	resource_desc desc = {};
	switch (internal_desc.Dimension)
	{
	default:
		assert(false);
		desc.type = resource_type::unknown;
		break;
	case D3D12_RESOURCE_DIMENSION_BUFFER:
		desc.type = resource_type::buffer;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		desc.type = resource_type::texture_1d;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		desc.type = resource_type::texture_2d;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		desc.type = resource_type::texture_3d;
		break;
	}

	if (desc.type == resource_type::buffer)
	{
		desc.size = internal_desc.Width;

		// Buffers may be of any type in D3D12, so add all possible usage flags
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

		if (desc.type == resource_type::texture_2d)
			desc.usage |= desc.samples > 1 ? resource_usage::resolve_source : resource_usage::resolve_dest;
	}

	switch (heap_props.Type)
	{
	default:
		desc.heap = memory_heap::unknown;
		break;
	case D3D12_HEAP_TYPE_DEFAULT:
		desc.heap = memory_heap::gpu_only;
		break;
	case D3D12_HEAP_TYPE_UPLOAD:
		desc.heap = memory_heap::cpu_to_gpu;
		break;
	case D3D12_HEAP_TYPE_READBACK:
		desc.heap = memory_heap::gpu_to_cpu;
		break;
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

	return desc;
}

void reshade::d3d12::convert_resource_view_desc(const resource_view_desc &desc, D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_DEPTH_STENCIL_VIEW_DESC::Flags
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.type != resource_view_type::buffer && desc.levels == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	}
}
void reshade::d3d12::convert_resource_view_desc(const resource_view_desc &desc, D3D12_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.type != resource_view_type::buffer && desc.levels == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_3d:
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
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case resource_view_type::buffer:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		internal_desc.Buffer.FirstElement = desc.offset;
		assert(desc.size <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.NumElements = static_cast<UINT>(desc.size);
		// Missing fields: D3D12_BUFFER_SRV::StructureByteStride, D3D12_BUFFER_SRV::Flags
		break;
	case resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MostDetailedMip = desc.first_level;
		internal_desc.Texture1D.MipLevels = desc.levels;
		// Missing fields: D3D12_TEX1D_SRV::ResourceMinLODClamp
		break;
	case resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MostDetailedMip = desc.first_level;
		internal_desc.Texture1DArray.MipLevels = desc.levels;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		// Missing fields: D3D12_TEX1D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MostDetailedMip = desc.first_level;
		internal_desc.Texture2D.MipLevels = desc.levels;
		// Missing fields: D3D12_TEX2D_SRV::PlaneSlice, D3D12_TEX2D_SRV::ResourceMinLODClamp
		break;
	case resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MostDetailedMip = desc.first_level;
		internal_desc.Texture2DArray.MipLevels = desc.levels;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		// Missing fields D3D12_TEX2D_ARRAY_SRV::PlaneSlice, D3D12_TEX2D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MostDetailedMip = desc.first_level;
		internal_desc.Texture3D.MipLevels = desc.levels;
		// Missing fields: D3D12_TEX3D_SRV::ResourceMinLODClamp
		break;
	case resource_view_type::texture_cube:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		internal_desc.TextureCube.MostDetailedMip = desc.first_level;
		internal_desc.TextureCube.MipLevels = desc.levels;
		// Missing fields: D3D12_TEXCUBE_SRV::ResourceMinLODClamp
		break;
	case resource_view_type::texture_cube_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
		internal_desc.TextureCubeArray.MostDetailedMip = desc.first_level;
		internal_desc.TextureCubeArray.MipLevels = desc.levels;
		internal_desc.TextureCubeArray.First2DArrayFace = desc.first_layer;
		if (desc.layers == 0xFFFFFFFF)
			internal_desc.TextureCubeArray.NumCubes = 0xFFFFFFFF;
		else
			internal_desc.TextureCubeArray.NumCubes = desc.layers / 6;
		// Missing fields: D3D12_TEXCUBE_ARRAY_SRV::ResourceMinLODClamp
		break;
	}
}
void reshade::d3d12::convert_resource_view_desc(const resource_view_desc &desc, D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.type == resource_view_type::buffer || desc.levels == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case resource_view_type::buffer:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		internal_desc.Buffer.FirstElement = desc.offset;
		assert(desc.size <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.NumElements = static_cast<UINT>(desc.size);
		// Missing fields: D3D12_BUFFER_UAV::StructureByteStride, D3D12_BUFFER_UAV::CounterOffsetInBytes, D3D12_BUFFER_UAV::Flags
		break;
	case resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_3d:
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
		desc.type = resource_view_type::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
		desc.type = resource_view_type::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2D:
		desc.type = resource_view_type::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
		desc.type = resource_view_type::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2DMS:
		desc.type = resource_view_type::texture_2d_multisample;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = resource_view_type::texture_2d_multisample_array;
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
		desc.type = resource_view_type::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
		desc.type = resource_view_type::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2D:
		desc.type = resource_view_type::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
		desc.type = resource_view_type::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2DMS:
		desc.type = resource_view_type::texture_2d_multisample;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = resource_view_type::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE3D:
		desc.type = resource_view_type::texture_3d;
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
		desc.type = resource_view_type::buffer;
		desc.offset = internal_desc.Buffer.FirstElement;
		desc.size = internal_desc.Buffer.NumElements;
		// Missing fields: D3D12_BUFFER_SRV::StructureByteStride, D3D12_BUFFER_SRV::Flags
		break;
	case D3D12_SRV_DIMENSION_TEXTURE1D:
		desc.type = resource_view_type::texture_1d;
		desc.first_level = internal_desc.Texture1D.MostDetailedMip;
		desc.levels = internal_desc.Texture1D.MipLevels;
		// Missing fields: D3D12_TEX1D_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
		desc.type = resource_view_type::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MostDetailedMip;
		desc.levels = internal_desc.Texture1DArray.MipLevels;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		// Missing fields: D3D12_TEX1D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2D:
		desc.type = resource_view_type::texture_2d;
		desc.first_level = internal_desc.Texture2D.MostDetailedMip;
		desc.levels = internal_desc.Texture2D.MipLevels;
		// Missing fields: D3D12_TEX2D_SRV::PlaneSlice, D3D12_TEX2D_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
		desc.type = resource_view_type::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MostDetailedMip;
		desc.levels = internal_desc.Texture2DArray.MipLevels;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		// Missing fields D3D12_TEX2D_ARRAY_SRV::PlaneSlice, D3D12_TEX2D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DMS:
		desc.type = resource_view_type::texture_2d_multisample;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = resource_view_type::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE3D:
		desc.type = resource_view_type::texture_3d;
		desc.first_level = internal_desc.Texture3D.MostDetailedMip;
		desc.levels = internal_desc.Texture3D.MipLevels;
		// Missing fields: D3D12_TEX3D_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURECUBE:
		desc.type = resource_view_type::texture_cube;
		desc.first_level = internal_desc.TextureCube.MostDetailedMip;
		desc.levels = internal_desc.TextureCube.MipLevels;
		// Missing fields: D3D12_TEXCUBE_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
		desc.type = resource_view_type::texture_cube_array;
		desc.first_level = internal_desc.TextureCubeArray.MostDetailedMip;
		desc.levels = internal_desc.TextureCubeArray.MipLevels;
		desc.first_layer = internal_desc.TextureCubeArray.First2DArrayFace;
		if (internal_desc.TextureCubeArray.NumCubes == 0xFFFFFFFF)
			desc.layers = 0xFFFFFFFF;
		else
			desc.layers = internal_desc.TextureCubeArray.NumCubes * 6;
		// Missing fields: D3D12_TEXCUBE_ARRAY_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
		desc.offset = internal_desc.RaytracingAccelerationStructure.Location;
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
		desc.type = resource_view_type::buffer;
		desc.offset = internal_desc.Buffer.FirstElement;
		desc.size = internal_desc.Buffer.NumElements;
		// Missing fields: D3D12_BUFFER_UAV::StructureByteStride, D3D12_BUFFER_UAV::CounterOffsetInBytes, D3D12_BUFFER_UAV::Flags
		break;
	case D3D12_UAV_DIMENSION_TEXTURE1D:
		desc.type = resource_view_type::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
		desc.type = resource_view_type::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE2D:
		desc.type = resource_view_type::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
		desc.type = resource_view_type::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE3D:
		desc.type = resource_view_type::texture_3d;
		desc.first_level = internal_desc.Texture3D.MipSlice;
		desc.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.layers = internal_desc.Texture3D.WSize;
		break;
	}
	return desc;
}
