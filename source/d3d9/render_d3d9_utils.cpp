/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d9.hpp"
#include "render_d3d9_utils.hpp"

using namespace reshade::api;

auto reshade::d3d9::convert_blend_op(blend_op value) -> D3DBLENDOP
{
	return static_cast<D3DBLENDOP>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d9::convert_blend_factor(blend_factor value) -> D3DBLEND
{
	switch (value)
	{
	default:
	case blend_factor::zero:
		return D3DBLEND_ZERO;
	case blend_factor::one:
		return D3DBLEND_ONE;
	case blend_factor::src_color:
		return D3DBLEND_SRCCOLOR;
	case blend_factor::inv_src_color:
		return D3DBLEND_INVSRCCOLOR;
	case blend_factor::dst_color:
		return D3DBLEND_DESTCOLOR;
	case blend_factor::inv_dst_color:
		return D3DBLEND_INVDESTCOLOR;
	case blend_factor::src_alpha:
		return D3DBLEND_SRCALPHA;
	case blend_factor::inv_src_alpha:
		return D3DBLEND_INVSRCALPHA;
	case blend_factor::dst_alpha:
		return D3DBLEND_DESTALPHA;
	case blend_factor::inv_dst_alpha:
		return D3DBLEND_INVDESTALPHA;
	case blend_factor::constant_color:
	case blend_factor::constant_alpha:
		return D3DBLEND_BLENDFACTOR;
	case blend_factor::inv_constant_color:
	case blend_factor::inv_constant_alpha:
		return D3DBLEND_INVBLENDFACTOR;
	case blend_factor::src_alpha_sat:
		return D3DBLEND_SRCALPHASAT;
	case blend_factor::src1_color:
		return D3DBLEND_SRCCOLOR2;
	case blend_factor::inv_src1_color:
		return D3DBLEND_INVSRCCOLOR2;
	case blend_factor::src1_alpha:
		assert(false);
		return D3DBLEND_SRCALPHA;
	case blend_factor::inv_src1_alpha:
		assert(false);
		return D3DBLEND_INVSRCALPHA;
	}
}
auto reshade::d3d9::convert_fill_mode(fill_mode value) -> D3DFILLMODE
{
	switch (value)
	{
	case fill_mode::point:
		return D3DFILL_POINT;
	case fill_mode::wireframe:
		return D3DFILL_WIREFRAME;
	default:
	case fill_mode::solid:
		return D3DFILL_SOLID;
	}
}
auto reshade::d3d9::convert_cull_mode(cull_mode value, bool front_counter_clockwise) -> D3DCULL
{
	if (value == api::cull_mode::none)
		return D3DCULL_NONE;
	return (value == api::cull_mode::front) == front_counter_clockwise ? D3DCULL_CCW : D3DCULL_CW;
}
auto reshade::d3d9::convert_compare_op(compare_op value) -> D3DCMPFUNC
{
	return static_cast<D3DCMPFUNC>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d9::convert_stencil_op(stencil_op value) -> D3DSTENCILOP
{
	return static_cast<D3DSTENCILOP>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d9::convert_primitive_topology(api::primitive_topology value) -> D3DPRIMITIVETYPE
{
	assert(value <= api::primitive_topology::triangle_fan);
	return static_cast<D3DPRIMITIVETYPE>(value);
}

UINT reshade::d3d9::calc_vertex_from_prim_count(D3DPRIMITIVETYPE type, UINT count)
{
	switch (type)
	{
	default:
		return 0;
	case D3DPT_LINELIST:
		return count * 2;
	case D3DPT_LINESTRIP:
		return count + 1;
	case D3DPT_TRIANGLELIST:
		return count * 3;
	case D3DPT_TRIANGLESTRIP:
	case D3DPT_TRIANGLEFAN:
		return count + 2;
	}
}
UINT reshade::d3d9::calc_prim_from_vertex_count(D3DPRIMITIVETYPE type, UINT count)
{
	switch (type)
	{
	default:
		return 0;
	case D3DPT_LINELIST:
		return count / 2;
	case D3DPT_LINESTRIP:
		return count - 1;
	case D3DPT_TRIANGLELIST:
		return count / 3;
	case D3DPT_TRIANGLESTRIP:
	case D3DPT_TRIANGLEFAN:
		return count - 2;
	}
}

void reshade::d3d9::convert_format_to_d3d_format(format format, D3DFORMAT &d3d_format, bool lockable)
{
	switch (format)
	{
	default:
	case format::unknown:
		break;
	case format::r1_unorm:
		d3d_format = D3DFMT_A1;
		break;
	case format::a8_unorm:
		d3d_format = D3DFMT_A8;
		break;
	case format::r8_typeless:
	case format::r8_uint:
	case format::r8_sint:
	case format::r8_unorm:
	case format::r8_snorm:
		// Use 4-component format so that blue component is returned as zero and alpha as one (to match behavior from other APIs)
		d3d_format = D3DFMT_X8R8G8B8;
		break;
	case format::r8g8_typeless:
	case format::r8g8_unorm:
	case format::r8g8_uint:
	case format::r8g8_snorm:
	case format::r8g8_sint:
		// Use 4-component format so that green/blue components are returned as zero and alpha as one (to match behavior from other APIs)
		d3d_format = D3DFMT_X8R8G8B8;
		break;
	case format::r8g8b8a8_typeless:
	case format::r8g8b8a8_uint:
	case format::r8g8b8a8_sint:
	case format::r8g8b8a8_unorm:
	case format::r8g8b8a8_unorm_srgb:
	case format::r8g8b8a8_snorm:
		d3d_format = D3DFMT_A8B8G8R8;
		break;
	case format::b8g8r8a8_typeless:
	case format::b8g8r8a8_unorm:
	case format::b8g8r8a8_unorm_srgb:
		d3d_format = D3DFMT_A8R8G8B8;
		break;
	case format::b8g8r8x8_typeless:
	case format::b8g8r8x8_unorm:
	case format::b8g8r8x8_unorm_srgb:
		d3d_format = D3DFMT_X8R8G8B8;
		break;
	case format::r10g10b10a2_typeless:
	case format::r10g10b10a2_uint:
	case format::r10g10b10a2_unorm:
		d3d_format = D3DFMT_A2B10G10R10;
		break;
	case format::r10g10b10a2_xr_bias:
		d3d_format = D3DFMT_A2B10G10R10_XR_BIAS;
		break;
	case format::r16_uint:
	case format::r16_sint:
	case format::r16_unorm:
	case format::r16_snorm:
		d3d_format = D3DFMT_L16;
		break;
	case format::r16_typeless:
	case format::r16_float:
		d3d_format = D3DFMT_R16F;
		break;
	case format::r16g16_uint:
	case format::r16g16_sint:
	case format::r16g16_unorm:
	case format::r16g16_snorm:
		d3d_format = D3DFMT_G16R16;
		break;
	case format::r16g16_typeless:
	case format::r16g16_float:
		d3d_format = D3DFMT_G16R16F;
		break;
	case format::r16g16b16a16_uint:
	case format::r16g16b16a16_sint:
	case format::r16g16b16a16_unorm:
	case format::r16g16b16a16_snorm:
		d3d_format = D3DFMT_A16B16G16R16;
		break;
	case format::r16g16b16a16_typeless:
	case format::r16g16b16a16_float:
		d3d_format = D3DFMT_A16B16G16R16F;
		break;
	case format::r32_uint:
	case format::r32_sint:
		// Unsupported
		break;
	case format::r32_typeless:
	case format::r32_float:
		d3d_format = D3DFMT_R32F;
		break;
	case format::r32g32_uint:
	case format::r32g32_sint:
		// Unsupported
		break;
	case format::r32g32_typeless:
	case format::r32g32_float:
		d3d_format = D3DFMT_G32R32F;
		break;
	case format::r32g32b32a32_uint:
	case format::r32g32b32a32_sint:
		// Unsupported
		break;
	case format::r32g32b32a32_typeless:
	case format::r32g32b32a32_float:
		d3d_format = D3DFMT_A32B32G32R32F;
		break;
	case format::r9g9b9e5:
	case format::r11g11b10_float:
		// Unsupported
		break;
	case format::b5g6r5_unorm:
		d3d_format = D3DFMT_R5G6B5;
		break;
	case format::b5g5r5a1_unorm:
		d3d_format = D3DFMT_A1R5G5B5;
		break;
	case format::b4g4r4a4_unorm:
		d3d_format = D3DFMT_A4R4G4B4;
		break;
	case format::s8_uint:
		d3d_format = D3DFMT_S8_LOCKABLE;
		break;
	case format::d16_unorm:
		d3d_format = lockable ? D3DFMT_D16_LOCKABLE : D3DFMT_D16;
		break;
	case format::d16_unorm_s8_uint:
		// Unsupported
		break;
	case format::r24_g8_typeless:
	case format::r24_unorm_x8_uint:
	case format::x24_unorm_g8_uint:
	case format::d24_unorm_s8_uint:
		d3d_format = D3DFMT_D24S8;
		break;
	case format::d32_float:
		d3d_format = lockable ? D3DFMT_D32F_LOCKABLE : D3DFMT_D32;
		break;
	case format::r32_g8_typeless:
	case format::r32_float_x8_uint:
	case format::x32_float_g8_uint:
	case format::d32_float_s8_uint:
		// Unsupported
		break;
	case format::bc1_typeless:
	case format::bc1_unorm:
	case format::bc1_unorm_srgb:
		d3d_format = D3DFMT_DXT1;
		break;
	case format::bc2_typeless:
	case format::bc2_unorm:
	case format::bc2_unorm_srgb:
		d3d_format = D3DFMT_DXT3;
		break;
	case format::bc3_typeless:
	case format::bc3_unorm:
	case format::bc3_unorm_srgb:
		d3d_format = D3DFMT_DXT5;
		break;
	case format::bc4_typeless:
	case format::bc4_unorm:
	case format::bc4_snorm:
		d3d_format = static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '1'));
		break;
	case format::bc5_typeless:
	case format::bc5_unorm:
	case format::bc5_snorm:
		d3d_format = static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '2'));
		break;
	case format::r8g8_b8g8_unorm:
		d3d_format = D3DFMT_G8R8_G8B8;
		break;
	case format::g8r8_g8b8_unorm:
		d3d_format = D3DFMT_R8G8_B8G8;
		break;
	case format::intz:
		d3d_format = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));
		break;
	}
}
void reshade::d3d9::convert_d3d_format_to_format(D3DFORMAT d3d_format, format &format)
{
	switch (static_cast<DWORD>(d3d_format))
	{
	default:
	case D3DFMT_UNKNOWN:
		format = format::unknown;
		break;
	case D3DFMT_A1:
		format = format::r1_unorm;
		break;
	case D3DFMT_A8:
		format = format::a8_unorm;
		break;
#if 0
	case D3DFMT_P8:
	case D3DFMT_L8:
		format = format::r8_unorm;
		break;
	case D3DFMT_A8P8:
	case D3DFMT_A8L8:
		format = format::r8g8_unorm;
		break;
#endif
	case D3DFMT_A8B8G8R8:
#if 0
	case D3DFMT_X8B8G8R8:
#endif
		format = format::r8g8b8a8_unorm;
		break;
	case D3DFMT_A8R8G8B8:
		format = format::b8g8r8a8_unorm;
		break;
	case D3DFMT_X8R8G8B8:
		format = format::b8g8r8x8_unorm;
		break;
	case D3DFMT_A2B10G10R10:
	case D3DFMT_A2R10G10B10:
		format = format::r10g10b10a2_unorm;
		break;
	case D3DFMT_A2B10G10R10_XR_BIAS:
		format = format::r10g10b10a2_xr_bias;
		break;
	case D3DFMT_L16:
		format = format::r16_uint;
		break;
	case D3DFMT_R16F:
		format = format::r16_float;
		break;
	case D3DFMT_G16R16F:
		format = format::r16g16_float;
		break;
	case D3DFMT_G16R16:
		format = format::r16g16_unorm;
		break;
	case D3DFMT_A16B16G16R16F:
		format = format::r16g16b16a16_float;
		break;
	case D3DFMT_A16B16G16R16:
		format = format::r16g16b16a16_unorm;
		break;
	case D3DFMT_R32F:
		format = format::r32_float;
		break;
	case D3DFMT_G32R32F:
		format = format::r32g32_float;
		break;
	case D3DFMT_A32B32G32R32F:
		format = format::r32g32b32a32_float;
		break;
	case D3DFMT_R5G6B5:
		format = format::b5g6r5_unorm;
		break;
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
		format = format::b5g5r5a1_unorm;
		break;
	case D3DFMT_A4R4G4B4:
		format = format::b4g4r4a4_unorm;
		break;
	case D3DFMT_S8_LOCKABLE:
		format = format::s8_uint;
		break;
	case D3DFMT_D16:
		format = format::d16_unorm;
		break;
	case D3DFMT_D24S8:
		format = format::d24_unorm_s8_uint;
		break;
	case D3DFMT_D32:
		format = format::d32_float;
		break;

	case D3DFMT_DXT1:
		format = format::bc1_unorm;
		break;
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
		format = format::bc2_unorm;
		break;
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		format = format::bc3_unorm;
		break;
	case MAKEFOURCC('A', 'T', 'I', '1'):
		format = format::bc4_unorm;
		break;
	case MAKEFOURCC('A', 'T', 'I', '2'):
		format = format::bc5_unorm;
		break;
	case D3DFMT_R8G8_B8G8:
		format = format::g8r8_g8b8_unorm;
		break;
	case D3DFMT_G8R8_G8B8:
		format = format::r8g8_b8g8_unorm;
		break;
	case D3DFMT_INDEX16:
		format = format::r16_uint;
		break;
	case D3DFMT_INDEX32:
		format = format::r32_uint;
		break;
	case MAKEFOURCC('I', 'N', 'T', 'Z'):
		format = format::intz;
		break;
	}
}

void reshade::d3d9::convert_memory_heap_to_d3d_pool(memory_heap heap, D3DPOOL &d3d_pool)
{
	// Managed resources are special and already moved to device-accessible memory as needed, so do not change pool to an explicit one for those
	if (d3d_pool == D3DPOOL_MANAGED)
		return;

	switch (heap)
	{
	case memory_heap::gpu_only:
		d3d_pool = D3DPOOL_DEFAULT;
		break;
	case memory_heap::cpu_to_gpu:
		d3d_pool = D3DPOOL_SYSTEMMEM;
		break;
	case memory_heap::cpu_only:
		d3d_pool = D3DPOOL_SCRATCH;
		break;
	}
}
void reshade::d3d9::convert_d3d_pool_to_memory_heap(D3DPOOL d3d_pool, memory_heap &heap)
{
	switch (d3d_pool)
	{
	case D3DPOOL_DEFAULT:
		heap = memory_heap::gpu_only;
		break;
	case D3DPOOL_MANAGED:
	case D3DPOOL_SYSTEMMEM:
		heap = memory_heap::cpu_to_gpu;
		break;
	case D3DPOOL_SCRATCH:
		heap = memory_heap::cpu_only;
		break;
	}
}

void reshade::d3d9::convert_resource_usage_to_d3d_usage(resource_usage usage, DWORD &d3d_usage)
{
	// Copying textures is implemented using the rasterization pipeline (see 'device_impl::copy_resource' implementation), so needs render target usage
	// When the destination in 'IDirect3DDevice9::StretchRect' is a texture surface, it too has to have render target usage (see https://docs.microsoft.com/windows/win32/api/d3d9helper/nf-d3d9helper-idirect3ddevice9-stretchrect)
	if ((usage & (resource_usage::render_target | resource_usage::copy_dest | resource_usage::resolve_dest)) != 0)
		d3d_usage |= D3DUSAGE_RENDERTARGET;
	else
		d3d_usage &= ~D3DUSAGE_RENDERTARGET;

	if ((usage & (resource_usage::depth_stencil)) != 0)
		d3d_usage |= D3DUSAGE_DEPTHSTENCIL;
	else
		d3d_usage &= ~D3DUSAGE_DEPTHSTENCIL;

	// Unordered access is not supported in D3D9
	assert((usage & resource_usage::unordered_access) == 0);
}
void reshade::d3d9::convert_d3d_usage_to_resource_usage(DWORD d3d_usage, resource_usage &usage)
{
	if (d3d_usage & D3DUSAGE_RENDERTARGET)
		usage |= resource_usage::render_target;
	if (d3d_usage & D3DUSAGE_DEPTHSTENCIL)
		usage |= resource_usage::depth_stencil;
}

void reshade::d3d9::convert_resource_desc(const resource_desc &desc, D3DVOLUME_DESC &internal_desc, UINT *levels)
{
	assert(desc.type == resource_type::texture_3d);

	internal_desc.Width = desc.texture.width;
	internal_desc.Height = desc.texture.height;
	internal_desc.Depth = desc.texture.depth_or_layers;
	convert_format_to_d3d_format(desc.texture.format, internal_desc.Format);
	assert(desc.texture.samples == 1);

	convert_memory_heap_to_d3d_pool(desc.heap, internal_desc.Pool);
	convert_resource_usage_to_d3d_usage(desc.usage, internal_desc.Usage);

	if (levels != nullptr)
		*levels = desc.texture.levels;
	else
		assert(desc.texture.levels == 1);
}
void reshade::d3d9::convert_resource_desc(const resource_desc &desc, D3DSURFACE_DESC &internal_desc, UINT *levels)
{
	assert(desc.type == resource_type::surface || desc.type == resource_type::texture_2d);

	internal_desc.Width = desc.texture.width;
	internal_desc.Height = desc.texture.height;
	assert(desc.texture.depth_or_layers == 1 || desc.texture.depth_or_layers == 6 /* D3DRTYPE_CUBETEXTURE */);
	convert_format_to_d3d_format(desc.texture.format, internal_desc.Format);

	if (desc.texture.samples > 1)
		internal_desc.MultiSampleType = static_cast<D3DMULTISAMPLE_TYPE>(desc.texture.samples);
	else
		internal_desc.MultiSampleType = D3DMULTISAMPLE_NONE;

	convert_memory_heap_to_d3d_pool(desc.heap, internal_desc.Pool);
	convert_resource_usage_to_d3d_usage(desc.usage, internal_desc.Usage);

	if (levels != nullptr)
		*levels = desc.texture.levels;
	else
		assert(desc.texture.levels == 1);
}
void reshade::d3d9::convert_resource_desc(const resource_desc &desc, D3DINDEXBUFFER_DESC &internal_desc)
{
	assert(desc.type == resource_type::buffer);

	assert(desc.buffer.size <= std::numeric_limits<UINT>::max());
	internal_desc.Size = static_cast<UINT>(desc.buffer.size);

	convert_memory_heap_to_d3d_pool(desc.heap, internal_desc.Pool);
	assert((desc.usage & (resource_usage::vertex_buffer | resource_usage::index_buffer)) == resource_usage::index_buffer);
	convert_resource_usage_to_d3d_usage(desc.usage, internal_desc.Usage);
}
void reshade::d3d9::convert_resource_desc(const resource_desc &desc, D3DVERTEXBUFFER_DESC &internal_desc)
{
	assert(desc.type == resource_type::buffer);

	assert(desc.buffer.size <= std::numeric_limits<UINT>::max());
	internal_desc.Size = static_cast<UINT>(desc.buffer.size);

	convert_memory_heap_to_d3d_pool(desc.heap, internal_desc.Pool);
	assert((desc.usage & (resource_usage::vertex_buffer | resource_usage::index_buffer)) == resource_usage::vertex_buffer);
	convert_resource_usage_to_d3d_usage(desc.usage, internal_desc.Usage);
}
resource_desc reshade::d3d9::convert_resource_desc(const D3DVOLUME_DESC &internal_desc, UINT levels)
{
	assert(internal_desc.Type == D3DRTYPE_VOLUME || internal_desc.Type == D3DRTYPE_VOLUMETEXTURE);

	resource_desc desc = {};
	desc.type = resource_type::texture_3d;
	desc.texture.width = internal_desc.Width;
	desc.texture.height = internal_desc.Height;
	assert(internal_desc.Depth <= std::numeric_limits<uint16_t>::max());
	desc.texture.depth_or_layers = static_cast<uint16_t>(internal_desc.Depth);
	assert(levels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(levels);
	convert_d3d_format_to_format(internal_desc.Format, desc.texture.format);
	desc.texture.samples = 1;

	convert_d3d_pool_to_memory_heap(internal_desc.Pool, desc.heap);
	convert_d3d_usage_to_resource_usage(internal_desc.Usage, desc.usage);
	if (internal_desc.Type == D3DRTYPE_VOLUMETEXTURE)
		desc.usage |= resource_usage::shader_resource;

	return desc;
}
resource_desc reshade::d3d9::convert_resource_desc(const D3DSURFACE_DESC &internal_desc, UINT levels, const D3DCAPS9 &caps)
{
	assert(internal_desc.Type == D3DRTYPE_SURFACE || internal_desc.Type == D3DRTYPE_TEXTURE || internal_desc.Type == D3DRTYPE_CUBETEXTURE);

	resource_desc desc = {};
	desc.type = (internal_desc.Type == D3DRTYPE_SURFACE) ? resource_type::surface : resource_type::texture_2d;
	desc.texture.width = internal_desc.Width;
	desc.texture.height = internal_desc.Height;
	desc.texture.depth_or_layers = internal_desc.Type == D3DRTYPE_CUBETEXTURE ? 6 : 1;
	assert(levels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(levels);
	convert_d3d_format_to_format(internal_desc.Format, desc.texture.format);

	if (internal_desc.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES)
		desc.texture.samples = static_cast<uint16_t>(internal_desc.MultiSampleType);
	else
		desc.texture.samples = 1;

	convert_d3d_pool_to_memory_heap(internal_desc.Pool, desc.heap);
	convert_d3d_usage_to_resource_usage(internal_desc.Usage, desc.usage);
	if (internal_desc.Type == D3DRTYPE_TEXTURE || internal_desc.Type == D3DRTYPE_CUBETEXTURE)
		desc.usage |= resource_usage::shader_resource;

	// Copying is restricted by limitations of 'IDirect3DDevice9::StretchRect' (see https://docs.microsoft.com/windows/win32/api/d3d9helper/nf-d3d9helper-idirect3ddevice9-stretchrect)
	// or performing copy between two textures using rasterization pipeline (see 'device_impl::copy_resource' implementation)
	if (internal_desc.Pool == D3DPOOL_DEFAULT && (internal_desc.Type == D3DRTYPE_SURFACE || (internal_desc.Type == D3DRTYPE_TEXTURE && (caps.Caps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES) != 0)))
	{
		switch (static_cast<DWORD>(internal_desc.Format))
		{
		default:
			desc.usage |= resource_usage::copy_source;
			if (internal_desc.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES)
				desc.usage |= resource_usage::resolve_source;
			if (internal_desc.Usage & D3DUSAGE_RENDERTARGET)
				desc.usage |= resource_usage::copy_dest | resource_usage::resolve_dest;
			break;
		case D3DFMT_DXT1:
		case D3DFMT_DXT2:
		case D3DFMT_DXT3:
		case D3DFMT_DXT4:
		case D3DFMT_DXT5:
			// Stretching is not supported if either surface is in a compressed format
			break;
		case D3DFMT_D16_LOCKABLE:
		case D3DFMT_D32:
		case D3DFMT_D15S1:
		case D3DFMT_D24S8:
		case D3DFMT_D24X8:
		case D3DFMT_D24X4S4:
		case D3DFMT_D16:
		case D3DFMT_D32F_LOCKABLE:
		case D3DFMT_D24FS8:
		case D3DFMT_D32_LOCKABLE:
		case D3DFMT_S8_LOCKABLE:
			// Stretching depth stencil surfaces is extremly limited (does not support copying from surface to texture for example), so just do not allow it
			assert(internal_desc.Usage & D3DUSAGE_DEPTHSTENCIL);
			break;
		case MAKEFOURCC('N', 'U', 'L', 'L'):
			// Special render target format that has no memory attached, so cannot be copied
			break;
		}
	}

	return desc;
}
resource_desc reshade::d3d9::convert_resource_desc(const D3DINDEXBUFFER_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.type = resource_type::buffer;
	desc.buffer.size = internal_desc.Size;
	convert_d3d_pool_to_memory_heap(internal_desc.Pool, desc.heap);
	convert_d3d_usage_to_resource_usage(internal_desc.Usage, desc.usage);
	desc.usage |= resource_usage::index_buffer;
	return desc;
}
resource_desc reshade::d3d9::convert_resource_desc(const D3DVERTEXBUFFER_DESC &internal_desc)
{
	resource_desc desc = {};
	desc.type = resource_type::buffer;
	desc.buffer.size = internal_desc.Size;
	convert_d3d_pool_to_memory_heap(internal_desc.Pool, desc.heap);
	convert_d3d_usage_to_resource_usage(internal_desc.Usage, desc.usage);
	desc.usage |= resource_usage::vertex_buffer;
	return desc;
}
