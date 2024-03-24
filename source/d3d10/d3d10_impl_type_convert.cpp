/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_impl_type_convert.hpp"
#include <limits>
#include <cassert>

auto reshade::d3d10::convert_format(api::format format) -> DXGI_FORMAT
{
	if (static_cast<uint32_t>(format) >= 1000)
		return DXGI_FORMAT_UNKNOWN;

	return static_cast<DXGI_FORMAT>(format);
}
auto reshade::d3d10::convert_format(DXGI_FORMAT format) -> api::format
{
	return static_cast<api::format>(format);
}

static void convert_memory_heap_to_d3d_usage(reshade::api::memory_heap heap, D3D10_USAGE &usage, UINT &cpu_access_flags)
{
	using namespace reshade;

	switch (heap)
	{
	case api::memory_heap::gpu_only:
		if (usage == D3D10_USAGE_IMMUTABLE)
			break;
		usage = D3D10_USAGE_DEFAULT;
		break;
	case api::memory_heap::cpu_to_gpu:
		if (usage == D3D10_USAGE_DEFAULT && cpu_access_flags == D3D10_CPU_ACCESS_WRITE)
			break;
		usage = D3D10_USAGE_DYNAMIC;
		cpu_access_flags |= D3D10_CPU_ACCESS_WRITE;
		break;
	case api::memory_heap::gpu_to_cpu:
		usage = D3D10_USAGE_STAGING;
		cpu_access_flags |= D3D10_CPU_ACCESS_READ;
		break;
	case api::memory_heap::cpu_only:
		usage = D3D10_USAGE_STAGING;
		if (cpu_access_flags == 0)
			cpu_access_flags |= D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE;
		break;
	}
}
static void convert_d3d_usage_to_memory_heap(D3D10_USAGE usage, UINT cpu_access_flags, reshade::api::memory_heap &heap)
{
	using namespace reshade;

	switch (usage)
	{
	case D3D10_USAGE_DEFAULT:
		if (cpu_access_flags == D3D10_CPU_ACCESS_WRITE)
		{
			heap = api::memory_heap::cpu_to_gpu;
			break;
		}
		[[fallthrough]];
	case D3D10_USAGE_IMMUTABLE:
		assert(cpu_access_flags == 0);
		heap = api::memory_heap::gpu_only;
		break;
	case D3D10_USAGE_DYNAMIC:
		assert(cpu_access_flags == D3D10_CPU_ACCESS_WRITE);
		heap = api::memory_heap::cpu_to_gpu;
		break;
	case D3D10_USAGE_STAGING:
		heap = cpu_access_flags == D3D10_CPU_ACCESS_READ ? api::memory_heap::gpu_to_cpu : api::memory_heap::cpu_only;
		break;
	}
}

static void convert_resource_usage_to_bind_flags(reshade::api::resource_usage usage, UINT &bind_flags)
{
	using namespace reshade;

	if ((usage & api::resource_usage::depth_stencil) != 0)
		bind_flags |= D3D10_BIND_DEPTH_STENCIL;
	else
		bind_flags &= ~D3D10_BIND_DEPTH_STENCIL;

	if ((usage & api::resource_usage::render_target) != 0)
		bind_flags |= D3D10_BIND_RENDER_TARGET;
	else
		bind_flags &= ~D3D10_BIND_RENDER_TARGET;

	if ((usage & api::resource_usage::shader_resource) != 0)
		bind_flags |= D3D10_BIND_SHADER_RESOURCE;
	else
		bind_flags &= ~D3D10_BIND_SHADER_RESOURCE;

	assert((usage & api::resource_usage::unordered_access) == 0);

	if ((usage & api::resource_usage::index_buffer) != 0)
		bind_flags |= D3D10_BIND_INDEX_BUFFER;
	else
		bind_flags &= ~D3D10_BIND_INDEX_BUFFER;

	if ((usage & api::resource_usage::vertex_buffer) != 0)
		bind_flags |= D3D10_BIND_VERTEX_BUFFER;
	else
		bind_flags &= ~D3D10_BIND_VERTEX_BUFFER;

	if ((usage & api::resource_usage::constant_buffer) != 0)
		bind_flags |= D3D10_BIND_CONSTANT_BUFFER;
	else
		bind_flags &= ~D3D10_BIND_CONSTANT_BUFFER;

	if ((usage & api::resource_usage::stream_output) != 0)
		bind_flags |= D3D10_BIND_STREAM_OUTPUT;
	else
		bind_flags &= ~D3D10_BIND_STREAM_OUTPUT;
}
static void convert_bind_flags_to_resource_usage(UINT bind_flags, reshade::api::resource_usage &usage)
{
	using namespace reshade;

	// Resources are generally copyable in D3D10
	usage |= api::resource_usage::copy_dest | api::resource_usage::copy_source;

	if ((bind_flags & D3D10_BIND_DEPTH_STENCIL) != 0)
		usage |= api::resource_usage::depth_stencil;
	if ((bind_flags & D3D10_BIND_RENDER_TARGET) != 0)
		usage |= api::resource_usage::render_target;
	if ((bind_flags & D3D10_BIND_SHADER_RESOURCE) != 0)
		usage |= api::resource_usage::shader_resource;

	if ((bind_flags & D3D10_BIND_INDEX_BUFFER) != 0)
		usage |= api::resource_usage::index_buffer;
	if ((bind_flags & D3D10_BIND_VERTEX_BUFFER) != 0)
		usage |= api::resource_usage::vertex_buffer;
	if ((bind_flags & D3D10_BIND_CONSTANT_BUFFER) != 0)
		usage |= api::resource_usage::constant_buffer;
	if ((bind_flags & D3D10_BIND_STREAM_OUTPUT) != 0)
		usage |= api::resource_usage::stream_output;
}

static void convert_resource_flags_to_misc_flags(reshade::api::resource_flags flags, UINT &misc_flags)
{
	using namespace reshade;

	if ((flags & api::resource_flags::shared) != 0 && (misc_flags & (D3D10_RESOURCE_MISC_SHARED | D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX)) == 0)
		misc_flags |= D3D10_RESOURCE_MISC_SHARED;

	if ((flags & api::resource_flags::cube_compatible) != 0)
		misc_flags |= D3D10_RESOURCE_MISC_TEXTURECUBE;
	else
		misc_flags &= ~D3D10_RESOURCE_MISC_TEXTURECUBE;

	if ((flags & api::resource_flags::generate_mipmaps) != 0)
		misc_flags |= D3D10_RESOURCE_MISC_GENERATE_MIPS;
	else
		misc_flags &= ~D3D10_RESOURCE_MISC_GENERATE_MIPS;
}
static void convert_misc_flags_to_resource_flags(UINT misc_flags, reshade::api::resource_flags &flags)
{
	using namespace reshade;

	if ((misc_flags & (D3D10_RESOURCE_MISC_SHARED | D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX)) != 0)
		flags |= api::resource_flags::shared;
	if ((misc_flags & D3D10_RESOURCE_MISC_TEXTURECUBE) != 0)
		flags |= api::resource_flags::cube_compatible;
	if ((misc_flags & D3D10_RESOURCE_MISC_GENERATE_MIPS) != 0)
		flags |= api::resource_flags::generate_mipmaps;
}

auto reshade::d3d10::convert_access_flags(api::map_access access) -> D3D10_MAP
{
	switch (access)
	{
	case api::map_access::read_only:
		return D3D10_MAP_READ;
	case api::map_access::write_only:
		return D3D10_MAP_WRITE;
	case api::map_access::read_write:
		return D3D10_MAP_READ_WRITE;
	case api::map_access::write_discard:
		return D3D10_MAP_WRITE_DISCARD;
	}
	return static_cast<D3D10_MAP>(0);
}
reshade::api::map_access reshade::d3d10::convert_access_flags(D3D10_MAP map_type)
{
	switch (map_type)
	{
	case D3D10_MAP_READ:
		return api::map_access::read_only;
	case D3D10_MAP_WRITE:
	case D3D10_MAP_WRITE_NO_OVERWRITE:
		return api::map_access::write_only;
	case D3D10_MAP_READ_WRITE:
		return api::map_access::read_write;
	case D3D10_MAP_WRITE_DISCARD:
		return api::map_access::write_discard;
	}
	return static_cast<api::map_access>(0);
}

void reshade::d3d10::convert_sampler_desc(const api::sampler_desc &desc, D3D10_SAMPLER_DESC &internal_desc)
{
	internal_desc.Filter = static_cast<D3D10_FILTER>(desc.filter);
	internal_desc.AddressU = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(desc.address_u);
	internal_desc.AddressV = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(desc.address_v);
	internal_desc.AddressW = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(desc.address_w);
	internal_desc.MipLODBias = desc.mip_lod_bias;
	internal_desc.MaxAnisotropy = static_cast<UINT>(desc.max_anisotropy);
	internal_desc.ComparisonFunc = convert_compare_op(desc.compare_op);
	std::copy_n(desc.border_color, 4, internal_desc.BorderColor);
	internal_desc.MinLOD = desc.min_lod;
	internal_desc.MaxLOD = desc.max_lod;
}
reshade::api::sampler_desc reshade::d3d10::convert_sampler_desc(const D3D10_SAMPLER_DESC &internal_desc)
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

void reshade::d3d10::convert_resource_desc(const api::resource_desc &desc, D3D10_BUFFER_DESC &internal_desc)
{
	assert(desc.type == api::resource_type::buffer);
	assert(desc.buffer.size <= std::numeric_limits<UINT>::max());
	internal_desc.ByteWidth = static_cast<UINT>(desc.buffer.size);
	convert_memory_heap_to_d3d_usage(desc.heap, internal_desc.Usage, internal_desc.CPUAccessFlags);
	convert_resource_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
	convert_resource_flags_to_misc_flags(desc.flags, internal_desc.MiscFlags);
}
void reshade::d3d10::convert_resource_desc(const api::resource_desc &desc, D3D10_TEXTURE1D_DESC &internal_desc)
{
	assert(desc.type == api::resource_type::texture_1d);
	internal_desc.Width = desc.texture.width;
	assert(desc.texture.height == 1);
	internal_desc.MipLevels = desc.texture.levels;
	internal_desc.ArraySize = desc.texture.depth_or_layers;
	internal_desc.Format = convert_format(desc.texture.format);
	assert(desc.texture.samples == 1);
	convert_memory_heap_to_d3d_usage(desc.heap, internal_desc.Usage, internal_desc.CPUAccessFlags);
	convert_resource_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
	convert_resource_flags_to_misc_flags(desc.flags, internal_desc.MiscFlags);

	// The 'D3D10_RESOURCE_MISC_GENERATE_MIPS' flag requires render target and shader resource bind flags
	if ((desc.flags & api::resource_flags::generate_mipmaps) != 0)
		internal_desc.BindFlags |= D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
}
void reshade::d3d10::convert_resource_desc(const api::resource_desc &desc, D3D10_TEXTURE2D_DESC &internal_desc)
{
	assert(desc.type == api::resource_type::texture_2d);
	internal_desc.Width = desc.texture.width;
	internal_desc.Height = desc.texture.height;
	internal_desc.MipLevels = desc.texture.levels;
	internal_desc.ArraySize = desc.texture.depth_or_layers;
	internal_desc.Format = convert_format(desc.texture.format);
	internal_desc.SampleDesc.Count = desc.texture.samples;
	convert_memory_heap_to_d3d_usage(desc.heap, internal_desc.Usage, internal_desc.CPUAccessFlags);
	convert_resource_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
	convert_resource_flags_to_misc_flags(desc.flags, internal_desc.MiscFlags);

	if ((desc.flags & api::resource_flags::generate_mipmaps) != 0)
		internal_desc.BindFlags |= D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
}
void reshade::d3d10::convert_resource_desc(const api::resource_desc &desc, D3D10_TEXTURE3D_DESC &internal_desc)
{
	assert(desc.type == api::resource_type::texture_3d);
	internal_desc.Width = desc.texture.width;
	internal_desc.Height = desc.texture.height;
	internal_desc.Depth = desc.texture.depth_or_layers;
	internal_desc.MipLevels = desc.texture.levels;
	internal_desc.Format = convert_format(desc.texture.format);
	assert(desc.texture.samples == 1);
	convert_memory_heap_to_d3d_usage(desc.heap, internal_desc.Usage, internal_desc.CPUAccessFlags);
	convert_resource_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
	convert_resource_flags_to_misc_flags(desc.flags, internal_desc.MiscFlags);

	if ((desc.flags & api::resource_flags::generate_mipmaps) != 0)
		internal_desc.BindFlags |= D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
}
reshade::api::resource_desc reshade::d3d10::convert_resource_desc(const D3D10_BUFFER_DESC &internal_desc)
{
	api::resource_desc desc = {};
	desc.type = api::resource_type::buffer;
	desc.buffer.size = internal_desc.ByteWidth;
	desc.buffer.stride = 0;
	convert_d3d_usage_to_memory_heap(internal_desc.Usage, internal_desc.CPUAccessFlags, desc.heap);
	convert_bind_flags_to_resource_usage(internal_desc.BindFlags, desc.usage);
	convert_misc_flags_to_resource_flags(internal_desc.MiscFlags, desc.flags);

	if (internal_desc.Usage == D3D10_USAGE_DYNAMIC)
		desc.flags |= api::resource_flags::dynamic;

	return desc;
}
reshade::api::resource_desc reshade::d3d10::convert_resource_desc(const D3D10_TEXTURE1D_DESC &internal_desc)
{
	api::resource_desc desc = {};
	desc.type = api::resource_type::texture_1d;
	desc.texture.width = internal_desc.Width;
	desc.texture.height = 1;
	assert(internal_desc.ArraySize <= std::numeric_limits<uint16_t>::max());
	desc.texture.depth_or_layers = static_cast<uint16_t>(internal_desc.ArraySize);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.texture.format = convert_format(internal_desc.Format);
	desc.texture.samples = 1;
	convert_d3d_usage_to_memory_heap(internal_desc.Usage, internal_desc.CPUAccessFlags, desc.heap);
	convert_bind_flags_to_resource_usage(internal_desc.BindFlags, desc.usage);
	convert_misc_flags_to_resource_flags(internal_desc.MiscFlags, desc.flags);

	if (internal_desc.Usage == D3D10_USAGE_DYNAMIC)
		desc.flags |= api::resource_flags::dynamic;

	return desc;
}
reshade::api::resource_desc reshade::d3d10::convert_resource_desc(const D3D10_TEXTURE2D_DESC &internal_desc)
{
	api::resource_desc desc = {};
	desc.type = api::resource_type::texture_2d;
	desc.texture.width = internal_desc.Width;
	desc.texture.height = internal_desc.Height;
	assert(internal_desc.ArraySize <= std::numeric_limits<uint16_t>::max());
	desc.texture.depth_or_layers = static_cast<uint16_t>(internal_desc.ArraySize);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.texture.format = convert_format(internal_desc.Format);
	desc.texture.samples = static_cast<uint16_t>(internal_desc.SampleDesc.Count);
	convert_d3d_usage_to_memory_heap(internal_desc.Usage, internal_desc.CPUAccessFlags, desc.heap);
	convert_bind_flags_to_resource_usage(internal_desc.BindFlags, desc.usage);
	desc.usage |= desc.texture.samples > 1 ? api::resource_usage::resolve_source : api::resource_usage::resolve_dest;
	convert_misc_flags_to_resource_flags(internal_desc.MiscFlags, desc.flags);

	if (internal_desc.Usage == D3D10_USAGE_DYNAMIC)
		desc.flags |= api::resource_flags::dynamic;

	return desc;
}
reshade::api::resource_desc reshade::d3d10::convert_resource_desc(const D3D10_TEXTURE3D_DESC &internal_desc)
{
	api::resource_desc desc = {};
	desc.type = api::resource_type::texture_3d;
	desc.texture.width = internal_desc.Width;
	desc.texture.height = internal_desc.Height;
	assert(internal_desc.Depth <= std::numeric_limits<uint16_t>::max());
	desc.texture.depth_or_layers = static_cast<uint16_t>(internal_desc.Depth);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.texture.format = convert_format(internal_desc.Format);
	desc.texture.samples = 1;
	convert_d3d_usage_to_memory_heap(internal_desc.Usage, internal_desc.CPUAccessFlags, desc.heap);
	convert_bind_flags_to_resource_usage(internal_desc.BindFlags, desc.usage);
	convert_misc_flags_to_resource_flags(internal_desc.MiscFlags, desc.flags);

	if (internal_desc.Usage == D3D10_USAGE_DYNAMIC)
		desc.flags |= api::resource_flags::dynamic;

	return desc;
}

void reshade::d3d10::convert_resource_view_desc(const api::resource_view_desc &desc, D3D10_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	internal_desc.Format = convert_format(desc.format);
	assert(desc.type != api::resource_view_type::buffer && desc.texture.level_count == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case api::resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layer_count;
		break;
	}
}
void reshade::d3d10::convert_resource_view_desc(const api::resource_view_desc &desc, D3D10_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	internal_desc.Format = convert_format(desc.format);
	assert(desc.type != api::resource_view_type::buffer && desc.texture.level_count == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case api::resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.texture.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.texture.first_layer;
		internal_desc.Texture3D.WSize = desc.texture.layer_count;
		break;
	}
}
void reshade::d3d10::convert_resource_view_desc(const api::resource_view_desc &desc, D3D10_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	internal_desc.Format = convert_format(desc.format);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case api::resource_view_type::buffer:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
		assert(desc.buffer.offset <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.FirstElement = static_cast<UINT>(desc.buffer.offset);
		assert(desc.buffer.size <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.NumElements = static_cast<UINT>(desc.buffer.size);
		break;
	case api::resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture1D.MipLevels = desc.texture.level_count;
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture1DArray.MipLevels = desc.texture.level_count;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture2D.MipLevels = desc.texture.level_count;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture2DArray.MipLevels = desc.texture.level_count;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture3D.MipLevels = desc.texture.level_count;
		break;
	case api::resource_view_type::texture_cube:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
		internal_desc.TextureCube.MostDetailedMip = desc.texture.first_level;
		internal_desc.TextureCube.MipLevels = desc.texture.level_count;
		break;
	}
}
void reshade::d3d10::convert_resource_view_desc(const api::resource_view_desc &desc, D3D10_SHADER_RESOURCE_VIEW_DESC1 &internal_desc)
{
	if (desc.type == api::resource_view_type::texture_cube_array)
	{
		internal_desc.Format = convert_format(desc.format);
		internal_desc.ViewDimension = D3D10_1_SRV_DIMENSION_TEXTURECUBEARRAY;
		internal_desc.TextureCubeArray.MostDetailedMip = desc.texture.first_level;
		internal_desc.TextureCubeArray.MipLevels = desc.texture.level_count;
		internal_desc.TextureCubeArray.First2DArrayFace = desc.texture.first_layer;
		if (desc.texture.layer_count == UINT32_MAX)
			internal_desc.TextureCubeArray.NumCubes = UINT_MAX;
		else
			internal_desc.TextureCubeArray.NumCubes = desc.texture.layer_count / 6;
	}
	else
	{
		convert_resource_view_desc(desc, reinterpret_cast<D3D10_SHADER_RESOURCE_VIEW_DESC &>(internal_desc));
	}
}
reshade::api::resource_view_desc reshade::d3d10::convert_resource_view_desc(const D3D10_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	desc.texture.level_count = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D10_DSV_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DMSArray.ArraySize;
		break;
	}
	return desc;
}
reshade::api::resource_view_desc reshade::d3d10::convert_resource_view_desc(const D3D10_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	desc.texture.level_count = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D10_RTV_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE3D:
		desc.type = api::resource_view_type::texture_3d;
		desc.texture.first_level = internal_desc.Texture3D.MipSlice;
		desc.texture.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.texture.layer_count = internal_desc.Texture3D.WSize;
		break;
	}
	return desc;
}
reshade::api::resource_view_desc reshade::d3d10::convert_resource_view_desc(const D3D10_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	switch (internal_desc.ViewDimension)
	{
	case D3D10_SRV_DIMENSION_BUFFER:
		desc.type = api::resource_view_type::buffer;
		desc.buffer.offset = internal_desc.Buffer.FirstElement;
		desc.buffer.size = internal_desc.Buffer.NumElements;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = internal_desc.Texture1D.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture1D.MipLevels;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture1DArray.MipLevels;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture2D.MipLevels;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture2DArray.MipLevels;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE3D:
		desc.type = api::resource_view_type::texture_3d;
		desc.texture.first_level = internal_desc.Texture3D.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture3D.MipLevels;
		break;
	case D3D10_SRV_DIMENSION_TEXTURECUBE:
		desc.type = api::resource_view_type::texture_cube;
		desc.texture.first_level = internal_desc.TextureCube.MostDetailedMip;
		desc.texture.level_count = internal_desc.TextureCube.MipLevels;
		break;
	}
	return desc;
}
reshade::api::resource_view_desc reshade::d3d10::convert_resource_view_desc(const D3D10_SHADER_RESOURCE_VIEW_DESC1 &internal_desc)
{
	if (internal_desc.ViewDimension == D3D10_1_SRV_DIMENSION_TEXTURECUBEARRAY)
	{
		api::resource_view_desc desc = {};
		desc.type = api::resource_view_type::texture_cube_array;
		desc.format = convert_format(internal_desc.Format);
		desc.texture.first_level = internal_desc.TextureCubeArray.MostDetailedMip;
		desc.texture.level_count = internal_desc.TextureCubeArray.MipLevels;
		desc.texture.first_layer = internal_desc.TextureCubeArray.First2DArrayFace;
		if (internal_desc.TextureCubeArray.NumCubes == UINT_MAX)
			desc.texture.layer_count = UINT32_MAX;
		else
			desc.texture.layer_count = internal_desc.TextureCubeArray.NumCubes * 6;
		return desc;
	}
	else
	{
		return convert_resource_view_desc(reinterpret_cast<const D3D10_SHADER_RESOURCE_VIEW_DESC &>(internal_desc));
	}
}

void reshade::d3d10::convert_input_layout_desc(uint32_t count, const api::input_element *elements, std::vector<D3D10_INPUT_ELEMENT_DESC> &internal_elements)
{
	internal_elements.reserve(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		const api::input_element &element = elements[i];

		D3D10_INPUT_ELEMENT_DESC &internal_element = internal_elements.emplace_back();
		internal_element.SemanticName = element.semantic;
		internal_element.SemanticIndex = element.semantic_index;
		internal_element.Format = convert_format(element.format);
		internal_element.InputSlot = element.buffer_binding;
		internal_element.AlignedByteOffset = element.offset;
		internal_element.InputSlotClass = element.instance_step_rate > 0 ? D3D10_INPUT_PER_INSTANCE_DATA : D3D10_INPUT_PER_VERTEX_DATA;
		internal_element.InstanceDataStepRate = element.instance_step_rate;
	}
}
std::vector<reshade::api::input_element> reshade::d3d10::convert_input_layout_desc(UINT count, const D3D10_INPUT_ELEMENT_DESC *internal_elements)
{
	std::vector<reshade::api::input_element> elements;
	elements.reserve(count);

	for (UINT i = 0; i < count; ++i)
	{
		const D3D10_INPUT_ELEMENT_DESC &internal_element = internal_elements[i];

		api::input_element &element = elements.emplace_back();
		element.semantic = internal_element.SemanticName;
		element.semantic_index = internal_element.SemanticIndex;
		element.format = convert_format(internal_element.Format);
		element.buffer_binding = internal_element.InputSlot;
		element.offset = internal_element.AlignedByteOffset;
		element.instance_step_rate = internal_element.InstanceDataStepRate;
	}

	return elements;
}

void reshade::d3d10::convert_blend_desc(const api::blend_desc &desc, D3D10_BLEND_DESC &internal_desc)
{
	internal_desc.AlphaToCoverageEnable = desc.alpha_to_coverage_enable;
	internal_desc.SrcBlend = convert_blend_factor(desc.source_color_blend_factor[0]);
	internal_desc.DestBlend = convert_blend_factor(desc.dest_color_blend_factor[0]);
	internal_desc.BlendOp = convert_blend_op(desc.color_blend_op[0]);
	internal_desc.SrcBlendAlpha = convert_blend_factor(desc.source_alpha_blend_factor[0]);
	internal_desc.DestBlendAlpha = convert_blend_factor(desc.dest_alpha_blend_factor[0]);
	internal_desc.BlendOpAlpha = convert_blend_op(desc.alpha_blend_op[0]);

	for (UINT i = 0; i < 8; ++i)
	{
		internal_desc.BlendEnable[i] = desc.blend_enable[i];
		internal_desc.RenderTargetWriteMask[i] = desc.render_target_write_mask[i];
	}
}
void reshade::d3d10::convert_blend_desc(const api::blend_desc &desc, D3D10_BLEND_DESC1 &internal_desc)
{
	internal_desc.AlphaToCoverageEnable = desc.alpha_to_coverage_enable;
	internal_desc.IndependentBlendEnable = FALSE;

	for (UINT i = 0; i < 8; ++i)
	{
		if (desc.blend_enable[i] != desc.blend_enable[0] ||
			desc.source_color_blend_factor[i] != desc.source_color_blend_factor[0] ||
			desc.dest_color_blend_factor[i] != desc.dest_color_blend_factor[0] ||
			desc.color_blend_op[i] != desc.color_blend_op[0] ||
			desc.source_alpha_blend_factor[i] != desc.source_alpha_blend_factor[0] ||
			desc.dest_alpha_blend_factor[i] != desc.dest_alpha_blend_factor[0] ||
			desc.alpha_blend_op[i] != desc.alpha_blend_op[0] ||
			desc.render_target_write_mask[i] != desc.render_target_write_mask[0])
			internal_desc.IndependentBlendEnable = TRUE;

		assert(!desc.logic_op_enable);

		internal_desc.RenderTarget[i].BlendEnable = desc.blend_enable[i];
		internal_desc.RenderTarget[i].SrcBlend = convert_blend_factor(desc.source_color_blend_factor[i]);
		internal_desc.RenderTarget[i].DestBlend = convert_blend_factor(desc.dest_color_blend_factor[i]);
		internal_desc.RenderTarget[i].BlendOp = convert_blend_op(desc.color_blend_op[i]);
		internal_desc.RenderTarget[i].SrcBlendAlpha = convert_blend_factor(desc.source_alpha_blend_factor[i]);
		internal_desc.RenderTarget[i].DestBlendAlpha = convert_blend_factor(desc.dest_alpha_blend_factor[i]);
		internal_desc.RenderTarget[i].BlendOpAlpha = convert_blend_op(desc.alpha_blend_op[i]);
		internal_desc.RenderTarget[i].RenderTargetWriteMask = desc.render_target_write_mask[i];
	}
}
reshade::api::blend_desc reshade::d3d10::convert_blend_desc(const D3D10_BLEND_DESC *internal_desc)
{
	api::blend_desc desc = {};

	if (internal_desc != nullptr)
	{
		desc.alpha_to_coverage_enable = internal_desc->AlphaToCoverageEnable;

		for (UINT i = 0; i < 8; ++i)
		{
			desc.blend_enable[i] = internal_desc->BlendEnable[i];

			// Only convert blend state if blending is enabled (since some applications leave these uninitialized in this case)
			if (internal_desc->BlendEnable[i])
			{
				desc.source_color_blend_factor[i] = convert_blend_factor(internal_desc->SrcBlend);
				desc.dest_color_blend_factor[i] = convert_blend_factor(internal_desc->DestBlend);
				desc.color_blend_op[i] = convert_blend_op(internal_desc->BlendOp);
				desc.source_alpha_blend_factor[i] = convert_blend_factor(internal_desc->SrcBlendAlpha);
				desc.dest_alpha_blend_factor[i] = convert_blend_factor(internal_desc->DestBlendAlpha);
				desc.alpha_blend_op[i] = convert_blend_op(internal_desc->BlendOpAlpha);
				desc.render_target_write_mask[i] = internal_desc->RenderTargetWriteMask[i];
			}
		}
	}
	else
	{
		// Default blend state (https://docs.microsoft.com/windows/win32/api/d3d10/ns-d3d10-d3d10_blend_desc)
		for (UINT i = 0; i < 8; ++i)
		{
			desc.source_color_blend_factor[i] = api::blend_factor::one;
			desc.dest_color_blend_factor[i] = api::blend_factor::zero;
			desc.color_blend_op[i] = api::blend_op::add;
			desc.source_alpha_blend_factor[i] = api::blend_factor::one;
			desc.dest_alpha_blend_factor[i] = api::blend_factor::zero;
			desc.alpha_blend_op[i] = api::blend_op::add;
			desc.render_target_write_mask[i] = D3D10_COLOR_WRITE_ENABLE_ALL;
		}
	}

	return desc;
}
reshade::api::blend_desc reshade::d3d10::convert_blend_desc(const D3D10_BLEND_DESC1 *internal_desc)
{
	api::blend_desc desc = {};

	if (internal_desc != nullptr)
	{
		desc.alpha_to_coverage_enable = internal_desc->AlphaToCoverageEnable;

		for (UINT i = 0; i < 8; ++i)
		{
			const D3D10_RENDER_TARGET_BLEND_DESC1 &target = internal_desc->RenderTarget[internal_desc->IndependentBlendEnable ? i : 0];

			desc.blend_enable[i] = target.BlendEnable;

			// Only convert blend state if blending is enabled (since some applications leave these uninitialized in this case)
			if (target.BlendEnable)
			{
				desc.source_color_blend_factor[i] = convert_blend_factor(target.SrcBlend);
				desc.dest_color_blend_factor[i] = convert_blend_factor(target.DestBlend);
				desc.color_blend_op[i] = convert_blend_op(target.BlendOp);
				desc.source_alpha_blend_factor[i] = convert_blend_factor(target.SrcBlendAlpha);
				desc.dest_alpha_blend_factor[i] = convert_blend_factor(target.DestBlendAlpha);
				desc.alpha_blend_op[i] = convert_blend_op(target.BlendOpAlpha);
			}

			desc.render_target_write_mask[i] = target.RenderTargetWriteMask;
		}
	}
	else
	{
		// Default blend state (https://docs.microsoft.com/windows/win32/api/d3d10_1/ns-d3d10_1-d3d10_blend_desc1)
		for (UINT i = 0; i < 8; ++i)
		{
			desc.source_color_blend_factor[i] = api::blend_factor::one;
			desc.dest_color_blend_factor[i] = api::blend_factor::zero;
			desc.color_blend_op[i] = api::blend_op::add;
			desc.source_alpha_blend_factor[i] = api::blend_factor::one;
			desc.dest_alpha_blend_factor[i] = api::blend_factor::zero;
			desc.alpha_blend_op[i] = api::blend_op::add;
			desc.render_target_write_mask[i] = D3D10_COLOR_WRITE_ENABLE_ALL;
		}
	}

	return desc;
}
void reshade::d3d10::convert_rasterizer_desc(const api::rasterizer_desc &desc, D3D10_RASTERIZER_DESC &internal_desc)
{
	internal_desc.FillMode = convert_fill_mode(desc.fill_mode);
	internal_desc.CullMode = convert_cull_mode(desc.cull_mode);
	internal_desc.FrontCounterClockwise = desc.front_counter_clockwise;
	internal_desc.DepthBias = static_cast<INT>(desc.depth_bias);
	internal_desc.DepthBiasClamp = desc.depth_bias_clamp;
	internal_desc.SlopeScaledDepthBias = desc.slope_scaled_depth_bias;
	internal_desc.DepthClipEnable = desc.depth_clip_enable;
	internal_desc.ScissorEnable = desc.scissor_enable;
	internal_desc.MultisampleEnable = desc.multisample_enable;
	internal_desc.AntialiasedLineEnable = desc.antialiased_line_enable;
}
reshade::api::rasterizer_desc reshade::d3d10::convert_rasterizer_desc(const D3D10_RASTERIZER_DESC *internal_desc)
{
	api::rasterizer_desc desc = {};

	if (internal_desc != nullptr)
	{
		desc.fill_mode = convert_fill_mode(internal_desc->FillMode);
		desc.cull_mode = convert_cull_mode(internal_desc->CullMode);
		desc.front_counter_clockwise = internal_desc->FrontCounterClockwise;
		desc.depth_bias = static_cast<float>(internal_desc->DepthBias);
		desc.depth_bias_clamp = internal_desc->DepthBiasClamp;
		desc.slope_scaled_depth_bias = internal_desc->SlopeScaledDepthBias;
		desc.depth_clip_enable = internal_desc->DepthClipEnable;
		desc.scissor_enable = internal_desc->ScissorEnable;
		desc.multisample_enable = internal_desc->MultisampleEnable;
		desc.antialiased_line_enable = internal_desc->AntialiasedLineEnable;
	}
	else
	{
		// Default rasterizer state (https://docs.microsoft.com/windows/win32/api/d3d10/ns-d3d10-d3d10_rasterizer_desc)
		desc.fill_mode = api::fill_mode::solid;
		desc.cull_mode = api::cull_mode::back;
		desc.depth_clip_enable = true;
	}

	return desc;
}
void reshade::d3d10::convert_depth_stencil_desc(const api::depth_stencil_desc &desc, D3D10_DEPTH_STENCIL_DESC &internal_desc)
{
	internal_desc.DepthEnable = desc.depth_enable;
	internal_desc.DepthWriteMask = desc.depth_write_mask ? D3D10_DEPTH_WRITE_MASK_ALL : D3D10_DEPTH_WRITE_MASK_ZERO;
	internal_desc.DepthFunc = convert_compare_op(desc.depth_func);
	internal_desc.StencilEnable = desc.stencil_enable;
	internal_desc.StencilReadMask = desc.front_stencil_read_mask;
	internal_desc.StencilWriteMask = desc.front_stencil_write_mask;
	internal_desc.FrontFace.StencilFailOp = convert_stencil_op(desc.front_stencil_fail_op);
	internal_desc.FrontFace.StencilDepthFailOp = convert_stencil_op(desc.front_stencil_depth_fail_op);
	internal_desc.FrontFace.StencilPassOp = convert_stencil_op(desc.front_stencil_pass_op);
	internal_desc.FrontFace.StencilFunc = convert_compare_op(desc.front_stencil_func);
	internal_desc.BackFace.StencilFailOp = convert_stencil_op(desc.back_stencil_fail_op);
	internal_desc.BackFace.StencilDepthFailOp = convert_stencil_op(desc.back_stencil_depth_fail_op);
	internal_desc.BackFace.StencilPassOp = convert_stencil_op(desc.back_stencil_pass_op);
	internal_desc.BackFace.StencilFunc = convert_compare_op(desc.back_stencil_func);
}
reshade::api::depth_stencil_desc reshade::d3d10::convert_depth_stencil_desc(const D3D10_DEPTH_STENCIL_DESC *internal_desc)
{
	api::depth_stencil_desc desc = {};

	if (internal_desc != nullptr)
	{
		desc.depth_enable = internal_desc->DepthEnable;
		desc.depth_write_mask = internal_desc->DepthWriteMask != D3D10_DEPTH_WRITE_MASK_ZERO;
		desc.depth_func = convert_compare_op(internal_desc->DepthFunc);
		desc.stencil_enable = internal_desc->StencilEnable;
		desc.front_stencil_read_mask = internal_desc->StencilReadMask;
		desc.front_stencil_write_mask = internal_desc->StencilWriteMask;
		desc.front_stencil_reference_value = D3D10_DEFAULT_STENCIL_REFERENCE;
		desc.front_stencil_func = convert_compare_op(internal_desc->FrontFace.StencilFunc);
		desc.front_stencil_fail_op = convert_stencil_op(internal_desc->FrontFace.StencilFailOp);
		desc.front_stencil_depth_fail_op = convert_stencil_op(internal_desc->FrontFace.StencilDepthFailOp);
		desc.front_stencil_pass_op = convert_stencil_op(internal_desc->FrontFace.StencilPassOp);
		desc.back_stencil_read_mask = internal_desc->StencilReadMask;
		desc.back_stencil_write_mask = internal_desc->StencilWriteMask;
		desc.back_stencil_reference_value = D3D10_DEFAULT_STENCIL_REFERENCE;
		desc.back_stencil_func = convert_compare_op(internal_desc->BackFace.StencilFunc);
		desc.back_stencil_fail_op = convert_stencil_op(internal_desc->BackFace.StencilFailOp);
		desc.back_stencil_depth_fail_op = convert_stencil_op(internal_desc->BackFace.StencilDepthFailOp);
		desc.back_stencil_pass_op = convert_stencil_op(internal_desc->BackFace.StencilPassOp);
	}
	else
	{
		// Default depth-stencil state (https://docs.microsoft.com/windows/win32/api/d3d10/ns-d3d10-d3d10_depth_stencil_desc)
		desc.depth_enable = true;
		desc.depth_write_mask = true;
		desc.depth_func = api::compare_op::less;
		desc.front_stencil_read_mask = D3D10_DEFAULT_STENCIL_READ_MASK;
		desc.front_stencil_write_mask = D3D10_DEFAULT_STENCIL_WRITE_MASK;
		desc.front_stencil_reference_value = D3D10_DEFAULT_STENCIL_REFERENCE;
		desc.front_stencil_func = api::compare_op::always;
		desc.front_stencil_fail_op = api::stencil_op::keep;
		desc.front_stencil_depth_fail_op = api::stencil_op::keep;
		desc.front_stencil_pass_op = api::stencil_op::keep;
		desc.back_stencil_read_mask = D3D10_DEFAULT_STENCIL_READ_MASK;
		desc.back_stencil_write_mask = D3D10_DEFAULT_STENCIL_WRITE_MASK;
		desc.back_stencil_reference_value = D3D10_DEFAULT_STENCIL_REFERENCE;
		desc.back_stencil_func = api::compare_op::always;
		desc.back_stencil_fail_op = api::stencil_op::keep;
		desc.back_stencil_depth_fail_op = api::stencil_op::keep;
		desc.back_stencil_pass_op = api::stencil_op::keep;
	}

	return desc;
}

auto reshade::d3d10::convert_blend_op(api::blend_op value) -> D3D10_BLEND_OP
{
	return static_cast<D3D10_BLEND_OP>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d10::convert_blend_op(D3D10_BLEND_OP value) -> api::blend_op
{
	return static_cast<api::blend_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d10::convert_blend_factor(api::blend_factor value) -> D3D10_BLEND
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::zero:
		return D3D10_BLEND_ZERO;
	case api::blend_factor::one:
		return D3D10_BLEND_ONE;
	case api::blend_factor::source_color:
		return D3D10_BLEND_SRC_COLOR;
	case api::blend_factor::one_minus_source_color:
		return D3D10_BLEND_INV_SRC_COLOR;
	case api::blend_factor::dest_color:
		return D3D10_BLEND_DEST_COLOR;
	case api::blend_factor::one_minus_dest_color:
		return D3D10_BLEND_INV_DEST_COLOR;
	case api::blend_factor::source_alpha:
		return D3D10_BLEND_SRC_ALPHA;
	case api::blend_factor::one_minus_source_alpha:
		return D3D10_BLEND_INV_SRC_ALPHA;
	case api::blend_factor::dest_alpha:
		return D3D10_BLEND_DEST_ALPHA;
	case api::blend_factor::one_minus_dest_alpha:
		return D3D10_BLEND_INV_DEST_ALPHA;
	case api::blend_factor::constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::constant_color:
		return D3D10_BLEND_BLEND_FACTOR;
	case api::blend_factor::one_minus_constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::one_minus_constant_color:
		return D3D10_BLEND_INV_BLEND_FACTOR;
	case api::blend_factor::source_alpha_saturate:
		return D3D10_BLEND_SRC_ALPHA_SAT;
	case api::blend_factor::source1_color:
		return D3D10_BLEND_SRC1_COLOR;
	case api::blend_factor::one_minus_source1_color:
		return D3D10_BLEND_INV_SRC1_COLOR;
	case api::blend_factor::source1_alpha:
		return D3D10_BLEND_SRC1_ALPHA;
	case api::blend_factor::one_minus_source1_alpha:
		return D3D10_BLEND_INV_SRC1_ALPHA;
	}
}
auto reshade::d3d10::convert_blend_factor(D3D10_BLEND value) -> api::blend_factor
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case D3D10_BLEND_ZERO:
		return api::blend_factor::zero;
	case D3D10_BLEND_ONE:
		return api::blend_factor::one;
	case D3D10_BLEND_SRC_COLOR:
		return api::blend_factor::source_color;
	case D3D10_BLEND_INV_SRC_COLOR:
		return api::blend_factor::one_minus_source_color;
	case D3D10_BLEND_DEST_COLOR:
		return api::blend_factor::dest_color;
	case D3D10_BLEND_INV_DEST_COLOR:
		return api::blend_factor::one_minus_dest_color;
	case D3D10_BLEND_SRC_ALPHA:
		return api::blend_factor::source_alpha;
	case D3D10_BLEND_INV_SRC_ALPHA:
		return api::blend_factor::one_minus_source_alpha;
	case D3D10_BLEND_DEST_ALPHA:
		return api::blend_factor::dest_alpha;
	case D3D10_BLEND_INV_DEST_ALPHA:
		return api::blend_factor::one_minus_dest_alpha;
	case D3D10_BLEND_BLEND_FACTOR:
		return api::blend_factor::constant_color;
	case D3D10_BLEND_INV_BLEND_FACTOR:
		return api::blend_factor::one_minus_constant_color;
	case D3D10_BLEND_SRC_ALPHA_SAT:
		return api::blend_factor::source_alpha_saturate;
	case D3D10_BLEND_SRC1_COLOR:
		return api::blend_factor::source1_color;
	case D3D10_BLEND_INV_SRC1_COLOR:
		return api::blend_factor::one_minus_source1_color;
	case D3D10_BLEND_SRC1_ALPHA:
		return api::blend_factor::source1_alpha;
	case D3D10_BLEND_INV_SRC1_ALPHA:
		return api::blend_factor::one_minus_source1_alpha;
	}
}
auto reshade::d3d10::convert_fill_mode(api::fill_mode value) -> D3D10_FILL_MODE
{
	switch (value)
	{
	default:
	case api::fill_mode::point:
		assert(false);
		[[fallthrough]];
	case api::fill_mode::solid:
		return D3D10_FILL_SOLID;
	case api::fill_mode::wireframe:
		return D3D10_FILL_WIREFRAME;
	}
}
auto reshade::d3d10::convert_fill_mode(D3D10_FILL_MODE value) -> api::fill_mode
{
	switch (value)
	{
	default:
	case D3D10_FILL_SOLID:
		return api::fill_mode::solid;
	case D3D10_FILL_WIREFRAME:
		return api::fill_mode::wireframe;
	}
}
auto reshade::d3d10::convert_cull_mode(api::cull_mode value) -> D3D10_CULL_MODE
{
	assert(value != api::cull_mode::front_and_back);
	return static_cast<D3D10_CULL_MODE>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d10::convert_cull_mode(D3D10_CULL_MODE value) -> api::cull_mode
{
	return static_cast<api::cull_mode>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d10::convert_compare_op(api::compare_op value) -> D3D10_COMPARISON_FUNC
{
	return static_cast<D3D10_COMPARISON_FUNC>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d10::convert_compare_op(D3D10_COMPARISON_FUNC value) -> api::compare_op
{
	return static_cast<api::compare_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d10::convert_stencil_op(api::stencil_op value) -> D3D10_STENCIL_OP
{
	return static_cast<D3D10_STENCIL_OP>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d10::convert_stencil_op(D3D10_STENCIL_OP value) -> api::stencil_op
{
	return static_cast<api::stencil_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d10::convert_primitive_topology(api::primitive_topology value) -> D3D10_PRIMITIVE_TOPOLOGY
{
	static_assert(
		(DWORD)reshade::api::primitive_topology::point_list == D3D10_PRIMITIVE_TOPOLOGY_POINTLIST &&
		(DWORD)reshade::api::primitive_topology::line_list == D3D10_PRIMITIVE_TOPOLOGY_LINELIST &&
		(DWORD)reshade::api::primitive_topology::line_strip == D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP &&
		(DWORD)reshade::api::primitive_topology::triangle_list == D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST &&
		(DWORD)reshade::api::primitive_topology::triangle_strip == D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP &&
		(DWORD)reshade::api::primitive_topology::line_list_adj == D3D10_PRIMITIVE_TOPOLOGY_LINELIST_ADJ &&
		(DWORD)reshade::api::primitive_topology::line_strip_adj == D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ &&
		(DWORD)reshade::api::primitive_topology::triangle_list_adj == D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ &&
		(DWORD)reshade::api::primitive_topology::triangle_strip_adj == D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ);

	return static_cast<D3D10_PRIMITIVE_TOPOLOGY>(value);
}
auto reshade::d3d10::convert_primitive_topology(D3D10_PRIMITIVE_TOPOLOGY value) -> api::primitive_topology
{
	return static_cast<api::primitive_topology>(value);
}
auto reshade::d3d10::convert_query_type(api::query_type value) -> D3D10_QUERY
{
	switch (value)
	{
	case api::query_type::occlusion:
		return D3D10_QUERY_OCCLUSION;
	case api::query_type::binary_occlusion:
		return D3D10_QUERY_OCCLUSION_PREDICATE;
	case api::query_type::timestamp:
		return D3D10_QUERY_TIMESTAMP;
	case api::query_type::pipeline_statistics:
		return D3D10_QUERY_PIPELINE_STATISTICS;
	case api::query_type::stream_output_statistics_0: // D3D10 only supports a single stream
		return D3D10_QUERY_SO_STATISTICS;
	default:
		assert(false);
		return static_cast<D3D10_QUERY>(UINT_MAX);
	}
}
