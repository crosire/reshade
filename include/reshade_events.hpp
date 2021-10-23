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
		/// Called after:
		/// - IDirect3D9::CreateDevice
		/// - IDirect3D9Ex::CreateDeviceEx
		/// - IDirect3DDevice9::Reset
		/// - IDirect3DDevice9Ex::ResetEx
		/// - D3D10CreateDevice
		/// - D3D10CreateDevice1
		/// - D3D10CreateDeviceAndSwapChain
		/// - D3D10CreateDeviceAndSwapChain1
		/// - D3D11CreateDevice
		/// - D3D11CreateDeviceAndSwapChain
		/// - D3D12CreateDevice
		/// - glMakeCurrent
		/// - vkCreateDevice
		/// 
		/// <para>Callback function signature: <c>void (api::device *device)</c></para>
		/// </summary>
		init_device,

		/// <summary>
		/// Called on device destruction, before:
		/// - IDirect3DDevice9::Reset
		/// - IDirect3DDevice9Ex::ResetEx
		/// - IDirect3DDevice9::Release
		/// - ID3D10Device::Release
		/// - ID3D11Device::Release
		/// - ID3D12Device::Release
		/// - wglDeleteContext
		/// - vkDestroyDevice
		/// 
		/// <para>Callback function signature: <c>void (api::device *device)</c></para>
		/// </summary>
		destroy_device,

		/// <summary>
		/// Called after:
		/// - ID3D11Device::CreateDeferredContext
		/// - ID3D11Device1::CreateDeferredContext1
		/// - ID3D11Device2::CreateDeferredContext2
		/// - ID3D11Device3::CreateDeferredContext3
		/// - ID3D12Device::CreateCommandList
		/// - ID3D12Device4::CreateCommandList1
		/// - vkAllocateCommandBuffers
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		/// <remarks>
		/// In case of D3D9, D3D10, D3D11 and OpenGL this is called during device initialization as well and behaves as if an implicit immediate command list was created.
		/// </remarks>
		init_command_list,

		/// <summary>
		/// Called on command list destruction, before:
		/// - ID3D11CommandList::Release
		/// - ID3D12CommandList::Release
		/// - vkFreeCommandBuffers
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		destroy_command_list,

		/// <summary>
		/// Called after:
		/// - ID3D12Device::CreateCommandQueue
		/// - vkCreateDevice (for every queue associated with the device)
		/// 
		/// <para>Callback function signature: <c>void (api::command_queue *queue)</c></para>
		/// </summary>
		/// <remarks>
		/// In case of D3D9, D3D10, D3D11 and OpenGL this is called during device initialization as well and behaves as if an implicit command queue was created.
		/// </remarks>
		init_command_queue,

		/// <summary>
		/// Called on command queue destruction, before:
		/// - ID3D12CommandQueue::Release
		/// - vkDestroyDevice (for every queue associated with the device)
		/// 
		/// <para>Callback function signature: <c>void (api::command_queue *queue)</c></para>
		/// </summary>
		destroy_command_queue,

		/// <summary>
		/// Called after:
		/// - IDirect3D9::CreateDevice (for the implicit swap chain)
		/// - IDirect3D9Ex::CreateDeviceEx (for the implicit swap chain)
		/// - IDirect3D9Device::CreateAdditionalSwapChain
		/// - IDirect3DDevice9::Reset (for the implicit swap chain)
		/// - IDirect3DDevice9Ex::ResetEx (for the implicit swap chain)
		/// - IDXGIFactory::CreateSwapChain
		/// - IDXGIFactory2::CreateSwapChain(...)
		/// - IDXGISwapChain::ResizeBuffers
		/// - IDXGISwapChain3::ResizeBuffers1
		/// - glMakeCurrent
		/// - vkCreateSwapchainKHR
		/// 
		/// <para>Callback function signature: <c>void (api::swapchain *swapchain)</c></para>
		/// </summary>
		init_swapchain,

		/// <summary>
		/// Called before:
		/// - IDirect3D9::CreateDevice (for the implicit swap chain)
		/// - IDirect3D9Ex::CreateDeviceEx (for the implicit swap chain)
		/// - IDirect3D9Device::CreateAdditionalSwapChain
		/// - IDXGIFactory::CreateSwapChain
		/// - IDXGIFactory2::CreateSwapChain(...)
		/// - glMakeCurrent
		/// - vkCreateSwapchainKHR
		/// 
		/// <para>Callback function signature: <c>bool (api::resource_desc &amp;buffer_desc, void *hwnd)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the swap chain description, modify <c>buffer_desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_swapchain,

		/// <summary>
		/// Called on swap chain destruction, before:
		/// - IDirect3DDevice9::Release (for the implicit swap chain)
		/// - IDirect3DSwapChain9::Release
		/// - IDXGISwapChain::Release
		/// - vkDestroySwapchainKHR
		/// 
		/// In addition, called when swap chain is reset, before:
		/// - IDirect3DDevice9::Reset (for the implicit swap chain)
		/// - IDirect3DDevice9Ex::ResetEx (for the implicit swap chain)
		/// - IDXGISwapChain::ResizeBuffers
		/// - IDXGISwapChain1::ResizeBuffers1
		/// 
		/// <para>Callback function signature: <c>void (api::swapchain *swapchain)</c></para>
		/// </summary>
		destroy_swapchain,

		/// <summary>
		/// Called after effect runtime initialization (which happens after swap chain creation or a swap chain buffer resize).
		/// 
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime)</c></para>
		/// </summary>
		init_effect_runtime,

		/// <summary>
		/// Called when an effect runtime is reset or destroyed.
		/// 
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime)</c></para>
		/// </summary>
		destroy_effect_runtime,

		/// <summary>
		/// Called after successfull sampler creation from:
		/// - ID3D10Device::CreateSamplerState
		/// - ID3D11Device::CreateSamplerState
		/// - ID3D12Device::CreateSampler
		/// - vkCreateSampler
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, const api::sampler_desc &amp;desc, api::sampler sampler)</c></para>
		/// </summary>
		init_sampler,

		/// <summary>
		/// Called before:
		/// - ID3D10Device::CreateSamplerState
		/// - ID3D11Device::CreateSamplerState
		/// - ID3D12Device::CreateSampler
		/// - vkCreateSampler
		/// 
		/// <para>Callback function signature: <c>bool (api::device *device, api::sampler_desc &amp;desc)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the sampler description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Is not called in D3D9 (since samplers are loose state there).
		/// </remarks>
		create_sampler,

		/// <summary>
		/// Called on sampler destruction, before:
		/// - ID3D10SamplerState::Release
		/// - ID3D11SamplerState::Release
		/// - glDeleteSamplers
		/// - vkDestroySampler
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, api::sampler sampler)</c></para>
		/// </summary>
		/// <remarks>
		/// Is not called in D3D12 (since samplers are descriptor handles instead of objects there).
		/// </remarks>
		destroy_sampler,

		/// <summary>
		/// Called after successfull resource creation from:
		/// - IDirect3DDevice9::CreateVertexBuffer
		/// - IDirect3DDevice9::CreateIndexBuffer
		/// - IDirect3DDevice9::CreateTexture
		/// - IDirect3DDevice9::CreateCubeTexture
		/// - IDirect3DDevice9::CreateVolumeTexture
		/// - IDirect3DDevice9::CreateRenderTargetSurface
		/// - IDirect3DDevice9::CreateDepthStencilSurface
		/// - IDirect3DDevice9::CreateOffscreenPlainSurface
		/// - IDirect3DDevice9Ex::CreateRenderTargetSurfaceEx
		/// - IDirect3DDevice9Ex::CreateDepthStencilSurfaceEx
		/// - IDirect3DDevice9Ex::CreateOffscreenPlainSurfaceEx
		/// - ID3D10Device::CreateBuffer
		/// - ID3D10Device::CreateTexture1D
		/// - ID3D10Device::CreateTexture2D
		/// - ID3D10Device::CreateTexture2D
		/// - ID3D11Device::CreateBuffer
		/// - ID3D11Device::CreateTexture1D
		/// - ID3D11Device::CreateTexture2D
		/// - ID3D11Device::CreateTexture3D
		/// - ID3D11Device3::CreateTexture2D
		/// - ID3D11Device3::CreateTexture3D
		/// - ID3D12Device::CreateCommittedResource
		/// - ID3D12Device::CreatePlacedResource
		/// - ID3D12Device::CreateReservedResource
		/// - ID3D12Device4::CreateCommittedResource1
		/// - ID3D12Device4::CreateReservedResource1
		/// - glBufferData
		/// - glBufferStorage
		/// - glNamedBufferData
		/// - glNamedBufferStorage
		/// - glTexImage1D
		/// - glTexImage2D
		/// - glTexImage2DMultisample
		/// - glTexImage3D
		/// - glTexImage3DMultisample
		/// - glCompressedTexImage1D
		/// - glCompressedTexImage2D
		/// - glCompressedTexImage3D
		/// - glTexStorage1D
		/// - glTexStorage2D
		/// - glTexStorage2DMultisample
		/// - glTexStorage3D
		/// - glTexStorage3DMultisample
		/// - glTextureStorage1D
		/// - glTextureStorage2D
		/// - glTextureStorage2DMultisample
		/// - glTextureStorage3D
		/// - glTextureStorage3DMultisample
		/// - glRenderbufferStorage
		/// - glRenderbufferStorageMultisample
		/// - glNamedRenderbufferStorage
		/// - glNamedRenderbufferStorageMultisample
		/// - vkCreateBuffer
		/// - vkCreateImage
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, const api::resource_desc &amp;desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource resource)</c></para>
		/// </summary>
		init_resource,

		/// <summary>
		/// Called before:
		/// - IDirect3DDevice9::CreateVertexBuffer
		/// - IDirect3DDevice9::CreateIndexBuffer
		/// - IDirect3DDevice9::CreateTexture
		/// - IDirect3DDevice9::CreateCubeTexture
		/// - IDirect3DDevice9::CreateVolumeTexture
		/// - IDirect3DDevice9::CreateRenderTargetSurface
		/// - IDirect3DDevice9::CreateDepthStencilSurface
		/// - IDirect3DDevice9::CreateOffscreenPlainSurface
		/// - IDirect3DDevice9Ex::CreateRenderTargetSurfaceEx
		/// - IDirect3DDevice9Ex::CreateDepthStencilSurfaceEx
		/// - IDirect3DDevice9Ex::CreateOffscreenPlainSurfaceEx
		/// - ID3D10Device::CreateBuffer
		/// - ID3D10Device::CreateTexture1D
		/// - ID3D10Device::CreateTexture2D
		/// - ID3D10Device::CreateTexture2D
		/// - ID3D11Device::CreateBuffer
		/// - ID3D11Device::CreateTexture1D
		/// - ID3D11Device::CreateTexture2D
		/// - ID3D11Device::CreateTexture3D
		/// - ID3D11Device3::CreateTexture2D
		/// - ID3D11Device3::CreateTexture3D
		/// - ID3D12Device::CreateCommittedResource
		/// - ID3D12Device::CreatePlacedResource
		/// - ID3D12Device::CreateReservedResource
		/// - ID3D12Device4::CreateCommittedResource1
		/// - ID3D12Device4::CreateReservedResource1
		/// - glBufferData
		/// - glBufferStorage
		/// - glNamedBufferData
		/// - glNamedBufferStorage
		/// - glTexImage1D
		/// - glTexImage2D
		/// - glTexImage2DMultisample
		/// - glTexImage3D
		/// - glTexImage3DMultisample
		/// - glCompressedTexImage1D
		/// - glCompressedTexImage2D
		/// - glCompressedTexImage3D
		/// - glTexStorage1D
		/// - glTexStorage2D
		/// - glTexStorage2DMultisample
		/// - glTexStorage3D
		/// - glTexStorage3DMultisample
		/// - glTextureStorage1D
		/// - glTextureStorage2D
		/// - glTextureStorage2DMultisample
		/// - glTextureStorage3D
		/// - glTextureStorage3DMultisample
		/// - glRenderbufferStorage
		/// - glRenderbufferStorageMultisample
		/// - glNamedRenderbufferStorage
		/// - glNamedRenderbufferStorageMultisample
		/// - vkCreateBuffer
		/// - vkCreateImage
		/// 
		/// <para>Callback function signature: <c>bool (api::device *device, api::resource_desc &amp;desc, api::subresource_data *initial_data, api::resource_usage initial_state)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the resource description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_resource,

		/// <summary>
		/// Called on resource destruction, before:
		/// - IDirect3DResource9::Release
		/// - ID3D10Resource::Release
		/// - ID3D11Resource::Release
		/// - ID3D12Resource::Release
		/// - glDeleteBuffers
		/// - glDeleteTextures
		/// - glDeleteRenderbuffers
		/// - vkDestroyBuffer
		/// - vkDestroyImage
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, api::resource resource)</c></para>
		/// </summary>
		destroy_resource,

		/// <summary>
		/// Called after successfull resource view creation from:
		/// - IDirect3DDevice9::CreateTexture
		/// - IDirect3DDevice9::CreateCubeTexture
		/// - IDirect3DDevice9::CreateVolumeTexture
		/// - ID3D10Device::CreateShaderResourceView
		/// - ID3D10Device::CreateRenderTargetView
		/// - ID3D10Device::CreateDepthStencilView
		/// - ID3D10Device1::CreateShaderResourceView1
		/// - ID3D11Device::CreateShaderResourceView
		/// - ID3D11Device::CreateUnorderedAccessView
		/// - ID3D11Device::CreateRenderTargetView
		/// - ID3D11Device::CreateDepthStencilView
		/// - ID3D11Device3::CreateShaderResourceView1
		/// - ID3D11Device3::CreateUnorderedAccessView1
		/// - ID3D11Device3::CreateRenderTargetView1
		/// - ID3D12Device::CreateShaderResourceView
		/// - ID3D12Device::CreateUnorderedAccessView
		/// - ID3D12Device::CreateRenderTargetView
		/// - ID3D12Device::CreateDepthStencilView
		/// - glTexBuffer
		/// - glTextureBuffer
		/// - glTextureView
		/// - vkCreateBufferView
		/// - vkCreateImageView
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &amp;desc, api::resource_view view)</c></para>
		/// </summary>
		init_resource_view,

		/// <summary>
		/// Called before:
		/// - ID3D10Device::CreateShaderResourceView
		/// - ID3D10Device::CreateRenderTargetView
		/// - ID3D10Device::CreateDepthStencilView
		/// - ID3D10Device1::CreateShaderResourceView1
		/// - ID3D11Device::CreateShaderResourceView
		/// - ID3D11Device::CreateUnorderedAccessView
		/// - ID3D11Device::CreateRenderTargetView
		/// - ID3D11Device::CreateDepthStencilView
		/// - ID3D11Device3::CreateShaderResourceView1
		/// - ID3D11Device3::CreateUnorderedAccessView1
		/// - ID3D11Device3::CreateRenderTargetView1
		/// - ID3D12Device::CreateShaderResourceView
		/// - ID3D12Device::CreateUnorderedAccessView
		/// - ID3D12Device::CreateRenderTargetView
		/// - ID3D12Device::CreateDepthStencilView
		/// - glTexBuffer
		/// - glTextureBuffer
		/// - glTextureView
		/// - vkCreateBufferView
		/// - vkCreateImageView
		/// 
		/// <para>Callback function signature: <c>bool (api::device *device, api::resource resource, api::resource_usage usage_type, api::resource_view_desc &amp;desc)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the resource view description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Is not called in D3D9 (since resource views are tied to resources there).
		/// </remarks>
		create_resource_view,

		/// <summary>
		/// Called on resource view destruction, before:
		/// - IDirect3DResource9::Release
		/// - ID3D10View::Release
		/// - ID3D11View::Release
		/// - glDeleteTextures
		/// - vkDestroyBufferView
		/// - vkDestroyImageView
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, api::resource_view view)</c></para>
		/// </summary>
		/// <remarks>
		/// Is not called in D3D12 (since resource views are descriptor handles instead of objects there).
		/// </remarks>
		destroy_resource_view,

		/// <summary>
		/// Called after successfull pipeline creation from:
		/// - IDirect3DDevice9::CreateVertexShader
		/// - IDirect3DDevice9::CreatePixelShader
		/// - IDirect3DDevice9::CreateVertexDeclaration
		/// - ID3D10Device::CreateVertexShader
		/// - ID3D10Device::CreateGeometryShader
		/// - ID3D10Device::CreateGeometryShaderWithStreamOutput
		/// - ID3D10Device::CreatePixelShader
		/// - ID3D10Device::CreateBlendState
		/// - ID3D10Device::CreateDepthStencilState
		/// - ID3D10Device::CreateRasterizerState
		/// - ID3D10Device1::CreateBlendState1
		/// - ID3D11Device::CreateVertexShader
		/// - ID3D11Device::CreateHullShader
		/// - ID3D11Device::CreateDomainShader
		/// - ID3D11Device::CreateGeometryShader
		/// - ID3D11Device::CreateGeometryShaderWithStreamOutput
		/// - ID3D11Device::CreatePixelShader
		/// - ID3D11Device::CreateComputeShader
		/// - ID3D11Device::CreateBlendState
		/// - ID3D11Device::CreateDepthStencilState
		/// - ID3D11Device::CreateRasterizerState
		/// - ID3D11Device1::CreateBlendState1
		/// - ID3D11Device1::CreateRasterizerState1
		/// - ID3D11Device3::CreateRasterizerState2
		/// - ID3D12Device::CreateComputePipelineState
		/// - ID3D12Device::CreateGraphicsPipelineState
		/// - glLinkProgram
		/// - vkCreateComputePipelines
		/// - vkCreateGraphicsPipelines
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, const api::pipeline_desc &amp;desc, uint32_t dynamic_state_count, const dynamic_state *dynamic_states, api::pipeline pipeline)</c></para>
		/// </summary>
		/// <remarks>
		/// May be called multiple times with the same pipeline handle (whenever the pipeline is updated or its reference count is incremented).
		/// </remarks>
		init_pipeline,

		/// <summary>
		/// Called before:
		/// - IDirect3DDevice9::CreateVertexShader
		/// - IDirect3DDevice9::CreatePixelShader
		/// - IDirect3DDevice9::CreateVertexDeclaration
		/// - ID3D10Device::CreateVertexShader
		/// - ID3D10Device::CreateGeometryShader
		/// - ID3D10Device::CreateGeometryShaderWithStreamOutput
		/// - ID3D10Device::CreatePixelShader
		/// - ID3D10Device::CreateBlendState
		/// - ID3D10Device::CreateDepthStencilState
		/// - ID3D10Device::CreateRasterizerState
		/// - ID3D10Device1::CreateBlendState1
		/// - ID3D11Device::CreateVertexShader
		/// - ID3D11Device::CreateHullShader
		/// - ID3D11Device::CreateDomainShader
		/// - ID3D11Device::CreateGeometryShader
		/// - ID3D11Device::CreateGeometryShaderWithStreamOutput
		/// - ID3D11Device::CreatePixelShader
		/// - ID3D11Device::CreateComputeShader
		/// - ID3D11Device::CreateBlendState
		/// - ID3D11Device::CreateDepthStencilState
		/// - ID3D11Device::CreateRasterizerState
		/// - ID3D11Device1::CreateBlendState1
		/// - ID3D11Device1::CreateRasterizerState1
		/// - ID3D11Device3::CreateRasterizerState2
		/// - ID3D12Device::CreateComputePipelineState
		/// - ID3D12Device::CreateGraphicsPipelineState
		/// - glShaderSource
		/// - vkCreateComputePipelines
		/// - vkCreateGraphicsPipelines
		/// 
		/// <para>Callback function signature: <c>bool (api::device *device, api::pipeline_desc &amp;desc, uint32_t dynamic_state_count, const dynamic_state *dynamic_states)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the pipeline description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_pipeline,

		/// <summary>
		/// Called on pipeline destruction, before:
		/// - IUnknown::Release
		/// - glDeleteProgram
		/// - vkDestroyPipeline
		/// 
		/// <para>Callback function signature: <c>bool (api::device *device, api::pipeline pipeline)</c></para>
		/// </summary>
		/// <remarks>
		/// Is not called in D3D9.
		/// </remarks>
		destroy_pipeline,

		/// <summary>
		/// Called after successfull render pass creation from:
		/// - vkCreateRenderPass
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, const api::render_pass_desc &amp;desc, api::render_pass pass)</c></para>
		/// </summary>
		init_render_pass,

		/// <summary>
		/// Called before:
		/// - vkCreateRenderPass
		/// 
		/// <para>Callback function signature: <c>bool (api::device *device, api::render_pass_desc &amp;desc)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the render pass description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_render_pass,

		/// <summary>
		/// Called on render pass destruction, before:
		/// - vkDestroyRenderPass
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, api::render_pass pass)</c></para>
		/// </summary>
		destroy_render_pass,

		/// <summary>
		/// Called after successfull framebuffer object creation from:
		/// - glFramebufferTexture
		/// - glFramebufferTexture1D
		/// - glFramebufferTexture2D
		/// - glFramebufferTexture3D
		/// - glFramebufferTextureLayer
		/// - glFramebufferRenderbuffer
		/// - glNamedFramebufferTexture
		/// - glNamedFramebufferRenderbuffer
		/// - vkCreateFramebuffer
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, const api::framebuffer_desc &amp;desc, api::framebuffer fbo)</c></para>
		/// </summary>
		/// <remarks>
		/// May be called multiple times with the same framebuffer handle (whenever the framebuffer object is updated).
		/// </remarks>
		init_framebuffer,

		/// <summary>
		/// Called before:
		/// - glFramebufferTexture
		/// - glFramebufferTexture1D
		/// - glFramebufferTexture2D
		/// - glFramebufferTexture3D
		/// - glFramebufferTextureLayer
		/// - glFramebufferRenderbuffer
		/// - glNamedFramebufferTexture
		/// - glNamedFramebufferRenderbuffer
		/// - vkCreateFramebuffer
		/// 
		/// <para>Callback function signature: <c>bool (api::device *device, api::framebuffer_desc &amp;desc)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the framebuffer description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_framebuffer,

		/// <summary>
		/// Called on framebuffer object destruction, before:
		/// - glDeleteFramebuffers
		/// - vkDestroyFramebuffer
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, api::framebuffer fbo)</c></para>
		/// </summary>
		destroy_framebuffer,

		/// <summary>
		/// Called after:
		/// - IDirect3DVertexBuffer9::Lock
		/// - IDirect3DIndexBuffer9::Lock
		/// - ID3D10Resource::Map
		/// - ID3D11DeviceContext::Map
		/// - ID3D12Resource::Map
		/// - glMapBuffer
		/// - glMapBufferRange
		/// - glMapNamedBuffer
		/// - glMapNamedBufferRange
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, resource resource, uint64_t offset, uint64_t size, map_access access, void **data)</c></para>
		/// </summary>
		map_buffer_region,

		/// <summary>
		/// Called before:
		/// - IDirect3DVertexBuffer9::Unlock
		/// - IDirect3DIndexBuffer9::Unlock
		/// - ID3D10Resource::Unmap
		/// - ID3D11DeviceContext::Unmap
		/// - ID3D12Resource::Unmap
		/// - glUnmapBuffer
		/// - glUnmapNamedBuffer
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, resource resource)</c></para>
		/// </summary>
		unmap_buffer_region,

		/// <summary>
		/// Called after:
		/// - IDirect3DTexture9::LockRect
		/// - IDirect3DCubeTexture9::LockRect
		/// - IDirect3DVolumeTexture9::LockBox
		/// - IDirect3DSurface9::LockRect
		/// - ID3D10Resource::Map
		/// - ID3D11DeviceContext::Map
		/// - ID3D12Resource::Map
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, resource resource, uint32_t subresource, const int32_t box[6], map_access access, subresource_data *data)</c></para>
		/// </summary>
		map_texture_region,

		/// <summary>
		/// Called before:
		/// - IDirect3DTexture9::UnlockRect
		/// - IDirect3DCubeTexture9::UnlockRect
		/// - IDirect3DVolumeTexture9::UnlockBox
		/// - IDirect3DSurface9::UnlockRect
		/// - ID3D10Resource::Unmap
		/// - ID3D11DeviceContext::Unmap
		/// - ID3D12Resource::Unmap
		/// 
		/// <para>Callback function signature: <c>void (api::device *device, resource resource, uint32_t subresource)</c></para>
		/// </summary>
		unmap_texture_region,

		/// <summary>
		/// Called before:
		/// - ID3D10Device::UpdateSubresource
		/// - ID3D11DeviceContext::UpdateSubresource
		/// - glBufferSubData
		/// - glNamedBufferSubData
		/// 
		/// <para>Callback function signature: <c>bool (api::device *device, const void *data, api::resource resource, uint64_t offset, uint64_t size)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		update_buffer_region,

		/// <summary>
		/// Called before:
		/// - ID3D10Device::UpdateSubresource
		/// - ID3D11DeviceContext::UpdateSubresource
		/// - glTexSubData1D
		/// - glTexSubData2D
		/// - glTexSubData3D
		/// - glTextureSubData1D
		/// - glTextureSubData2D
		/// - glTextureSubData3D
		/// - glCompressedTexSubData1D
		/// - glCompressedTexSubData2D
		/// - glCompressedTexSubData3D
		/// - glCompressedTextureSubData1D
		/// - glCompressedTextureSubData2D
		/// - glCompressedTextureSubData3D
		/// 
		/// <para>Callback function signature: <c>bool (api::device *device, const api::subresource_data &data, api::resource resource, uint32_t subresource, const int32_t box[6])</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		update_texture_region,

		/// <summary>
		/// Called before:
		/// - ID3D12Device::CreateConstantBufferView
		/// - ID3D12Device::CopyDescriptors
		/// - ID3D12Device::CopyDescriptorsSimple
		/// - vkUpdateDescriptorSets
		/// 
		/// <para>Callback function signature: <c>bool (api::device *device, uint32_t count, const api::descriptor_set_updates *updates)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		update_descriptor_sets,

		/// <summary>
		/// Called after:
		/// - ID3D12GraphicsCommandList::ResourceBarrier
		/// - vkCmdPipelineBarrier
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states)</c></para>
		/// </summary>
		barrier,

		/// <summary>
		/// Called after:
		/// - ID3D12GraphicsCommandList4::BeginRenderPass
		/// - glBindFramebuffer
		/// - vkCmdBeginRenderPass
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::render_pass pass, api::framebuffer fbo)</c></para>
		/// </summary>
		begin_render_pass,

		/// <summary>
		/// Called after:
		/// - ID3D12GraphicsCommandList4::EndRenderPass
		/// - vkCmdEndRenderPass
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		finish_render_pass,

		/// <summary>
		/// Called after:
		/// - IDirect3DDevice9::SetRenderTarget
		/// - IDirect3DDevice9::SetDepthStencilSurface
		/// - ID3D10Device::OMSetRenderTargets
		/// - ID3D11DeviceContext::OMSetRenderTargets
		/// - ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews
		/// - ID3D12GraphicsCommandList::OMSetRenderTargets
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)</c></para>
		/// </summary>
		bind_render_targets_and_depth_stencil,

		/// <summary>
		/// Called after:
		/// - IDirect3DDevice9::SetVertexShader
		/// - IDirect3DDevice9::SetPixelShader
		/// - IDirect3DDevice9::SetVertexDeclaration
		/// - ID3D10Device::VSSetShader
		/// - ID3D10Device::GSSetShader
		/// - ID3D10Device::PSSetShader
		/// - ID3D10Device::IASetInputLayout
		/// - ID3D10Device::OMSetBlendState
		/// - ID3D10Device::OMSetDepthStencilState
		/// - ID3D10Device::RSSetState
		/// - ID3D11DeviceContext::VSSetShader
		/// - ID3D11DeviceContext::HSSetShader
		/// - ID3D11DeviceContext::DSSetShader
		/// - ID3D11DeviceContext::GSSetShader
		/// - ID3D11DeviceContext::PSSetShader
		/// - ID3D11DeviceContext::CSSetShader
		/// - ID3D11DeviceContext::IASetInputLayout
		/// - ID3D11DeviceContext::OMSetBlendState
		/// - ID3D11DeviceContext::OMSetDepthStencilState
		/// - ID3D11DeviceContext::RSSetState
		/// - ID3D12GraphicsCommandList::Reset
		/// - ID3D12GraphicsCommandList::SetPipelineState
		/// - glUseProgram
		/// - vkCmdBindPipeline
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::pipeline_stage type, api::pipeline pipeline)</c></para>
		/// </summary>
		bind_pipeline,

		/// <summary>
		/// Called after:
		/// - IDirect3DDevice9::SetRenderState
		/// - ID3D10Device::IASetPrimitiveTopology
		/// - ID3D10Device::OMSetBlendState
		/// - ID3D10Device::OMSetDepthStencilState
		/// - ID3D11DeviceContext::IASetPrimitiveTopology
		/// - ID3D11DeviceContext::OMSetBlendState
		/// - ID3D11DeviceContext::OMSetDepthStencilState
		/// - ID3D12GraphicsCommandList::IASetPrimitiveTopology
		/// - ID3D12GraphicsCommandList::OMSetBlendFactor
		/// - ID3D12GraphicsCommandList::OMSetStencilRef
		/// - gl(...)
		/// - vkCmdSetDepthBias
		/// - vkCmdSetBlendConstants
		/// - vkCmdSetStencilCompareMask
		/// - vkCmdSetStencilWriteMask
		/// - vkCmdSetStencilReference
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t count, const api::dynamic_state *states, const uint32_t *values)</c></para>
		/// </summary>
		bind_pipeline_states,

		/// <summary>
		/// Called after:
		/// - IDirect3DDevice9::SetViewport
		/// - IDirect3DDevice9::SetRenderTarget (which implicitly updates the viewport)
		/// - ID3D10Device::RSSetViewports
		/// - ID3D11DeviceContext::RSSetViewports
		/// - ID3D12GraphicsCommandList::RSSetViewports
		/// - glViewport
		/// - glViewportArrayv
		/// - glViewportIndexedf
		/// - glViewportIndexedfv
		/// - vkCmdSetViewport
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const float *viewports)</c></para>
		/// </summary>
		/// <remarks>
		/// Viewport data format is <c>{ viewport[0].x, viewport[0].y, viewport[0].width, viewport[0].height, viewport[0].min_depth, viewport[0].max_depth, viewport[1].x, viewport[1].y, ... }</c>.
		/// </remarks>
		bind_viewports,

		/// <summary>
		/// Called after:
		/// - IDirect3DDevice9::SetScissorRect
		/// - ID3D10Device::RSSetScissorRects
		/// - ID3D11DeviceContext::RSSetScissorRects
		/// - ID3D12GraphicsCommandList::RSSetScissorRects
		/// - glScissor
		/// - glScissorArrayv
		/// - glScissorIndexed
		/// - glScissorIndexedv
		/// - vkCmdSetScissor
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// Rectangle data format is <c>{ rect[0].left, rect[0].top, rect[0].right, rect[0].bottom, rect[1].left, rect[1].right, ... }</c>.
		/// </remarks>
		bind_scissor_rects,

		/// <summary>
		/// Called after:
		/// - IDirect3DDevice9::SetVertexShaderConstantF
		/// - IDirect3DDevice9::SetPixelShaderConstantF
		/// - ID3D12GraphicsCommandList::SetComputeRoot32BitConstant
		/// - ID3D12GraphicsCommandList::SetComputeRoot32BitConstants
		/// - ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstant
		/// - ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstants
		/// - glUniform(...)
		/// - vkCmdPushConstants
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const uint32_t *values)</c></para>
		/// </summary>
		push_constants,

		/// <summary>
		/// Called after:
		/// - IDirect3DDevice9::SetTexture
		/// - ID3D10Device::VSSetSamplers
		/// - ID3D10Device::VSSetShaderResources
		/// - ID3D10Device::VSSetConstantBuffers
		/// - ID3D10Device::GSSetSamplers
		/// - ID3D10Device::GSSetShaderResources
		/// - ID3D10Device::GSSetConstantBuffers
		/// - ID3D10Device::PSSetSamplers
		/// - ID3D10Device::PSSetShaderResources
		/// - ID3D10Device::PSSetConstantBuffers
		/// - ID3D11DeviceContext::VSSetSamplers
		/// - ID3D11DeviceContext::VSSetShaderResources
		/// - ID3D11DeviceContext::VSSetConstantBuffers
		/// - ID3D11DeviceContext::HSSetSamplers
		/// - ID3D11DeviceContext::HSSetShaderResources
		/// - ID3D11DeviceContext::HSSetConstantBuffers
		/// - ID3D11DeviceContext::DSSetSamplers
		/// - ID3D11DeviceContext::DSSetShaderResources
		/// - ID3D11DeviceContext::DSSetConstantBuffers
		/// - ID3D11DeviceContext::GSSetSamplers
		/// - ID3D11DeviceContext::GSSetShaderResources
		/// - ID3D11DeviceContext::GSSetConstantBuffers
		/// - ID3D11DeviceContext::PSSetSamplers
		/// - ID3D11DeviceContext::PSSetShaderResources
		/// - ID3D11DeviceContext::PSSetConstantBuffers
		/// - ID3D11DeviceContext::CSSetSamplers
		/// - ID3D11DeviceContext::CSSetShaderResources
		/// - ID3D11DeviceContext::CSSetUnorderedAccessViews
		/// - ID3D11DeviceContext::CSSetConstantBuffers
		/// - ID3D12GraphicsCommandList::SetComputeRootConstantBufferView
		/// - ID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView
		/// - glBindBuffer
		/// - glBindBuffers
		/// - glBindTexture
		/// - glBindTextureUnit
		/// - glBindImageTexture
		/// - glBindTextures
		/// - vkCmdPushDescriptorSetKHR
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::write_descriptor_set &amp;update)</c></para>
		/// </summary>
		push_descriptors,

		/// <summary>
		/// Called after:
		/// - ID3D12GraphicsCommandList::SetComputeRootDescriptorTable
		/// - ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable
		/// - vkCmdBindDescriptorSets
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)</c></para>
		/// </summary>
		bind_descriptor_sets,

		/// <summary>
		/// Called after:
		/// - IDirect3DDevice9::SetIndices
		/// - ID3D10Device::IASetIndexBuffer
		/// - ID3D11DeviceContext::IASetIndexBuffer
		/// - ID3D12GraphicsCommandList::IASetIndexBuffer
		/// - glBindBuffer
		/// - vkCmdBindIndexBuffer
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::resource buffer, uint64_t offset, uint32_t index_size)</c></para>
		/// </summary>
		bind_index_buffer,

		/// <summary>
		/// Called after:
		/// - IDirect3DDevice9::SetStreamSource
		/// - ID3D10Device::IASetVertexBuffers
		/// - ID3D11DeviceContext::IASetVertexBuffers
		/// - ID3D12GraphicsCommandList::IASetVertexBuffers
		/// - glBindBuffer
		/// - glBindVertexBuffer
		/// - glBindVertexBuffers
		/// - vkCmdBindVertexBuffers
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)</c></para>
		/// </summary>
		bind_vertex_buffers,

		/// <summary>
		/// Called before:
		/// - IDirect3DDevice9::DrawPrimitive
		/// - IDirect3DDevice9::DrawPrimitiveUP
		/// - ID3D10Device::Draw
		/// - ID3D10Device::DrawInstanced
		/// - ID3D11DeviceContext::Draw
		/// - ID3D11DeviceContext::DrawInstanced
		/// - ID3D12GraphicsCommandList::DrawInstanced
		/// - glDrawArrays
		/// - glDrawArraysInstanced
		/// - glDrawArraysInstancedBaseInstance
		/// - glMultiDrawArrays
		/// - vkCmdDraw
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		draw,

		/// <summary>
		/// Called before:
		/// - IDirect3DDevice9::DrawIndexedPrimitive
		/// - IDirect3DDevice9::DrawIndexedPrimitiveUP
		/// - ID3D10Device::DrawIndexed
		/// - ID3D10Device::DrawIndexedInstanced
		/// - ID3D11DeviceContext::DrawIndexed
		/// - ID3D11DeviceContext::DrawIndexedInstanced
		/// - ID3D12GraphicsCommandList::DrawIndexedInstanced
		/// - glDrawElements
		/// - glDrawElementsBaseVertex
		/// - glDrawElementsInstanced
		/// - glDrawElementsInstancedBaseVertex
		/// - glDrawElementsInstancedBaseInstance
		/// - glDrawElementsInstancedBaseVertexBaseInstance
		/// - glMultiDrawElements
		/// - glMultiDrawElementsBaseVertex
		/// - vkCmdDrawIndexed
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		draw_indexed,

		/// <summary>
		/// Called before:
		/// - ID3D11DeviceContext::Dispatch
		/// - ID3D12GraphicsCommandList::Dispatch
		/// - glDispatchCompute
		/// - vkCmdDispatch
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		dispatch,

		/// <summary>
		/// Called before:
		/// - ID3D11DeviceContext::DrawInstancedIndirect
		/// - ID3D11DeviceContext::DrawIndexedInstancedIndirect
		/// - ID3D11DeviceContext::DispatchIndirect
		/// - ID3D12GraphicsCommandList::ExecuteIndirect
		/// - glDrawArraysIndirect
		/// - glDrawElementsIndirect
		/// - glMultiDrawArraysIndirect
		/// - glMultiDrawElementsIndirect
		/// - glDispatchComputeIndirect
		/// - vkCmdDrawIndirect
		/// - vkCmdDrawIndexedIndirect
		/// - vkCmdDispatchIndirect
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		draw_or_dispatch_indirect,

		/// <summary>
		/// Called before:
		/// - IDirect3DDevice9::UpdateTexture
		/// - IDirect3DDevice9::GetRenderTargetData
		/// - ID3D10Device::CopyResource
		/// - ID3D11DeviceContext::CopyResource
		/// - ID3D12GraphicsCommandList::CopyResource
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource source, api::resource dest)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="api::resource_usage::copy_source"/> state.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_resource,

		/// <summary>
		/// Called before:
		/// - ID3D12GraphicsCommandList::CopyBufferRegion
		/// - glCopyBufferSubData
		/// - glCopyNamedBufferSubData
		/// - vkCmdCopyBuffer
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource source, uint64_t source_offset, api::resource dest, uint64_t dest_offset, uint64_t size)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="api::resource_usage::copy_source"/> state.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_buffer_region,

		/// <summary>
		/// Called before:
		/// - ID3D12GraphicsCommandList::CopyTextureRegion
		/// - vkCmdCopyBufferToImage
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource source, uint64_t source_offset, uint32_t row_length, uint32_t slice_height, api::resource dest, uint32_t dest_subresource, const int32_t dest_box[6])</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="api::resource_usage::copy_source"/> state.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_buffer_to_texture,

		/// <summary>
		/// Called before:
		/// - IDirect3DDevice9::UpdateSurface
		/// - IDirect3DDevice9::StretchRect
		/// - ID3D10Device::CopySubresourceRegion
		/// - ID3D11DeviceContext::CopySubresourceRegion
		/// - ID3D12GraphicsCommandList::CopyTextureRegion
		/// - glBlitFramebuffer
		/// - glBlitNamedFramebuffer
		/// - glCopyImageSubData
		/// - glCopyTexSubImage1D
		/// - glCopyTexSubImage2D
		/// - glCopyTexSubImage3D
		/// - glCopyTextureSubImage1D
		/// - glCopyTextureSubImage2D
		/// - glCopyTextureSubImage3D
		/// - vkCmdBlitImage
		/// - vkCmdCopyImage
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const int32_t source_box[6], api::resource dest, uint32_t dest_subresource, const int32_t dest_box[6], api::filter_mode filter)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="api::resource_usage::copy_source"/> state.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_texture_region,

		/// <summary>
		/// Called before:
		/// - ID3D12GraphicsCommandList::CopyTextureRegion
		/// - vkCmdCopyImageToBuffer
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const int32_t source_box[6], api::resource dest, uint64_t dest_offset, uint32_t row_length, uint32_t slice_height)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="api::resource_usage::copy_source"/> state.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_texture_to_buffer,

		/// <summary>
		/// Called before:
		/// - IDirect3DDevice9::StretchRect
		/// - ID3D10Device::ResolveSubresource
		/// - ID3D11DeviceContext::ResolveSubresource
		/// - ID3D12GraphicsCommandList::ResolveSubresource
		/// - ID3D12GraphicsCommandList1::ResolveSubresourceRegion
		/// - glBlitFramebuffer
		/// - glBlitNamedFramebuffer
		/// - vkCmdResolveImage
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const int32_t source_box[6], api::resource dest, uint32_t dest_subresource, const int32_t dest_offset[3], api::format format)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="api::resource_usage::resolve_source"/> state.
		/// Destination resource will be in the <see cref="api::resource_usage::resolve_dest"/> state.
		/// </remarks>
		resolve_texture_region,

		/// <summary>
		/// Called before:
		/// - IDirect3DDevice9::Clear
		/// - glClear
		/// - vkCmdClearAttachments
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::attachment_type clear_flags, const float color[4], float depth, uint8_t stencil, uint32_t rect_count, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		clear_attachments,

		/// <summary>
		/// Called before:
		/// - ID3D10Device::ClearDepthStencilView
		/// - ID3D11DeviceContext::ClearDepthStencilView
		/// - ID3D11DeviceContext1::ClearView
		/// - ID3D12GraphicsCommandList::ClearDepthStencilView
		/// - ID3D12GraphicsCommandList4::BeginRenderPass
		/// - glClearBufferfi
		/// - glClearBufferfv
		/// - glClearNamedFramebufferfi
		/// - glClearNamedFramebufferfv
		/// - vkCmdClearDepthStencilImage
		/// - vkCmdBeginRenderPass
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view dsv, api::attachment_type clear_flags, float depth, uint8_t stencil, uint32_t rect_count, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resource will be in the <see cref="api::resource_usage::depth_stencil_write"/> state.
		/// </remarks>
		clear_depth_stencil_view,

		/// <summary>
		/// Called before:
		/// - IDirect3DDevice9::ColorFill
		/// - ID3D10Device::ClearRenderTargetView
		/// - ID3D11DeviceContext::ClearRenderTargetView
		/// - ID3D11DeviceContext1::ClearView
		/// - ID3D12GraphicsCommandList::ClearRenderTargetView
		/// - ID3D12GraphicsCommandList4::BeginRenderPass
		/// - glClearBufferfv
		/// - glClearNamedFramebufferfv
		/// - vkCmdClearColorImage
		/// - vkCmdBeginRenderPass
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view rtv, const float color[4], uint32_t rect_count, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resources will be in the <see cref="api::resource_usage::render_target"/> state.
		/// </remarks>
		clear_render_target_view,

		/// <summary>
		/// Called before:
		/// - ID3D11DeviceContext::ClearUnorderedAccessViewUint
		/// - ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view uav, const uint32_t values[4], uint32_t rect_count, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resource will be in the <see cref="api::resource_usage::unordered_access"/> state.
		/// </remarks>
		clear_unordered_access_view_uint,

		/// <summary>
		/// Called before:
		/// - ID3D11DeviceContext::ClearUnorderedAccessViewFloat
		/// - ID3D11DeviceContext1::ClearView
		/// - ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view uav, const float values[4], uint32_t rect_count, const int32_t *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resource will be in the <see cref="api::resource_usage::unordered_access"/> state.
		/// </remarks>
		clear_unordered_access_view_float,

		/// <summary>
		/// Called before:
		/// - ID3D10Device::GenerateMips
		/// - ID3D11DeviceContext::GenerateMips
		/// - glGenerateMipmap
		/// - glGenerateTextureMipmap
		/// 
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view srv)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		generate_mipmaps,

		/// <summary>
		/// Called before:
		/// - ID3D12GraphicsCommandList::Reset
		/// - vkBeginCommandBuffer
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		reset_command_list,

		/// <summary>
		/// Called before:
		/// - ID3D11DeviceContext::ExecuteCommandLis
		/// - ID3D12CommandQueue::ExecuteCommandLists
		/// - vkQueueSubmit
		/// 
		/// <para>Callback function signature: <c>void (api::command_queue *queue, api::command_list *cmd_list)</c></para>
		/// </summary>
		execute_command_list,

		/// <summary>
		/// Called before:
		/// - ID3D12GraphicsCommandList::ExecuteBundle
		/// - vkCmdExecuteCommands
		/// 
		/// In addition, called after:
		/// - ID3D11DeviceContext::FinishCommandList
		/// 
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::command_list *secondary_cmd_list)</c></para>
		/// </summary>
		execute_secondary_command_list,

		/// <summary>
		/// Called before:
		/// - IDirect3DDevice9::Present
		/// - IDirect3DDevice9Ex::PresentEx
		/// - IDirect3DSwapChain9::Present
		/// - IDXGISwapChain::Present(1)
		/// - IDXGISwapChain3::Present1
		/// - D3D12CommandQueueDownlevel::Present
		/// - wglSwapBuffers
		/// - vkQueuePresentKHR
		/// 
		/// <para>Callback function signature: <c>void (api::command_queue *queue, api::swapchain *swapchain)</c></para>
		/// </summary>
		present,

		/// <summary>
		/// Called right before ReShade effects are rendered.
		/// 
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime, api::command_list *cmd_list)</c></para>
		/// </summary>
		reshade_begin_effects,

		/// <summary>
		/// Called right after ReShade effects were rendered.
		/// 
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime, api::command_list *cmd_list)</c></para>
		/// </summary>
		reshade_finish_effects,

		/// <summary>
		/// Called right after ReShade effects were reloaded.
		/// 
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime)</c></para>
		/// </summary>
		reshade_reloaded_effects,

#ifdef RESHADE_ADDON
		max // Last value used internally by ReShade to determine number of events in this enum
#endif
	};

	template <addon_event ev>
	struct addon_event_traits;

#define RESHADE_DEFINE_ADDON_EVENT_TRAITS(ev, ret, ...) \
	template <> \
	struct addon_event_traits<ev> { \
		using decl = ret(*)(__VA_ARGS__); \
		using type = ret; \
	}

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_device, void, api::device *device);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_device, void, api::device *device);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_command_list, void, api::command_list *cmd_list);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_command_list, void, api::command_list *cmd_list);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_command_queue, void, api::command_queue *queue);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_command_queue, void, api::command_queue *queue);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_swapchain, void, api::swapchain *swapchain);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::create_swapchain, bool, api::resource_desc &buffer_desc, void *hwnd);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_swapchain, void, api::swapchain *swapchain);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_effect_runtime, void, api::effect_runtime *runtime);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_effect_runtime, void, api::effect_runtime *runtime);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_sampler, void, api::device *device, const api::sampler_desc &desc, api::sampler sampler);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::create_sampler, bool, api::device *device, api::sampler_desc &desc);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_sampler, void, api::device *device, api::sampler sampler);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_resource, void, api::device *device, const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource resource);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::create_resource, bool, api::device *device, api::resource_desc &desc, api::subresource_data *initial_data, api::resource_usage initial_state);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_resource, void, api::device *device, api::resource resource);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_resource_view, void, api::device *device, api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view view);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::create_resource_view, bool, api::device *device, api::resource resource, api::resource_usage usage_type, api::resource_view_desc &desc);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_resource_view, void, api::device *device, api::resource_view view);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_pipeline, void, api::device *device, const api::pipeline_desc &desc, uint32_t dynamic_state_count, const api::dynamic_state *dynamic_states, api::pipeline pipeline);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::create_pipeline, bool, api::device *device, api::pipeline_desc &desc, uint32_t dynamic_state_count, const api::dynamic_state *dynamic_states);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_pipeline, void, api::device *device, api::pipeline pipeline);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_render_pass, void, api::device *device, const api::render_pass_desc &desc, api::render_pass pass);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::create_render_pass, bool, api::device *device, api::render_pass_desc &desc);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_render_pass, void, api::device *device, api::render_pass pass);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_framebuffer, void, api::device *device, const api::framebuffer_desc &desc, api::framebuffer fbo);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::create_framebuffer, bool, api::device *device, api::framebuffer_desc &desc);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_framebuffer, void, api::device *device, api::framebuffer fbo);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::map_buffer_region, void, api::device *device, api::resource resource, uint64_t offset, uint64_t size, api::map_access access, void **data);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::unmap_buffer_region, void, api::device *device, api::resource resource);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::map_texture_region, void, api::device *device, api::resource resource, uint32_t subresource, const int32_t box[6], api::map_access access, api::subresource_data *data);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::unmap_texture_region, void, api::device *device, api::resource resource, uint32_t subresource);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::update_buffer_region, bool, api::device *device, const void *data, api::resource resource, uint64_t offset, uint64_t size);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::update_texture_region, bool, api::device *device, const api::subresource_data &data, api::resource resource, uint32_t subresource, const int32_t box[6]);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::update_descriptor_sets, bool, api::device *device, uint32_t count, const api::descriptor_set_update *updates);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::barrier, void, api::command_list *cmd_list, uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::begin_render_pass, void, api::command_list *cmd_list, api::render_pass pass, api::framebuffer fbo);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::finish_render_pass, void, api::command_list *cmd_list);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_render_targets_and_depth_stencil, void, api::command_list *cmd_list, uint32_t count, const api::resource_view *rtvs, api::resource_view dsv);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_pipeline, void, api::command_list *cmd_list, api::pipeline_stage type, api::pipeline pipeline);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_pipeline_states, void, api::command_list *cmd_list, uint32_t count, const api::dynamic_state *states, const uint32_t *values);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_viewports, void, api::command_list *cmd_list, uint32_t first, uint32_t count, const float *viewports);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_scissor_rects, void, api::command_list *cmd_list, uint32_t first, uint32_t count, const int32_t *rects);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::push_constants, void, api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const uint32_t *values);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::push_descriptors, void, api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_set_update &update);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_descriptor_sets, void, api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_index_buffer, void, api::command_list *cmd_list, api::resource buffer, uint64_t offset, uint32_t index_size);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_vertex_buffers, void, api::command_list *cmd_list, uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::draw, bool, api::command_list *cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::draw_indexed, bool, api::command_list *cmd_list, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::dispatch, bool, api::command_list *cmd_list, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::draw_or_dispatch_indirect, bool, api::command_list *cmd_list, api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_resource, bool, api::command_list *cmd_list, api::resource source, api::resource dest);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_buffer_region, bool, api::command_list *cmd_list, api::resource source, uint64_t source_offset, api::resource dest, uint64_t dest_offset, uint64_t size);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_buffer_to_texture, bool, api::command_list *cmd_list, api::resource source, uint64_t source_offset, uint32_t row_length, uint32_t slice_height, api::resource dest, uint32_t dest_subresource, const int32_t dest_box[6]);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_texture_region, bool, api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const int32_t source_box[6], api::resource dest, uint32_t dest_subresource, const int32_t dest_box[6], api::filter_mode filter);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_texture_to_buffer, bool, api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const int32_t source_box[6], api::resource dest, uint64_t dest_offset, uint32_t row_length, uint32_t slice_height);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::resolve_texture_region, bool, api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const int32_t source_box[6], api::resource dest, uint32_t dest_subresource, const int32_t dest_offset[3], api::format format);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::clear_attachments, bool, api::command_list *cmd_list, api::attachment_type clear_flags, const float color[4], float depth, uint8_t stencil, uint32_t rect_count, const int32_t *rects);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::clear_depth_stencil_view, bool, api::command_list *cmd_list, api::resource_view dsv, api::attachment_type clear_flags, float depth, uint8_t stencil, uint32_t rect_count, const int32_t *rects);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::clear_render_target_view, bool, api::command_list *cmd_list, api::resource_view rtv, const float color[4], uint32_t rect_count, const int32_t *rects);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::clear_unordered_access_view_uint, bool, api::command_list *cmd_list, api::resource_view uav, const uint32_t values[4], uint32_t rect_count, const int32_t *rects);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::clear_unordered_access_view_float, bool, api::command_list *cmd_list, api::resource_view uav, const float values[4], uint32_t rect_count, const int32_t *rects);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::generate_mipmaps, bool, api::command_list *cmd_list, api::resource_view srv);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reset_command_list, void, api::command_list *cmd_list);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::execute_command_list, void, api::command_queue *queue, api::command_list *cmd_list);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::execute_secondary_command_list, void, api::command_list *cmd_list, api::command_list *secondary_cmd_list);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::present, void, api::command_queue *queue, api::swapchain *swapchain);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_begin_effects, void, api::effect_runtime *runtime, api::command_list *cmd_list);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_finish_effects, void, api::effect_runtime *runtime, api::command_list *cmd_list);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_reloaded_effects, void, api::effect_runtime *runtime);
}
