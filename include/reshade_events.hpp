/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_api.hpp"

namespace reshade
{
	enum class addon_event
	{
		/// <summary>
		/// Called after 'IDirect3D9::CreateDevice(Ex)', 'D3D10CreateDevice(AndSwapChain)', 'D3D11CreateDevice(AndSwapChain)', 'D3D12CreateDevice', 'glMakeCurrent' or 'vkCreateDevice'.
		/// <para>Callback function signature: <c>void (api::device *device)</c></para>
		/// </summary>
		init_device,
		/// <summary>
		/// Called on device destruction, before last 'IDirect3DDevice9::Release', 'ID3D10Device::Release', 'ID3D11Device::Release', 'ID3D12Device::Release', 'wglDeleteContext' or 'vkDestroyDevice'.
		/// <para>Callback function signature: <c>void (api::device *device)</c></para>
		/// </summary>
		destroy_device,
		/// <summary>
		/// Called after 'ID3D11DeviceContext::CreateDeferredContext(1/2/3)', 'ID3D12Device::CreateCommandList(1)' or 'vkAllocateCommandBuffers'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		init_command_list,
		/// <summary>
		/// Called on command list destruction, before last 'ID3D11CommandList::Release', 'ID3D12CommandList::Release' or 'vkFreeCommandBuffers'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		destroy_command_list,
		/// <summary>
		/// Called after 'ID3D12Device::CreateCommandQueue' or 'vkCreateDevice'.
		/// <para>Callback function signature: <c>void (api::command_queue *queue)</c></para>
		/// </summary>
		/// <remarks>
		/// In case of D3D9, D3D10, D3D11 and OpenGL this is called during device initialization as well and behaves as if an implicit command queue and list was created.
		/// </remarks>
		init_command_queue,
		/// <summary>
		/// Called on command queue destruction, before last 'ID3D12CommandQueue::Release' or 'vkDestroyDevice'.
		/// <para>Callback function signature: <c>void (api::command_queue *queue)</c></para>
		/// </summary>
		destroy_command_queue,
		/// <summary>
		/// Called after effect runtime initialization (which happens after swap chain creation or a swap chain buffer resize).
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime)</c></para>
		/// </summary>
		init_effect_runtime,
		/// <summary>
		/// Called when an effect runtime is reset or destroyed.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime)</c></para>
		/// </summary>
		destroy_effect_runtime,

		/// <summary>
		/// Called before 'IDirect3Device9::Create(...)Buffer/Texture', 'IDirect3DDevice9::Create(...)Surface(Ex)', 'ID3D10Device::CreateBuffer/Texture(...)', 'ID3D11Device::CreateBuffer/Texture(...)', 'ID3D12Device::Create(...)Resource', 'gl(Named)Buffer/Tex(ture)Storage(...)' or 'vkCreateBuffer/Image'.
		/// <para>Callback function signature: <c>bool (api::device *device, const api::resource_desc &desc, const api::mapped_subresource *initial_data, api::resource_usage initial_state)</c></para>
		/// </summary>
		create_resource,
		/// <summary>
		/// Called before 'IDirect3DDevice9::Create(...)Surface(Ex)', 'ID3D10Device::Create(...)View', 'ID3D11Device::Create(...)View', 'ID3D12Device::Create(...)View', 'glTex(ture)Buffer', 'glTextureView(...)' or 'vkCreateBuffer/ImageView'.
		/// <para>Callback function signature: <c>bool (api::device *device, api::resource_handle resource, api::resource_usage usage_type, const api::resource_view_desc &desc)</c></para>
		/// </summary>
		create_resource_view,
		/// <summary>
		/// Called before 'IDirect3DDevice9::Create(...)Shader', 'ID3D10Device::Create(...)Shader', 'ID3D11Device::Create(...)Shader', 'ID3D12Device::Create(...)PipelineState', 'glShaderSource' or 'vkCreateShaderModule'.
		/// <para>Callback function signature: <c>bool (api::device *device, const void *code, size_t code_size)</c></para>
		/// </summary>
		create_shader_module,

		/// <summary>
		/// Called after 'IDirect3DDevice9::SetIndices', 'ID3D10Device::IASetIndexBuffer', 'ID3D11DeviceContext::IASetIndexBuffer', 'ID3D12GraphicsCommandList::IASetIndexBuffer', 'glBindBuffer(...)' or 'vkCmdBindIndexBuffer'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::resource_handle buffer, uint32_t format, uint64_t offset)</c></para>
		/// </summary>
		set_index_buffer,
		/// <summary>
		/// Called after 'IDirect3DDevice9::SetStreamSource', 'ID3D10Device::IASetVertexBuffers', 'ID3D11DeviceContext::IASetVertexBuffers', 'ID3D12GraphicsCommandList::IASetVertexBuffers', 'glBindBuffer(...)' or 'vkCmdBindVertexBuffers'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const api::resource_handle *buffers, const uint32_t *strides, const uint64_t *offsets)</c></para>
		/// </summary>
		set_vertex_buffers,
		/// <summary>
		/// Called after 'IDirect3DDevice9::SetViewport', 'IDirect3DDevice9::SetRenderTarget' (which implicitly sets the viewport), 'ID3D10Device::RSSetViewports', 'ID3D11DeviceContext::RSSetViewports', 'ID3D12GraphicsCommandList::RSSetViewports', 'glViewport(...)' or 'vkCmdSetViewport'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const float *viewports)</c></para>
		/// <para>Viewport data format is { viewport[0].x, viewport[0].y, viewport[0].width, viewport[0].height, viewport[0].min_depth, viewport[0].max_depth, viewport[1].x, viewport[1].y, ... }.</para>
		/// </summary>
		set_viewports,
		/// <summary>
		/// Called after 'IDirect3DDevice9::SetScissorRect', 'ID3D10Device::RSSetScissorRects', 'ID3D11DeviceContext::RSSetScissorRects', 'ID3D12GraphicsCommandList::RSSetScissorRects', 'glScissor' or 'vkCmdSetScissor'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const int32_t *rects)</c></para>
		/// <para>Rectangle data format is { rect[0].left, rect[0].top, rect[0].right, rect[0].bottom, rect[1].left, rect[1].right, ... }.</para>
		/// </summary>
		set_scissor_rects,
		/// <summary>
		/// Called after 'IDirect3DDevice9::SetRenderTarget', 'IDirect3DDevice9::SetDepthStencilSurface', 'ID3D10Device::OMSetRenderTargets', 'ID3D11DeviceContext::OMSetRenderTargets(AndUnorderedAccessViews)', 'ID3D12GraphicsCommandList::OMSetRenderTargets', 'ID3D12GraphicsCommandList::BeginRenderPass', 'glBindFramebuffer' or before 'vkCmdBeginRenderPass' or 'vkCmdNextSubpass'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t count, const api::resource_view_handle *rtvs, api::resource_view_handle dsv)</c></para>
		/// </summary>
		set_render_targets_and_depth_stencil,
		/// <summary>
		/// Called after 'IDirect3DDevice9::SetRenderState', 'ID3D10Device::(...)Set(...)State', 'ID3D11DeviceContext::(...)Set(...)State', 'ID3D12GraphicsCommandList::(...)Set(...)', 'gl(...)', 'vkCmdSet(...)' or 'vkCmdBindPipeline'.
		/// </summary>
		set_pipeline_states,

		/// <summary>
		/// Called before 'IDirect3DDevice9::DrawPrimitive(UP)', 'ID3D10Device::Draw(Instanced)', 'ID3D11DeviceContext::Draw(Instanced)', 'ID3D12GraphicsCommandList::DrawInstanced', 'gl(Multi)DrawArrays(...)' or 'vkCmdDraw'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)</c></para>
		/// </summary>
		draw,
		/// <summary>
		/// Called before 'IDirect3DDevice9::DrawIndexedPrimitive(UP)', 'ID3D10Device::DrawIndexed(Instanced)', 'ID3D11DeviceContext::DrawIndexed(Instanced)', 'ID3D12GraphicsCommandList::DrawIndexedInstanced', 'gl(Multi)DrawElements(...)' or 'vkCmdDrawIndexed'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)</c></para>
		/// </summary>
		draw_indexed,
		/// <summary>
		/// Called before 'ID3D11DeviceContext::Dispatch', 'ID3D12GraphicsCommandList::Dispatch', 'glDispatchCompute' or 'vkCmdDispatch'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)</c></para>
		/// </summary>
		dispatch,
		/// <summary>
		/// Called before 'ID3D11DeviceContext::Draw(Indexed)InstancedIndirect', 'ID3D11DeviceContext::DispatchIndirect', 'ID3D12GraphicsCommandList::ExecuteIndirect', 'gl(Multi)Draw(...)Indirect', 'glDispatchComputeIndirect', 'vkCmdDraw(Indexed)Indirect' or 'vkCmdDispatchIndirect'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, addon_event type, api::resource_handle buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)</c></para>
		/// <para>The "type" parameter to the callback function will be <c>addon_event::draw</c>, <c>addon_event::draw_indexed</c> or <c>addon_event::dispatch</c> depending on the indirect command. Can also be <c>addon_event::draw_or_dispatch_indirect</c> if the type could not be determined.</para>
		/// </summary>
		draw_or_dispatch_indirect,
		/// <summary>
		/// Called before 'IDirect3DDevice9::UpdateTexture', 'IDirect3DDevice9::StretchRect', 'ID3D10Device::CopyResource', 'ID3D11DeviceContext::CopyResource', 'ID3D12GraphicsCommandList::CopyResource'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_handle src, api::resource_handle dst)</c></para>
		/// </summary>
		/// <remarks>
		/// Source resource will be in the <see cref="resource_usage::copy_src"/> state. Destination resource will be in the <see cref="resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_resource,
		/// <summary>
		/// Called before 'ID3D12GraphicsCommandList::CopyBufferRegion' or 'vkCmdCopyBuffer'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_handle src, uint64_t src_offset, api::resource_handle dst, uint64_t dst_offset, uint64_t size)</c></para>
		/// </summary>
		/// <remarks>
		/// Source resource will be in the <see cref="resource_usage::copy_src"/> state. Destination resource will be in the <see cref="resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_buffer_region,
		/// <summary>
		/// Called before 'IDirect3DDevice9::UpdateSurface', 'IDirect3DDevice9::StretchRect', 'ID3D10Device::CopySubresourceRegion', 'ID3D11DeviceContext::CopySubresourceRegion', 'ID3D12GraphicsCommandList::CopyTextureRegion', 'glBlit(Named)Framebuffer', 'vkCmdBlitImage' or 'vkCmdCopy(...)'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_handle src, const api::copy_location &src_location, const int32_t *src_box, api::resource_handle dst, const api::copy_location &dst_location, const int32_t *dst_box)</c></para>
		/// <para>Box data format is { box->left, box->top, box->front, box->right, box->bottom, box->back }.</para>
		/// </summary>
		/// <remarks>
		/// Source resource will be in the <see cref="resource_usage::copy_src"/> state. Destination resource will be in the <see cref="resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_texture_region,
		/// <summary>
		/// Called before 'ID3D10Device::UpdateSubresource' or 'ID3D11DeviceContext::UpdateSubresource'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, const api::mapped_subresource *data, api::resource_handle dst, const api::copy_location &dst_location, const int32_t *dst_box)</c></para>
		/// <para>Box data format is { box->left, box->top, box->front, box->right, box->bottom, box->back }.</para>
		/// </summary>
		/// <remarks>
		/// Destination resource will be in the <see cref="resource_usage::copy_dest"/> state.
		/// </remarks>
		update_resource_region,
		/// <summary>
		/// Called before 'IDirect3DDevice9::Clear', 'ID3D10Device::ClearDepthStencilView', 'ID3D11DeviceContext::ClearDepthStencilView', 'ID3D12GraphicsCommandList::ClearDepthStencilView', 'glClear(...) ', 'vkCmdBeginRenderPass' or 'vkCmdClearDepthStencilImage'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)</c></para>
		/// </summary>
		/// <remarks>
		/// Resource will be in the <see cref="resource_usage::depth_stencil_write"/> state.
		/// </remarks>
		clear_depth_stencil,
		/// <summary>
		/// Called before 'IDirect3DDevice9::Clear', 'ID3D10Device::ClearRenderTargetView', 'ID3D11DeviceContext::ClearRenderTargetView', 'ID3D12GraphicsCommandList::ClearRenderTargetView', 'glClear(...)', 'vkCmdBeginRenderPass' or 'vkCmdClearColorImage'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view_handle rtv, const float color[4])</c></para>
		/// </summary>
		/// <remarks>
		/// Resource will be in the <see cref="resource_usage::render_target"/> state.
		/// </remarks>
		clear_render_target,

		/// <summary>
		/// Called before 'ID3D12GraphicsCommandList::Reset' or 'vkBeginCommandBuffer'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		reset_command_list,
		/// <summary>
		/// Called before 'ID3D11DeviceContext::ExecuteCommandList', 'ID3D12CommandQueue::ExecuteCommandLists' or 'vkQueueSubmit'.
		/// <para>Callback function signature: <c>void (api::command_queue *queue, api::command_list *cmd_list)</c></para>
		/// </summary>
		execute_command_list,
		/// <summary>
		/// Called before 'ID3D12GraphicsCommandList::ExecuteBundle' or 'vkCmdExecuteCommands' or after 'ID3D11DeviceContext::FinishCommandList'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::command_list *secondary_cmd_list)</c></para>
		/// </summary>
		execute_secondary_command_list,

		/// <summary>
		/// Called before 'IDXGISwapChain::ResizeBuffers' or after existing swap chain was updated via 'vkCreateSwapchainKHR'.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime, uint32_t width, uint32_t height)</c></para>
		/// </summary>
		resize,
		/// <summary>
		/// Called before 'IDirect3DDevice9::Present(Ex)', 'IDirect3DSwapChain9::Present', 'IDXGISwapChain::Present(1)', 'wglSwapBuffers' or 'vkQueuePresentKHR'.
		/// <para>Callback function signature: <c>void (api::command_queue *present_queue, api::effect_runtime *runtime)</c></para>
		/// </summary>
		present,

		/// <summary>
		/// Called right before ReShade effects are rendered.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime, api::command_list *cmd_list)</c></para>
		/// </summary>
		reshade_before_effects,
		/// <summary>
		/// Called right after ReShade effects were rendered.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime, api::command_list *cmd_list)</c></para>
		/// </summary>
		reshade_after_effects,

#ifdef RESHADE_ADDON
		max // Last value used internally by ReShade to determine number of events in this enum
#endif
	};

	template <addon_event ev>
	struct addon_event_traits;
	template <addon_event ev>
	class  addon_event_trampoline;
	template <typename F>
	class  addon_event_trampoline_data;
	template <typename R, typename... Args>
	class  addon_event_trampoline_data<R(Args...)>
	{
	public:
		inline R operator()(Args... args) {
			return (next_callback != last_callback ? *next_callback++ : term_callback)(*this, std::forward<Args>(args)...);
		}

	protected:
		using callback_type = R(*)(addon_event_trampoline_data &, Args...);

		callback_type *next_callback; // Pointer to the next function in the call chain to execute
		callback_type *last_callback; // Pointer to the last function in the call chain
		callback_type  term_callback; // Terminator function that is called by the last function in the chain
	};

	// Define callback event
#define DEFINE_ADDON_EVENT_TYPE_1(ev, ...) \
	template <> \
	struct addon_event_traits<ev> { \
		using decl = void(*)(__VA_ARGS__); \
		static const int type = 1; \
	}

	// Define action callback event (return value determines whether the action the event was triggered for should be skipped)
#define DEFINE_ADDON_EVENT_TYPE_2(ev, ...) \
	template <> \
	struct addon_event_traits<ev> { \
		using decl = bool(*)(__VA_ARGS__); \
		static const int type = 2; \
	}

	// Define event with a call chain (first argument points to the next function in the chain that should be called)
#define DEFINE_ADDON_EVENT_TYPE_3(ev, ...) \
	template <> \
	struct addon_event_traits<ev> { \
		using decl = bool(*)(addon_event_trampoline<ev> &next, __VA_ARGS__); \
		static const int type = 3; \
	}; \
	template <> \
	class  addon_event_trampoline<ev> : public addon_event_trampoline_data<bool(__VA_ARGS__)> {}

	DEFINE_ADDON_EVENT_TYPE_1(addon_event::init_device, api::device *device);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::destroy_device, api::device *device);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::init_command_list, api::command_list *cmd_list);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::destroy_command_list, api::command_list *cmd_list);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::init_command_queue, api::command_queue *queue);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::destroy_command_queue, api::command_queue *queue);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::init_effect_runtime, api::effect_runtime *runtime);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::destroy_effect_runtime, api::effect_runtime *runtime);

	DEFINE_ADDON_EVENT_TYPE_3(addon_event::create_resource, api::device *device, const api::resource_desc &desc, const api::mapped_subresource *initial_data, api::resource_usage initial_state);
	DEFINE_ADDON_EVENT_TYPE_3(addon_event::create_resource_view, api::device *device, api::resource_handle resource, api::resource_usage usage_type, const api::resource_view_desc &desc);
	DEFINE_ADDON_EVENT_TYPE_3(addon_event::create_shader_module, api::device *device, const void *code, size_t code_size);

	DEFINE_ADDON_EVENT_TYPE_1(addon_event::set_index_buffer, api::command_list *cmd_list, api::resource_handle buffer, uint32_t format, uint64_t offset);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::set_vertex_buffers, api::command_list *cmd_list, uint32_t first, uint32_t count, const api::resource_handle *buffers, const uint32_t *strides, const uint64_t *offsets);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::set_viewports, api::command_list *cmd_list, uint32_t first, uint32_t count, const float *viewports);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::set_scissor_rects, api::command_list *cmd_list, uint32_t first, uint32_t count, const int32_t *rects);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::set_render_targets_and_depth_stencil, api::command_list *cmd_list, uint32_t count, const api::resource_view_handle *rtvs, api::resource_view_handle dsv);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::set_pipeline_states, api::command_list *cmd_list, uint32_t count, const api::pipeline_state *states, const uint32_t *values);

	DEFINE_ADDON_EVENT_TYPE_2(addon_event::draw, api::command_list *cmd_list, uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance);
	DEFINE_ADDON_EVENT_TYPE_2(addon_event::draw_indexed, api::command_list *cmd_list, uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);
	DEFINE_ADDON_EVENT_TYPE_2(addon_event::dispatch, api::command_list *cmd_list, uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z);
	DEFINE_ADDON_EVENT_TYPE_2(addon_event::draw_or_dispatch_indirect, api::command_list *cmd_list, addon_event type, api::resource_handle buffer, uint64_t offset, uint32_t draw_count, uint32_t stride);
	DEFINE_ADDON_EVENT_TYPE_2(addon_event::clear_depth_stencil, api::command_list *cmd_list, api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil);
	DEFINE_ADDON_EVENT_TYPE_2(addon_event::clear_render_target, api::command_list *cmd_list, api::resource_view_handle rtv, const float color[4]);
	DEFINE_ADDON_EVENT_TYPE_2(addon_event::copy_resource, api::command_list *cmd_list, api::resource_handle src, api::resource_handle dst);
	DEFINE_ADDON_EVENT_TYPE_2(addon_event::copy_buffer_region, api::command_list *cmd_list, api::resource_handle src, uint64_t src_offset, api::resource_handle dst, uint64_t dst_offset, uint64_t size);
	DEFINE_ADDON_EVENT_TYPE_2(addon_event::copy_texture_region, api::command_list *cmd_list, api::resource_handle src, const api::copy_location &src_location, const int32_t src_box[6], api::resource_handle dst, const api::copy_location &dst_location, const int32_t dst_box[6]);
	DEFINE_ADDON_EVENT_TYPE_2(addon_event::update_resource_region, api::command_list *cmd_list, const api::mapped_subresource *data, api::resource_handle dst, const api::copy_location &dst_location, const int32_t dst_box[6]);

	DEFINE_ADDON_EVENT_TYPE_1(addon_event::reset_command_list, api::command_list *cmd_list);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::execute_command_list, api::command_queue *queue, api::command_list *cmd_list);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::execute_secondary_command_list, api::command_list *cmd_list, api::command_list *secondary_cmd_list);

	DEFINE_ADDON_EVENT_TYPE_1(addon_event::resize, api::effect_runtime *runtime, uint32_t width, uint32_t height);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::present, api::command_queue *present_queue, api::effect_runtime *runtime);

	DEFINE_ADDON_EVENT_TYPE_1(addon_event::reshade_before_effects, api::effect_runtime *runtime, api::command_list *cmd_list);
	DEFINE_ADDON_EVENT_TYPE_1(addon_event::reshade_after_effects, api::effect_runtime *runtime, api::command_list *cmd_list);

#undef DEFINE_ADDON_EVENT_TYPE_1
#undef DEFINE_ADDON_EVENT_TYPE_2
#undef DEFINE_ADDON_EVENT_TYPE_3
}
