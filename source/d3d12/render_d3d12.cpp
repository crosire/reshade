/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_d3d12.hpp"
#include "dll_resources.hpp"

using namespace reshade::api;

static std::mutex s_global_mutex;

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

	internal_desc.Width = desc.width;
	internal_desc.Height = desc.height;
	internal_desc.DepthOrArraySize = desc.depth_or_layers;
	internal_desc.MipLevels = desc.levels;
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	internal_desc.SampleDesc.Count = desc.samples;

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
	assert(internal_desc.Width <= std::numeric_limits<uint32_t>::max());
	desc.width = static_cast<uint32_t>(internal_desc.Width);
	desc.height = internal_desc.Height;
	desc.levels = internal_desc.MipLevels;
	desc.depth_or_layers = internal_desc.DepthOrArraySize;
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	desc.samples = static_cast<uint16_t>(internal_desc.SampleDesc.Count);

	// Resources are generally copyable in D3D12
	desc.usage = resource_usage::copy_dest | resource_usage::copy_source;

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

void reshade::d3d12::convert_depth_stencil_view_desc(const resource_view_desc &desc, D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_DEPTH_STENCIL_VIEW_DESC::Flags
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
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
resource_view_desc reshade::d3d12::convert_depth_stencil_view_desc(const D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc)
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

void reshade::d3d12::convert_render_target_view_desc(const resource_view_desc &desc, D3D12_RENDER_TARGET_VIEW_DESC &internal_desc)
{
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
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
resource_view_desc reshade::d3d12::convert_render_target_view_desc(const D3D12_RENDER_TARGET_VIEW_DESC &internal_desc)
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

void reshade::d3d12::convert_shader_resource_view_desc(const resource_view_desc &desc, D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_SHADER_RESOURCE_VIEW_DESC::Shader4ComponentMapping
	internal_desc.Format = static_cast<DXGI_FORMAT>(desc.format);
	switch (desc.dimension) // Do not modifiy description in case dimension is 'resource_view_dimension::unknown'
	{
	case resource_view_dimension::buffer:
		internal_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		internal_desc.Buffer.FirstElement = desc.byte_offset;
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
resource_view_desc reshade::d3d12::convert_shader_resource_view_desc(const D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc)
{
	// Missing fields: D3D12_SHADER_RESOURCE_VIEW_DESC::Shader4ComponentMapping
	resource_view_desc desc = {};
	desc.format = static_cast<uint32_t>(internal_desc.Format);
	switch (internal_desc.ViewDimension)
	{
	case D3D12_SRV_DIMENSION_BUFFER:
		desc.dimension = resource_view_dimension::buffer;
		desc.byte_offset = internal_desc.Buffer.FirstElement;
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
		desc.byte_offset = internal_desc.RaytracingAccelerationStructure.Location;
		break;
	}
	return desc;
}

reshade::d3d12::device_impl::device_impl(ID3D12Device *device) :
	_device(device)
{
	srv_handle_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	rtv_handle_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	dsv_handle_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	sampler_handle_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	// Create mipmap generation states
	{   D3D12_DESCRIPTOR_RANGE srv_range = {};
		srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srv_range.NumDescriptors = 1;
		srv_range.BaseShaderRegister = 0; // t0
		D3D12_DESCRIPTOR_RANGE uav_range = {};
		uav_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uav_range.NumDescriptors = 1;
		uav_range.BaseShaderRegister = 0; // u0

		D3D12_ROOT_PARAMETER params[3] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[0].Constants.ShaderRegister = 0; // b0
		params[0].Constants.Num32BitValues = 2;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &srv_range;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[2].DescriptorTable.NumDescriptorRanges = 1;
		params[2].DescriptorTable.pDescriptorRanges = &uav_range;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_STATIC_SAMPLER_DESC samplers[1] = {};
		samplers[0].Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplers[0].ShaderRegister = 0; // s0
		samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = ARRAYSIZE(params);
		desc.pParameters = params;
		desc.NumStaticSamplers = ARRAYSIZE(samplers);
		desc.pStaticSamplers = samplers;

		_mipmap_signature = create_root_signature(desc);
		assert(_mipmap_signature != nullptr);
	}

	{   D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
		pso_desc.pRootSignature = _mipmap_signature.get();

		const resources::data_resource cs = resources::load_data_resource(IDR_MIPMAP_CS);
		pso_desc.CS = { cs.data, cs.data_size };

		if (FAILED(_device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&_mipmap_pipeline))))
			return;
	}

#if RESHADE_ADDON
	// Load and initialize add-ons
	reshade::addon::load_addons();
#endif

	RESHADE_ADDON_EVENT(init_device, this);
}
reshade::d3d12::device_impl::~device_impl()
{
	RESHADE_ADDON_EVENT(destroy_device, this);

#if RESHADE_ADDON
	reshade::addon::unload_addons();
#endif
}

bool reshade::d3d12::device_impl::check_format_support(uint32_t format, resource_usage usage)
{
	D3D12_FEATURE_DATA_FORMAT_SUPPORT feature = { static_cast<DXGI_FORMAT>(format) };
	if (FAILED(_device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &feature, sizeof(feature))))
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

bool reshade::d3d12::device_impl::is_resource_valid(resource_handle resource)
{
	return resource.handle != 0 && _resources.has_object(reinterpret_cast<ID3D12Resource *>(resource.handle));
}
bool reshade::d3d12::device_impl::is_resource_view_valid(resource_view_handle view)
{
	if (view.handle & 1)
	{
		return is_resource_valid({ view.handle ^ 1 });
	}
	else
	{
		const std::lock_guard<std::mutex> lock(s_global_mutex);

		return _views.find(view.handle) != _views.end();
	}
}

bool reshade::d3d12::device_impl::create_resource(resource_type type, const resource_desc &desc, resource_handle *out_resource)
{
	D3D12_RESOURCE_DESC internal_desc = {};
	convert_resource_desc(type, desc, internal_desc);

	D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_DEFAULT };

	if (com_ptr<ID3D12Resource> resource;
		SUCCEEDED(_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &internal_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource))))
	{
		register_resource(resource.get());
		*out_resource = { reinterpret_cast<uintptr_t>(resource.release()) };
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

	if (type == resource_view_type::shader_resource)
	{
		// Resource pointers should be at least 4-byte aligned, so that this is safe
		assert((resource.handle & 1) == 0);

		reinterpret_cast<ID3D12Resource *>(resource.handle)->AddRef();
		*out_view = { resource.handle | 1 };
		return true;
	}
	else
	{
		assert(false);
		*out_view = { 0 };
		return false; // TODO
	}
}

void reshade::d3d12::device_impl::destroy_resource(resource_handle resource)
{
	assert(resource.handle != 0);
	reinterpret_cast<ID3D12Resource *>(resource.handle)->Release();
}
void reshade::d3d12::device_impl::destroy_resource_view(resource_view_handle view)
{
	assert(view.handle != 0);

	if (view.handle & 1)
	{
		reinterpret_cast<ID3D12Resource *>(view.handle ^ 1)->Release();
	}
}

void reshade::d3d12::device_impl::get_resource_from_view(resource_view_handle view, resource_handle *out_resource)
{
	assert(view.handle != 0);

	if (view.handle & 1)
	{
		*out_resource = { view.handle ^ 1 };
	}
	else
	{
		const std::lock_guard<std::mutex> lock(s_global_mutex);

		*out_resource = { reinterpret_cast<uintptr_t>(_views[view.handle]) };
	}
}

resource_desc reshade::d3d12::device_impl::get_resource_desc(resource_handle resource)
{
	assert(resource.handle != 0);
	return convert_resource_desc(reinterpret_cast<ID3D12Resource *>(resource.handle)->GetDesc()).second;
}

void reshade::d3d12::device_impl::wait_idle()
{
	com_ptr<ID3D12Fence> fence;
	if (FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
		return;

	const HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr)
		return;

	const std::lock_guard<std::mutex> lock(s_global_mutex);

	UINT64 signal_value = 1;
	for (const com_ptr<ID3D12CommandQueue> &queue : _queues)
	{
		queue->Signal(fence.get(), signal_value);
		fence->SetEventOnCompletion(signal_value, fence_event);
		WaitForSingleObject(fence_event, INFINITE);
		signal_value++;
	}

	CloseHandle(fence_event);
}

void reshade::d3d12::device_impl::register_queue(ID3D12CommandQueue *queue)
{
	const std::lock_guard<std::mutex> lock(s_global_mutex);

	_queues.push_back(queue);
}
void reshade::d3d12::device_impl::register_resource_view(ID3D12Resource *resource, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	assert(resource != nullptr);

	const std::lock_guard<std::mutex> lock(s_global_mutex);

	_views.emplace(handle.ptr, resource);
}

com_ptr<ID3D12RootSignature> reshade::d3d12::device_impl::create_root_signature(const D3D12_ROOT_SIGNATURE_DESC &desc) const
{
	com_ptr<ID3DBlob> signature_blob;
	com_ptr<ID3D12RootSignature> signature;
	if (SUCCEEDED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, nullptr)))
		_device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(), IID_PPV_ARGS(&signature));
	return signature;
}

reshade::d3d12::command_list_impl::command_list_impl(device_impl *device, ID3D12GraphicsCommandList *cmd_list) :
	_device_impl(device), _cmd_list(cmd_list), _has_commands(cmd_list != nullptr)
{
	if (_has_commands)
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

void reshade::d3d12::command_list_impl::transition_state(resource_handle resource, resource_usage old_layout, resource_usage new_layout)
{
	_has_commands = true;

	assert(resource.handle != 0);

	// Depth write state is mutually exclusive with other states
	if ((old_layout & resource_usage::depth_stencil) == resource_usage::depth_stencil)
		old_layout  = resource_usage::depth_stencil_write;
	if ((new_layout & resource_usage::depth_stencil) == resource_usage::depth_stencil)
		new_layout  = resource_usage::depth_stencil_write;

	D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transition.Transition.pResource = reinterpret_cast<ID3D12Resource *>(resource.handle);
	transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transition.Transition.StateBefore = static_cast<D3D12_RESOURCE_STATES>(old_layout);
	transition.Transition.StateAfter = static_cast<D3D12_RESOURCE_STATES>(new_layout);

	_cmd_list->ResourceBarrier(1, &transition);
}

void reshade::d3d12::command_list_impl::clear_depth_stencil_view(resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	_has_commands = true;

	assert(dsv.handle != 0 && (dsv.handle & 1) == 0);
	_cmd_list->ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(dsv.handle) }, static_cast<D3D12_CLEAR_FLAGS>(clear_flags), depth, stencil, 0, nullptr);
}
void reshade::d3d12::command_list_impl::clear_render_target_view(resource_view_handle rtv, const float color[4])
{
	_has_commands = true;

	assert(rtv.handle != 0 && (rtv.handle & 1) == 0);
	_cmd_list->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(rtv.handle) }, color, 0, nullptr);
}

void reshade::d3d12::command_list_impl::copy_resource(resource_handle source, resource_handle dest)
{
	_has_commands = true;

	assert(source.handle != 0 && dest.handle != 0);
	_cmd_list->CopyResource(reinterpret_cast<ID3D12Resource *>(dest.handle), reinterpret_cast<ID3D12Resource *>(source.handle));
}

reshade::d3d12::command_list_immediate_impl::command_list_immediate_impl(device_impl *device) :
	command_list_impl(device, nullptr)
{
	// Create multiple command allocators to buffer for multiple frames
	for (UINT i = 0; i < NUM_COMMAND_FRAMES; ++i)
	{
		if (FAILED(_device_impl->_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence[i]))))
			return;
		if (FAILED(_device_impl->_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmd_alloc[i]))))
			return;
	}

	// Create and open the command list for recording
	if (SUCCEEDED(_device_impl->_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmd_alloc[_cmd_index].get(), nullptr, IID_PPV_ARGS(&_cmd_list))))
	{
		_cmd_list->SetName(L"ReShade command list");
	}

	// Create auto-reset event and fences for synchronization
	_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}
reshade::d3d12::command_list_immediate_impl::~command_list_immediate_impl()
{
	if (_fence_event != nullptr)
		CloseHandle(_fence_event);

	// Signal to 'command_list_impl' destructor that this is an immediate command list
	_has_commands = false;
}

bool reshade::d3d12::command_list_immediate_impl::flush(ID3D12CommandQueue *queue)
{
	if (!_has_commands)
		return true;

	if (FAILED(_cmd_list->Close()))
		return false;

	ID3D12CommandList *const cmd_lists[] = { _cmd_list.get() };
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
	return SUCCEEDED(_cmd_list->Reset(_cmd_alloc[_cmd_index].get(), nullptr));
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
	_device_impl(device), _queue(queue)
{
	// Only create an immediate command list for graphics queues (since the implemented commands do not work on other queue types)
	if (queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		_immediate_cmd_list = new command_list_immediate_impl(device);
	}

	RESHADE_ADDON_EVENT(init_command_queue, this);
}
reshade::d3d12::command_queue_impl::~command_queue_impl()
{
	RESHADE_ADDON_EVENT(destroy_command_queue, this);

	delete _immediate_cmd_list;
}
