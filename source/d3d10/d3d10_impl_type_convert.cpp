/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d10_impl_device.hpp"
#include "d3d10_impl_type_convert.hpp"

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
		if (desc.texture.layer_count == 0xFFFFFFFF)
			internal_desc.TextureCubeArray.NumCubes = 0xFFFFFFFF;
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
		if (internal_desc.TextureCubeArray.NumCubes == 0xFFFFFFFF)
			desc.texture.layer_count = 0xFFFFFFFF;
		else
			desc.texture.layer_count = internal_desc.TextureCubeArray.NumCubes * 6;
		return desc;
	}
	else
	{
		return convert_resource_view_desc(reinterpret_cast<const D3D10_SHADER_RESOURCE_VIEW_DESC &>(internal_desc));
	}
}

void reshade::d3d10::convert_pipeline_desc(const api::pipeline_desc &desc, std::vector<D3D10_INPUT_ELEMENT_DESC> &internal_elements)
{
	assert(desc.type == api::pipeline_stage::all_graphics || desc.type == api::pipeline_stage::input_assembler);
	internal_elements.reserve(16);

	for (UINT i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
	{
		const api::input_layout_element &element = desc.graphics.input_layout[i];

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
void reshade::d3d10::convert_pipeline_desc(const api::pipeline_desc &desc, D3D10_BLEND_DESC &internal_desc)
{
	assert(desc.type == api::pipeline_stage::all_graphics || desc.type == api::pipeline_stage::output_merger);
	internal_desc.AlphaToCoverageEnable = desc.graphics.blend_state.alpha_to_coverage_enable;
	internal_desc.SrcBlend = convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[0]);
	internal_desc.DestBlend = convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[0]);
	internal_desc.BlendOp = convert_blend_op(desc.graphics.blend_state.color_blend_op[0]);
	internal_desc.SrcBlendAlpha = convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[0]);
	internal_desc.DestBlendAlpha = convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[0]);
	internal_desc.BlendOpAlpha = convert_blend_op(desc.graphics.blend_state.alpha_blend_op[0]);

	for (UINT i = 0; i < 8; ++i)
	{
		internal_desc.BlendEnable[i] = desc.graphics.blend_state.blend_enable[i];
		internal_desc.RenderTargetWriteMask[i] = desc.graphics.blend_state.render_target_write_mask[i];
	}
}
void reshade::d3d10::convert_pipeline_desc(const api::pipeline_desc &desc, D3D10_BLEND_DESC1 &internal_desc)
{
	assert(desc.type == api::pipeline_stage::all_graphics || desc.type == api::pipeline_stage::output_merger);
	internal_desc.AlphaToCoverageEnable = desc.graphics.blend_state.alpha_to_coverage_enable;
	internal_desc.IndependentBlendEnable = TRUE;

	for (UINT i = 0; i < 8; ++i)
	{
		internal_desc.RenderTarget[i].BlendEnable = desc.graphics.blend_state.blend_enable[i];
		internal_desc.RenderTarget[i].SrcBlend = convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[i]);
		internal_desc.RenderTarget[i].DestBlend = convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[i]);
		internal_desc.RenderTarget[i].BlendOp = convert_blend_op(desc.graphics.blend_state.color_blend_op[i]);
		internal_desc.RenderTarget[i].SrcBlendAlpha = convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[i]);
		internal_desc.RenderTarget[i].DestBlendAlpha = convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[i]);
		internal_desc.RenderTarget[i].BlendOpAlpha = convert_blend_op(desc.graphics.blend_state.alpha_blend_op[i]);
		internal_desc.RenderTarget[i].RenderTargetWriteMask = desc.graphics.blend_state.render_target_write_mask[i];
	}
}
void reshade::d3d10::convert_pipeline_desc(const api::pipeline_desc &desc, D3D10_RASTERIZER_DESC &internal_desc)
{
	assert(desc.type == api::pipeline_stage::all_graphics || desc.type == api::pipeline_stage::rasterizer);
	internal_desc.FillMode = convert_fill_mode(desc.graphics.rasterizer_state.fill_mode);
	internal_desc.CullMode = convert_cull_mode(desc.graphics.rasterizer_state.cull_mode);
	internal_desc.FrontCounterClockwise = desc.graphics.rasterizer_state.front_counter_clockwise;
	internal_desc.DepthBias = static_cast<INT>(desc.graphics.rasterizer_state.depth_bias);
	internal_desc.DepthBiasClamp = desc.graphics.rasterizer_state.depth_bias_clamp;
	internal_desc.SlopeScaledDepthBias = desc.graphics.rasterizer_state.slope_scaled_depth_bias;
	internal_desc.DepthClipEnable = desc.graphics.rasterizer_state.depth_clip_enable;
	internal_desc.ScissorEnable = desc.graphics.rasterizer_state.scissor_enable;
	internal_desc.MultisampleEnable = desc.graphics.rasterizer_state.multisample_enable;
	internal_desc.AntialiasedLineEnable = desc.graphics.rasterizer_state.antialiased_line_enable;
}
void reshade::d3d10::convert_pipeline_desc(const api::pipeline_desc &desc, D3D10_DEPTH_STENCIL_DESC &internal_desc)
{
	assert(desc.type == api::pipeline_stage::all_graphics || desc.type == api::pipeline_stage::depth_stencil);
	internal_desc.DepthEnable = desc.graphics.depth_stencil_state.depth_enable;
	internal_desc.DepthWriteMask = desc.graphics.depth_stencil_state.depth_write_mask ? D3D10_DEPTH_WRITE_MASK_ALL : D3D10_DEPTH_WRITE_MASK_ZERO;
	internal_desc.DepthFunc = convert_compare_op(desc.graphics.depth_stencil_state.depth_func);
	internal_desc.StencilEnable = desc.graphics.depth_stencil_state.stencil_enable;
	internal_desc.StencilReadMask = desc.graphics.depth_stencil_state.stencil_read_mask;
	internal_desc.StencilWriteMask = desc.graphics.depth_stencil_state.stencil_write_mask;
	internal_desc.BackFace.StencilFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_fail_op);
	internal_desc.BackFace.StencilDepthFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_depth_fail_op);
	internal_desc.BackFace.StencilPassOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_pass_op);
	internal_desc.BackFace.StencilFunc = convert_compare_op(desc.graphics.depth_stencil_state.back_stencil_func);
	internal_desc.FrontFace.StencilFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_fail_op);
	internal_desc.FrontFace.StencilDepthFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_depth_fail_op);
	internal_desc.FrontFace.StencilPassOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_pass_op);
	internal_desc.FrontFace.StencilFunc = convert_compare_op(desc.graphics.depth_stencil_state.front_stencil_func);
}
reshade::api::pipeline_desc reshade::d3d10::convert_pipeline_desc(const D3D10_INPUT_ELEMENT_DESC *element_desc, UINT num_elements)
{
	api::pipeline_desc desc = { api::pipeline_stage::input_assembler };

	for (UINT i = 0; i < num_elements && i < 16; ++i)
	{
		desc.graphics.input_layout[i].semantic = element_desc[i].SemanticName;
		desc.graphics.input_layout[i].semantic_index = element_desc[i].SemanticIndex;
		desc.graphics.input_layout[i].format = convert_format(element_desc[i].Format);
		desc.graphics.input_layout[i].buffer_binding = element_desc[i].InputSlot;
		desc.graphics.input_layout[i].offset = element_desc[i].AlignedByteOffset;
		desc.graphics.input_layout[i].instance_step_rate = element_desc[i].InstanceDataStepRate;
	}

	return desc;
}
reshade::api::pipeline_desc reshade::d3d10::convert_pipeline_desc(const D3D10_BLEND_DESC *internal_desc)
{
	api::pipeline_desc desc = { api::pipeline_stage::output_merger };

	if (internal_desc != nullptr)
	{
		desc.graphics.blend_state.alpha_to_coverage_enable = internal_desc->AlphaToCoverageEnable;

		for (UINT i = 0; i < 8; ++i)
		{
			desc.graphics.blend_state.blend_enable[i] = internal_desc->BlendEnable[i];
			desc.graphics.blend_state.src_color_blend_factor[i] = convert_blend_factor(internal_desc->SrcBlend);
			desc.graphics.blend_state.dst_color_blend_factor[i] = convert_blend_factor(internal_desc->DestBlend);
			desc.graphics.blend_state.color_blend_op[i] = convert_blend_op(internal_desc->BlendOp);
			desc.graphics.blend_state.src_alpha_blend_factor[i] = convert_blend_factor(internal_desc->SrcBlendAlpha);
			desc.graphics.blend_state.dst_alpha_blend_factor[i] = convert_blend_factor(internal_desc->DestBlendAlpha);
			desc.graphics.blend_state.alpha_blend_op[i] = convert_blend_op(internal_desc->BlendOpAlpha);
			desc.graphics.blend_state.render_target_write_mask[i] = internal_desc->RenderTargetWriteMask[i];
		}
	}
	else
	{
		// Default blend state (https://docs.microsoft.com/windows/win32/api/d3d10/ns-d3d10-d3d10_blend_desc)
		for (UINT i = 0; i < 8; ++i)
		{
			desc.graphics.blend_state.src_color_blend_factor[i] = api::blend_factor::one;
			desc.graphics.blend_state.dst_color_blend_factor[i] = api::blend_factor::zero;
			desc.graphics.blend_state.color_blend_op[i] = api::blend_op::add;
			desc.graphics.blend_state.src_alpha_blend_factor[i] = api::blend_factor::one;
			desc.graphics.blend_state.dst_alpha_blend_factor[i] = api::blend_factor::zero;
			desc.graphics.blend_state.alpha_blend_op[i] = api::blend_op::add;
			desc.graphics.blend_state.render_target_write_mask[i] = D3D10_COLOR_WRITE_ENABLE_ALL;
		}
	}

	return desc;
}
reshade::api::pipeline_desc reshade::d3d10::convert_pipeline_desc(const D3D10_BLEND_DESC1 *internal_desc)
{
	api::pipeline_desc desc = { api::pipeline_stage::output_merger };

	if (internal_desc != nullptr)
	{
		desc.graphics.blend_state.alpha_to_coverage_enable = internal_desc->AlphaToCoverageEnable;

		for (UINT i = 0; i < 8; ++i)
		{
			const D3D10_RENDER_TARGET_BLEND_DESC1 &target = internal_desc->RenderTarget[internal_desc->IndependentBlendEnable ? i : 0];

			desc.graphics.blend_state.blend_enable[i] = target.BlendEnable;

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

			desc.graphics.blend_state.render_target_write_mask[i] = target.RenderTargetWriteMask;
		}
	}
	else
	{
		// Default blend state (https://docs.microsoft.com/windows/win32/api/d3d10_1/ns-d3d10_1-d3d10_blend_desc1)
		for (UINT i = 0; i < 8; ++i)
		{
			desc.graphics.blend_state.src_color_blend_factor[i] = api::blend_factor::one;
			desc.graphics.blend_state.dst_color_blend_factor[i] = api::blend_factor::zero;
			desc.graphics.blend_state.color_blend_op[i] = api::blend_op::add;
			desc.graphics.blend_state.src_alpha_blend_factor[i] = api::blend_factor::one;
			desc.graphics.blend_state.dst_alpha_blend_factor[i] = api::blend_factor::zero;
			desc.graphics.blend_state.alpha_blend_op[i] = api::blend_op::add;
			desc.graphics.blend_state.render_target_write_mask[i] = D3D10_COLOR_WRITE_ENABLE_ALL;
		}
	}

	return desc;
}
reshade::api::pipeline_desc reshade::d3d10::convert_pipeline_desc(const D3D10_RASTERIZER_DESC *internal_desc)
{
	api::pipeline_desc desc = { api::pipeline_stage::rasterizer };

	if (internal_desc != nullptr)
	{
		desc.graphics.rasterizer_state.fill_mode = convert_fill_mode(internal_desc->FillMode);
		desc.graphics.rasterizer_state.cull_mode = convert_cull_mode(internal_desc->CullMode);
		desc.graphics.rasterizer_state.front_counter_clockwise = internal_desc->FrontCounterClockwise;
		desc.graphics.rasterizer_state.depth_bias = static_cast<float>(internal_desc->DepthBias);
		desc.graphics.rasterizer_state.depth_bias_clamp = internal_desc->DepthBiasClamp;
		desc.graphics.rasterizer_state.slope_scaled_depth_bias = internal_desc->SlopeScaledDepthBias;
		desc.graphics.rasterizer_state.depth_clip_enable = internal_desc->DepthClipEnable;
		desc.graphics.rasterizer_state.scissor_enable = internal_desc->ScissorEnable;
		desc.graphics.rasterizer_state.multisample_enable = internal_desc->MultisampleEnable;
		desc.graphics.rasterizer_state.antialiased_line_enable = internal_desc->AntialiasedLineEnable;
	}
	else
	{
		// Default rasterizer state (https://docs.microsoft.com/windows/win32/api/d3d10/ns-d3d10-d3d10_rasterizer_desc)
		desc.graphics.rasterizer_state.fill_mode = api::fill_mode::solid;
		desc.graphics.rasterizer_state.cull_mode = api::cull_mode::back;
		desc.graphics.rasterizer_state.depth_clip_enable = true;
	}

	return desc;
}
reshade::api::pipeline_desc reshade::d3d10::convert_pipeline_desc(const D3D10_DEPTH_STENCIL_DESC *internal_desc)
{
	api::pipeline_desc desc = { api::pipeline_stage::depth_stencil };

	if (internal_desc != nullptr)
	{
		desc.graphics.depth_stencil_state.depth_enable = internal_desc->DepthEnable;
		desc.graphics.depth_stencil_state.depth_write_mask = internal_desc->DepthWriteMask != D3D10_DEPTH_WRITE_MASK_ZERO;
		desc.graphics.depth_stencil_state.depth_func = convert_compare_op(internal_desc->DepthFunc);
		desc.graphics.depth_stencil_state.stencil_enable = internal_desc->StencilEnable;
		desc.graphics.depth_stencil_state.stencil_read_mask = internal_desc->StencilReadMask;
		desc.graphics.depth_stencil_state.stencil_write_mask = internal_desc->StencilWriteMask;
		desc.graphics.depth_stencil_state.back_stencil_fail_op = convert_stencil_op(internal_desc->BackFace.StencilFailOp);
		desc.graphics.depth_stencil_state.back_stencil_depth_fail_op = convert_stencil_op(internal_desc->BackFace.StencilDepthFailOp);
		desc.graphics.depth_stencil_state.back_stencil_pass_op = convert_stencil_op(internal_desc->BackFace.StencilPassOp);
		desc.graphics.depth_stencil_state.back_stencil_func = convert_compare_op(internal_desc->BackFace.StencilFunc);
		desc.graphics.depth_stencil_state.front_stencil_fail_op = convert_stencil_op(internal_desc->FrontFace.StencilFailOp);
		desc.graphics.depth_stencil_state.front_stencil_depth_fail_op = convert_stencil_op(internal_desc->FrontFace.StencilDepthFailOp);
		desc.graphics.depth_stencil_state.front_stencil_pass_op = convert_stencil_op(internal_desc->FrontFace.StencilPassOp);
		desc.graphics.depth_stencil_state.front_stencil_func = convert_compare_op(internal_desc->FrontFace.StencilFunc);
	}
	else
	{
		// Default depth-stencil state (https://docs.microsoft.com/windows/win32/api/d3d10/ns-d3d10-d3d10_depth_stencil_desc)
		desc.graphics.depth_stencil_state.depth_enable = true;
		desc.graphics.depth_stencil_state.depth_write_mask = true;
		desc.graphics.depth_stencil_state.depth_func = api::compare_op::less;
		desc.graphics.depth_stencil_state.stencil_read_mask = D3D10_DEFAULT_STENCIL_READ_MASK;
		desc.graphics.depth_stencil_state.stencil_write_mask = D3D10_DEFAULT_STENCIL_WRITE_MASK;
		desc.graphics.depth_stencil_state.back_stencil_fail_op = api::stencil_op::keep;
		desc.graphics.depth_stencil_state.back_stencil_depth_fail_op = api::stencil_op::keep;
		desc.graphics.depth_stencil_state.back_stencil_pass_op = api::stencil_op::keep;
		desc.graphics.depth_stencil_state.back_stencil_func = api::compare_op::always;
		desc.graphics.depth_stencil_state.front_stencil_fail_op = api::stencil_op::keep;
		desc.graphics.depth_stencil_state.front_stencil_depth_fail_op = api::stencil_op::keep;
		desc.graphics.depth_stencil_state.front_stencil_pass_op = api::stencil_op::keep;
		desc.graphics.depth_stencil_state.front_stencil_func = api::compare_op::always;
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
		return api::blend_factor::src_color;
	case D3D10_BLEND_INV_SRC_COLOR:
		return api::blend_factor::inv_src_color;
	case D3D10_BLEND_DEST_COLOR:
		return api::blend_factor::dst_color;
	case D3D10_BLEND_INV_DEST_COLOR:
		return api::blend_factor::inv_dst_color;
	case D3D10_BLEND_SRC_ALPHA:
		return api::blend_factor::src_alpha;
	case D3D10_BLEND_INV_SRC_ALPHA:
		return api::blend_factor::inv_src_alpha;
	case D3D10_BLEND_DEST_ALPHA:
		return api::blend_factor::dst_alpha;
	case D3D10_BLEND_INV_DEST_ALPHA:
		return api::blend_factor::inv_dst_alpha;
	case D3D10_BLEND_BLEND_FACTOR:
		return api::blend_factor::constant_color;
	case D3D10_BLEND_INV_BLEND_FACTOR:
		return api::blend_factor::inv_constant_color;
	case D3D10_BLEND_SRC_ALPHA_SAT:
		return api::blend_factor::src_alpha_sat;
	case D3D10_BLEND_SRC1_COLOR:
		return api::blend_factor::src1_color;
	case D3D10_BLEND_INV_SRC1_COLOR:
		return api::blend_factor::inv_src1_color;
	case D3D10_BLEND_SRC1_ALPHA:
		return api::blend_factor::src1_alpha;
	case D3D10_BLEND_INV_SRC1_ALPHA:
		return api::blend_factor::inv_src1_alpha;
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
