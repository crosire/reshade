/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "version.h"
#include "dll_log.hpp"
#include "dll_config.hpp"
#include "hook_manager.hpp"
#include <Psapi.h>
#include <Windows.h>

HMODULE g_module_handle = nullptr;
std::filesystem::path g_reshade_dll_path;
std::filesystem::path g_reshade_base_path;
std::filesystem::path g_target_executable_path;

/// <summary>
/// Checks whether the current operation system is Windows 7 or earlier.
/// </summary>
bool is_windows7()
{
	ULONGLONG condition = 0;
	VER_SET_CONDITION(condition, VER_MAJORVERSION, VER_LESS_EQUAL);
	VER_SET_CONDITION(condition, VER_MINORVERSION, VER_LESS_EQUAL);

	OSVERSIONINFOEX verinfo_windows7 = { sizeof(OSVERSIONINFOEX), 6, 1 };
	return VerifyVersionInfo(&verinfo_windows7, VER_MAJORVERSION | VER_MINORVERSION, condition) != FALSE;
}

/// <summary>
/// Expands any environment variables in the path (like "%userprofile%") and checks whether it points towards an existing directory.
/// </summary>
static bool resolve_env_path(std::filesystem::path &path, const std::filesystem::path &base = g_reshade_dll_path.parent_path())
{
	WCHAR buf[4096];
	if (!ExpandEnvironmentStringsW(path.c_str(), buf, ARRAYSIZE(buf)))
		return false;
	path = buf;

	if (path.is_relative())
		path = base / path;

	if (!GetLongPathNameW(path.c_str(), buf, ARRAYSIZE(buf)))
		return false;
	path = buf;

	path = path.lexically_normal();
	if (!path.has_stem()) // Remove trailing slash
		path = path.parent_path();

	std::error_code ec;
	return std::filesystem::is_directory(path, ec);
}

/// <summary>
/// Returns the path that should be used as base for relative paths.
/// </summary>
std::filesystem::path get_base_path()
{
	std::filesystem::path result;

	if (reshade::global_config().get("INSTALL", "BasePath", result) && resolve_env_path(result))
		return result;

	WCHAR buf[4096] = L"";
	if (GetEnvironmentVariableW(L"RESHADE_BASE_PATH_OVERRIDE", buf, ARRAYSIZE(buf)) && resolve_env_path(result = buf))
		return result;

	std::error_code ec;
	if (std::filesystem::exists(reshade::global_config().path(), ec) || !std::filesystem::exists(g_target_executable_path.parent_path() / L"ReShade.ini", ec))
	{
		return g_reshade_dll_path.parent_path();
	}
	else
	{
		// Use target executable directory when a unique configuration already exists
		return g_target_executable_path.parent_path();
	}
}

/// <summary>
/// Returns the path to the "System32" directory or the module path from global configuration if it exists.
/// </summary>
std::filesystem::path get_system_path()
{
	static std::filesystem::path result;
	if (!result.empty())
		return result; // Return the cached path if it exists

	if (reshade::global_config().get("INSTALL", "ModulePath", result) && resolve_env_path(result))
		return result;

	WCHAR buf[4096] = L"";
	if (GetEnvironmentVariableW(L"RESHADE_MODULE_PATH_OVERRIDE", buf, ARRAYSIZE(buf)) && resolve_env_path(result = buf))
		return result;

	// First try environment variable, use system directory if it does not exist or is empty
	GetSystemDirectoryW(buf, ARRAYSIZE(buf));
	return result = buf;
}

/// <summary>
/// Returns the path to the module file identified by the specified <paramref name="module"/> handle.
/// </summary>
static inline std::filesystem::path get_module_path(HMODULE module)
{
	WCHAR buf[4096];
	return GetModuleFileNameW(module, buf, ARRAYSIZE(buf)) ? buf : std::filesystem::path();
}

#ifdef RESHADE_TEST_APPLICATION

#  include "com_ptr.hpp"
#  include <d3d9.h>
#  include <d3d11.h>
#  include <d3d12.h>
#  include <D3D12Downlevel.h>
#  include <GL/gl3w.h>
#  include <vulkan/vulkan.h>

#  define HR_CHECK(exp) { const HRESULT res = (exp); assert(SUCCEEDED(res)); }
#  define VK_CHECK(exp) { const VkResult res = (exp); assert(res == VK_SUCCESS); }

#  define VK_CALL_CMD(name, device, ...) reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name))(__VA_ARGS__)
#  define VK_CALL_DEVICE(name, device, ...) reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name))(device, __VA_ARGS__)
#  define VK_CALL_INSTANCE(name, instance, ...) reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name))(__VA_ARGS__)

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
	g_module_handle = hInstance;
	g_reshade_dll_path = get_module_path(hInstance);
	g_target_executable_path = get_module_path(hInstance);
	g_reshade_base_path = get_base_path();

	reshade::log::open_log_file(g_reshade_base_path / g_reshade_dll_path.filename().replace_extension(L".log"));

	reshade::hooks::register_module(L"user32.dll");

	static UINT s_resize_w = 0, s_resize_h = 0;

	// Register window class
	WNDCLASS wc = { sizeof(wc) };
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

	// Create and show window instance
	const HWND window_handle = CreateWindow(
		wc.lpszClassName, TEXT("ReShade ") TEXT(VERSION_STRING_PRODUCT), WS_OVERLAPPEDWINDOW,
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
		const auto d3d9_module = LoadLibraryW(L"d3d9.dll");
		assert(d3d9_module != nullptr);
		reshade::hooks::register_module(L"d3d9.dll");

		D3DPRESENT_PARAMETERS pp = {};
		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.hDeviceWindow = window_handle;
		pp.Windowed = true;
		pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

		// Initialize Direct3D 9
		com_ptr<IDirect3D9Ex> d3d;
		HR_CHECK(Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d));
		com_ptr<IDirect3DDevice9Ex> device;
		HR_CHECK(d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window_handle, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, nullptr, &device));

		while (msg.message != WM_QUIT)
		{
			while (msg.message != WM_QUIT &&
				PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				DispatchMessage(&msg);

			if (s_resize_w != 0)
			{
				pp.BackBufferWidth = s_resize_w;
				pp.BackBufferHeight = s_resize_h;

				HR_CHECK(device->ResetEx(&pp, nullptr));

				s_resize_w = s_resize_h = 0;
			}

			HR_CHECK(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF7F7F7F, 0, 0));
			HR_CHECK(device->Present(nullptr, nullptr, nullptr, nullptr));
		}

		reshade::hooks::uninstall();

		FreeLibrary(d3d9_module);

		return static_cast<int>(msg.wParam);
	}
	#pragma endregion

	#pragma region D3D11 Implementation
	if (strstr(lpCmdLine, "-d3d11"))
	{
		const auto dxgi_module = LoadLibraryW(L"dxgi.dll");
		const auto d3d11_module = LoadLibraryW(L"d3d11.dll");
		assert(dxgi_module != nullptr);
		assert(d3d11_module != nullptr);
		reshade::hooks::register_module(L"dxgi.dll");
		reshade::hooks::register_module(L"d3d11.dll");

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

			HR_CHECK(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG, nullptr, 0, D3D11_SDK_VERSION, &desc, &swapchain, &device, nullptr, &immediate_context));
		}

		com_ptr<ID3D11Texture2D> backbuffer;
		HR_CHECK(swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)));
		com_ptr<ID3D11RenderTargetView> target;
		HR_CHECK(device->CreateRenderTargetView(backbuffer.get(), nullptr, &target));

		while (msg.message != WM_QUIT)
		{
			while (msg.message != WM_QUIT &&
				PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				DispatchMessage(&msg);

			if (s_resize_w != 0)
			{
				target.reset();
				backbuffer.reset();

				HR_CHECK(swapchain->ResizeBuffers(1, s_resize_w, s_resize_h, DXGI_FORMAT_UNKNOWN, 0));

				HR_CHECK(swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)));
				HR_CHECK(device->CreateRenderTargetView(backbuffer.get(), nullptr, &target));

				s_resize_w = s_resize_h = 0;
			}

			const float color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
			immediate_context->ClearRenderTargetView(target.get(), color);

			HR_CHECK(swapchain->Present(1, 0));
		}

		reshade::hooks::uninstall();

		FreeLibrary(dxgi_module);
		FreeLibrary(d3d11_module);

		return static_cast<int>(msg.wParam);
	}
	#pragma endregion

	#pragma region D3D12 Implementation
	if (strstr(lpCmdLine, "-d3d12"))
	{
		const auto dxgi_module = LoadLibraryW(L"dxgi.dll");
		const auto d3d12_module = LoadLibraryW(L"d3d12.dll");
		assert(dxgi_module != nullptr);
		assert(d3d12_module != nullptr);
		reshade::hooks::register_module(L"dxgi.dll");
		reshade::hooks::register_module(L"d3d12.dll");

		// Enable D3D debug layer if it is available
		{   com_ptr<ID3D12Debug> debug_iface;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_iface))))
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

		HR_CHECK(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));

		// Check if this device was created using d3d12on7 on Windows 7
		// See https://microsoft.github.io/DirectX-Specs/d3d/D3D12onWin7.html for more information
		com_ptr<ID3D12DeviceDownlevel> downlevel;
		const bool is_d3d12on7 = SUCCEEDED(device->QueryInterface(&downlevel));
		downlevel.reset();

		{   D3D12_COMMAND_QUEUE_DESC desc = { D3D12_COMMAND_LIST_TYPE_DIRECT };
			HR_CHECK(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)));
		}

		const UINT num_buffers = is_d3d12on7 ? 1 : ARRAYSIZE(backbuffers);

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

			com_ptr<IDXGIFactory2> dxgi_factory;
			com_ptr<IDXGISwapChain1> dxgi_swapchain;
			HR_CHECK(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgi_factory)));
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
				HR_CHECK(swapchain->GetBuffer(i, IID_PPV_ARGS(&backbuffers[i])));
			}
			else
			{
				RECT window_rect = {};
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
				HR_CHECK(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&backbuffers[i])));
			}

			device->CreateRenderTargetView(backbuffers[i].get(), nullptr, rtv_handle);

			HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_alloc.get(), nullptr, IID_PPV_ARGS(&cmd_lists[i])));

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

			HR_CHECK(cmd_lists[i]->Close());
		}

		while (msg.message != WM_QUIT)
		{
			while (msg.message != WM_QUIT &&
				PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				DispatchMessage(&msg);

			if (s_resize_w != 0)
			{
				// Clean up current resources referencing the back buffer
				for (auto &ptr : cmd_lists)
					ptr.reset();
				for (auto &ptr : backbuffers)
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
				HR_CHECK(queue_downlevel->Present(dummy_list.get(), backbuffers[swap_index].get(), window_handle, D3D12_DOWNLEVEL_PRESENT_FLAG_WAIT_FOR_VBLANK));
			}
			else
			{
				// Synchronization is handled in "runtime_d3d12::on_present"
				HR_CHECK(swapchain->Present(1, 0));
			}
		}

		reshade::hooks::uninstall();

		FreeLibrary(dxgi_module);
		FreeLibrary(d3d12_module);

		return static_cast<int>(msg.wParam);
	}
	#pragma endregion

	#pragma region OpenGL Implementation
	if (strstr(lpCmdLine, "-opengl"))
	{
		const auto opengl_module = LoadLibraryW(L"opengl32.dll");
		assert(opengl_module != nullptr);
		reshade::hooks::register_module(L"opengl32.dll");

		// Initialize OpenGL
		const HDC hdc = GetDC(window_handle);

		PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1 };
		pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;

		SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);

		const HGLRC hglrc1 = wglCreateContext(hdc);
		if (hglrc1 == nullptr)
			return 0;

		wglMakeCurrent(hdc, hglrc1);

		// Create an OpenGL 4.3 context
		const int attribs[] = {
			0x2091 /*WGL_CONTEXT_MAJOR_VERSION_ARB*/, 4,
			0x2092 /*WGL_CONTEXT_MINOR_VERSION_ARB*/, 3,
			0 // Terminate list
		};

		const HGLRC hglrc2 = reinterpret_cast<HGLRC(WINAPI*)(HDC, HGLRC, const int *)>(
			wglGetProcAddress("wglCreateContextAttribsARB"))(hdc, nullptr, attribs);
		if (hglrc2 == nullptr)
			return 0;

		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(hglrc1);
		wglMakeCurrent(hdc, hglrc2);

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

			wglSwapLayerBuffers(hdc, WGL_SWAP_MAIN_PLANE);
		}

		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(hglrc2);

		reshade::hooks::uninstall();

		FreeLibrary(opengl_module);

		return static_cast<int>(msg.wParam);
	}
	#pragma endregion

	#pragma region Vulkan Implementation
	if (strstr(lpCmdLine, "-vulkan"))
	{
		const auto vulkan_module = LoadLibraryW(L"vulkan-1.dll");
		assert(vulkan_module != nullptr);
		reshade::hooks::register_module(L"vulkan-1.dll");

		VkDevice device = VK_NULL_HANDLE;
		VkInstance instance = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkPhysicalDevice physical_device = VK_NULL_HANDLE;

		{   VkApplicationInfo app_info { VK_STRUCTURE_TYPE_APPLICATION_INFO };
			app_info.apiVersion = VK_API_VERSION_1_0;
			app_info.pApplicationName = "ReShade Test Application";
			app_info.applicationVersion = VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION;

			const char *const enabled_layers[] = {
#  if VK_HEADER_VERSION >= 106
				"VK_LAYER_KHRONOS_validation"
#  else
				"VK_LAYER_LUNARG_standard_validation"
#  endif
			};
			const char *const enabled_extensions[] = {
				VK_KHR_SURFACE_EXTENSION_NAME,
				VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
				VK_EXT_DEBUG_REPORT_EXTENSION_NAME
			};

			VkInstanceCreateInfo create_info { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
			create_info.pApplicationInfo = &app_info;
			create_info.enabledLayerCount = ARRAYSIZE(enabled_layers);
			create_info.ppEnabledLayerNames = enabled_layers;
			create_info.enabledExtensionCount = ARRAYSIZE(enabled_extensions);
			create_info.ppEnabledExtensionNames = enabled_extensions;

			VK_CHECK(VK_CALL_INSTANCE(vkCreateInstance, VK_NULL_HANDLE, &create_info, nullptr, &instance));
		}

		// Pick the first physical device.
		uint32_t num_physical_devices = 1;
		VK_CHECK(VK_CALL_INSTANCE(vkEnumeratePhysicalDevices, instance, instance, &num_physical_devices, &physical_device));

		// Pick the first queue with graphics support.
		uint32_t queue_family_index = 0, num_queue_families = 0;
		VK_CALL_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties, instance, physical_device, &num_queue_families, nullptr);
		std::vector<VkQueueFamilyProperties> queue_families(num_queue_families);
		VK_CALL_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties, instance, physical_device, &num_queue_families, queue_families.data());
		for (uint32_t index = 0; index < num_queue_families && (queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0; ++index)
			queue_family_index = index;

		{   VkDeviceQueueCreateInfo queue_info { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
			queue_info.queueCount = 1;
			queue_info.queueFamilyIndex = queue_family_index;
			float queue_priority = 1.0f;
			queue_info.pQueuePriorities = &queue_priority;

			const char *const enabled_layers[] = {
#  if VK_HEADER_VERSION >= 106
				"VK_LAYER_KHRONOS_validation"
#  else
				"VK_LAYER_LUNARG_standard_validation"
#  endif
			};
			const char *const enabled_extensions[] = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME
			};

			VkDeviceCreateInfo create_info { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
			create_info.queueCreateInfoCount = 1;
			create_info.pQueueCreateInfos = &queue_info;
			create_info.enabledLayerCount = ARRAYSIZE(enabled_layers);
			create_info.ppEnabledLayerNames = enabled_layers;
			create_info.enabledExtensionCount = ARRAYSIZE(enabled_extensions);
			create_info.ppEnabledExtensionNames = enabled_extensions;

			VK_CHECK(VK_CALL_INSTANCE(vkCreateDevice, instance, physical_device, &create_info, nullptr, &device));
		}

		{   VkWin32SurfaceCreateInfoKHR create_info { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
			create_info.hinstance = hInstance;
			create_info.hwnd = window_handle;

			VK_CHECK(VK_CALL_INSTANCE(vkCreateWin32SurfaceKHR, instance, instance, &create_info, nullptr, &surface));

			// Check presentation support to make validation layers happy
			VkBool32 supported = VK_FALSE;
			VK_CHECK(VK_CALL_INSTANCE(vkGetPhysicalDeviceSurfaceSupportKHR, instance, physical_device, queue_family_index, surface, &supported));
			assert(VK_FALSE != supported);
		}

		VkQueue queue = VK_NULL_HANDLE;
		VK_CALL_DEVICE(vkGetDeviceQueue, device, queue_family_index, 0, &queue);
		VkCommandPool cmd_alloc = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> cmd_list;

		{   VkCommandPoolCreateInfo create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			create_info.queueFamilyIndex = queue_family_index;

			VK_CHECK(VK_CALL_DEVICE(vkCreateCommandPool, device, &create_info, nullptr, &cmd_alloc));
		}

		const auto resize_swapchain = [&](VkSwapchainKHR old_swapchain = VK_NULL_HANDLE) {
			uint32_t num_present_modes = 0, num_surface_formats = 0;
			VK_CALL_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR, instance, physical_device, surface, &num_surface_formats, nullptr);
			VK_CALL_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR, instance, physical_device, surface, &num_present_modes, nullptr);
			std::vector<VkPresentModeKHR> present_modes(num_present_modes);
			std::vector<VkSurfaceFormatKHR> surface_formats(num_surface_formats);
			VK_CALL_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR, instance, physical_device, surface, &num_surface_formats, surface_formats.data());
			VK_CALL_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR, instance, physical_device, surface, &num_present_modes, present_modes.data());
			VkSurfaceCapabilitiesKHR capabilities = {};
			VK_CALL_INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, instance, physical_device, surface, &capabilities);

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

			VK_CHECK(VK_CALL_DEVICE(vkCreateSwapchainKHR, device, &create_info, nullptr, &swapchain));

			if (old_swapchain != VK_NULL_HANDLE)
				VK_CALL_DEVICE(vkDestroySwapchainKHR, device, old_swapchain, nullptr);
			VK_CALL_DEVICE(vkFreeCommandBuffers, device, cmd_alloc, uint32_t(cmd_list.size()), cmd_list.data());

			uint32_t num_swapchain_images = 0;
			VK_CHECK(VK_CALL_DEVICE(vkGetSwapchainImagesKHR, device, swapchain, &num_swapchain_images, nullptr));
			std::vector<VkImage> swapchain_images(num_swapchain_images);
			VK_CHECK(VK_CALL_DEVICE(vkGetSwapchainImagesKHR, device, swapchain, &num_swapchain_images, swapchain_images.data()));

			cmd_list.resize(num_swapchain_images);

			{   VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
				alloc_info.commandPool = cmd_alloc;
				alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				alloc_info.commandBufferCount = num_swapchain_images;

				VK_CHECK(VK_CALL_DEVICE(vkAllocateCommandBuffers, device, &alloc_info, cmd_list.data()));
			}

			for (uint32_t i = 0; i < num_swapchain_images; ++i)
			{
				VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
				begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				VK_CHECK(VK_CALL_CMD(vkBeginCommandBuffer, device, cmd_list[i], &begin_info));

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
				VK_CALL_CMD(vkCmdPipelineBarrier, device, cmd_list[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

				const VkClearColorValue color = { 0.5f, 0.5f, 0.5f, 1.0f };
				VK_CALL_CMD(vkCmdClearColorImage, device, cmd_list[i], swapchain_images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

				transition.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				transition.dstAccessMask = 0;
				transition.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				transition.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				VK_CALL_CMD(vkCmdPipelineBarrier, device, cmd_list[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

				VK_CHECK(VK_CALL_CMD(vkEndCommandBuffer, device, cmd_list[i]));
			}
		};

		resize_swapchain();

		VkSemaphore sem_acquire = VK_NULL_HANDLE;
		VkSemaphore sem_present = VK_NULL_HANDLE;
		{   VkSemaphoreCreateInfo create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VK_CHECK(VK_CALL_DEVICE(vkCreateSemaphore, device, &create_info, nullptr, &sem_acquire));
			VK_CHECK(VK_CALL_DEVICE(vkCreateSemaphore, device, &create_info, nullptr, &sem_present));
		}

		while (true)
		{
			while (msg.message != WM_QUIT &&
				PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				break;

			if (s_resize_w != 0)
			{
				resize_swapchain(swapchain);

				s_resize_w = s_resize_h = 0;
			}

			uint32_t swapchain_image_index = 0;

			VkResult present_res = VK_CALL_DEVICE(vkAcquireNextImageKHR, device, swapchain, UINT64_MAX, sem_acquire, VK_NULL_HANDLE, &swapchain_image_index);
			if (present_res == VK_SUCCESS)
			{
				VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
				submit_info.waitSemaphoreCount = 1;
				VkPipelineStageFlags wait_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
				submit_info.pWaitDstStageMask = &wait_mask;
				submit_info.pWaitSemaphores = &sem_acquire;
				submit_info.commandBufferCount = 1;
				submit_info.pCommandBuffers = &cmd_list[swapchain_image_index];
				submit_info.signalSemaphoreCount = 1;
				submit_info.pSignalSemaphores = &sem_present;

				VK_CHECK(VK_CALL_CMD(vkQueueSubmit, device, queue, 1, &submit_info, VK_NULL_HANDLE));

				VkPresentInfoKHR present_info { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
				present_info.waitSemaphoreCount = 1;
				present_info.pWaitSemaphores = &sem_present;
				present_info.swapchainCount = 1;
				present_info.pSwapchains = &swapchain;
				present_info.pImageIndices = &swapchain_image_index;

				present_res = VK_CALL_CMD(vkQueuePresentKHR, device, queue, &present_info);
			}

			// Ignore out of date errors during presentation, since swap chain will be recreated on next minimize/maximize event anyway
			if (present_res != VK_SUBOPTIMAL_KHR && present_res != VK_ERROR_OUT_OF_DATE_KHR)
				VK_CHECK(present_res);
		}

		// Wait for all GPU work to finish before destroying objects
		VK_CALL_DEVICE(vkDeviceWaitIdle, device);

		VK_CALL_DEVICE(vkDestroySemaphore, device, sem_acquire, nullptr);
		VK_CALL_DEVICE(vkDestroySemaphore, device, sem_present, nullptr);
		VK_CALL_DEVICE(vkFreeCommandBuffers, device, cmd_alloc, uint32_t(cmd_list.size()), cmd_list.data());
		VK_CALL_DEVICE(vkDestroyCommandPool, device, cmd_alloc, nullptr);
		VK_CALL_DEVICE(vkDestroySwapchainKHR, device, swapchain, nullptr);
		VK_CALL_INSTANCE(vkDestroySurfaceKHR, instance, instance, surface, nullptr);
		VK_CALL_DEVICE(vkDestroyDevice, device, nullptr);
		VK_CALL_INSTANCE(vkDestroyInstance, instance, instance, nullptr);

		reshade::hooks::uninstall();

		FreeLibrary(vulkan_module);

		return static_cast<int>(msg.wParam);
	}
	#pragma endregion

	return EXIT_FAILURE;
}

#else

#  ifndef NDEBUG
#include <DbgHelp.h>

static PVOID g_exception_handler_handle = nullptr;
#  endif

// Export special symbol to identify modules as ReShade instances
extern "C" __declspec(dllexport) const char *ReShadeVersion = VERSION_STRING_PRODUCT;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		// Do NOT call 'DisableThreadLibraryCalls', since we are linking against the static CRT, which requires the thread notifications to work properly
		// It does not do anything when static TLS is used anyway, which is the case (see https://docs.microsoft.com/windows/win32/api/libloaderapi/nf-libloaderapi-disablethreadlibrarycalls)
		g_module_handle = hModule;
		g_reshade_dll_path = get_module_path(hModule);
		g_target_executable_path = get_module_path(nullptr);
		g_reshade_base_path = get_base_path(); // Needs to happen after DLL and executable path are set (since those are referenced in 'get_base_path')

		reshade::log::open_log_file(g_reshade_base_path / g_reshade_dll_path.filename().replace_extension(L".log"));

#  ifdef WIN64
		LOG(INFO) << "Initializing crosire's ReShade version '" VERSION_STRING_FILE "' (64-bit) built on '" VERSION_DATE " " VERSION_TIME "' loaded from " << g_reshade_dll_path << " into " << g_target_executable_path << " ...";
#  else
		LOG(INFO) << "Initializing crosire's ReShade version '" VERSION_STRING_FILE "' (32-bit) built on '" VERSION_DATE " " VERSION_TIME "' loaded from " << g_reshade_dll_path << " into " << g_target_executable_path << " ...";
#  endif

#  ifndef NDEBUG
		g_exception_handler_handle = AddVectoredExceptionHandler(1, [](PEXCEPTION_POINTERS ex) -> LONG {
			// Ignore debugging and some common language exceptions
			if (const DWORD code = ex->ExceptionRecord->ExceptionCode;
				code == CONTROL_C_EXIT || code == 0x406D1388 /* SetThreadName */ ||
				code == DBG_PRINTEXCEPTION_C || code == DBG_PRINTEXCEPTION_WIDE_C || code == STATUS_BREAKPOINT ||
				code == 0xE0434352 /* CLR exception */ ||
				code == 0xE06D7363 /* Visual C++ exception */)
				goto continue_search;

			// Create dump with exception information for the first 100 occurrences
			if (static unsigned int dump_index = 0;
				++dump_index < 100)
			{
				// Call into the original "LoadLibrary" directly, to avoid failing memory corruption checks
				extern HMODULE WINAPI HookLoadLibraryW(LPCWSTR lpFileName);
				const auto ll = reshade::hooks::call(HookLoadLibraryW);
				if (ll == nullptr)
					goto continue_search;

				const auto dbghelp = ll(L"dbghelp.dll");
				if (dbghelp == nullptr)
					goto continue_search;

				const auto dbghelp_write_dump = reinterpret_cast<BOOL(WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION)>(
					GetProcAddress(dbghelp, "MiniDumpWriteDump"));
				if (dbghelp_write_dump == nullptr)
					goto continue_search;

				char dump_name[] = "exception_00.dmp";
				dump_name[10] = '0' + static_cast<char>(dump_index / 10);
				dump_name[11] = '0' + static_cast<char>(dump_index % 10);

				const HANDLE file = CreateFileA(dump_name, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (file == INVALID_HANDLE_VALUE)
					goto continue_search;

				MINIDUMP_EXCEPTION_INFORMATION info;
				info.ThreadId = GetCurrentThreadId();
				info.ExceptionPointers = ex;
				info.ClientPointers = FALSE;

				dbghelp_write_dump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &info, nullptr, nullptr);

				CloseHandle(file);
			}

		continue_search:
			return EXCEPTION_CONTINUE_SEARCH;
		});
#  endif

		// Check if another ReShade instance was already loaded into the process
		if (HMODULE modules[1024]; K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &fdwReason)) // Use kernel32 variant which is available in DllMain
		{
			for (DWORD i = 0; i < std::min<DWORD>(fdwReason / sizeof(HMODULE), ARRAYSIZE(modules)); ++i)
			{
				if (modules[i] != hModule && GetProcAddress(modules[i], "ReShadeVersion") != nullptr)
				{
					LOG(WARN) << "Another ReShade instance was already loaded from " << get_module_path(modules[i]) << "! Aborting initialization ...";
					return FALSE; // Make the "LoadLibrary" call that loaded this instance fail
				}
			}
		}

		reshade::hooks::register_module(L"user32.dll");
		reshade::hooks::register_module(L"ws2_32.dll");

		reshade::hooks::register_module(get_system_path() / L"d2d1.dll");
		reshade::hooks::register_module(get_system_path() / L"d3d9.dll");
		reshade::hooks::register_module(get_system_path() / L"d3d10.dll");
		reshade::hooks::register_module(get_system_path() / L"d3d10_1.dll");
		reshade::hooks::register_module(get_system_path() / L"d3d11.dll");

		// On Windows 7 the d3d12on7 module is not in the system path, so register to hook any d3d12.dll loaded instead
		if (is_windows7() && _wcsicmp(g_reshade_dll_path.stem().c_str(), L"d3d12") != 0)
			reshade::hooks::register_module(L"d3d12.dll");
		else
			reshade::hooks::register_module(get_system_path() / L"d3d12.dll");

		reshade::hooks::register_module(get_system_path() / L"dxgi.dll");
		reshade::hooks::register_module(get_system_path() / L"opengl32.dll");
		// Do not register Vulkan hooks, since Vulkan layering mechanism is used instead

		LOG(INFO) << "Initialized.";
		break;
	case DLL_PROCESS_DETACH:
		LOG(INFO) << "Exiting ...";

		reshade::hooks::uninstall();

		// Module is now invalid, so break out of any message loops that may still have it in the call stack (see 'HookGetMessage' implementation in input.cpp)
		// This is necessary since a different thread may have called into the 'GetMessage' hook from ReShade, but not receive a message until after the module was unloaded
		// At that point it would return to code that was already unloaded and crash
		// Hooks were already uninstalled now, so after returning from any existing 'GetMessage' hook call, application will call the real one next and things continue to work
		g_module_handle = nullptr;
		// This duration has to be slightly larger than the timeout in 'HookGetMessage' to ensure success
		// It should also be large enough to cover any potential other calls to previous hooks that may still be in flight from other threads
		Sleep(1050);

#  ifndef NDEBUG
		RemoveVectoredExceptionHandler(g_exception_handler_handle);
#  endif

		LOG(INFO) << "Finished exiting.";
		break;
	}

	return TRUE;
}

#endif
