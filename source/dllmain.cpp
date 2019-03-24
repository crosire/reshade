/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "version.h"
#include <Windows.h>

HMODULE g_module_handle = nullptr;

std::filesystem::path g_reshade_dll_path;
std::filesystem::path g_target_executable_path;

extern std::filesystem::path get_system_path()
{
	static std::filesystem::path system_path;
	if (!system_path.empty())
		return system_path;
	TCHAR buf[MAX_PATH] = {};
	GetSystemDirectory(buf, ARRAYSIZE(buf));
	return system_path = buf;
}
static inline std::filesystem::path get_module_path(HMODULE module)
{
	TCHAR buf[MAX_PATH] = {};
	GetModuleFileName(module, buf, ARRAYSIZE(buf));
	return buf;
}

#if defined(RESHADE_TEST_APPLICATION)

#include "d3d9/runtime_d3d9.hpp"
#include "d3d11/runtime_d3d11.hpp"
#include "d3d12/runtime_d3d12.hpp"
#include "opengl/runtime_opengl.hpp"

#define HCHECK(exp) assert(SUCCEEDED(exp))

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
	using namespace reshade;

	g_module_handle = hInstance;
	g_reshade_dll_path = get_module_path(nullptr);
	g_target_executable_path = get_module_path(nullptr);

	log::open(std::filesystem::path(g_reshade_dll_path).replace_extension(".log"));

	hooks::register_module("user32.dll");

	static UINT s_resize_w = 0, s_resize_h = 0;

	// Register window class
	WNDCLASS wc = { sizeof(wc) };
	wc.hInstance = hInstance;
	wc.style = CS_HREDRAW | CS_VREDRAW;
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

	// Create and show window instance
	const HWND window_handle = CreateWindow(
		wc.lpszClassName, TEXT("ReShade ") TEXT(VERSION_STRING_FILE) TEXT(" by crosire"), WS_OVERLAPPEDWINDOW,
		0, 0, 1024, 800, nullptr, nullptr, hInstance, nullptr);

	if (window_handle == nullptr)
		return 0;

	ShowWindow(window_handle, nCmdShow);

	// Avoid resize caused by 'ShowWindow' call
	s_resize_w = 0;
	s_resize_h = 0;

	MSG msg = {};

	#pragma region D3D9 Implementation
	if (strstr(lpCmdLine, "-d3d9"))
	{
		hooks::register_module("d3d9.dll");
		const auto d3d9_module = LoadLibrary(TEXT("d3d9.dll"));

		D3DPRESENT_PARAMETERS pp = {};
		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.hDeviceWindow = window_handle;
		pp.Windowed = true;
		pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

		// Initialize Direct3D 9
		com_ptr<IDirect3D9> d3d(Direct3DCreate9(D3D_SDK_VERSION), true);
		com_ptr<IDirect3DDevice9> device;

		HCHECK(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window_handle, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device));

		while (msg.message != WM_QUIT)
		{
			while (msg.message != WM_QUIT &&
				PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				DispatchMessage(&msg);

			if (s_resize_w != 0)
			{
				pp.BackBufferWidth = s_resize_w;
				pp.BackBufferHeight = s_resize_h;

				HCHECK(device->Reset(&pp));

				s_resize_w = s_resize_h = 0;
			}

			HCHECK(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF7F7F7F, 0, 0));
			HCHECK(device->Present(nullptr, nullptr, nullptr, nullptr));
		}

		reshade::hooks::uninstall();

		FreeLibrary(d3d9_module);

		return static_cast<int>(msg.wParam);
	}
	#pragma endregion

	#pragma region D3D11 Implementation
	if (strstr(lpCmdLine, "-d3d11"))
	{
		hooks::register_module("dxgi.dll");
		hooks::register_module("d3d11.dll");
		const auto d3d11_module = LoadLibrary(TEXT("d3d11.dll"));

		// Initialize Direct3D 11
		com_ptr<ID3D11Device> device;
		com_ptr<ID3D11DeviceContext> immediate_context;
		com_ptr<IDXGISwapChain> swapchain;

		{   DXGI_SWAP_CHAIN_DESC desc = {};
			desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc = { 1, 0 };
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.BufferCount = 1;
			desc.OutputWindow = window_handle;
			desc.Windowed = true;
			desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

			HCHECK(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG, nullptr, 0, D3D11_SDK_VERSION, &desc, &swapchain, &device, nullptr, &immediate_context));
		}

		com_ptr<ID3D11Texture2D> backbuffer;
		HCHECK(swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)));
		com_ptr<ID3D11RenderTargetView> target;
		HCHECK(device->CreateRenderTargetView(backbuffer.get(), nullptr, &target));

		while (msg.message != WM_QUIT)
		{
			while (msg.message != WM_QUIT &&
				PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				DispatchMessage(&msg);

			if (s_resize_w != 0)
			{
				target.reset();
				backbuffer.reset();

				HCHECK(swapchain->ResizeBuffers(1, s_resize_w, s_resize_h, DXGI_FORMAT_UNKNOWN, 0));

				HCHECK(swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)));
				HCHECK(device->CreateRenderTargetView(backbuffer.get(), nullptr, &target));

				s_resize_w = s_resize_h = 0;
			}

			const float color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
			immediate_context->ClearRenderTargetView(target.get(), color);

			HCHECK(swapchain->Present(1, 0));
		}

		reshade::hooks::uninstall();

		FreeLibrary(d3d11_module);

		return static_cast<int>(msg.wParam);
	}
	#pragma endregion

	#pragma region D3D12 Implementation
	if (strstr(lpCmdLine, "-d3d12"))
	{
		hooks::register_module("dxgi.dll");
		hooks::register_module("d3d12.dll");
		const auto d3d12_module = LoadLibrary(TEXT("d3d12.dll"));

		// Enable D3D debug layer
		{   com_ptr<ID3D12Debug> debug_iface;
			HCHECK(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_iface)));
			debug_iface->EnableDebugLayer();
		}

		// Initialize Direct3D 12
		com_ptr<ID3D12Device> device;
		com_ptr<ID3D12CommandQueue> command_queue;
		com_ptr<IDXGISwapChain3> swapchain;
		com_ptr<ID3D12Resource> backbuffers[3];
		com_ptr<ID3D12DescriptorHeap> rtv_heap;
		com_ptr<ID3D12CommandAllocator> cmd_alloc;
		com_ptr<ID3D12GraphicsCommandList> cmd_lists[3];

		HCHECK(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));

		{   D3D12_COMMAND_QUEUE_DESC desc = { D3D12_COMMAND_LIST_TYPE_DIRECT };
			HCHECK(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)));
		}

		{   DXGI_SWAP_CHAIN_DESC1 desc = {};
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc = { 1, 0 };
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.BufferCount = ARRAYSIZE(backbuffers);
			desc.Scaling = DXGI_SCALING_STRETCH;
			desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

			com_ptr<IDXGIFactory2> dxgi_factory;
			com_ptr<IDXGISwapChain1> dxgi_swapchain;
			HCHECK(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgi_factory)));
			HCHECK(dxgi_factory->CreateSwapChainForHwnd(command_queue.get(), window_handle, &desc, nullptr, nullptr, &dxgi_swapchain));
			HCHECK(dxgi_swapchain->QueryInterface(&swapchain));
		}

		{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
			desc.NumDescriptors = ARRAYSIZE(backbuffers);
			HCHECK(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtv_heap)));
		}

		const UINT rtv_handle_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();

		HCHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_alloc)));

		for (UINT i = 0; i < ARRAYSIZE(backbuffers); ++i, rtv_handle.ptr += rtv_handle_size)
		{
			HCHECK(swapchain->GetBuffer(i, IID_PPV_ARGS(&backbuffers[i])));

			device->CreateRenderTargetView(backbuffers[i].get(), nullptr, rtv_handle);

			HCHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_alloc.get(), nullptr, IID_PPV_ARGS(&cmd_lists[i])));

			D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
			barrier.Transition.pResource = backbuffers[i].get();
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			cmd_lists[i]->ResourceBarrier(1, &barrier);

			const float color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
			cmd_lists[i]->ClearRenderTargetView(rtv_handle, color, 0, nullptr);

			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			cmd_lists[i]->ResourceBarrier(1, &barrier);

			HCHECK(cmd_lists[i]->Close());
		}

		UINT64 next_fence_value = 0;
		const HANDLE frame_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		assert(frame_event != nullptr);
		com_ptr<ID3D12Fence> frame_fence;
		HCHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frame_fence)));

		while (msg.message != WM_QUIT)
		{
			while (msg.message != WM_QUIT &&
				PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				DispatchMessage(&msg);

			const UINT swap_index = swapchain->GetCurrentBackBufferIndex();

			ID3D12CommandList *const cmd_list = cmd_lists[swap_index].get();
			command_queue->ExecuteCommandLists(1, &cmd_list);

			swapchain->Present(1, 0);

			// Wait for frame to complete
			const UINT64 fence_value = ++next_fence_value;
			command_queue->Signal(frame_fence.get(), fence_value);
			if (frame_fence->GetCompletedValue() < fence_value)
			{
				frame_fence->SetEventOnCompletion(fence_value, frame_event);
				WaitForSingleObject(frame_event, INFINITE);
				Sleep(1); // TODO: Mmmmmh
			}
		}

		CloseHandle(frame_event);

		reshade::hooks::uninstall();

		FreeLibrary(d3d12_module);

		return static_cast<int>(msg.wParam);
	}
	#pragma endregion

	#pragma region OpenGL Implementation
	if (strstr(lpCmdLine, "-opengl"))
	{
		hooks::register_module("opengl32.dll");
		const auto opengl_module = LoadLibrary(TEXT("opengl32.dll"));

		// Initialize OpenGL
		const HDC hdc = GetDC(window_handle);

		PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;

		const int pf = ChoosePixelFormat(hdc, &pfd);
		SetPixelFormat(hdc, pf, &pfd);

		const HGLRC hglrc = wglCreateContext(hdc);
		if (hglrc == nullptr)
			return 0;

		wglMakeCurrent(hdc, hglrc);

		while (msg.message != WM_QUIT)
		{
			while (msg.message != WM_QUIT &&
				PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				DispatchMessage(&msg);

			if (s_resize_w != 0)
			{
				glViewport(0, 0, s_resize_w, s_resize_h);

				s_resize_w = s_resize_h = 0;
			}

			glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			SwapBuffers(hdc);
		}

		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(hglrc);

		reshade::hooks::uninstall();

		FreeLibrary(opengl_module);

		return static_cast<int>(msg.wParam);
	}
	#pragma endregion

	return EXIT_FAILURE;
}

#else

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	using namespace reshade;

	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		g_module_handle = hModule;
		g_reshade_dll_path = get_module_path(hModule);
		g_target_executable_path = get_module_path(nullptr);

		log::open(std::filesystem::path(g_reshade_dll_path).replace_extension(".log"));

#ifdef WIN64
		LOG(INFO) << "Initializing crosire's ReShade version '" VERSION_STRING_FILE "' (64-bit) built on '" VERSION_DATE " " VERSION_TIME "' loaded from " << g_reshade_dll_path << " to " << g_target_executable_path << " ...";
#else
		LOG(INFO) << "Initializing crosire's ReShade version '" VERSION_STRING_FILE "' (32-bit) built on '" VERSION_DATE " " VERSION_TIME "' loaded from " << g_reshade_dll_path << " to " << g_target_executable_path << " ...";
#endif

		hooks::register_module("user32.dll");
		hooks::register_module("ws2_32.dll");

		hooks::register_module(get_system_path() / "d3d9.dll");
		hooks::register_module(get_system_path() / "d3d10.dll");
		hooks::register_module(get_system_path() / "d3d10_1.dll");
		hooks::register_module(get_system_path() / "d3d11.dll");
		hooks::register_module(get_system_path() / "d3d12.dll");
		hooks::register_module(get_system_path() / "dxgi.dll");
		hooks::register_module(get_system_path() / "opengl32.dll");

		LOG(INFO) << "Initialized.";
		break;
	case DLL_PROCESS_DETACH:
		LOG(INFO) << "Exiting ...";

		hooks::uninstall();

		LOG(INFO) << "Exited.";
		break;
	}

	return TRUE;
}

#endif
