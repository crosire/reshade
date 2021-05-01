/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_api_format.hpp"

namespace reshade { namespace api
{
	/// <summary>
	/// The available resource types. The type of a resource is specified during creation and is immutable.
	/// Various operations may have special requirements on the type of resources they operate on (e.g. copies can only happen between resources of the same type, ...).
	/// </summary>
	enum class resource_type : uint32_t
	{
		unknown,
		buffer,
		texture_1d,
		texture_2d,
		texture_3d,
		surface // Special type for resources that are implicitly both resource and render target view. These cannot be created via the API, but may be referenced by the application.
	};

	/// <summary>
	/// The available resource view types, which identify how a view interprets the data of its resource.
	/// </summary>
	enum class resource_view_type : uint32_t
	{
		unknown,
		buffer,
		texture_1d,
		texture_1d_array,
		texture_2d,
		texture_2d_array,
		texture_2d_multisample,
		texture_2d_multisample_array,
		texture_3d,
		texture_cube,
		texture_cube_array
	};

	/// <summary>
	/// The available memory heap types, which give a hint as to where to place the memory allocation for a resource.
	/// </summary>
	enum class memory_heap : uint32_t
	{
		unknown,
		gpu_only,
		cpu_to_gpu,
		gpu_to_cpu,
		cpu_only
	};

	/// <summary>
	/// A list of flags that specify how a resource is to be used.
	/// This needs to be specified during creation and is also used to transition between different usages within a command stream.
	/// </summary>
	enum class resource_usage : uint32_t
	{
		undefined = 0,
		depth_stencil = 0x30,
		depth_stencil_read = 0x20,
		depth_stencil_write = 0x10,
		render_target = 0x4,
		shader_resource = 0xC0,
		shader_resource_pixel = 0x80,
		shader_resource_non_pixel = 0x40,
		unordered_access = 0x8,
		copy_dest = 0x400,
		copy_source = 0x800,
		resolve_dest = 0x1000,
		resolve_source = 0x2000,
		index_buffer = 0x2,
		vertex_buffer = 0x1,
		constant_buffer = 0x8000,
	};

	constexpr bool operator!=(resource_usage lhs, uint32_t rhs) { return static_cast<uint32_t>(lhs) != rhs; }
	constexpr bool operator!=(uint32_t lhs, resource_usage rhs) { return lhs != static_cast<uint32_t>(rhs); }
	constexpr bool operator==(resource_usage lhs, uint32_t rhs) { return static_cast<uint32_t>(lhs) == rhs; }
	constexpr bool operator==(uint32_t lhs, resource_usage rhs) { return lhs == static_cast<uint32_t>(rhs); }
	constexpr resource_usage operator^(resource_usage lhs, resource_usage rhs) { return static_cast<resource_usage>(static_cast<uint32_t>(lhs) ^ static_cast<uint32_t>(rhs)); }
	constexpr resource_usage operator&(resource_usage lhs, resource_usage rhs) { return static_cast<resource_usage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)); }
	constexpr resource_usage operator|(resource_usage lhs, resource_usage rhs) { return static_cast<resource_usage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)); }
	constexpr resource_usage &operator^=(resource_usage &lhs, resource_usage rhs) { return lhs = static_cast<resource_usage>(static_cast<uint32_t>(lhs) ^ static_cast<uint32_t>(rhs)); }
	constexpr resource_usage &operator&=(resource_usage &lhs, resource_usage rhs) { return lhs = static_cast<resource_usage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)); }
	constexpr resource_usage &operator|=(resource_usage &lhs, resource_usage rhs) { return lhs = static_cast<resource_usage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)); }

	/// <summary>
	/// A filtering type used for texture lookups.
	/// </summary>
	enum class texture_filter
	{
		min_mag_mip_point = 0,
		min_mag_point_mip_linear = 0x1,
		min_point_mag_linear_mip_point = 0x4,
		min_point_mag_mip_linear = 0x5,
		min_linear_mag_mip_point = 0x10,
		min_linear_mag_point_mip_linear = 0x11,
		min_mag_linear_mip_point = 0x14,
		min_mag_mip_linear = 0x15,
		anisotropic = 0x55
	};

	/// <summary>
	/// Specifies behavior of sampling with texture coordinates outside an image.
	/// </summary>
	enum class texture_address_mode
	{
		wrap = 1,
		mirror = 2,
		clamp = 3,
		border = 4,
		mirror_once = 5
	};

	/// <summary>
	/// Describes a sampler state.
	/// </summary>
	struct sampler_desc
	{
		texture_filter filter;
		texture_address_mode address_u;
		texture_address_mode address_v;
		texture_address_mode address_w;
		float mip_lod_bias;
		float max_anisotropy;
		float min_lod;
		float max_lod;
	};

	/// <summary>
	/// Describes a resource, such as a buffer or texture.
	/// </summary>
	struct resource_desc
	{
		resource_desc() :
			type(resource_type::unknown), texture(), heap(memory_heap::unknown), usage(resource_usage::undefined) {}
		resource_desc(uint64_t size, memory_heap heap, resource_usage usage) :
			type(resource_type::buffer), buffer({ size }), heap(heap), usage(usage) {}
		resource_desc(uint32_t width, uint32_t height, uint16_t layers, uint16_t levels, format format, uint16_t samples, memory_heap heap, resource_usage usage) :
			type(resource_type::texture_2d), texture({ width, height, layers, levels, format, samples }), heap(heap), usage(usage) {}
		resource_desc(resource_type type, uint32_t width, uint32_t height, uint16_t depth_or_layers, uint16_t levels, format format, uint16_t samples, memory_heap heap, resource_usage usage) :
			type(type), texture({ width, height, depth_or_layers, levels, format, samples }), heap(heap), usage(usage) {}

		// Type of the resource.
		resource_type type;

		union
		{
			// Used when resource type is a buffer.
			struct
			{
				// Size of the buffer (in bytes).
				uint64_t size;
			} buffer;
			// Used when resource type is a surface or texture.
			struct
			{
				// Width of the texture (in texels).
				uint32_t width;
				// If this is a 2D or 3D texture, height of the texture (in texels), otherwise 1.
				uint32_t height;
				// If this is a 3D texture, depth of the texture (in texels), otherwise number of array layers.
				uint16_t depth_or_layers;
				// Maximum number of mipmap levels in the texture, including the base level, so at least 1.
				uint16_t levels;
				// Data format of each texel in the texture.
				format   format;
				// The number of samples per texel. Set to a value higher than 1 for multisampling.
				uint16_t samples;
			} texture;
		};

		// The heap the resource allocation is placed in.
		memory_heap heap;
		// Flags that specify how this resource may be used.
		resource_usage usage;
	};

	/// <summary>
	/// Describes a resource view, which specifies how to interpret the data of a resource.
	/// </summary>
	struct resource_view_desc
	{
		resource_view_desc() :
			type(resource_view_type::unknown), format(format::unknown), texture() {}
		resource_view_desc(format format, uint64_t offset, uint64_t size) :
			type(resource_view_type::buffer), format(format), buffer({ offset, size }) {}
		resource_view_desc(format format, uint32_t first_level, uint32_t levels, uint32_t first_layer, uint32_t layers) :
			type(resource_view_type::texture_2d), format(format), texture({ first_level, levels, first_layer, layers }) {}
		resource_view_desc(resource_view_type type, format format, uint32_t first_level, uint32_t levels, uint32_t first_layer, uint32_t layers) :
			type(type), format(format), texture({ first_level, levels, first_layer, layers }) {}

		// Type of the view. Identifies how the view should interpret the resource data.
		resource_view_type type;
		// Viewing format of this view. The data of the resource is reinterpreted in this format.
		format format;

		union
		{
			// Used when view type is a buffer.
			struct
			{
				// Offset from the start of the buffer resource (in bytes).
				uint64_t offset;
				// Number of elements this view covers in the buffer resource (in bytes).
				uint64_t size;
			} buffer;
			// Used when view type is a texture.
			struct
			{
				// Index of the most detailed mipmap level to use. This number has to be between zero and the maximum number of mipmap levels in the texture minus 1.
				uint32_t first_level;
				// Maximum number of mipmap levels for the view of the texture.
				// Set to -1 (0xFFFFFFFF) to indicate that all mipmap levels down to the least detailed should be used.
				uint32_t levels;
				// Index of the first array layer of the texture array to use. This value is ignored if the texture is not layered.
				uint32_t first_layer;
				// Maximum number of array layers for the view of the texture array. This value is ignored if the texture is not layered.
				// Set to -1 (0xFFFFFFFF) to indicate that all array layers should be used.
				uint32_t layers;
			} texture;
		};
	};

	/// <summary>
	/// Used to specify data for initializing a subresource or access existing subresource data.
	/// </summary>
	struct subresource_data
	{
		// Pointer to the data.
		const void *data;
		// The row pitch of the data (added to the data pointer to move between texture rows, unused for buffers and 1D textures).
		uint32_t row_pitch;
		// The depth pitch of the data (added to the data pointer to move between texture depth/array slices, unused for buffers and 1D/2D textures).
		uint32_t depth_pitch;
	};

	/// <summary>
	/// An opaque handle to a sampler state object.
	/// <para>Depending on the render API this is really a pointer to a 'ID3D10SamplerState', 'ID3D11SamplerState' or a 'D3D12_CPU_DESCRIPTOR_HANDLE' or 'VkSampler' handle.</para>
	/// </summary>
	typedef struct { uint64_t handle; } sampler;

	/// <summary>
	/// An opaque handle to a resource object (buffer, texture, ...).
	/// <para>Resources created by the application are only guaranteed to be valid during event callbacks.
	/// If you want to use one outside that scope, first ensure the resource is still valid via <see cref="device::check_resource_handle_valid"/>.</para>
	/// <para>Depending on the render API this is really a pointer to a 'IDirect3DResource9', 'ID3D10Resource', 'ID3D11Resource' or 'ID3D12Resource' object or a 'VkImage' handle.</para>
	/// </summary>
	typedef struct { uint64_t handle; } resource;

	constexpr bool operator< (resource lhs, resource rhs) { return lhs.handle < rhs.handle; }
	constexpr bool operator!=(resource lhs, uint64_t rhs) { return lhs.handle != rhs; }
	constexpr bool operator!=(uint64_t lhs, resource rhs) { return lhs != rhs.handle; }
	constexpr bool operator!=(resource lhs, resource rhs) { return lhs.handle != rhs.handle; }
	constexpr bool operator==(resource lhs, uint64_t rhs) { return lhs.handle == rhs; }
	constexpr bool operator==(uint64_t lhs, resource rhs) { return lhs == rhs.handle; }
	constexpr bool operator==(resource lhs, resource rhs) { return lhs.handle == rhs.handle; }

	/// <summary>
	/// An opaque handle to a resource view object (depth-stencil, render target, shader resource view, ...).
	/// <para>Resource views created by the application are only guaranteed to be valid during event callbacks.
	/// If you want to use one outside that scope, first ensure the resource view is still valid via <see cref="device::check_resource_view_handle_valid"/>.</para>
	/// <para>Depending on the render API this is really a pointer to a 'IDirect3DResource9', 'ID3D10View' or 'ID3D11View' object, or a 'D3D12_CPU_DESCRIPTOR_HANDLE' or 'VkImageView' handle.</para>
	/// </summary>
	typedef struct { uint64_t handle; } resource_view;

	constexpr bool operator< (resource_view lhs, resource_view rhs) { return lhs.handle < rhs.handle; }
	constexpr bool operator!=(resource_view lhs, uint64_t rhs) { return lhs.handle != rhs; }
	constexpr bool operator!=(uint64_t lhs, resource_view rhs) { return lhs != rhs.handle; }
	constexpr bool operator!=(resource_view lhs, resource_view rhs) { return lhs.handle != rhs.handle; }
	constexpr bool operator==(resource_view lhs, uint64_t rhs) { return lhs.handle == rhs; }
	constexpr bool operator==(uint64_t lhs, resource_view rhs) { return lhs == rhs.handle; }
	constexpr bool operator==(resource_view lhs, resource_view rhs) { return lhs.handle == rhs.handle; }
} }
