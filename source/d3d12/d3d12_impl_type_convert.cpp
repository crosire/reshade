/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d12_impl_device.hpp"
#include "d3d12_impl_type_convert.hpp"

auto reshade::d3d12::convert_format(api::format format) -> DXGI_FORMAT
{
	if (static_cast<uint32_t>(format) >= 1000)
		return DXGI_FORMAT_UNKNOWN;

	return static_cast<DXGI_FORMAT>(format);
}
auto reshade::d3d12::convert_format(DXGI_FORMAT format) -> api::format
{
	return static_cast<api::format>(format);
}

D3D12_RESOURCE_STATES reshade::d3d12::convert_resource_usage_to_states(api::resource_usage usage)
{
	// Undefined usage does not exist in D3D12
	assert(usage != api::resource_usage::undefined);

	if (usage == api::resource_usage::general)
		return D3D12_RESOURCE_STATE_COMMON;
	if (usage == api::resource_usage::present)
		return D3D12_RESOURCE_STATE_PRESENT;
	if (usage == api::resource_usage::cpu_access)
		return D3D12_RESOURCE_STATE_GENERIC_READ;

	auto result = static_cast<D3D12_RESOURCE_STATES>(static_cast<uint32_t>(usage) & 0xFFFFFFF);

	// Depth write state is mutually exclusive with other states, so remove it when read state is specified too
	if ((usage & api::resource_usage::depth_stencil) == api::resource_usage::depth_stencil)
	{
		result ^= D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	// The separate constant buffer state does not exist in D3D12, so replace it with the combined vertex/constant buffer one
	if ((usage & api::resource_usage::constant_buffer) == api::resource_usage::constant_buffer)
	{
		result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		result ^= static_cast<D3D12_RESOURCE_STATES>(api::resource_usage::constant_buffer);
	}

	return result;
}

void reshade::d3d12::convert_sampler_desc(const api::sampler_desc &desc, D3D12_SAMPLER_DESC &internal_desc)
{
	internal_desc.Filter = static_cast<D3D12_FILTER>(desc.filter);
	internal_desc.AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(desc.address_u);
	internal_desc.AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(desc.address_v);
	internal_desc.AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(desc.address_w);
	internal_desc.MipLODBias = desc.mip_lod_bias;
	internal_desc.MaxAnisotropy = static_cast<UINT>(desc.max_anisotropy);
	internal_desc.ComparisonFunc = convert_compare_op(desc.compare_op);
	std::copy_n(desc.border_color, 4, internal_desc.BorderColor);
	internal_desc.MinLOD = desc.min_lod;
	internal_desc.MaxLOD = desc.max_lod;
}
reshade::api::sampler_desc reshade::d3d12::convert_sampler_desc(const D3D12_SAMPLER_DESC &internal_desc)
{
	api::sampler_desc desc = {};
	desc.filter = static_cast<api::filter_mode>(internal_desc.Filter);
	desc.address_u = static_cast<api::texture_address_mode>(internal_desc.AddressU);
	desc.address_v = static_cast<api::texture_address_mode>(internal_desc.AddressV);
	desc.address_w = static_cast<api::texture_address_mode>(internal_desc.AddressW);
	desc.mip_lod_bias = internal_desc.MipLODBias;
	desc.max_anisotropy = static_cast<float>(internal_desc.MaxAnisotropy);
	desc.compare_op = convert_compare_op(internal_desc.ComparisonFunc);
	std::copy_n(internal_desc.BorderColor, 4, desc.border_color);
	desc.min_lod = internal_desc.MinLOD;
	desc.max_lod = internal_desc.MaxLOD;
	return desc;
}

void reshade::d3d12::convert_resource_desc(const api::resource_desc &desc, D3D12_RESOURCE_DESC &internal_desc, D3D12_HEAP_PROPERTIES &heap_props, D3D12_HEAP_FLAGS &heap_flags)
{
	switch (desc.type)
	{
	default:
		assert(false);
		internal_desc.Dimension = D3D12_RESOURCE_DIMENSION_UNKNOWN;
		break;
	case api::resource_type::buffer:
		internal_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		break;
	case api::resource_type::texture_1d:
		internal_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
		break;
	case api::resource_type::texture_2d:
		internal_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		break;
	case api::resource_type::texture_3d:
		internal_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		break;
	}

	if (desc.type == api::resource_type::buffer)
	{
		internal_desc.Width = desc.buffer.size;
		internal_desc.Height = 1;
		internal_desc.DepthOrArraySize = 1;
		internal_desc.MipLevels = 1;
		internal_desc.SampleDesc.Count = 1;
	}
	else
	{
		internal_desc.Width = desc.texture.width;
		internal_desc.Height = desc.texture.height;
		internal_desc.DepthOrArraySize = desc.texture.depth_or_layers;
		internal_desc.MipLevels = desc.texture.levels;
		internal_desc.Format = convert_format(desc.texture.format);
		internal_desc.SampleDesc.Count = desc.texture.samples;
	}

	switch (desc.heap)
	{
	case api::memory_heap::gpu_only:
		heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
		break;
	case api::memory_heap::cpu_only:
	case api::memory_heap::cpu_to_gpu:
		heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
		break;
	case api::memory_heap::gpu_to_cpu:
		heap_props.Type = D3D12_HEAP_TYPE_READBACK;
		break;
	case api::memory_heap::custom:
		heap_props.Type = D3D12_HEAP_TYPE_CUSTOM;
		break;
	}

	if ((desc.usage & api::resource_usage::depth_stencil) != api::resource_usage::undefined)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	if ((desc.usage & api::resource_usage::render_target) != api::resource_usage::undefined)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	if ((desc.usage & api::resource_usage::shader_resource) == api::resource_usage::undefined && (internal_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	// Mipmap generation is using compute shaders and therefore needs unordered access flag (see 'command_list_impl::generate_mipmaps')
	if ((desc.usage & api::resource_usage::unordered_access) != api::resource_usage::undefined ||
		(desc.flags & api::resource_flags::generate_mipmaps) == api::resource_flags::generate_mipmaps)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	if ((desc.flags & api::resource_flags::shared) == api::resource_flags::shared)
		heap_flags |= D3D12_HEAP_FLAG_SHARED;

	// Dynamic resources do not exist in D3D12
	assert((desc.flags & api::resource_flags::dynamic) != api::resource_flags::dynamic);
}
reshade::api::resource_desc reshade::d3d12::convert_resource_desc(const D3D12_RESOURCE_DESC &internal_desc, const D3D12_HEAP_PROPERTIES &heap_props, D3D12_HEAP_FLAGS heap_flags)
{
	api::resource_desc desc = {};
	switch (internal_desc.Dimension)
	{
	default:
		assert(false);
		desc.type = api::resource_type::unknown;
		break;
	case D3D12_RESOURCE_DIMENSION_BUFFER:
		desc.type = api::resource_type::buffer;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		desc.type = api::resource_type::texture_1d;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		desc.type = api::resource_type::texture_2d;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		desc.type = api::resource_type::texture_3d;
		break;
	}

	if (desc.type == api::resource_type::buffer)
	{
		desc.buffer.size = internal_desc.Width;

		// Buffers may be of any type in D3D12, so add all possible usage flags
		desc.usage |= api::resource_usage::vertex_buffer | api::resource_usage::index_buffer | api::resource_usage::constant_buffer;
	}
	else
	{
		assert(internal_desc.Width <= std::numeric_limits<uint32_t>::max());
		desc.texture.width = static_cast<uint32_t>(internal_desc.Width);
		desc.texture.height = internal_desc.Height;
		desc.texture.levels = internal_desc.MipLevels;
		desc.texture.depth_or_layers = internal_desc.DepthOrArraySize;
		desc.texture.format = convert_format(internal_desc.Format);
		desc.texture.samples = static_cast<uint16_t>(internal_desc.SampleDesc.Count);

		if (desc.type == api::resource_type::texture_2d)
			desc.usage |= desc.texture.samples > 1 ? api::resource_usage::resolve_source : api::resource_usage::resolve_dest;
	}

	switch (heap_props.Type)
	{
	default:
		desc.heap = api::memory_heap::unknown;
		// This is a resource created with 'CreateReservedResource' which are always sparse
		desc.flags |= api::resource_flags::sparse_binding;
		break;
	case D3D12_HEAP_TYPE_DEFAULT:
		desc.heap = api::memory_heap::gpu_only;
		break;
	case D3D12_HEAP_TYPE_UPLOAD:
		desc.heap = api::memory_heap::cpu_to_gpu;
		break;
	case D3D12_HEAP_TYPE_READBACK:
		desc.heap = api::memory_heap::gpu_to_cpu;
		break;
	case D3D12_HEAP_TYPE_CUSTOM:
		desc.heap = api::memory_heap::custom;
		break;
	}

	// Resources are generally copyable in D3D12
	desc.usage |= api::resource_usage::copy_dest | api::resource_usage::copy_source;

	if ((internal_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0)
		desc.usage |= api::resource_usage::depth_stencil;
	if ((internal_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0)
		desc.usage |= api::resource_usage::render_target;
	if ((internal_desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0)
		desc.usage |= api::resource_usage::shader_resource;

	if ((internal_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0)
	{
		desc.usage |= api::resource_usage::unordered_access;
		// Resources that have unordered access are usable with the 'generate_mipmaps' function
		if (internal_desc.MipLevels > 1)
			desc.flags |= api::resource_flags::generate_mipmaps;
	}

	if ((heap_flags & D3D12_HEAP_FLAG_SHARED) != 0)
		desc.flags |= api::resource_flags::shared;

	return desc;
}

void reshade::d3d12::convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_DEPTH_STENCIL_VIEW_DESC::Flags
	internal_desc.Format = convert_format(desc.format);
	assert(desc.type != api::resource_view_type::buffer && desc.texture.level_count == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case api::resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layer_count;
		break;
	}
}
void reshade::d3d12::convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	internal_desc.Format = convert_format(desc.format);
	assert(desc.type != api::resource_view_type::buffer && desc.texture.level_count == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case api::resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.texture.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.texture.first_layer;
		internal_desc.Texture3D.WSize = desc.texture.layer_count;
		break;
	}
}
void reshade::d3d12::convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_SHADER_RESOURCE_VIEW_DESC::Shader4ComponentMapping
	internal_desc.Format = convert_format(desc.format);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case api::resource_view_type::buffer:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		internal_desc.Buffer.FirstElement = desc.buffer.offset;
		assert(desc.buffer.size <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.NumElements = static_cast<UINT>(desc.buffer.size);
		// Missing fields: D3D12_BUFFER_SRV::StructureByteStride, D3D12_BUFFER_SRV::Flags
		break;
	case api::resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture1D.MipLevels = desc.texture.level_count;
		// Missing fields: D3D12_TEX1D_SRV::ResourceMinLODClamp
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture1DArray.MipLevels = desc.texture.level_count;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layer_count;
		// Missing fields: D3D12_TEX1D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture2D.MipLevels = desc.texture.level_count;
		// Missing fields: D3D12_TEX2D_SRV::PlaneSlice, D3D12_TEX2D_SRV::ResourceMinLODClamp
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture2DArray.MipLevels = desc.texture.level_count;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
		// Missing fields D3D12_TEX2D_ARRAY_SRV::PlaneSlice, D3D12_TEX2D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture3D.MipLevels = desc.texture.level_count;
		// Missing fields: D3D12_TEX3D_SRV::ResourceMinLODClamp
		break;
	case api::resource_view_type::texture_cube:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		internal_desc.TextureCube.MostDetailedMip = desc.texture.first_level;
		internal_desc.TextureCube.MipLevels = desc.texture.level_count;
		// Missing fields: D3D12_TEXCUBE_SRV::ResourceMinLODClamp
		break;
	case api::resource_view_type::texture_cube_array:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
		internal_desc.TextureCubeArray.MostDetailedMip = desc.texture.first_level;
		internal_desc.TextureCubeArray.MipLevels = desc.texture.level_count;
		internal_desc.TextureCubeArray.First2DArrayFace = desc.texture.first_layer;
		if (desc.texture.layer_count == 0xFFFFFFFF)
			internal_desc.TextureCubeArray.NumCubes = 0xFFFFFFFF;
		else
			internal_desc.TextureCubeArray.NumCubes = desc.texture.layer_count / 6;
		// Missing fields: D3D12_TEXCUBE_ARRAY_SRV::ResourceMinLODClamp
		break;
	}
}
void reshade::d3d12::convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc)
{
	internal_desc.Format = convert_format(desc.format);
	assert(desc.type == api::resource_view_type::buffer || desc.texture.level_count == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case api::resource_view_type::buffer:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		internal_desc.Buffer.FirstElement = desc.buffer.offset;
		assert(desc.buffer.size <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.NumElements = static_cast<UINT>(desc.buffer.size);
		// Missing fields: D3D12_BUFFER_UAV::StructureByteStride, D3D12_BUFFER_UAV::CounterOffsetInBytes, D3D12_BUFFER_UAV::Flags
		break;
	case api::resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.texture.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.texture.first_layer;
		internal_desc.Texture3D.WSize = desc.texture.layer_count;
		break;
	}
}
reshade::api::resource_view_desc reshade::d3d12::convert_resource_view_desc(const D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_DEPTH_STENCIL_VIEW_DESC::Flags
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	desc.texture.level_count = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D12_DSV_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DMSArray.ArraySize;
		break;
	}
	return desc;
}
reshade::api::resource_view_desc reshade::d3d12::convert_resource_view_desc(const D3D12_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	desc.texture.level_count = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D12_RTV_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D12_RTV_DIMENSION_TEXTURE3D:
		desc.type = api::resource_view_type::texture_3d;
		desc.texture.first_level = internal_desc.Texture3D.MipSlice;
		desc.texture.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.texture.layer_count = internal_desc.Texture3D.WSize;
		break;
	}
	return desc;
}
reshade::api::resource_view_desc reshade::d3d12::convert_resource_view_desc(const D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_SHADER_RESOURCE_VIEW_DESC::Shader4ComponentMapping
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	switch (internal_desc.ViewDimension)
	{
	case D3D12_SRV_DIMENSION_BUFFER:
		desc.type = api::resource_view_type::buffer;
		desc.buffer.offset = internal_desc.Buffer.FirstElement;
		desc.buffer.size = internal_desc.Buffer.NumElements;
		// Missing fields: D3D12_BUFFER_SRV::StructureByteStride, D3D12_BUFFER_SRV::Flags
		break;
	case D3D12_SRV_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = internal_desc.Texture1D.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture1D.MipLevels;
		// Missing fields: D3D12_TEX1D_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture1DArray.MipLevels;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture1DArray.ArraySize;
		// Missing fields: D3D12_TEX1D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture2D.MipLevels;
		// Missing fields: D3D12_TEX2D_SRV::PlaneSlice, D3D12_TEX2D_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture2DArray.MipLevels;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
		// Missing fields D3D12_TEX2D_ARRAY_SRV::PlaneSlice, D3D12_TEX2D_ARRAY_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE3D:
		desc.type = api::resource_view_type::texture_3d;
		desc.texture.first_level = internal_desc.Texture3D.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture3D.MipLevels;
		// Missing fields: D3D12_TEX3D_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURECUBE:
		desc.type = api::resource_view_type::texture_cube;
		desc.texture.first_level = internal_desc.TextureCube.MostDetailedMip;
		desc.texture.level_count = internal_desc.TextureCube.MipLevels;
		// Missing fields: D3D12_TEXCUBE_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
		desc.type = api::resource_view_type::texture_cube_array;
		desc.texture.first_level = internal_desc.TextureCubeArray.MostDetailedMip;
		desc.texture.level_count = internal_desc.TextureCubeArray.MipLevels;
		desc.texture.first_layer = internal_desc.TextureCubeArray.First2DArrayFace;
		if (internal_desc.TextureCubeArray.NumCubes == 0xFFFFFFFF)
			desc.texture.layer_count = 0xFFFFFFFF;
		else
			desc.texture.layer_count = internal_desc.TextureCubeArray.NumCubes * 6;
		// Missing fields: D3D12_TEXCUBE_ARRAY_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
		desc.buffer.offset = internal_desc.RaytracingAccelerationStructure.Location;
		break;
	}
	return desc;
}
reshade::api::resource_view_desc reshade::d3d12::convert_resource_view_desc(const D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc)
{
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	desc.texture.level_count = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D12_UAV_DIMENSION_BUFFER:
		desc.type = api::resource_view_type::buffer;
		desc.buffer.offset = internal_desc.Buffer.FirstElement;
		desc.buffer.size = internal_desc.Buffer.NumElements;
		// Missing fields: D3D12_BUFFER_UAV::StructureByteStride, D3D12_BUFFER_UAV::CounterOffsetInBytes, D3D12_BUFFER_UAV::Flags
		break;
	case D3D12_UAV_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE3D:
		desc.type = api::resource_view_type::texture_3d;
		desc.texture.first_level = internal_desc.Texture3D.MipSlice;
		desc.texture.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.texture.layer_count = internal_desc.Texture3D.WSize;
		break;
	}
	return desc;
}

void reshade::d3d12::convert_pipeline_desc(const api::pipeline_desc &desc, D3D12_COMPUTE_PIPELINE_STATE_DESC &internal_desc)
{
	assert(desc.type == api::pipeline_stage::all_compute);
	internal_desc.pRootSignature = reinterpret_cast<ID3D12RootSignature *>(desc.layout.handle);

	assert(desc.compute.shader.entry_point == nullptr && desc.compute.shader.spec_constants == 0);
	internal_desc.CS.pShaderBytecode = desc.compute.shader.code;
	internal_desc.CS.BytecodeLength = desc.compute.shader.code_size;
}
void reshade::d3d12::convert_pipeline_desc(const api::pipeline_desc &desc, D3D12_GRAPHICS_PIPELINE_STATE_DESC &internal_desc)
{
	assert(desc.type == api::pipeline_stage::all_graphics);
	internal_desc.pRootSignature = reinterpret_cast<ID3D12RootSignature *>(desc.layout.handle);

	assert(desc.graphics.vertex_shader.entry_point == nullptr && desc.graphics.vertex_shader.spec_constants == 0);
	internal_desc.VS.pShaderBytecode = desc.graphics.vertex_shader.code;
	internal_desc.VS.BytecodeLength = desc.graphics.vertex_shader.code_size;

	assert(desc.graphics.hull_shader.entry_point == nullptr && desc.graphics.hull_shader.spec_constants == 0);
	internal_desc.HS.pShaderBytecode = desc.graphics.hull_shader.code;
	internal_desc.HS.BytecodeLength = desc.graphics.hull_shader.code_size;

	assert(desc.graphics.domain_shader.entry_point == nullptr && desc.graphics.domain_shader.spec_constants == 0);
	internal_desc.DS.pShaderBytecode = desc.graphics.domain_shader.code;
	internal_desc.DS.BytecodeLength = desc.graphics.domain_shader.code_size;

	assert(desc.graphics.geometry_shader.entry_point == nullptr && desc.graphics.geometry_shader.spec_constants == 0);
	internal_desc.GS.pShaderBytecode = desc.graphics.geometry_shader.code;
	internal_desc.GS.BytecodeLength = desc.graphics.geometry_shader.code_size;

	assert(desc.graphics.pixel_shader.entry_point == nullptr && desc.graphics.pixel_shader.spec_constants == 0);
	internal_desc.PS.pShaderBytecode = desc.graphics.pixel_shader.code;
	internal_desc.PS.BytecodeLength = desc.graphics.pixel_shader.code_size;

	internal_desc.BlendState.AlphaToCoverageEnable = desc.graphics.blend_state.alpha_to_coverage_enable;
	internal_desc.BlendState.IndependentBlendEnable = TRUE;

	for (UINT i = 0; i < 8; ++i)
	{
		internal_desc.BlendState.RenderTarget[i].BlendEnable = desc.graphics.blend_state.blend_enable[i];
		internal_desc.BlendState.RenderTarget[i].LogicOpEnable = desc.graphics.blend_state.logic_op_enable[i];
		internal_desc.BlendState.RenderTarget[i].SrcBlend = convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[i]);
		internal_desc.BlendState.RenderTarget[i].DestBlend = convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[i]);
		internal_desc.BlendState.RenderTarget[i].BlendOp = convert_blend_op(desc.graphics.blend_state.color_blend_op[i]);
		internal_desc.BlendState.RenderTarget[i].SrcBlendAlpha = convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[i]);
		internal_desc.BlendState.RenderTarget[i].DestBlendAlpha = convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[i]);
		internal_desc.BlendState.RenderTarget[i].BlendOpAlpha = convert_blend_op(desc.graphics.blend_state.alpha_blend_op[i]);
		internal_desc.BlendState.RenderTarget[i].LogicOp = convert_logic_op(desc.graphics.blend_state.logic_op[i]);
		internal_desc.BlendState.RenderTarget[i].RenderTargetWriteMask = desc.graphics.blend_state.render_target_write_mask[i];
	}

	internal_desc.SampleMask = desc.graphics.sample_mask;

	internal_desc.RasterizerState.FillMode = convert_fill_mode(desc.graphics.rasterizer_state.fill_mode);
	internal_desc.RasterizerState.CullMode = convert_cull_mode(desc.graphics.rasterizer_state.cull_mode);
	internal_desc.RasterizerState.FrontCounterClockwise = desc.graphics.rasterizer_state.front_counter_clockwise;
	internal_desc.RasterizerState.DepthBias = static_cast<INT>(desc.graphics.rasterizer_state.depth_bias);
	internal_desc.RasterizerState.DepthBiasClamp = desc.graphics.rasterizer_state.depth_bias_clamp;
	internal_desc.RasterizerState.SlopeScaledDepthBias = desc.graphics.rasterizer_state.slope_scaled_depth_bias;
	internal_desc.RasterizerState.DepthClipEnable = desc.graphics.rasterizer_state.depth_clip_enable;
	// Note: Scissor testing is always enabled in D3D12, so "desc.graphics.rasterizer_state.scissor_enable" is ignored
	internal_desc.RasterizerState.MultisampleEnable = desc.graphics.rasterizer_state.multisample_enable;
	internal_desc.RasterizerState.AntialiasedLineEnable = desc.graphics.rasterizer_state.antialiased_line_enable;
	internal_desc.RasterizerState.ForcedSampleCount = 0;
	internal_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	internal_desc.DepthStencilState.DepthEnable = desc.graphics.depth_stencil_state.depth_enable;
	internal_desc.DepthStencilState.DepthWriteMask = desc.graphics.depth_stencil_state.depth_write_mask ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	internal_desc.DepthStencilState.DepthFunc = convert_compare_op(desc.graphics.depth_stencil_state.depth_func);
	internal_desc.DepthStencilState.StencilEnable = desc.graphics.depth_stencil_state.stencil_enable;
	internal_desc.DepthStencilState.StencilReadMask = desc.graphics.depth_stencil_state.stencil_read_mask;
	internal_desc.DepthStencilState.StencilWriteMask = desc.graphics.depth_stencil_state.stencil_write_mask;
	internal_desc.DepthStencilState.BackFace.StencilFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_fail_op);
	internal_desc.DepthStencilState.BackFace.StencilDepthFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_depth_fail_op);
	internal_desc.DepthStencilState.BackFace.StencilPassOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_pass_op);
	internal_desc.DepthStencilState.BackFace.StencilFunc = convert_compare_op(desc.graphics.depth_stencil_state.back_stencil_func);
	internal_desc.DepthStencilState.FrontFace.StencilFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_fail_op);
	internal_desc.DepthStencilState.FrontFace.StencilDepthFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_depth_fail_op);
	internal_desc.DepthStencilState.FrontFace.StencilPassOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_pass_op);
	internal_desc.DepthStencilState.FrontFace.StencilFunc = convert_compare_op(desc.graphics.depth_stencil_state.front_stencil_func);
}
reshade::api::pipeline_desc reshade::d3d12::convert_pipeline_desc(const D3D12_COMPUTE_PIPELINE_STATE_DESC &internal_desc)
{
	api::pipeline_desc desc = { api::pipeline_stage::all_compute };
	desc.layout = { reinterpret_cast<uintptr_t>(internal_desc.pRootSignature) };

	desc.compute.shader.code = internal_desc.CS.pShaderBytecode;
	desc.compute.shader.code_size = internal_desc.CS.BytecodeLength;

	return desc;
}
reshade::api::pipeline_desc reshade::d3d12::convert_pipeline_desc(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &internal_desc)
{
	api::pipeline_desc desc = { api::pipeline_stage::all_graphics };
	desc.layout = { reinterpret_cast<uintptr_t>(internal_desc.pRootSignature) };

	desc.graphics.vertex_shader.code = internal_desc.VS.pShaderBytecode;
	desc.graphics.vertex_shader.code_size = internal_desc.VS.BytecodeLength;
	desc.graphics.hull_shader.code = internal_desc.HS.pShaderBytecode;
	desc.graphics.hull_shader.code_size = internal_desc.HS.BytecodeLength;
	desc.graphics.domain_shader.code = internal_desc.DS.pShaderBytecode;
	desc.graphics.domain_shader.code_size = internal_desc.DS.BytecodeLength;
	desc.graphics.geometry_shader.code = internal_desc.GS.pShaderBytecode;
	desc.graphics.geometry_shader.code_size = internal_desc.GS.BytecodeLength;
	desc.graphics.pixel_shader.code = internal_desc.PS.pShaderBytecode;
	desc.graphics.pixel_shader.code_size = internal_desc.PS.BytecodeLength;

	for (UINT i = 0; i < internal_desc.InputLayout.NumElements; ++i)
	{
		const D3D12_INPUT_ELEMENT_DESC &element = internal_desc.InputLayout.pInputElementDescs[i];

		desc.graphics.input_layout[i].semantic = element.SemanticName;
		desc.graphics.input_layout[i].semantic_index = element.SemanticIndex;
		desc.graphics.input_layout[i].format = convert_format(element.Format);
		desc.graphics.input_layout[i].buffer_binding = element.InputSlot;
		desc.graphics.input_layout[i].offset = element.AlignedByteOffset;
		desc.graphics.input_layout[i].instance_step_rate = element.InstanceDataStepRate;
	}

	desc.graphics.blend_state.alpha_to_coverage_enable = internal_desc.BlendState.AlphaToCoverageEnable;

	for (UINT i = 0; i < 8; ++i)
	{
		const D3D12_RENDER_TARGET_BLEND_DESC &target = internal_desc.BlendState.RenderTarget[internal_desc.BlendState.IndependentBlendEnable ? i : 0];

		desc.graphics.blend_state.blend_enable[i] = target.BlendEnable;
		desc.graphics.blend_state.logic_op_enable[i] = target.LogicOpEnable;

		// Only convert blend state if blending is enabled (since some applications leave these uninitialized in this case)
		if (target.BlendEnable)
		{
			desc.graphics.blend_state.src_color_blend_factor[i] = convert_blend_factor(target.SrcBlend);
			desc.graphics.blend_state.dst_color_blend_factor[i] = convert_blend_factor(target.DestBlend);
			desc.graphics.blend_state.color_blend_op[i] = convert_blend_op(target.BlendOp);
			desc.graphics.blend_state.src_alpha_blend_factor[i] = convert_blend_factor(target.SrcBlendAlpha);
			desc.graphics.blend_state.dst_alpha_blend_factor[i] = convert_blend_factor(target.DestBlendAlpha);
			desc.graphics.blend_state.alpha_blend_op[i] = convert_blend_op(target.BlendOpAlpha);
		}
		if (target.LogicOpEnable)
		{
			desc.graphics.blend_state.logic_op[i] = convert_logic_op(target.LogicOp);
		}

		desc.graphics.blend_state.render_target_write_mask[i] = target.RenderTargetWriteMask;
	}

	desc.graphics.sample_mask = internal_desc.SampleMask;

	desc.graphics.rasterizer_state.fill_mode = convert_fill_mode(internal_desc.RasterizerState.FillMode);
	desc.graphics.rasterizer_state.cull_mode = convert_cull_mode(internal_desc.RasterizerState.CullMode);
	desc.graphics.rasterizer_state.front_counter_clockwise = internal_desc.RasterizerState.FrontCounterClockwise;
	desc.graphics.rasterizer_state.depth_bias = static_cast<float>(internal_desc.RasterizerState.DepthBias);
	desc.graphics.rasterizer_state.depth_bias_clamp = internal_desc.RasterizerState.DepthBiasClamp;
	desc.graphics.rasterizer_state.slope_scaled_depth_bias = internal_desc.RasterizerState.SlopeScaledDepthBias;
	desc.graphics.rasterizer_state.depth_clip_enable = internal_desc.RasterizerState.DepthClipEnable;
	desc.graphics.rasterizer_state.scissor_enable = true;
	desc.graphics.rasterizer_state.multisample_enable = internal_desc.RasterizerState.MultisampleEnable;
	desc.graphics.rasterizer_state.antialiased_line_enable = internal_desc.RasterizerState.AntialiasedLineEnable;

	desc.graphics.depth_stencil_state.depth_enable = internal_desc.DepthStencilState.DepthEnable;
	desc.graphics.depth_stencil_state.depth_write_mask = internal_desc.DepthStencilState.DepthWriteMask != D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.graphics.depth_stencil_state.depth_func = convert_compare_op(internal_desc.DepthStencilState.DepthFunc);
	desc.graphics.depth_stencil_state.stencil_enable = internal_desc.DepthStencilState.StencilEnable;
	desc.graphics.depth_stencil_state.stencil_read_mask = internal_desc.DepthStencilState.StencilReadMask;
	desc.graphics.depth_stencil_state.stencil_write_mask = internal_desc.DepthStencilState.StencilWriteMask;
	desc.graphics.depth_stencil_state.back_stencil_fail_op = convert_stencil_op(internal_desc.DepthStencilState.BackFace.StencilFailOp);
	desc.graphics.depth_stencil_state.back_stencil_depth_fail_op = convert_stencil_op(internal_desc.DepthStencilState.BackFace.StencilDepthFailOp);
	desc.graphics.depth_stencil_state.back_stencil_pass_op = convert_stencil_op(internal_desc.DepthStencilState.BackFace.StencilPassOp);
	desc.graphics.depth_stencil_state.back_stencil_func = convert_compare_op(internal_desc.DepthStencilState.BackFace.StencilFunc);
	desc.graphics.depth_stencil_state.front_stencil_fail_op = convert_stencil_op(internal_desc.DepthStencilState.FrontFace.StencilFailOp);
	desc.graphics.depth_stencil_state.front_stencil_depth_fail_op = convert_stencil_op(internal_desc.DepthStencilState.FrontFace.StencilDepthFailOp);
	desc.graphics.depth_stencil_state.front_stencil_pass_op = convert_stencil_op(internal_desc.DepthStencilState.FrontFace.StencilPassOp);
	desc.graphics.depth_stencil_state.front_stencil_func = convert_compare_op(internal_desc.DepthStencilState.FrontFace.StencilFunc);

	desc.graphics.topology = convert_primitive_topology_type(internal_desc.PrimitiveTopologyType);
	desc.graphics.viewport_count = D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

	return desc;
}

auto reshade::d3d12::convert_logic_op(api::logic_op value) -> D3D12_LOGIC_OP
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::logic_op::clear:
		return D3D12_LOGIC_OP_CLEAR;
	case api::logic_op::bitwise_and:
		return D3D12_LOGIC_OP_AND;
	case api::logic_op::bitwise_and_reverse:
		return D3D12_LOGIC_OP_AND_REVERSE;
	case api::logic_op::copy:
		return D3D12_LOGIC_OP_COPY;
	case api::logic_op::bitwise_and_inverted:
		return D3D12_LOGIC_OP_AND_INVERTED;
	case api::logic_op::noop:
		return D3D12_LOGIC_OP_NOOP;
	case api::logic_op::bitwise_xor:
		return D3D12_LOGIC_OP_XOR;
	case api::logic_op::bitwise_or:
		return D3D12_LOGIC_OP_OR;
	case api::logic_op::bitwise_nor:
		return D3D12_LOGIC_OP_NOR;
	case api::logic_op::equivalent:
		return D3D12_LOGIC_OP_EQUIV;
	case api::logic_op::invert:
		return D3D12_LOGIC_OP_INVERT;
	case api::logic_op::bitwise_or_reverse:
		return D3D12_LOGIC_OP_OR_REVERSE;
	case api::logic_op::copy_inverted:
		return D3D12_LOGIC_OP_COPY_INVERTED;
	case api::logic_op::bitwise_or_inverted:
		return D3D12_LOGIC_OP_OR_INVERTED;
	case api::logic_op::bitwise_nand:
		return D3D12_LOGIC_OP_NAND;
	case api::logic_op::set:
		return D3D12_LOGIC_OP_SET;
	}
}
auto reshade::d3d12::convert_logic_op(D3D12_LOGIC_OP value) -> api::logic_op
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case D3D12_LOGIC_OP_CLEAR:
		return api::logic_op::clear;
	case D3D12_LOGIC_OP_AND:
		return api::logic_op::bitwise_and;
	case D3D12_LOGIC_OP_AND_REVERSE:
		return api::logic_op::bitwise_and_reverse;
	case D3D12_LOGIC_OP_COPY:
		return api::logic_op::copy;
	case D3D12_LOGIC_OP_AND_INVERTED:
		return api::logic_op::bitwise_and_inverted;
	case D3D12_LOGIC_OP_NOOP:
		return api::logic_op::noop;
	case D3D12_LOGIC_OP_XOR:
		return api::logic_op::bitwise_xor;
	case D3D12_LOGIC_OP_OR:
		return api::logic_op::bitwise_or;
	case D3D12_LOGIC_OP_NOR:
		return api::logic_op::bitwise_nor;
	case D3D12_LOGIC_OP_EQUIV:
		return api::logic_op::equivalent;
	case D3D12_LOGIC_OP_INVERT:
		return api::logic_op::invert;
	case D3D12_LOGIC_OP_OR_REVERSE:
		return api::logic_op::bitwise_or_reverse;
	case D3D12_LOGIC_OP_COPY_INVERTED:
		return api::logic_op::copy_inverted;
	case D3D12_LOGIC_OP_OR_INVERTED:
		return api::logic_op::bitwise_or_inverted;
	case D3D12_LOGIC_OP_NAND:
		return api::logic_op::bitwise_nand;
	case D3D12_LOGIC_OP_SET:
		return api::logic_op::set;
	}
}
auto reshade::d3d12::convert_blend_op(api::blend_op value) -> D3D12_BLEND_OP
{
	return static_cast<D3D12_BLEND_OP>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d12::convert_blend_op(D3D12_BLEND_OP value) -> api::blend_op
{
	return static_cast<api::blend_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d12::convert_blend_factor(api::blend_factor value) -> D3D12_BLEND
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::zero:
		return D3D12_BLEND_ZERO;
	case api::blend_factor::one:
		return D3D12_BLEND_ONE;
	case api::blend_factor::src_color:
		return D3D12_BLEND_SRC_COLOR;
	case api::blend_factor::inv_src_color:
		return D3D12_BLEND_INV_SRC_COLOR;
	case api::blend_factor::dst_color:
		return D3D12_BLEND_DEST_COLOR;
	case api::blend_factor::inv_dst_color:
		return D3D12_BLEND_INV_DEST_COLOR;
	case api::blend_factor::src_alpha:
		return D3D12_BLEND_SRC_ALPHA;
	case api::blend_factor::inv_src_alpha:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case api::blend_factor::dst_alpha:
		return D3D12_BLEND_DEST_ALPHA;
	case api::blend_factor::inv_dst_alpha:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case api::blend_factor::constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::constant_color:
		return D3D12_BLEND_BLEND_FACTOR;
	case api::blend_factor::inv_constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::inv_constant_color:
		return D3D12_BLEND_INV_BLEND_FACTOR;
	case api::blend_factor::src_alpha_sat:
		return D3D12_BLEND_SRC_ALPHA_SAT;
	case api::blend_factor::src1_color:
		return D3D12_BLEND_SRC1_COLOR;
	case api::blend_factor::inv_src1_color:
		return D3D12_BLEND_INV_SRC1_COLOR;
	case api::blend_factor::src1_alpha:
		return D3D12_BLEND_SRC1_ALPHA;
	case api::blend_factor::inv_src1_alpha:
		return D3D12_BLEND_INV_SRC1_ALPHA;
	}
}
auto reshade::d3d12::convert_blend_factor(D3D12_BLEND value) -> api::blend_factor
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case D3D12_BLEND_ZERO:
		return api::blend_factor::zero;
	case D3D12_BLEND_ONE:
		return api::blend_factor::one;
	case D3D12_BLEND_SRC_COLOR:
		return api::blend_factor::src_color;
	case D3D12_BLEND_INV_SRC_COLOR:
		return api::blend_factor::inv_src_color;
	case D3D12_BLEND_DEST_COLOR:
		return api::blend_factor::dst_color;
	case D3D12_BLEND_INV_DEST_COLOR:
		return api::blend_factor::inv_dst_color;
	case D3D12_BLEND_SRC_ALPHA:
		return api::blend_factor::src_alpha;
	case D3D12_BLEND_INV_SRC_ALPHA:
		return api::blend_factor::inv_src_alpha;
	case D3D12_BLEND_DEST_ALPHA:
		return api::blend_factor::dst_alpha;
	case D3D12_BLEND_INV_DEST_ALPHA:
		return api::blend_factor::inv_dst_alpha;
	case D3D12_BLEND_BLEND_FACTOR:
		return api::blend_factor::constant_color;
	case D3D12_BLEND_INV_BLEND_FACTOR:
		return api::blend_factor::inv_constant_color;
	case D3D12_BLEND_SRC_ALPHA_SAT:
		return api::blend_factor::src_alpha_sat;
	case D3D12_BLEND_SRC1_COLOR:
		return api::blend_factor::src1_color;
	case D3D12_BLEND_INV_SRC1_COLOR:
		return api::blend_factor::inv_src1_color;
	case D3D12_BLEND_SRC1_ALPHA:
		return api::blend_factor::src1_alpha;
	case D3D12_BLEND_INV_SRC1_ALPHA:
		return api::blend_factor::inv_src1_alpha;
	}
}
auto reshade::d3d12::convert_fill_mode(api::fill_mode value) -> D3D12_FILL_MODE
{
	switch (value)
	{
	default:
	case api::fill_mode::point:
		assert(false);
		[[fallthrough]];
	case api::fill_mode::solid:
		return D3D12_FILL_MODE_SOLID;
	case api::fill_mode::wireframe:
		return D3D12_FILL_MODE_WIREFRAME;
	}
}
auto reshade::d3d12::convert_fill_mode(D3D12_FILL_MODE value) -> api::fill_mode
{
	switch (value)
	{
	default:
	case D3D12_FILL_MODE_SOLID:
		return api::fill_mode::solid;
	case D3D12_FILL_MODE_WIREFRAME:
		return api::fill_mode::wireframe;
	}
}
auto reshade::d3d12::convert_cull_mode(api::cull_mode value) -> D3D12_CULL_MODE
{
	assert(value != api::cull_mode::front_and_back);
	return static_cast<D3D12_CULL_MODE>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d12::convert_cull_mode(D3D12_CULL_MODE value) -> api::cull_mode
{
	return static_cast<api::cull_mode>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d12::convert_compare_op(api::compare_op value) -> D3D12_COMPARISON_FUNC
{
	return static_cast<D3D12_COMPARISON_FUNC>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d12::convert_compare_op(D3D12_COMPARISON_FUNC value) -> api::compare_op
{
	return static_cast<api::compare_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d12::convert_stencil_op(api::stencil_op value) -> D3D12_STENCIL_OP
{
	return static_cast<D3D12_STENCIL_OP>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d12::convert_stencil_op(D3D12_STENCIL_OP value) -> api::stencil_op
{
	return static_cast<api::stencil_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d12::convert_primitive_topology_type(api::primitive_topology value) -> D3D12_PRIMITIVE_TOPOLOGY_TYPE
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::primitive_topology::undefined:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
	case api::primitive_topology::point_list:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	case api::primitive_topology::line_list:
	case api::primitive_topology::line_strip:
	case api::primitive_topology::line_list_adj:
	case api::primitive_topology::line_strip_adj:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case api::primitive_topology::triangle_list:
	case api::primitive_topology::triangle_strip:
	case api::primitive_topology::triangle_fan:
	case api::primitive_topology::triangle_list_adj:
	case api::primitive_topology::triangle_strip_adj:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case api::primitive_topology::patch_list_01_cp:
	case api::primitive_topology::patch_list_02_cp:
	case api::primitive_topology::patch_list_03_cp:
	case api::primitive_topology::patch_list_04_cp:
	case api::primitive_topology::patch_list_05_cp:
	case api::primitive_topology::patch_list_06_cp:
	case api::primitive_topology::patch_list_07_cp:
	case api::primitive_topology::patch_list_08_cp:
	case api::primitive_topology::patch_list_09_cp:
	case api::primitive_topology::patch_list_10_cp:
	case api::primitive_topology::patch_list_11_cp:
	case api::primitive_topology::patch_list_12_cp:
	case api::primitive_topology::patch_list_13_cp:
	case api::primitive_topology::patch_list_14_cp:
	case api::primitive_topology::patch_list_15_cp:
	case api::primitive_topology::patch_list_16_cp:
	case api::primitive_topology::patch_list_17_cp:
	case api::primitive_topology::patch_list_18_cp:
	case api::primitive_topology::patch_list_19_cp:
	case api::primitive_topology::patch_list_20_cp:
	case api::primitive_topology::patch_list_21_cp:
	case api::primitive_topology::patch_list_22_cp:
	case api::primitive_topology::patch_list_23_cp:
	case api::primitive_topology::patch_list_24_cp:
	case api::primitive_topology::patch_list_25_cp:
	case api::primitive_topology::patch_list_26_cp:
	case api::primitive_topology::patch_list_27_cp:
	case api::primitive_topology::patch_list_28_cp:
	case api::primitive_topology::patch_list_29_cp:
	case api::primitive_topology::patch_list_30_cp:
	case api::primitive_topology::patch_list_31_cp:
	case api::primitive_topology::patch_list_32_cp:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	}
}
auto reshade::d3d12::convert_primitive_topology_type(D3D12_PRIMITIVE_TOPOLOGY_TYPE value) -> api::primitive_topology
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED:
		return api::primitive_topology::undefined;
	case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT:
		return api::primitive_topology::point_list;
	case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE:
		return api::primitive_topology::line_list;
	case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE:
		return api::primitive_topology::triangle_list;
	case D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH:
		return api::primitive_topology::patch_list_01_cp;
	}
}

auto reshade::d3d12::convert_query_type(api::query_type type) -> D3D12_QUERY_TYPE
{
	return static_cast<D3D12_QUERY_TYPE>(type);
}
auto reshade::d3d12::convert_query_type(D3D12_QUERY_TYPE type) -> api::query_type
{
	return static_cast<api::query_type>(type);
}
auto reshade::d3d12::convert_query_type_to_heap_type(api::query_type type) -> D3D12_QUERY_HEAP_TYPE
{
	switch (type)
	{
	case api::query_type::occlusion:
	case api::query_type::binary_occlusion:
		return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	case api::query_type::timestamp:
		return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	case api::query_type::pipeline_statistics:
		return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
	default:
		assert(false);
		return static_cast<D3D12_QUERY_HEAP_TYPE>(0xFFFFFFFF);
	}
}

auto reshade::d3d12::convert_descriptor_type(api::descriptor_type type) -> D3D12_DESCRIPTOR_RANGE_TYPE
{
	switch (type)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::descriptor_type::shader_resource_view:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	case api::descriptor_type::unordered_access_view:
		return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	case reshade::api::descriptor_type::constant_buffer:
		return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	case reshade::api::descriptor_type::sampler:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	}
}
auto reshade::d3d12::convert_descriptor_type(D3D12_DESCRIPTOR_RANGE_TYPE type) -> api::descriptor_type
{
	switch (type)
	{
	default:
		assert(false);
		[[fallthrough]];
	case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
		return api::descriptor_type::shader_resource_view;
	case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
		return api::descriptor_type::unordered_access_view;
	case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
		return api::descriptor_type::constant_buffer;
	case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
		return api::descriptor_type::sampler;
	}
}
auto reshade::d3d12::convert_descriptor_type_to_heap_type(api::descriptor_type type) -> D3D12_DESCRIPTOR_HEAP_TYPE
{
	switch (type)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::descriptor_type::constant_buffer:
	case api::descriptor_type::shader_resource_view:
	case api::descriptor_type::unordered_access_view:
		return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	case api::descriptor_type::sampler:
		return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	}
}

auto reshade::d3d12::convert_shader_visibility(D3D12_SHADER_VISIBILITY visibility) -> api::shader_stage
{
	switch (visibility)
	{
	default:
		assert(false);
		[[fallthrough]];
	case D3D12_SHADER_VISIBILITY_ALL:
		return api::shader_stage::all;
	case D3D12_SHADER_VISIBILITY_VERTEX:
		return api::shader_stage::vertex;
	case D3D12_SHADER_VISIBILITY_HULL:
		return api::shader_stage::hull;
	case D3D12_SHADER_VISIBILITY_DOMAIN:
		return api::shader_stage::domain;
	case D3D12_SHADER_VISIBILITY_GEOMETRY:
		return api::shader_stage::geometry;
	case D3D12_SHADER_VISIBILITY_PIXEL:
		return api::shader_stage::pixel;
	}
}
