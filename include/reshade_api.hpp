/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <cstdint>

#pragma warning(push)
#pragma warning(disable: 4201)

namespace reshade { namespace api
{
	/// <summary>
	/// The underlying render API a device is using, as returned by <see cref="device::get_api"/>.
	/// </summary>
	enum class render_api
	{
		d3d9 = 0x9000,
		d3d10 = 0xa000,
		d3d11 = 0xb000,
		d3d12 = 0xc000,
		opengl = 0x10000,
		vulkan = 0x20000
	};

	/// <summary>
	/// A list of flags that represent the available shader stages in the render pipeline.
	/// </summary>
	enum class shader_stage
	{
		vertex = 0x1,
		hull = 0x2,
		domain = 0x4,
		geometry = 0x8,
		pixel = 0x10,
		compute = 0x20,

		all = 0x7FFFFFFF,
		all_graphics = 0x1F
	};

	/// <summary>
	/// A list of all possible render pipeline states that can be set. Support for these varies between render APIs.
	/// </summary>
	enum class pipeline_state
	{
		unknown = 0,

		// Blend state

		alpha_test = 15,
		alpha_ref = 24,
		alpha_func = 25,

		sample_alpha_to_coverage = 907,

		blend = 27,
		blend_factor = 193,
		blend_color_src = 19,
		blend_color_dest = 20,
		blend_color_op = 171,
		blend_alpha_src = 207,
		blend_alpha_dest = 208,
		blend_alpha_op = 209,

		srgb_write = 194,
		render_target_write_mask = 168,
		render_target_write_mask_1 = 190,
		render_target_write_mask_2 = 191,
		render_target_write_mask_3 = 192,
		sample_mask = 162,

		// Rasterizer state

		fill_mode = 8,
		cull_mode = 22,
		front_face_ccw = 908,

		depth_bias = 195,
		depth_bias_clamp = 909,
		depth_bias_slope_scaled = 175,

		depth_clip = 136,
		scissor_test = 174,
		multisample = 161,
		antialiased_line = 176,

		primitive_topology = 900,

		// Depth-stencil state

		depth_test = 7,
		depth_write_mask = 14,
		depth_func = 23,

		stencil_test = 52,
		stencil_ref = 57,
		stencil_read_mask = 58,
		stencil_write_mask = 59,
		stencil_back_fail = 186,
		stencil_back_depth_fail = 187,
		stencil_back_pass = 188,
		stencil_back_func = 189,
		stencil_front_fail = 53,
		stencil_front_depth_fail = 54,
		stencil_front_pass = 55,
		stencil_front_func = 56,
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
		constant_buffer = 0x8000
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
			type(resource_type::unknown), width(0), height(0), depth_or_layers(0), levels(0), format(0), samples(0), heap(memory_heap::unknown), usage(resource_usage::undefined) {}
		resource_desc(uint64_t size, memory_heap heap, resource_usage usage) :
			type(resource_type::buffer), size(size), heap(heap), usage(usage) {}
		resource_desc(uint32_t width, uint32_t height, uint16_t layers, uint16_t levels, uint32_t format, uint16_t samples, memory_heap heap, resource_usage usage) :
			type(resource_type::texture_2d), width(width), height(height), depth_or_layers(layers), levels(levels), format(format), samples(samples), heap(heap), usage(usage) {}
		resource_desc(resource_type type, uint32_t width, uint32_t height, uint16_t depth_or_layers, uint16_t levels, uint32_t format, uint16_t samples, memory_heap heap, resource_usage usage) :
			type(type), width(width), height(height), depth_or_layers(depth_or_layers), levels(levels), format(format), samples(samples), heap(heap), usage(usage) {}

		// Type of the resource.
		resource_type type;

		union
		{
			// Used when resource type is a buffer.
			struct
			{
				// Size of the buffer (in bytes).
				uint64_t size;
			};
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
				// This is a 'D3DFORMAT', 'DXGI_FORMAT', OpenGL internal format or 'VkFormat' value, depending on the render API.
				uint32_t format;
				// The number of samples per texel. Set to a value higher than 1 for multisampling.
				uint16_t samples;
			};
		};

		// The heap the resource allocation is placed in.
		memory_heap heap;
		// Flags that specify how this resource may be used.
		resource_usage usage;
	};

	/// <summary>
	/// Describes a view (also known as subresource) into a resource.
	/// </summary>
	struct resource_view_desc
	{
		resource_view_desc() :
			type(resource_view_type::unknown), format(0), first_level(0), levels(0), first_layer(0), layers(0) {}
		resource_view_desc(uint32_t format, uint64_t offset, uint64_t size) :
			type(resource_view_type::buffer), format(format), offset(offset), size(size) {}
		resource_view_desc(uint32_t format, uint32_t first_level, uint32_t levels, uint32_t first_layer, uint32_t layers) :
			type(resource_view_type::texture_2d), format(format), first_level(first_level), levels(levels), first_layer(first_layer), layers(layers) {}
		resource_view_desc(resource_view_type type, uint32_t format, uint32_t first_level, uint32_t levels, uint32_t first_layer, uint32_t layers) :
			type(type), format(format), first_level(first_level), levels(levels), first_layer(first_layer), layers(layers) {}

		// Type of the view. Identifies how the view should interpret the resource.
		resource_view_type type;
		// Viewing format of this view. The data of the resource is reinterpreted in this format when accessed through this view.
		// This is a 'D3DFORMAT', 'DXGI_FORMAT', OpenGL internal format or 'VkFormat' value, depending on the render API.
		uint32_t format;

		union
		{
			// Used when view type is a buffer.
			struct
			{
				// Offset from the start of the buffer resource (in bytes).
				uint64_t offset;
				// Number of elements this view covers in the buffer resource (in bytes).
				uint64_t size;
			};
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
			};
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
	/// Depending on the render API this is really a pointer to a 'ID3D10SamplerState', 'ID3D11SamplerState' or a 'VkSampler' handle.
	/// </summary>
	typedef struct { uint64_t handle; } sampler_handle;

	constexpr bool operator< (sampler_handle lhs, sampler_handle rhs) { return lhs.handle < rhs.handle; }
	constexpr bool operator!=(sampler_handle lhs, uint64_t rhs) { return lhs.handle != rhs; }
	constexpr bool operator!=(uint64_t lhs, sampler_handle rhs) { return lhs != rhs.handle; }
	constexpr bool operator!=(sampler_handle lhs, sampler_handle rhs) { return lhs.handle != rhs.handle; }
	constexpr bool operator==(sampler_handle lhs, uint64_t rhs) { return lhs.handle == rhs; }
	constexpr bool operator==(uint64_t lhs, sampler_handle rhs) { return lhs == rhs.handle; }
	constexpr bool operator==(sampler_handle lhs, sampler_handle rhs) { return lhs.handle == rhs.handle; }

	/// <summary>
	/// An opaque handle to a resource object (buffer, texture, ...).
	/// Resources created by the application are only guaranteed to be valid during event callbacks.
	/// If you want to use one outside that scope, first ensure the resource is still valid via <see cref="device::check_resource_handle_valid"/>.
	/// Depending on the render API this is really a pointer to a 'IDirect3DResource9', 'ID3D10Resource', 'ID3D11Resource' or 'ID3D12Resource' object or a 'VkImage' handle.
	/// </summary>
	typedef struct { uint64_t handle; } resource_handle;

	constexpr bool operator< (resource_handle lhs, resource_handle rhs) { return lhs.handle < rhs.handle; }
	constexpr bool operator!=(resource_handle lhs, uint64_t rhs) { return lhs.handle != rhs; }
	constexpr bool operator!=(uint64_t lhs, resource_handle rhs) { return lhs != rhs.handle; }
	constexpr bool operator!=(resource_handle lhs, resource_handle rhs) { return lhs.handle != rhs.handle; }
	constexpr bool operator==(resource_handle lhs, uint64_t rhs) { return lhs.handle == rhs; }
	constexpr bool operator==(uint64_t lhs, resource_handle rhs) { return lhs == rhs.handle; }
	constexpr bool operator==(resource_handle lhs, resource_handle rhs) { return lhs.handle == rhs.handle; }

	/// <summary>
	/// An opaque handle to a resource view object (depth-stencil, render target, shader resource view, ...).
	/// Resource views created by the application are only guaranteed to be valid during event callbacks.
	/// If you want to use one outside that scope, first ensure the resource view is still valid via <see cref="device::check_resource_view_handle_valid"/>.
	/// Depending on the render API this is really a pointer to a 'IDirect3DResource9', 'ID3D10View' or 'ID3D11View' object, or a 'D3D12_CPU_DESCRIPTOR_HANDLE' or 'VkImageView' handle.
	/// </summary>
	typedef struct { uint64_t handle; } resource_view_handle;

	constexpr bool operator< (resource_view_handle lhs, resource_view_handle rhs) { return lhs.handle < rhs.handle; }
	constexpr bool operator!=(resource_view_handle lhs, uint64_t rhs) { return lhs.handle != rhs; }
	constexpr bool operator!=(uint64_t lhs, resource_view_handle rhs) { return lhs != rhs.handle; }
	constexpr bool operator!=(resource_view_handle lhs, resource_view_handle rhs) { return lhs.handle != rhs.handle; }
	constexpr bool operator==(resource_view_handle lhs, uint64_t rhs) { return lhs.handle == rhs; }
	constexpr bool operator==(uint64_t lhs, resource_view_handle rhs) { return lhs == rhs.handle; }
	constexpr bool operator==(resource_view_handle lhs, resource_view_handle rhs) { return lhs.handle == rhs.handle; }

	/// <summary>
	/// The base class for objects provided by the ReShade API.
	/// This lets you store and retrieve custom data with objects, to be able to communicate persistent data between event callbacks.
	/// </summary>
	struct __declspec(novtable) api_object
	{
		/// <summary>
		/// Gets a custom data pointer from the object that was previously set via <see cref="api_object::set_data"/>.
		/// This function is not thread-safe!
		/// </summary>
		/// <returns><c>true</c> if a pointer was previously set with this <paramref name="guid"/>, <c>false</c> otherwise.</returns>
		virtual bool get_data(const uint8_t guid[16], void **ptr) const = 0;
		/// <summary>
		/// Sets a custom data pointer associated with the specified <paramref name="guid"/> to the object.
		/// You can call this with <paramref name="ptr"/> set to <c>nullptr</c> to remove the pointer associated with the provided <paramref name="guid"/> from this object.
		/// This function is not thread-safe!
		/// </summary>
		virtual void set_data(const uint8_t guid[16], void * const ptr) = 0;

		/// <summary>
		/// Gets the underlying native object for this API object.
		/// For <see cref="device"/>s this will be be a pointer to a 'IDirect3DDevice9', 'ID3D10Device', 'ID3D11Device' or 'ID3D12Device' object or a 'HGLRC' or 'VkDevice' handle.
		/// For <see cref="command_list"/>s this will be a pointer to a 'ID3D11DeviceContext' (when recording), 'ID3D11CommandList' (when executing) or 'ID3D12GraphicsCommandList' object or a 'VkCommandBuffer' handle.
		/// For <see cref="command_queue"/>s this will be a pointer to a 'ID3D11DeviceContext' or 'ID3D12CommandQueue' object or a 'VkQueue' handle.
		/// For <see cref="effect_runtime"/>s this will be a pointer to a 'IDirect3DSwapChain9' or 'IDXGISwapChain' object or a 'HDC' or 'VkSwapchainKHR' handle.
		/// </summary>
		virtual uint64_t get_native_object() const = 0;

		// Helper templates to manage custom data creation and destruction:
		template <typename T> inline T &get_data(const uint8_t guid[16]) const
		{
			T *res = nullptr;
			get_data(guid, reinterpret_cast<void **>(&res));
			return *res;
		}
		template <typename T> inline T &create_data(const uint8_t guid[16])
		{
			T *res = new T();
			set_data(guid, res);
			return *res;
		}
		template <typename T> inline void destroy_data(const uint8_t guid[16])
		{
			T *res = nullptr;
			get_data(guid, reinterpret_cast<void **>(&res));
			delete res;
			set_data(guid, nullptr);
		}
	};

	/// <summary>
	/// A logical render device, used for resource creation and global operations.
	/// Functionally equivalent to a 'IDirect3DDevice9', 'ID3D10Device', 'ID3D11Device', 'ID3D12Device', 'HGLRC' or 'VkDevice'.
	/// </summary>
	struct __declspec(novtable) device : public api_object
	{
		/// <summary>
		/// Gets the underlying render API used by this device.
		/// </summary>
		virtual render_api get_api() const = 0;

		/// <summary>
		/// Checks whether the specified <paramref name="format"/> supports the specified <paramref name="usage"/>.
		/// </summary>
		virtual bool check_format_support(uint32_t format, resource_usage usage) const = 0;

		/// <summary>
		/// Checks whether the specified <paramref name="resource"/> handle points to a resource that is still alive and valid.
		/// This function is thread-safe.
		/// </summary>
		virtual bool check_resource_handle_valid(resource_handle resource) const = 0;
		/// <summary>
		/// Checks whether the specified resource <paramref name="view"/> handle points to a resource view that is still alive and valid.
		/// This function is thread-safe.
		/// </summary>
		virtual bool check_resource_view_handle_valid(resource_view_handle view) const = 0;

		/// <summary>
		/// Creates a new sampler state object based on the specified <paramref name="desc"/>ription.
		/// </summary>
		/// <param name="desc">The description of the sampler to create.</param>
		/// <param name="out_sampler">Pointer to a handle that is set to the handle of the created sampler.</param>
		/// <returns><c>true</c>if the sampler was successfully created, <c>false</c> otherwise (in this case <paramref name="out_sampler"/> is set to zero).</returns>
		virtual bool create_sampler(const sampler_desc &desc, sampler_handle *out_sampler) = 0;
		/// <summary>
		/// Allocates and creates a new resource based on the specified <paramref name="desc"/>ription.
		/// </summary>
		/// <param name="desc">The description of the resource to create.</param>
		/// <param name="initial_data">Data to upload to the resource after creation. This should point to an array of <see cref="mapped_subresource"/>, one for each subresource (mipmap levels and array layers).</param>
		/// <param name="initial_state">Initial usage of the resource after creation. This can later be changed via <see cref="command_list::transition_state"/>.</param>
		/// <param name="out_resource">Pointer to a handle that is set to the handle of the created resource.</param>
		/// <returns><c>true</c>if the resource was successfully created, <c>false</c> otherwise (in this case <paramref name="out_resource"/> is set to zero).</returns>
		virtual bool create_resource(const resource_desc &desc, const subresource_data *initial_data, resource_usage initial_state, resource_handle *out_resource) = 0;
		/// <summary>
		/// Creates a new resource view for the specified <paramref name="resource"/> based on the specified <paramref name="desc"/>ription.
		/// </summary>
		/// <param name="resource">The resource the view is created for.</param>
		/// <param name="usage_type">The usage type of the resource view to create. Set to <see cref="resource_usage::shader_resource"/> to create a shader resource view, <see cref="resource_usage::depth_stencil"/> for a depth-stencil view, <see cref="resource_usage::render_target"/> for a render target etc.</param>
		/// <param name="desc">The description of the resource to create.</param>
		/// <param name="out_view">Pointer to a handle that is set to the handle of the created resource view.</param>
		/// <returns><c>true</c>if the resource view was successfully created, <c>false</c> otherwise (in this case <paramref name="out_view"/> is set to zero).</returns>
		virtual bool create_resource_view(resource_handle resource, resource_usage usage_type, const resource_view_desc &desc, resource_view_handle *out_view) = 0;

		/// <summary>
		/// Instantly destroys a <paramref name="sampler"/> that was previously created via <see cref="create_sampler"/>.
		/// </summary>
		virtual void destroy_sampler(sampler_handle sampler) = 0;
		/// <summary>
		/// Instantly destroys a <paramref name="resource"/> that was previously created via <see cref="create_resource"/> and frees its memory.
		/// Make sure it is no longer in use on the GPU (via any command list that may reference it and is still being executed) before doing this and never try to destroy resources created by the application!
		/// </summary>
		virtual void destroy_resource(resource_handle resource) = 0;
		/// <summary>
		/// Instantly destroys a resource <paramref name="view"/> that was previously created via <see cref="create_resource_view"/>.
		/// </summary>
		virtual void destroy_resource_view(resource_view_handle view) = 0;

		/// <summary>
		/// Gets the handle to the underlying resource the specified resource <paramref name="view"/> was created for.
		/// This function is thread-safe.
		/// </summary>
		virtual void get_resource_from_view(resource_view_handle view, resource_handle *out_resource) const = 0;

		/// <summary>
		/// Gets the description of the specified <paramref name="resource"/>.
		/// This function is thread-safe.
		/// </summary>
		/// <param name="resource">The resource to get the description from.</param>
		virtual resource_desc get_resource_desc(resource_handle resource) const = 0;

		/// <summary>
		/// Waits for all issued GPU operations to finish before returning.
		/// This can be used to ensure that e.g. resources are no longer in use on the GPU before destroying them.
		/// </summary>
		virtual void wait_idle() const = 0;
	};

	/// <summary>
	/// The base class for objects that are children to a logical render <see cref="device"/>.
	/// </summary>
	struct __declspec(novtable) device_object : public api_object
	{
		/// <summary>
		/// Gets the parent device for this object.
		/// </summary>
		virtual device *get_device() = 0;
	};

	/// <summary>
	/// A command list, used to enqueue render commands on the CPU, before later executing them in a command queue.
	/// Functionally equivalent to a 'ID3D11CommandList', 'ID3D12CommandList' or 'VkCommandBuffer'.
	/// </summary>
	struct __declspec(novtable) command_list : public device_object
	{
		/// <summary>
		/// Blits a region from the <paramref name="source"/> texture to a different region of the <paramref name="destination"/> texture.
		/// <para>The <paramref name="source"/> resource has to be in the <see cref="resource_usage::copy_source"/> state.</para>
		/// <para>The <paramref name="destination"/> resource has to be in the <see cref="resource_usage::copy_dest"/> state.</para>
		/// </summary>
		/// <param name="source">The texture resource to blit from.</param>
		/// <param name="src_subresource">The subresource of the <paramref name="source"/> texture to blit from.</param>
		/// <param name="src_box">A 3D box (or <c>nullptr</c> to reference the entire subresource) that defines the region in the <paramref name="source"/> resource to blit from, in the format { left, top, front, right, bottom, back }.</param>
		/// <param name="destination">The texture resource to blit to.</param>
		/// <param name="dst_subresource">The subresource of the <paramref name="destination"/> texture to blit to.</param>
		/// <param name="dst_box">A 3D box (or <c>nullptr</c> to reference the entire subresource) that defines the region in the <paramref name="destination"/> resource to blit to, in the format { left, top, front, right, bottom, back }.</param>
		/// <param name="filter">The filter to apply when blit requires scaling.</param>
		virtual void blit(resource_handle source, uint32_t src_subresource, const int32_t src_box[6], resource_handle destination, uint32_t dst_subresource, const int32_t dst_box[6], texture_filter filter) = 0;
		/// <summary>
		/// Copies a region from the multisampled <paramref name="source"/> texture to the non-multisampled <paramref name="destination"/> texture.
		/// <para>The <paramref name="source"/> resource has to be in the <see cref="resource_usage::resolve_source"/> state.</para>
		/// <para>The <paramref name="destination"/> resource has to be in the <see cref="resource_usage::resolve_dest"/> state.</para>
		/// </summary>
		/// <param name="source">The texture to resolve from.</param>
		/// <param name="src_subresource">The subresource of the <paramref name="source"/> texture to resolve from.</param>
		/// <param name="src_offset">A 3D offset to start resolving at. In D3D10, D3D11 and D3D12 this has to be <c>nullptr</c>.</param>
		/// <param name="destination">The texture to resolve to.</param>
		/// <param name="dst_subresource">The subresource of the <paramref name="destination"/> texture to resolve to.</param>
		/// <param name="dst_offset">A 3D offset to start resolving to. In D3D10, D3D11 and D3D12 this has to be <c>nullptr</c>.</param>
		/// <param name="size">Width, height and depth of the texture region to resolve.</param>
		/// <param name="format">The format of the resource data.</param>
		virtual void resolve(resource_handle source, uint32_t src_subresource, const int32_t src_offset[3], resource_handle destination, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], uint32_t format) = 0;
		/// <summary>
		/// Copies the entire contents of the <paramref name="source"/> resource to the <paramref name="destination"/> resource.
		/// <para>The <paramref name="source"/> resource has to be in the <see cref="resource_usage::copy_source"/> state.</para>
		/// <para>The <paramref name="destination"/> resource has to be in the <see cref="resource_usage::copy_dest"/> state.</para>
		/// </summary>
		/// <param name="source">The resource to copy from.</param>
		/// <param name="destination">The resource to copy to.</param>
		virtual void copy_resource(resource_handle source, resource_handle destination) = 0;
		/// <summary>
		/// Copies a linear memory region from the <paramref name="source"/> buffer to the <paramref name="destination"/> buffer.
		/// This is not supported (and will do nothing) in D3D9.
		/// <para>The <paramref name="source"/> resource has to be in the <see cref="resource_usage::copy_source"/> state.</para>
		/// <para>The <paramref name="destination"/> resource has to be in the <see cref="resource_usage::copy_dest"/> state.</para>
		/// </summary>
		/// <param name="source">The buffer to copy from.</param>
		/// <param name="src_offset">An offset in bytes into the <paramref name="source"/> buffer to start copying at.</param>
		/// <param name="destination">The buffer to copy to.</param>
		/// <param name="dst_offset">An offset in bytes into the <paramref name="destination"/> buffer to start copying to.</param>
		/// <param name="size">The number of bytes to copy.</param>
		virtual void copy_buffer_region(resource_handle source, uint64_t src_offset, resource_handle dst, uint64_t dst_offset, uint64_t size) = 0;
		/// <summary>
		/// Copies a texture region from the <paramref name="source"/> buffer to the <paramref name="destination"/> texture.
		/// <para>The <paramref name="source"/> resource has to be in the <see cref="resource_usage::copy_source"/> state.</para>
		/// <para>The <paramref name="destination"/> resource has to be in the <see cref="resource_usage::copy_dest"/> state.</para>
		/// </summary>
		/// <param name="source">The buffer to copy from.</param>
		/// <param name="src_offset">An offset in bytes into the <paramref name="source"/> buffer to start copying at.</param>
		/// <param name="row_length">The number of pixels from one row to the next (in the buffer), or zero if data is tightly packed.</param>
		/// <param name="slice_height">The number of rows from one slice to the next (in the buffer) or zero if data is tightly packed.</param>
		/// <param name="destination">The texture to copy to.</param>
		/// <param name="dst_subresource">The subresource of the <paramref name="destination"/> texture to copy to.</param>
		/// <param name="dst_box">A 3D box (or <c>nullptr</c> to reference the entire subresource) that defines the region in the <paramref name="destination"/> texture to copy to, in the format { left, top, front, right, bottom, back }.</param>
		virtual void copy_buffer_to_texture(resource_handle source, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, resource_handle destination, uint32_t dst_subresource, const int32_t dst_box[6]) = 0;
		/// <summary>
		/// Copies a texture region from the <paramref name="source"/> texture to the <paramref name="destination"/> texture.
		/// <para>The <paramref name="source"/> resource has to be in the <see cref="resource_usage::copy_source"/> state.</para>
		/// <para>The <paramref name="destination"/> resource has to be in the <see cref="resource_usage::copy_dest"/> state.</para>
		/// </summary>
		/// <param name="source">The texture to copy from.</param>
		/// <param name="src_subresource">The subresource of the <paramref name="source"/> texture to copy from.</param>
		/// <param name="src_offset">A 3D offset to start copying at.</param>
		/// <param name="destination">The texture to copy to.</param>
		/// <param name="dst_subresource">The subresource of the <paramref name="destination"/> texture to copy to.</param>
		/// <param name="dst_offset">A 3D offset to start copying to.</param>
		/// <param name="size">Width, height and depth of the texture region to copy.</param>
		virtual void copy_texture_region(resource_handle source, uint32_t src_subresource, const int32_t src_offset[3], resource_handle destination, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3]) = 0;
		/// <summary>
		/// Copies a texture region from the <paramref name="source"/> texture to the <paramref name="destination"/> buffer.
		/// <para>The <paramref name="source"/> resource has to be in the <see cref="resource_usage::copy_source"/> state.</para>
		/// <para>The <paramref name="destination"/> resource has to be in the <see cref="resource_usage::copy_dest"/> state.</para>
		/// </summary>
		/// <param name="source">The texture to copy from.</param>
		/// <param name="src_subresource">The subresource of the <paramref name="source"/> texture to copy from.</param>
		/// <param name="src_box">A 3D box (or <c>nullptr</c> to reference the entire subresource) that defines the region in the <paramref name="source"/> texture to copy from, in the format { left, top, front, right, bottom, back }.</param>
		/// <param name="destination">The buffer to copy to.</param>
		/// <param name="dst_offset">An offset in bytes into the <paramref name="destination"/> buffer to start copying to.</param>
		/// <param name="row_length">The number of pixels from one row to the next (in the buffer), or zero if data is tightly packed.</param>
		/// <param name="slice_height">The number of rows from one slice to the next (in the buffer), op zero if data is tightly packed.</param>
		virtual void copy_texture_to_buffer(resource_handle source, uint32_t src_subresource, const int32_t src_box[6], resource_handle destination, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height) = 0;

		/// <summary>
		/// Clears a depth-stencil resource.
		/// <para>The resource the <paramref name="dsv"/> view points to has to be in the <see cref="resource_usage::depth_stencil_write"/> state.</para>
		/// </summary>
		/// <param name="dsv">The view handle of the depth-stencil.</param>
		/// <param name="clear_flags">A combination of flags to identify which types of data to clear: <c>0x1</c> for depth, <c>0x2</c> for stencil</param>
		/// <param name="depth">The value to clear the depth buffer with.</param>
		/// <param name="stencil">The value to clear the stencil buffer with.</param>
		virtual void clear_depth_stencil_view(resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) = 0;
		/// <summary>
		/// Clears an entire texture resource.
		/// <para>The resource the <paramref name="rtv"/> view points to has to be in the <see cref="resource_usage::render_target"/> state.</para>
		/// </summary>
		/// <param name="rtv">The view handle of the render target.</param>
		/// <param name="color">The value to clear the resource with.</param>
		virtual void clear_render_target_views(uint32_t count, const resource_view_handle *rtvs, const float color[4]) = 0;

		/// <summary>
		/// Adds a transition barrier for the specified <paramref name="resource"/> to the command stream.
		/// </summary>
		/// <param name="resource">The resource to transition.</param>
		/// <param name="old_state">The usage flags describing how the resource was used before this barrier.</param>
		/// <param name="new_state">The usage flags describing how the resource will be used after this barrier.</param>
		virtual void transition_state(resource_handle resource, resource_usage old_state, resource_usage new_state) = 0;
	};

	/// <summary>
	/// A command queue, used to execute command lists on the GPU.
	/// Functionally equivalent to the immediate 'ID3D11DeviceContext' or a 'ID3D12CommandQueue' or 'VkQueue'.
	/// </summary>
	struct __declspec(novtable) command_queue : public device_object
	{
		/// <summary>
		/// Gets a special command list, on which all issued commands are executed as soon as possible (or right before the application executes its next command list on this queue).
		/// </summary>
		virtual command_list *get_immediate_command_list() = 0;

		/// <summary>
		/// Flushes and executes the special immediate command list returned by <see cref="command_queue::get_immediate_command_list"/> immediately.
		/// This can be used to force commands to execute right away instead of waiting for the runtime to flush it automatically at some point.
		/// </summary>
		virtual void flush_immediate_command_list() const  = 0;
	};

	/// <summary>
	/// A ReShade effect runtime, used to control effects. A separate runtime is instantiated for every swap chain ('IDirect3DSwapChain9', 'IDXGISwapChain', 'HDC' or 'VkSwapchainKHR').
	/// </summary>
	struct __declspec(novtable) effect_runtime : public device_object
	{
		/// <summary>
		/// Gets the main graphics command queue associated with this effect runtime.
		/// This may potentially be different from the presentation queue and should be used to execute graphics commands on.
		/// </summary>
		virtual command_queue *get_command_queue() = 0;

		/// <summary>
		/// Gets the current buffer dimensions of the swap chain.
		/// </summary>
		virtual void get_frame_width_and_height(uint32_t *width, uint32_t *height) const = 0;

		/// <summary>
		/// Updates all textures that use the specified <paramref name="semantic"/> in all active effects to new resource view.
		/// </summary>
		virtual void update_texture_bindings(const char *semantic, resource_view_handle shader_resource_view) = 0;

		/// <summary>
		/// Updates the values of all uniform variables with a "source" annotation set to <paramref name="source"/> to the specified <paramref name="values"/>.
		/// </summary>
		virtual void update_uniform_variables(const char *source, const bool *values, size_t count, size_t array_index = 0) = 0;
		virtual void update_uniform_variables(const char *source, const float *values, size_t count, size_t array_index = 0) = 0;
		virtual void update_uniform_variables(const char *source, const int32_t *values, size_t count, size_t array_index = 0) = 0;
		virtual void update_uniform_variables(const char *source, const uint32_t *values, size_t count, size_t array_index = 0) = 0;
	};
} }

#pragma warning(pop)
