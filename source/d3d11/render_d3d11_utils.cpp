/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d11.hpp"
#include "render_d3d11_utils.hpp"

using namespace reshade::api;

const pipeline_state reshade::d3d11::pipeline_states_blend[] = {
	// D3D11_BLEND_DESC::AlphaToCoverageEnable
	pipeline_state::sample_alpha_to_coverage,
	// D3D11_BLEND_DESC::RenderTarget[0]::BlendEnable
	pipeline_state::blend,
	// D3D11_BLEND_DESC::RenderTarget[0]::SrcBlend
	pipeline_state::blend_color_src,
	// D3D11_BLEND_DESC::RenderTarget[0]::DestBlend
	pipeline_state::blend_color_dest,
	// D3D11_BLEND_DESC::RenderTarget[0]::BlendOp
	pipeline_state::blend_color_op,
	// D3D11_BLEND_DESC::RenderTarget[0]::SrcBlendAlpha
	pipeline_state::blend_alpha_src,
	// D3D11_BLEND_DESC::RenderTarget[0]::DestBlendAlpha
	pipeline_state::blend_alpha_dest,
	// D3D11_BLEND_DESC::RenderTarget[0]::BlendOpAlpha
	pipeline_state::blend_alpha_op,
	// D3D11_BLEND_DESC::RenderTarget[0]::RenderTargetWriteMask
	pipeline_state::render_target_write_mask,
	// BlendFactor
	pipeline_state::blend_factor,
	// SampleMask
	pipeline_state::sample_mask,
};
const pipeline_state reshade::d3d11::pipeline_states_rasterizer[] = {
	// D3D11_RASTERIZER_DESC::FillMode
	pipeline_state::fill_mode,
	// D3D11_RASTERIZER_DESC::CullMode
	pipeline_state::cull_mode,
	// D3D11_RASTERIZER_DESC::FrontCounterClockwise
	pipeline_state::front_face_ccw,
	// D3D11_RASTERIZER_DESC::DepthBias
	pipeline_state::depth_bias,
	// D3D11_RASTERIZER_DESC::DepthBiasClamp
	pipeline_state::depth_bias_clamp,
	// D3D11_RASTERIZER_DESC::SlopeScaledDepthBias
	pipeline_state::depth_bias_slope_scaled,
	// D3D11_RASTERIZER_DESC::DepthClipEnable
	pipeline_state::depth_clip,
	// D3D11_RASTERIZER_DESC::ScissorEnable
	pipeline_state::scissor_test,
	// D3D11_RASTERIZER_DESC::MultisampleEnable
	pipeline_state::multisample,
	// D3D11_RASTERIZER_DESC::AntialiasedLineEnable
	pipeline_state::antialiased_line,
};
const pipeline_state reshade::d3d11::pipeline_states_depth_stencil[] = {
	// D3D11_DEPTH_STENCIL_DESC::DepthEnable
	pipeline_state::depth_test,
	// D3D11_DEPTH_STENCIL_DESC::DepthWriteMask
	pipeline_state::depth_write_mask,
	// D3D11_DEPTH_STENCIL_DESC::DepthFunc
	pipeline_state::depth_func,
	// D3D11_DEPTH_STENCIL_DESC::StencilEnable
	pipeline_state::stencil_test,
	// D3D11_DEPTH_STENCIL_DESC::StencilReadMask
	pipeline_state::stencil_read_mask,
	// D3D11_DEPTH_STENCIL_DESC::StencilWriteMask
	pipeline_state::stencil_write_mask,
	// D3D11_DEPTH_STENCIL_DESC::FrontFace::StencilFailOp
	pipeline_state::stencil_front_fail,
	// D3D11_DEPTH_STENCIL_DESC::FrontFace::StencilDepthFailOp
	pipeline_state::stencil_front_depth_fail,
	// D3D11_DEPTH_STENCIL_DESC::FrontFace::StencilPassOp
	pipeline_state::stencil_front_pass,
	// D3D11_DEPTH_STENCIL_DESC::FrontFace::StencilFunc
	pipeline_state::stencil_front_func,
	// D3D11_DEPTH_STENCIL_DESC::BackFace::StencilFailOp
	pipeline_state::stencil_back_fail,
	// D3D11_DEPTH_STENCIL_DESC::BackFace::StencilDepthFailOp
	pipeline_state::stencil_back_depth_fail,
	// D3D11_DEPTH_STENCIL_DESC::BackFace::StencilPassOp
	pipeline_state::stencil_back_pass,
	// D3D11_DEPTH_STENCIL_DESC::BackFace::StencilFunc
	pipeline_state::stencil_back_func,
	// StencilRef
	pipeline_state::stencil_ref,
};

void reshade::d3d11::fill_pipeline_state_values(ID3D11BlendState *state, const FLOAT factor[4], UINT sample_mask, uint32_t (&values)[ARRAYSIZE(pipeline_states_blend)])
{
	if (state != nullptr)
	{
		D3D11_BLEND_DESC desc;
		state->GetDesc(&desc);

		values[0] = desc.AlphaToCoverageEnable;
		values[1] = desc.RenderTarget[0].BlendEnable;
		values[2] = desc.RenderTarget[0].SrcBlend;
		values[3] = desc.RenderTarget[0].DestBlend;
		values[4] = desc.RenderTarget[0].BlendOp;
		values[5] = desc.RenderTarget[0].SrcBlendAlpha;
		values[6] = desc.RenderTarget[0].DestBlendAlpha;
		values[7] = desc.RenderTarget[0].BlendOpAlpha;
		values[8] = desc.RenderTarget[0].RenderTargetWriteMask;
	}
	else
	{
		values[0] = FALSE;
		values[1] = FALSE;
		values[2] = D3D11_BLEND_ONE;
		values[3] = D3D11_BLEND_ZERO;
		values[4] = D3D11_BLEND_OP_ADD;
		values[5] = D3D11_BLEND_ONE;
		values[6] = D3D11_BLEND_ZERO;
		values[7] = D3D11_BLEND_OP_ADD;
		values[8] = D3D11_COLOR_WRITE_ENABLE_ALL;
	}

	if (factor != nullptr)
	{
		values[9] =
			((static_cast<uint32_t>(factor[0] * 255.f) & 0xFF)) |
			((static_cast<uint32_t>(factor[1] * 255.f) & 0xFF) << 8) |
			((static_cast<uint32_t>(factor[2] * 255.f) & 0xFF) << 16) |
			((static_cast<uint32_t>(factor[3] * 255.f) & 0xFF) << 24);
	}
	else
	{
		// Default blend factor is { 1, 1, 1, 1 }
		values[9] = 0xFFFFFFFF;
	}

	values[10] = sample_mask;
}
void reshade::d3d11::fill_pipeline_state_values(ID3D11RasterizerState *state, uint32_t (&values)[ARRAYSIZE(pipeline_states_rasterizer)])
{
	if (state != nullptr)
	{
		// All fields in 'D3D11_RASTERIZER_DESC' are 4 bytes, so can just load them into the value array directly
		static_assert(sizeof(D3D11_RASTERIZER_DESC) == sizeof(values));

		state->GetDesc(reinterpret_cast<D3D11_RASTERIZER_DESC *>(values));
	}
	else
	{
		values[0] = D3D11_FILL_SOLID;	// D3D11_RASTERIZER_DESC::FillMode
		values[1] = D3D11_CULL_BACK;	// D3D11_RASTERIZER_DESC::CullMode
		values[2] = FALSE;				// D3D11_RASTERIZER_DESC::FrontCounterClockwise
		values[3] = 0;					// D3D11_RASTERIZER_DESC::DepthBias
		values[4] = 0;					// D3D11_RASTERIZER_DESC::DepthBiasClamp
		values[5] = 0;					// D3D11_RASTERIZER_DESC::SlopeScaledDepthBias
		values[6] = TRUE;				// D3D11_RASTERIZER_DESC::DepthClipEnable
		values[7] = FALSE;				// D3D11_RASTERIZER_DESC::ScissorEnable
		values[8] = FALSE;				// D3D11_RASTERIZER_DESC::MultisampleEnable
		values[9] = FALSE;				// D3D11_RASTERIZER_DESC::AntialiasedLineEnable
	}
}
void reshade::d3d11::fill_pipeline_state_values(ID3D11DepthStencilState *state, UINT ref, uint32_t (&values)[ARRAYSIZE(pipeline_states_depth_stencil)])
{
	if (state != nullptr)
	{
		D3D11_DEPTH_STENCIL_DESC desc;
		state->GetDesc(&desc);

		values[0] = desc.DepthEnable;
		values[1] = desc.DepthWriteMask;
		values[2] = desc.DepthFunc;
		values[3] = desc.StencilEnable;
		values[4] = desc.StencilReadMask;
		values[5] = desc.StencilWriteMask;
		values[6] = desc.FrontFace.StencilFailOp;
		values[7] = desc.FrontFace.StencilDepthFailOp;
		values[8] = desc.FrontFace.StencilPassOp;
		values[9] = desc.FrontFace.StencilFunc;
		values[10] = desc.BackFace.StencilFailOp;
		values[11] = desc.BackFace.StencilDepthFailOp;
		values[12] = desc.BackFace.StencilPassOp;
		values[13] = desc.BackFace.StencilFunc;
	}
	else
	{
		values[0] = TRUE;
		values[1] = D3D11_DEPTH_WRITE_MASK_ALL;
		values[2] = D3D11_COMPARISON_LESS;
		values[3] = FALSE;
		values[4] = D3D11_DEFAULT_STENCIL_READ_MASK;
		values[5] = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		values[6] = values[10] = D3D11_STENCIL_OP_KEEP;
		values[7] = values[11] = D3D11_STENCIL_OP_KEEP;
		values[8] = values[12] = D3D11_STENCIL_OP_KEEP;
		values[9] = values[13] = D3D11_COMPARISON_ALWAYS;
	}

	values[14] = ref;
}

static void convert_memory_heap_to_d3d_usage(memory_heap heap, D3D11_USAGE &usage, UINT &cpu_access_flags)
{
	switch (heap)
	{
	case memory_heap::gpu_only:
		usage = D3D11_USAGE_DEFAULT;
		break;
	case memory_heap::cpu_to_gpu:
		usage = D3D11_USAGE_DYNAMIC;
		cpu_access_flags |= D3D11_CPU_ACCESS_WRITE;
		break;
	case memory_heap::gpu_to_cpu:
		usage = D3D11_USAGE_STAGING;
		cpu_access_flags |= D3D11_CPU_ACCESS_READ;
		break;
	case memory_heap::cpu_only:
		usage = D3D11_USAGE_STAGING;
		cpu_access_flags |= D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		break;
	}
}
static void convert_d3d_usage_to_memory_heap(D3D11_USAGE usage, memory_heap &heap)
{
	switch (usage)
	{
	case D3D11_USAGE_DEFAULT:
	case D3D11_USAGE_IMMUTABLE:
		heap = memory_heap::gpu_only;
		break;
	case D3D11_USAGE_DYNAMIC:
		heap = memory_heap::cpu_to_gpu;
		break;
	case D3D11_USAGE_STAGING:
		heap = memory_heap::gpu_to_cpu;
		break;
	}
}

static void convert_resource_usage_to_bind_flags(resource_usage usage, UINT &bind_flags)
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
static void convert_bind_flags_to_resource_usage(UINT bind_flags, resource_usage &usage)
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

void reshade::d3d11::convert_sampler_desc(const sampler_desc &desc, D3D11_SAMPLER_DESC &internal_desc)
{
	internal_desc.Filter = static_cast<D3D11_FILTER>(desc.filter);
	internal_desc.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.address_u);
	internal_desc.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.address_v);
	internal_desc.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.address_w);
	internal_desc.MipLODBias = desc.mip_lod_bias;
	internal_desc.MaxAnisotropy = static_cast<UINT>(desc.max_anisotropy);
	internal_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	internal_desc.BorderColor[0] = 0.0f;
	internal_desc.BorderColor[1] = 0.0f;
	internal_desc.BorderColor[2] = 0.0f;
	internal_desc.BorderColor[3] = 0.0f;
	internal_desc.MinLOD = desc.min_lod;
	internal_desc.MaxLOD = desc.max_lod;
}
sampler_desc reshade::d3d11::convert_sampler_desc(const D3D11_SAMPLER_DESC &internal_desc)
{
	sampler_desc desc = {};
	desc.filter = static_cast<texture_filter>(internal_desc.Filter);
	desc.address_u = static_cast<texture_address_mode>(internal_desc.AddressU);
	desc.address_v = static_cast<texture_address_mode>(internal_desc.AddressV);
	desc.address_w = static_cast<texture_address_mode>(internal_desc.AddressW);
	desc.mip_lod_bias = internal_desc.MipLODBias;
	desc.max_anisotropy = static_cast<float>(internal_desc.MaxAnisotropy);
	desc.min_lod = internal_desc.MinLOD;
	desc.max_lod = internal_desc.MaxLOD;
	return desc;
}

void reshade::d3d11::convert_resource_desc(const resource_desc &desc, D3D11_BUFFER_DESC &internal_desc)
{
	assert(desc.type == resource_type::buffer);
	assert(desc.size <= std::numeric_limits<UINT>::max());
	internal_desc.ByteWidth = static_cast<UINT>(desc.size);
	convert_memory_heap_to_d3d_usage(desc.heap, internal_desc.Usage, internal_desc.CPUAccessFlags);
	convert_resource_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
}
void reshade::d3d11::convert_resource_desc(const resource_desc &desc, D3D11_TEXTURE1D_DESC &internal_desc)
{
	assert(desc.type == resource_type::texture_1d);
	internal_desc.Width = desc.width;
	assert(desc.height == 1);
	internal_desc.MipLevels = desc.levels;
	internal_desc.ArraySize = desc.depth_or_layers;
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.samples == 1);
	convert_memory_heap_to_d3d_usage(desc.heap, internal_desc.Usage, internal_desc.CPUAccessFlags);
	convert_resource_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
}
void reshade::d3d11::convert_resource_desc(const resource_desc &desc, D3D11_TEXTURE2D_DESC &internal_desc)
{
	assert(desc.type == resource_type::texture_2d);
	internal_desc.Width = desc.width;
	internal_desc.Height = desc.height;
	internal_desc.MipLevels = desc.levels;
	internal_desc.ArraySize = desc.depth_or_layers;
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	internal_desc.SampleDesc.Count = desc.samples;
	convert_memory_heap_to_d3d_usage(desc.heap, internal_desc.Usage, internal_desc.CPUAccessFlags);
	convert_resource_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
}
void reshade::d3d11::convert_resource_desc(const resource_desc &desc, D3D11_TEXTURE3D_DESC &internal_desc)
{
	assert(desc.type == resource_type::texture_3d);
	internal_desc.Width = desc.width;
	internal_desc.Height = desc.height;
	internal_desc.Depth = desc.depth_or_layers;
	internal_desc.MipLevels = desc.levels;
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.samples == 1);
	convert_memory_heap_to_d3d_usage(desc.heap, internal_desc.Usage, internal_desc.CPUAccessFlags);
	convert_resource_usage_to_bind_flags(desc.usage, internal_desc.BindFlags);
}
resource_desc reshade::d3d11::convert_resource_desc(const D3D11_BUFFER_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.type = resource_type::buffer;
	desc.size = internal_desc.ByteWidth;
	convert_d3d_usage_to_memory_heap(internal_desc.Usage, desc.heap);
	convert_bind_flags_to_resource_usage(internal_desc.BindFlags, desc.usage);
	return desc;
}
resource_desc reshade::d3d11::convert_resource_desc(const D3D11_TEXTURE1D_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.type = resource_type::texture_1d;
	desc.width = internal_desc.Width;
	desc.height = 1;
	assert(internal_desc.ArraySize <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(internal_desc.ArraySize);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.format = static_cast<format>(internal_desc.Format);
	desc.samples = 1;
	convert_d3d_usage_to_memory_heap(internal_desc.Usage, desc.heap);
	convert_bind_flags_to_resource_usage(internal_desc.BindFlags, desc.usage);
	return desc;
}
resource_desc reshade::d3d11::convert_resource_desc(const D3D11_TEXTURE2D_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.type = resource_type::texture_2d;
	desc.width = internal_desc.Width;
	desc.height = internal_desc.Height;
	assert(internal_desc.ArraySize <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(internal_desc.ArraySize);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.format = static_cast<format>(internal_desc.Format);
	desc.samples = static_cast<uint16_t>(internal_desc.SampleDesc.Count);
	convert_d3d_usage_to_memory_heap(internal_desc.Usage, desc.heap);
	convert_bind_flags_to_resource_usage(internal_desc.BindFlags, desc.usage);
	desc.usage |= desc.samples > 1 ? resource_usage::resolve_source : resource_usage::resolve_dest;
	return desc;
}
resource_desc reshade::d3d11::convert_resource_desc(const D3D11_TEXTURE3D_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.type = resource_type::texture_3d;
	desc.width = internal_desc.Width;
	desc.height = internal_desc.Height;
	assert(internal_desc.Depth <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(internal_desc.Depth);
	assert(internal_desc.MipLevels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(internal_desc.MipLevels);
	desc.format = static_cast<format>(internal_desc.Format);
	desc.samples = 1;
	convert_d3d_usage_to_memory_heap(internal_desc.Usage, desc.heap);
	convert_bind_flags_to_resource_usage(internal_desc.BindFlags, desc.usage);
	return desc;
}

void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D11_DEPTH_STENCIL_VIEW_DESC::Flags
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.type != resource_view_type::buffer && desc.levels == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	assert(desc.type != resource_view_type::buffer && desc.levels == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.first_layer;
		internal_desc.Texture3D.WSize = desc.layers;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_RENDER_TARGET_VIEW_DESC1 &internal_desc)
{
	if (desc.type == resource_view_type::texture_2d || desc.type == resource_view_type::texture_2d_array)
	{
		internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
		assert(desc.type != resource_view_type::buffer && desc.levels == 1);
		switch (desc.type)
		{
		case resource_view_type::texture_2d:
			internal_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			internal_desc.Texture2D.MipSlice = desc.first_level;
			// Missing fields: D3D11_TEX2D_RTV1::PlaneSlice
			break;
		case resource_view_type::texture_2d_array:
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
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case resource_view_type::buffer:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		assert(desc.offset <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.FirstElement = static_cast<UINT>(desc.offset);
		assert(desc.size <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.NumElements = static_cast<UINT>(desc.size);
		break;
	case resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MostDetailedMip = desc.first_level;
		internal_desc.Texture1D.MipLevels = desc.levels;
		break;
	case resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MostDetailedMip = desc.first_level;
		internal_desc.Texture1DArray.MipLevels = desc.levels;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MostDetailedMip = desc.first_level;
		internal_desc.Texture2D.MipLevels = desc.levels;
		break;
	case resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MostDetailedMip = desc.first_level;
		internal_desc.Texture2DArray.MipLevels = desc.levels;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d_multisample:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
		break;
	case resource_view_type::texture_2d_multisample_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
		internal_desc.Texture2DMSArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DMSArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MostDetailedMip = desc.first_level;
		internal_desc.Texture3D.MipLevels = desc.levels;
		break;
	case resource_view_type::texture_cube:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		internal_desc.TextureCube.MostDetailedMip = desc.first_level;
		internal_desc.TextureCube.MipLevels = desc.levels;
		break;
	case resource_view_type::texture_cube_array:
		internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
		internal_desc.TextureCubeArray.MostDetailedMip = desc.first_level;
		internal_desc.TextureCubeArray.MipLevels = desc.levels;
		internal_desc.TextureCubeArray.First2DArrayFace = desc.first_layer;
		if (desc.layers == 0xFFFFFFFF)
			internal_desc.TextureCubeArray.NumCubes = 0xFFFFFFFF;
		else
			internal_desc.TextureCubeArray.NumCubes = desc.layers / 6;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_SHADER_RESOURCE_VIEW_DESC1 &internal_desc)
{
	if (desc.type == resource_view_type::texture_2d || desc.type == resource_view_type::texture_2d_array)
	{
		internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
		switch (desc.type)
		{
		case resource_view_type::texture_2d:
			internal_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			internal_desc.Texture2D.MostDetailedMip = desc.first_level;
			internal_desc.Texture2D.MipLevels = desc.levels;
			// Missing fields: D3D11_TEX2D_SRV1::PlaneSlice
			break;
		case resource_view_type::texture_2d_array:
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
	assert(desc.type == resource_view_type::buffer || desc.levels == 1);
	switch (desc.type) // Do not modifiy description in case type is 'resource_view_type::unknown'
	{
	case resource_view_type::buffer:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		assert(desc.offset <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.FirstElement = static_cast<UINT>(desc.offset);
		assert(desc.size <= std::numeric_limits<UINT>::max());
		internal_desc.Buffer.NumElements = static_cast<UINT>(desc.size);
		break;
	case resource_view_type::texture_1d:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
		internal_desc.Texture1D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_1d_array:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
		internal_desc.Texture1DArray.MipSlice = desc.first_level;
		internal_desc.Texture1DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture1DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_2d:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		internal_desc.Texture2D.MipSlice = desc.first_level;
		break;
	case resource_view_type::texture_2d_array:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		internal_desc.Texture2DArray.MipSlice = desc.first_level;
		internal_desc.Texture2DArray.FirstArraySlice = desc.first_layer;
		internal_desc.Texture2DArray.ArraySize = desc.layers;
		break;
	case resource_view_type::texture_3d:
		internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		internal_desc.Texture3D.MipSlice = desc.first_level;
		internal_desc.Texture3D.FirstWSlice = desc.first_layer;
		internal_desc.Texture3D.WSize = desc.layers;
		break;
	}
}
void reshade::d3d11::convert_resource_view_desc(const resource_view_desc &desc, D3D11_UNORDERED_ACCESS_VIEW_DESC1 &internal_desc)
{
	if (desc.type == resource_view_type::texture_2d || desc.type == resource_view_type::texture_2d_array)
	{
		internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
		assert(desc.type == resource_view_type::buffer || desc.levels == 1);
		switch (desc.type)
		{
		case resource_view_type::texture_2d:
			internal_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			internal_desc.Texture2D.MipSlice = desc.first_level;
			// Missing fields: D3D11_TEX2D_UAV1::PlaneSlice
			break;
		case resource_view_type::texture_2d_array:
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
	desc.format = static_cast<format>(internal_desc.Format);
	desc.levels = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D11_DSV_DIMENSION_TEXTURE1D:
		desc.type = resource_view_type::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
		desc.type = resource_view_type::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2D:
		desc.type = resource_view_type::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
		desc.type = resource_view_type::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2DMS:
		desc.type = resource_view_type::texture_2d_multisample;
		break;
	case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = resource_view_type::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	}
	return desc;
}
resource_view_desc reshade::d3d11::convert_resource_view_desc(const D3D11_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	resource_view_desc desc = {};
	desc.format = static_cast<format>(internal_desc.Format);
	desc.levels = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D11_RTV_DIMENSION_TEXTURE1D:
		desc.type = resource_view_type::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
		desc.type = resource_view_type::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2D:
		desc.type = resource_view_type::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
		desc.type = resource_view_type::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2DMS:
		desc.type = resource_view_type::texture_2d_multisample;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = resource_view_type::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D11_RTV_DIMENSION_TEXTURE3D:
		desc.type = resource_view_type::texture_3d;
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
		desc.format = static_cast<format>(internal_desc.Format);
		desc.levels = 1;
		switch (internal_desc.ViewDimension)
		{
		case D3D11_RTV_DIMENSION_TEXTURE2D:
			desc.type = resource_view_type::texture_2d;
			desc.first_level = internal_desc.Texture2D.MipSlice;
			// Missing fields: D3D11_TEX2D_RTV1::PlaneSlice
			break;
		case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
			desc.type = resource_view_type::texture_2d_array;
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
	desc.format = static_cast<format>(internal_desc.Format);
	switch (internal_desc.ViewDimension)
	{
	case D3D11_SRV_DIMENSION_BUFFER:
		desc.type = resource_view_type::buffer;
		desc.offset = internal_desc.Buffer.FirstElement;
		desc.size = internal_desc.Buffer.NumElements;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE1D:
		desc.type = resource_view_type::texture_1d;
		desc.first_level = internal_desc.Texture1D.MostDetailedMip;
		desc.levels = internal_desc.Texture1D.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
		desc.type = resource_view_type::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MostDetailedMip;
		desc.levels = internal_desc.Texture1DArray.MipLevels;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2D:
		desc.type = resource_view_type::texture_2d;
		desc.first_level = internal_desc.Texture2D.MostDetailedMip;
		desc.levels = internal_desc.Texture2D.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
		desc.type = resource_view_type::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MostDetailedMip;
		desc.levels = internal_desc.Texture2DArray.MipLevels;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2DMS:
		desc.type = resource_view_type::texture_2d_multisample;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
		desc.type = resource_view_type::texture_2d_multisample_array;
		desc.first_layer = internal_desc.Texture2DMSArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DMSArray.ArraySize;
		break;
	case D3D11_SRV_DIMENSION_TEXTURE3D:
		desc.type = resource_view_type::texture_3d;
		desc.first_level = internal_desc.Texture3D.MostDetailedMip;
		desc.levels = internal_desc.Texture3D.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURECUBE:
		desc.type = resource_view_type::texture_cube;
		desc.first_level = internal_desc.TextureCube.MostDetailedMip;
		desc.levels = internal_desc.TextureCube.MipLevels;
		break;
	case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
		desc.type = resource_view_type::texture_cube_array;
		desc.first_level = internal_desc.TextureCubeArray.MostDetailedMip;
		desc.levels = internal_desc.TextureCubeArray.MipLevels;
		desc.first_layer = internal_desc.TextureCubeArray.First2DArrayFace;
		if (internal_desc.TextureCubeArray.NumCubes == 0xFFFFFFFF)
			desc.layers = 0xFFFFFFFF;
		else
			desc.layers = internal_desc.TextureCubeArray.NumCubes * 6;
		break;
	case D3D11_SRV_DIMENSION_BUFFEREX:
		// Do not set type to 'resource_view_type::buffer', since that would translate to D3D11_SRV_DIMENSION_BUFFER on the conversion back
		desc.offset = internal_desc.BufferEx.FirstElement;
		desc.size = internal_desc.BufferEx.NumElements;
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
		desc.format = static_cast<format>(internal_desc.Format);
		switch (internal_desc.ViewDimension)
		{
		case D3D11_SRV_DIMENSION_TEXTURE2D:
			desc.type = resource_view_type::texture_2d;
			desc.first_level = internal_desc.Texture2D.MostDetailedMip;
			desc.levels = internal_desc.Texture2D.MipLevels;
			// Missing fields: D3D11_TEX2D_SRV1::PlaneSlice
			break;
		case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
			desc.type = resource_view_type::texture_2d_array;
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
	desc.format = static_cast<format>(internal_desc.Format);
	desc.levels = 1;
	switch (internal_desc.ViewDimension)
	{
	case D3D11_UAV_DIMENSION_BUFFER:
		desc.type = resource_view_type::buffer;
		desc.offset = internal_desc.Buffer.FirstElement;
		desc.size = internal_desc.Buffer.NumElements;
		// Missing fields: D3D11_BUFFER_UAV::Flags
		break;
	case D3D11_UAV_DIMENSION_TEXTURE1D:
		desc.type = resource_view_type::texture_1d;
		desc.first_level = internal_desc.Texture1D.MipSlice;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
		desc.type = resource_view_type::texture_1d_array;
		desc.first_level = internal_desc.Texture1DArray.MipSlice;
		desc.first_layer = internal_desc.Texture1DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture1DArray.ArraySize;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE2D:
		desc.type = resource_view_type::texture_2d;
		desc.first_level = internal_desc.Texture2D.MipSlice;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
		desc.type = resource_view_type::texture_2d_array;
		desc.first_level = internal_desc.Texture2DArray.MipSlice;
		desc.first_layer = internal_desc.Texture2DArray.FirstArraySlice;
		desc.layers = internal_desc.Texture2DArray.ArraySize;
		break;
	case D3D11_UAV_DIMENSION_TEXTURE3D:
		desc.type = resource_view_type::texture_3d;
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
		desc.format = static_cast<format>(internal_desc.Format);
		desc.levels = 1;
		switch (internal_desc.ViewDimension)
		{
		case D3D11_UAV_DIMENSION_TEXTURE2D:
			desc.type = resource_view_type::texture_2d;
			desc.first_level = internal_desc.Texture2D.MipSlice;
			// Missing fields: D3D11_TEX2D_UAV1::PlaneSlice
			break;
		case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
			desc.type = resource_view_type::texture_2d_array;
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
