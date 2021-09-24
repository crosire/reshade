/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_GUI

#include "version.h"
#include "dll_log.hpp"
#include "runtime.hpp"
#include "hook_manager.hpp"
#include "dll_resources.hpp"
#include "vulkan/vulkan_impl_device.hpp"
#include <cassert>
#include <openvr.h>
#include <ivrclientcore.h>
#include <imgui.h>
#include <stb_image.h>

static vr::IVROverlay *s_overlay = nullptr;
extern vr::IVRClientCore *g_client_core;
static vr::VROverlayHandle_t s_main_handle = vr::k_ulOverlayHandleInvalid;
static vr::VROverlayHandle_t s_thumbnail_handle = vr::k_ulOverlayHandleInvalid;

static const uint32_t OVERLAY_WIDTH = 400;
static const uint32_t OVERLAY_HEIGHT = 400;

void reshade::runtime::init_gui_vr()
{
	if (s_main_handle != vr::k_ulOverlayHandleInvalid)
		return;

	if (s_overlay == nullptr)
	{
		assert(g_client_core != nullptr);

		vr::EVRInitError init_e;
		if ((s_overlay = static_cast<vr::IVROverlay *>(g_client_core->GetGenericInterface(vr::IVROverlay_Version, &init_e))) == nullptr)
			return;
	}

	const vr::EVROverlayError overlay_e = s_overlay->CreateDashboardOverlay("reshade", "ReShade " VERSION_STRING_PRODUCT, &s_main_handle, &s_thumbnail_handle);
	if (overlay_e != vr::VROverlayError_None)
	{
		LOG(ERROR) << "Failed to create VR dashboard overlay with error code " << static_cast<int>(overlay_e) << '.';
		return;
	}

	if (s_thumbnail_handle != vr::k_ulOverlayHandleInvalid)
	{
		const resources::data_resource icon = resources::load_data_resource(IDB_MAIN_ICON);

		int width = 0, height = 0, channels = 0;
		stbi_uc *const icon_data = stbi_load_from_memory(static_cast<const stbi_uc *>(icon.data), static_cast<int>(icon.data_size), &width, &height, &channels, 0);
		s_overlay->SetOverlayRaw(s_thumbnail_handle, icon_data, width, height, channels);
		stbi_image_free(icon_data);
	}

	s_overlay->SetOverlayInputMethod(s_main_handle, vr::VROverlayInputMethod_Mouse);
	const vr::HmdVector2_t window_size = { static_cast<float>(OVERLAY_WIDTH), static_cast<float>(OVERLAY_HEIGHT) };
	s_overlay->SetOverlayMouseScale(s_main_handle, &window_size);

	s_overlay->SetOverlayWidthInMeters(s_main_handle, 1.5f);
	s_overlay->SetOverlayFlag(s_main_handle, vr::VROverlayFlags_SendVRSmoothScrollEvents, true);

	if (!_device->create_resource(api::resource_desc(OVERLAY_WIDTH, OVERLAY_HEIGHT, 1, 1, api::format::r8g8b8a8_unorm, 1, api::memory_heap::gpu_only, api::resource_usage::render_target | api::resource_usage::shader_resource), nullptr, api::resource_usage::shader_resource_pixel, &_vr_overlay_texture))
	{
		LOG(ERROR) << "Failed to create VR dashboard overlay texture!";
		return;
	}
	if (!_device->create_resource_view(_vr_overlay_texture, api::resource_usage::render_target, api::resource_view_desc(api::format::r8g8b8a8_unorm), &_vr_overlay_target))
	{
		LOG(ERROR) << "Failed to create VR dashboard overlay render target!";
		return;
	}

	api::render_pass_desc pass_desc = {};
	pass_desc.depth_stencil_format = _effect_stencil_format;
	pass_desc.render_targets_format[0] = api::format::r8g8b8a8_unorm;
	pass_desc.samples = 1;

	if (!_device->create_render_pass(pass_desc, &_vr_overlay_pass))
	{
		LOG(ERROR) << "Failed to create VR dashboard overlay render pass!";
		return;
	}

	api::framebuffer_desc fbo_desc = {};
	fbo_desc.render_pass_template = _vr_overlay_pass;
	fbo_desc.depth_stencil = _effect_stencil_target;
	fbo_desc.render_targets[0] = _vr_overlay_target;
	fbo_desc.width = OVERLAY_WIDTH;
	fbo_desc.height = OVERLAY_HEIGHT;
	fbo_desc.layers = 1;

	if (!_device->create_framebuffer(fbo_desc, &_vr_overlay_fbo))
	{
		LOG(ERROR) << "Failed to create VR dashboard overlay framebuffer!";
		return;
	}
}

void reshade::runtime::deinit_gui_vr()
{
	if (s_main_handle == vr::k_ulOverlayHandleInvalid)
		return;

	_device->destroy_resource(_vr_overlay_texture);
	_vr_overlay_texture = {};
	_device->destroy_resource_view(_vr_overlay_target);
	_vr_overlay_target = {};
	_device->destroy_render_pass(_vr_overlay_pass);
	_vr_overlay_pass = {};

	s_overlay->DestroyOverlay(s_main_handle);
	s_main_handle = vr::k_ulOverlayHandleInvalid;
	s_overlay->DestroyOverlay(s_thumbnail_handle);
	s_thumbnail_handle = vr::k_ulOverlayHandleInvalid;
}

void reshade::runtime::draw_gui_vr()
{
	_gather_gpu_statistics = false;

	if (s_main_handle == vr::k_ulOverlayHandleInvalid || !s_overlay->IsOverlayVisible(s_main_handle))
		return;

	if (_rebuild_font_atlas)
		build_font_atlas();
	if (_font_atlas_srv.handle == 0)
		return; // Cannot render GUI without font atlas

	ImGui::SetCurrentContext(_imgui_context);
	auto &imgui_io = ImGui::GetIO();
	imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
	imgui_io.DisplaySize.x = static_cast<float>(OVERLAY_WIDTH);
	imgui_io.DisplaySize.y = static_cast<float>(OVERLAY_HEIGHT);
	imgui_io.Fonts->TexID = _font_atlas_srv.handle;

	imgui_io.KeysDown[0x08] = false;
	imgui_io.KeysDown[0x09] = false;
	imgui_io.KeysDown[0x0D] = false;

	vr::VREvent_t ev;
	while (s_overlay->PollNextOverlayEvent(s_main_handle, &ev, sizeof(ev)))
	{
		switch (ev.eventType)
		{
		case vr::VREvent_MouseMove:
			imgui_io.MousePos.x = ev.data.mouse.x;
			imgui_io.MousePos.y = (_renderer_id & 0x10000) != 0 ? ev.data.mouse.y : OVERLAY_HEIGHT - ev.data.mouse.y;
			break;
		case vr::VREvent_MouseButtonDown:
			switch (ev.data.mouse.button)
			{
			case vr::VRMouseButton_Left:
				imgui_io.MouseDown[ImGuiMouseButton_Left] = true;
				break;
			case vr::VRMouseButton_Right:
				imgui_io.MouseDown[ImGuiMouseButton_Right] = true;
				break;
			case vr::VRMouseButton_Middle:
				imgui_io.MouseDown[ImGuiMouseButton_Middle] = true;
				break;
			}
			break;
		case vr::VREvent_MouseButtonUp:
			switch (ev.data.mouse.button)
			{
			case vr::VRMouseButton_Left:
				imgui_io.MouseDown[ImGuiMouseButton_Left] = false;
				break;
			case vr::VRMouseButton_Right:
				imgui_io.MouseDown[ImGuiMouseButton_Right] = false;
				break;
			case vr::VRMouseButton_Middle:
				imgui_io.MouseDown[ImGuiMouseButton_Middle] = false;
				break;
			}
			break;
		case vr::VREvent_ScrollSmooth:
			imgui_io.MouseWheel += ev.data.scroll.ydelta;
			imgui_io.MouseWheelH += ev.data.scroll.xdelta;
			break;
		case vr::VREvent_KeyboardCharInput:
			if (ev.data.keyboard.cNewInput[0] == '\b')
				imgui_io.KeysDown[0x08] = true;
			if (ev.data.keyboard.cNewInput[0] == '\t')
				imgui_io.KeysDown[0x09] = true;
			if (ev.data.keyboard.cNewInput[0] == '\n')
				imgui_io.KeysDown[0x0D] = true;
			for (int i = 0; i < 8 && ev.data.keyboard.cNewInput[i] != 0; ++i)
				imgui_io.AddInputCharacter(ev.data.keyboard.cNewInput[i]);
			break;
		}
	}

	ImGui::NewFrame();

	const ImGuiViewport *const viewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::Begin("VR Viewport", nullptr,
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDocking);

	if (ImGui::BeginMenuBar())
	{
		for (size_t menu_index = 0; menu_index < _menu_callables.size(); ++menu_index)
			if (ImGui::MenuItem(_menu_callables[menu_index].first.c_str(), nullptr, menu_index == _selected_menu))
				_selected_menu = menu_index;

#if RESHADE_ADDON
		if (addon::enabled)
		{
			for (size_t menu_index = 0; menu_index < addon::overlay_list.size(); ++menu_index)
				if (ImGui::MenuItem(addon::overlay_list[menu_index].first.c_str(), nullptr, (menu_index + _menu_callables.size()) == _selected_menu))
					_selected_menu = menu_index + _menu_callables.size();
		}
#endif

		ImGui::EndMenuBar();
	}

	if (_selected_menu < _menu_callables.size())
		_menu_callables[_selected_menu].second();
#if RESHADE_ADDON
	else if ((_selected_menu - _menu_callables.size()) < addon::overlay_list.size())
		addon::overlay_list[_selected_menu - _menu_callables.size()].second(this, _imgui_context);
#endif
	else
		_selected_menu = 0;

	ImGui::End();

	if (imgui_io.WantTextInput)
	{
		s_overlay->ShowKeyboardForOverlay(s_main_handle, vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine, vr::KeyboardFlag_Minimal, nullptr, 0, "", 0);
	}

	ImGui::Render();

	if (ImDrawData *const draw_data = ImGui::GetDrawData();
		draw_data != nullptr && draw_data->CmdListsCount != 0 && draw_data->TotalVtxCount != 0)
	{
		api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();

		cmd_list->barrier(_vr_overlay_texture, api::resource_usage::shader_resource_pixel, api::resource_usage::render_target);
		render_imgui_draw_data(cmd_list, draw_data, _vr_overlay_pass, _vr_overlay_fbo);
		cmd_list->barrier(_vr_overlay_texture, api::resource_usage::render_target, api::resource_usage::shader_resource_pixel);
	}

	vr::Texture_t texture;
	texture.eColorSpace = vr::ColorSpace_Auto;
	vr::VRVulkanTextureData_t vulkan_data;

	switch (_device->get_api())
	{
	case api::device_api::d3d10:
	case api::device_api::d3d11:
		texture.handle = reinterpret_cast<void *>(_vr_overlay_texture.handle);
		texture.eType = vr::TextureType_DirectX;
		break;
	case api::device_api::d3d12:
		texture.handle = reinterpret_cast<void *>(_vr_overlay_texture.handle);
		texture.eType = vr::TextureType_DirectX12;
		break;
	case api::device_api::opengl:
		texture.handle = reinterpret_cast<void *>(_vr_overlay_texture.handle & 0xFFFFFFFF);
		texture.eType = vr::TextureType_OpenGL;
		break;
	case api::device_api::vulkan:
		const auto device_impl = static_cast<vulkan::device_impl *>(_device);
		vulkan_data.m_nImage = _vr_overlay_texture.handle;
		vulkan_data.m_pDevice = reinterpret_cast<VkDevice_T *>(_device->get_native_object());
		vulkan_data.m_pPhysicalDevice = device_impl->_physical_device;
		vulkan_data.m_pInstance = VK_NULL_HANDLE;
		vulkan_data.m_pQueue = reinterpret_cast<VkQueue_T *>(_graphics_queue->get_native_object());
		vulkan_data.m_nQueueFamilyIndex = device_impl->_graphics_queue_family_index;
		vulkan_data.m_nWidth = 800;
		vulkan_data.m_nHeight = 600;
		vulkan_data.m_nFormat = VK_FORMAT_R8G8B8A8_UNORM;
		vulkan_data.m_nSampleCount = 1;
		texture.handle = &vulkan_data;
		texture.eType = vr::TextureType_Vulkan;
		break;
	}

	s_overlay->SetOverlayTexture(s_main_handle, &texture);
}

#endif
