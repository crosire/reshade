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
#include "imgui_widgets.hpp"
#include "vulkan/vulkan_impl_device.hpp"
#include <cassert>
#include <openvr.h>
#include <ivrclientcore.h>
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

	const vr::HmdVector2_t window_size = { static_cast<float>(OVERLAY_WIDTH), static_cast<float>(OVERLAY_HEIGHT) };
	s_overlay->SetOverlayMouseScale(s_main_handle, &window_size);
	s_overlay->SetOverlayInputMethod(s_main_handle, vr::VROverlayInputMethod_Mouse);

	s_overlay->SetOverlayFlag(s_main_handle, vr::VROverlayFlags_SendVRSmoothScrollEvents, true);

	s_overlay->SetOverlayWidthInMeters(s_main_handle, 1.5f);

	if (!_device->create_resource(api::resource_desc(OVERLAY_WIDTH, OVERLAY_HEIGHT, 1, 1, api::format::r8g8b8a8_unorm, 1, api::memory_heap::gpu_only, api::resource_usage::render_target | api::resource_usage::shader_resource), nullptr, api::resource_usage::shader_resource_pixel, &_vr_overlay_tex))
	{
		LOG(ERROR) << "Failed to create VR dashboard overlay texture!";
		return;
	}
	if (!_device->create_resource_view(_vr_overlay_tex, api::resource_usage::render_target, api::resource_view_desc(api::format::r8g8b8a8_unorm), &_vr_overlay_target))
	{
		LOG(ERROR) << "Failed to create VR dashboard overlay render target!";
		return;
	}
}

void reshade::runtime::deinit_gui_vr()
{
	if (s_main_handle == vr::k_ulOverlayHandleInvalid)
		return;

	_device->destroy_resource(_vr_overlay_tex);
	_vr_overlay_tex = {};
	_device->destroy_resource_view(_vr_overlay_target);
	_vr_overlay_target = {};

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

	ImGuiContext *const backup_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(_imgui_context);

	auto &imgui_io = ImGui::GetIO();
	imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
	imgui_io.DisplaySize.x = static_cast<float>(OVERLAY_WIDTH);
	imgui_io.DisplaySize.y = static_cast<float>(OVERLAY_HEIGHT);
	imgui_io.Fonts->TexID = _font_atlas_srv.handle;

	imgui_io.KeysDown[0x08] = false;
	imgui_io.KeysDown[0x09] = false;
	imgui_io.KeysDown[0x0D] = false;

	bool keyboard_closed = false;

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
		case vr::VREvent_KeyboardClosed:
			ImGui::ClearActiveID();
			keyboard_closed = true;
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

	if (imgui_io.WantTextInput && !keyboard_closed)
	{
		s_overlay->ShowKeyboardForOverlay(s_main_handle, vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine, vr::KeyboardFlag_Minimal, nullptr, 0, "", 0);
		// Avoid keyboard showing up behind dashboard overlay
		s_overlay->SetKeyboardPositionForOverlay(s_main_handle, vr::HmdRect2_t { { 0.0f, 1.0f }, { 1.0f, 0.0f } });
	}

	static constexpr std::pair<const char *, void(runtime::*)()> overlay_callbacks[] = {
#if RESHADE_EFFECTS
		{ "Home", &runtime::draw_gui_home },
#endif
#if RESHADE_ADDON
		{ "Add-ons", &runtime::draw_gui_addons },
#endif
		{ "Settings", &runtime::draw_gui_settings },
		{ "Statistics", &runtime::draw_gui_statistics },
		{ "Log", &runtime::draw_gui_log },
		{ "About", &runtime::draw_gui_about }
	};

	const ImGuiViewport *const viewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::Begin("VR Viewport", nullptr,
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDocking);

	int overlay_index = 0;
	int selected_overlay_index = ImGui::GetStateStorage()->GetInt(ImGui::GetID("##overlay_index"), 0);

	ImGui::BeginChild("##overlay", ImVec2(0, ImGui::GetFrameHeight()), false, ImGuiWindowFlags_NoScrollbar);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(_imgui_context->Style.FramePadding.x, 0));

	for (const auto &widget : overlay_callbacks)
	{
		if (bool state = (overlay_index == selected_overlay_index);
			imgui::toggle_button(widget.first, state, 0.0f, ImGuiButtonFlags_AlignTextBaseLine))
			selected_overlay_index = overlay_index;
		ImGui::SameLine();

		++overlay_index;
	}

#if RESHADE_ADDON
	if (addon::enabled)
	{
		for (const auto &info : addon::loaded_info)
		{
			for (const auto &widget : info.overlay_callbacks)
			{
				if (bool state = (overlay_index == selected_overlay_index);
					imgui::toggle_button(widget.first.c_str(), state, 0.0f, ImGuiButtonFlags_AlignTextBaseLine))
					selected_overlay_index = overlay_index;
				ImGui::SameLine();

				++overlay_index;
			}
		}
	}
#endif

	ImGui::PopStyleVar();
	ImGui::EndChild();

	if (selected_overlay_index < static_cast<int>(std::size(overlay_callbacks)))
	{
		(this->*overlay_callbacks[selected_overlay_index].second)();
	}
#if RESHADE_ADDON
	else if (selected_overlay_index < overlay_index)
	{
		assert(addon::enabled);

		overlay_index = static_cast<int>(std::size(overlay_callbacks));

		for (const auto &info : addon::loaded_info)
		{
			for (const auto &widget : info.overlay_callbacks)
			{
				if (selected_overlay_index == overlay_index++)
				{
					widget.second(this);
					break;
				}
			}
		}
	}
#endif
	else
	{
		selected_overlay_index = 0;
	}

	ImGui::GetStateStorage()->SetInt(ImGui::GetID("##overlay_index"), selected_overlay_index);

	ImGui::End(); // VR Viewport window

	ImGui::Render();

	if (ImDrawData *const draw_data = ImGui::GetDrawData();
		draw_data != nullptr && draw_data->CmdListsCount != 0 && draw_data->TotalVtxCount != 0)
	{
		api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();

		cmd_list->barrier(_vr_overlay_tex, api::resource_usage::shader_resource_pixel, api::resource_usage::render_target);
		render_imgui_draw_data(cmd_list, draw_data, _vr_overlay_target);
		cmd_list->barrier(_vr_overlay_tex, api::resource_usage::render_target, api::resource_usage::shader_resource_pixel);
	}

	ImGui::SetCurrentContext(backup_context);

	vr::Texture_t texture;
	texture.eColorSpace = vr::ColorSpace_Auto;
	vr::VRVulkanTextureData_t vulkan_data;

	switch (_device->get_api())
	{
	case api::device_api::d3d10:
	case api::device_api::d3d11:
		texture.handle = reinterpret_cast<void *>(_vr_overlay_tex.handle);
		texture.eType = vr::TextureType_DirectX;
		break;
	case api::device_api::d3d12:
		texture.handle = reinterpret_cast<void *>(_vr_overlay_tex.handle);
		texture.eType = vr::TextureType_DirectX12;
		break;
	case api::device_api::opengl:
		texture.handle = reinterpret_cast<void *>(_vr_overlay_tex.handle & 0xFFFFFFFF);
		texture.eType = vr::TextureType_OpenGL;
		break;
	case api::device_api::vulkan:
		const auto device_impl = static_cast<vulkan::device_impl *>(_device);
		vulkan_data.m_nImage = _vr_overlay_tex.handle;
		vulkan_data.m_pDevice = reinterpret_cast<VkDevice_T *>(_device->get_native_object());
		vulkan_data.m_pPhysicalDevice = device_impl->_physical_device;
		vulkan_data.m_pInstance = VK_NULL_HANDLE;
		vulkan_data.m_pQueue = reinterpret_cast<VkQueue_T *>(_graphics_queue->get_native_object());
		vulkan_data.m_nQueueFamilyIndex = device_impl->_graphics_queue_family_index;
		vulkan_data.m_nWidth = OVERLAY_WIDTH;
		vulkan_data.m_nHeight = OVERLAY_HEIGHT;
		vulkan_data.m_nFormat = VK_FORMAT_R8G8B8A8_UNORM;
		vulkan_data.m_nSampleCount = 1;
		texture.handle = &vulkan_data;
		texture.eType = vr::TextureType_Vulkan;
		break;
	}

	s_overlay->SetOverlayTexture(s_main_handle, &texture);
}

#endif
