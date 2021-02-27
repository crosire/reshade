/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <cstdint>

namespace reshade { namespace api
{
	/// <summary>
	/// The underlying render API a device is using, as returned by <see cref="device::get_api"/>.
	/// </summary>
	enum class render_api
	{
		d3d9,
		d3d10,
		d3d11,
		d3d12,
		opengl,
		vulkan
	};

	/// <summary>
	/// The available resource types. The type of a resource is specified during creation and is immutable.
	/// Various operations may have special requirements on the type of resources they operate on (e.g. copies can only happen between resources of the same type, ...).
	/// </summary>
	enum class resource_type : uint32_t
	{
		unknown,
		buffer,
		surface, // Surfaces are resources that can be rendered into (can create a render target for them), but can not be read in shaders.
		texture_1d,
		texture_2d,
		texture_3d
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
	/// The available resource view types. The type of a resource view is specified during creation and is immutable.
	/// Some render APIs are more strict here then others, so views created by the application and passed into events may have an unknown view type.
	/// All resource views created through <see cref="device::create_resource_view"/> should have a known type specified however (not <see cref="resource_view_type::unknown"/>).
	/// </summary>
	enum class resource_view_type : uint32_t
	{
		unknown,
		depth_stencil,
		render_target,
		shader_resource,
		unordered_access
	};

	/// <summary>
	/// The available resource view dimensions. Identifies how a view should interpret the resource.
	/// </summary>
	enum class resource_view_dimension : uint32_t
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
	/// Describes a resource, such as a buffer or texture.
	/// </summary>
	struct resource_desc
	{
		// If this is a texture, width of the texture (in texels), otherwise lower 32-bits of the 64-bit buffer size (in bytes).
		uint32_t width;
		// If this is a 2D or 3D texture, height of the texture (in texels), otherwise 1 or upper 32-bits of the 64-bit buffer size (in bytes).
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
		// Flags that specify how this resource may be used.
		resource_usage usage;
	};

	/// <summary>
	/// Describes a view (also known as subresource) into a resource.
	/// </summary>
	struct resource_view_desc
	{
		// Type of the view. Identifies how the view should interpret the resource.
		resource_view_dimension dimension;
		// Viewing format of this view. The data of the resource is reinterpreted in this format when accessed through this view.
		// This is a 'D3DFORMAT', 'DXGI_FORMAT', OpenGL internal format or 'VkFormat' value, depending on the render API.
		uint32_t format;
		// If this is a buffer view, lower 32-bits of the 64-bit offset from the start of the buffer resource (in bytes).
		// If this is a texture view, index of the most detailed mipmap level to use. This number is between zero and the maximum number of mipmap levels in the texture minus 1.
		uint32_t first_level;
		// If this is a buffer view, lower 32-bits of the 64-bit range of elements this view covers in the buffer resource (in bytes).
		// If this is a texture view, maximum number of mipmap levels for the view of the texture. Set to -1 to indicate that all mipmap levels down to the least detailed should be used.
		uint32_t levels;
		// If this is a buffer view, upper 32-bits of the 64-bit offset from the start of the buffer resource (in bytes).
		// If this is a texture view, index of the first array layer of the texture array to use.
		uint32_t first_layer;
		// If this is a buffer view, upper 32-bits of the 64-bit range of elements this view covers in the buffer resource (in bytes).
		// If this is a texture view, maximum number of array layers for the view of the texture array.
		uint32_t layers;
	};

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
	class __declspec(novtable) api_object
	{
	public:
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
	class __declspec(novtable) device : public api_object
	{
	public:
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
		/// Allocates and creates a new resource based on the specified <paramref name="desc"/>ription.
		/// </summary>
		/// <param name="type">The type of the resource to create.</param>
		/// <param name="desc">The description of the resource to create.</param>
		/// <param name="initial_state">Initial usage of the resource after creation. This can later be changed via <see cref="command_list::transition_state"/>.</param>
		/// <param name="out_resource">Pointer to a handle that is set to the handle of the created resource.</param>
		/// <returns><c>true</c>if the resource was successfully created, <c>false</c> otherwise (in this case <paramref name="out_resource"/> is set to zero).</returns>
		virtual bool create_resource(resource_type type, const resource_desc &desc, resource_usage initial_state, resource_handle *out_resource) = 0;
		/// <summary>
		/// Creates a new resource view for the specified <paramref name="resource"/> based on the specified <paramref name="desc"/>ription.
		/// </summary>
		/// <param name="resource">The resource the view is created for.</param>
		/// <param name="type">The type of the resource view to create.</param>
		/// <param name="desc">The description of the resource to create.</param>
		/// <param name="out_view">Pointer to a handle that is set to the handle of the created resource view.</param>
		/// <returns><c>true</c>if the resource view was successfully created, <c>false</c> otherwise (in this case <paramref name="out_view"/> is set to zero).</returns>
		virtual bool create_resource_view(resource_handle resource, resource_view_type type, const resource_view_desc &desc, resource_view_handle *out_view) = 0;

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
	class __declspec(novtable) device_object : public api_object
	{
	public:
		/// <summary>
		/// Gets the parent device for this object.
		/// </summary>
		virtual device *get_device() = 0;
	};

	/// <summary>
	/// A command list, used to enqueue render commands on the CPU, before later executing them in a command queue.
	/// Functionally equivalent to a 'ID3D11CommandList', 'ID3D12CommandList' or 'VkCommandBuffer'.
	/// </summary>
	class __declspec(novtable) command_list : public device_object
	{
	public:
		/// <summary>
		/// Copies the entire contents of the <paramref name="source"/> resource to the <paramref name="destination"/> resource.
		/// The <paramref name="source"/> resource has to be in the <see cref="resource_usage::copy_source"/> state.
		/// The <paramref name="destination"/> resource has to be in the <see cref="resource_usage::copy_dest"/> state.
		/// </summary>
		virtual void copy_resource(resource_handle source, resource_handle destination) = 0;

		/// <summary>
		/// Adds a transition barrier for the specified <paramref name="resource"/> to the command stream.
		/// </summary>
		/// <param name="resource">The resource to transition.</param>
		/// <param name="old_state">The usage flags describing how the resource was used before this barrier.</param>
		/// <param name="new_state">The usage flags describing how the resource will be used after this barrier.</param>
		virtual void transition_state(resource_handle resource, resource_usage old_state, resource_usage new_state) = 0;

		/// <summary>
		/// Clears a depth-stencil resource.
		/// The resource the <paramref name="dsv"/> view points to has to be in the <see cref="resource_usage::depth_stencil_write"/> state.
		/// </summary>
		/// <param name="dsv">The view handle of the depth-stencil.</param>
		/// <param name="clear_flags">A combination of flags to identify which types of data to clear: <c>0x1</c> for depth, <c>0x2</c> for stencil</param>
		/// <param name="depth">The value to clear the depth buffer with.</param>
		/// <param name="stencil">The value to clear the stencil buffer with.</param>
		virtual void clear_depth_stencil_view(resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) = 0;
		/// <summary>
		/// Clears an entire texture resource.
		/// The resource the <paramref name="rtv"/> view points to has to be in the <see cref="resource_usage::render_target"/> state-
		/// </summary>
		/// <param name="rtv">The view handle of the render target.</param>
		/// <param name="color">The value to clear the resource with.</param>
		virtual void clear_render_target_view(resource_view_handle rtv, const float color[4]) = 0;
	};

	/// <summary>
	/// A command queue, used to execute command lists on the GPU.
	/// Functionally equivalent to the immediate 'ID3D11DeviceContext' or a 'ID3D12CommandQueue' or 'VkQueue'.
	/// </summary>
	class __declspec(novtable) command_queue : public device_object
	{
	public:
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
	class __declspec(novtable) effect_runtime : public device_object
	{
	public:
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
