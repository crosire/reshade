/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "reshade_api.hpp"

namespace reshade
{
	enum class addon_event : uint32_t
	{
		/// <summary>
		/// Called after successfull device creation, from:
		/// <list type="bullet">
		/// <item><description>IDirect3D9::CreateDevice</description></item>
		/// <item><description>IDirect3D9Ex::CreateDeviceEx</description></item>
		/// <item><description>IDirect3DDevice9::Reset</description></item>
		/// <item><description>IDirect3DDevice9Ex::ResetEx</description></item>
		/// <item><description>D3D10CreateDevice</description></item>
		/// <item><description>D3D10CreateDevice1</description></item>
		/// <item><description>D3D10CreateDeviceAndSwapChain</description></item>
		/// <item><description>D3D10CreateDeviceAndSwapChain1</description></item>
		/// <item><description>D3D11CreateDevice</description></item>
		/// <item><description>D3D11CreateDeviceAndSwapChain</description></item>
		/// <item><description>D3D12CreateDevice</description></item>
		/// <item><description>glMakeCurrent</description></item>
		/// <item><description>vkCreateDevice</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device)</c></para>
		/// </summary>
		init_device,

		/// <summary>
		/// Called on device destruction, before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::Reset</description></item>
		/// <item><description>IDirect3DDevice9Ex::ResetEx</description></item>
		/// <item><description>IDirect3DDevice9::Release</description></item>
		/// <item><description>ID3D10Device::Release</description></item>
		/// <item><description>ID3D11Device::Release</description></item>
		/// <item><description>ID3D12Device::Release</description></item>
		/// <item><description>wglDeleteContext</description></item>
		/// <item><description>vkDestroyDevice</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device)</c></para>
		/// </summary>
		destroy_device,

		/// <summary>
		/// Called after successfull command list creation, from:
		/// <list type="bullet">
		/// <item><description>ID3D11Device::CreateDeferredContext</description></item>
		/// <item><description>ID3D11Device1::CreateDeferredContext1</description></item>
		/// <item><description>ID3D11Device2::CreateDeferredContext2</description></item>
		/// <item><description>ID3D11Device3::CreateDeferredContext3</description></item>
		/// <item><description>ID3D12Device::CreateCommandList</description></item>
		/// <item><description>ID3D12Device4::CreateCommandList1</description></item>
		/// <item><description>vkAllocateCommandBuffers</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		/// <remarks>
		/// In case of D3D9, D3D10, D3D11 and OpenGL this is called during device initialization as well and behaves as if an implicit immediate command list was created.
		/// </remarks>
		init_command_list,

		/// <summary>
		/// Called on command list destruction, before:
		/// <list type="bullet">
		/// <item><description>ID3D11CommandList::Release</description></item>
		/// <item><description>ID3D12CommandList::Release</description></item>
		/// <item><description>vkFreeCommandBuffers</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		destroy_command_list,

		/// <summary>
		/// Called after successfull command queue creation, from:
		/// <list type="bullet">
		/// <item><description>ID3D12Device::CreateCommandQueue</description></item>
		/// <item><description>vkCreateDevice (for every queue associated with the device)</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_queue *queue)</c></para>
		/// </summary>
		/// <remarks>
		/// In case of D3D9, D3D10, D3D11 and OpenGL this is called during device initialization as well and behaves as if an implicit command queue was created.
		/// </remarks>
		init_command_queue,

		/// <summary>
		/// Called on command queue destruction, before:
		/// <list type="bullet">
		/// <item><description>ID3D12CommandQueue::Release</description></item>
		/// <item><description>vkDestroyDevice (for every queue associated with the device)</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_queue *queue)</c></para>
		/// </summary>
		destroy_command_queue,

		/// <summary>
		/// Called after successfull swap chain creation, from:
		/// <list type="bullet">
		/// <item><description>IDirect3D9::CreateDevice (for the implicit swap chain)</description></item>
		/// <item><description>IDirect3D9Ex::CreateDeviceEx (for the implicit swap chain)</description></item>
		/// <item><description>IDirect3D9Device::CreateAdditionalSwapChain</description></item>
		/// <item><description>IDirect3DDevice9::Reset (for the implicit swap chain)</description></item>
		/// <item><description>IDirect3DDevice9Ex::ResetEx (for the implicit swap chain)</description></item>
		/// <item><description>IDXGIFactory::CreateSwapChain</description></item>
		/// <item><description>IDXGIFactory2::CreateSwapChain(...)</description></item>
		/// <item><description>IDXGISwapChain::ResizeBuffers</description></item>
		/// <item><description>IDXGISwapChain3::ResizeBuffers1</description></item>
		/// <item><description>wglMakeCurrent</description></item>
		/// <item><description>wglSwapBuffers (after window was resized)</description></item>
		/// <item><description>vkCreateSwapchainKHR</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::swapchain *swapchain)</c></para>
		/// </summary>
		init_swapchain,

		/// <summary>
		/// Called on swap chain creation, before:
		/// <list type="bullet">
		/// <item><description>IDirect3D9::CreateDevice (for the implicit swap chain)</description></item>
		/// <item><description>IDirect3D9Ex::CreateDeviceEx (for the implicit swap chain)</description></item>
		/// <item><description>IDirect3D9Device::CreateAdditionalSwapChain</description></item>
		/// <item><description>IDirect3D9Device::Reset (for the implicit swap chain)</description></item>
		/// <item><description>IDirect3D9DeviceEx::ResetEx (for the implicit swap chain)</description></item>
		/// <item><description>IDXGIFactory::CreateSwapChain</description></item>
		/// <item><description>IDXGIFactory2::CreateSwapChain(...)</description></item>
		/// <item><description>IDXGISwapChain::ResizeBuffers</description></item>
		/// <item><description>IDXGISwapChain3::ResizeBuffers1</description></item>
		/// <item><description>vkCreateSwapchainKHR</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::swapchain_desc &amp;desc, void *hwnd)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the swap chain description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Is not called in OpenGL (since it is not possible to influence swap chain creation there).
		/// </remarks>
		create_swapchain,

		/// <summary>
		/// Called on swap chain destruction, before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::Release (for the implicit swap chain)</description></item>
		/// <item><description>IDirect3DSwapChain9::Release</description></item>
		/// <item><description>IDXGISwapChain::Release</description></item>
		/// <item><description>wglDeleteContext</description></item>
		/// <item><description>wglSwapBuffers (after window was resized)</description></item>
		/// <item><description>vkDestroySwapchainKHR</description></item>
		/// </list>
		/// In addition, called when swap chain is reset, before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::Reset (for the implicit swap chain)</description></item>
		/// <item><description>IDirect3DDevice9Ex::ResetEx (for the implicit swap chain)</description></item>
		/// <item><description>IDXGISwapChain::ResizeBuffers</description></item>
		/// <item><description>IDXGISwapChain1::ResizeBuffers1</description></item>
		/// </list>
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
		/// Called after successfull sampler creation from:
		/// <list type="bullet">
		/// <item><description>ID3D10Device::CreateSamplerState</description></item>
		/// <item><description>ID3D11Device::CreateSamplerState</description></item>
		/// <item><description>ID3D12Device::CreateSampler</description></item>
		/// <item><description>vkCreateSampler</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, const api::sampler_desc &amp;desc, api::sampler sampler)</c></para>
		/// </summary>
		/// <remarks>
		/// Is not called in D3D9 (since samplers are loose state there) or OpenGL.
		/// </remarks>
		init_sampler,

		/// <summary>
		/// Called on sampler creation, before:
		/// <list type="bullet">
		/// <item><description>ID3D10Device::CreateSamplerState</description></item>
		/// <item><description>ID3D11Device::CreateSamplerState</description></item>
		/// <item><description>ID3D12Device::CreateSampler</description></item>
		/// <item><description>vkCreateSampler</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::device *device, api::sampler_desc &amp;desc)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the sampler description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Is not called in D3D9 (since samplers are loose state there) or OpenGL.
		/// </remarks>
		create_sampler,

		/// <summary>
		/// Called on sampler destruction, before:
		/// <list type="bullet">
		/// <item><description>ID3D10SamplerState::Release</description></item>
		/// <item><description>ID3D11SamplerState::Release</description></item>
		/// <item><description>glDeleteSamplers</description></item>
		/// <item><description>vkDestroySampler</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::sampler sampler)</c></para>
		/// </summary>
		/// <remarks>
		/// Is not called in D3D9 (since samplers are loose state there), D3D12 (since samplers are descriptor handles instead of objects there) or OpenGL.
		/// </remarks>
		destroy_sampler,

		/// <summary>
		/// Called after successfull resource creation from:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::CreateVertexBuffer</description></item>
		/// <item><description>IDirect3DDevice9::CreateIndexBuffer</description></item>
		/// <item><description>IDirect3DDevice9::CreateTexture</description></item>
		/// <item><description>IDirect3DDevice9::CreateCubeTexture</description></item>
		/// <item><description>IDirect3DDevice9::CreateVolumeTexture</description></item>
		/// <item><description>IDirect3DDevice9::CreateRenderTargetSurface</description></item>
		/// <item><description>IDirect3DDevice9::CreateDepthStencilSurface</description></item>
		/// <item><description>IDirect3DDevice9::CreateOffscreenPlainSurface</description></item>
		/// <item><description>IDirect3DDevice9Ex::CreateRenderTargetSurfaceEx</description></item>
		/// <item><description>IDirect3DDevice9Ex::CreateDepthStencilSurfaceEx</description></item>
		/// <item><description>IDirect3DDevice9Ex::CreateOffscreenPlainSurfaceEx</description></item>
		/// <item><description>ID3D10Device::CreateBuffer</description></item>
		/// <item><description>ID3D10Device::CreateTexture1D</description></item>
		/// <item><description>ID3D10Device::CreateTexture2D</description></item>
		/// <item><description>ID3D10Device::CreateTexture2D</description></item>
		/// <item><description>ID3D11Device::CreateBuffer</description></item>
		/// <item><description>ID3D11Device::CreateTexture1D</description></item>
		/// <item><description>ID3D11Device::CreateTexture2D</description></item>
		/// <item><description>ID3D11Device::CreateTexture3D</description></item>
		/// <item><description>ID3D11Device3::CreateTexture2D</description></item>
		/// <item><description>ID3D11Device3::CreateTexture3D</description></item>
		/// <item><description>ID3D12Device::CreateCommittedResource</description></item>
		/// <item><description>ID3D12Device::CreatePlacedResource</description></item>
		/// <item><description>ID3D12Device::CreateReservedResource</description></item>
		/// <item><description>ID3D12Device4::CreateCommittedResource1</description></item>
		/// <item><description>ID3D12Device4::CreateReservedResource1</description></item>
		/// <item><description>glBufferData</description></item>
		/// <item><description>glBufferStorage</description></item>
		/// <item><description>glNamedBufferData</description></item>
		/// <item><description>glNamedBufferStorage</description></item>
		/// <item><description>glTexImage1D</description></item>
		/// <item><description>glTexImage2D</description></item>
		/// <item><description>glTexImage2DMultisample</description></item>
		/// <item><description>glTexImage3D</description></item>
		/// <item><description>glTexImage3DMultisample</description></item>
		/// <item><description>glCompressedTexImage1D</description></item>
		/// <item><description>glCompressedTexImage2D</description></item>
		/// <item><description>glCompressedTexImage3D</description></item>
		/// <item><description>glTexStorage1D</description></item>
		/// <item><description>glTexStorage2D</description></item>
		/// <item><description>glTexStorage2DMultisample</description></item>
		/// <item><description>glTexStorage3D</description></item>
		/// <item><description>glTexStorage3DMultisample</description></item>
		/// <item><description>glTextureStorage1D</description></item>
		/// <item><description>glTextureStorage2D</description></item>
		/// <item><description>glTextureStorage2DMultisample</description></item>
		/// <item><description>glTextureStorage3D</description></item>
		/// <item><description>glTextureStorage3DMultisample</description></item>
		/// <item><description>glRenderbufferStorage</description></item>
		/// <item><description>glRenderbufferStorageMultisample</description></item>
		/// <item><description>glNamedRenderbufferStorage</description></item>
		/// <item><description>glNamedRenderbufferStorageMultisample</description></item>
		/// <item><description>vkBindBufferMemory</description></item>
		/// <item><description>vkBindBufferMemory2</description></item>
		/// <item><description>vkBindImageMemory</description></item>
		/// <item><description>vkBindImageMemory2</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, const api::resource_desc &amp;desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource resource)</c></para>
		/// </summary>
		init_resource,

		/// <summary>
		/// Called on resource creation, before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::CreateVertexBuffer</description></item>
		/// <item><description>IDirect3DDevice9::CreateIndexBuffer</description></item>
		/// <item><description>IDirect3DDevice9::CreateTexture</description></item>
		/// <item><description>IDirect3DDevice9::CreateCubeTexture</description></item>
		/// <item><description>IDirect3DDevice9::CreateVolumeTexture</description></item>
		/// <item><description>IDirect3DDevice9::CreateRenderTargetSurface</description></item>
		/// <item><description>IDirect3DDevice9::CreateDepthStencilSurface</description></item>
		/// <item><description>IDirect3DDevice9::CreateOffscreenPlainSurface</description></item>
		/// <item><description>IDirect3DDevice9Ex::CreateRenderTargetSurfaceEx</description></item>
		/// <item><description>IDirect3DDevice9Ex::CreateDepthStencilSurfaceEx</description></item>
		/// <item><description>IDirect3DDevice9Ex::CreateOffscreenPlainSurfaceEx</description></item>
		/// <item><description>ID3D10Device::CreateBuffer</description></item>
		/// <item><description>ID3D10Device::CreateTexture1D</description></item>
		/// <item><description>ID3D10Device::CreateTexture2D</description></item>
		/// <item><description>ID3D10Device::CreateTexture2D</description></item>
		/// <item><description>ID3D11Device::CreateBuffer</description></item>
		/// <item><description>ID3D11Device::CreateTexture1D</description></item>
		/// <item><description>ID3D11Device::CreateTexture2D</description></item>
		/// <item><description>ID3D11Device::CreateTexture3D</description></item>
		/// <item><description>ID3D11Device3::CreateTexture2D</description></item>
		/// <item><description>ID3D11Device3::CreateTexture3D</description></item>
		/// <item><description>ID3D12Device::CreateCommittedResource</description></item>
		/// <item><description>ID3D12Device::CreatePlacedResource</description></item>
		/// <item><description>ID3D12Device::CreateReservedResource</description></item>
		/// <item><description>ID3D12Device4::CreateCommittedResource1</description></item>
		/// <item><description>ID3D12Device4::CreateReservedResource1</description></item>
		/// <item><description>glBufferData</description></item>
		/// <item><description>glBufferStorage</description></item>
		/// <item><description>glNamedBufferData</description></item>
		/// <item><description>glNamedBufferStorage</description></item>
		/// <item><description>glTexImage1D</description></item>
		/// <item><description>glTexImage2D</description></item>
		/// <item><description>glTexImage2DMultisample</description></item>
		/// <item><description>glTexImage3D</description></item>
		/// <item><description>glTexImage3DMultisample</description></item>
		/// <item><description>glCompressedTexImage1D</description></item>
		/// <item><description>glCompressedTexImage2D</description></item>
		/// <item><description>glCompressedTexImage3D</description></item>
		/// <item><description>glTexStorage1D</description></item>
		/// <item><description>glTexStorage2D</description></item>
		/// <item><description>glTexStorage2DMultisample</description></item>
		/// <item><description>glTexStorage3D</description></item>
		/// <item><description>glTexStorage3DMultisample</description></item>
		/// <item><description>glTextureStorage1D</description></item>
		/// <item><description>glTextureStorage2D</description></item>
		/// <item><description>glTextureStorage2DMultisample</description></item>
		/// <item><description>glTextureStorage3D</description></item>
		/// <item><description>glTextureStorage3DMultisample</description></item>
		/// <item><description>glRenderbufferStorage</description></item>
		/// <item><description>glRenderbufferStorageMultisample</description></item>
		/// <item><description>glNamedRenderbufferStorage</description></item>
		/// <item><description>glNamedRenderbufferStorageMultisample</description></item>
		/// <item><description>vkCreateBuffer</description></item>
		/// <item><description>vkCreateImage</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::device *device, api::resource_desc &amp;desc, api::subresource_data *initial_data, api::resource_usage initial_state)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the resource description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_resource,

		/// <summary>
		/// Called on resource destruction, before:
		/// <list type="bullet">
		/// <item><description>IDirect3DResource9::Release</description></item>
		/// <item><description>ID3D10Resource::Release</description></item>
		/// <item><description>ID3D11Resource::Release</description></item>
		/// <item><description>ID3D12Resource::Release</description></item>
		/// <item><description>glDeleteBuffers</description></item>
		/// <item><description>glDeleteTextures</description></item>
		/// <item><description>glDeleteRenderbuffers</description></item>
		/// <item><description>vkDestroyBuffer</description></item>
		/// <item><description>vkDestroyImage</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::resource resource)</c></para>
		/// </summary>
		destroy_resource,

		/// <summary>
		/// Called after successfull resource view creation from:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::CreateTexture</description></item>
		/// <item><description>IDirect3DDevice9::CreateCubeTexture</description></item>
		/// <item><description>IDirect3DDevice9::CreateVolumeTexture</description></item>
		/// <item><description>ID3D10Device::CreateShaderResourceView</description></item>
		/// <item><description>ID3D10Device::CreateRenderTargetView</description></item>
		/// <item><description>ID3D10Device::CreateDepthStencilView</description></item>
		/// <item><description>ID3D10Device1::CreateShaderResourceView1</description></item>
		/// <item><description>ID3D11Device::CreateShaderResourceView</description></item>
		/// <item><description>ID3D11Device::CreateUnorderedAccessView</description></item>
		/// <item><description>ID3D11Device::CreateRenderTargetView</description></item>
		/// <item><description>ID3D11Device::CreateDepthStencilView</description></item>
		/// <item><description>ID3D11Device3::CreateShaderResourceView1</description></item>
		/// <item><description>ID3D11Device3::CreateUnorderedAccessView1</description></item>
		/// <item><description>ID3D11Device3::CreateRenderTargetView1</description></item>
		/// <item><description>ID3D12Device::CreateShaderResourceView</description></item>
		/// <item><description>ID3D12Device::CreateUnorderedAccessView</description></item>
		/// <item><description>ID3D12Device::CreateRenderTargetView</description></item>
		/// <item><description>ID3D12Device::CreateDepthStencilView</description></item>
		/// <item><description>glTexBuffer</description></item>
		/// <item><description>glTextureBuffer</description></item>
		/// <item><description>glTextureView</description></item>
		/// <item><description>vkCreateBufferView</description></item>
		/// <item><description>vkCreateImageView</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &amp;desc, api::resource_view view)</c></para>
		/// </summary>
		init_resource_view,

		/// <summary>
		/// Called on resource view creation, before:
		/// <list type="bullet">
		/// <item><description>ID3D10Device::CreateShaderResourceView</description></item>
		/// <item><description>ID3D10Device::CreateRenderTargetView</description></item>
		/// <item><description>ID3D10Device::CreateDepthStencilView</description></item>
		/// <item><description>ID3D10Device1::CreateShaderResourceView1</description></item>
		/// <item><description>ID3D11Device::CreateShaderResourceView</description></item>
		/// <item><description>ID3D11Device::CreateUnorderedAccessView</description></item>
		/// <item><description>ID3D11Device::CreateRenderTargetView</description></item>
		/// <item><description>ID3D11Device::CreateDepthStencilView</description></item>
		/// <item><description>ID3D11Device3::CreateShaderResourceView1</description></item>
		/// <item><description>ID3D11Device3::CreateUnorderedAccessView1</description></item>
		/// <item><description>ID3D11Device3::CreateRenderTargetView1</description></item>
		/// <item><description>ID3D12Device::CreateShaderResourceView</description></item>
		/// <item><description>ID3D12Device::CreateUnorderedAccessView</description></item>
		/// <item><description>ID3D12Device::CreateRenderTargetView</description></item>
		/// <item><description>ID3D12Device::CreateDepthStencilView</description></item>
		/// <item><description>glTexBuffer</description></item>
		/// <item><description>glTextureBuffer</description></item>
		/// <item><description>glTextureView</description></item>
		/// <item><description>vkCreateBufferView</description></item>
		/// <item><description>vkCreateImageView</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::device *device, api::resource resource, api::resource_usage usage_type, api::resource_view_desc &amp;desc)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the resource view description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Is not called in D3D9 (since resource views are tied to resources there).
		/// </remarks>
		create_resource_view,

		/// <summary>
		/// Called on resource view destruction, before:
		/// <list type="bullet">
		/// <item><description>IDirect3DResource9::Release</description></item>
		/// <item><description>ID3D10View::Release</description></item>
		/// <item><description>ID3D11View::Release</description></item>
		/// <item><description>glDeleteTextures</description></item>
		/// <item><description>vkDestroyBufferView</description></item>
		/// <item><description>vkDestroyImageView</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::resource_view view)</c></para>
		/// </summary>
		/// <remarks>
		/// Is not called in D3D12 (since resource views are descriptor handles instead of objects there).
		/// </remarks>
		destroy_resource_view,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DVertexBuffer9::Lock</description></item>
		/// <item><description>IDirect3DIndexBuffer9::Lock</description></item>
		/// <item><description>ID3D10Resource::Map</description></item>
		/// <item><description>ID3D11DeviceContext::Map</description></item>
		/// <item><description>ID3D12Resource::Map</description></item>
		/// <item><description>glMapBuffer</description></item>
		/// <item><description>glMapBufferRange</description></item>
		/// <item><description>glMapNamedBuffer</description></item>
		/// <item><description>glMapNamedBufferRange</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::resource resource, uint64_t offset, uint64_t size, api::map_access access, void **data)</c></para>
		/// </summary>
		map_buffer_region,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>IDirect3DVertexBuffer9::Unlock</description></item>
		/// <item><description>IDirect3DIndexBuffer9::Unlock</description></item>
		/// <item><description>ID3D10Resource::Unmap</description></item>
		/// <item><description>ID3D11DeviceContext::Unmap</description></item>
		/// <item><description>ID3D12Resource::Unmap</description></item>
		/// <item><description>glUnmapBuffer</description></item>
		/// <item><description>glUnmapNamedBuffer</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::resource resource)</c></para>
		/// </summary>
		unmap_buffer_region,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DSurface9::LockRect</description></item>
		/// <item><description>IDirect3DVolume9::LockBox</description></item>
		/// <item><description>IDirect3DTexture9::LockRect</description></item>
		/// <item><description>IDirect3DVolumeTexture9::LockBox</description></item>
		/// <item><description>IDirect3DCubeTexture9::LockRect</description></item>
		/// <item><description>ID3D10Resource::Map</description></item>
		/// <item><description>ID3D11DeviceContext::Map</description></item>
		/// <item><description>ID3D12Resource::Map</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::resource resource, uint32_t subresource, const api::subresource_box *box, api::map_access access, api::subresource_data *data)</c></para>
		/// </summary>
		map_texture_region,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>IDirect3DSurface9::UnlockRect</description></item>
		/// <item><description>IDirect3DVolume9::UnlockBox</description></item>
		/// <item><description>IDirect3DTexture9::UnlockRect</description></item>
		/// <item><description>IDirect3DVolumeTexture9::UnlockBox</description></item>
		/// <item><description>IDirect3DCubeTexture9::UnlockRect</description></item>
		/// <item><description>ID3D10Resource::Unmap</description></item>
		/// <item><description>ID3D11DeviceContext::Unmap</description></item>
		/// <item><description>ID3D12Resource::Unmap</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::resource resource, uint32_t subresource)</c></para>
		/// </summary>
		unmap_texture_region,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D10Device::UpdateSubresource</description></item>
		/// <item><description>ID3D11DeviceContext::UpdateSubresource</description></item>
		/// <item><description>glBufferSubData</description></item>
		/// <item><description>glNamedBufferSubData</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::device *device, const void *data, api::resource resource, uint64_t offset, uint64_t size)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		update_buffer_region,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D10Device::UpdateSubresource</description></item>
		/// <item><description>ID3D11DeviceContext::UpdateSubresource</description></item>
		/// <item><description>glTexSubData1D</description></item>
		/// <item><description>glTexSubData2D</description></item>
		/// <item><description>glTexSubData3D</description></item>
		/// <item><description>glTextureSubData1D</description></item>
		/// <item><description>glTextureSubData2D</description></item>
		/// <item><description>glTextureSubData3D</description></item>
		/// <item><description>glCompressedTexSubData1D</description></item>
		/// <item><description>glCompressedTexSubData2D</description></item>
		/// <item><description>glCompressedTexSubData3D</description></item>
		/// <item><description>glCompressedTextureSubData1D</description></item>
		/// <item><description>glCompressedTextureSubData2D</description></item>
		/// <item><description>glCompressedTextureSubData3D</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::device *device, const api::subresource_data &data, api::resource resource, uint32_t subresource, const api::subresource_box *box)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		update_texture_region,

		/// <summary>
		/// Called after successfull pipeline creation from:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::CreateVertexShader</description></item>
		/// <item><description>IDirect3DDevice9::CreatePixelShader</description></item>
		/// <item><description>IDirect3DDevice9::CreateVertexDeclaration</description></item>
		/// <item><description>ID3D10Device::CreateVertexShader</description></item>
		/// <item><description>ID3D10Device::CreateGeometryShader</description></item>
		/// <item><description>ID3D10Device::CreateGeometryShaderWithStreamOutput</description></item>
		/// <item><description>ID3D10Device::CreatePixelShader</description></item>
		/// <item><description>ID3D10Device::CreateInputLayout</description></item>
		/// <item><description>ID3D10Device::CreateBlendState</description></item>
		/// <item><description>ID3D10Device::CreateDepthStencilState</description></item>
		/// <item><description>ID3D10Device::CreateRasterizerState</description></item>
		/// <item><description>ID3D10Device1::CreateBlendState1</description></item>
		/// <item><description>ID3D11Device::CreateVertexShader</description></item>
		/// <item><description>ID3D11Device::CreateHullShader</description></item>
		/// <item><description>ID3D11Device::CreateDomainShader</description></item>
		/// <item><description>ID3D11Device::CreateGeometryShader</description></item>
		/// <item><description>ID3D11Device::CreateGeometryShaderWithStreamOutput</description></item>
		/// <item><description>ID3D11Device::CreatePixelShader</description></item>
		/// <item><description>ID3D11Device::CreateComputeShader</description></item>
		/// <item><description>ID3D11Device::CreateInputLayout</description></item>
		/// <item><description>ID3D11Device::CreateBlendState</description></item>
		/// <item><description>ID3D11Device::CreateDepthStencilState</description></item>
		/// <item><description>ID3D11Device::CreateRasterizerState</description></item>
		/// <item><description>ID3D11Device1::CreateBlendState1</description></item>
		/// <item><description>ID3D11Device1::CreateRasterizerState1</description></item>
		/// <item><description>ID3D11Device3::CreateRasterizerState2</description></item>
		/// <item><description>ID3D12Device::CreateComputePipelineState</description></item>
		/// <item><description>ID3D12Device::CreateGraphicsPipelineState</description></item>
		/// <item><description>ID3D12Device2::CreatePipelineState</description></item>
		/// <item><description>ID3D12PipelineLibrary::LoadComputePipeline</description></item>
		/// <item><description>ID3D12PipelineLibrary::LoadGraphicsPipeline</description></item>
		/// <item><description>ID3D12PipelineLibrary1::LoadPipeline</description></item>
		/// <item><description>glLinkProgram</description></item>
		/// <item><description>vkCreateComputePipelines</description></item>
		/// <item><description>vkCreateGraphicsPipelines</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::pipeline_layout layout, uint32_t subobject_count, const api::pipeline_subobject *subobjects, api::pipeline pipeline)</c></para>
		/// </summary>
		/// <remarks>
		/// May be called multiple times with the same pipeline handle (whenever the pipeline is updated or its reference count is incremented).
		/// </remarks>
		init_pipeline,

		/// <summary>
		/// Called on pipeline creation, before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::CreateVertexShader</description></item>
		/// <item><description>IDirect3DDevice9::CreatePixelShader</description></item>
		/// <item><description>IDirect3DDevice9::CreateVertexDeclaration</description></item>
		/// <item><description>ID3D10Device::CreateVertexShader</description></item>
		/// <item><description>ID3D10Device::CreateGeometryShader</description></item>
		/// <item><description>ID3D10Device::CreateGeometryShaderWithStreamOutput</description></item>
		/// <item><description>ID3D10Device::CreatePixelShader</description></item>
		/// <item><description>ID3D10Device::CreateInputLayout</description></item>
		/// <item><description>ID3D10Device::CreateBlendState</description></item>
		/// <item><description>ID3D10Device::CreateDepthStencilState</description></item>
		/// <item><description>ID3D10Device::CreateRasterizerState</description></item>
		/// <item><description>ID3D10Device1::CreateBlendState1</description></item>
		/// <item><description>ID3D11Device::CreateVertexShader</description></item>
		/// <item><description>ID3D11Device::CreateHullShader</description></item>
		/// <item><description>ID3D11Device::CreateDomainShader</description></item>
		/// <item><description>ID3D11Device::CreateGeometryShader</description></item>
		/// <item><description>ID3D11Device::CreateGeometryShaderWithStreamOutput</description></item>
		/// <item><description>ID3D11Device::CreatePixelShader</description></item>
		/// <item><description>ID3D11Device::CreateComputeShader</description></item>
		/// <item><description>ID3D11Device::CreateInputLayout</description></item>
		/// <item><description>ID3D11Device::CreateBlendState</description></item>
		/// <item><description>ID3D11Device::CreateDepthStencilState</description></item>
		/// <item><description>ID3D11Device::CreateRasterizerState</description></item>
		/// <item><description>ID3D11Device1::CreateBlendState1</description></item>
		/// <item><description>ID3D11Device1::CreateRasterizerState1</description></item>
		/// <item><description>ID3D11Device3::CreateRasterizerState2</description></item>
		/// <item><description>ID3D12Device::CreateComputePipelineState</description></item>
		/// <item><description>ID3D12Device::CreateGraphicsPipelineState</description></item>
		/// <item><description>ID3D12Device2::CreatePipelineState</description></item>
		/// <item><description>glShaderSource</description></item>
		/// <item><description>vkCreateComputePipelines</description></item>
		/// <item><description>vkCreateGraphicsPipelines</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::device *device, api::pipeline_layout layout, uint32_t subobject_count, const api::pipeline_subobject *subobjects)</c></para>
		/// </summary>
		/// <remarks>
		/// To overwrite the pipeline description, modify <c>desc</c> in the callback and return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		create_pipeline,

		/// <summary>
		/// Called on pipeline destruction, before:
		/// <list type="bullet">
		/// <item><description>ID3D10VertexShader::Release</description></item>
		/// <item><description>ID3D10GeometryShader::Release</description></item>
		/// <item><description>ID3D10PixelShader::Release</description></item>
		/// <item><description>ID3D10InputLayout::Release</description></item>
		/// <item><description>ID3D10BlendState::Release</description></item>
		/// <item><description>ID3D10DepthStencilState::Release</description></item>
		/// <item><description>ID3D10RasterizerState::Release</description></item>
		/// <item><description>ID3D11VertexShader::Release</description></item>
		/// <item><description>ID3D11HullShader::Release</description></item>
		/// <item><description>ID3D11DomainShader::Release</description></item>
		/// <item><description>ID3D11GeometryShader::Release</description></item>
		/// <item><description>ID3D11PixelShader::Release</description></item>
		/// <item><description>ID3D11ComputeShader::Release</description></item>
		/// <item><description>ID3D11InputLayout::Release</description></item>
		/// <item><description>ID3D11BlendState::Release</description></item>
		/// <item><description>ID3D11DepthStencilState::Release</description></item>
		/// <item><description>ID3D11RasterizerState::Release</description></item>
		/// <item><description>ID3D12PipelineState::Release</description></item>
		/// <item><description>glDeleteProgram</description></item>
		/// <item><description>vkDestroyPipeline</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::pipeline pipeline)</c></para>
		/// </summary>
		/// <remarks>
		/// Is not called in D3D9.
		/// </remarks>
		destroy_pipeline,

		/// <summary>
		/// Called after successfull pipeline layout creation from:
		/// <list type="bullet">
		/// <item><description>ID3D12Device::CreateRootSignature</description></item>
		/// <item><description>vkCreatePipelineLayout</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout layout)</c></para>
		/// </summary>
		init_pipeline_layout,

		/// <summary>
		/// Called on pipeline layout creation.
		/// <para>Callback function signature: <c>bool (api::device *device, uint32_t param_count, const api::pipeline_layout_param *params)</c></para>
		/// </summary>
		create_pipeline_layout,

		/// <summary>
		/// Called on pipeline layout destruction, before:
		/// <list type="bullet">
		/// <item><description>ID3D12RootSignature::Release</description></item>
		/// <item><description>VkDestroyPipelineLayout</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::pipeline_layout layout)</c></para>
		/// </summary>
		destroy_pipeline_layout,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D12Device::CopyDescriptors</description></item>
		/// <item><description>ID3D12Device::CopyDescriptorsSimple</description></item>
		/// <item><description>vkUpdateDescriptorSets</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::device *device, uint32_t count, const api::descriptor_set_copy *copies)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		copy_descriptor_sets,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D12Device::CreateConstantBufferView</description></item>
		/// <item><description>ID3D12Device::CreateShaderResourceView</description></item>
		/// <item><description>ID3D12Device::CreateUnorderedAccessView</description></item>
		/// <item><description>ID3D12Device::CreateSampler</description></item>
		/// <item><description>vkUpdateDescriptorSets</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::device *device, uint32_t count, const api::descriptor_set_update *updates)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		update_descriptor_sets,

		/// <summary>
		/// Called after successfull query pool creation from:
		/// <list type="bullet">
		/// <item><description>ID3D12Device::CreateQueryHeap</description></item>
		/// <item><description>vkCreateQueryPool</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::query_type type, uint32_t size, api::query_pool pool)</c></para>
		/// </summary>
		init_query_pool,

		/// <summary>
		/// Called on query pool creation, before:
		/// <list type="bullet">
		/// <item><description>ID3D12Device::CreateQueryHeap</description></item>
		/// <item><description>vkCreateQueryPool</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::device *device, api::query_type type, uint32_t &amp;size)</c></para>
		/// </summary>
		create_query_pool,

		/// <summary>
		/// Called on query pool destruction, before:
		/// <list type="bullet">
		/// <item><description>ID3D12QueryHeap::Release</description></item>
		/// <item><description>vkDestroyQueryPool</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::device *device, api::query_pool pool)</c></para>
		/// </summary>
		destroy_query_pool,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>vkGetQueryPoolResults</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::device *device, api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride)</c></para>
		/// </summary>
		get_query_pool_results,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>ID3D12GraphicsCommandList::ResourceBarrier</description></item>
		/// <item><description>ID3D12GraphicsCommandList7::Barrier</description></item>
		/// <item><description>vkCmdPipelineBarrier</description></item>
		/// <item><description>vkCmdPipelineBarrier2</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states)</c></para>
		/// </summary>
		barrier,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D12GraphicsCommandList4::BeginRenderPass</description></item>
		/// <item><description>vkCmdBeginRenderPass</description></item>
		/// <item><description>vkCmdBeginRenderPass2</description></item>
		/// <item><description>vkCmdNextSubpass</description></item>
		/// <item><description>vkCmdNextSubpass2</description></item>
		/// <item><description>vkCmdBeginRendering</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds)</c></para>
		/// </summary>
		begin_render_pass,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D12GraphicsCommandList4::EndRenderPass</description></item>
		/// <item><description>vkCmdEndRenderPass</description></item>
		/// <item><description>vkCmdEndRenderPass2</description></item>
		/// <item><description>vkCmdNextSubpass</description></item>
		/// <item><description>vkCmdNextSubpass2</description></item>
		/// <item><description>vkCmdEndRendering</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		end_render_pass,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::SetRenderTarget</description></item>
		/// <item><description>IDirect3DDevice9::SetDepthStencilSurface</description></item>
		/// <item><description>ID3D10Device::OMSetRenderTargets</description></item>
		/// <item><description>ID3D11DeviceContext::OMSetRenderTargets</description></item>
		/// <item><description>ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews</description></item>
		/// <item><description>ID3D12GraphicsCommandList::OMSetRenderTargets</description></item>
		/// <item><description>glBindFramebuffer</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)</c></para>
		/// </summary>
		bind_render_targets_and_depth_stencil,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::SetVertexShader</description></item>
		/// <item><description>IDirect3DDevice9::SetPixelShader</description></item>
		/// <item><description>IDirect3DDevice9::SetVertexDeclaration</description></item>
		/// <item><description>IDirect3DDevice9::ProcessVertices</description></item>
		/// <item><description>ID3D10Device::VSSetShader</description></item>
		/// <item><description>ID3D10Device::GSSetShader</description></item>
		/// <item><description>ID3D10Device::PSSetShader</description></item>
		/// <item><description>ID3D10Device::IASetInputLayout</description></item>
		/// <item><description>ID3D10Device::OMSetBlendState</description></item>
		/// <item><description>ID3D10Device::OMSetDepthStencilState</description></item>
		/// <item><description>ID3D10Device::RSSetState</description></item>
		/// <item><description>ID3D11DeviceContext::VSSetShader</description></item>
		/// <item><description>ID3D11DeviceContext::HSSetShader</description></item>
		/// <item><description>ID3D11DeviceContext::DSSetShader</description></item>
		/// <item><description>ID3D11DeviceContext::GSSetShader</description></item>
		/// <item><description>ID3D11DeviceContext::PSSetShader</description></item>
		/// <item><description>ID3D11DeviceContext::CSSetShader</description></item>
		/// <item><description>ID3D11DeviceContext::IASetInputLayout</description></item>
		/// <item><description>ID3D11DeviceContext::OMSetBlendState</description></item>
		/// <item><description>ID3D11DeviceContext::OMSetDepthStencilState</description></item>
		/// <item><description>ID3D11DeviceContext::RSSetState</description></item>
		/// <item><description>ID3D12GraphicsCommandList::Reset</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetPipelineState</description></item>
		/// <item><description>glUseProgram</description></item>
		/// <item><description>vkCmdBindPipeline</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::pipeline_stage stages, api::pipeline pipeline)</c></para>
		/// </summary>
		bind_pipeline,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::SetRenderState</description></item>
		/// <item><description>ID3D10Device::IASetPrimitiveTopology</description></item>
		/// <item><description>ID3D10Device::OMSetBlendState</description></item>
		/// <item><description>ID3D10Device::OMSetDepthStencilState</description></item>
		/// <item><description>ID3D11DeviceContext::IASetPrimitiveTopology</description></item>
		/// <item><description>ID3D11DeviceContext::OMSetBlendState</description></item>
		/// <item><description>ID3D11DeviceContext::OMSetDepthStencilState</description></item>
		/// <item><description>ID3D12GraphicsCommandList::IASetPrimitiveTopology</description></item>
		/// <item><description>ID3D12GraphicsCommandList::OMSetBlendFactor</description></item>
		/// <item><description>ID3D12GraphicsCommandList::OMSetStencilRef</description></item>
		/// <item><description>gl(...)</description></item>
		/// <item><description>vkCmdSetDepthBias</description></item>
		/// <item><description>vkCmdSetBlendConstants</description></item>
		/// <item><description>vkCmdSetStencilCompareMask</description></item>
		/// <item><description>vkCmdSetStencilWriteMask</description></item>
		/// <item><description>vkCmdSetStencilReference</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t count, const api::dynamic_state *states, const uint32_t *values)</c></para>
		/// </summary>
		bind_pipeline_states,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::SetViewport</description></item>
		/// <item><description>IDirect3DDevice9::SetRenderTarget (implicitly updates the viewport)</description></item>
		/// <item><description>ID3D10Device::RSSetViewports</description></item>
		/// <item><description>ID3D11DeviceContext::RSSetViewports</description></item>
		/// <item><description>ID3D12GraphicsCommandList::RSSetViewports</description></item>
		/// <item><description>glViewport</description></item>
		/// <item><description>glViewportArrayv</description></item>
		/// <item><description>glViewportIndexedf</description></item>
		/// <item><description>glViewportIndexedfv</description></item>
		/// <item><description>vkCmdSetViewport</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const api::viewport *viewports)</c></para>
		/// </summary>
		bind_viewports,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::SetScissorRect</description></item>
		/// <item><description>ID3D10Device::RSSetScissorRects</description></item>
		/// <item><description>ID3D11DeviceContext::RSSetScissorRects</description></item>
		/// <item><description>ID3D12GraphicsCommandList::RSSetScissorRects</description></item>
		/// <item><description>glScissor</description></item>
		/// <item><description>glScissorArrayv</description></item>
		/// <item><description>glScissorIndexed</description></item>
		/// <item><description>glScissorIndexedv</description></item>
		/// <item><description>vkCmdSetScissor</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const api::rect *rects)</c></para>
		/// </summary>
		bind_scissor_rects,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::SetVertexShaderConstantF</description></item>
		/// <item><description>IDirect3DDevice9::SetPixelShaderConstantF</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetComputeRoot32BitConstant</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetComputeRoot32BitConstants</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstant</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetGraphicsRoot32BitConstants</description></item>
		/// <item><description>glUniform(...)</description></item>
		/// <item><description>vkCmdPushConstants</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const uint32_t *values)</c></para>
		/// </summary>
		push_constants,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::SetTexture</description></item>
		/// <item><description>ID3D10Device::VSSetSamplers</description></item>
		/// <item><description>ID3D10Device::VSSetShaderResources</description></item>
		/// <item><description>ID3D10Device::VSSetConstantBuffers</description></item>
		/// <item><description>ID3D10Device::GSSetSamplers</description></item>
		/// <item><description>ID3D10Device::GSSetShaderResources</description></item>
		/// <item><description>ID3D10Device::GSSetConstantBuffers</description></item>
		/// <item><description>ID3D10Device::PSSetSamplers</description></item>
		/// <item><description>ID3D10Device::PSSetShaderResources</description></item>
		/// <item><description>ID3D10Device::PSSetConstantBuffers</description></item>
		/// <item><description>ID3D11DeviceContext::VSSetSamplers</description></item>
		/// <item><description>ID3D11DeviceContext::VSSetShaderResources</description></item>
		/// <item><description>ID3D11DeviceContext::VSSetConstantBuffers</description></item>
		/// <item><description>ID3D11DeviceContext::HSSetSamplers</description></item>
		/// <item><description>ID3D11DeviceContext::HSSetShaderResources</description></item>
		/// <item><description>ID3D11DeviceContext::HSSetConstantBuffers</description></item>
		/// <item><description>ID3D11DeviceContext::DSSetSamplers</description></item>
		/// <item><description>ID3D11DeviceContext::DSSetShaderResources</description></item>
		/// <item><description>ID3D11DeviceContext::DSSetConstantBuffers</description></item>
		/// <item><description>ID3D11DeviceContext::GSSetSamplers</description></item>
		/// <item><description>ID3D11DeviceContext::GSSetShaderResources</description></item>
		/// <item><description>ID3D11DeviceContext::GSSetConstantBuffers</description></item>
		/// <item><description>ID3D11DeviceContext::PSSetSamplers</description></item>
		/// <item><description>ID3D11DeviceContext::PSSetShaderResources</description></item>
		/// <item><description>ID3D11DeviceContext::PSSetConstantBuffers</description></item>
		/// <item><description>ID3D11DeviceContext::CSSetSamplers</description></item>
		/// <item><description>ID3D11DeviceContext::CSSetShaderResources</description></item>
		/// <item><description>ID3D11DeviceContext::CSSetUnorderedAccessViews</description></item>
		/// <item><description>ID3D11DeviceContext::CSSetConstantBuffers</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetComputeRootConstantBufferView</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetComputeRootShaderResourceView</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetComputeRootUnorderedAccessView</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView</description></item>
		/// <item><description>glBindBufferBase</description></item>
		/// <item><description>glBindBufferRange</description></item>
		/// <item><description>glBindBuffersBase</description></item>
		/// <item><description>glBindBuffersRange</description></item>
		/// <item><description>glBindTexture</description></item>
		/// <item><description>glBindImageTexture</description></item>
		/// <item><description>glBindTextures</description></item>
		/// <item><description>glBindImageTextures</description></item>
		/// <item><description>glBindTextureUnit</description></item>
		/// <item><description>glBindMultiTextureEXT</description></item>
		/// <item><description>vkCmdPushDescriptorSetKHR</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_set_update &amp;update)</c></para>
		/// </summary>
		push_descriptors,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>ID3D12GraphicsCommandList::SetComputeRootSignature</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetGraphicsRootSignature</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetComputeRootDescriptorTable</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable</description></item>
		/// <item><description>vkCmdBindDescriptorSets</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)</c></para>
		/// </summary>
		bind_descriptor_sets,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::SetIndices</description></item>
		/// <item><description>ID3D10Device::IASetIndexBuffer</description></item>
		/// <item><description>ID3D11DeviceContext::IASetIndexBuffer</description></item>
		/// <item><description>ID3D12GraphicsCommandList::IASetIndexBuffer</description></item>
		/// <item><description>glBindBuffer</description></item>
		/// <item><description>vkCmdBindIndexBuffer</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::resource buffer, uint64_t offset, uint32_t index_size)</c></para>
		/// </summary>
		bind_index_buffer,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::SetStreamSource</description></item>
		/// <item><description>ID3D10Device::IASetVertexBuffers</description></item>
		/// <item><description>ID3D11DeviceContext::IASetVertexBuffers</description></item>
		/// <item><description>ID3D12GraphicsCommandList::IASetVertexBuffers</description></item>
		/// <item><description>glBindBuffer</description></item>
		/// <item><description>glBindVertexBuffer</description></item>
		/// <item><description>glBindVertexBuffers</description></item>
		/// <item><description>vkCmdBindVertexBuffers</description></item>
		/// <item><description>vkCmdBindVertexBuffers2</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)</c></para>
		/// </summary>
		bind_vertex_buffers,

		/// <summary>
		/// Called after:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::ProcessVertices</description></item>
		/// <item><description>ID3D10Device::SOSetTargets</description></item>
		/// <item><description>ID3D11DeviceContext::SOSetTargets</description></item>
		/// <item><description>ID3D12GraphicsCommandList::SOSetTargets</description></item>
		/// <item><description>glBindBufferBase</description></item>
		/// <item><description>glBindBufferRange</description></item>
		/// <item><description>glBindBuffersBase</description></item>
		/// <item><description>glBindBuffersRange</description></item>
		/// <item><description>vkCmdBindTransformFeedbackBuffersEXT</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint64_t *max_sizes)</c></para>
		/// </summary>
		bind_stream_output_buffers,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::DrawPrimitive</description></item>
		/// <item><description>IDirect3DDevice9::DrawPrimitiveUP</description></item>
		/// <item><description>IDirect3DDevice9::ProcessVertices</description></item>
		/// <item><description>ID3D10Device::Draw</description></item>
		/// <item><description>ID3D10Device::DrawInstanced</description></item>
		/// <item><description>ID3D11DeviceContext::Draw</description></item>
		/// <item><description>ID3D11DeviceContext::DrawInstanced</description></item>
		/// <item><description>ID3D12GraphicsCommandList::DrawInstanced</description></item>
		/// <item><description>glDrawArrays</description></item>
		/// <item><description>glDrawArraysInstanced</description></item>
		/// <item><description>glDrawArraysInstancedBaseInstance</description></item>
		/// <item><description>glMultiDrawArrays</description></item>
		/// <item><description>vkCmdDraw</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		draw,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::DrawIndexedPrimitive</description></item>
		/// <item><description>IDirect3DDevice9::DrawIndexedPrimitiveUP</description></item>
		/// <item><description>ID3D10Device::DrawIndexed</description></item>
		/// <item><description>ID3D10Device::DrawIndexedInstanced</description></item>
		/// <item><description>ID3D11DeviceContext::DrawIndexed</description></item>
		/// <item><description>ID3D11DeviceContext::DrawIndexedInstanced</description></item>
		/// <item><description>ID3D12GraphicsCommandList::DrawIndexedInstanced</description></item>
		/// <item><description>glDrawElements</description></item>
		/// <item><description>glDrawElementsBaseVertex</description></item>
		/// <item><description>glDrawElementsInstanced</description></item>
		/// <item><description>glDrawElementsInstancedBaseVertex</description></item>
		/// <item><description>glDrawElementsInstancedBaseInstance</description></item>
		/// <item><description>glDrawElementsInstancedBaseVertexBaseInstance</description></item>
		/// <item><description>glMultiDrawElements</description></item>
		/// <item><description>glMultiDrawElementsBaseVertex</description></item>
		/// <item><description>vkCmdDrawIndexed</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		draw_indexed,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D11DeviceContext::Dispatch</description></item>
		/// <item><description>ID3D12GraphicsCommandList::Dispatch</description></item>
		/// <item><description>glDispatchCompute</description></item>
		/// <item><description>vkCmdDispatch</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		dispatch,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D11DeviceContext::DrawInstancedIndirect</description></item>
		/// <item><description>ID3D11DeviceContext::DrawIndexedInstancedIndirect</description></item>
		/// <item><description>ID3D11DeviceContext::DispatchIndirect</description></item>
		/// <item><description>ID3D12GraphicsCommandList::ExecuteIndirect</description></item>
		/// <item><description>glDrawArraysIndirect</description></item>
		/// <item><description>glDrawElementsIndirect</description></item>
		/// <item><description>glMultiDrawArraysIndirect</description></item>
		/// <item><description>glMultiDrawElementsIndirect</description></item>
		/// <item><description>glDispatchComputeIndirect</description></item>
		/// <item><description>vkCmdDrawIndirect</description></item>
		/// <item><description>vkCmdDrawIndexedIndirect</description></item>
		/// <item><description>vkCmdDispatchIndirect</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		draw_or_dispatch_indirect,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::UpdateTexture</description></item>
		/// <item><description>IDirect3DDevice9::GetRenderTargetData</description></item>
		/// <item><description>ID3D10Device::CopyResource</description></item>
		/// <item><description>ID3D11DeviceContext::CopyResource</description></item>
		/// <item><description>ID3D12GraphicsCommandList::CopyResource</description></item>
		/// </list>
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
		/// <list type="bullet">
		/// <item><description>ID3D12GraphicsCommandList::CopyBufferRegion</description></item>
		/// <item><description>glCopyBufferSubData</description></item>
		/// <item><description>glCopyNamedBufferSubData</description></item>
		/// <item><description>vkCmdCopyBuffer</description></item>
		/// <item><description>vkCmdCopyBuffer2</description></item>
		/// </list>
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
		/// <list type="bullet">
		/// <item><description>ID3D12GraphicsCommandList::CopyTextureRegion</description></item>
		/// <item><description>vkCmdCopyBufferToImage</description></item>
		/// <item><description>vkCmdCopyBufferToImage2</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource source, uint64_t source_offset, uint32_t row_length, uint32_t slice_height, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="api::resource_usage::copy_source"/> state.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_buffer_to_texture,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::UpdateSurface</description></item>
		/// <item><description>IDirect3DDevice9::StretchRect</description></item>
		/// <item><description>ID3D10Device::CopySubresourceRegion</description></item>
		/// <item><description>ID3D11DeviceContext::CopySubresourceRegion</description></item>
		/// <item><description>ID3D12GraphicsCommandList::CopyTextureRegion</description></item>
		/// <item><description>glBlitFramebuffer</description></item>
		/// <item><description>glBlitNamedFramebuffer</description></item>
		/// <item><description>glCopyImageSubData</description></item>
		/// <item><description>glCopyTexSubImage1D</description></item>
		/// <item><description>glCopyTexSubImage2D</description></item>
		/// <item><description>glCopyTexSubImage3D</description></item>
		/// <item><description>glCopyTextureSubImage1D</description></item>
		/// <item><description>glCopyTextureSubImage2D</description></item>
		/// <item><description>glCopyTextureSubImage3D</description></item>
		/// <item><description>vkCmdBlitImage</description></item>
		/// <item><description>vkCmdBlitImage2</description></item>
		/// <item><description>vkCmdCopyImage</description></item>
		/// <item><description>vkCmdCopyImage2</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box, api::filter_mode filter)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="api::resource_usage::copy_source"/> state.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_texture_region,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D12GraphicsCommandList::CopyTextureRegion</description></item>
		/// <item><description>vkCmdCopyImageToBuffer</description></item>
		/// <item><description>vkCmdCopyImageToBuffer2</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint64_t dest_offset, uint32_t row_length, uint32_t slice_height)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="api::resource_usage::copy_source"/> state.
		/// Destination resource will be in the <see cref="api::resource_usage::copy_dest"/> state.
		/// </remarks>
		copy_texture_to_buffer,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::StretchRect</description></item>
		/// <item><description>ID3D10Device::ResolveSubresource</description></item>
		/// <item><description>ID3D11DeviceContext::ResolveSubresource</description></item>
		/// <item><description>ID3D12GraphicsCommandList::ResolveSubresource</description></item>
		/// <item><description>ID3D12GraphicsCommandList1::ResolveSubresourceRegion</description></item>
		/// <item><description>glBlitFramebuffer</description></item>
		/// <item><description>glBlitNamedFramebuffer</description></item>
		/// <item><description>vkCmdResolveImage</description></item>
		/// <item><description>vkCmdResolveImage2</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint32_t dest_subresource, int32_t dest_x, int32_t dest_y, int32_t dest_z, api::format format)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Source resource will be in the <see cref="api::resource_usage::resolve_source"/> state.
		/// Destination resource will be in the <see cref="api::resource_usage::resolve_dest"/> state.
		/// </remarks>
		resolve_texture_region,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::Clear</description></item>
		/// <item><description>ID3D10Device::ClearDepthStencilView</description></item>
		/// <item><description>ID3D11DeviceContext::ClearDepthStencilView</description></item>
		/// <item><description>ID3D11DeviceContext1::ClearView (for depth-stencil views)</description></item>
		/// <item><description>ID3D12GraphicsCommandList::ClearDepthStencilView</description></item>
		/// <item><description>glClear</description></item>
		/// <item><description>glClearBufferfi</description></item>
		/// <item><description>glClearBufferfv</description></item>
		/// <item><description>glClearNamedFramebufferfi</description></item>
		/// <item><description>glClearNamedFramebufferfv</description></item>
		/// <item><description>vkCmdClearDepthStencilImage</description></item>
		/// <item><description>vkCmdClearAttachments</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const api::rect *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resource will be in the <see cref="api::resource_usage::depth_stencil_write"/> state.
		/// </remarks>
		clear_depth_stencil_view,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::Clear</description></item>
		/// <item><description>IDirect3DDevice9::ColorFill</description></item>
		/// <item><description>ID3D10Device::ClearRenderTargetView</description></item>
		/// <item><description>ID3D11DeviceContext::ClearRenderTargetView</description></item>
		/// <item><description>ID3D11DeviceContext1::ClearView (for render target views)</description></item>
		/// <item><description>ID3D12GraphicsCommandList::ClearRenderTargetView</description></item>
		/// <item><description>glClear</description></item>
		/// <item><description>glClearBufferfv</description></item>
		/// <item><description>glClearNamedFramebufferfv</description></item>
		/// <item><description>vkCmdClearColorImage</description></item>
		/// <item><description>vkCmdClearAttachments</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view rtv, const float color[4], uint32_t rect_count, const api::rect *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resources will be in the <see cref="api::resource_usage::render_target"/> state.
		/// </remarks>
		clear_render_target_view,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D11DeviceContext::ClearUnorderedAccessViewUint</description></item>
		/// <item><description>ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view uav, const uint32_t values[4], uint32_t rect_count, const api::rect *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resource will be in the <see cref="api::resource_usage::unordered_access"/> state.
		/// </remarks>
		clear_unordered_access_view_uint,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D11DeviceContext::ClearUnorderedAccessViewFloat</description></item>
		/// <item><description>ID3D11DeviceContext1::ClearView (for unordered access views)</description></item>
		/// <item><description>ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view uav, const float values[4], uint32_t rect_count, const api::rect *rects)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// Resource will be in the <see cref="api::resource_usage::unordered_access"/> state.
		/// </remarks>
		clear_unordered_access_view_float,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D10Device::GenerateMips</description></item>
		/// <item><description>ID3D11DeviceContext::GenerateMips</description></item>
		/// <item><description>glGenerateMipmap</description></item>
		/// <item><description>glGenerateTextureMipmap</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::resource_view srv)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		generate_mipmaps,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D12GraphicsCommandList::BeginQuery</description></item>
		/// <item><description>vkCmdBeginQuery</description></item>
		/// <item><description>vkCmdBeginQueryIndexedEXT</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::query_pool pool, api::query_type type, uint32_t index)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		begin_query,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D12GraphicsCommandList::EndQuery</description></item>
		/// <item><description>vkCmdEndQuery</description></item>
		/// <item><description>vkCmdEndQueryIndexedEXT</description></item>
		/// <item><description>vkCmdWriteTimestamp</description></item>
		/// <item><description>vkCmdWriteTimestamp2</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::query_pool pool, api::query_type type, uint32_t index)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		end_query,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D12GraphicsCommandList::ResolveQueryData</description></item>
		/// <item><description>vkCmdCopyQueryPoolResults</description></item>
		/// </list>
		/// <para>Callback function signature: <c>bool (api::command_list *cmd_list, api::query_pool pool, api::query_type type, uint32_t first, uint32_t count, api::resource dest, uint64_t dest_offset, uint32_t stride)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent this command from being executed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		copy_query_pool_results,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D12GraphicsCommandList::Reset</description></item>
		/// <item><description>vkBeginCommandBuffer</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		reset_command_list,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>ID3D11DeviceContext::FinishCommandList</description></item>
		/// <item><description>ID3D12GraphicsCommandList::Close</description></item>
		/// <item><description>vkEndCommandBuffer</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list)</c></para>
		/// </summary>
		close_command_list,

		/// <summary>
		/// Called when a command list is submitted to a command queue (or an immediate command list is flushed), before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::EndScene</description></item>
		/// <item><description>ID3D10Device::Flush</description></item>
		/// <item><description>ID3D11DeviceContext::Flush</description></item>
		/// <item><description>ID3D12CommandQueue::ExecuteCommandLists</description></item>
		/// <item><description>glFlush</description></item>
		/// <item><description>vkQueueSubmit</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_queue *queue, api::command_list *cmd_list)</c></para>
		/// </summary>
		execute_command_list,

		/// <summary>
		/// Called when a secondary command list is executed on a primary command list, before:
		/// <list type="bullet">
		/// <item><description>ID3D11DeviceContext::ExecuteCommandList</description></item>
		/// <item><description>ID3D12GraphicsCommandList::ExecuteBundle</description></item>
		/// <item><description>vkCmdExecuteCommands</description></item>
		/// </list>
		/// In addition, called after:
		/// <list type="bullet">
		/// <item><description>ID3D11DeviceContext::FinishCommandList</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_list *cmd_list, api::command_list *secondary_cmd_list)</c></para>
		/// </summary>
		execute_secondary_command_list,

		/// <summary>
		/// Called before:
		/// <list type="bullet">
		/// <item><description>IDirect3DDevice9::Present</description></item>
		/// <item><description>IDirect3DDevice9Ex::PresentEx</description></item>
		/// <item><description>IDirect3DSwapChain9::Present</description></item>
		/// <item><description>IDXGISwapChain::Present</description></item>
		/// <item><description>IDXGISwapChain3::Present1</description></item>
		/// <item><description>ID3D12CommandQueueDownlevel::Present</description></item>
		/// <item><description>wglSwapBuffers</description></item>
		/// <item><description>vkQueuePresentKHR</description></item>
		/// <item><description>IVRCompositor::Submit</description></item>
		/// </list>
		/// <para>Callback function signature: <c>void (api::command_queue *queue, api::swapchain *swapchain, const api::rect *source_rect, const api::rect *dest_rect, uint32_t dirty_rect_count, const api::rect *dirty_rects)</c></para>
		/// </summary>
		present,

		/// <summary>
		/// Called after ReShade has rendered its overlay.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime)</c></para>
		/// </summary>
		reshade_present,

		/// <summary>
		/// Called right before ReShade effects are rendered.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)</c></para>
		/// </summary>
		reshade_begin_effects,

		/// <summary>
		/// Called right after ReShade effects were rendered.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)</c></para>
		/// </summary>
		reshade_finish_effects,

		/// <summary>
		/// Called right after all ReShade effects were reloaded.
		/// This occurs during effect runtime initialization or because the user pressed the "Reload" button in the overlay.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime)</c></para>
		/// </summary>
		reshade_reloaded_effects,

		/// <summary>
		/// Called before a uniform variable is changed.
		/// <para>Callback function signature: <c>bool (api::effect_runtime *runtime, api::effect_uniform_variable variable, const void *data, size_t size)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent the variable from being changed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		reshade_set_uniform_value,
		/// <summary>
		/// Called before a technique is enabled or disabled.
		/// <para>Callback function signature: <c>bool (api::effect_runtime *runtime, api::effect_technique technique, bool enabled)</c></para>
		/// </summary>
		/// <remarks>
		/// To prevent the technique from being changed, return <see langword="true"/>, otherwise return <see langword="false"/>.
		/// </remarks>
		reshade_set_technique_state,

		/// <summary>
		/// Called between the <c>ImGui::NewFrame</c> and <c>ImGui::EndFrame</c> calls for the ReShade overlay.
		/// Can be used to perform custom Dear ImGui calls, but it is recommended to instead use <see cref="register_overlay"/> to register a dedicated overlay.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime)</c></para>
		/// </summary>
		/// <remarks>
		/// This is not called for effect runtimes in VR.
		/// </remarks>
		reshade_overlay,

		/// <summary>
		/// Called after a screenshot was taken and saved to disk.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime, const char *filename)</c></para>
		/// </summary>
		reshade_screenshot,

		/// <summary>
		/// Called for each technique after it was rendered, usually between <see cref="reshade_begin_effects"/> and <see cref="reshade_finish_effects"/>.
		/// <para>Callback function signature: <c>void (api::effect_runtime *runtime, api::effect_technique technique, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)</c></para>
		/// </summary>
		reshade_render_technique,

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
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::create_swapchain, bool, api::swapchain_desc &desc, void *hwnd);
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

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::map_buffer_region, void, api::device *device, api::resource resource, uint64_t offset, uint64_t size, api::map_access access, void **data);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::unmap_buffer_region, void, api::device *device, api::resource resource);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::map_texture_region, void, api::device *device, api::resource resource, uint32_t subresource, const api::subresource_box *box, api::map_access access, api::subresource_data *data);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::unmap_texture_region, void, api::device *device, api::resource resource, uint32_t subresource);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::update_buffer_region, bool, api::device *device, const void *data, api::resource resource, uint64_t offset, uint64_t size);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::update_texture_region, bool, api::device *device, const api::subresource_data &data, api::resource resource, uint32_t subresource, const api::subresource_box *box);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_pipeline, void, api::device *device, api::pipeline_layout layout, uint32_t subobject_count, const api::pipeline_subobject *subobjects, api::pipeline pipeline);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::create_pipeline, bool, api::device *device, api::pipeline_layout layout, uint32_t subobject_count, const api::pipeline_subobject *subobjects);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_pipeline, void, api::device *device, api::pipeline pipeline);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_pipeline_layout, void, api::device *device, uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout layout);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::create_pipeline_layout, bool, api::device *device, uint32_t param_count, const api::pipeline_layout_param *params);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_pipeline_layout, void, api::device *device, api::pipeline_layout layout);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_descriptor_sets, bool, api::device *device, uint32_t count, const api::descriptor_set_copy *copies);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::update_descriptor_sets, bool, api::device *device, uint32_t count, const api::descriptor_set_update *updates);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::init_query_pool, void, api::device *device, api::query_type type, uint32_t size, api::query_pool pool);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::create_query_pool, bool, api::device *device, api::query_type type, uint32_t &size);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::destroy_query_pool, void, api::device *device, api::query_pool pool);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::get_query_pool_results, bool, api::device *device, api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::barrier, void, api::command_list *cmd_list, uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::begin_render_pass, void, api::command_list *cmd_list, uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::end_render_pass, void, api::command_list *cmd_list);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_render_targets_and_depth_stencil, void, api::command_list *cmd_list, uint32_t count, const api::resource_view *rtvs, api::resource_view dsv);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_pipeline, void, api::command_list *cmd_list, api::pipeline_stage stages, api::pipeline pipeline);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_pipeline_states, void, api::command_list *cmd_list, uint32_t count, const api::dynamic_state *states, const uint32_t *values);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_viewports, void, api::command_list *cmd_list, uint32_t first, uint32_t count, const api::viewport *viewports);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_scissor_rects, void, api::command_list *cmd_list, uint32_t first, uint32_t count, const api::rect *rects);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::push_constants, void, api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const uint32_t *values);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::push_descriptors, void, api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_set_update &update);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_descriptor_sets, void, api::command_list *cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_index_buffer, void, api::command_list *cmd_list, api::resource buffer, uint64_t offset, uint32_t index_size);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_vertex_buffers, void, api::command_list *cmd_list, uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::bind_stream_output_buffers, void, api::command_list *cmd_list, uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint64_t *max_sizes);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::draw, bool, api::command_list *cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::draw_indexed, bool, api::command_list *cmd_list, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::dispatch, bool, api::command_list *cmd_list, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::draw_or_dispatch_indirect, bool, api::command_list *cmd_list, api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_resource, bool, api::command_list *cmd_list, api::resource source, api::resource dest);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_buffer_region, bool, api::command_list *cmd_list, api::resource source, uint64_t source_offset, api::resource dest, uint64_t dest_offset, uint64_t size);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_buffer_to_texture, bool, api::command_list *cmd_list, api::resource source, uint64_t source_offset, uint32_t row_length, uint32_t slice_height, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_texture_region, bool, api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box, api::filter_mode filter);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_texture_to_buffer, bool, api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint64_t dest_offset, uint32_t row_length, uint32_t slice_height);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::resolve_texture_region, bool, api::command_list *cmd_list, api::resource source, uint32_t source_subresource, const api::subresource_box *source_box, api::resource dest, uint32_t dest_subresource, int32_t dest_x, int32_t dest_y, int32_t dest_z, api::format format);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::clear_depth_stencil_view, bool, api::command_list *cmd_list, api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const api::rect *rects);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::clear_render_target_view, bool, api::command_list *cmd_list, api::resource_view rtv, const float color[4], uint32_t rect_count, const api::rect *rects);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::clear_unordered_access_view_uint, bool, api::command_list *cmd_list, api::resource_view uav, const uint32_t values[4], uint32_t rect_count, const api::rect *rects);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::clear_unordered_access_view_float, bool, api::command_list *cmd_list, api::resource_view uav, const float values[4], uint32_t rect_count, const api::rect *rects);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::generate_mipmaps, bool, api::command_list *cmd_list, api::resource_view srv);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::begin_query, bool, api::command_list *cmd_list, api::query_pool pool, api::query_type type, uint32_t index);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::end_query, bool, api::command_list *cmd_list, api::query_pool pool, api::query_type type, uint32_t index);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::copy_query_pool_results, bool, api::command_list *cmd_list, api::query_pool pool, api::query_type type, uint32_t first, uint32_t count, api::resource dest, uint64_t dest_offset, uint32_t stride);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reset_command_list, void, api::command_list *cmd_list);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::close_command_list, void, api::command_list *cmd_list);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::execute_command_list, void, api::command_queue *queue, api::command_list *cmd_list);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::execute_secondary_command_list, void, api::command_list *cmd_list, api::command_list *secondary_cmd_list);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::present, void, api::command_queue *queue, api::swapchain *swapchain, const api::rect *source_rect, const api::rect *dest_rect, uint32_t dirty_rect_count, const api::rect *dirty_rects);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_present, void, api::effect_runtime *runtime);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_begin_effects, void, api::effect_runtime *runtime, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_finish_effects, void, api::effect_runtime *runtime, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_reloaded_effects, void, api::effect_runtime *runtime);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_set_uniform_value, bool, api::effect_runtime *runtime, api::effect_uniform_variable variable, const void *data, size_t size);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_set_technique_state, bool, api::effect_runtime *runtime, api::effect_technique technique, bool enabled);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_overlay, void, api::effect_runtime *runtime);
	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_screenshot, void, api::effect_runtime *runtime, const char *filename);

	RESHADE_DEFINE_ADDON_EVENT_TRAITS(addon_event::reshade_render_technique, void, api::effect_runtime *runtime, api::effect_technique technique, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb);
}
