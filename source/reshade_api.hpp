/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <cstdint>

namespace reshade::api
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
		surface,
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
		common = 0,
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
		resolve_source = 0x2000
	};

	constexpr bool operator!=(resource_usage lhs, uint32_t rhs) { return static_cast<uint32_t>(lhs) != rhs; }
	constexpr bool operator!=(uint32_t lhs, resource_usage rhs) { return lhs != static_cast<uint32_t>(rhs); }
	constexpr bool operator==(resource_usage lhs, uint32_t rhs) { return static_cast<uint32_t>(lhs) == rhs; }
	constexpr bool operator==(uint32_t lhs, resource_usage rhs) { return lhs == static_cast<uint32_t>(rhs); }
	constexpr resource_usage operator&(resource_usage lhs, resource_usage rhs) { return static_cast<resource_usage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)); }
	constexpr resource_usage operator|(resource_usage lhs, resource_usage rhs) { return static_cast<resource_usage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)); }
	constexpr resource_usage &operator&=(resource_usage &lhs, resource_usage rhs) { return lhs = static_cast<resource_usage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)); }
	constexpr resource_usage &operator|=(resource_usage &lhs, resource_usage rhs) { return lhs = static_cast<resource_usage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)); }

	/// <summary>
	/// The available resource view types. The type of a resource view is specified during creation and is immutable.
	/// Some render APIs are more strict here then others, so views created by the game and passed into events may have an unknown view type.
	/// All resource views created through <see cref="device::create_resource_view"/> should always have a known type specified however (never <see cref="resource_view_type::unknown"/>).
	/// </summary>
	enum class resource_view_type : uint32_t
	{
		unknown,
		depth_stencil,
		render_target,
		shader_resource,
	};

	/// <summary>
	/// The available resource view dimensions. Identifies how the data of a resource should be interpreted.
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
		resource_type type;
		uint32_t width;
		uint32_t height;
		uint16_t layers;
		uint16_t levels;
		uint32_t format;
		uint16_t samples;
		resource_usage usage;
	};

	/// <summary>
	/// Describes a view (also known as subresource) into a resource.
	/// </summary>
	struct resource_view_desc
	{
		resource_view_type type;
		resource_view_dimension dimension;
		uint32_t format;
		uint32_t first_level;
		uint32_t levels;
		uint32_t first_layer;
		uint32_t layers;
		uint64_t byte_offset;
	};

	/// <summary>
	/// An opaque handle to a resource object (buffer, texture, ...).
	/// Resources created by the application are only guaranteed to be valid during event callbacks.
	/// If you want to use one outside that scope, first ensure the resource is still valid via <see cref="device::is_resource_valid"/>.
	/// Functionally equivalent to a 'IDirect3DResource9', 'ID3D10Resource', 'ID3D11Resource', 'ID3D12Resource' or 'VkImage'.
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
	/// If you want to use one outside that scope, first ensure the resource is still valid via <see cref="device::is_resource_view_valid"/>.
	/// Functionally equivalent to a 'ID3D10View', 'ID3D11View', 'D3D12_CPU_DESCRIPTOR_HANDLE' or 'VkImageView'.
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
	/// The base class for objects provided by this API.
	/// This lets you store and retrieve custom data with objects, to be able to have persistent data between event callbacks.
	/// </summary>
	class __declspec(novtable) api_object
	{
	public:
		/// <summary>
		/// Gets custom data from the object that was previously set via <see cref="api_object::set_data"/>.
		/// </summary>
		/// <returns><c>true</c> if the data with the associated <paramref name="guid"/> exists and was copied to <paramref name="data"/>, <c>false</c> otherwise.</returns>
		virtual bool get_data(const uint8_t guid[16], uint32_t size, void *data) = 0;
		/// <summary>
		/// Sets custom data to the object and associate it with a <paramref name="guid"/> for later retrieval.
		/// You can call this with <paramref name="size"/> set to zero and <paramref name="data"/> set to <c>nullptr</c> to destroy an entry.
		/// </summary>
		virtual void set_data(const uint8_t guid[16], uint32_t size, const void *data) = 0;

		// Helper templates to manage custom data creation and destruction:
		template <typename T> inline T &get_data(const uint8_t guid[16])
		{
			T *res = nullptr;
			get_data(guid, sizeof(res), &res);
			return *res;
		}
		template <typename T> inline T &create_data(const uint8_t guid[16])
		{
			T *res = new T();
			set_data(guid, sizeof(res), &res);
			return *res;
		}
		template <typename T> inline void destroy_data(const uint8_t guid[16])
		{
			T *res = nullptr;
			get_data(guid, sizeof(res), &res);
			delete res;
			set_data(guid, 0, nullptr);
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
		/// Gets the underlying render API used for this device.
		/// </summary>
		virtual render_api get_api() = 0;

		/// <summary>
		/// Checks whether the specified <paramref name="format"/> is supported with the specified <paramref name="usage"/> on the current system.
		/// </summary>
		virtual bool check_format_support(uint32_t format, resource_usage usage) = 0;

		/// <summary>
		/// Checks whether the specified <paramref name="resource"/> handle points to a resource that is still alive and valid.
		/// </summary>
		virtual bool is_resource_valid(resource_handle resource) = 0;
		/// <summary>
		/// Checks whether the specified <paramref name="view"/> handle points to a resource view that is still alive and valid.
		/// </summary>
		virtual bool is_resource_view_valid(resource_view_handle view) = 0;

		/// <summary>
		/// Allocates and creates a new resource based on the specified <paramref name="desc"/>ription.
		/// </summary>
		virtual bool create_resource(const resource_desc &desc, resource_usage initial_state, resource_handle *out_resource) = 0;
		/// <summary>
		/// Creates a new resource view for the <paramref name="resource"/> based on the specified <paramref name="desc"/>ription.
		/// </summary>
		virtual bool create_resource_view(resource_handle resource, const resource_view_desc &desc, resource_view_handle *out_view) = 0;

		/// <summary>
		/// Instantly destroys a <paramref name="resource"/> that was previously created via <see cref="create_resource"/>.
		/// Make sure it is no longer in use on the GPU (via any command list that may reference it and is still being executed) before doing this.
		/// Also never try to destroy resources created by the application!
		/// </summary>
		virtual void destroy_resource(resource_handle resource) = 0;
		/// <summary>
		/// Instantly destroys a resource <paramref name="view"/> that was previously created via <see cref="create_resource_view"/>.
		/// </summary>
		virtual void destroy_resource_view(resource_view_handle view) = 0;

		/// <summary>
		/// Gets the handle to the underlying resource behind the specified resource <paramref name="view"/>.
		/// </summary>
		virtual void get_resource_from_view(resource_view_handle view, resource_handle *out_resource) = 0;

		/// <summary>
		/// Gets the description of the specified <paramref name="resource"/>.
		/// </summary>
		virtual resource_desc get_resource_desc(resource_handle resource) = 0;

		/// <summary>
		/// Waits for all issued GPU operations to finish before returning.
		/// This can be used to ensure that e.g. resources are no longer in use on the GPU before destroying them.
		/// </summary>
		virtual void wait_idle() = 0;
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
		/// Adds a transition barrier for the specified <paramref name="resource"/> to the command stream.
		/// </summary>
		/// <param name="resource">The resource to transition.</param>
		/// <param name="old_state">The usage flags describing how the resource was used before this barrier.</param>
		/// <param name="new_state">The usage flags describing how the resource will be used after this barrier.</param>
		virtual void transition_state(resource_handle resource, resource_usage old_state, resource_usage new_state) = 0;

		/// <summary>
		/// Clears a depth-stencil resource.
		/// </summary>
		/// <param name="dsv">The view handle of the depth-stencil.</param>
		/// <param name="clear_flags">A combination of the following flags to identify which types of data to clear: <c>0x1</c> for depth, <c>0x2</c> for stencil</param>
		/// <param name="depth">The value to clear the depth buffer with.</param>
		/// <param name="stencil">The value to clear the stencil buffer with.</param>
		virtual void clear_depth_stencil_view(resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) = 0;
		/// <summary>
		/// Clears an entire texture resource.
		/// </summary>
		/// <param name="rtv">The view handle of the render target.</param>
		/// <param name="color">The value to clear the resource with.</param>
		virtual void clear_render_target_view(resource_view_handle rtv, const float color[4]) = 0;

		/// <summary>
		/// Copies the entire contents of the <paramref name="source"/> resource to the <paramref name="destination"/> resource.
		/// </summary>
		virtual void copy_resource(resource_handle source, resource_handle destination) = 0;
	};

	/// <summary>
	/// A command queue, used to execute command lists on the GPU.
	/// Functionally equivalent to a 'ID3D11DeviceContext', 'ID3D12CommandQueue' or 'VkQueue'.
	/// </summary>
	class __declspec(novtable) command_queue : public device_object
	{
	public:
		/// <summary>
		/// Gets a special command list, on which all issued commands are executed immediately.
		/// This is only available in D3D9, D3D10, D3D11 and OpenGL. D3D12 and Vulkan will return a <c>nullptr</c>.
		/// </summary>
		virtual command_list *get_immediate_command_list() = 0;
	};

	/// <summary>
	/// A ReShade effect runtime, used to control effects. A separate runtime is instantiated for every swap chain.
	/// </summary>
	class __declspec(novtable) effect_runtime : public device_object
	{
	public:
		/// <summary>
		/// Gets the current buffer dimensions of the swap chain.
		/// </summary>
		virtual void get_frame_width_and_height(uint32_t *width, uint32_t *height) const = 0;

		/// <summary>
		/// Updates all textures that use the specified <paramref name="semantic"/> in all active effects to new resource view.
		/// </summary>
		virtual void update_texture_bindings(const char *semantic, resource_view_handle shader_resource_view) = 0;
	};
}
