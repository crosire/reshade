/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_api_format.hpp"

#ifndef RESHADE_DEFINE_HANDLE
	#define RESHADE_DEFINE_HANDLE(name) \
		typedef struct { uint64_t handle; } name; \
		constexpr bool operator< (name lhs, name rhs) { return lhs.handle < rhs.handle; } \
		constexpr bool operator!=(name lhs, name rhs) { return lhs.handle != rhs.handle; } \
		constexpr bool operator!=(name lhs, uint64_t rhs) { return lhs.handle != rhs; } \
		constexpr bool operator==(name lhs, name rhs) { return lhs.handle == rhs.handle; } \
		constexpr bool operator==(name lhs, uint64_t rhs) { return lhs.handle == rhs; }
#endif

#ifndef RESHADE_DEFINE_ENUM_FLAG_OPERATORS
	#define RESHADE_DEFINE_ENUM_FLAG_OPERATORS(type) \
		constexpr type operator~(type a) { return static_cast<type>(~static_cast<uint32_t>(a)); } \
		inline type &operator&=(type &a, type b) { return reinterpret_cast<type &>(reinterpret_cast<uint32_t &>(a) &= static_cast<uint32_t>(b)); } \
		constexpr type operator&(type a, type b) { return static_cast<type>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); } \
		inline type &operator|=(type &a, type b) { return reinterpret_cast<type &>(reinterpret_cast<uint32_t &>(a) |= static_cast<uint32_t>(b)); } \
		constexpr type operator|(type a, type b) { return static_cast<type>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); } \
		inline type &operator^=(type &a, type b) { return reinterpret_cast<type &>(reinterpret_cast<uint32_t &>(a) ^= static_cast<uint32_t>(b)); } \
		constexpr type operator^(type a, type b) { return static_cast<type>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b)); }
#endif

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
		surface // Special type for resources that are implicitly both resource and resource view.
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
	/// The available memory map access types.
	/// </summary>
	enum class map_access
	{
		read_only,
		write_only,
		read_write,
		write_discard
	};

	/// <summary>
	/// The available memory heap types, which give a hint as to where to place the memory allocation for a resource.
	/// </summary>
	enum class memory_heap : uint32_t
	{
		unknown, // Used to indicate a resource that is reserved, but not yet bound to any memory.
		gpu_only,
		cpu_to_gpu,
		gpu_to_cpu,
		cpu_only,
		custom
	};

	/// <summary>
	/// A list of flags that describe additional parameters of a resource.
	/// </summary>
	enum class resource_flags : uint32_t
	{
		none = 0,
		shared = (1 << 0),
		dynamic = (1 << 1),
		sparse_binding = (1 << 2),
		cube_compatible = (1 << 3),
		generate_mipmaps = (1 << 4)
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(resource_flags);

	/// <summary>
	/// A list of flags that specify how a resource is to be used.
	/// This needs to be specified during creation and is also used to transition between different resource states within a command stream.
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

		general = 0x80000000,
		present = 0x80000000 | render_target | copy_source,
		cpu_access = vertex_buffer | index_buffer | shader_resource | 0x200 | copy_source
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(resource_usage);

	/// <summary>
	/// The available comparison types.
	/// This is compatible with 'VkCompareOp'.
	/// </summary>
	enum class compare_op : uint32_t
	{
		never = 0,
		less = 1,
		equal = 2,
		less_equal = 3,
		greater = 4,
		not_equal = 5,
		greater_equal = 6,
		always = 7
	};

	/// <summary>
	/// The available filtering types used for texture sampling operations.
	/// </summary>
	enum class filter_type : uint32_t
	{
		min_mag_mip_point = 0,
		min_mag_point_mip_linear = 0x1,
		min_point_mag_linear_mip_point = 0x4,
		min_point_mag_mip_linear = 0x5,
		min_linear_mag_mip_point = 0x10,
		min_linear_mag_point_mip_linear = 0x11,
		min_mag_linear_mip_point = 0x14,
		min_mag_mip_linear = 0x15,
		anisotropic = 0x55,
		compare_min_mag_mip_point = 0x80,
		compare_min_mag_point_mip_linear = 0x81,
		compare_min_point_mag_linear_mip_point = 0x84,
		compare_min_point_mag_mip_linear = 0x85,
		compare_min_linear_mag_mip_point = 0x90,
		compare_min_linear_mag_point_mip_linear = 0x91,
		compare_min_mag_linear_mip_point = 0x94,
		compare_min_mag_mip_linear = 0x95,
		compare_anisotropic = 0xd5,
	};

	/// <summary>
	/// Specifies behavior of sampling with texture coordinates outside a texture resource.
	/// </summary>
	enum class texture_address_mode : uint32_t
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
		filter_type filter;
		texture_address_mode address_u;
		texture_address_mode address_v;
		texture_address_mode address_w;
		float mip_lod_bias;
		float max_anisotropy;
		compare_op compare_op;
		float min_lod;
		float max_lod;
	};

	/// <summary>
	/// Describes a resource, such as a buffer or texture.
	/// </summary>
	struct resource_desc
	{
		resource_desc() :
			type(resource_type::unknown), texture(), heap(memory_heap::unknown), usage(resource_usage::undefined), flags(resource_flags::none) {}
		resource_desc(uint64_t size, memory_heap heap, resource_usage usage) :
			type(resource_type::buffer), buffer({ size }), heap(heap), usage(usage), flags(resource_flags::none) {}
		resource_desc(uint32_t width, uint32_t height, uint16_t layers, uint16_t levels, format format, uint16_t samples, memory_heap heap, resource_usage usage, resource_flags flags = resource_flags::none) :
			type(resource_type::texture_2d), texture({ width, height, layers, levels, format, samples }), heap(heap), usage(usage), flags(flags) {}
		resource_desc(resource_type type, uint32_t width, uint32_t height, uint16_t depth_or_layers, uint16_t levels, format format, uint16_t samples, memory_heap heap, resource_usage usage, resource_flags flags = resource_flags::none) :
			type(type), texture({ width, height, depth_or_layers, levels, format, samples }), heap(heap), usage(usage), flags(flags) {}

		/// <summary>
		/// Type of the resource.
		/// </summary>
		resource_type type;

		union
		{
			/// <summary>
			/// Used when resource type is a buffer.
			/// </summary>
			struct
			{
				/// <summary>
				/// Size of the buffer (in bytes).
				/// </summary>
				uint64_t size;
			} buffer;

			/// <summary>
			/// Used when resource type is a texture or surface.
			/// </summary>
			struct
			{
				/// <summary>
				/// Width of the texture (in texels).
				/// </summary>
				uint32_t width;
				/// <summary>
				/// If this is a 2D or 3D texture, height of the texture (in texels), otherwise 1.
				/// </summary>
				uint32_t height;
				/// <summary>
				/// If this is a 3D texture, depth of the texture (in texels), otherwise number of array layers.
				/// </summary>
				uint16_t depth_or_layers;
				/// <summary>
				/// Maximum number of mipmap levels in the texture, including the base level, so at least 1.
				/// </summary>
				uint16_t levels;
				/// <summary>
				/// Data format of each texel in the texture.
				/// </summary>
				format   format;
				/// <summary>
				/// The number of samples per texel. Set to a value higher than 1 for multisampling.
				/// </summary>
				uint16_t samples;
			} texture;
		};

		/// <summary>
		/// The heap the resource allocation is placed in.
		/// </summary>
		memory_heap heap;
		/// <summary>
		/// Flags that specify how this resource may be used.
		/// </summary>
		resource_usage usage;
		/// <summary>
		/// Flags that describe additional parameters.
		/// </summary>
		resource_flags flags;
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
		explicit resource_view_desc(format format) :
			type(resource_view_type::texture_2d), format(format), texture({ 0, 1, 0, 1 }) {}

		/// <summary>
		/// Type of the view. Identifies how the view should interpret the resource data.
		/// </summary>
		resource_view_type type;
		/// <summary>
		/// Viewing format of this view. 
		/// The data of the resource is reinterpreted to this format (can be different than the format of the underlying resource as long as the formats are compatible).
		/// </summary>
		format format;

		union
		{
			/// <summary>
			/// Used when view type is a buffer.
			/// </summary>
			struct
			{
				/// <summary>
				/// Offset from the start of the buffer resource (in bytes).
				/// </summary>
				uint64_t offset;
				/// <summary>
				/// Number of elements this view covers in the buffer resource (in bytes).
				/// </summary>
				uint64_t size;
			} buffer;

			/// <summary>
			/// Used when view type is a texture.
			/// </summary>
			struct
			{
				/// <summary>
				/// Index of the most detailed mipmap level to use. This number has to be between zero and the maximum number of mipmap levels in the texture minus 1.
				/// </summary>
				uint32_t first_level;
				/// <summary>
				/// Maximum number of mipmap levels for the view of the texture.
				/// Set to -1 (0xFFFFFFFF) to indicate that all mipmap levels down to the least detailed should be used.
				/// </summary>
				uint32_t levels;
				/// <summary>
				/// Index of the first array layer of the texture array to use. This value is ignored if the texture is not layered.
				/// </summary>
				uint32_t first_layer;
				/// <summary>
				/// Maximum number of array layers for the view of the texture array. This value is ignored if the texture is not layered.
				/// Set to -1 (0xFFFFFFFF) to indicate that all array layers should be used.
				/// </summary>
				uint32_t layers;
			} texture;
		};
	};

	/// <summary>
	/// Used to specify data for initializing a subresource or access existing subresource data.
	/// </summary>
	struct subresource_data
	{
		/// <summary>
		/// Pointer to the data.
		/// </summary>
		const void *data;
		/// <summary>
		/// The row pitch of the data (added to the data pointer to move between texture rows, unused for buffers and 1D textures).
		/// </summary>
		uint32_t row_pitch;
		/// <summary>
		/// The depth pitch of the data (added to the data pointer to move between texture depth/array slices, unused for buffers and 1D/2D textures).
		/// </summary>
		uint32_t slice_pitch;
	};

	/// <summary>
	/// An opaque handle to a sampler state object.
	/// <para>Depending on the render API this can be a pointer to a 'ID3D10SamplerState', 'ID3D11SamplerState' or a 'D3D12_CPU_DESCRIPTOR_HANDLE' (to a sampler descriptor) or 'VkSampler' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(sampler);

	/// <summary>
	/// An opaque handle to a resource object (buffer, texture, ...).
	/// <para>Resources created by the application are only guaranteed to be valid during event callbacks.
	/// If you want to use one outside that scope, first ensure the resource is still valid via <see cref="device::check_resource_handle_valid"/>.</para>
	/// <para>Depending on the render API this can be a pointer to a 'IDirect3DResource9', 'ID3D10Resource', 'ID3D11Resource' or 'ID3D12Resource' object or a 'VkImage' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(resource);

	/// <summary>
	/// An opaque handle to a resource view object (depth-stencil, render target, shader resource view, ...).
	/// <para>Resource views created by the application are only guaranteed to be valid during event callbacks.
	/// If you want to use one outside that scope, first ensure the resource view is still valid via <see cref="device::is_resource_view_handle_valid"/>.</para>
	/// <para>Depending on the render API this can be a pointer to a 'IDirect3DResource9', 'ID3D10View' or 'ID3D11View' object, or a 'D3D12_CPU_DESCRIPTOR_HANDLE' (to a view descriptor) or 'VkImageView' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(resource_view);
} }
