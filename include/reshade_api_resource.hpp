/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#define RESHADE_DEFINE_HANDLE(name) \
	typedef struct { uint64_t handle; } name; \
	constexpr bool operator< (name lhs, name rhs) { return lhs.handle < rhs.handle; } \
	constexpr bool operator!=(name lhs, name rhs) { return lhs.handle != rhs.handle; } \
	constexpr bool operator!=(name lhs, uint64_t rhs) { return lhs.handle != rhs; } \
	constexpr bool operator==(name lhs, name rhs) { return lhs.handle == rhs.handle; } \
	constexpr bool operator==(name lhs, uint64_t rhs) { return lhs.handle == rhs; }

#define RESHADE_DEFINE_ENUM_FLAG_OPERATORS(type) \
	constexpr type operator~(type a) { return static_cast<type>(~static_cast<uint32_t>(a)); } \
	inline type &operator&=(type &a, type b) { return reinterpret_cast<type &>(reinterpret_cast<uint32_t &>(a) &= static_cast<uint32_t>(b)); } \
	constexpr type operator&(type a, type b) { return static_cast<type>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); } \
	inline type &operator|=(type &a, type b) { return reinterpret_cast<type &>(reinterpret_cast<uint32_t &>(a) |= static_cast<uint32_t>(b)); } \
	constexpr type operator|(type a, type b) { return static_cast<type>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); } \
	inline type &operator^=(type &a, type b) { return reinterpret_cast<type &>(reinterpret_cast<uint32_t &>(a) ^= static_cast<uint32_t>(b)); } \
	constexpr type operator^(type a, type b) { return static_cast<type>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b)); } \
	constexpr bool operator==(type lhs, uint32_t rhs) { return static_cast<uint32_t>(lhs) == rhs; } \
	constexpr bool operator!=(type lhs, uint32_t rhs) { return static_cast<uint32_t>(lhs) != rhs; }

#include "reshade_api_format.hpp"

namespace reshade::api
{
	/// <summary>
	/// The available comparison types.
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
	/// The available filtering modes used for texture sampling operations.
	/// </summary>
	enum class filter_mode : uint32_t
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
		compare_anisotropic = 0xd5
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
		/// <summary>
		/// Filtering mode to use when sampling a texture.
		/// </summary>
		filter_mode filter = filter_mode::min_mag_mip_linear;
		/// <summary>
		/// Method to use for resolving U texture coordinates outside 0 to 1 range.
		/// </summary>
		texture_address_mode address_u = texture_address_mode::clamp;
		/// <summary>
		/// Method to use for resolving V texture coordinates outside 0 to 1 range.
		/// </summary>
		texture_address_mode address_v = texture_address_mode::clamp;
		/// <summary>
		/// Method to use for resolving W texture coordinates outside 0 to 1 range.
		/// </summary>
		texture_address_mode address_w = texture_address_mode::clamp;
		/// <summary>
		/// Offset applied to the calculated mipmap level when sampling a texture.
		/// </summary>
		float mip_lod_bias = 0.0f;
		/// <summary>
		/// Clamping value to use when filtering mode is <see cref="filter_mode::anisotropic"/>.
		/// </summary>
		float max_anisotropy = 1.0f;
		/// <summary>
		/// Comparison function to use to compare sampled data against existing sampled data.
		/// </summary>
		compare_op compare_op = compare_op::never;
		/// <summary>
		/// RGBA value to return for texture coordinates outside 0 to 1 range when addressing mode is <see cref="texture_address_mode::border"/>.
		/// </summary>
		float border_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		/// <summary>
		/// Lower end of the mipmap range to clamp access to.
		/// </summary>
		float min_lod = -FLT_MAX;
		/// <summary>
		/// Upper end of the mipmap range to clamp access to.
		/// </summary>
		float max_lod = +FLT_MAX;
	};

	/// <summary>
	/// An opaque handle to a sampler state object.
	/// <para>Depending on the render API this can be a pointer to a 'ID3D10SamplerState', 'ID3D11SamplerState' or a 'D3D12_CPU_DESCRIPTOR_HANDLE' (to a sampler descriptor) or 'VkSampler' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(sampler);

	/// <summary>
	/// The available memory mapping access types.
	/// </summary>
	enum class map_access
	{
		read_only = 1,
		write_only,
		read_write,
		write_discard
	};

	/// <summary>
	/// The available memory heap types, which give a hint as to where to place the memory allocation for a resource.
	/// </summary>
	enum class memory_heap : uint32_t
	{
		unknown, // Usually indicates a resource that is reserved, but not yet bound to any memory.
		gpu_only,
		// Upload heap
		cpu_to_gpu,
		// Readback heap
		gpu_to_cpu,
		cpu_only,
		custom
	};

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
		surface // Special type for resources that are implicitly both resource and render target view.
	};

	/// <summary>
	/// A list of flags that describe additional parameters of a resource.
	/// </summary>
	enum class resource_flags : uint32_t
	{
		none = 0,
		dynamic = (1 << 3),
		cube_compatible = (1 << 2),
		generate_mipmaps = (1 << 0),
		shared = (1 << 1),
		shared_nt_handle = (1 << 11),
		structured = (1 << 6),
		sparse_binding = (1 << 18)
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(resource_flags);

	/// <summary>
	/// A list of flags that specify how a resource is to be used.
	/// This needs to be specified during creation and is also used to transition between different resource states within a command list.
	/// </summary>
	enum class resource_usage : uint32_t
	{
		undefined = 0,

		index_buffer = 0x2,
		vertex_buffer = 0x1,
		constant_buffer = 0x8000,
		stream_output = 0x100,
		indirect_argument = 0x200,

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

		// The following are special resource states and may only be used in barriers:

		general = 0x80000000,
		present = 0x80000000 | render_target | copy_source,
		cpu_access = vertex_buffer | index_buffer | shader_resource | indirect_argument | copy_source
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(resource_usage);

	/// <summary>
	/// Describes a resource, such as a buffer or texture.
	/// </summary>
	struct resource_desc
	{
		constexpr resource_desc() : texture() {}
		constexpr resource_desc(uint64_t size, memory_heap heap, resource_usage usage) :
			type(resource_type::buffer), buffer({ size }), heap(heap), usage(usage) {}
		constexpr resource_desc(uint32_t width, uint32_t height, uint16_t layers, uint16_t levels, format format, uint16_t samples, memory_heap heap, resource_usage usage, resource_flags flags = resource_flags::none) :
			type(resource_type::texture_2d), texture({ width, height, layers, levels, format, samples }), heap(heap), usage(usage), flags(flags) {}
		constexpr resource_desc(resource_type type, uint32_t width, uint32_t height, uint16_t depth_or_layers, uint16_t levels, format format, uint16_t samples, memory_heap heap, resource_usage usage, resource_flags flags = resource_flags::none) :
			type(type), texture({ width, height, depth_or_layers, levels, format, samples }), heap(heap), usage(usage), flags(flags) {}

		/// <summary>
		/// Type of the resource.
		/// </summary>
		resource_type type = resource_type::unknown;

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
				uint64_t size = 0;
				/// <summary>
				/// Structure stride for structured buffers (in bytes), otherwise zero.
				/// </summary>
				uint32_t stride = 0;
			} buffer;

			/// <summary>
			/// Used when resource type is a texture or surface.
			/// </summary>
			struct
			{
				/// <summary>
				/// Width of the texture (in texels).
				/// </summary>
				uint32_t width = 1;
				/// <summary>
				/// If this is a 2D or 3D texture, height of the texture (in texels), otherwise 1.
				/// </summary>
				uint32_t height = 1;
				/// <summary>
				/// If this is a 3D texture, depth of the texture (in texels), otherwise number of array layers.
				/// </summary>
				uint16_t depth_or_layers = 1;
				/// <summary>
				/// Maximum number of mipmap levels in the texture, including the base level, so at least 1.
				/// Can also be zero in case the exact number of mipmap levels is unknown.
				/// </summary>
				uint16_t levels = 1;
				/// <summary>
				/// Data format of each texel in the texture.
				/// </summary>
				format   format = format::unknown;
				/// <summary>
				/// The number of samples per texel. Set to a value higher than 1 for multisampling.
				/// </summary>
				uint16_t samples = 1;
			} texture;
		};

		/// <summary>
		/// Memory heap the resource allocation is placed in.
		/// </summary>
		memory_heap heap = memory_heap::unknown;
		/// <summary>
		/// Flags that specify how this resource may be used.
		/// </summary>
		resource_usage usage = resource_usage::undefined;
		/// <summary>
		/// Flags that describe additional parameters.
		/// </summary>
		resource_flags flags = resource_flags::none;
	};

	/// <summary>
	/// An opaque handle to a resource object (buffer, texture, ...).
	/// <para>Resources created by the application are only guaranteed to be valid during event callbacks.
	/// <para>Depending on the render API this can be a pointer to a 'IDirect3DResource9', 'ID3D10Resource', 'ID3D11Resource' or 'ID3D12Resource' object or a 'VkImage' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(resource);

	/// <summary>
	/// The available resource view types. These identify how a resource view interprets the data of its resource.
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
	/// Describes a resource view, which specifies how to interpret the data of a resource.
	/// </summary>
	struct resource_view_desc
	{
		constexpr resource_view_desc() : texture() {}
		constexpr resource_view_desc(format format, uint64_t offset, uint64_t size) :
			type(resource_view_type::buffer), format(format), buffer({ offset, size }) {}
		constexpr resource_view_desc(format format, uint32_t first_level, uint32_t levels, uint32_t first_layer, uint32_t layers) :
			type(resource_view_type::texture_2d), format(format), texture({ first_level, levels, first_layer, layers }) {}
		constexpr resource_view_desc(resource_view_type type, format format, uint32_t first_level, uint32_t levels, uint32_t first_layer, uint32_t layers) :
			type(type), format(format), texture({ first_level, levels, first_layer, layers }) {}
		constexpr explicit resource_view_desc(format format) : type(resource_view_type::texture_2d), format(format), texture({ 0, 1, 0, 1 }) {}

		/// <summary>
		/// Resource type the view should interpret the resource data to.
		/// </summary>
		resource_view_type type = resource_view_type::unknown;
		/// <summary>
		/// Format the view should reinterpret the resource data to (can be different than the format of the resource as long as they are compatible).
		/// </summary>
		format format = format::unknown;

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
				uint64_t offset = 0;
				/// <summary>
				/// Number of elements this view covers in the buffer resource (in bytes).
				/// Set to -1 (UINT64_MAX) to indicate that the entire buffer resource should be used.
				/// </summary>
				uint64_t size = UINT64_MAX;
			} buffer;

			/// <summary>
			/// Used when view type is a texture.
			/// </summary>
			struct
			{
				/// <summary>
				/// Index of the most detailed mipmap level to use. This number has to be between zero and the maximum number of mipmap levels in the texture minus 1.
				/// </summary>
				uint32_t first_level = 0;
				/// <summary>
				/// Maximum number of mipmap levels for the view of the texture.
				/// Set to -1 (UINT32_MAX) to indicate that all mipmap levels down to the least detailed should be used.
				/// </summary>
				uint32_t level_count = UINT32_MAX;
				/// <summary>
				/// Index of the first array layer of the texture array to use. This value is ignored if the texture is not layered.
				/// </summary>
				uint32_t first_layer = 0;
				/// <summary>
				/// Maximum number of array layers for the view of the texture array. This value is ignored if the texture is not layered.
				/// Set to -1 (UINT32_MAX) to indicate that all array layers should be used.
				/// </summary>
				uint32_t layer_count = UINT32_MAX;
			} texture;
		};
	};

	/// <summary>
	/// An opaque handle to a resource view object (depth-stencil, render target, shader resource view, ...).
	/// <para>Resource views created by the application are only guaranteed to be valid during event callbacks.
	/// <para>Depending on the render API this can be a pointer to a 'IDirect3DResource9', 'ID3D10View' or 'ID3D11View' object, or a 'D3D12_CPU_DESCRIPTOR_HANDLE' (to a view descriptor) or 'VkImageView' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(resource_view);

	/// <summary>
	/// Describes a region inside a subresource.
	/// </summary>
	struct subresource_box
	{
		int32_t left = 0;
		int32_t top = 0;
		int32_t front = 0;
		int32_t right = 0;
		int32_t bottom = 0;
		int32_t back = 0;

		constexpr uint32_t width() const { return right - left; }
		constexpr uint32_t height() const { return bottom - top; }
		constexpr uint32_t depth() const { return back - front; }
	};

	/// <summary>
	/// Describes the data of a subresource.
	/// </summary>
	struct subresource_data
	{
		/// <summary>
		/// Pointer to the data.
		/// </summary>
		void *data = nullptr;
		/// <summary>
		/// Row pitch of the data (added to the data pointer to move between texture rows, unused for buffers and 1D textures).
		/// </summary>
		/// <seealso cref="format_row_pitch"/>
		uint32_t row_pitch = 0;
		/// <summary>
		/// Depth pitch of the data (added to the data pointer to move between texture depth/array slices, unused for buffers and 1D/2D textures).
		/// </summary>
		/// <seealso cref="format_slice_pitch"/>
		uint32_t slice_pitch = 0;
	};

	/// <summary>
	/// Specifies how the contents of a render target or depth-stencil view are treated at the start of a render pass.
	/// </summary>
	enum class render_pass_load_op : uint32_t
	{
		load,
		clear,
		discard,
		no_access
	};

	/// <summary>
	/// Specifies how the contents of a render target or depth-stencil view are treated at the end of a render pass.
	/// </summary>
	enum class render_pass_store_op : uint32_t
	{
		store,
		discard,
		no_access
	};

	/// <summary>
	/// Describes a depth-stencil view and how it is treated at the start and end of a render pass.
	/// </summary>
	struct render_pass_depth_stencil_desc
	{
		/// <summary>
		/// Depth-stencil resource view.
		/// </summary>
		resource_view view = { 0 };
		/// <summary>
		/// Specifies how the depth contents of the depth-stencil view are treated at the start of the render pass.
		/// </summary>
		render_pass_load_op depth_load_op = render_pass_load_op::load;
		/// <summary>
		/// Specifies how the depth contents of the depth-stencil view are treated at the end of the render pass.
		/// </summary>
		render_pass_store_op depth_store_op = render_pass_store_op::store;
		/// <summary>
		/// Specifies how the stencil contents of the depth-stencil view are treated at the start of the render pass.
		/// </summary>
		render_pass_load_op stencil_load_op = render_pass_load_op::load;
		/// <summary>
		/// Specifies how the stencil contents of the depth-stencil view are treated at the end of the render pass.
		/// </summary>
		render_pass_store_op stencil_store_op = render_pass_store_op::store;
		/// <summary>
		/// Value the depth contents of the depth-stencil resource is cleared to when <see cref="depth_load_op"/> is <see cref="render_pass_load_op::clear"/>.
		/// </summary>
		float clear_depth = 0.0f;
		/// <summary>
		/// Value the stencil contents of the depth-stencil resource is cleared to when <see cref="stencil_load_op"/> is <see cref="render_pass_load_op::clear"/>.
		/// </summary>
		uint8_t clear_stencil = 0;
	};

	/// <summary>
	/// Describes a render target view and how it is treated at the start and end of a render pass.
	/// </summary>
	struct render_pass_render_target_desc
	{
		/// <summary>
		/// Render target resource view.
		/// </summary>
		resource_view view = { 0 };
		/// <summary>
		/// Specifies how the contents of the render target view are treated at the start of the render pass.
		/// </summary>
		render_pass_load_op load_op = render_pass_load_op::load;
		/// <summary>
		/// Specifies how the contents of the render target view are treated at the end of the render pass.
		/// </summary>
		render_pass_store_op store_op = render_pass_store_op::store;
		/// <summary>
		/// Value the render target resource is cleared to when <see cref="load_op"/> is <see cref="render_pass_load_op::clear"/>.
		/// </summary>
		float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	};
}
