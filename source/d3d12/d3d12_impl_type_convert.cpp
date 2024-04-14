/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d12_impl_type_convert.hpp"
#include <limits>
#include <cassert>

// {B2257A30-4014-46EA-BD88-DEC21DB6A02B}
const GUID reshade::d3d12::extra_data_guid = { 0xB2257A30, 0x4014, 0x46EA, { 0xBD, 0x88, 0xDE, 0xC2, 0x1D, 0xB6, 0xA0, 0x2B } };

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

auto reshade::d3d12::convert_color_space(api::color_space type) -> DXGI_COLOR_SPACE_TYPE
{
	switch (type)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::color_space::srgb_nonlinear:
		return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
	case api::color_space::extended_srgb_linear:
		return DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
	case api::color_space::hdr10_st2084:
		return DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
	case api::color_space::hdr10_hlg:
		return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020;
	}
}
auto reshade::d3d12::convert_color_space(DXGI_COLOR_SPACE_TYPE type) -> api::color_space
{
	switch (type)
	{
	default:
		assert(false);
		return api::color_space::unknown;
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
		return api::color_space::srgb_nonlinear;
	case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
		return api::color_space::extended_srgb_linear;
	case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
		return api::color_space::hdr10_st2084;
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
		return api::color_space::hdr10_hlg;
	}
}

auto reshade::d3d12::convert_access_to_usage(D3D12_BARRIER_ACCESS access) -> api::resource_usage
{
	if (access == D3D12_BARRIER_ACCESS_COMMON)
		return api::resource_usage::general;
	if (access == D3D12_BARRIER_ACCESS_NO_ACCESS)
		return api::resource_usage::undefined;

	api::resource_usage result = api::resource_usage::undefined;
	if ((access & D3D12_BARRIER_ACCESS_VERTEX_BUFFER) != 0)
		result |= api::resource_usage::vertex_buffer;
	if ((access & D3D12_BARRIER_ACCESS_CONSTANT_BUFFER) != 0)
		result |= api::resource_usage::constant_buffer;
	if ((access & D3D12_BARRIER_ACCESS_INDEX_BUFFER) != 0)
		result |= api::resource_usage::index_buffer;
	if ((access & D3D12_BARRIER_ACCESS_RENDER_TARGET) != 0)
		result |= api::resource_usage::render_target;
	if ((access & D3D12_BARRIER_ACCESS_UNORDERED_ACCESS) != 0)
		result |= api::resource_usage::unordered_access;
	if ((access & D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE) != 0)
		result |= api::resource_usage::depth_stencil_write;
	if ((access & D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ) != 0)
		result |= api::resource_usage::depth_stencil_read;
	if ((access & D3D12_BARRIER_ACCESS_SHADER_RESOURCE) != 0)
		result |= api::resource_usage::shader_resource;
	if ((access & D3D12_BARRIER_ACCESS_STREAM_OUTPUT) != 0)
		result |= api::resource_usage::stream_output;
	if ((access & D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT) != 0)
		result |= api::resource_usage::indirect_argument;
	if ((access & D3D12_BARRIER_ACCESS_COPY_DEST) != 0)
		result |= api::resource_usage::copy_dest;
	if ((access & D3D12_BARRIER_ACCESS_COPY_SOURCE) != 0)
		result |= api::resource_usage::copy_source;
	if ((access & D3D12_BARRIER_ACCESS_RESOLVE_DEST) != 0)
		result |= api::resource_usage::resolve_dest;
	if ((access & D3D12_BARRIER_ACCESS_RESOLVE_SOURCE) != 0)
		result |= api::resource_usage::resolve_source;
	if ((access & (D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ | D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE)) != 0)
		result |= api::resource_usage::acceleration_structure;

	return result;
}
auto reshade::d3d12::convert_barrier_layout_to_usage(D3D12_BARRIER_LAYOUT layout) -> api::resource_usage
{
	switch (layout)
	{
	case D3D12_BARRIER_LAYOUT_UNDEFINED:
		return api::resource_usage::undefined;
	default:
	case D3D12_BARRIER_LAYOUT_COMMON:
	case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON:
	case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COMMON:
	case D3D12_BARRIER_LAYOUT_VIDEO_QUEUE_COMMON:
		return api::resource_usage::general;
	case D3D12_BARRIER_LAYOUT_GENERIC_READ:
	case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ:
	case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_GENERIC_READ:
		return api::resource_usage::cpu_access;
	case D3D12_BARRIER_LAYOUT_RENDER_TARGET:
		return api::resource_usage::render_target;
	case D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS:
	case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS:
	case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_UNORDERED_ACCESS:
		return api::resource_usage::unordered_access;
	case D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE:
		return api::resource_usage::depth_stencil_write;
	case D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ:
		return api::resource_usage::depth_stencil_read;
	case D3D12_BARRIER_LAYOUT_SHADER_RESOURCE:
	case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE:
	case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_SHADER_RESOURCE:
		return api::resource_usage::shader_resource;
	case D3D12_BARRIER_LAYOUT_COPY_SOURCE:
	case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_SOURCE:
	case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COPY_SOURCE:
		return api::resource_usage::copy_source;
	case D3D12_BARRIER_LAYOUT_COPY_DEST:
	case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_DEST:
	case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COPY_DEST:
		return api::resource_usage::copy_dest;
	case D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE:
		return api::resource_usage::resolve_source;
	case D3D12_BARRIER_LAYOUT_RESOLVE_DEST:
		return api::resource_usage::resolve_dest;
	}
}
auto reshade::d3d12::convert_resource_states_to_usage(D3D12_RESOURCE_STATES states) -> api::resource_usage
{
	static_assert(
		api::resource_usage::index_buffer == D3D12_RESOURCE_STATE_INDEX_BUFFER &&
		api::resource_usage::vertex_buffer == D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER &&
		api::resource_usage::stream_output == D3D12_RESOURCE_STATE_STREAM_OUT &&
		api::resource_usage::indirect_argument == D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT &&
		api::resource_usage::depth_stencil_read == D3D12_RESOURCE_STATE_DEPTH_READ &&
		api::resource_usage::depth_stencil_write == D3D12_RESOURCE_STATE_DEPTH_WRITE &&
		api::resource_usage::render_target == D3D12_RESOURCE_STATE_RENDER_TARGET &&
		api::resource_usage::shader_resource_pixel == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE &&
		api::resource_usage::shader_resource_non_pixel == D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE &&
		api::resource_usage::unordered_access == D3D12_RESOURCE_STATE_UNORDERED_ACCESS &&
		api::resource_usage::copy_dest == D3D12_RESOURCE_STATE_COPY_DEST &&
		api::resource_usage::copy_source == D3D12_RESOURCE_STATE_COPY_SOURCE &&
		api::resource_usage::resolve_dest == D3D12_RESOURCE_STATE_RESOLVE_DEST &&
		api::resource_usage::resolve_source == D3D12_RESOURCE_STATE_RESOLVE_SOURCE &&
		api::resource_usage::acceleration_structure == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

	return states == D3D12_RESOURCE_STATE_COMMON ? api::resource_usage::general : static_cast<api::resource_usage>(states);
}

auto reshade::d3d12::convert_usage_to_access(api::resource_usage state) -> D3D12_BARRIER_ACCESS
{
	if (state == api::resource_usage::general)
		return D3D12_BARRIER_ACCESS_COMMON;
	if (state == api::resource_usage::undefined)
		return D3D12_BARRIER_ACCESS_NO_ACCESS;

	D3D12_BARRIER_ACCESS result = D3D12_BARRIER_ACCESS_COMMON;
	if ((state & api::resource_usage::index_buffer) != 0)
		result |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
	if ((state & api::resource_usage::vertex_buffer) != 0)
		result |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
	if ((state & api::resource_usage::constant_buffer) != 0)
		result |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
	if ((state & api::resource_usage::stream_output) != 0)
		result |= D3D12_BARRIER_ACCESS_STREAM_OUTPUT;
	if ((state & api::resource_usage::indirect_argument) != 0)
		result |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
	if ((state & api::resource_usage::depth_stencil_read) != 0)
		result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
	if ((state & api::resource_usage::depth_stencil_write) != 0)
		result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
	if ((state & api::resource_usage::render_target) != 0)
		result |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
	if ((state & api::resource_usage::shader_resource) != 0)
		result |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
	if ((state & api::resource_usage::unordered_access) != 0)
		result |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	if ((state & api::resource_usage::copy_dest) != 0)
		result |= D3D12_BARRIER_ACCESS_COPY_DEST;
	if ((state & api::resource_usage::copy_source) != 0)
		result |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
	if ((state & api::resource_usage::resolve_dest) != 0)
		result |= D3D12_BARRIER_ACCESS_RESOLVE_DEST;
	if ((state & api::resource_usage::resolve_source) != 0)
		result |= D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
	if ((state & api::resource_usage::acceleration_structure) != 0)
		result |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ | D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;

	return result;
}
auto reshade::d3d12::convert_usage_to_resource_states(api::resource_usage state) -> D3D12_RESOURCE_STATES
{
	// Undefined resource state does not exist in D3D12
	assert(state != api::resource_usage::undefined);

	if (state == api::resource_usage::general)
		return D3D12_RESOURCE_STATE_COMMON;
	if (state == api::resource_usage::present)
		return D3D12_RESOURCE_STATE_PRESENT;
	if (state == api::resource_usage::cpu_access)
		return D3D12_RESOURCE_STATE_GENERIC_READ;

	auto result = static_cast<D3D12_RESOURCE_STATES>(static_cast<uint32_t>(state) & 0xFFFFFFF);

	// Depth write state is mutually exclusive with other states, so remove it when read state is specified too
	if ((state & api::resource_usage::depth_stencil) == api::resource_usage::depth_stencil)
	{
		if (state == api::resource_usage::depth_stencil)
			return D3D12_RESOURCE_STATE_DEPTH_WRITE; // Unless there are no other states specified, in that case only specify write state
		else
			result ^= D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	// The separate constant buffer state does not exist in D3D12, so replace it with the combined vertex/constant buffer one
	if ((state & api::resource_usage::constant_buffer) == api::resource_usage::constant_buffer)
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
void reshade::d3d12::convert_sampler_desc(const api::sampler_desc &desc, D3D12_SAMPLER_DESC2 &internal_desc)
{
	// D3D12_SAMPLER_DESC2 is a superset of D3D12_SAMPLER_DESC
	// Missing fields: Flags
	convert_sampler_desc(desc, reinterpret_cast<D3D12_SAMPLER_DESC &>(internal_desc));

	if ((internal_desc.Flags & D3D12_SAMPLER_FLAG_UINT_BORDER_COLOR) != 0)
		for (int i = 0; i < 4; ++i)
			internal_desc.UintBorderColor[i] = static_cast<UINT>(desc.border_color[i]);
}
void reshade::d3d12::convert_sampler_desc(const api::sampler_desc &desc, D3D12_STATIC_SAMPLER_DESC &internal_desc)
{
	internal_desc.Filter = static_cast<D3D12_FILTER>(desc.filter);
	internal_desc.AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(desc.address_u);
	internal_desc.AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(desc.address_v);
	internal_desc.AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(desc.address_w);
	internal_desc.MipLODBias = desc.mip_lod_bias;
	internal_desc.MaxAnisotropy = static_cast<UINT>(desc.max_anisotropy);
	internal_desc.ComparisonFunc = convert_compare_op(desc.compare_op);
	internal_desc.MinLOD = desc.min_lod;
	internal_desc.MaxLOD = desc.max_lod;

	const bool is_float_border_color =
		internal_desc.BorderColor != D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT &&
		internal_desc.BorderColor != D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE_UINT;

	if (desc.border_color[3] == 0.0f)
		internal_desc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	else if (desc.border_color[0] == 0.0f && desc.border_color[1] == 0.0f && desc.border_color[2] == 0.0f)
		internal_desc.BorderColor = is_float_border_color ? D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK : D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT;
	else
		internal_desc.BorderColor = is_float_border_color ? D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE : D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE_UINT;
}
void reshade::d3d12::convert_sampler_desc(const api::sampler_desc &desc, D3D12_STATIC_SAMPLER_DESC1 &internal_desc)
{
	// D3D12_STATIC_SAMPLER_DESC1 is a superset of D3D12_STATIC_SAMPLER_DESC
	// Missing fields: Flags
	convert_sampler_desc(desc, reinterpret_cast<D3D12_STATIC_SAMPLER_DESC &>(internal_desc));
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
reshade::api::sampler_desc reshade::d3d12::convert_sampler_desc(const D3D12_SAMPLER_DESC2 &internal_desc)
{
	// D3D12_SAMPLER_DESC2 is a superset of D3D12_SAMPLER_DESC
	// Missing fields: Flags
	api::sampler_desc desc = convert_sampler_desc(reinterpret_cast<const D3D12_SAMPLER_DESC &>(internal_desc));

	static_assert(offsetof(D3D12_SAMPLER_DESC, BorderColor) == offsetof(D3D12_SAMPLER_DESC2, FloatBorderColor));
	if ((internal_desc.Flags & D3D12_SAMPLER_FLAG_UINT_BORDER_COLOR) != 0)
		for (int i = 0; i < 4; ++i)
			desc.border_color[i] = static_cast<float>(internal_desc.UintBorderColor[i]);

	return desc;
}
reshade::api::sampler_desc reshade::d3d12::convert_sampler_desc(const D3D12_STATIC_SAMPLER_DESC &internal_desc)
{
	api::sampler_desc desc = {};
	desc.filter = static_cast<api::filter_mode>(internal_desc.Filter);
	desc.address_u = static_cast<api::texture_address_mode>(internal_desc.AddressU);
	desc.address_v = static_cast<api::texture_address_mode>(internal_desc.AddressV);
	desc.address_w = static_cast<api::texture_address_mode>(internal_desc.AddressW);
	desc.mip_lod_bias = internal_desc.MipLODBias;
	desc.max_anisotropy = static_cast<float>(internal_desc.MaxAnisotropy);
	desc.compare_op = convert_compare_op(internal_desc.ComparisonFunc);
	desc.min_lod = internal_desc.MinLOD;
	desc.max_lod = internal_desc.MaxLOD;

	switch (internal_desc.BorderColor)
	{
	case D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK:
		break;
	case D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK:
	case D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT:
		desc.border_color[3] = 1.0f;
		break;
	case D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE:
	case D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE_UINT:
		std::fill_n(desc.border_color, 4, 1.0f);
		break;
	}

	return desc;
}
reshade::api::sampler_desc reshade::d3d12::convert_sampler_desc(const D3D12_STATIC_SAMPLER_DESC1 &internal_desc)
{
	// D3D12_STATIC_SAMPLER_DESC1 is a superset of D3D12_STATIC_SAMPLER_DESC
	// Missing fields: Flags
	return convert_sampler_desc(reinterpret_cast<const D3D12_STATIC_SAMPLER_DESC &>(internal_desc));
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

	if ((desc.usage & api::resource_usage::depth_stencil) != 0)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	if ((desc.usage & api::resource_usage::render_target) != 0)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	if ((desc.usage & api::resource_usage::shader_resource) == 0 && (internal_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	// Mipmap generation is using compute shaders and therefore needs unordered access flag (see 'command_list_impl::generate_mipmaps')
	if ((desc.usage & (api::resource_usage::unordered_access | api::resource_usage::acceleration_structure)) != 0 || (desc.flags & api::resource_flags::generate_mipmaps) != 0)
		internal_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	else
		internal_desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	if ((desc.flags & api::resource_flags::shared) != 0)
		heap_flags |= D3D12_HEAP_FLAG_SHARED;

	// Dynamic resources do not exist in D3D12
	assert((desc.flags & api::resource_flags::dynamic) == 0);
}
void reshade::d3d12::convert_resource_desc(const api::resource_desc &desc, D3D12_RESOURCE_DESC1 &internal_desc, D3D12_HEAP_PROPERTIES &heap_props, D3D12_HEAP_FLAGS &heap_flags)
{
	// D3D12_RESOURCE_DESC1 is a superset of D3D12_RESOURCE_DESC
	convert_resource_desc(desc, reinterpret_cast<D3D12_RESOURCE_DESC &>(internal_desc), heap_props, heap_flags);
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
		desc.buffer.stride = 0;

		// Buffers may be of any type in D3D12, so add all possible usage flags
		desc.usage |= api::resource_usage::vertex_buffer | api::resource_usage::index_buffer | api::resource_usage::constant_buffer | api::resource_usage::stream_output;
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
			desc.usage |= (desc.texture.samples > 1) ? api::resource_usage::resolve_source : api::resource_usage::resolve_dest;
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
		// Buffers that have unordered access may be used as acceleration structures
		if (desc.type == api::resource_type::buffer)
			desc.usage |= api::resource_usage::acceleration_structure;
		// Resources that have unordered access are usable with the 'generate_mipmaps' function
		if (internal_desc.MipLevels > 1)
			desc.flags |= api::resource_flags::generate_mipmaps;
	}

	if ((heap_flags & D3D12_HEAP_FLAG_SHARED) != 0)
		desc.flags |= api::resource_flags::shared | api::resource_flags::shared_nt_handle;

	return desc;
}
reshade::api::resource_desc reshade::d3d12::convert_resource_desc(const D3D12_RESOURCE_DESC1 &internal_desc, const D3D12_HEAP_PROPERTIES &heap_props, D3D12_HEAP_FLAGS heap_flags)
{
	// D3D12_RESOURCE_DESC1 is a superset of D3D12_RESOURCE_DESC
	return convert_resource_desc(reinterpret_cast<const D3D12_RESOURCE_DESC &>(internal_desc), heap_props, heap_flags);
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
		if (desc.texture.layer_count == UINT32_MAX)
			internal_desc.TextureCubeArray.NumCubes = UINT_MAX;
		else
			internal_desc.TextureCubeArray.NumCubes = desc.texture.layer_count / 6;
		// Missing fields: D3D12_TEXCUBE_ARRAY_SRV::ResourceMinLODClamp
		break;
	case api::resource_view_type::acceleration_structure:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		internal_desc.RaytracingAccelerationStructure.Location = desc.buffer.offset;
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
reshade::api::resource_view_desc reshade::d3d12::convert_resource_view_desc(const D3D12_RESOURCE_DESC &resource_desc)
{
	api::resource_view_desc desc = {};
	switch (resource_desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_BUFFER:
		desc.type = api::resource_view_type::buffer;
		desc.buffer.offset = 0;
		desc.buffer.size = UINT64_MAX;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		desc.type = api::resource_view_type::texture_1d;
		desc.texture.first_level = 0;
		desc.texture.level_count = UINT32_MAX;
		desc.texture.first_layer = 0;
		desc.texture.layer_count = UINT32_MAX;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		desc.type = api::resource_view_type::texture_2d;
		desc.texture.first_level = 0;
		desc.texture.level_count = UINT32_MAX;
		desc.texture.first_layer = 0;
		desc.texture.layer_count = UINT32_MAX;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		desc.type = api::resource_view_type::texture_3d;
		desc.texture.first_level = 0;
		desc.texture.level_count = UINT32_MAX;
		desc.texture.first_layer = 0;
		desc.texture.layer_count = UINT32_MAX;
		break;
	}

	desc.format = convert_format(resource_desc.Format);

	return desc;
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
		if (internal_desc.TextureCubeArray.NumCubes == UINT_MAX)
			desc.texture.layer_count = UINT32_MAX;
		else
			desc.texture.layer_count = internal_desc.TextureCubeArray.NumCubes * 6;
		// Missing fields: D3D12_TEXCUBE_ARRAY_SRV::ResourceMinLODClamp
		break;
	case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
		desc.type = api::resource_view_type::acceleration_structure;
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

void reshade::d3d12::convert_shader_desc(const api::shader_desc &desc, D3D12_SHADER_BYTECODE &internal_desc)
{
	assert(desc.entry_point == nullptr && desc.spec_constants == 0);
	internal_desc.pShaderBytecode = desc.code;
	internal_desc.BytecodeLength = desc.code_size;
}
reshade::api::shader_desc reshade::d3d12::convert_shader_desc(const D3D12_SHADER_BYTECODE &internal_desc)
{
	api::shader_desc desc = {};
	desc.code = internal_desc.pShaderBytecode;
	desc.code_size = internal_desc.BytecodeLength;
	return desc;
}

void reshade::d3d12::convert_input_layout_desc(uint32_t count, const api::input_element *elements, std::vector<D3D12_INPUT_ELEMENT_DESC> &internal_elements)
{
	internal_elements.reserve(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		const api::input_element &element = elements[i];

		D3D12_INPUT_ELEMENT_DESC &internal_element = internal_elements.emplace_back();
		internal_element.SemanticName = element.semantic;
		internal_element.SemanticIndex = element.semantic_index;
		internal_element.Format = convert_format(element.format);
		internal_element.InputSlot = element.buffer_binding;
		internal_element.AlignedByteOffset = element.offset;
		internal_element.InputSlotClass = element.instance_step_rate > 0 ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		internal_element.InstanceDataStepRate = element.instance_step_rate;
	}
}
std::vector<reshade::api::input_element> reshade::d3d12::convert_input_layout_desc(const D3D12_INPUT_LAYOUT_DESC &internal_desc)
{
	std::vector<api::input_element> elements;
	elements.reserve(internal_desc.NumElements);

	for (UINT i = 0; i < internal_desc.NumElements; ++i)
	{
		const D3D12_INPUT_ELEMENT_DESC &internal_element = internal_desc.pInputElementDescs[i];

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

void reshade::d3d12::convert_stream_output_desc(const api::stream_output_desc &desc, D3D12_STREAM_OUTPUT_DESC &internal_desc)
{
	internal_desc.RasterizedStream = desc.rasterized_stream;
}
reshade::api::stream_output_desc reshade::d3d12::convert_stream_output_desc(const D3D12_STREAM_OUTPUT_DESC &internal_desc)
{
	api::stream_output_desc desc = {};
	desc.rasterized_stream = internal_desc.RasterizedStream;
	return desc;
}
void reshade::d3d12::convert_blend_desc(const api::blend_desc &desc, D3D12_BLEND_DESC &internal_desc)
{
	internal_desc.AlphaToCoverageEnable = desc.alpha_to_coverage_enable;
	internal_desc.IndependentBlendEnable = FALSE;

	for (UINT i = 0; i < 8; ++i)
	{
		if (desc.blend_enable[i] != desc.blend_enable[0] ||
			desc.logic_op_enable[i] != desc.logic_op_enable[0] ||
			desc.source_color_blend_factor[i] != desc.source_color_blend_factor[0] ||
			desc.dest_color_blend_factor[i] != desc.dest_color_blend_factor[0] ||
			desc.color_blend_op[i] != desc.color_blend_op[0] ||
			desc.source_alpha_blend_factor[i] != desc.source_alpha_blend_factor[0] ||
			desc.dest_alpha_blend_factor[i] != desc.dest_alpha_blend_factor[0] ||
			desc.alpha_blend_op[i] != desc.alpha_blend_op[0] ||
			desc.logic_op[i] != desc.logic_op[0] ||
			desc.render_target_write_mask[i] != desc.render_target_write_mask[0])
			internal_desc.IndependentBlendEnable = TRUE;

		internal_desc.RenderTarget[i].BlendEnable = desc.blend_enable[i];
		internal_desc.RenderTarget[i].LogicOpEnable = desc.logic_op_enable[i];
		internal_desc.RenderTarget[i].SrcBlend = convert_blend_factor(desc.source_color_blend_factor[i]);
		internal_desc.RenderTarget[i].DestBlend = convert_blend_factor(desc.dest_color_blend_factor[i]);
		internal_desc.RenderTarget[i].BlendOp = convert_blend_op(desc.color_blend_op[i]);
		internal_desc.RenderTarget[i].SrcBlendAlpha = convert_blend_factor(desc.source_alpha_blend_factor[i]);
		internal_desc.RenderTarget[i].DestBlendAlpha = convert_blend_factor(desc.dest_alpha_blend_factor[i]);
		internal_desc.RenderTarget[i].BlendOpAlpha = convert_blend_op(desc.alpha_blend_op[i]);
		internal_desc.RenderTarget[i].LogicOp = convert_logic_op(desc.logic_op[i]);
		internal_desc.RenderTarget[i].RenderTargetWriteMask = desc.render_target_write_mask[i];
	}
}
reshade::api::blend_desc reshade::d3d12::convert_blend_desc(const D3D12_BLEND_DESC &internal_desc)
{
	api::blend_desc desc = {};
	desc.alpha_to_coverage_enable = internal_desc.AlphaToCoverageEnable;

	for (UINT i = 0; i < 8; ++i)
	{
		const D3D12_RENDER_TARGET_BLEND_DESC &target = internal_desc.RenderTarget[internal_desc.IndependentBlendEnable ? i : 0];

		desc.blend_enable[i] = target.BlendEnable;
		desc.logic_op_enable[i] = target.LogicOpEnable;

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
		if (target.LogicOpEnable)
		{
			assert(!target.BlendEnable && !internal_desc.IndependentBlendEnable);

			desc.logic_op[i] = convert_logic_op(target.LogicOp);
		}

		desc.render_target_write_mask[i] = target.RenderTargetWriteMask;
	}

	return desc;
}
void reshade::d3d12::convert_rasterizer_desc(const api::rasterizer_desc &desc, D3D12_RASTERIZER_DESC &internal_desc)
{
	internal_desc.FillMode = convert_fill_mode(desc.fill_mode);
	internal_desc.CullMode = convert_cull_mode(desc.cull_mode);
	internal_desc.FrontCounterClockwise = desc.front_counter_clockwise;
	internal_desc.DepthBias = static_cast<INT>(desc.depth_bias);
	internal_desc.DepthBiasClamp = desc.depth_bias_clamp;
	internal_desc.SlopeScaledDepthBias = desc.slope_scaled_depth_bias;
	internal_desc.DepthClipEnable = desc.depth_clip_enable;
	// Note: Scissor testing is always enabled in D3D12, so "desc.scissor_enable" is ignored
	internal_desc.MultisampleEnable = desc.multisample_enable;
	internal_desc.AntialiasedLineEnable = desc.antialiased_line_enable;
	internal_desc.ForcedSampleCount = 0;
	internal_desc.ConservativeRaster = static_cast<D3D12_CONSERVATIVE_RASTERIZATION_MODE>(desc.conservative_rasterization);
}
void reshade::d3d12::convert_rasterizer_desc(const api::rasterizer_desc &desc, D3D12_RASTERIZER_DESC1 &internal_desc)
{
	internal_desc.FillMode = convert_fill_mode(desc.fill_mode);
	internal_desc.CullMode = convert_cull_mode(desc.cull_mode);
	internal_desc.FrontCounterClockwise = desc.front_counter_clockwise;
	internal_desc.DepthBias = desc.depth_bias;
	internal_desc.DepthBiasClamp = desc.depth_bias_clamp;
	internal_desc.SlopeScaledDepthBias = desc.slope_scaled_depth_bias;
	internal_desc.DepthClipEnable = desc.depth_clip_enable;
	internal_desc.MultisampleEnable = desc.multisample_enable;
	internal_desc.AntialiasedLineEnable = desc.antialiased_line_enable;
	internal_desc.ForcedSampleCount = 0;
	internal_desc.ConservativeRaster = static_cast<D3D12_CONSERVATIVE_RASTERIZATION_MODE>(desc.conservative_rasterization);
}
void reshade::d3d12::convert_rasterizer_desc(const api::rasterizer_desc &desc, D3D12_RASTERIZER_DESC2 &internal_desc)
{
	internal_desc.FillMode = convert_fill_mode(desc.fill_mode);
	internal_desc.CullMode = convert_cull_mode(desc.cull_mode);
	internal_desc.FrontCounterClockwise = desc.front_counter_clockwise;
	internal_desc.DepthBias = desc.depth_bias;
	internal_desc.DepthBiasClamp = desc.depth_bias_clamp;
	internal_desc.SlopeScaledDepthBias = desc.slope_scaled_depth_bias;
	internal_desc.DepthClipEnable = desc.depth_clip_enable;
	// Note: Multisampling is not supported with this rasterizer description variant, so "desc.multisample_enable" is ignored
	internal_desc.LineRasterizationMode = static_cast<D3D12_LINE_RASTERIZATION_MODE>(desc.antialiased_line_enable);
	internal_desc.ForcedSampleCount = 0;
	internal_desc.ConservativeRaster = static_cast<D3D12_CONSERVATIVE_RASTERIZATION_MODE>(desc.conservative_rasterization);
}
reshade::api::rasterizer_desc reshade::d3d12::convert_rasterizer_desc(const D3D12_RASTERIZER_DESC &internal_desc)
{
	api::rasterizer_desc desc = {};
	desc.fill_mode = convert_fill_mode(internal_desc.FillMode);
	desc.cull_mode = convert_cull_mode(internal_desc.CullMode);
	desc.front_counter_clockwise = internal_desc.FrontCounterClockwise;
	desc.depth_bias = static_cast<float>(internal_desc.DepthBias);
	desc.depth_bias_clamp = internal_desc.DepthBiasClamp;
	desc.slope_scaled_depth_bias = internal_desc.SlopeScaledDepthBias;
	desc.depth_clip_enable = internal_desc.DepthClipEnable;
	desc.scissor_enable = true;
	desc.multisample_enable = internal_desc.MultisampleEnable;
	desc.antialiased_line_enable = internal_desc.AntialiasedLineEnable;
	desc.conservative_rasterization = static_cast<uint32_t>(internal_desc.ConservativeRaster);
	return desc;
}
reshade::api::rasterizer_desc reshade::d3d12::convert_rasterizer_desc(const D3D12_RASTERIZER_DESC1 &internal_desc)
{
	api::rasterizer_desc desc = {};
	desc.fill_mode = convert_fill_mode(internal_desc.FillMode);
	desc.cull_mode = convert_cull_mode(internal_desc.CullMode);
	desc.front_counter_clockwise = internal_desc.FrontCounterClockwise;
	desc.depth_bias = internal_desc.DepthBias;
	desc.depth_bias_clamp = internal_desc.DepthBiasClamp;
	desc.slope_scaled_depth_bias = internal_desc.SlopeScaledDepthBias;
	desc.depth_clip_enable = internal_desc.DepthClipEnable;
	desc.scissor_enable = true;
	desc.multisample_enable = internal_desc.MultisampleEnable;
	desc.antialiased_line_enable = internal_desc.AntialiasedLineEnable;
	desc.conservative_rasterization = static_cast<uint32_t>(internal_desc.ConservativeRaster);
	return desc;
}
reshade::api::rasterizer_desc reshade::d3d12::convert_rasterizer_desc(const D3D12_RASTERIZER_DESC2 &internal_desc)
{
	api::rasterizer_desc desc = {};
	desc.fill_mode = convert_fill_mode(internal_desc.FillMode);
	desc.cull_mode = convert_cull_mode(internal_desc.CullMode);
	desc.front_counter_clockwise = internal_desc.FrontCounterClockwise;
	desc.depth_bias = internal_desc.DepthBias;
	desc.depth_bias_clamp = internal_desc.DepthBiasClamp;
	desc.slope_scaled_depth_bias = internal_desc.SlopeScaledDepthBias;
	desc.depth_clip_enable = internal_desc.DepthClipEnable;
	desc.scissor_enable = true;
	desc.multisample_enable = false;
	desc.antialiased_line_enable = internal_desc.LineRasterizationMode != D3D12_LINE_RASTERIZATION_MODE_ALIASED;
	desc.conservative_rasterization = static_cast<uint32_t>(internal_desc.ConservativeRaster);
	return desc;
}
void reshade::d3d12::convert_depth_stencil_desc(const api::depth_stencil_desc &desc, D3D12_DEPTH_STENCIL_DESC &internal_desc)
{
	internal_desc.DepthEnable = desc.depth_enable;
	internal_desc.DepthWriteMask = desc.depth_write_mask ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
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
void reshade::d3d12::convert_depth_stencil_desc(const api::depth_stencil_desc &desc, D3D12_DEPTH_STENCIL_DESC1 &internal_desc)
{
	// D3D12_DEPTH_STENCIL_DESC1 is a superset of D3D12_DEPTH_STENCIL_DESC
	// Missing fields: DepthBoundsTestEnable
	convert_depth_stencil_desc(desc, reinterpret_cast<D3D12_DEPTH_STENCIL_DESC &>(internal_desc));
}
void reshade::d3d12::convert_depth_stencil_desc(const api::depth_stencil_desc &desc, D3D12_DEPTH_STENCIL_DESC2 &internal_desc)
{
	internal_desc.DepthEnable = desc.depth_enable;
	internal_desc.DepthWriteMask = desc.depth_write_mask ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	internal_desc.DepthFunc = convert_compare_op(desc.depth_func);
	internal_desc.StencilEnable = desc.stencil_enable;
	internal_desc.FrontFace.StencilFailOp = convert_stencil_op(desc.front_stencil_fail_op);
	internal_desc.FrontFace.StencilDepthFailOp = convert_stencil_op(desc.front_stencil_depth_fail_op);
	internal_desc.FrontFace.StencilPassOp = convert_stencil_op(desc.front_stencil_pass_op);
	internal_desc.FrontFace.StencilFunc = convert_compare_op(desc.front_stencil_func);
	internal_desc.FrontFace.StencilReadMask = desc.front_stencil_read_mask;
	internal_desc.FrontFace.StencilWriteMask = desc.front_stencil_write_mask;
	internal_desc.BackFace.StencilFailOp = convert_stencil_op(desc.back_stencil_fail_op);
	internal_desc.BackFace.StencilDepthFailOp = convert_stencil_op(desc.back_stencil_depth_fail_op);
	internal_desc.BackFace.StencilPassOp = convert_stencil_op(desc.back_stencil_pass_op);
	internal_desc.BackFace.StencilFunc = convert_compare_op(desc.back_stencil_func);
	internal_desc.BackFace.StencilReadMask = desc.back_stencil_read_mask;
	internal_desc.BackFace.StencilWriteMask = desc.back_stencil_write_mask;
	// Missing fields: DepthBoundsTestEnable
}
reshade::api::depth_stencil_desc reshade::d3d12::convert_depth_stencil_desc(const D3D12_DEPTH_STENCIL_DESC &internal_desc)
{
	api::depth_stencil_desc desc = {};
	desc.depth_enable = internal_desc.DepthEnable;
	desc.depth_write_mask = internal_desc.DepthWriteMask != D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.depth_func = convert_compare_op(internal_desc.DepthFunc);
	desc.stencil_enable = internal_desc.StencilEnable;
	desc.front_stencil_read_mask = internal_desc.StencilReadMask;
	desc.front_stencil_write_mask = internal_desc.StencilWriteMask;
	desc.front_stencil_reference_value = D3D12_DEFAULT_STENCIL_REFERENCE;
	desc.front_stencil_func = convert_compare_op(internal_desc.FrontFace.StencilFunc);
	desc.front_stencil_fail_op = convert_stencil_op(internal_desc.FrontFace.StencilFailOp);
	desc.front_stencil_depth_fail_op = convert_stencil_op(internal_desc.FrontFace.StencilDepthFailOp);
	desc.front_stencil_pass_op = convert_stencil_op(internal_desc.FrontFace.StencilPassOp);
	desc.back_stencil_read_mask = internal_desc.StencilReadMask;
	desc.back_stencil_write_mask = internal_desc.StencilWriteMask;
	desc.back_stencil_reference_value = D3D12_DEFAULT_STENCIL_REFERENCE;
	desc.back_stencil_func = convert_compare_op(internal_desc.BackFace.StencilFunc);
	desc.back_stencil_fail_op = convert_stencil_op(internal_desc.BackFace.StencilFailOp);
	desc.back_stencil_depth_fail_op = convert_stencil_op(internal_desc.BackFace.StencilDepthFailOp);
	desc.back_stencil_pass_op = convert_stencil_op(internal_desc.BackFace.StencilPassOp);
	return desc;
}
reshade::api::depth_stencil_desc reshade::d3d12::convert_depth_stencil_desc(const D3D12_DEPTH_STENCIL_DESC1 &internal_desc)
{
	// D3D12_DEPTH_STENCIL_DESC1 is a superset of D3D12_DEPTH_STENCIL_DESC
	// Missing fields: DepthBoundsTestEnable
	return convert_depth_stencil_desc(reinterpret_cast<const D3D12_DEPTH_STENCIL_DESC &>(internal_desc));
}
reshade::api::depth_stencil_desc reshade::d3d12::convert_depth_stencil_desc(const D3D12_DEPTH_STENCIL_DESC2 &internal_desc)
{
	api::depth_stencil_desc desc = {};
	desc.depth_enable = internal_desc.DepthEnable;
	desc.depth_write_mask = internal_desc.DepthWriteMask != D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.depth_func = convert_compare_op(internal_desc.DepthFunc);
	desc.stencil_enable = internal_desc.StencilEnable;
	desc.front_stencil_read_mask = internal_desc.FrontFace.StencilReadMask;
	desc.front_stencil_write_mask = internal_desc.FrontFace.StencilWriteMask;
	desc.front_stencil_reference_value = D3D12_DEFAULT_STENCIL_REFERENCE;
	desc.front_stencil_func = convert_compare_op(internal_desc.FrontFace.StencilFunc);
	desc.front_stencil_fail_op = convert_stencil_op(internal_desc.FrontFace.StencilFailOp);
	desc.front_stencil_depth_fail_op = convert_stencil_op(internal_desc.FrontFace.StencilDepthFailOp);
	desc.front_stencil_pass_op = convert_stencil_op(internal_desc.FrontFace.StencilPassOp);
	desc.back_stencil_read_mask = internal_desc.BackFace.StencilReadMask;
	desc.back_stencil_write_mask = internal_desc.BackFace.StencilWriteMask;
	desc.back_stencil_reference_value = D3D12_DEFAULT_STENCIL_REFERENCE;
	desc.back_stencil_func = convert_compare_op(internal_desc.BackFace.StencilFunc);
	desc.back_stencil_fail_op = convert_stencil_op(internal_desc.BackFace.StencilFailOp);
	desc.back_stencil_depth_fail_op = convert_stencil_op(internal_desc.BackFace.StencilDepthFailOp);
	desc.back_stencil_pass_op = convert_stencil_op(internal_desc.BackFace.StencilPassOp);
	// Missing fields: DepthBoundsTestEnable
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
	case api::blend_factor::source_color:
		return D3D12_BLEND_SRC_COLOR;
	case api::blend_factor::one_minus_source_color:
		return D3D12_BLEND_INV_SRC_COLOR;
	case api::blend_factor::dest_color:
		return D3D12_BLEND_DEST_COLOR;
	case api::blend_factor::one_minus_dest_color:
		return D3D12_BLEND_INV_DEST_COLOR;
	case api::blend_factor::source_alpha:
		return D3D12_BLEND_SRC_ALPHA;
	case api::blend_factor::one_minus_source_alpha:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case api::blend_factor::dest_alpha:
		return D3D12_BLEND_DEST_ALPHA;
	case api::blend_factor::one_minus_dest_alpha:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case api::blend_factor::constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::constant_color:
		return D3D12_BLEND_BLEND_FACTOR;
	case api::blend_factor::one_minus_constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::one_minus_constant_color:
		return D3D12_BLEND_INV_BLEND_FACTOR;
	case api::blend_factor::source_alpha_saturate:
		return D3D12_BLEND_SRC_ALPHA_SAT;
	case api::blend_factor::source1_color:
		return D3D12_BLEND_SRC1_COLOR;
	case api::blend_factor::one_minus_source1_color:
		return D3D12_BLEND_INV_SRC1_COLOR;
	case api::blend_factor::source1_alpha:
		return D3D12_BLEND_SRC1_ALPHA;
	case api::blend_factor::one_minus_source1_alpha:
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
		return api::blend_factor::source_color;
	case D3D12_BLEND_INV_SRC_COLOR:
		return api::blend_factor::one_minus_source_color;
	case D3D12_BLEND_DEST_COLOR:
		return api::blend_factor::dest_color;
	case D3D12_BLEND_INV_DEST_COLOR:
		return api::blend_factor::one_minus_dest_color;
	case D3D12_BLEND_SRC_ALPHA:
		return api::blend_factor::source_alpha;
	case D3D12_BLEND_INV_SRC_ALPHA:
		return api::blend_factor::one_minus_source_alpha;
	case D3D12_BLEND_DEST_ALPHA:
		return api::blend_factor::dest_alpha;
	case D3D12_BLEND_INV_DEST_ALPHA:
		return api::blend_factor::one_minus_dest_alpha;
	case D3D12_BLEND_BLEND_FACTOR:
		return api::blend_factor::constant_color;
	case D3D12_BLEND_INV_BLEND_FACTOR:
		return api::blend_factor::one_minus_constant_color;
	case D3D12_BLEND_SRC_ALPHA_SAT:
		return api::blend_factor::source_alpha_saturate;
	case D3D12_BLEND_SRC1_COLOR:
		return api::blend_factor::source1_color;
	case D3D12_BLEND_INV_SRC1_COLOR:
		return api::blend_factor::one_minus_source1_color;
	case D3D12_BLEND_SRC1_ALPHA:
		return api::blend_factor::source1_alpha;
	case D3D12_BLEND_INV_SRC1_ALPHA:
		return api::blend_factor::one_minus_source1_alpha;
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
auto reshade::d3d12::convert_primitive_topology(api::primitive_topology value) -> D3D12_PRIMITIVE_TOPOLOGY
{
	return static_cast<D3D12_PRIMITIVE_TOPOLOGY>(value);
}
auto reshade::d3d12::convert_primitive_topology(D3D12_PRIMITIVE_TOPOLOGY value) -> api::primitive_topology
{
	return static_cast<api::primitive_topology>(value);
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
	case api::query_type::stream_output_statistics_0:
	case api::query_type::stream_output_statistics_1:
	case api::query_type::stream_output_statistics_2:
	case api::query_type::stream_output_statistics_3:
		return D3D12_QUERY_HEAP_TYPE_SO_STATISTICS;
	default:
		assert(false);
		return static_cast<D3D12_QUERY_HEAP_TYPE>(UINT_MAX);
	}
}
auto reshade::d3d12::convert_query_heap_type_to_type(D3D12_QUERY_HEAP_TYPE type) -> api::query_type
{
	switch (type)
	{
	case D3D12_QUERY_HEAP_TYPE_OCCLUSION:
		return api::query_type::occlusion;
	case D3D12_QUERY_HEAP_TYPE_TIMESTAMP:
		return api::query_type::timestamp;
	case D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS:
		return api::query_type::pipeline_statistics;
	case D3D12_QUERY_HEAP_TYPE_SO_STATISTICS:
		return api::query_type::stream_output_statistics_0;
	default:
		assert(false);
		return static_cast<api::query_type>(UINT_MAX);
	}
}

auto reshade::d3d12::convert_descriptor_type(api::descriptor_type type) -> D3D12_DESCRIPTOR_RANGE_TYPE
{
	switch (type)
	{
	case api::descriptor_type::sampler:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	default:
		assert(false);
		[[fallthrough]];
	case api::descriptor_type::buffer_shader_resource_view:
	case api::descriptor_type::texture_shader_resource_view:
	case api::descriptor_type::acceleration_structure:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	case api::descriptor_type::buffer_unordered_access_view:
	case api::descriptor_type::texture_unordered_access_view:
		return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	case api::descriptor_type::constant_buffer:
		return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
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
		return api::descriptor_type::texture_shader_resource_view;
	case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
		return api::descriptor_type::texture_unordered_access_view;
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
	case api::descriptor_type::sampler:
		return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	default:
		assert(false);
		[[fallthrough]];
	case api::descriptor_type::buffer_shader_resource_view:
	case api::descriptor_type::buffer_unordered_access_view:
	case api::descriptor_type::texture_shader_resource_view:
	case api::descriptor_type::texture_unordered_access_view:
	case api::descriptor_type::constant_buffer:
	case api::descriptor_type::acceleration_structure:
		return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
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
	case D3D12_SHADER_VISIBILITY_AMPLIFICATION:
		return api::shader_stage::amplification;
	case D3D12_SHADER_VISIBILITY_MESH:
		return api::shader_stage::mesh;
	}
}

auto reshade::d3d12::convert_render_pass_load_op(api::render_pass_load_op value) -> D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case reshade::api::render_pass_load_op::load:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
	case reshade::api::render_pass_load_op::clear:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	case reshade::api::render_pass_load_op::discard:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
	case reshade::api::render_pass_load_op::no_access:
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
	}
}
auto reshade::d3d12::convert_render_pass_load_op(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE value) -> api::render_pass_load_op
{
	switch (value)
	{
	case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD:
		return reshade::api::render_pass_load_op::discard;
	default:
		assert(false);
		[[fallthrough]];
	case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE:
		return reshade::api::render_pass_load_op::load;
	case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR:
		return reshade::api::render_pass_load_op::clear;
	case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS:
		return reshade::api::render_pass_load_op::no_access;
	}
}
auto reshade::d3d12::convert_render_pass_store_op(api::render_pass_store_op value) -> D3D12_RENDER_PASS_ENDING_ACCESS_TYPE
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case reshade::api::render_pass_store_op::store:
		return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	case reshade::api::render_pass_store_op::discard:
		return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
	case reshade::api::render_pass_store_op::no_access:
		return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
	}
}
auto reshade::d3d12::convert_render_pass_store_op(D3D12_RENDER_PASS_ENDING_ACCESS_TYPE value) -> api::render_pass_store_op
{
	switch (value)
	{
	case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD:
		return reshade::api::render_pass_store_op::discard;
	default:
		assert(false);
		[[fallthrough]];
	case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE:
	case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE:
		return reshade::api::render_pass_store_op::store;
	case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS:
		return reshade::api::render_pass_store_op::no_access;
	}
}

auto reshade::d3d12::convert_fence_flags(api::fence_flags value) -> D3D12_FENCE_FLAGS
{
	D3D12_FENCE_FLAGS result = D3D12_FENCE_FLAG_NONE;
	if ((value & api::fence_flags::shared) != 0)
		result |= D3D12_FENCE_FLAG_SHARED;
	if ((value & api::fence_flags::non_monitored) != 0)
		result |= D3D12_FENCE_FLAG_NON_MONITORED;
	return result;
}

auto reshade::d3d12::convert_pipeline_flags(api::pipeline_flags value) -> D3D12_RAYTRACING_PIPELINE_FLAGS
{
	D3D12_RAYTRACING_PIPELINE_FLAGS result = D3D12_RAYTRACING_PIPELINE_FLAG_NONE;
	if ((value & api::pipeline_flags::skip_triangles) != 0)
		result |= D3D12_RAYTRACING_PIPELINE_FLAG_SKIP_TRIANGLES;
	if ((value & api::pipeline_flags::skip_aabbs) != 0)
		result |= D3D12_RAYTRACING_PIPELINE_FLAG_SKIP_PROCEDURAL_PRIMITIVES;

	return result;
}
auto reshade::d3d12::convert_pipeline_flags(D3D12_RAYTRACING_PIPELINE_FLAGS value) -> api::pipeline_flags
{
	api::pipeline_flags result = api::pipeline_flags::none;
	if ((value & D3D12_RAYTRACING_PIPELINE_FLAG_SKIP_TRIANGLES) != 0)
		result |= api::pipeline_flags::skip_triangles;
	if ((value & D3D12_RAYTRACING_PIPELINE_FLAG_SKIP_PROCEDURAL_PRIMITIVES) != 0)
		result |= api::pipeline_flags::skip_aabbs;

	return result;
}
auto reshade::d3d12::convert_acceleration_structure_type(api::acceleration_structure_type value) -> D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE
{
	return static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE>(value);
}
auto reshade::d3d12::convert_acceleration_structure_type(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE value) -> api::acceleration_structure_type
{
	return static_cast<api::acceleration_structure_type>(value);
}
auto reshade::d3d12::convert_acceleration_structure_copy_mode(api::acceleration_structure_copy_mode value) -> D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE
{
	switch (value)
	{
	case api::acceleration_structure_copy_mode::clone:
		return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_CLONE;
	case api::acceleration_structure_copy_mode::compact:
		return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT;
	case api::acceleration_structure_copy_mode::serialize:
		return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_SERIALIZE;
	case api::acceleration_structure_copy_mode::deserialize:
		return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_DESERIALIZE;
	default:
		assert(false);
		return static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE>(UINT_MAX);
	}
}
auto reshade::d3d12::convert_acceleration_structure_copy_mode(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE value) -> api::acceleration_structure_copy_mode
{
	switch (value)
	{
	case D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_CLONE:
		return api::acceleration_structure_copy_mode::clone;
	case D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT:
		return api::acceleration_structure_copy_mode::compact;
	case D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_SERIALIZE:
		return api::acceleration_structure_copy_mode::serialize;
	case D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_DESERIALIZE:
		return api::acceleration_structure_copy_mode::deserialize;
	default:
	case D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_VISUALIZATION_DECODE_FOR_TOOLS:
		assert(false);
		return static_cast<api::acceleration_structure_copy_mode>(UINT_MAX);
	}
}
auto reshade::d3d12::convert_acceleration_structure_build_flags(api::acceleration_structure_build_flags value, api::acceleration_structure_build_mode mode) -> D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS
{
	auto result = static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS>(value);

	if (mode == api::acceleration_structure_build_mode::update)
		result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

	return result;
}
auto reshade::d3d12::convert_acceleration_structure_build_flags(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS value) -> api::acceleration_structure_build_flags
{
	return static_cast<api::acceleration_structure_build_flags>(value & ~D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE);
}

void reshade::d3d12::convert_acceleration_structure_build_input(const api::acceleration_structure_build_input &build_input, D3D12_RAYTRACING_GEOMETRY_DESC &geometry)
{
	assert(build_input.type != api::acceleration_structure_build_input_type::instances);
	geometry.Type = static_cast<D3D12_RAYTRACING_GEOMETRY_TYPE>(build_input.type);

	switch (geometry.Type)
	{
	case D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES:
		geometry.Triangles.Transform3x4 = (build_input.triangles.transform_buffer.handle != 0 ? reinterpret_cast<ID3D12Resource *>(build_input.triangles.transform_buffer.handle)->GetGPUVirtualAddress() : 0) + build_input.triangles.transform_offset;
		geometry.Triangles.IndexFormat = convert_format(build_input.triangles.index_format);
		geometry.Triangles.VertexFormat = convert_format(build_input.triangles.vertex_format);
		geometry.Triangles.IndexCount = build_input.triangles.index_count;
		geometry.Triangles.VertexCount = build_input.triangles.vertex_count;
		geometry.Triangles.IndexBuffer = (build_input.triangles.index_buffer.handle != 0 ? reinterpret_cast<ID3D12Resource *>(build_input.triangles.index_buffer.handle)->GetGPUVirtualAddress() : 0) + build_input.triangles.index_offset;
		geometry.Triangles.VertexBuffer.StartAddress = (build_input.triangles.vertex_buffer.handle != 0 ? reinterpret_cast<ID3D12Resource *>(build_input.triangles.vertex_buffer.handle)->GetGPUVirtualAddress() : 0) + build_input.triangles.vertex_offset;
		geometry.Triangles.VertexBuffer.StrideInBytes = build_input.triangles.vertex_stride;
		break;
	case D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS:
		geometry.AABBs.AABBCount = build_input.aabbs.count;
		geometry.AABBs.AABBs.StartAddress = (build_input.aabbs.buffer.handle != 0 ? reinterpret_cast<ID3D12Resource *>(build_input.aabbs.buffer.handle)->GetGPUVirtualAddress() : 0) + build_input.aabbs.offset;
		geometry.AABBs.AABBs.StrideInBytes = build_input.aabbs.stride;
		break;
	}

	geometry.Flags = static_cast<D3D12_RAYTRACING_GEOMETRY_FLAGS>(build_input.flags);
}
reshade::api::acceleration_structure_build_input reshade::d3d12::convert_acceleration_structure_build_input(const D3D12_RAYTRACING_GEOMETRY_DESC &geometry)
{
	api::acceleration_structure_build_input result = {};
	result.type = static_cast<api::acceleration_structure_build_input_type>(geometry.Type);

	switch (geometry.Type)
	{
	case D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES:
		result.triangles.vertex_offset = geometry.Triangles.VertexBuffer.StartAddress;
		result.triangles.vertex_count = geometry.Triangles.VertexCount;
		result.triangles.vertex_stride = geometry.Triangles.VertexBuffer.StrideInBytes;
		result.triangles.vertex_format = convert_format(geometry.Triangles.VertexFormat);
		result.triangles.index_offset = geometry.Triangles.IndexBuffer;
		result.triangles.index_count = geometry.Triangles.IndexCount;
		result.triangles.index_format = convert_format(geometry.Triangles.IndexFormat);
		result.triangles.transform_offset = geometry.Triangles.Transform3x4;
		break;
	case D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS:
		result.aabbs.offset = geometry.AABBs.AABBs.StartAddress;
		assert(geometry.AABBs.AABBCount <= std::numeric_limits<uint32_t>::max());
		result.aabbs.count = static_cast<uint32_t>(geometry.AABBs.AABBCount);
		result.aabbs.stride = geometry.AABBs.AABBs.StrideInBytes;
		break;
	}

	result.flags = static_cast<api::acceleration_structure_build_input_flags>(geometry.Flags);

	return result;
}
