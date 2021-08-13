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
		/// Called after 'IDirect3D9::CreateDevice(Ex)', 'IDirect3DDevice9::Reset(Ex)', 'D3D10CreateDevice(AndSwapChain)', 'D3D11CreateDevice(AndSwapChain)', 'D3D12CreateDevice', 'glMakeCurrent' or 'vkCreateDevice'.
		/// <para>Callback function signature: <c>void (api::device *device)</c></para>
		/// </summary>
		init_device,
		/// <summary>
		/// Called before 'IDirect3DDevice9::Reset(Ex)' and on device destruction, before last 'IDirect3DDevice9::Release', 'ID3D10Device::Release', 'ID3D11Device::Release', 'ID3D12Device::Release', 'wglDeleteContext' or 'vkDestroyDevice'.
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
		/// In case of D3D9, D3D10, D3D11 and OpenGL this is called during device initialization as well and behaves as if an implicit command queue and list was created.
		/// <para>Callback function signature: <c>void (api::command_queue *queue)</c></para>
		/// </summary>
		init_command_queue,
		/// <summary>
		/// Called on command queue destruction, before last 'ID3D12CommandQueue::Release' or 'vkDestroyDevice'.
		/// <para>Callback function signature: <c>void (api::command_queue *queue)</c></para>
		/// </summary>
		destroy_command_queue,

		/// <summary>
		/// Called after 'IDirect3D9::CreateDevice(Ex)' (for the implicit swap chain), 'IDirect3D9Device::CreateAdditionalSwapChain', 'IDirect3DDevice9::Reset(Ex)', 'IDXGIFactory(2)::CreateSwapChain(...)', 'IDXGISwapChain::ResizeBuffers(1)', 'glMakeCurrent' or 'vkCreateSwapchainKHR'.
		/// <para>Callback function signature: <c>void (api::swapchain *swapchain)</c></para>
		/// </summary>
		init_swapchain,
		/// <summary>
		/// Called before 'IDirect3D9::CreateDevice(Ex)' (for the implicit swap chain), 'IDirect3D9Device::CreateAdditionalSwapChain', 'IDirect3DDevice9::Reset(Ex)', 'IDXGIFactory(2)::CreateSwapChain(...)' or 'vkCreateSwapchainKHR'.
		/// <para>Callback function signature: <c>bool (api::resource_desc &amp;buffer_desc, void *hwnd)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the swap chain description, modify <c>buffer_desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_swapchain,
		/// <summary>
		/// Called before 'IDirect3DDevice9::Reset(Ex)', 'IDXGISwapChain::ResizeBuffers(1)' and on swapchain destruction, before last 'IDirect3DSwapChain9::Release', 'IDXGISwapChain::Release' or 'vkDestroySwapchainKHR'.
		/// <para>Callback function signature: <c>void (api::swapchain *swapchain)</c></para>
		/// </summary>
		destroy_swapchain,

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
		/// Called after successfull sampler creation from 'ID3D10Device::CreateSamplerState', 'ID3D11Device::CreateSamplerState', 'ID3D12Device::CreateSampler' or 'vkCreateSampler'.
		/// <para>Callback function signature: <c>void (api::device *device, const api::sampler_desc &amp;desc, api::sampler sampler)</c></para>
		/// </summary>
		init_sampler,
		/// <summary>
		/// Called before 'ID3D10Device::CreateSamplerState', 'ID3D11Device::CreateSamplerState', 'ID3D12Device::CreateSampler' or 'vkCreateSampler'.
		/// <para>Callback function signature: <c>bool (api::device *device, api::sampler_desc &amp;desc)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the sampler description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Is not called in D3D9 (since samplers are loose state there).
		/// </remarks>
		create_sampler,
		/// <summary>
		/// Called on sampler destruction, before last 'ID3D10SamplerState::Release', 'ID3D11SamplerState::Release', 'glDeleteSamplers' or 'vkDestroySampler'.
		/// <para>Callback function signature: <c>void (api::device *device, api::sampler sampler)</c></para>
		/// </summary>
		/// <remarks>
		/// Is not called in D3D12 (since samplers are descriptor handles instead of objects there).
		/// </remarks>
		destroy_sampler,

		/// <summary>
		/// Called after successfull resource creation from 'IDirect3Device9::Create(...)(Buffer/Texture/Surface(Ex))', 'ID3D10Device::Create(Buffer/Texture(...))', 'ID3D11Device::Create(Buffer/Texture(...))', 'ID3D12Device::Create(...)Resource(...)', 'gl(Named)(Buffer/Renderbuffer/Tex(ture))Storage(...)(Multisample)' or 'vkCreate(Buffer/Image)'.
		/// <para>Callback function signature: <c>void (api::device *device, const api::resource_desc &amp;desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource resource)</c></para>
		/// </summary>
		init_resource,
		/// <summary>
		/// Called before 'IDirect3Device9::Create(...)(Buffer/Texture/Surface(Ex))', 'ID3D10Device::Create(Buffer/Texture(...))', 'ID3D11Device::Create(Buffer/Texture(...))', 'ID3D12Device::Create(...)Resource(...)', 'gl(Named)(Buffer/Renderbuffer/Tex(ture))Storage(...)(Multisample)' or 'vkCreate(Buffer/Image)'.
		/// <para>Callback function signature: <c>bool (api::device *device, api::resource_desc &amp;desc, api::subresource_data *initial_data, api::resource_usage initial_state)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the resource description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_resource,
		/// <summary>
		/// Called on resource destruction, before last 'IDirect3DResource9::Release', 'ID3D10Resource::Release', 'ID3D11Resource::Release', 'ID3D12Resource::Release', 'glDelete(Buffers/Textures)' or 'vkDestroy(Buffer/Image)'.
		/// <para>Callback function signature: <c>void (api::device *device, api::resource resource)</c></para>
		/// </summary>
		destroy_resource,

		/// <summary>
		/// Called after successfull resource view creation from 'IDirect3DDevice9::Create(...)(Texture/Surface(Ex))', 'ID3D10Device::Create(...)View(1)', 'ID3D11Device::Create(...)View(1)', 'ID3D12Device::Create(...)View', 'glTex(ture)Buffer', 'glTextureView(...)' or 'vkCreate(Buffer/Image)View'.
		/// <para>Callback function signature: <c>void (api::device *device, api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &amp;desc, api::resource_view view)</c></para>
		/// </summary>
		init_resource_view,
		/// <summary>
		/// Called before 'ID3D10Device::Create(...)View(1)', 'ID3D11Device::Create(...)View(1)', 'ID3D12Device::Create(...)View', 'glTex(ture)Buffer', 'glTextureView(...)' or 'vkCreate(Buffer/Image)View'.
		/// <para>Callback function signature: <c>bool (api::device *device, api::resource resource, api::resource_usage usage_type, api::resource_view_desc &amp;desc)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the resource view description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Is not called in D3D9 (since resource views are tied to resources there).
		/// </remarks>
		create_resource_view,
		/// <summary>
		/// Called on resource view destruction, before last 'IDirect3DSurface9::Release', 'ID3D10View::Release', 'ID3D11View::Release' or 'vkDestroyBuffer/ImageView'.
		/// <para>Callback function signature: <c>void (api::device *device, api::resource_view view)</c></para>
		/// </summary>
		/// <remarks>
		/// Is not called in D3D12 (since resource views are descriptor handles instead of objects there).
		/// </remarks>
		destroy_resource_view,

		/// <summary>
		/// Called after successfull pipeline creation from 'IDirect3DDevice9::Create(...)(Shader/VertexDeclaration)', 'ID3D10Device::Create(...)(Shader/State)', 'ID3D11Device::Create(...)(Shader/State)', 'ID3D12Device::Create(...)PipelineState', 'glLinkProgram' or 'vkCreate(...)Pipelines'.
		/// <para>Callback function signature: <c>void (api::device *device, const api::pipeline_desc &amp;desc, api::pipeline pipeline)</c></para>
		/// </summary>
		/// <remarks>
		/// May be called multiple times with the same pipeline handle (whenever the pipeline is updated or its reference count is incremented).
		/// </remarks>
		init_pipeline,
		/// <summary>
		/// Called before 'IDirect3DDevice9::Create(...)(Shader/VertexDeclaration)', 'ID3D10Device::Create(...)(Shader/State)', 'ID3D11Device::Create(...)(Shader/State)', 'ID3D12Device::Create(...)PipelineState', 'glShaderSource' or 'vkCreate(...)Pipelines'.
		/// <para>Callback function signature: <c>bool (api::device *device, api::pipeline_desc &amp;desc)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the pipeline description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_pipeline,
		/// <summary>
		/// Called on pipeline destruction, before last 'IUnknown::Release', 'glDeleteProgram' or 'vkDestroyPipeline'.
		/// <para>Callback function signature: <c>bool (api::device *device, api::pipeline pipeline)</c></para>
		/// </summary>
		/// <remarks>
		/// Is not called in D3D9.
		/// </remarks>
		destroy_pipeline,

		/// <summary>
		/// Called after successfull render pass creation from 'vkCreateRenderPass'.
		/// <para>Callback function signature: <c>void (api::device *device, const api::render_pass_desc &amp;desc, api::render_pass pass)</c></para>
		/// </summary>
		init_render_pass,
		/// <summary>
		/// Called before 'vkCreateRenderPass'.
		/// <para>Callback function signature: <c>bool (api::device *device, api::render_pass_desc &amp;desc)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the render pass description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_render_pass,
		/// <summary>
		/// Called on render pass destruction, before 'vkDestroyRenderPass'.
		/// <para>Callback function signature: <c>void (api::device *device, api::render_pass pass)</c></para>
		/// </summary>
		destroy_render_pass,

		/// <summary>
		/// Called after successfull framebuffer object creation from 'gl(Named)Framebuffer(Renderbuffer/Texture)(...)' or 'vkCreateFramebuffer'.
		/// <para>Callback function signature: <c>void (api::device *device, const api::framebuffer_desc &amp;desc, api::framebuffer fbo)</c></para>
		/// </summary>
		/// <remarks>
		/// May be called multiple times with the same framebuffer handle (whenever the framebuffer object is updated).
		/// </remarks>
		init_framebuffer,
		/// <summary>
		/// Called before 'gl(Named)Framebuffer(Renderbuffer/Texture)(...)' or 'vkCreateFramebuffer'.
		/// <para>Callback function signature: <c>bool (api::device *device, api::framebuffer_desc &amp;desc)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the framebuffer description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_framebuffer,
		/// <summary>
		/// Called on framebuffer object destruction, before 'glDeleteFramebuffers' or 'vkDestroyFramebuffer'.
		/// <para>Callback function signature: <c>void (api::device *device, api::framebuffer fbo)</c></para>
		/// </summary>
		destroy_framebuffer,

		/// <summary>
		/// Called before 'ID3D10Device::UpdateSubresource' or 'ID3D11DeviceContext::UpdateSubresource'.
		/// <para>Callback function signature: <c>bool (api::device *device, const void *data, api::resource dst, uint64_t dst_offset, uint64_t size)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Destination resource will be in the <see cref="resource_usage::copy_dest"/> state.
		/// </remarks>
		upload_buffer_region,
		/// <summary>
		/// Called before 'ID3D10Device::UpdateSubresource' or 'ID3D11DeviceContext::UpdateSubresource'.
		/// <para>Callback function signature: <c>bool (api::device *device, const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Destination resource will be in the <see cref="resource_usage::copy_dest"/> state.
		/// </remarks>
		upload_texture_region,

		/// <summary>
		/// Called before 'ID3D12Device::Create(...)View', 'ID3D12Device::CopyDescriptors(Simple)' or 'vkUpdateDescriptorSets'.
		/// <para>Callback function signature: <c>bool (api::device *device, uint32_t num_writes, const api::write_descriptor_set *writes, uint32_t num_copies, const api::copy_descriptor_set *copies)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		update_descriptor_sets,

		/// <summary>
		/// Called after 'ID3D12GraphicsCommandList::ResourceBarrier' or 'vkCmdPipelineBarrier'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t num_resources, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states)</c></para>
		/// </summary>
		barrier,

		/// <summary>
		/// Called after 'ID3D12GraphicsCommandList::BeginRenderPass', 'vkCmdBeginRenderPass' or when OpenGL FBO changed.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::render_pass pass, api::framebuffer fbo)</c></para>
		/// </summary>
		begin_render_pass,
		/// <summary>
		/// Called after 'ID3D12GraphicsCommandList::EndRenderPass', 'vkCmdEndRenderPass' or when OpenGL FBO changed.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		finish_render_pass,
		/// <summary>
		/// Called after 'IDirect3DDevice9::SetRenderTarget', 'IDirect3DDevice9::SetDepthStencilSurface', 'ID3D10Device::OMSetRenderTargets', 'ID3D11DeviceContext::OMSetRenderTargets(AndUnorderedAccessViews)' or 'ID3D12GraphicsCommandList::OMSetRenderTargets'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)</c></para>
		/// </summary>
		bind_render_targets_and_depth_stencil,

		/// <summary>
		/// Called after 'IDirect3DDevice9::Set(...)Shader', 'ID3D10Device::(...)SetShader', 'ID3D11DeviceContext::(...)SetShader', 'ID3D12GraphicsCommandList::SetPipelineState', 'glUseProgram' or 'vkCmdBindPipeline'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::pipeline_stage type, api::pipeline pipeline)</c></para>
		/// </summary>
		bind_pipeline,
		/// <summary>
		/// Called after 'IDirect3DDevice9::SetRenderState', 'ID3D10Device::(...)Set(...)State', 'ID3D11DeviceContext::(...)Set(...)State', 'ID3D12GraphicsCommandList::(...)Set(...)', 'gl(...)', 'vkCmdSet(...)' or 'vkCmdBindPipeline'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t count, const api::dynamic_state *states, const uint32_t *values)</c></para>
		/// </summary>
		bind_pipeline_states,
		/// <summary>
		/// Called after 'IDirect3DDevice9::SetViewport', 'IDirect3DDevice9::SetRenderTarget' (which implicitly sets the viewport), 'ID3D10Device::RSSetViewports', 'ID3D11DeviceContext::RSSetViewports', 'ID3D12GraphicsCommandList::RSSetViewports', 'glViewport(...)' or 'vkCmdSetViewport'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const float *viewports)</c></para>
		/// </summary>
		/// <remarks>
		/// Viewport data format is { viewport[0].x, viewport[0].y, viewport[0].width, viewport[0].height, viewport[0].min_depth, viewport[0].max_depth, viewport[1].x, viewport[1].y, ... }.
		/// </remarks>
		bind_viewports,
		/// <summary>
		/// Called after 'IDirect3DDevice9::SetScissorRect', 'ID3D10Device::RSSetScissorRects', 'ID3D11DeviceContext::RSSetScissorRects', 'ID3D12GraphicsCommandList::RSSetScissorRects', 'glScissor(...)' or 'vkCmdSetScissor'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// Rectangle data format is { rect[0].left, rect[0].top, rect[0].right, rect[0].bottom, rect[1].left, rect[1].right, ... }.
		/// </remarks>
		bind_scissor_rects,

		/// <summary>
		/// Called after 'IDirect3DDevice9::Set(...)ShaderConstant(...)', 'ID3D12GraphicsCommandList::Set(...)Root32BitConstant(s)', 'glUniform(...)' or 'vkCmdPushConstants'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const uint32_t *values)</c></para>
		/// </summary>
		push_constants,
		/// <summary>
		/// Called after 'IDirect3DDevice9::Set(SamplerState/Texture)', 'ID3D10Device::(...)Set(Samplers/ShaderResources/ConstantBuffers)', 'ID3D11DeviceContext::(...)Set(Samplers/ShaderResources/UnorderedAccessViews/ConstantBuffers)', 'ID3D12GraphicsCommandList::Set(...)RootConstantBufferView' or 'glBind(Sampler/Texture/TextureUnit/ImageTexture/Buffer)(s)(...)'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)</c></para>
		/// </summary>
		push_descriptors,
		/// <summary>
		/// Called after 'ID3D12GraphicsCommandList::Set(...)RootDescriptorTable' or 'vkCmdBindDescriptorSets'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets, const uint32_t *offsets)</c></para>
		/// </summary>
		bind_descriptor_sets,

		/// <summary>
		/// Called after 'IDirect3DDevice9::SetIndices', 'ID3D10Device::IASetIndexBuffer', 'ID3D11DeviceContext::IASetIndexBuffer', 'ID3D12GraphicsCommandList::IASetIndexBuffer', 'glBindBuffer' or 'vkCmdBindIndexBuffer'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::resource buffer, uint64_t offset, uint32_t index_size)</c></para>
		/// </summary>
		bind_index_buffer,
		/// <summary>
		/// Called after 'IDirect3DDevice9::SetStreamSource', 'ID3D10Device::IASetVertexBuffers', 'ID3D11DeviceContext::IASetVertexBuffers', 'ID3D12GraphicsCommandList::IASetVertexBuffers', 'glBindBuffer', 'glBindVertexBuffer(s)' or 'vkCmdBindVertexBuffers'.
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)</c></para>
		/// </summary>
		bind_vertex_buffers,

		/// <summary>
		/// Called before 'IDirect3DDevice9::DrawPrimitive(UP)', 'ID3D10Device::Draw(Instanced)', 'ID3D11DeviceContext::Draw(Instanced)', 'ID3D12GraphicsCommandList::DrawInstanced', 'gl(Multi)DrawArrays(...)' or 'vkCmdDraw'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		draw,
		/// <summary>
		/// Called before 'IDirect3DDevice9::DrawIndexedPrimitive(UP)', 'ID3D10Device::DrawIndexed(Instanced)', 'ID3D11DeviceContext::DrawIndexed(Instanced)', 'ID3D12GraphicsCommandList::DrawIndexedInstanced', 'gl(Multi)DrawElements(...)' or 'vkCmdDrawIndexed'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		draw_indexed,
		/// <summary>
		/// Called before 'ID3D11DeviceContext::Dispatch', 'ID3D12GraphicsCommandList::Dispatch', 'glDispatchCompute' or 'vkCmdDispatch'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		dispatch,
		/// <summary>
		/// Called before 'ID3D11DeviceContext::Draw(Indexed)InstancedIndirect', 'ID3D11DeviceContext::DispatchIndirect', 'ID3D12GraphicsCommandList::ExecuteIndirect', 'gl(Multi)Draw(...)Indirect', 'glDispatchComputeIndirect', 'vkCmdDraw(Indexed)Indirect' or 'vkCmdDispatchIndirect'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		draw_or_dispatch_indirect,

		/// <summary>
		/// Called before 'IDirect3DDevice9::UpdateTexture', 'IDirect3DDevice9::GetRenderTargetData', 'ID3D10Device::CopyResource', 'ID3D11DeviceContext::CopyResource' or 'ID3D12GraphicsCommandList::CopyResource'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource src, api::resource dst)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="resource_usage::copy_source"/> state. Destination resource will be in the <see cref="resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_resource,
		/// <summary>
		/// Called before 'ID3D12GraphicsCommandList::CopyBufferRegion', 'glCopy(Named)BufferSubData' or 'vkCmdCopyBuffer'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="resource_usage::copy_source"/> state. Destination resource will be in the <see cref="resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_buffer_region,
		/// <summary>
		/// Called before 'ID3D12GraphicsCommandList::CopyTextureRegion' or 'vkCmdCopyBufferToImage'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="resource_usage::copy_source"/> state. Destination resource will be in the <see cref="resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_buffer_to_texture,
		/// <summary>
		/// Called before 'IDirect3DDevice9::UpdateSurface', 'IDirect3DDevice9::StretchRect', 'ID3D10Device::CopySubresourceRegion', 'ID3D11DeviceContext::CopySubresourceRegion', 'ID3D12GraphicsCommandList::CopyTextureRegion', 'glBlit(Named)Framebuffer', 'glCopyImageSubData', 'glCopyTex(ture)(Sub)Image(...)', 'vkCmdBlitImage' or 'vkCmdCopyImage'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::filter_type filter)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="resource_usage::copy_source"/> state. Destination resource will be in the <see cref="resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_texture_region,
		/// <summary>
		/// Called before 'ID3D12GraphicsCommandList::CopyTextureRegion' or 'vkCmdCopyImageToBuffer'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="resource_usage::copy_source"/> state. Destination resource will be in the <see cref="resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_texture_to_buffer,
		/// <summary>
		/// Called before 'IDirect3DDevice9::StretchRect', 'ID3D10Device::ResolveSubresource', 'ID3D11DeviceContext::ResolveSubresource', 'ID3D12GraphicsCommandList::ResolveSubresource', 'ID3D12GraphicsCommandList1::ResolveSubresourceRegion', 'glBlit(Named)Framebuffer' or 'vkCmdResolveImage'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], api::format format)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="resource_usage::resolve_source"/> state. Destination resource will be in the <see cref="resource_usage::resolve_dest"/> state.
		/// </remarks>
		resolve_texture_region,

		/// <summary>
		/// Called before 'IDirect3DDevice9::Clear', 'glClear' or 'vkCmdClearAttachments'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::attachment_type clear_flags, const float color[4], float depth, uint8_t stencil, uint32_t num_rects, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		clear_attachments,
		/// <summary>
		/// Called before 'ID3D10Device::ClearDepthStencilView', 'ID3D11DeviceContext::ClearDepthStencilView', 'ID3D11DeviceContext1::ClearView', 'ID3D12GraphicsCommandList::ClearDepthStencilView', 'ID3D12GraphicsCommandList::BeginRenderPass', 'glClear(NamedFrame)Bufferfi', 'vkCmdClearDepthStencilImage' or 'vkCmdBeginRenderPass'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view dsv, api::attachment_type clear_flags, float depth, uint8_t stencil, uint32_t num_rects, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resource will be in the <see cref="resource_usage::depth_stencil_write"/> state.
		/// </remarks>
		clear_depth_stencil_view,
		/// <summary>
		/// Called before 'IDirect3DDevice9::ColorFill', 'ID3D10Device::ClearRenderTargetView', 'ID3D11DeviceContext::ClearRenderTargetView', 'ID3D11DeviceContext1::ClearView', 'ID3D12GraphicsCommandList::ClearRenderTargetView', 'ID3D12GraphicsCommandList::BeginRenderPass', 'glClear(NamedFrame)Bufferfv', 'vkCmdClearColorImage' or 'vkCmdBeginRenderPass'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view rtv, const float color[4], uint32_t num_rects, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resources will be in the <see cref="resource_usage::render_target"/> state.
		/// </remarks>
		clear_render_target_view,
		/// <summary>
		/// Called before 'ID3D11DeviceContext::ClearUnorderedAccessViewUint' or 'ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view uav, const uint32_t values[4], uint32_t num_rects, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resource will be in the <see cref="resource_usage::unordered_access"/> state.
		/// </remarks>
		clear_unordered_access_view_uint,
		/// <summary>
		/// Called before 'ID3D11DeviceContext::ClearUnorderedAccessViewFloat', 'ID3D11DeviceContext1::ClearView' or 'ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view uav, const float values[4], uint32_t num_rects, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resource will be in the <see cref="resource_usage::unordered_access"/> state.
		/// </remarks>
		clear_unordered_access_view_float,

		/// <summary>
		/// Called before 'ID3D10Device::GenerateMips', 'ID3D11DeviceContext::GenerateMips' or 'glGenerate(Texture)Mipmap'.
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view srv)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		generate_mipmaps,

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
		/// Called before 'IDirect3DDevice9::Present(Ex)', 'IDirect3DSwapChain9::Present', 'IDXGISwapChain::Present(1)', 'D3D12CommandQueueDownlevel::Present', 'wglSwapBuffers' or 'vkQueuePresentKHR'.
		/// <para>Callback function signature: <c>void (api::command_queue *queue, api::swapchain *swapchain)</c></para>
		/// </summary>
		present,

		/// <summary>
		/// Called right before ReShade effects are rendered.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime, api::command_list *cmd_list)</c></para>
		/// </summary>
		reshade_begin_effects,
		/// <summary>
		/// Called right after ReShade effects were rendered.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime, api::command_list *cmd_list)</c></para>
		/// </summary>
		reshade_finish_effects,

#ifdef RESHADE_ADDON
		max // Last value used internally by ReShade to determine number of events in this enum
#endif
	};

	template <addon_event ev>
	struct addon_event_traits;

#define DEFINE_ADDON_EVENT_TYPE(ev, ret, ...) \
	template <> \
	struct addon_event_traits<ev> { \
		using decl = ret(*)(__VA_ARGS__); \
		using type = ret; \
	}

	DEFINE_ADDON_EVENT_TYPE(addon_event::init_device, void, api::device *device);
	DEFINE_ADDON_EVENT_TYPE(addon_event::destroy_device, void, api::device *device);

	DEFINE_ADDON_EVENT_TYPE(addon_event::init_command_list, void, api::command_list *cmd_list);
	DEFINE_ADDON_EVENT_TYPE(addon_event::destroy_command_list, void, api::command_list *cmd_list);

	DEFINE_ADDON_EVENT_TYPE(addon_event::init_command_queue, void, api::command_queue *queue);
	DEFINE_ADDON_EVENT_TYPE(addon_event::destroy_command_queue, void, api::command_queue *queue);

	DEFINE_ADDON_EVENT_TYPE(addon_event::init_swapchain, void, api::swapchain *swapchain);
	DEFINE_ADDON_EVENT_TYPE(addon_event::create_swapchain, bool, api::resource_desc &buffer_desc, void *hwnd);
	DEFINE_ADDON_EVENT_TYPE(addon_event::destroy_swapchain, void, api::swapchain *swapchain);

	DEFINE_ADDON_EVENT_TYPE(addon_event::init_effect_runtime, void, api::effect_runtime *runtime);
	DEFINE_ADDON_EVENT_TYPE(addon_event::destroy_effect_runtime, void, api::effect_runtime *runtime);

	DEFINE_ADDON_EVENT_TYPE(addon_event::init_sampler, void, api::device *device, const api::sampler_desc &desc, api::sampler sampler);
	DEFINE_ADDON_EVENT_TYPE(addon_event::create_sampler, bool, api::device *device, api::sampler_desc &desc);
	DEFINE_ADDON_EVENT_TYPE(addon_event::destroy_sampler, void, api::device *device, api::sampler sampler);

	DEFINE_ADDON_EVENT_TYPE(addon_event::init_resource, void, api::device *device, const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource resource);
	DEFINE_ADDON_EVENT_TYPE(addon_event::create_resource, bool, api::device *device, api::resource_desc &desc, api::subresource_data *initial_data, api::resource_usage initial_state);
	DEFINE_ADDON_EVENT_TYPE(addon_event::destroy_resource, void, api::device *device, api::resource resource);

	DEFINE_ADDON_EVENT_TYPE(addon_event::init_resource_view, void, api::device *device, api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view view);
	DEFINE_ADDON_EVENT_TYPE(addon_event::create_resource_view, bool, api::device *device, api::resource resource, api::resource_usage usage_type, api::resource_view_desc &desc);
	DEFINE_ADDON_EVENT_TYPE(addon_event::destroy_resource_view, void, api::device *device, api::resource_view view);

	DEFINE_ADDON_EVENT_TYPE(addon_event::init_pipeline, void, api::device *device, const api::pipeline_desc &desc, api::pipeline pipeline);
	DEFINE_ADDON_EVENT_TYPE(addon_event::create_pipeline, bool, api::device *device, api::pipeline_desc &desc);
	DEFINE_ADDON_EVENT_TYPE(addon_event::destroy_pipeline, void, api::device *device, api::pipeline pipeline);

	DEFINE_ADDON_EVENT_TYPE(addon_event::init_render_pass, void, api::device *device, const api::render_pass_desc &desc, api::render_pass pass);
	DEFINE_ADDON_EVENT_TYPE(addon_event::create_render_pass, bool, api::device *device, api::render_pass_desc &desc);
	DEFINE_ADDON_EVENT_TYPE(addon_event::destroy_render_pass, void, api::device *device, api::render_pass pass);

	DEFINE_ADDON_EVENT_TYPE(addon_event::init_framebuffer, void, api::device *device, const api::framebuffer_desc &desc, api::framebuffer fbo);
	DEFINE_ADDON_EVENT_TYPE(addon_event::create_framebuffer, bool, api::device *device, api::framebuffer_desc &desc);
	DEFINE_ADDON_EVENT_TYPE(addon_event::destroy_framebuffer, void, api::device *device, api::framebuffer fbo);

	DEFINE_ADDON_EVENT_TYPE(addon_event::upload_buffer_region, bool, api::device *device, const void *data, api::resource dst, uint64_t dst_offset, uint64_t size);
	DEFINE_ADDON_EVENT_TYPE(addon_event::upload_texture_region, bool, api::device *device, const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]);

	DEFINE_ADDON_EVENT_TYPE(addon_event::update_descriptor_sets, bool, api::device *device, uint32_t num_writes, const api::write_descriptor_set *writes, uint32_t num_copies, const api::copy_descriptor_set *copies);

	DEFINE_ADDON_EVENT_TYPE(addon_event::barrier, void, api::command_list *cmd_list, uint32_t num_resources, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states);

	DEFINE_ADDON_EVENT_TYPE(addon_event::begin_render_pass, void, api::command_list *cmd_list, api::render_pass pass, api::framebuffer fbo);
	DEFINE_ADDON_EVENT_TYPE(addon_event::finish_render_pass, void, api::command_list *cmd_list);
	DEFINE_ADDON_EVENT_TYPE(addon_event::bind_render_targets_and_depth_stencil, void, api::command_list *cmd_list, uint32_t count, const api::resource_view *rtvs, api::resource_view dsv);

	DEFINE_ADDON_EVENT_TYPE(addon_event::bind_pipeline, void, api::command_list *cmd_list, api::pipeline_stage type, api::pipeline pipeline);
	DEFINE_ADDON_EVENT_TYPE(addon_event::bind_pipeline_states, void, api::command_list *cmd_list, uint32_t count, const api::dynamic_state *states, const uint32_t *values);
	DEFINE_ADDON_EVENT_TYPE(addon_event::bind_viewports, void, api::command_list *cmd_list, uint32_t first, uint32_t count, const float *viewports);
	DEFINE_ADDON_EVENT_TYPE(addon_event::bind_scissor_rects, void, api::command_list *cmd_list, uint32_t first, uint32_t count, const int32_t *rects);
	DEFINE_ADDON_EVENT_TYPE(addon_event::push_constants, void, api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const uint32_t *values);
	DEFINE_ADDON_EVENT_TYPE(addon_event::push_descriptors, void, api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors);
	DEFINE_ADDON_EVENT_TYPE(addon_event::bind_descriptor_sets, void, api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets, const uint32_t *offsets);
	DEFINE_ADDON_EVENT_TYPE(addon_event::bind_index_buffer, void, api::command_list *cmd_list, api::resource buffer, uint64_t offset, uint32_t index_size);
	DEFINE_ADDON_EVENT_TYPE(addon_event::bind_vertex_buffers, void, api::command_list *cmd_list, uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides);

	DEFINE_ADDON_EVENT_TYPE(addon_event::draw, bool, api::command_list *cmd_list, uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance);
	DEFINE_ADDON_EVENT_TYPE(addon_event::draw_indexed, bool, api::command_list *cmd_list, uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);
	DEFINE_ADDON_EVENT_TYPE(addon_event::dispatch, bool, api::command_list *cmd_list, uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z);
	DEFINE_ADDON_EVENT_TYPE(addon_event::draw_or_dispatch_indirect, bool, api::command_list *cmd_list, api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride);

	DEFINE_ADDON_EVENT_TYPE(addon_event::copy_resource, bool, api::command_list *cmd_list, api::resource src, api::resource dst);
	DEFINE_ADDON_EVENT_TYPE(addon_event::copy_buffer_region, bool, api::command_list *cmd_list, api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size);
	DEFINE_ADDON_EVENT_TYPE(addon_event::copy_buffer_to_texture, bool, api::command_list *cmd_list, api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]);
	DEFINE_ADDON_EVENT_TYPE(addon_event::copy_texture_region, bool, api::command_list *cmd_list, api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::filter_type filter);
	DEFINE_ADDON_EVENT_TYPE(addon_event::copy_texture_to_buffer, bool, api::command_list *cmd_list, api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height);
	DEFINE_ADDON_EVENT_TYPE(addon_event::resolve_texture_region, bool, api::command_list *cmd_list, api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], api::format format);

	DEFINE_ADDON_EVENT_TYPE(addon_event::clear_attachments, bool, api::command_list *cmd_list, api::attachment_type clear_flags, const float color[4], float depth, uint8_t stencil, uint32_t num_rects, const int32_t *rects);
	DEFINE_ADDON_EVENT_TYPE(addon_event::clear_depth_stencil_view, bool, api::command_list *cmd_list, api::resource_view dsv, api::attachment_type clear_flags, float depth, uint8_t stencil, uint32_t num_rects, const int32_t *rects);
	DEFINE_ADDON_EVENT_TYPE(addon_event::clear_render_target_view, bool, api::command_list *cmd_list, api::resource_view rtv, const float color[4], uint32_t num_rects, const int32_t *rects);
	DEFINE_ADDON_EVENT_TYPE(addon_event::clear_unordered_access_view_uint, bool, api::command_list *cmd_list, api::resource_view uav, const uint32_t values[4], uint32_t num_rects, const int32_t *rects);
	DEFINE_ADDON_EVENT_TYPE(addon_event::clear_unordered_access_view_float, bool, api::command_list *cmd_list, api::resource_view uav, const float values[4], uint32_t num_rects, const int32_t *rects);

	DEFINE_ADDON_EVENT_TYPE(addon_event::generate_mipmaps, bool, api::command_list *cmd_list, api::resource_view srv);

	DEFINE_ADDON_EVENT_TYPE(addon_event::reset_command_list, void, api::command_list *cmd_list);
	DEFINE_ADDON_EVENT_TYPE(addon_event::execute_command_list, void, api::command_queue *queue, api::command_list *cmd_list);
	DEFINE_ADDON_EVENT_TYPE(addon_event::execute_secondary_command_list, void, api::command_list *cmd_list, api::command_list *secondary_cmd_list);

	DEFINE_ADDON_EVENT_TYPE(addon_event::present, void, api::command_queue *queue, api::swapchain *swapchain);

	DEFINE_ADDON_EVENT_TYPE(addon_event::reshade_begin_effects, void, api::effect_runtime *runtime, api::command_list *cmd_list);
	DEFINE_ADDON_EVENT_TYPE(addon_event::reshade_finish_effects, void, api::effect_runtime *runtime, api::command_list *cmd_list);

#undef DEFINE_ADDON_EVENT_TYPE
}
