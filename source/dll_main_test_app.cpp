/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef RESHADE_TEST_APPLICATION

#include "version.h"
#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "addon_manager.hpp"
#include "com_ptr.hpp"
#include "ini_file.hpp"
#include <d3d9.h>
#include <d3d11.h>
#include <d3d12.h>
#include <D3D12Downlevel.h>
#include <glad/wgl.h>
#include <glad/vulkan.h>

extern HMODULE g_module_handle;
extern std::filesystem::path g_reshade_dll_path;
extern std::filesystem::path g_reshade_base_path;
extern std::filesystem::path g_target_executable_path;

extern std::filesystem::path get_base_path(bool default_to_target_executable_path = false);
extern std::filesystem::path get_module_path(HMODULE module);

extern "C" PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName);
extern "C" PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName);

#define HR_CHECK(exp) { const HRESULT res = (exp); assert(SUCCEEDED(res)); }
#define VK_CHECK(exp) { const VkResult res = (exp); assert(res == VK_SUCCESS); }

struct scoped_module_handle
{
	scoped_module_handle(LPCWSTR name) : module(LoadLibraryW(name))
	{
		assert(module != nullptr);
		reshade::hooks::register_module(name);
	}
	~scoped_module_handle()
	{
		FreeLibrary(module);
	}

	const HMODULE module;
};

static LONG APIENTRY HookD3DKMTQueryAdapterInfo(const void *pData)
{
	struct D3DKMT_QUERYADAPTERINFO { UINT hAdapter; UINT Type; VOID *pPrivateDriverData; UINT PrivateDriverDataSize; };

	if (pData != nullptr && static_cast<const D3DKMT_QUERYADAPTERINFO *>(pData)->Type == 1 /* KMTQAITYPE_UMDRIVERNAME */)
	{
		if (reshade::global_config().get("APP", "ForceD3D9On12") && *static_cast<const UINT *>(static_cast<const D3DKMT_QUERYADAPTERINFO *>(pData)->pPrivateDriverData) == 0 /* KMTUMDVERSION_DX9 */)
			return STATUS_INVALID_PARAMETER;
		if (reshade::global_config().get("APP", "ForceD3D11On12") && *static_cast<const UINT *>(static_cast<const D3DKMT_QUERYADAPTERINFO *>(pData)->pPrivateDriverData) == 2 /* KMTUMDVERSION_DX11 */)
			return STATUS_INVALID_PARAMETER;
	}

	return reshade::hooks::call(HookD3DKMTQueryAdapterInfo)(pData);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
	g_module_handle = hInstance;
	g_reshade_dll_path = get_module_path(hInstance);
	g_target_executable_path = g_reshade_dll_path;
	g_reshade_base_path = get_base_path();

	std::error_code ec;
	reshade::log::open_log_file(g_reshade_base_path / L"ReShade.log", ec);

	reshade::hooks::register_module(L"user32.dll");

	reshade::hooks::install("D3DKMTQueryAdapterInfo", GetProcAddress(GetModuleHandleW(L"gdi32.dll"), "D3DKMTQueryAdapterInfo"), HookD3DKMTQueryAdapterInfo);

	static UINT s_resize_w = 0, s_resize_h = 0;

	// Register window class
	WNDCLASS wc = { sizeof(wc) };
	wc.hIcon = LoadIcon(hInstance, TEXT("MAIN_ICON"));
	wc.hInstance = hInstance;
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpszClassName = TEXT("Test");
	wc.lpfnWndProc =
		[](HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
			switch (Msg)
			{
			case WM_DESTROY:
				PostQuitMessage(EXIT_SUCCESS);
				break;
			case WM_SIZE:
				s_resize_w = LOWORD(lParam);
				s_resize_h = HIWORD(lParam);
				break;
			}

			return DefWindowProc(hWnd, Msg, wParam, lParam);
		};

	RegisterClass(&wc);

	LONG window_w = 1024;
	if (LPSTR width_arg = std::strstr(lpCmdLine, "-width "))
		window_w = std::strtol(width_arg + 7, nullptr, 10);
	LONG window_h =  800;
	if (LPSTR height_arg = std::strstr(lpCmdLine, "-height "))
		window_h = std::strtol(height_arg + 8, nullptr, 10);

	const LONG window_x = (GetSystemMetrics(SM_CXSCREEN) - window_w) / 2;
	const LONG window_y = (GetSystemMetrics(SM_CYSCREEN) - window_h) / 2;

	RECT window_rect = { window_x, window_y, window_x + window_w, window_y + window_h };
	AdjustWindowRect(&window_rect, window_x > 0 && window_y > 0 ? WS_OVERLAPPEDWINDOW : WS_POPUP, FALSE);

	// Create and show window instance
	const HWND window_handle = CreateWindow(
		wc.lpszClassName, TEXT("ReShade ") TEXT(VERSION_STRING_PRODUCT), window_x > 0 && window_y > 0 ? WS_OVERLAPPEDWINDOW : WS_POPUP,
		window_rect.left, window_rect.top, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top, nullptr, nullptr, hInstance, nullptr);

	if (window_handle == nullptr)
		return 0;

	ShowWindow(window_handle, nCmdShow);

	// Avoid resize caused by 'ShowWindow' call
	s_resize_w = 0;
	s_resize_h = 0;

	MSG msg = {};

	reshade::api::device_api api = reshade::api::device_api::d3d11;
	if (strstr(lpCmdLine, "-d3d9"))
		api = reshade::api::device_api::d3d9;
	if (strstr(lpCmdLine, "-d3d10"))
		api = reshade::api::device_api::d3d10;
	if (strstr(lpCmdLine, "-d3d11"))
		api = reshade::api::device_api::d3d11;
	if (strstr(lpCmdLine, "-d3d12"))
		api = reshade::api::device_api::d3d12;
	if (strstr(lpCmdLine, "-opengl"))
		api = reshade::api::device_api::opengl;
	if (strstr(lpCmdLine, "-vulkan"))
		api = reshade::api::device_api::vulkan;

	const bool multisample = strstr(lpCmdLine, "-multisample") != nullptr;

	switch (api)
	{
	#pragma region D3D9 Implementation
	case reshade::api::device_api::d3d9:
	{
		const scoped_module_handle d3d9_module(L"d3d9.dll");

		D3DPRESENT_PARAMETERS pp = {};
		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.hDeviceWindow = window_handle;
		pp.Windowed = true;
		pp.MultiSampleType = multisample ? D3DMULTISAMPLE_4_SAMPLES : D3DMULTISAMPLE_NONE;
		pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

		// Initialize Direct3D 9
		com_ptr<IDirect3D9> d3d = Direct3DCreate9(D3D_SDK_VERSION);
		com_ptr<IDirect3DDevice9> device;
		HR_CHECK(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window_handle, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device));

		while (true)
		{
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && msg.message != WM_QUIT)
				DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				break;

			if (s_resize_w != 0)
			{
				pp.BackBufferWidth = s_resize_w;
				pp.BackBufferHeight = s_resize_h;

				HR_CHECK(device->Reset(&pp));

				s_resize_w = s_resize_h = 0;
			}

			HR_CHECK(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF7F7F7F, 0, 0));
			HR_CHECK(device->Present(nullptr, nullptr, nullptr, nullptr));
		}
	}
	#pragma endregion
		break;
	#pragma region D3D10 Implementation
	case reshade::api::device_api::d3d10:
	{
		const scoped_module_handle dxgi_module(L"dxgi.dll");
		const scoped_module_handle d3d11_module(L"d3d10_1.dll");

		// Initialize Direct3D 10
		com_ptr<ID3D10Device> device;
		com_ptr<IDXGISwapChain> swapchain;

		{   DXGI_SWAP_CHAIN_DESC desc = {};
			desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc = { multisample ? 4u : 1u, 0u };
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.BufferCount = 1;
			desc.OutputWindow = window_handle;
			desc.Windowed = true;
			desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

#ifndef NDEBUG
			const UINT flags = D3D11_CREATE_DEVICE_DEBUG;
#else
			const UINT flags = 0;
#endif
			HR_CHECK(D3D10CreateDeviceAndSwapChain(nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, flags, D3D10_SDK_VERSION, &desc, &swapchain, &device));
		}

		com_ptr<ID3D10Texture2D> back_buffer;
		HR_CHECK(swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)));
		com_ptr<ID3D10RenderTargetView> back_buffer_rtv;
		HR_CHECK(device->CreateRenderTargetView(back_buffer.get(), nullptr, &back_buffer_rtv));

		while (true)
		{
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && msg.message != WM_QUIT)
				DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				break;

			if (s_resize_w != 0)
			{
				back_buffer_rtv.reset();
				back_buffer.reset();

				HR_CHECK(swapchain->ResizeBuffers(1, s_resize_w, s_resize_h, DXGI_FORMAT_UNKNOWN, 0));

				HR_CHECK(swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)));
				HR_CHECK(device->CreateRenderTargetView(back_buffer.get(), nullptr, &back_buffer_rtv));

				s_resize_w = s_resize_h = 0;
			}

			const float color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
			device->ClearRenderTargetView(back_buffer_rtv.get(), color);

			HR_CHECK(swapchain->Present(1, 0));
		}
	}
	#pragma endregion
		break;
	#pragma region D3D11 Implementation
	case reshade::api::device_api::d3d11:
	{
		const scoped_module_handle dxgi_module(L"dxgi.dll");
		const scoped_module_handle d3d11_module(L"d3d11.dll");

		// Initialize Direct3D 11
		com_ptr<ID3D11Device> device;
		com_ptr<ID3D11DeviceContext> immediate_context;
		com_ptr<IDXGISwapChain> swapchain;

		{   DXGI_SWAP_CHAIN_DESC desc = {};
			desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc = { multisample ? 4u : 1u, 0u };
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.BufferCount = 1;
			desc.OutputWindow = window_handle;
			desc.Windowed = true;
			desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

#ifndef NDEBUG
			const UINT flags = D3D11_CREATE_DEVICE_DEBUG;
#else
			const UINT flags = 0;
#endif
			HR_CHECK(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &desc, &swapchain, &device, nullptr, &immediate_context));
		}

		com_ptr<ID3D11Texture2D> back_buffer;
		HR_CHECK(swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)));
		com_ptr<ID3D11RenderTargetView> back_buffer_rtv;
		HR_CHECK(device->CreateRenderTargetView(back_buffer.get(), nullptr, &back_buffer_rtv));

		while (true)
		{
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && msg.message != WM_QUIT)
				DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				break;

			if (s_resize_w != 0)
			{
				back_buffer_rtv.reset();
				back_buffer.reset();

				HR_CHECK(swapchain->ResizeBuffers(1, s_resize_w, s_resize_h, DXGI_FORMAT_UNKNOWN, 0));

				HR_CHECK(swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)));
				HR_CHECK(device->CreateRenderTargetView(back_buffer.get(), nullptr, &back_buffer_rtv));

				s_resize_w = s_resize_h = 0;
			}

			const float color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
			immediate_context->ClearRenderTargetView(back_buffer_rtv.get(), color);

			HR_CHECK(swapchain->Present(1, 0));
		}
	}
	#pragma endregion
		break;
	#pragma region D3D12 Implementation
	case reshade::api::device_api::d3d12:
	{
		const scoped_module_handle dxgi_module(L"dxgi.dll");
		const scoped_module_handle d3d12_module(L"d3d12.dll");

#ifndef NDEBUG
		// Enable D3D debug layer if it is available
		{   com_ptr<ID3D12Debug> debug_iface;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_iface))))
				debug_iface->EnableDebugLayer();
		}
#endif

		// Initialize Direct3D 12
		com_ptr<ID3D12Device> device;
		com_ptr<IDXGIFactory2> dxgi_factory;
		com_ptr<ID3D12CommandQueue> command_queue;
		com_ptr<IDXGISwapChain3> swapchain;
		com_ptr<ID3D12Resource> back_buffers[3];
		com_ptr<ID3D12DescriptorHeap> rtv_heap;
		com_ptr<ID3D12CommandAllocator> cmd_alloc;
		com_ptr<ID3D12GraphicsCommandList> cmd_lists[3];

		HR_CHECK(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgi_factory)));

		{	HRESULT hr = E_FAIL; com_ptr<IDXGIAdapter> adapter;
			for (UINT i = 0; dxgi_factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++, adapter.reset())
				if (SUCCEEDED(hr = D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
					break;
			HR_CHECK(hr);
		}

		// Check if this device was created using d3d12on7 on Windows 7
		// See https://microsoft.github.io/DirectX-Specs/d3d/D3D12onWin7.html for more information
		com_ptr<ID3D12DeviceDownlevel> downlevel;
		const bool is_d3d12on7 = SUCCEEDED(device->QueryInterface(&downlevel));
		downlevel.reset();

		{   D3D12_COMMAND_QUEUE_DESC desc = { D3D12_COMMAND_LIST_TYPE_DIRECT };
			HR_CHECK(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)));
		}

		const UINT num_buffers = is_d3d12on7 ? 1 : ARRAYSIZE(back_buffers);

		if (!is_d3d12on7)
		{
			// DXGI has not been updated for d3d12on7, so cannot use swap chain created from D3D12 device there
			DXGI_SWAP_CHAIN_DESC1 desc = {};
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc = { 1, 0 };
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.BufferCount = num_buffers;
			desc.Scaling = DXGI_SCALING_STRETCH;
			desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;


			com_ptr<IDXGISwapChain1> dxgi_swapchain;

			HR_CHECK(dxgi_factory->CreateSwapChainForHwnd(command_queue.get(), window_handle, &desc, nullptr, nullptr, &dxgi_swapchain));
			HR_CHECK(dxgi_swapchain->QueryInterface(&swapchain));
		}

		{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
			desc.NumDescriptors = num_buffers;
			HR_CHECK(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtv_heap)));
		}

		const UINT rtv_handle_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_alloc)));

	resize_buffers:
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();

		for (UINT i = 0; i < num_buffers; ++i, rtv_handle.ptr += rtv_handle_size)
		{
			if (!is_d3d12on7)
			{
				HR_CHECK(swapchain->GetBuffer(i, IID_PPV_ARGS(&back_buffers[i])));

				const std::wstring debug_name = L"Back buffer " + std::to_wstring(i);
				back_buffers[i]->SetName(debug_name.c_str());
			}
			else
			{
				GetClientRect(window_handle, &window_rect);

				D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_TEXTURE2D };
				desc.Width = window_rect.right;
				desc.Height = window_rect.bottom;
				desc.DepthOrArraySize = desc.MipLevels = 1;
				desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				desc.SampleDesc = { 1, 0 };
				desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

				D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_DEFAULT };
				// Create a fake back buffer resource for d3d12on7
				HR_CHECK(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&back_buffers[i])));
			}

			device->CreateRenderTargetView(back_buffers[i].get(), nullptr, rtv_handle);

			HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_alloc.get(), nullptr, IID_PPV_ARGS(&cmd_lists[i])));

			D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
			barrier.Transition.pResource = back_buffers[i].get();
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			cmd_lists[i]->ResourceBarrier(1, &barrier);

			const float color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
			cmd_lists[i]->ClearRenderTargetView(rtv_handle, color, 0, nullptr);

			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			cmd_lists[i]->ResourceBarrier(1, &barrier);

			HR_CHECK(cmd_lists[i]->Close());
		}

		while (true)
		{
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && msg.message != WM_QUIT)
				DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				break;

			if (s_resize_w != 0)
			{
				// Clean up current resources referencing the back buffer
				for (auto &ptr : cmd_lists)
					ptr.reset();
				for (auto &ptr : back_buffers)
					ptr.reset();

				if (!is_d3d12on7)
					HR_CHECK(swapchain->ResizeBuffers(num_buffers, s_resize_w, s_resize_h, DXGI_FORMAT_UNKNOWN, 0));

				s_resize_w = s_resize_h = 0;

				goto resize_buffers; // Re-create command lists
			}

			const UINT swap_index = is_d3d12on7 ? 0 : swapchain->GetCurrentBackBufferIndex();

			ID3D12CommandList *const cmd_list = cmd_lists[swap_index].get();
			command_queue->ExecuteCommandLists(1, &cmd_list);

			if (is_d3d12on7)
			{
				// Create a dummy list to pass into present
				com_ptr<ID3D12GraphicsCommandList> dummy_list;
				HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_alloc.get(), nullptr, IID_PPV_ARGS(&dummy_list)));

				com_ptr<ID3D12CommandQueueDownlevel> queue_downlevel;
				HR_CHECK(command_queue->QueryInterface(&queue_downlevel));

				// Windows 7 present path does not have a DXGI swap chain
				HR_CHECK(queue_downlevel->Present(dummy_list.get(), back_buffers[swap_index].get(), window_handle, D3D12_DOWNLEVEL_PRESENT_FLAG_WAIT_FOR_VBLANK));
			}
			else
			{
				// Synchronization is handled in 'swapchain_impl::on_present'
				HR_CHECK(swapchain->Present(1, 0));
			}
		}
	}
	#pragma endregion
		break;
	#pragma region OpenGL Implementation
	case reshade::api::device_api::opengl:
	{
		const scoped_module_handle opengl_module(L"opengl32.dll");

		// Initialize OpenGL
		const HWND temp_window_handle = CreateWindow(TEXT("STATIC"), nullptr, WS_POPUP, 0, 0, 0, 0, window_handle, nullptr, hInstance, nullptr);
		if (temp_window_handle == nullptr)
			return 1;

		const HDC hdc1 = GetDC(temp_window_handle);
		const HDC hdc2 = GetDC(window_handle);

		PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1 };
		pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 24;
		pfd.cAlphaBits = 8;

		int pix_format = ChoosePixelFormat(hdc1, &pfd);
		SetPixelFormat(hdc1, pix_format, &pfd);

		const HGLRC hglrc1 = wglCreateContext(hdc1);
		if (hglrc1 == nullptr)
			return 1;

		wglMakeCurrent(hdc1, hglrc1);

		if (!gladLoadWGL(hdc1,
				[](const char *name) -> GLADapiproc {
					return reinterpret_cast<GLADapiproc>(wglGetProcAddress(name));
				}))
			return 1;

		const int pix_attribs[] = {
			WGL_DOUBLE_BUFFER_ARB, 1,
			WGL_DRAW_TO_WINDOW_ARB, 1,
			WGL_SUPPORT_OPENGL_ARB, 1,
			WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
			WGL_COLOR_BITS_ARB, pfd.cColorBits,
			WGL_ALPHA_BITS_ARB, pfd.cAlphaBits,
			WGL_SAMPLE_BUFFERS_ARB, multisample ? GL_TRUE : GL_FALSE,
			WGL_SAMPLES_ARB, multisample ? 4 : 1,
			0 // Terminate list
		};

		UINT num_formats = 0;
		if (!wglChoosePixelFormatARB(hdc2, pix_attribs, nullptr, 1, &pix_format, &num_formats))
			return 1;

		SetPixelFormat(hdc2, pix_format, &pfd);

		// Create an OpenGL 4.3 context
		const int attribs[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
			WGL_CONTEXT_MINOR_VERSION_ARB, 3,
			0 // Terminate list
		};

		const HGLRC hglrc2 = wglCreateContextAttribsARB(hdc2, nullptr, attribs);
		if (hglrc2 == nullptr)
			return 1;

		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(hglrc1);
		DestroyWindow(temp_window_handle);

		wglMakeCurrent(hdc2, hglrc2);

		GladGLContext gl = {};
		if (!gladLoadGLContextUserPtr(&gl,
				[](void *user, const char *name) -> GLADapiproc {
					FARPROC proc_address = wglGetProcAddress(name);
					if (nullptr == proc_address)
						proc_address = GetProcAddress(static_cast<HMODULE>(user), name);
					return reinterpret_cast<GLADapiproc>(proc_address);
				}, opengl_module.module))
			return 1;

		while (true)
		{
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && msg.message != WM_QUIT)
				DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				break;

			if (s_resize_w != 0)
			{
				gl.Viewport(0, 0, s_resize_w, s_resize_h);

				s_resize_w = s_resize_h = 0;
			}

			gl.ClearColor(0.5f, 0.5f, 0.5f, 1.0f);
			gl.Clear(GL_COLOR_BUFFER_BIT);

#if 1
			wglSwapLayerBuffers(hdc2, WGL_SWAP_MAIN_PLANE); // Call directly for RenderDoc compatibility
#else
			SwapBuffers(hdc2);
#endif
		}

		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(hglrc2);
	}
	#pragma endregion
		break;
	#pragma region Vulkan Implementation
	case reshade::api::device_api::vulkan:
	{
		const scoped_module_handle vulkan_module(L"vulkan-1.dll");

		struct vulkan_instance_and_device_type
		{
			VkDevice device_handle = VK_NULL_HANDLE;
			VkInstance instance_handle = VK_NULL_HANDLE;
		} vulkan_instance_and_device;

		VkDevice &device = vulkan_instance_and_device.device_handle;
		VkInstance &instance = vulkan_instance_and_device.instance_handle;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkPhysicalDevice physical_device = VK_NULL_HANDLE;

		GladVulkanContext vk = {};
		vk.CreateInstance = reinterpret_cast<PFN_vkCreateInstance>(vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
		if (vk.CreateInstance == nullptr)
			return 1;

		{	VkApplicationInfo app_info { VK_STRUCTURE_TYPE_APPLICATION_INFO };
			app_info.apiVersion = VK_API_VERSION_1_0;
			app_info.pApplicationName = "ReShade Test Application";
			app_info.applicationVersion = VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION;

			const char *const enabled_layers[] = {
#if VK_HEADER_VERSION >= 106
				"VK_LAYER_KHRONOS_validation"
#else
				"VK_LAYER_LUNARG_standard_validation"
#endif
			};
			const char *const enabled_extensions[] = {
				VK_KHR_SURFACE_EXTENSION_NAME,
				VK_KHR_WIN32_SURFACE_EXTENSION_NAME
			};

			VkInstanceCreateInfo create_info { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
			create_info.pApplicationInfo = &app_info;
#ifndef NDEBUG
			create_info.enabledLayerCount = ARRAYSIZE(enabled_layers);
			create_info.ppEnabledLayerNames = enabled_layers;
#endif
			create_info.enabledExtensionCount = ARRAYSIZE(enabled_extensions);
			create_info.ppEnabledExtensionNames = enabled_extensions;

			VK_CHECK(vk.CreateInstance(&create_info, nullptr, &instance));
		}

		gladLoadVulkanContextUserPtr(&vk, VK_NULL_HANDLE,
			[](void *user, const char *name) -> GLADapiproc {
				const auto &vulkan_instance_and_device = *static_cast<const vulkan_instance_and_device_type *>(user);
				return reinterpret_cast<GLADapiproc>(vkGetInstanceProcAddr(vulkan_instance_and_device.instance_handle, name));
			}, &vulkan_instance_and_device);

		// Pick the first physical device.
		uint32_t num_physical_devices = 1;
		VK_CHECK(vk.EnumeratePhysicalDevices(instance, &num_physical_devices, &physical_device));

		// Pick the first queue with graphics support.
		uint32_t queue_family_index = 0, num_queue_families = 0;
		vk.GetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, nullptr);
		std::vector<VkQueueFamilyProperties> queue_families(num_queue_families);
		vk.GetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, queue_families.data());
		for (uint32_t index = 0; index < num_queue_families && (queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0; ++index)
			queue_family_index = index;

		{	VkDeviceQueueCreateInfo queue_info { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
			queue_info.queueCount = 1;
			queue_info.queueFamilyIndex = queue_family_index;
			float queue_priority = 1.0f;
			queue_info.pQueuePriorities = &queue_priority;

			const char *const enabled_layers[] = {
#if VK_HEADER_VERSION >= 106
				"VK_LAYER_KHRONOS_validation"
#else
				"VK_LAYER_LUNARG_standard_validation"
#endif
			};
			const char *const enabled_extensions[] = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME
			};

			VkDeviceCreateInfo create_info { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
			create_info.queueCreateInfoCount = 1;
			create_info.pQueueCreateInfos = &queue_info;
#ifndef NDEBUG
			create_info.enabledLayerCount = ARRAYSIZE(enabled_layers);
			create_info.ppEnabledLayerNames = enabled_layers;
#endif
			create_info.enabledExtensionCount = ARRAYSIZE(enabled_extensions);
			create_info.ppEnabledExtensionNames = enabled_extensions;

			VK_CHECK(vk.CreateDevice(physical_device, &create_info, nullptr, &device));
		}

		gladLoadVulkanContextUserPtr(&vk, physical_device,
			[](void *user, const char *name) -> GLADapiproc {
				const auto &vulkan_instance_and_device = *static_cast<const vulkan_instance_and_device_type *>(user);

				// Need to distinguish between instance and device functions
				if (0 == std::strcmp(name, "vkGetDeviceProcAddr") ||
					0 == std::strcmp(name, "vkGetInstanceProcAddr") ||
					0 == std::strcmp(name, "vkCreateDevice") ||
					0 == std::strcmp(name, "vkCreateInstance") ||
					0 == std::strcmp(name, "vkDestroyInstance") ||
					0 == std::strcmp(name, "vkSubmitDebugUtilsMessageEXT") ||
					0 == std::strcmp(name, "vkCreateDebugUtilsMessengerEXT") ||
					0 == std::strcmp(name, "vkDestroyDebugUtilsMessengerEXT") ||
					(std::strstr(name, "Properties") != nullptr && std::strstr(name, "AccelerationStructures") == nullptr && std::strstr(name, "Handle") == nullptr) ||
					(std::strstr(name, "Surface") != nullptr && std::strstr(name, "DeviceGroupSurface") == nullptr) ||
					(std::strstr(name, "PhysicalDevice") != nullptr))
					return reinterpret_cast<GLADapiproc>(vkGetInstanceProcAddr(vulkan_instance_and_device.instance_handle, name));

				const PFN_vkVoidFunction device_proc_address = vkGetDeviceProcAddr(vulkan_instance_and_device.device_handle, name);
				return reinterpret_cast<GLADapiproc>(device_proc_address);
			}, &vulkan_instance_and_device);

		{	VkWin32SurfaceCreateInfoKHR create_info { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
			create_info.hinstance = hInstance;
			create_info.hwnd = window_handle;

			VK_CHECK(vk.CreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface));

			// Check presentation support to make validation layers happy
			VkBool32 supported = VK_FALSE;
			VK_CHECK(vk.GetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family_index, surface, &supported));
			assert(VK_FALSE != supported);
		}

		VkQueue queue = VK_NULL_HANDLE;
		vk.GetDeviceQueue(device, queue_family_index, 0, &queue);

		VkCommandPool cmd_alloc = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> cmd_buffers;

		{	VkCommandPoolCreateInfo create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			create_info.queueFamilyIndex = queue_family_index;

			VK_CHECK(vk.CreateCommandPool(device, &create_info, nullptr, &cmd_alloc));
		}

		const auto resize_swapchain = [&](VkSwapchainKHR old_swapchain = VK_NULL_HANDLE) {
			uint32_t num_present_modes = 0, num_surface_formats = 0;
			vk.GetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_surface_formats, nullptr);
			vk.GetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &num_present_modes, nullptr);
			std::vector<VkPresentModeKHR> present_modes(num_present_modes);
			std::vector<VkSurfaceFormatKHR> surface_formats(num_surface_formats);
			vk.GetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_surface_formats, surface_formats.data());
			vk.GetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &num_present_modes, present_modes.data());
			VkSurfaceCapabilitiesKHR capabilities = {};
			vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);

			VkSwapchainCreateInfoKHR create_info { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
			create_info.surface = surface;
			create_info.minImageCount = 3;
			create_info.imageFormat = surface_formats[0].format;
			create_info.imageColorSpace = surface_formats[0].colorSpace;
			create_info.imageExtent = capabilities.currentExtent;
			create_info.imageArrayLayers = 1;
			create_info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
			create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			create_info.presentMode = present_modes[0];
			create_info.clipped = VK_TRUE;
			create_info.oldSwapchain = old_swapchain;

			VK_CHECK(vk.CreateSwapchainKHR(device, &create_info, nullptr, &swapchain));

			if (old_swapchain != VK_NULL_HANDLE)
				vk.DestroySwapchainKHR(device, old_swapchain, nullptr);

			uint32_t num_swapchain_images = 0;
			VK_CHECK(vk.GetSwapchainImagesKHR(device, swapchain, &num_swapchain_images, nullptr));
			std::vector<VkImage> swapchain_images(num_swapchain_images);
			VK_CHECK(vk.GetSwapchainImagesKHR(device, swapchain, &num_swapchain_images, swapchain_images.data()));

			if (!cmd_buffers.empty())
				vk.FreeCommandBuffers(device, cmd_alloc, static_cast<uint32_t>(cmd_buffers.size()), cmd_buffers.data());
			cmd_buffers.resize(num_swapchain_images);

			{	VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
				alloc_info.commandPool = cmd_alloc;
				alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				alloc_info.commandBufferCount = static_cast<uint32_t>(cmd_buffers.size());

				VK_CHECK(vk.AllocateCommandBuffers(device, &alloc_info, cmd_buffers.data()));
			}

			for (uint32_t i = 0; i < num_swapchain_images; ++i)
			{
				const VkCommandBuffer cmd_buffer = cmd_buffers[i];

				VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
				begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				VK_CHECK(vk.BeginCommandBuffer(cmd_buffer, &begin_info));

				const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				transition.srcAccessMask = 0;
				transition.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				transition.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				transition.image = swapchain_images[i];
				transition.subresourceRange = range;
				vk.CmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

				const VkClearColorValue color = { 0.5f, 0.5f, 0.5f, 1.0f };
				vk.CmdClearColorImage(cmd_buffer, swapchain_images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

				transition.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				transition.dstAccessMask = 0;
				transition.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				transition.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				vk.CmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

				VK_CHECK(vk.EndCommandBuffer(cmd_buffer));
			}
		};

		resize_swapchain();

		uint32_t sem_index = 0;
		std::vector<VkSemaphore> cmd_semaphores(4);
		std::vector<VkSemaphore> acquire_semaphores(4);

		for (size_t i = 0; i < cmd_semaphores.size(); ++i)
		{
			VkSemaphoreCreateInfo create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VK_CHECK(vk.CreateSemaphore(device, &create_info, nullptr, &cmd_semaphores[i]));
			VK_CHECK(vk.CreateSemaphore(device, &create_info, nullptr, &acquire_semaphores[i]));
		}

		while (true)
		{
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && msg.message != WM_QUIT)
				DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				break;

			if (s_resize_w != 0)
			{
				resize_swapchain(swapchain);

				s_resize_w = s_resize_h = 0;
			}

			uint32_t swapchain_image_index = 0;
			sem_index = (sem_index + 1) % cmd_semaphores.size();

			VkResult present_res = vk.AcquireNextImageKHR(device, swapchain, UINT64_MAX, acquire_semaphores[sem_index], VK_NULL_HANDLE, &swapchain_image_index);
			if (present_res == VK_SUCCESS)
			{
				VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
				submit_info.waitSemaphoreCount = 1;
				submit_info.pWaitSemaphores = &acquire_semaphores[sem_index];
				const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				submit_info.pWaitDstStageMask = &wait_stage;
				submit_info.commandBufferCount = 1;
				submit_info.pCommandBuffers = &cmd_buffers[swapchain_image_index];
				submit_info.signalSemaphoreCount = 1;
				submit_info.pSignalSemaphores = &cmd_semaphores[sem_index];

				VK_CHECK(vk.QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));

				VkPresentInfoKHR present_info { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
				present_info.waitSemaphoreCount = 1;
				present_info.pWaitSemaphores = &cmd_semaphores[sem_index];
				present_info.swapchainCount = 1;
				present_info.pSwapchains = &swapchain;
				present_info.pImageIndices = &swapchain_image_index;

				present_res = vk.QueuePresentKHR(queue, &present_info);
			}

			// Ignore out of date errors during presentation, since swap chain will be recreated on next minimize/maximize event anyway
			if (present_res != VK_SUBOPTIMAL_KHR && present_res != VK_ERROR_OUT_OF_DATE_KHR)
				VK_CHECK(present_res);
		}

		// Wait for all GPU work to finish before destroying objects
		vk.DeviceWaitIdle(device);

		for (size_t i = 0; i < cmd_semaphores.size(); ++i)
		{
			vk.DestroySemaphore(device, cmd_semaphores[i], nullptr);
			vk.DestroySemaphore(device, acquire_semaphores[i], nullptr);
		}
		vk.FreeCommandBuffers(device, cmd_alloc, static_cast<uint32_t>(cmd_buffers.size()), cmd_buffers.data());
		vk.DestroyCommandPool(device, cmd_alloc, nullptr);
		vk.DestroySwapchainKHR(device, swapchain, nullptr);
		vk.DestroySurfaceKHR(instance, surface, nullptr);
		vk.DestroyDevice(device, nullptr);
		vk.DestroyInstance(instance, nullptr);
	}
	#pragma endregion
		break;
	default:
		msg.wParam = EXIT_FAILURE;
		break;
	}

	reshade::hooks::uninstall();

	return static_cast<int>(msg.wParam);
}

#endif
