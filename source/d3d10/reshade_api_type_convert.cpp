/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "reshade_api_device.hpp"
#include "reshade_api_type_convert.hpp"

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

	if ((usage & api::resource_usage::depth_stencil) != api::resource_usage::undefined)
		bind_flags |= D3D10_BIND_DEPTH_STENCIL;
	else
		bind_flags &= ~D3D10_BIND_DEPTH_STENCIL;

	if ((usage & api::resource_usage::render_target) != api::resource_usage::undefined)
		bind_flags |= D3D10_BIND_RENDER_TARGET;
	else
		bind_flags &= ~D3D10_BIND_RENDER_TARGET;

	if ((usage & api::resource_usage::shader_resource) != api::resource_usage::undefined)
		bind_flags |= D3D10_BIND_SHADER_RESOURCE;
	else
		bind_flags &= ~D3D10_BIND_SHADER_RESOURCE;

	assert((usage & api::resource_usage::unordered_access) == api::resource_usage::undefined);

	if ((usage & api::resource_usage::index_buffer) != api::resource_usage::undefined)
		bind_flags |= D3D10_BIND_INDEX_BUFFER;
	else
		bind_flags &= ~D3D10_BIND_INDEX_BUFFER;

	if ((usage & api::resource_usage::vertex_buffer) != api::resource_usage::undefined)
		bind_flags |= D3D10_BIND_VERTEX_BUFFER;
	else
		bind_flags &= ~D3D10_BIND_VERTEX_BUFFER;

	if ((usage & api::resource_usage::constant_buffer) != api::resource_usage::undefined)
		bind_flags |= D3D10_BIND_CONSTANT_BUFFER;
	else
		bind_flags &= ~D3D10_BIND_CONSTANT_BUFFER;
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
}

static void convert_resource_flags_to_misc_flags(reshade::api::resource_flags flags, UINT &misc_flags)
{
	using namespace reshade;

	if ((flags & api::resource_flags::shared) == api::resource_flags::shared && (misc_flags & (D3D10_RESOURCE_MISC_SHARED | D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX)) == 0)
		misc_flags |= D3D10_RESOURCE_MISC_SHARED;

	if ((flags & api::resource_flags::cube_compatible) == api::resource_flags::cube_compatible)
		misc_flags |= D3D10_RESOURCE_MISC_TEXTURECUBE;
	else
		misc_flags &= ~D3D10_RESOURCE_MISC_TEXTURECUBE;

	if ((flags & api::resource_flags::generate_mipmaps) == api::resource_flags::generate_mipmaps)
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

void reshade::d3d10::convert_sampler_desc(const api::sampler_desc &desc, D3D10_SAMPLER_DESC &internal_desc)
{
	internal_desc.Filter = static_cast<D3D10_FILTER>(desc.filter);
	internal_desc.AddressU = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(desc.address_u);
	internal_desc.AddressV = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(desc.address_v);
	internal_desc.AddressW = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(desc.address_w);
	internal_desc.MipLODBias = desc.mip_lod_bias;
	internal_desc.MaxAnisotropy = static_cast<UINT>(desc.max_anisotropy);
	internal_desc.ComparisonFunc = convert_compare_op(desc.compare_op);
	internal_desc.MinLOD = desc.min_lod;
	internal_desc.MaxLOD = desc.max_lod;
}
reshade::api::sampler_desc reshade::d3d10::convert_sampler_desc(const D3D10_SAMPLER_DESC &internal_desc)
{
	api::sampler_desc desc = {};
	desc.filter = static_cast<api::filter_type>(internal_desc.Filter);
	desc.address_u = static_cast<api::texture_address_mode>(internal_desc.AddressU);
	desc.address_v = static_cast<api::texture_address_mode>(internal_desc.AddressV);
	desc.address_w = static_cast<api::texture_address_mode>(internal_desc.AddressW);
	desc.mip_lod_bias = internal_desc.MipLODBias;
	desc.max_anisotropy = static_cast<float>(internal_desc.MaxAnisotropy);
	desc.compare_op = convert_compare_op(internal_desc.ComparisonFunc);
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
	if ((desc.flags & api::resource_flags::generate_mipmaps) == api::resource_flags::generate_mipmaps)
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

	if ((desc.flags & api::resource_flags::generate_mipmaps) == api::resource_flags::generate_mipmaps)
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

	if ((desc.flags & api::resource_flags::generate_mipmaps) == api::resource_flags::generate_mipmaps)
		internal_desc.BindFlags |= D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
}
reshade::api::resource_desc reshade::d3d10::convert_resource_desc(const D3D10_BUFFER_DESC &internal_desc)
{
	api::resource_desc desc = {};
	desc.type = api::resource_type::buffer;
	desc.buffer.size = internal_desc.ByteWidth;
	convert_d3d_usage_to_memory_heap(internal_desc.Usage, internal_desc.CPUAccessFlags, desc.heap);
	convert_bind_flags_to_resource_usage(internal_desc.BindFlags, desc.usage);
	convert_misc_flags_to_resource_flags(internal_desc.MiscFlags, desc.flags);
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
	return desc;
}

void reshade::d3d10::convert_resource_view_desc(const api::resource_view_desc &desc, D3D10_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	internal_desc.Format = convert_format(desc.format);
	assert(desc.type != api::resource_view_type::buffer && desc.texture.levels == 1);
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
		internal_desc.Texture1DArray.ArraySize = desc.texture.layers;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layers;
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layers;
		break;
	}
}
void reshade::d3d10::convert_resource_view_desc(const api::resource_view_desc &desc, D3D10_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	internal_desc.Format = convert_format(desc.format);
	assert(desc.type != api::resource_view_type::buffer && desc.texture.levels == 1);
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
		internal_desc.Texture1DArray.ArraySize = desc.texture.layers;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layers;
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layers;
		break;
	case api::resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.texture.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.texture.first_layer;
		internal_desc.Texture3D.WSize = desc.texture.layers;
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
		internal_desc.Texture1D.MipLevels = desc.texture.levels;
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture1DArray.MipLevels = desc.texture.levels;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layers;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture2D.MipLevels = desc.texture.levels;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture2DArray.MipLevels = desc.texture.levels;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layers;
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layers;
		break;
	case api::resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture3D.MipLevels = desc.texture.levels;
		break;
	case api::resource_view_type::texture_cube:
		internal_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
		internal_desc.TextureCube.MostDetailedMip = desc.texture.first_level;
		internal_desc.TextureCube.MipLevels = desc.texture.levels;
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
		internal_desc.TextureCubeArray.MipLevels = desc.texture.levels;
		internal_desc.TextureCubeArray.First2DArrayFace = desc.texture.first_layer;
		if (desc.texture.layers == 0xFFFFFFFF)
			internal_desc.TextureCubeArray.NumCubes = 0xFFFFFFFF;
		else
			internal_desc.TextureCubeArray.NumCubes = desc.texture.layers / 6;
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
	desc.texture.levels = 1;
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
		desc.texture.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D10_DSV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	}
	return desc;
}
reshade::api::resource_view_desc reshade::d3d10::convert_resource_view_desc(const D3D10_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	desc.texture.levels = 1;
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
		desc.texture.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D10_RTV_DIMENSION_TEXTURE3D:
		desc.type = api::resource_view_type::texture_3d;
		desc.texture.first_level = internal_desc.Texture3D.MipSlice;
		desc.texture.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.texture.layers = internal_desc.Texture3D.WSize;
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
		desc.texture.levels = internal_desc.Texture1D.MipLevels;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MostDetailedMip;
		desc.texture.levels = internal_desc.Texture1DArray.MipLevels;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MostDetailedMip;
		desc.texture.levels = internal_desc.Texture2D.MipLevels;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MostDetailedMip;
		desc.texture.levels = internal_desc.Texture2DArray.MipLevels;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D10_SRV_DIMENSION_TEXTURE3D:
		desc.type = api::resource_view_type::texture_3d;
		desc.texture.first_level = internal_desc.Texture3D.MostDetailedMip;
		desc.texture.levels = internal_desc.Texture3D.MipLevels;
		break;
	case D3D10_SRV_DIMENSION_TEXTURECUBE:
		desc.type = api::resource_view_type::texture_cube;
		desc.texture.first_level = internal_desc.TextureCube.MostDetailedMip;
		desc.texture.levels = internal_desc.TextureCube.MipLevels;
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
		desc.texture.levels = internal_desc.TextureCubeArray.MipLevels;
		desc.texture.first_layer = internal_desc.TextureCubeArray.First2DArrayFace;
		if (internal_desc.TextureCubeArray.NumCubes == 0xFFFFFFFF)
			desc.texture.layers = 0xFFFFFFFF;
		else
			desc.texture.layers = internal_desc.TextureCubeArray.NumCubes * 6;
		return desc;
	}
	else
	{
		return convert_resource_view_desc(reinterpret_cast<const D3D10_SHADER_RESOURCE_VIEW_DESC &>(internal_desc));
	}
}

auto reshade::d3d10::convert_blend_op(api::blend_op value) -> D3D10_BLEND_OP
{
	return static_cast<D3D10_BLEND_OP>(static_cast<uint32_t>(value) + 1);
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
	case api::blend_factor::src_color:
		return D3D10_BLEND_SRC_COLOR;
	case api::blend_factor::inv_src_color:
		return D3D10_BLEND_INV_SRC_COLOR;
	case api::blend_factor::dst_color:
		return D3D10_BLEND_DEST_COLOR;
	case api::blend_factor::inv_dst_color:
		return D3D10_BLEND_INV_DEST_COLOR;
	case api::blend_factor::src_alpha:
		return D3D10_BLEND_SRC_ALPHA;
	case api::blend_factor::inv_src_alpha:
		return D3D10_BLEND_INV_SRC_ALPHA;
	case api::blend_factor::dst_alpha:
		return D3D10_BLEND_DEST_ALPHA;
	case api::blend_factor::inv_dst_alpha:
		return D3D10_BLEND_INV_DEST_ALPHA;
	case api::blend_factor::constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::constant_color:
		return D3D10_BLEND_BLEND_FACTOR;
	case api::blend_factor::inv_constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::inv_constant_color:
		return D3D10_BLEND_INV_BLEND_FACTOR;
	case api::blend_factor::src_alpha_sat:
		return D3D10_BLEND_SRC_ALPHA_SAT;
	case api::blend_factor::src1_color:
		return D3D10_BLEND_SRC1_COLOR;
	case api::blend_factor::inv_src1_color:
		return D3D10_BLEND_INV_SRC1_COLOR;
	case api::blend_factor::src1_alpha:
		return D3D10_BLEND_SRC1_ALPHA;
	case api::blend_factor::inv_src1_alpha:
		return D3D10_BLEND_INV_SRC1_ALPHA;
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
auto reshade::d3d10::convert_cull_mode(api::cull_mode value) -> D3D10_CULL_MODE
{
	assert(value != api::cull_mode::front_and_back);
	return static_cast<D3D10_CULL_MODE>(static_cast<uint32_t>(value) + 1);
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
auto reshade::d3d10::convert_primitive_topology(api::primitive_topology value) -> D3D10_PRIMITIVE_TOPOLOGY
{
	assert(value != api::primitive_topology::triangle_fan);
	return static_cast<D3D10_PRIMITIVE_TOPOLOGY>(value);
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
	default:
		assert(false);
		return static_cast<D3D10_QUERY>(0xFFFFFFFF);
	}
}
