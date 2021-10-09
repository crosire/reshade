/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include <vector>
#include <limits>
#include "com_ptr.hpp"
#include "reshade_api_pipeline.hpp"
#include "d3d11_impl_type_convert.hpp"

auto reshade::d3d11::convert_format(api::format format) -> DXGI_FORMAT
{
	if (static_cast<uint32_t>(format) >= 1000)
		return DXGI_FORMAT_UNKNOWN;

	return static_cast<DXGI_FORMAT>(format);
}
auto reshade::d3d11::convert_format(DXGI_FORMAT format) -> api::format
{
	return static_cast<api::format>(format);
}

static void convert_memory_heap_to_d3d_usage(reshade::api::memory_heap heap, D3D11_USAGE &usage, UINT &cpu_access_flags)
{
	using namespace reshade;

	switch (heap)
	{
	case api::memory_heap::gpu_only:
		if (usage == D3D11_USAGE_IMMUTABLE)
			break;
		usage = D3D11_USAGE_DEFAULT;
		break;
	case api::memory_heap::cpu_to_gpu:
		usage = D3D11_USAGE_DYNAMIC;
		cpu_access_flags |= D3D11_CPU_ACCESS_WRITE;
		break;
	case api::memory_heap::gpu_to_cpu:
		usage = D3D11_USAGE_STAGING;
		cpu_access_flags |= D3D11_CPU_ACCESS_READ;
		break;
	case api::memory_heap::cpu_only:
		usage = D3D11_USAGE_STAGING;
		if (cpu_access_flags == 0)
			cpu_access_flags |= D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		break;
	}
}
static void convert_d3d_usage_to_memory_heap(D3D11_USAGE usage, UINT cpu_access_flags, reshade::api::memory_heap &heap)
{
	using namespace reshade;

	switch (usage)
	{
	case D3D11_USAGE_DEFAULT:
	case D3D11_USAGE_IMMUTABLE:
		assert(cpu_access_flags == 0);
		heap = api::memory_heap::gpu_only;
		break;
	case D3D11_USAGE_DYNAMIC:
		assert(cpu_access_flags == D3D11_CPU_ACCESS_WRITE);
		heap = api::memory_heap::cpu_to_gpu;
		break;
	case D3D11_USAGE_STAGING:
		heap = cpu_access_flags == D3D11_CPU_ACCESS_READ ? api::memory_heap::gpu_to_cpu : api::memory_heap::cpu_only;
		break;
	}
}

static void convert_resource_usage_to_bind_flags(reshade::api::resource_usage usage, UINT &bind_flags)
{
	using namespace reshade;

	if ((usage & api::resource_usage::depth_stencil) != api::resource_usage::undefined)
		bind_flags |= D3D11_BIND_DEPTH_STENCIL;
	else
		bind_flags &= ~D3D11_BIND_DEPTH_STENCIL;

	if ((usage & api::resource_usage::render_target) != api::resource_usage::undefined)
		bind_flags |= D3D11_BIND_RENDER_TARGET;
	else
		bind_flags &= ~D3D11_BIND_RENDER_TARGET;

	if ((usage & api::resource_usage::shader_resource) != api::resource_usage::undefined)
		bind_flags |= D3D11_BIND_SHADER_RESOURCE;
	else
		bind_flags &= ~D3D11_BIND_SHADER_RESOURCE;

	if ((usage & api::resource_usage::unordered_access) != api::resource_usage::undefined)
		bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
	else
		bind_flags &= ~D3D11_BIND_UNORDERED_ACCESS;

	if ((usage & api::resource_usage::index_buffer) != api::resource_usage::undefined)
		bind_flags |= D3D11_BIND_INDEX_BUFFER;
	else
		bind_flags &= ~D3D11_BIND_INDEX_BUFFER;

	if ((usage & api::resource_usage::vertex_buffer) != api::resource_usage::undefined)
		bind_flags |= D3D11_BIND_VERTEX_BUFFER;
	else
		bind_flags &= ~D3D11_BIND_VERTEX_BUFFER;

	if ((usage & api::resource_usage::constant_buffer) != api::resource_usage::undefined)
		bind_flags |= D3D11_BIND_CONSTANT_BUFFER;
	else
		bind_flags &= ~D3D11_BIND_CONSTANT_BUFFER;
}
static void convert_bind_flags_to_resource_usage(UINT bind_flags, reshade::api::resource_usage &usage)
{
	using namespace reshade;

	// Resources are generally copyable in D3D11
	usage |= api::resource_usage::copy_dest | api::resource_usage::copy_source;

	if ((bind_flags & D3D11_BIND_DEPTH_STENCIL) != 0)
		usage |= api::resource_usage::depth_stencil;
	if ((bind_flags & D3D11_BIND_RENDER_TARGET) != 0)
		usage |= api::resource_usage::render_target;
	if ((bind_flags & D3D11_BIND_SHADER_RESOURCE) != 0)
		usage |= api::resource_usage::shader_resource;
	if ((bind_flags & D3D11_BIND_UNORDERED_ACCESS) != 0)
		usage |= api::resource_usage::unordered_access;

	if ((bind_flags & D3D11_BIND_INDEX_BUFFER) != 0)
		usage |= api::resource_usage::index_buffer;
	if ((bind_flags & D3D11_BIND_VERTEX_BUFFER) != 0)
		usage |= api::resource_usage::vertex_buffer;
	if ((bind_flags & D3D11_BIND_CONSTANT_BUFFER) != 0)
		usage |= api::resource_usage::constant_buffer;
}

static void convert_resource_flags_to_misc_flags(reshade::api::resource_flags flags, UINT &misc_flags)
{
	using namespace reshade;

	if ((flags & api::resource_flags::shared) == api::resource_flags::shared && (misc_flags & (D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE)) == 0)
		misc_flags |= D3D11_RESOURCE_MISC_SHARED;

	if ((flags & api::resource_flags::cube_compatible) == api::resource_flags::cube_compatible)
		misc_flags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
	else
		misc_flags &= ~D3D11_RESOURCE_MISC_TEXTURECUBE;

	if ((flags & api::resource_flags::generate_mipmaps) == api::resource_flags::generate_mipmaps)
		misc_flags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	else
		misc_flags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;

	if ((flags & api::resource_flags::sparse_binding) == api::resource_flags::sparse_binding)
		misc_flags |= D3D11_RESOURCE_MISC_TILED;
	else
		misc_flags &= ~D3D11_RESOURCE_MISC_TILED;
}
static void convert_misc_flags_to_resource_flags(UINT misc_flags, reshade::api::resource_flags &flags)
{
	using namespace reshade;

	if ((misc_flags & (D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE)) != 0)
		flags |= api::resource_flags::shared;
	if ((misc_flags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0)
		flags |= api::resource_flags::cube_compatible;
	if ((misc_flags & D3D11_RESOURCE_MISC_GENERATE_MIPS) != 0)
		flags |= api::resource_flags::generate_mipmaps;
	if ((misc_flags & D3D11_RESOURCE_MISC_TILED) != 0)
		flags |= api::resource_flags::sparse_binding;
}

auto reshade::d3d11::convert_access_flags(api::map_access access) -> D3D11_MAP
{
	switch (access)
	{
	case api::map_access::read_only:
		return D3D11_MAP_READ;
	case api::map_access::write_only:
		// Use no overwrite flag to simulate D3D12 behavior of there only being one allocation that backs a buffer (instead of the runtime managing multiple ones behind the scenes)
		return D3D11_MAP_WRITE_NO_OVERWRITE;
	case api::map_access::read_write:
		return D3D11_MAP_READ_WRITE;
	case api::map_access::write_discard:
		return D3D11_MAP_WRITE_DISCARD;
	}
	return static_cast<D3D11_MAP>(0);
}
reshade::api::map_access reshade::d3d11::convert_access_flags(D3D11_MAP map_type)
{
	switch (map_type)
	{
	case D3D11_MAP_READ:
		return api::map_access::read_only;
	case D3D11_MAP_WRITE:
	case D3D11_MAP_WRITE_NO_OVERWRITE:
		return api::map_access::write_only;
	case D3D11_MAP_READ_WRITE:
		return api::map_access::read_write;
	case D3D11_MAP_WRITE_DISCARD:
		return api::map_access::write_discard;
	}
	return static_cast<api::map_access>(0);
}

void reshade::d3d11::convert_sampler_desc(const api::sampler_desc &desc, D3D11_SAMPLER_DESC &internal_desc)
{
	internal_desc.Filter = static_cast<D3D11_FILTER>(desc.filter);
	internal_desc.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.address_u);
	internal_desc.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.address_v);
	internal_desc.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.address_w);
	internal_desc.MipLODBias = desc.mip_lod_bias;
	internal_desc.MaxAnisotropy = static_cast<UINT>(desc.max_anisotropy);
	internal_desc.ComparisonFunc = convert_compare_op(desc.compare_op);
	std::copy_n(desc.border_color, 4, internal_desc.BorderColor);
	internal_desc.MinLOD = desc.min_lod;
	internal_desc.MaxLOD = desc.max_lod;
}
reshade::api::sampler_desc reshade::d3d11::convert_sampler_desc(const D3D11_SAMPLER_DESC &internal_desc)
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

void reshade::d3d11::convert_resource_desc(const api::resource_desc &desc, D3D11_BUFFER_DESC &internal_desc)
{
	assert(desc.type == api::resource_type::buffer);
	assert(desc.buffer.size <= std::numeric_limits<UINT>::max());
	internal_desc.ByteWidth = static_cast<UINT>(desc.buffer.size);
	convert_memory_heap_to_d3d_usage(desc.heap, internal_desc.Usage, internal_desc.CPUAccessFlags);
	convert_resource_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
	convert_resource_flags_to_misc_flags(desc.flags, internal_desc.MiscFlags);
	internal_desc.StructureByteStride = desc.buffer.struct_stride;
}
void reshade::d3d11::convert_resource_desc(const api::resource_desc &desc, D3D11_TEXTURE1D_DESC &internal_desc)
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

	// The 'D3D11_RESOURCE_MISC_GENERATE_MIPS' flag requires render target and shader resource bind flags
	if ((desc.flags & api::resource_flags::generate_mipmaps) == api::resource_flags::generate_mipmaps)
		internal_desc.BindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
}
void reshade::d3d11::convert_resource_desc(const api::resource_desc &desc, D3D11_TEXTURE2D_DESC &internal_desc)
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
		internal_desc.BindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
}
void reshade::d3d11::convert_resource_desc(const api::resource_desc &desc, D3D11_TEXTURE2D_DESC1 &internal_desc)
{
	convert_resource_desc(desc, reinterpret_cast<D3D11_TEXTURE2D_DESC &>(internal_desc));
}
void reshade::d3d11::convert_resource_desc(const api::resource_desc &desc, D3D11_TEXTURE3D_DESC &internal_desc)
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
		internal_desc.BindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
}
void reshade::d3d11::convert_resource_desc(const api::resource_desc &desc, D3D11_TEXTURE3D_DESC1 &internal_desc)
{
	convert_resource_desc(desc, reinterpret_cast<D3D11_TEXTURE3D_DESC &>(internal_desc));
}
reshade::api::resource_desc reshade::d3d11::convert_resource_desc(const D3D11_BUFFER_DESC &internal_desc)
{
	api::resource_desc desc = {};
	desc.type = api::resource_type::buffer;
	desc.buffer.size = internal_desc.ByteWidth;
	desc.buffer.struct_stride = internal_desc.StructureByteStride;
	convert_d3d_usage_to_memory_heap(internal_desc.Usage, internal_desc.CPUAccessFlags, desc.heap);
	convert_bind_flags_to_resource_usage(internal_desc.BindFlags, desc.usage);
	convert_misc_flags_to_resource_flags(internal_desc.MiscFlags, desc.flags);

	if (internal_desc.Usage == D3D11_USAGE_DYNAMIC)
		desc.flags |= api::resource_flags::dynamic;

	return desc;
}
reshade::api::resource_desc reshade::d3d11::convert_resource_desc(const D3D11_TEXTURE1D_DESC &internal_desc)
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

	if (internal_desc.Usage == D3D11_USAGE_DYNAMIC)
		desc.flags |= api::resource_flags::dynamic;

	return desc;
}
reshade::api::resource_desc reshade::d3d11::convert_resource_desc(const D3D11_TEXTURE2D_DESC &internal_desc)
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

	if (internal_desc.Usage == D3D11_USAGE_DYNAMIC)
		desc.flags |= api::resource_flags::dynamic;

	return desc;
}
reshade::api::resource_desc reshade::d3d11::convert_resource_desc(const D3D11_TEXTURE2D_DESC1 &internal_desc)
{
	// D3D11_TEXTURE2D_DESC1 is a superset of D3D11_TEXTURE2D_DESC
	return convert_resource_desc(reinterpret_cast<const D3D11_TEXTURE2D_DESC &>(internal_desc));
}
reshade::api::resource_desc reshade::d3d11::convert_resource_desc(const D3D11_TEXTURE3D_DESC &internal_desc)
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

	if (internal_desc.Usage == D3D11_USAGE_DYNAMIC)
		desc.flags |= api::resource_flags::dynamic;

	return desc;
}
reshade::api::resource_desc reshade::d3d11::convert_resource_desc(const D3D11_TEXTURE3D_DESC1 &internal_desc)
{
	// D3D11_TEXTURE3D_DESC1 is a superset of D3D11_TEXTURE3D_DESC
	return convert_resource_desc(reinterpret_cast<const D3D11_TEXTURE3D_DESC &>(internal_desc));
}

void reshade::d3d11::convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D11_DEPTH_STENCIL_VIEW_DESC::Flags
	internal_desc.Format = convert_format(desc.format);
	assert(desc.type != api::resource_view_type::buffer && desc.texture.level_count == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case api::resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layer_count;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	internal_desc.Format = convert_format(desc.format);
	assert(desc.type != api::resource_view_type::buffer && desc.texture.level_count == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case api::resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.texture.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.texture.first_layer;
		internal_desc.Texture3D.WSize = desc.texture.layer_count;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_RENDER_TARGET_VIEW_DESC1 &internal_desc)
{
	if (desc.type == api::resource_view_type::texture_2d || desc.type == api::resource_view_type::texture_2d_array)
	{
		internal_desc.Format = convert_format(desc.format);
		assert(desc.type != api::resource_view_type::buffer && desc.texture.level_count == 1);
		switch (desc.type)
		{
		case api::resource_view_type::texture_2d:
			internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			internal_desc.Texture2D.MipSlice = desc.texture.first_level;
			// Missing fields: D3D11_TEX2D_RTV1::PlaneSlice
			break;
		case api::resource_view_type::texture_2d_array:
			internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
			internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
			internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
			break;
			// Missing fields: D3D11_TEX2D_ARRAY_RTV1::PlaneSlice
		}
	}
	else
	{
		convert_resource_view_desc(desc, reinterpret_cast<D3D11_RENDER_TARGET_VIEW_DESC &>(internal_desc));
	}
}
void reshade::d3d11::convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	internal_desc.Format = convert_format(desc.format);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case api::resource_view_type::buffer:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		assert(desc.buffer.offset <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.FirstElement = static_cast<UINT>(desc.buffer.offset);
		assert(desc.buffer.size <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.NumElements = static_cast<UINT>(desc.buffer.size);
		break;
	case api::resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture1D.MipLevels = desc.texture.level_count;
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture1DArray.MipLevels = desc.texture.level_count;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture2D.MipLevels = desc.texture.level_count;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture2DArray.MipLevels = desc.texture.level_count;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MostDetailedMip = desc.texture.first_level;
		internal_desc.Texture3D.MipLevels = desc.texture.level_count;
		break;
	case api::resource_view_type::texture_cube:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		internal_desc.TextureCube.MostDetailedMip = desc.texture.first_level;
		internal_desc.TextureCube.MipLevels = desc.texture.level_count;
		break;
	case api::resource_view_type::texture_cube_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
		internal_desc.TextureCubeArray.MostDetailedMip = desc.texture.first_level;
		internal_desc.TextureCubeArray.MipLevels = desc.texture.level_count;
		internal_desc.TextureCubeArray.First2DArrayFace = desc.texture.first_layer;
		if (desc.texture.layer_count == 0xFFFFFFFF)
			internal_desc.TextureCubeArray.NumCubes = 0xFFFFFFFF;
		else
			internal_desc.TextureCubeArray.NumCubes = desc.texture.layer_count / 6;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_SHADER_RESOURCE_VIEW_DESC1 &internal_desc)
{
	if (desc.type == api::resource_view_type::texture_2d || desc.type == api::resource_view_type::texture_2d_array)
	{
		internal_desc.Format = convert_format(desc.format);
		switch (desc.type)
		{
		case api::resource_view_type::texture_2d:
			internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			internal_desc.Texture2D.MostDetailedMip = desc.texture.first_level;
			internal_desc.Texture2D.MipLevels = desc.texture.level_count;
			// Missing fields: D3D11_TEX2D_SRV1::PlaneSlice
			break;
		case api::resource_view_type::texture_2d_array:
			internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			internal_desc.Texture2DArray.MostDetailedMip = desc.texture.first_level;
			internal_desc.Texture2DArray.MipLevels = desc.texture.level_count;
			internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
			internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
			break;
			// Missing fields: D3D11_TEX2D_ARRAY_SRV1::PlaneSlice
		}
	}
	else
	{
		convert_resource_view_desc(desc, reinterpret_cast<D3D11_SHADER_RESOURCE_VIEW_DESC &>(internal_desc));
	}
}
void reshade::d3d11::convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_UNORDERED_ACCESS_VIEW_DESC &internal_desc)
{
	internal_desc.Format = convert_format(desc.format);
	assert(desc.type == api::resource_view_type::buffer || desc.texture.level_count == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case api::resource_view_type::buffer:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		assert(desc.buffer.offset <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.FirstElement = static_cast<UINT>(desc.buffer.offset);
		assert(desc.buffer.size <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.NumElements = static_cast<UINT>(desc.buffer.size);
		break;
	case api::resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.texture.first_level;
		break;
	case api::resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
		break;
	case api::resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.texture.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.texture.first_layer;
		internal_desc.Texture3D.WSize = desc.texture.layer_count;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_UNORDERED_ACCESS_VIEW_DESC1 &internal_desc)
{
	if (desc.type == api::resource_view_type::texture_2d || desc.type == api::resource_view_type::texture_2d_array)
	{
		internal_desc.Format = convert_format(desc.format);
		assert(desc.type == api::resource_view_type::buffer || desc.texture.level_count == 1);
		switch (desc.type)
		{
		case api::resource_view_type::texture_2d:
			internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			internal_desc.Texture2D.MipSlice = desc.texture.first_level;
			// Missing fields: D3D11_TEX2D_UAV1::PlaneSlice
			break;
		case api::resource_view_type::texture_2d_array:
			internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
			internal_desc.Texture2DArray.MipSlice = desc.texture.first_level;
			internal_desc.Texture2DArray.FirstArraySlice = desc.texture.first_layer;
			internal_desc.Texture2DArray.ArraySize = desc.texture.layer_count;
			// Missing fields: D3D11_TEX2D_ARRAY_UAV1::PlaneSlice
			break;
		}
	}
	else
	{
		convert_resource_view_desc(desc, reinterpret_cast<D3D11_UNORDERED_ACCESS_VIEW_DESC &>(internal_desc));
	}
}
reshade::api::resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D11_DEPTH_STENCIL_VIEW_DESC::Flags
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	desc.texture.level_count = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D11_DSV_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DMSArray.ArraySize;
		break;
	}
	return desc;
}
reshade::api::resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	desc.texture.level_count = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D11_RTV_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE3D:
		desc.type = api::resource_view_type::texture_3d;
		desc.texture.first_level = internal_desc.Texture3D.MipSlice;
		desc.texture.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.texture.layer_count = internal_desc.Texture3D.WSize;
		break;
	}
	return desc;
}
reshade::api::resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_RENDER_TARGET_VIEW_DESC1 &internal_desc)
{
	if (internal_desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D || internal_desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
	{
		api::resource_view_desc desc = {};
		desc.format = convert_format(internal_desc.Format);
		desc.texture.level_count = 1;
		switch (internal_desc.ViewDimension)
		{
		case D3D11_RTV_DIMENSION_TEXTURE2D:
			desc.type = api::resource_view_type::texture_2d;
			desc.texture.first_level = internal_desc.Texture2D.MipSlice;
			// Missing fields: D3D11_TEX2D_RTV1::PlaneSlice
			break;
		case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
			desc.type = api::resource_view_type::texture_2d_array;
			desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
			desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
			desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
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
reshade::api::resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	switch (internal_desc.ViewDimension)
	{
	case D3D11_SRV_DIMENSION_BUFFER:
		desc.type = api::resource_view_type::buffer;
		desc.buffer.offset = internal_desc.Buffer.FirstElement;
		desc.buffer.size = internal_desc.Buffer.NumElements;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = internal_desc.Texture1D.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture1D.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture1DArray.MipLevels;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture2D.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture2DArray.MipLevels;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2DMS:
		desc.type = api::resource_view_type::texture_2d_multisample;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = api::resource_view_type::texture_2d_multisample_array;
		desc.texture.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE3D:
		desc.type = api::resource_view_type::texture_3d;
		desc.texture.first_level = internal_desc.Texture3D.MostDetailedMip;
		desc.texture.level_count = internal_desc.Texture3D.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURECUBE:
		desc.type = api::resource_view_type::texture_cube;
		desc.texture.first_level = internal_desc.TextureCube.MostDetailedMip;
		desc.texture.level_count = internal_desc.TextureCube.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
		desc.type = api::resource_view_type::texture_cube_array;
		desc.texture.first_level = internal_desc.TextureCubeArray.MostDetailedMip;
		desc.texture.level_count = internal_desc.TextureCubeArray.MipLevels;
		desc.texture.first_layer = internal_desc.TextureCubeArray.First2DArrayFace;
		if (internal_desc.TextureCubeArray.NumCubes == 0xFFFFFFFF)
			desc.texture.layer_count = 0xFFFFFFFF;
		else
			desc.texture.layer_count = internal_desc.TextureCubeArray.NumCubes * 6;
		break;
	case D3D11_SRV_DIMENSION_BUFFEREX:
		// Do not set type to 'resource_view_type::buffer', since that would translate to D3D11_SRV_DIMENSION_BUFFER on the conversion back
		desc.buffer.offset = internal_desc.BufferEx.FirstElement;
		desc.buffer.size = internal_desc.BufferEx.NumElements;
		// Missing fields: D3D11_BUFFEREX_SRV::Flags
		break;
	}
	return desc;
}
reshade::api::resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_SHADER_RESOURCE_VIEW_DESC1 &internal_desc)
{
	if (internal_desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D || internal_desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
	{
		api::resource_view_desc desc = {};
		desc.format = convert_format(internal_desc.Format);
		switch (internal_desc.ViewDimension)
		{
		case D3D11_SRV_DIMENSION_TEXTURE2D:
			desc.type = api::resource_view_type::texture_2d;
			desc.texture.first_level = internal_desc.Texture2D.MostDetailedMip;
			desc.texture.level_count = internal_desc.Texture2D.MipLevels;
			// Missing fields: D3D11_TEX2D_SRV1::PlaneSlice
			break;
		case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
			desc.type = api::resource_view_type::texture_2d_array;
			desc.texture.first_level = internal_desc.Texture2DArray.MostDetailedMip;
			desc.texture.level_count = internal_desc.Texture2DArray.MipLevels;
			desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
			desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
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
reshade::api::resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_UNORDERED_ACCESS_VIEW_DESC &internal_desc)
{
	api::resource_view_desc desc = {};
	desc.format = convert_format(internal_desc.Format);
	desc.texture.level_count = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D11_UAV_DIMENSION_BUFFER:
		desc.type = api::resource_view_type::buffer;
		desc.buffer.offset = internal_desc.Buffer.FirstElement;
		desc.buffer.size = internal_desc.Buffer.NumElements;
		// Missing fields: D3D11_BUFFER_UAV::Flags
		break;
	case D3D11_UAV_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		desc.texture.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE3D:
		desc.type = api::resource_view_type::texture_3d;
		desc.texture.first_level = internal_desc.Texture3D.MipSlice;
		desc.texture.first_layer = internal_desc.Texture3D.FirstWSlice;
		desc.texture.layer_count = internal_desc.Texture3D.WSize;
		break;
	}
	return desc;
}
reshade::api::resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 &internal_desc)
{
	if (internal_desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D || internal_desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
	{
		api::resource_view_desc desc = {};
		desc.format = convert_format(internal_desc.Format);
		desc.texture.level_count = 1;
		switch (internal_desc.ViewDimension)
		{
		case D3D11_UAV_DIMENSION_TEXTURE2D:
			desc.type = api::resource_view_type::texture_2d;
			desc.texture.first_level = internal_desc.Texture2D.MipSlice;
			// Missing fields: D3D11_TEX2D_UAV1::PlaneSlice
			break;
		case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
			desc.type = api::resource_view_type::texture_2d_array;
			desc.texture.first_level = internal_desc.Texture2DArray.MipSlice;
			desc.texture.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
			desc.texture.layer_count = internal_desc.Texture2DArray.ArraySize;
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

void reshade::d3d11::convert_pipeline_desc(const api::pipeline_desc &desc, std::vector<D3D11_INPUT_ELEMENT_DESC> &internal_elements)
{
	assert(desc.type == api::pipeline_stage::all_graphics || desc.type == api::pipeline_stage::input_assembler);
	internal_elements.reserve(16);

	for (UINT i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
	{
		const api::input_layout_element &element = desc.graphics.input_layout[i];

		D3D11_INPUT_ELEMENT_DESC &internal_element = internal_elements.emplace_back();
		internal_element.SemanticName = element.semantic;
		internal_element.SemanticIndex = element.semantic_index;
		internal_element.Format = convert_format(element.format);
		internal_element.InputSlot = element.buffer_binding;
		internal_element.AlignedByteOffset = element.offset;
		internal_element.InputSlotClass = element.instance_step_rate > 0 ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
		internal_element.InstanceDataStepRate = element.instance_step_rate;
	}
}
void reshade::d3d11::convert_pipeline_desc(const api::pipeline_desc &desc, D3D11_BLEND_DESC &internal_desc)
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
void reshade::d3d11::convert_pipeline_desc(const api::pipeline_desc &desc, D3D11_BLEND_DESC1 &internal_desc)
{
	assert(desc.type == api::pipeline_stage::all_graphics || desc.type == api::pipeline_stage::output_merger);
	internal_desc.AlphaToCoverageEnable = desc.graphics.blend_state.alpha_to_coverage_enable;
	internal_desc.IndependentBlendEnable = TRUE;

	for (UINT i = 0; i < 8; ++i)
	{
		internal_desc.RenderTarget[i].BlendEnable = desc.graphics.blend_state.blend_enable[i];
		internal_desc.RenderTarget[i].LogicOpEnable = desc.graphics.blend_state.logic_op_enable[i];
		internal_desc.RenderTarget[i].SrcBlend = convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[i]);
		internal_desc.RenderTarget[i].DestBlend = convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[i]);
		internal_desc.RenderTarget[i].BlendOp = convert_blend_op(desc.graphics.blend_state.color_blend_op[i]);
		internal_desc.RenderTarget[i].SrcBlendAlpha = convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[i]);
		internal_desc.RenderTarget[i].DestBlendAlpha = convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[i]);
		internal_desc.RenderTarget[i].BlendOpAlpha = convert_blend_op(desc.graphics.blend_state.alpha_blend_op[i]);
		internal_desc.RenderTarget[i].LogicOp = convert_logic_op(desc.graphics.blend_state.logic_op[i]);
		internal_desc.RenderTarget[i].RenderTargetWriteMask = desc.graphics.blend_state.render_target_write_mask[i];
	}
}
void reshade::d3d11::convert_pipeline_desc(const api::pipeline_desc &desc, D3D11_RASTERIZER_DESC &internal_desc)
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
void reshade::d3d11::convert_pipeline_desc(const api::pipeline_desc &desc, D3D11_RASTERIZER_DESC1 &internal_desc)
{
	// D3D11_RASTERIZER_DESC1 is a superset of D3D11_RASTERIZER_DESC
	// Missing fields: ForcedSampleCount
	convert_pipeline_desc(desc, reinterpret_cast<D3D11_RASTERIZER_DESC &>(internal_desc));
}
void reshade::d3d11::convert_pipeline_desc(const api::pipeline_desc &desc, D3D11_RASTERIZER_DESC2 &internal_desc)
{
	// D3D11_RASTERIZER_DESC2 is a superset of D3D11_RASTERIZER_DESC1
	// Missing fields: ForcedSampleCount, ConservativeRaster
	convert_pipeline_desc(desc, reinterpret_cast<D3D11_RASTERIZER_DESC &>(internal_desc));
}
void reshade::d3d11::convert_pipeline_desc(const api::pipeline_desc &desc, D3D11_DEPTH_STENCIL_DESC &internal_desc)
{
	assert(desc.type == api::pipeline_stage::all_graphics || desc.type == api::pipeline_stage::depth_stencil);
	internal_desc.DepthEnable = desc.graphics.depth_stencil_state.depth_enable;
	internal_desc.DepthWriteMask = desc.graphics.depth_stencil_state.depth_write_mask ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
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
reshade::api::pipeline_desc reshade::d3d11::convert_pipeline_desc(const D3D11_INPUT_ELEMENT_DESC *element_desc, UINT num_elements)
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
reshade::api::pipeline_desc reshade::d3d11::convert_pipeline_desc(const D3D11_BLEND_DESC *internal_desc)
{
	api::pipeline_desc desc = { api::pipeline_stage::output_merger };

	if (internal_desc != nullptr)
	{
		desc.graphics.blend_state.alpha_to_coverage_enable = internal_desc->AlphaToCoverageEnable;

		for (UINT i = 0; i < 8; ++i)
		{
			const D3D11_RENDER_TARGET_BLEND_DESC &target = internal_desc->RenderTarget[internal_desc->IndependentBlendEnable ? i : 0];

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
		// Default blend state (https://docs.microsoft.com/windows/win32/api/d3d11/ns-d3d11-d3d11_blend_desc)
		for (UINT i = 0; i < 8; ++i)
		{
			desc.graphics.blend_state.src_color_blend_factor[i] = api::blend_factor::one;
			desc.graphics.blend_state.dst_color_blend_factor[i] = api::blend_factor::zero;
			desc.graphics.blend_state.color_blend_op[i] = api::blend_op::add;
			desc.graphics.blend_state.src_alpha_blend_factor[i] = api::blend_factor::one;
			desc.graphics.blend_state.dst_alpha_blend_factor[i] = api::blend_factor::zero;
			desc.graphics.blend_state.alpha_blend_op[i] = api::blend_op::add;
			desc.graphics.blend_state.render_target_write_mask[i] = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
	}

	return desc;
}
reshade::api::pipeline_desc reshade::d3d11::convert_pipeline_desc(const D3D11_BLEND_DESC1 *internal_desc)
{
	api::pipeline_desc desc = { api::pipeline_stage::output_merger };

	if (internal_desc != nullptr)
	{
		desc.graphics.blend_state.alpha_to_coverage_enable = internal_desc->AlphaToCoverageEnable;

		for (UINT i = 0; i < 8; ++i)
		{
			const D3D11_RENDER_TARGET_BLEND_DESC1 &target = internal_desc->RenderTarget[internal_desc->IndependentBlendEnable ? i : 0];

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
	}
	else
	{
		// Default blend state (https://docs.microsoft.com/windows/win32/api/d3d11_1/ns-d3d11_1-d3d11_blend_desc1)
		for (UINT i = 0; i < 8; ++i)
		{
			desc.graphics.blend_state.src_color_blend_factor[i] = api::blend_factor::one;
			desc.graphics.blend_state.dst_color_blend_factor[i] = api::blend_factor::zero;
			desc.graphics.blend_state.color_blend_op[i] = api::blend_op::add;
			desc.graphics.blend_state.src_alpha_blend_factor[i] = api::blend_factor::one;
			desc.graphics.blend_state.dst_alpha_blend_factor[i] = api::blend_factor::zero;
			desc.graphics.blend_state.alpha_blend_op[i] = api::blend_op::add;
			desc.graphics.blend_state.logic_op[i] = api::logic_op::noop;
			desc.graphics.blend_state.render_target_write_mask[i] = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
	}

	return desc;
}
reshade::api::pipeline_desc reshade::d3d11::convert_pipeline_desc(const D3D11_RASTERIZER_DESC *internal_desc)
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
		// Default rasterizer state (https://docs.microsoft.com/windows/win32/api/d3d11/ns-d3d11-d3d11_rasterizer_desc)
		desc.graphics.rasterizer_state.fill_mode = api::fill_mode::solid;
		desc.graphics.rasterizer_state.cull_mode = api::cull_mode::back;
		desc.graphics.rasterizer_state.depth_clip_enable = true;
	}

	return desc;
}
reshade::api::pipeline_desc reshade::d3d11::convert_pipeline_desc(const D3D11_RASTERIZER_DESC1 *internal_desc)
{
	// D3D11_RASTERIZER_DESC1 is a superset of D3D11_RASTERIZER_DESC
	// Missing fields: ForcedSampleCount
	return convert_pipeline_desc(reinterpret_cast<const D3D11_RASTERIZER_DESC *>(internal_desc));
}
reshade::api::pipeline_desc reshade::d3d11::convert_pipeline_desc(const D3D11_RASTERIZER_DESC2 *internal_desc)
{
	// D3D11_RASTERIZER_DESC2 is a superset of D3D11_RASTERIZER_DESC1
	// Missing fields: ForcedSampleCount, ConservativeRaster
	return convert_pipeline_desc(reinterpret_cast<const D3D11_RASTERIZER_DESC *>(internal_desc));
}
reshade::api::pipeline_desc reshade::d3d11::convert_pipeline_desc(const D3D11_DEPTH_STENCIL_DESC *internal_desc)
{
	api::pipeline_desc desc = { api::pipeline_stage::depth_stencil };

	if (internal_desc != nullptr)
	{
		desc.graphics.depth_stencil_state.depth_enable = internal_desc->DepthEnable;
		desc.graphics.depth_stencil_state.depth_write_mask = internal_desc->DepthWriteMask != D3D11_DEPTH_WRITE_MASK_ZERO;
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
		// Default depth-stencil state (https://docs.microsoft.com/windows/win32/api/d3d11/ns-d3d11-d3d11_depth_stencil_desc)
		desc.graphics.depth_stencil_state.depth_enable = true;
		desc.graphics.depth_stencil_state.depth_write_mask = true;
		desc.graphics.depth_stencil_state.depth_func = api::compare_op::less;
		desc.graphics.depth_stencil_state.stencil_read_mask = D3D11_DEFAULT_STENCIL_READ_MASK;
		desc.graphics.depth_stencil_state.stencil_write_mask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
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

auto reshade::d3d11::convert_logic_op(api::logic_op value) -> D3D11_LOGIC_OP
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::logic_op::clear:
		return D3D11_LOGIC_OP_CLEAR;
	case api::logic_op::bitwise_and:
		return D3D11_LOGIC_OP_AND;
	case api::logic_op::bitwise_and_reverse:
		return D3D11_LOGIC_OP_AND_REVERSE;
	case api::logic_op::copy:
		return D3D11_LOGIC_OP_COPY;
	case api::logic_op::bitwise_and_inverted:
		return D3D11_LOGIC_OP_AND_INVERTED;
	case api::logic_op::noop:
		return D3D11_LOGIC_OP_NOOP;
	case api::logic_op::bitwise_xor:
		return D3D11_LOGIC_OP_XOR;
	case api::logic_op::bitwise_or:
		return D3D11_LOGIC_OP_OR;
	case api::logic_op::bitwise_nor:
		return D3D11_LOGIC_OP_NOR;
	case api::logic_op::equivalent:
		return D3D11_LOGIC_OP_EQUIV;
	case api::logic_op::invert:
		return D3D11_LOGIC_OP_INVERT;
	case api::logic_op::bitwise_or_reverse:
		return D3D11_LOGIC_OP_OR_REVERSE;
	case api::logic_op::copy_inverted:
		return D3D11_LOGIC_OP_COPY_INVERTED;
	case api::logic_op::bitwise_or_inverted:
		return D3D11_LOGIC_OP_OR_INVERTED;
	case api::logic_op::bitwise_nand:
		return D3D11_LOGIC_OP_NAND;
	case api::logic_op::set:
		return D3D11_LOGIC_OP_SET;
	}
}
auto reshade::d3d11::convert_logic_op(D3D11_LOGIC_OP value) -> api::logic_op
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case D3D11_LOGIC_OP_CLEAR:
		return api::logic_op::clear;
	case D3D11_LOGIC_OP_AND:
		return api::logic_op::bitwise_and;
	case D3D11_LOGIC_OP_AND_REVERSE:
		return api::logic_op::bitwise_and_reverse;
	case D3D11_LOGIC_OP_COPY:
		return api::logic_op::copy;
	case D3D11_LOGIC_OP_AND_INVERTED:
		return api::logic_op::bitwise_and_inverted;
	case D3D11_LOGIC_OP_NOOP:
		return api::logic_op::noop;
	case D3D11_LOGIC_OP_XOR:
		return api::logic_op::bitwise_xor;
	case D3D11_LOGIC_OP_OR:
		return api::logic_op::bitwise_or;
	case D3D11_LOGIC_OP_NOR:
		return api::logic_op::bitwise_nor;
	case D3D11_LOGIC_OP_EQUIV:
		return api::logic_op::equivalent;
	case D3D11_LOGIC_OP_INVERT:
		return api::logic_op::invert;
	case D3D11_LOGIC_OP_OR_REVERSE:
		return api::logic_op::bitwise_or_reverse;
	case D3D11_LOGIC_OP_COPY_INVERTED:
		return api::logic_op::copy_inverted;
	case D3D11_LOGIC_OP_OR_INVERTED:
		return api::logic_op::bitwise_or_inverted;
	case D3D11_LOGIC_OP_NAND:
		return api::logic_op::bitwise_nand;
	case D3D11_LOGIC_OP_SET:
		return api::logic_op::set;
	}
}
auto reshade::d3d11::convert_blend_op(api::blend_op value) -> D3D11_BLEND_OP
{
	return static_cast<D3D11_BLEND_OP>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d11::convert_blend_op(D3D11_BLEND_OP value) -> api::blend_op
{
	return static_cast<api::blend_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d11::convert_blend_factor(api::blend_factor value) -> D3D11_BLEND
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::zero:
		return D3D11_BLEND_ZERO;
	case api::blend_factor::one:
		return D3D11_BLEND_ONE;
	case api::blend_factor::src_color:
		return D3D11_BLEND_SRC_COLOR;
	case api::blend_factor::inv_src_color:
		return D3D11_BLEND_INV_SRC_COLOR;
	case api::blend_factor::dst_color:
		return D3D11_BLEND_DEST_COLOR;
	case api::blend_factor::inv_dst_color:
		return D3D11_BLEND_INV_DEST_COLOR;
	case api::blend_factor::src_alpha:
		return D3D11_BLEND_SRC_ALPHA;
	case api::blend_factor::inv_src_alpha:
		return D3D11_BLEND_INV_SRC_ALPHA;
	case api::blend_factor::dst_alpha:
		return D3D11_BLEND_DEST_ALPHA;
	case api::blend_factor::inv_dst_alpha:
		return D3D11_BLEND_INV_DEST_ALPHA;
	case api::blend_factor::constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::constant_color:
		return D3D11_BLEND_BLEND_FACTOR;
	case api::blend_factor::inv_constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::inv_constant_color:
		return D3D11_BLEND_INV_BLEND_FACTOR;
	case api::blend_factor::src_alpha_sat:
		return D3D11_BLEND_SRC_ALPHA_SAT;
	case api::blend_factor::src1_color:
		return D3D11_BLEND_SRC1_COLOR;
	case api::blend_factor::inv_src1_color:
		return D3D11_BLEND_INV_SRC1_COLOR;
	case api::blend_factor::src1_alpha:
		return D3D11_BLEND_SRC1_ALPHA;
	case api::blend_factor::inv_src1_alpha:
		return D3D11_BLEND_INV_SRC1_ALPHA;
	}
}
auto reshade::d3d11::convert_blend_factor(D3D11_BLEND value) -> api::blend_factor
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case D3D11_BLEND_ZERO:
		return api::blend_factor::zero;
	case D3D11_BLEND_ONE:
		return api::blend_factor::one;
	case D3D11_BLEND_SRC_COLOR:
		return api::blend_factor::src_color;
	case D3D11_BLEND_INV_SRC_COLOR:
		return api::blend_factor::inv_src_color;
	case D3D11_BLEND_DEST_COLOR:
		return api::blend_factor::dst_color;
	case D3D11_BLEND_INV_DEST_COLOR:
		return api::blend_factor::inv_dst_color;
	case D3D11_BLEND_SRC_ALPHA:
		return api::blend_factor::src_alpha;
	case D3D11_BLEND_INV_SRC_ALPHA:
		return api::blend_factor::inv_src_alpha;
	case D3D11_BLEND_DEST_ALPHA:
		return api::blend_factor::dst_alpha;
	case D3D11_BLEND_INV_DEST_ALPHA:
		return api::blend_factor::inv_dst_alpha;
	case D3D11_BLEND_BLEND_FACTOR:
		return api::blend_factor::constant_color;
	case D3D11_BLEND_INV_BLEND_FACTOR:
		return api::blend_factor::inv_constant_color;
	case D3D11_BLEND_SRC_ALPHA_SAT:
		return api::blend_factor::src_alpha_sat;
	case D3D11_BLEND_SRC1_COLOR:
		return api::blend_factor::src1_color;
	case D3D11_BLEND_INV_SRC1_COLOR:
		return api::blend_factor::inv_src1_color;
	case D3D11_BLEND_SRC1_ALPHA:
		return api::blend_factor::src1_alpha;
	case D3D11_BLEND_INV_SRC1_ALPHA:
		return api::blend_factor::inv_src1_alpha;
	}
}
auto reshade::d3d11::convert_fill_mode(api::fill_mode value) -> D3D11_FILL_MODE
{
	switch (value)
	{
	default:
	case api::fill_mode::point:
		assert(false);
		[[fallthrough]];
	case api::fill_mode::solid:
		return D3D11_FILL_SOLID;
	case api::fill_mode::wireframe:
		return D3D11_FILL_WIREFRAME;
	}
}
auto reshade::d3d11::convert_fill_mode(D3D11_FILL_MODE value) -> api::fill_mode
{
	switch (value)
	{
	default:
	case D3D11_FILL_SOLID:
		return api::fill_mode::solid;
	case D3D11_FILL_WIREFRAME:
		return api::fill_mode::wireframe;
	}
}
auto reshade::d3d11::convert_cull_mode(api::cull_mode value) -> D3D11_CULL_MODE
{
	assert(value != api::cull_mode::front_and_back);
	return static_cast<D3D11_CULL_MODE>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d11::convert_cull_mode(D3D11_CULL_MODE value) -> api::cull_mode
{
	return static_cast<api::cull_mode>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d11::convert_compare_op(api::compare_op value) -> D3D11_COMPARISON_FUNC
{
	return static_cast<D3D11_COMPARISON_FUNC>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d11::convert_compare_op(D3D11_COMPARISON_FUNC value) -> api::compare_op
{
	return static_cast<api::compare_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d11::convert_stencil_op(api::stencil_op value) -> D3D11_STENCIL_OP
{
	return static_cast<D3D11_STENCIL_OP>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d11::convert_stencil_op(D3D11_STENCIL_OP value) -> api::stencil_op
{
	return static_cast<api::stencil_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d11::convert_query_type(api::query_type value) -> D3D11_QUERY
{
	switch (value)
	{
	case api::query_type::occlusion:
		return D3D11_QUERY_OCCLUSION;
	case api::query_type::binary_occlusion:
		return D3D11_QUERY_OCCLUSION_PREDICATE;
	case api::query_type::timestamp:
		return D3D11_QUERY_TIMESTAMP;
	case api::query_type::pipeline_statistics:
		return D3D11_QUERY_PIPELINE_STATISTICS;
	default:
		assert(false);
		return static_cast<D3D11_QUERY>(0xFFFFFFFF);
	}
}
