/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_GUI

#include "runtime.hpp"
#include "version.h"
#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "imgui_widgets.hpp"
#include "localization.hpp"
#include "addon_manager.hpp"
#include "vulkan/vulkan_impl_device.hpp"
#include <openvr.h>
#include <ivrclientcore.h>
#include <stb_image.h>

static vr::IVROverlay *s_overlay = nullptr;
extern vr::IVRClientCore *g_client_core;
static vr::VROverlayHandle_t s_main_handle = vr::k_ulOverlayHandleInvalid;
static vr::VROverlayHandle_t s_thumbnail_handle = vr::k_ulOverlayHandleInvalid;

static constexpr uint32_t OVERLAY_WIDTH = 500;
static constexpr uint32_t OVERLAY_HEIGHT = 500;

bool reshade::runtime::init_gui_vr()
{
	if (s_main_handle != vr::k_ulOverlayHandleInvalid)
		return true;

	if (s_overlay == nullptr)
	{
		if (g_client_core == nullptr)
		{
			LOG(ERROR) << "Failed to create VR dashboard overlay because SteamVR is not loaded!";
			return true; // Do not prevent effect runtime from initializing
		}

		vr::EVRInitError init_e;
		if ((s_overlay = static_cast<vr::IVROverlay *>(g_client_core->GetGenericInterface(vr::IVROverlay_Version, &init_e))) == nullptr)
			return false;
	}

	const vr::EVROverlayError overlay_e = s_overlay->CreateDashboardOverlay("reshade", "ReShade " VERSION_STRING_PRODUCT, &s_main_handle, &s_thumbnail_handle);
	if (overlay_e != vr::VROverlayError_None)
	{
		LOG(ERROR) << "Failed to create VR dashboard overlay with error code " << static_cast<int>(overlay_e) << '!';
		return false;
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

	if (!_device->create_resource(api::resource_desc(OVERLAY_WIDTH, OVERLAY_HEIGHT, 1, 1, api::format::r8g8b8a8_unorm, 1, api::memory_heap::gpu_only, api::resource_usage::render_target | api::resource_usage::copy_source), nullptr, api::resource_usage::copy_source, &_vr_overlay_tex))
	{
		LOG(ERROR) << "Failed to create VR dashboard overlay texture!";
		return false;
	}
	if (!_device->create_resource_view(_vr_overlay_tex, api::resource_usage::render_target, api::resource_view_desc(api::format::r8g8b8a8_unorm), &_vr_overlay_target))
	{
		LOG(ERROR) << "Failed to create VR dashboard overlay render target!";
		return false;
	}

	return true;
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
#if RESHADE_FX
	_gather_gpu_statistics = false;
#endif

	if (s_main_handle == vr::k_ulOverlayHandleInvalid || !s_overlay->IsOverlayVisible(s_main_handle))
		return;

	build_font_atlas();
	if (_font_atlas_srv == 0)
		return; // Cannot render GUI without font atlas

	ImGuiContext *const backup_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(_imgui_context);

	ImGuiIO &imgui_io = ImGui::GetIO();
	imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
	imgui_io.DisplaySize.x = static_cast<float>(OVERLAY_WIDTH);
	imgui_io.DisplaySize.y = static_cast<float>(OVERLAY_HEIGHT);
	imgui_io.Fonts->TexID = _font_atlas_srv.handle;

	imgui_io.AddKeyEvent(ImGuiKey_Backspace, false);
	imgui_io.AddKeyEvent(ImGuiKey_Tab, false);
	imgui_io.AddKeyEvent(ImGuiKey_Enter, false);

	bool keyboard_closed = false;

	vr::VREvent_t ev;
	while (s_overlay->PollNextOverlayEvent(s_main_handle, &ev, sizeof(ev)))
	{
		switch (ev.eventType)
		{
		case vr::VREvent_MouseMove:
			imgui_io.AddMousePosEvent(
				ev.data.mouse.x,
				(_renderer_id & 0x10000) != 0 ? ev.data.mouse.y : OVERLAY_HEIGHT - ev.data.mouse.y);
			break;
		case vr::VREvent_MouseButtonDown:
			switch (ev.data.mouse.button)
			{
			case vr::VRMouseButton_Left:
				imgui_io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
				break;
			case vr::VRMouseButton_Right:
				imgui_io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
				break;
			case vr::VRMouseButton_Middle:
				imgui_io.AddMouseButtonEvent(ImGuiMouseButton_Middle, true);
				break;
			}
			break;
		case vr::VREvent_MouseButtonUp:
			switch (ev.data.mouse.button)
			{
			case vr::VRMouseButton_Left:
				imgui_io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
				break;
			case vr::VRMouseButton_Right:
				imgui_io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
				break;
			case vr::VRMouseButton_Middle:
				imgui_io.AddMouseButtonEvent(ImGuiMouseButton_Middle, false);
				break;
			}
			break;
		case vr::VREvent_ScrollSmooth:
			imgui_io.AddMouseWheelEvent(ev.data.scroll.xdelta, ev.data.scroll.ydelta);
			break;
		case vr::VREvent_KeyboardClosed:
			ImGui::ClearActiveID();
			keyboard_closed = true;
			break;
		case vr::VREvent_KeyboardCharInput:
			if (ev.data.keyboard.cNewInput[0] == '\b')
				imgui_io.AddKeyEvent(ImGuiKey_Backspace, true);
			if (ev.data.keyboard.cNewInput[0] == '\t')
				imgui_io.AddKeyEvent(ImGuiKey_Tab, true);
			if (ev.data.keyboard.cNewInput[0] == '\n')
				imgui_io.AddKeyEvent(ImGuiKey_Enter, true);
			imgui_io.AddInputCharactersUTF8(ev.data.keyboard.cNewInput);
			break;
		}
	}

	ImGui::NewFrame();

	s_overlay->SetOverlayAlpha(s_main_handle, ImGui::GetStyle().Alpha);

	if (imgui_io.WantTextInput && !keyboard_closed)
	{
		s_overlay->ShowKeyboardForOverlay(s_main_handle, vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine, vr::KeyboardFlag_Minimal, nullptr, 0, "", 0);
		// Avoid keyboard showing up behind dashboard overlay
		s_overlay->SetKeyboardPositionForOverlay(s_main_handle, vr::HmdRect2_t { { 0.0f, 1.0f }, { 1.0f, 0.0f } });
	}

#if RESHADE_LOCALIZATION
	const std::string prev_language = resources::set_current_language(_selected_language);
	_current_language = resources::get_current_language();
#endif

	const std::pair<std::string, void(runtime::*)()> overlay_callbacks[] = {
#if RESHADE_FX
		{ _("Home###home"), &runtime::draw_gui_home },
#endif
#if RESHADE_ADDON
		{ _("Add-ons###addons"), &runtime::draw_gui_addons },
#endif
		{ _("Settings###settings"), &runtime::draw_gui_settings },
		{ _("Statistics###statistics"), &runtime::draw_gui_statistics },
		{ _("Log###log"), &runtime::draw_gui_log },
		{ _("About###about"), &runtime::draw_gui_about }
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

	for (const std::pair<std::string, void(runtime:: *)()> &widget : overlay_callbacks)
	{
		if (bool state = (overlay_index == selected_overlay_index);
			imgui::toggle_button(widget.first.c_str(), state, 0.0f, ImGuiButtonFlags_AlignTextBaseLine))
			selected_overlay_index = overlay_index;
		ImGui::SameLine();

		++overlay_index;
	}

#if RESHADE_ADDON == 1
	if (addon_enabled)
#endif
#if RESHADE_ADDON
	{
		for (const addon_info &info : addon_loaded_info)
		{
			for (const addon_info::overlay_callback &widget : info.overlay_callbacks)
			{
				if (bool state = (overlay_index == selected_overlay_index);
					imgui::toggle_button(widget.title.c_str(), state, 0.0f, ImGuiButtonFlags_AlignTextBaseLine))
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
#if RESHADE_ADDON == 1
		assert(addon_enabled);
#endif

		overlay_index = static_cast<int>(std::size(overlay_callbacks));

		for (const addon_info &info : addon_loaded_info)
		{
			for (const addon_info::overlay_callback &widget : info.overlay_callbacks)
			{
				if (selected_overlay_index == overlay_index++)
				{
					widget.callback(this);
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

#if RESHADE_LOCALIZATION
	resources::set_current_language(prev_language);
#endif

	ImGui::Render();

	if (ImDrawData *const draw_data = ImGui::GetDrawData();
		draw_data != nullptr && draw_data->CmdListsCount != 0 && draw_data->TotalVtxCount != 0)
	{
		api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();

		cmd_list->barrier(_vr_overlay_tex, api::resource_usage::copy_source, api::resource_usage::render_target);
		render_imgui_draw_data(cmd_list, draw_data, _vr_overlay_target);
		cmd_list->barrier(_vr_overlay_tex, api::resource_usage::render_target, api::resource_usage::copy_source);
	}

	ImGui::SetCurrentContext(backup_context);

	vr::Texture_t texture;
	texture.eColorSpace = vr::ColorSpace_Auto;
	union
	{
		vr::D3D12TextureData_t d3d12;
		vr::VRVulkanTextureData_t vulkan;
	} texture_data;

	switch (_device->get_api())
	{
	case api::device_api::d3d10:
	case api::device_api::d3d11:
		texture.handle = reinterpret_cast<void *>(_vr_overlay_tex.handle);
		texture.eType = vr::TextureType_DirectX;
		break;
	case api::device_api::d3d12:
		texture_data.d3d12.m_pResource = reinterpret_cast<ID3D12Resource *>(_vr_overlay_tex.handle);
		texture_data.d3d12.m_pCommandQueue = reinterpret_cast<ID3D12CommandQueue *>(_graphics_queue->get_native());
		texture_data.d3d12.m_nNodeMask = 0;
		texture.handle = &texture_data.d3d12;
		texture.eType = vr::TextureType_DirectX12;
		break;
	case api::device_api::opengl:
		texture.handle = reinterpret_cast<void *>(_vr_overlay_tex.handle & 0xFFFFFFFF);
		texture.eType = vr::TextureType_OpenGL;
		break;
	case api::device_api::vulkan:
		texture_data.vulkan.m_nImage = _vr_overlay_tex.handle;
		texture_data.vulkan.m_pDevice = reinterpret_cast<VkDevice_T *>(_device->get_native());
		texture_data.vulkan.m_pPhysicalDevice = static_cast<vulkan::device_impl *>(_device)->_physical_device;
		texture_data.vulkan.m_pInstance = VK_NULL_HANDLE;
		texture_data.vulkan.m_pQueue = reinterpret_cast<VkQueue_T *>(_graphics_queue->get_native());
		texture_data.vulkan.m_nQueueFamilyIndex = static_cast<vulkan::device_impl *>(_device)->_graphics_queue_family_index;
		texture_data.vulkan.m_nWidth = OVERLAY_WIDTH;
		texture_data.vulkan.m_nHeight = OVERLAY_HEIGHT;
		texture_data.vulkan.m_nFormat = VK_FORMAT_R8G8B8A8_UNORM;
		texture_data.vulkan.m_nSampleCount = 1;
		texture.handle = &texture_data.vulkan;
		texture.eType = vr::TextureType_Vulkan;
		break;
	}

	s_overlay->SetOverlayTexture(s_main_handle, &texture);
}

#endif
