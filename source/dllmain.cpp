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
#include "vulkan/runtime_vulkan.hpp"

#define HCHECK(exp) assert(SUCCEEDED(exp))
#define VKCHECK(exp) assert((exp) == VK_SUCCESS)

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

		while (msg.message != WM_QUIT)
		{
			while (msg.message != WM_QUIT &&
				PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				DispatchMessage(&msg);

			const UINT swap_index = swapchain->GetCurrentBackBufferIndex();

			ID3D12CommandList *const cmd_list = cmd_lists[swap_index].get();
			command_queue->ExecuteCommandLists(1, &cmd_list);

			// Synchronization is handled in "runtime_d3d12::on_present"
			swapchain->Present(1, 0);
		}

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

	#pragma region Vulkan Implementation
	if (strstr(lpCmdLine, "-vulkan"))
	{
		hooks::register_module("vulkan-1.dll");

		VkDevice device = VK_NULL_HANDLE;
		VkInstance instance = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkPhysicalDevice physical_device = VK_NULL_HANDLE;

		{   VkApplicationInfo app_info { VK_STRUCTURE_TYPE_APPLICATION_INFO };
			app_info.apiVersion = VK_API_VERSION_1_1;
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
				VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
				VK_EXT_DEBUG_REPORT_EXTENSION_NAME
			};

			VkInstanceCreateInfo create_info { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
			create_info.pApplicationInfo = &app_info;
			create_info.enabledLayerCount = ARRAYSIZE(enabled_layers);
			create_info.ppEnabledLayerNames = enabled_layers;
			create_info.enabledExtensionCount = ARRAYSIZE(enabled_extensions);
			create_info.ppEnabledExtensionNames = enabled_extensions;

			VKCHECK(vkCreateInstance(&create_info, nullptr, &instance));
		}

		// Pick the first physical device.
		uint32_t num_physical_devices = 1;
		VKCHECK(vkEnumeratePhysicalDevices(instance, &num_physical_devices, &physical_device));

		// Pick the first queue with graphics support.
		uint32_t queue_family_index = 0, num_queue_families = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, nullptr);
		std::vector<VkQueueFamilyProperties> queue_families(num_queue_families);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, queue_families.data());
		for (uint32_t index = 0; index < num_queue_families && (queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0; ++index)
			queue_family_index = index;

		{   VkDeviceQueueCreateInfo queue_info { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
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
			create_info.enabledLayerCount = ARRAYSIZE(enabled_layers);
			create_info.ppEnabledLayerNames = enabled_layers;
			create_info.enabledExtensionCount = ARRAYSIZE(enabled_extensions);
			create_info.ppEnabledExtensionNames = enabled_extensions;

			VKCHECK(vkCreateDevice(physical_device, &create_info, nullptr, &device));
		}

		{   VkWin32SurfaceCreateInfoKHR create_info { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
			create_info.hinstance = hInstance;
			create_info.hwnd = window_handle;

			VKCHECK(vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface));

			// Check presentation support to make validation layers happy
			VkBool32 supported = VK_FALSE;
			VKCHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family_index, surface, &supported));
			assert(VK_FALSE != supported);
		}

		VkQueue queue = VK_NULL_HANDLE;
		vkGetDeviceQueue(device, queue_family_index, 0, &queue);
		VkCommandPool cmd_alloc = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> cmd_list;

		{   VkCommandPoolCreateInfo create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			create_info.queueFamilyIndex = queue_family_index;

			VKCHECK(vkCreateCommandPool(device, &create_info, nullptr, &cmd_alloc));
		}

		const auto resize_swapchain = [&](VkSwapchainKHR old_swapchain = VK_NULL_HANDLE) {
			uint32_t num_present_modes = 0, num_surface_formats = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_surface_formats, nullptr);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &num_present_modes, nullptr);
			std::vector<VkPresentModeKHR> present_modes(num_present_modes);
			std::vector<VkSurfaceFormatKHR> surface_formats(num_surface_formats);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_surface_formats, surface_formats.data());
			vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &num_present_modes, present_modes.data());
			VkSurfaceCapabilitiesKHR capabilities = {};
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);

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

			VKCHECK(vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain));

			if (old_swapchain != VK_NULL_HANDLE)
				vkDestroySwapchainKHR(device, old_swapchain, nullptr);
			vkFreeCommandBuffers(device, cmd_alloc, uint32_t(cmd_list.size()), cmd_list.data());

			uint32_t num_swapchain_images = 0;
			VKCHECK(vkGetSwapchainImagesKHR(device, swapchain, &num_swapchain_images, nullptr));
			std::vector<VkImage> swapchain_images(num_swapchain_images);
			VKCHECK(vkGetSwapchainImagesKHR(device, swapchain, &num_swapchain_images, swapchain_images.data()));

			cmd_list.resize(num_swapchain_images);

			{   VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
				alloc_info.commandPool = cmd_alloc;
				alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				alloc_info.commandBufferCount = num_swapchain_images;

				VKCHECK(vkAllocateCommandBuffers(device, &alloc_info, cmd_list.data()));
			}

			for (uint32_t i = 0; i < num_swapchain_images; ++i)
			{
				VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
				begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				VKCHECK(vkBeginCommandBuffer(cmd_list[i], &begin_info));

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
				vkCmdPipelineBarrier(cmd_list[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

				const VkClearColorValue color = { 0.5f, 0.5f, 0.5f, 1.0f };
				vkCmdClearColorImage(cmd_list[i], swapchain_images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

				transition.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				transition.dstAccessMask = 0;
				transition.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				transition.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				vkCmdPipelineBarrier(cmd_list[i], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

				VKCHECK(vkEndCommandBuffer(cmd_list[i]));
			}
		};

		resize_swapchain();

		VkSemaphore sem_acquire = VK_NULL_HANDLE;
		VkSemaphore sem_present = VK_NULL_HANDLE;
		{   VkSemaphoreCreateInfo create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VKCHECK(vkCreateSemaphore(device, &create_info, nullptr, &sem_acquire));
			VKCHECK(vkCreateSemaphore(device, &create_info, nullptr, &sem_present));
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
			VKCHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, sem_acquire, VK_NULL_HANDLE, &swapchain_image_index));

			VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
			submit_info.waitSemaphoreCount = 1;
			VkPipelineStageFlags wait_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			submit_info.pWaitDstStageMask = &wait_mask;
			submit_info.pWaitSemaphores = &sem_acquire;
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &cmd_list[swapchain_image_index];
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = &sem_present;

			VKCHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));

			VkPresentInfoKHR present_info { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
			present_info.waitSemaphoreCount = 1;
			present_info.pWaitSemaphores = &sem_present;
			present_info.swapchainCount = 1;
			present_info.pSwapchains = &swapchain;
			present_info.pImageIndices = &swapchain_image_index;

			VKCHECK(vkQueuePresentKHR(queue, &present_info));
		}

		// Wait for all GPU work to finish before destroying objects
		vkDeviceWaitIdle(device);

		vkDestroySemaphore(device, sem_acquire, nullptr);
		vkDestroySemaphore(device, sem_present, nullptr);
		vkFreeCommandBuffers(device, cmd_alloc, uint32_t(cmd_list.size()), cmd_list.data());
		vkDestroyCommandPool(device, cmd_alloc, nullptr);
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyDevice(device, nullptr);
		vkDestroyInstance(instance, nullptr);

		reshade::hooks::uninstall();

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
		hooks::register_module(get_system_path() / "vulkan-1.dll");

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
