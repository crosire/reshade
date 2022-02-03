/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_GUI

#include "version.h"
#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "ini_file.hpp"
#include "addon_manager.hpp"
#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "input.hpp"
#include "imgui_widgets.hpp"
#include "process_utils.hpp"
#include "fonts/forkawesome.inl"
#include <fstream>
#include <algorithm>
#include <Windows.h> // ClipCursor

static bool filter_text(const std::string_view &text, const std::string_view &filter)
{
	return filter.empty() ||
		std::search(text.begin(), text.end(), filter.begin(), filter.end(),
			[](const char c1, const char c2) { // Search case-insensitive
				return (('a' <= c1 && c1 <= 'z') ? static_cast<char>(c1 - ' ') : c1) == (('a' <= c2 && c2 <= 'z') ? static_cast<char>(c2 - ' ') : c2);
			}) != text.end();
}

static const ImVec4 COLOR_RED = ImColor(240, 100, 100);
static const ImVec4 COLOR_YELLOW = ImColor(204, 204, 0);

void reshade::runtime::init_gui()
{
	// Default shortcut: Home
	_overlay_key_data[0] = 0x24;
	_overlay_key_data[1] = false;
	_overlay_key_data[2] = false;
	_overlay_key_data[3] = false;

	_imgui_context = ImGui::CreateContext();
	auto &imgui_io = _imgui_context->IO;
	auto &imgui_style = _imgui_context->Style;
	imgui_io.IniFilename = nullptr;
	imgui_io.KeyMap[ImGuiKey_Tab] = 0x09; // VK_TAB
	imgui_io.KeyMap[ImGuiKey_LeftArrow] = 0x25; // VK_LEFT
	imgui_io.KeyMap[ImGuiKey_RightArrow] = 0x27; // VK_RIGHT
	imgui_io.KeyMap[ImGuiKey_UpArrow] = 0x26; // VK_UP
	imgui_io.KeyMap[ImGuiKey_DownArrow] = 0x28; // VK_DOWN
	imgui_io.KeyMap[ImGuiKey_PageUp] = 0x21; // VK_PRIOR
	imgui_io.KeyMap[ImGuiKey_PageDown] = 0x22; // VK_NEXT
	imgui_io.KeyMap[ImGuiKey_Home] = 0x24; // VK_HOME
	imgui_io.KeyMap[ImGuiKey_End] = 0x23; // VK_END
	imgui_io.KeyMap[ImGuiKey_Insert] = 0x2D; // VK_INSERT
	imgui_io.KeyMap[ImGuiKey_Delete] = 0x2E; // VK_DELETE
	imgui_io.KeyMap[ImGuiKey_Backspace] = 0x08; // VK_BACK
	imgui_io.KeyMap[ImGuiKey_Space] = 0x20; // VK_SPACE
	imgui_io.KeyMap[ImGuiKey_Enter] = 0x0D; // VK_RETURN
	imgui_io.KeyMap[ImGuiKey_Escape] = 0x1B; // VK_ESCAPE
	imgui_io.KeyMap[ImGuiKey_A] = 'A';
	imgui_io.KeyMap[ImGuiKey_C] = 'C';
	imgui_io.KeyMap[ImGuiKey_V] = 'V';
	imgui_io.KeyMap[ImGuiKey_X] = 'X';
	imgui_io.KeyMap[ImGuiKey_Y] = 'Y';
	imgui_io.KeyMap[ImGuiKey_Z] = 'Z';
	imgui_io.ConfigFlags = ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
	imgui_io.BackendFlags = ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_RendererHasVtxOffset;

	// Disable rounding by default
	imgui_style.GrabRounding = 0.0f;
	imgui_style.FrameRounding = 0.0f;
	imgui_style.ChildRounding = 0.0f;
	imgui_style.ScrollbarRounding = 0.0f;
	imgui_style.WindowRounding = 0.0f;
	imgui_style.WindowBorderSize = 0.0f;

	ImGui::SetCurrentContext(nullptr);
}
void reshade::runtime::deinit_gui()
{
	ImGui::DestroyContext(_imgui_context);
}

void reshade::runtime::build_font_atlas()
{
	ImFontAtlas *const atlas = _imgui_context->IO.Fonts;
	// Remove any existing fonts from atlas first
	atlas->Clear();

	for (int i = 0; i < 2; ++i)
	{
		ImFontConfig cfg;
		cfg.SizePixels = static_cast<float>(i == 0 ? _font_size : _editor_font_size);

		const std::filesystem::path &font_path = i == 0 ? _font : _editor_font;
		if (std::error_code ec; !std::filesystem::is_regular_file(font_path, ec) || !atlas->AddFontFromFileTTF(font_path.u8string().c_str(), cfg.SizePixels))
			atlas->AddFontDefault(&cfg); // Use default font if custom font failed to load or does not exist

		if (i == 0)
		{
			// Merge icons into main font
			ImFontConfig icon_config;
			icon_config.MergeMode = true;
			icon_config.PixelSnapH = true;
			icon_config.GlyphOffset = ImVec2(0.0f, 0.1f * _font_size);
			constexpr ImWchar icon_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 }; // Zero-terminated list
			atlas->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_FK, cfg.SizePixels, &icon_config, icon_ranges);
		}
	}

	// If unable to build font atlas due to an invalid font, revert to the default font
	if (!atlas->Build())
	{
		_font.clear();
		_editor_font.clear();

		atlas->Clear();

		for (int i = 0; i < 2; ++i)
		{
			ImFontConfig cfg;
			cfg.SizePixels = static_cast<float>(i == 0 ? _font_size : _editor_font_size);

			atlas->AddFontDefault(&cfg);
		}
	}

	_show_splash = true;
	_rebuild_font_atlas = false;

	int width, height;
	unsigned char *pixels;
	atlas->GetTexDataAsRGBA32(&pixels, &width, &height);

	// Make sure font atlas is not currently in use before destroying it
	_graphics_queue->wait_idle();

	_device->destroy_resource(_font_atlas_tex);
	_font_atlas_tex = {};
	_device->destroy_resource_view(_font_atlas_srv);
	_font_atlas_srv = {};

	const api::subresource_data initial_data = { pixels, static_cast<uint32_t>(width * 4), static_cast<uint32_t>(width * height * 4) };

	// Create font atlas texture and upload it
	if (!_device->create_resource(
			api::resource_desc(width, height, 1, 1, api::format::r8g8b8a8_unorm, 1, api::memory_heap::gpu_only, api::resource_usage::shader_resource),
			&initial_data, api::resource_usage::shader_resource, &_font_atlas_tex))
	{
		LOG(ERROR) << "Failed to create front atlas resource!";
		return;
	}
	if (!_device->create_resource_view(_font_atlas_tex, api::resource_usage::shader_resource, api::resource_view_desc(api::format::r8g8b8a8_unorm), &_font_atlas_srv))
	{
		LOG(ERROR) << "Failed to create font atlas resource view!";
		return;
	}

	_device->set_resource_name(_font_atlas_tex, "ImGui Font Atlas");
}

void reshade::runtime::load_config_gui(const ini_file &config)
{
	config.get("INPUT", "KeyOverlay", _overlay_key_data);
	config.get("INPUT", "InputProcessing", _input_processing_mode);

	config.get("OVERLAY", "ClockFormat", _clock_format);
	config.get("OVERLAY", "FPSPosition", _fps_pos);
	config.get("OVERLAY", "NoFontScaling", _no_font_scaling);
	config.get("OVERLAY", "SaveWindowState", _save_imgui_window_state);
	config.get("OVERLAY", "ShowClock", _show_clock);
#if RESHADE_FX
	config.get("OVERLAY", "ShowForceLoadEffectsButton", _show_force_load_effects_button);
#endif
	config.get("OVERLAY", "ShowFPS", _show_fps);
	config.get("OVERLAY", "ShowFrameTime", _show_frametime);
	config.get("OVERLAY", "ShowScreenshotMessage", _show_screenshot_message);
#if RESHADE_FX
	config.get("OVERLAY", "TutorialProgress", _tutorial_index);
	config.get("OVERLAY", "VariableListHeight", _variable_editor_height);
	config.get("OVERLAY", "VariableListUseTabs", _variable_editor_tabs);
#endif

	auto &imgui_style = _imgui_context->Style;
	config.get("STYLE", "Alpha", imgui_style.Alpha);
	config.get("STYLE", "ChildRounding", imgui_style.ChildRounding);
	config.get("STYLE", "ColFPSText", _fps_col);
	config.get("STYLE", "EditorFont", _editor_font);
	config.get("STYLE", "EditorFontSize", _editor_font_size);
	config.get("STYLE", "EditorStyleIndex", _editor_style_index);
	config.get("STYLE", "Font", _font);
	config.get("STYLE", "FontSize", _font_size);
	config.get("STYLE", "FPSScale", _fps_scale);
	config.get("STYLE", "FrameRounding", imgui_style.FrameRounding);
	config.get("STYLE", "GrabRounding", imgui_style.GrabRounding);
	config.get("STYLE", "PopupRounding", imgui_style.PopupRounding);
	config.get("STYLE", "ScrollbarRounding", imgui_style.ScrollbarRounding);
	config.get("STYLE", "StyleIndex", _style_index);
	config.get("STYLE", "TabRounding", imgui_style.TabRounding);
	config.get("STYLE", "WindowRounding", imgui_style.WindowRounding);

	// For compatibility with older versions, set the alpha value if it is missing
	if (_fps_col[3] == 0.0f)
		_fps_col[3]  = 1.0f;

	load_custom_style();

	if (_save_imgui_window_state && !_imgui_context->SettingsLoaded)
	{
		ImGuiContext *const backup_context = ImGui::GetCurrentContext();
		ImGui::SetCurrentContext(_imgui_context);
		ImGui::LoadIniSettingsFromDisk((_config_path.parent_path() / L"ReShadeGUI.ini").u8string().c_str());
		ImGui::SetCurrentContext(backup_context);
	}
}
void reshade::runtime::save_config_gui(ini_file &config) const
{
	config.set("INPUT", "KeyOverlay", _overlay_key_data);
	config.set("INPUT", "InputProcessing", _input_processing_mode);

	config.set("OVERLAY", "ClockFormat", _clock_format);
	config.set("OVERLAY", "FPSPosition", _fps_pos);
	config.set("OVERLAY", "NoFontScaling", _no_font_scaling);
	config.set("OVERLAY", "SaveWindowState", _save_imgui_window_state);
	config.set("OVERLAY", "ShowClock", _show_clock);
#if RESHADE_FX
	config.set("OVERLAY", "ShowForceLoadEffectsButton", _show_force_load_effects_button);
#endif
	config.set("OVERLAY", "ShowFPS", _show_fps);
	config.set("OVERLAY", "ShowFrameTime", _show_frametime);
	config.set("OVERLAY", "ShowScreenshotMessage", _show_screenshot_message);
#if RESHADE_FX
	config.set("OVERLAY", "TutorialProgress", _tutorial_index);
	config.set("OVERLAY", "VariableListHeight", _variable_editor_height);
	config.set("OVERLAY", "VariableListUseTabs", _variable_editor_tabs);
#endif

	const auto &imgui_style = _imgui_context->Style;
	config.set("STYLE", "Alpha", imgui_style.Alpha);
	config.set("STYLE", "ChildRounding", imgui_style.ChildRounding);
	config.set("STYLE", "ColFPSText", _fps_col);
	config.set("STYLE", "EditorFont", _editor_font);
	config.set("STYLE", "EditorFontSize", _editor_font_size);
	config.set("STYLE", "EditorStyleIndex", _editor_style_index);
	config.set("STYLE", "Font", _font);
	config.set("STYLE", "FontSize", _font_size);
	config.set("STYLE", "FPSScale", _fps_scale);
	config.set("STYLE", "FrameRounding", imgui_style.FrameRounding);
	config.set("STYLE", "GrabRounding", imgui_style.GrabRounding);
	config.set("STYLE", "PopupRounding", imgui_style.PopupRounding);
	config.set("STYLE", "ScrollbarRounding", imgui_style.ScrollbarRounding);
	config.set("STYLE", "StyleIndex", _style_index);
	config.set("STYLE", "TabRounding", imgui_style.TabRounding);
	config.set("STYLE", "WindowRounding", imgui_style.WindowRounding);

	// Do not save custom style colors by default, only when actually used and edited

	if (_save_imgui_window_state)
	{
		ImGuiContext *const backup_context = ImGui::GetCurrentContext();
		ImGui::SetCurrentContext(_imgui_context);
		ImGui::SaveIniSettingsToDisk((_config_path.parent_path() / L"ReShadeGUI.ini").u8string().c_str());
		ImGui::SetCurrentContext(backup_context);
	}
}

void reshade::runtime::load_custom_style()
{
	const ini_file &config = ini_file::load_cache(_config_path);

	ImVec4 *const colors = _imgui_context->Style.Colors;
	switch (_style_index)
	{
	case 0:
		ImGui::StyleColorsDark(&_imgui_context->Style);
		break;
	case 1:
		ImGui::StyleColorsLight(&_imgui_context->Style);
		break;
	case 2:
		colors[ImGuiCol_Text] = ImVec4(0.862745f, 0.862745f, 0.862745f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.58f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.117647f, 0.117647f, 0.117647f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 0.00f);
		colors[ImGuiCol_Border] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.30f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.470588f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.588235f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.45f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.35f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.58f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 0.57f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 1.00f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.31f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.78f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.117647f, 0.117647f, 0.117647f, 0.92f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.80f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.784314f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.44f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.86f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.76f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.86f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_Separator] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.32f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.78f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.862745f, 0.862745f, 0.862745f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.20f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.78f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_Tab] = colors[ImGuiCol_Button];
		colors[ImGuiCol_TabActive] = colors[ImGuiCol_ButtonActive];
		colors[ImGuiCol_TabHovered] = colors[ImGuiCol_ButtonHovered];
		colors[ImGuiCol_TabUnfocused] = ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
		colors[ImGuiCol_TabUnfocusedActive] = ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);
		colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_Header] * ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
		colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.63f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.63f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.43f);
		break;
	case 5:
		colors[ImGuiCol_Text] = ImColor(0xff969483);
		colors[ImGuiCol_TextDisabled] = ImColor(0xff756e58);
		colors[ImGuiCol_WindowBg] = ImColor(0xff362b00);
		colors[ImGuiCol_ChildBg] = ImColor();
		colors[ImGuiCol_PopupBg] = ImColor(0xfc362b00); // Customized
		colors[ImGuiCol_Border] = ImColor(0xff423607);
		colors[ImGuiCol_BorderShadow] = ImColor();
		colors[ImGuiCol_FrameBg] = ImColor(0xfc423607); // Customized
		colors[ImGuiCol_FrameBgHovered] = ImColor(0xff423607);
		colors[ImGuiCol_FrameBgActive] = ImColor(0xff423607);
		colors[ImGuiCol_TitleBg] = ImColor(0xff362b00);
		colors[ImGuiCol_TitleBgActive] = ImColor(0xff362b00);
		colors[ImGuiCol_TitleBgCollapsed] = ImColor(0xff362b00);
		colors[ImGuiCol_MenuBarBg] = ImColor(0xff423607);
		colors[ImGuiCol_ScrollbarBg] = ImColor(0xff362b00);
		colors[ImGuiCol_ScrollbarGrab] = ImColor(0xff423607);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(0xff423607);
		colors[ImGuiCol_ScrollbarGrabActive] = ImColor(0xff423607);
		colors[ImGuiCol_CheckMark] = ImColor(0xff756e58);
		colors[ImGuiCol_SliderGrab] = ImColor(0xff5e5025); // Customized
		colors[ImGuiCol_SliderGrabActive] = ImColor(0xff5e5025); // Customized
		colors[ImGuiCol_Button] = ImColor(0xff423607);
		colors[ImGuiCol_ButtonHovered] = ImColor(0xff423607);
		colors[ImGuiCol_ButtonActive] = ImColor(0xff362b00);
		colors[ImGuiCol_Header] = ImColor(0xff423607);
		colors[ImGuiCol_HeaderHovered] = ImColor(0xff423607);
		colors[ImGuiCol_HeaderActive] = ImColor(0xff423607);
		colors[ImGuiCol_Separator] = ImColor(0xff423607);
		colors[ImGuiCol_SeparatorHovered] = ImColor(0xff423607);
		colors[ImGuiCol_SeparatorActive] = ImColor(0xff423607);
		colors[ImGuiCol_ResizeGrip] = ImColor(0xff423607);
		colors[ImGuiCol_ResizeGripHovered] = ImColor(0xff423607);
		colors[ImGuiCol_ResizeGripActive] = ImColor(0xff756e58);
		colors[ImGuiCol_Tab] = ImColor(0xff362b00);
		colors[ImGuiCol_TabHovered] = ImColor(0xff423607);
		colors[ImGuiCol_TabActive] = ImColor(0xff423607);
		colors[ImGuiCol_TabUnfocused] = ImColor(0xff362b00);
		colors[ImGuiCol_TabUnfocusedActive] = ImColor(0xff423607);
		colors[ImGuiCol_DockingPreview] = ImColor(0xee837b65); // Customized
		colors[ImGuiCol_DockingEmptyBg] = ImColor();
		colors[ImGuiCol_PlotLines] = ImColor(0xff756e58);
		colors[ImGuiCol_PlotLinesHovered] = ImColor(0xff756e58);
		colors[ImGuiCol_PlotHistogram] = ImColor(0xff756e58);
		colors[ImGuiCol_PlotHistogramHovered] = ImColor(0xff756e58);
		colors[ImGuiCol_TextSelectedBg] = ImColor(0xff756e58);
		colors[ImGuiCol_DragDropTarget] = ImColor(0xff756e58);
		colors[ImGuiCol_NavHighlight] = ImColor();
		colors[ImGuiCol_NavWindowingHighlight] = ImColor(0xee969483); // Customized
		colors[ImGuiCol_NavWindowingDimBg] = ImColor(0x20e3f6fd); // Customized
		colors[ImGuiCol_ModalWindowDimBg] = ImColor(0x20e3f6fd); // Customized
		break;
	case 6:
		colors[ImGuiCol_Text] = ImColor(0xff837b65);
		colors[ImGuiCol_TextDisabled] = ImColor(0xffa1a193);
		colors[ImGuiCol_WindowBg] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_ChildBg] = ImColor();
		colors[ImGuiCol_PopupBg] = ImColor(0xfce3f6fd); // Customized
		colors[ImGuiCol_Border] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_BorderShadow] = ImColor();
		colors[ImGuiCol_FrameBg] = ImColor(0xfcd5e8ee); // Customized
		colors[ImGuiCol_FrameBgHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_FrameBgActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_TitleBg] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_TitleBgActive] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_TitleBgCollapsed] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_MenuBarBg] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ScrollbarBg] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_ScrollbarGrab] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ScrollbarGrabActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_CheckMark] = ImColor(0xffa1a193);
		colors[ImGuiCol_SliderGrab] = ImColor(0xffc3d3d9); // Customized
		colors[ImGuiCol_SliderGrabActive] = ImColor(0xffc3d3d9); // Customized
		colors[ImGuiCol_Button] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ButtonHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ButtonActive] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_Header] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_HeaderHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_HeaderActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_Separator] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_SeparatorHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_SeparatorActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ResizeGrip] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ResizeGripHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_ResizeGripActive] = ImColor(0xffa1a193);
		colors[ImGuiCol_Tab] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_TabHovered] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_TabActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_TabUnfocused] = ImColor(0xffe3f6fd);
		colors[ImGuiCol_TabUnfocusedActive] = ImColor(0xffd5e8ee);
		colors[ImGuiCol_DockingPreview] = ImColor(0xeea1a193); // Customized
		colors[ImGuiCol_DockingEmptyBg] = ImColor();
		colors[ImGuiCol_PlotLines] = ImColor(0xffa1a193);
		colors[ImGuiCol_PlotLinesHovered] = ImColor(0xffa1a193);
		colors[ImGuiCol_PlotHistogram] = ImColor(0xffa1a193);
		colors[ImGuiCol_PlotHistogramHovered] = ImColor(0xffa1a193);
		colors[ImGuiCol_TextSelectedBg] = ImColor(0xffa1a193);
		colors[ImGuiCol_DragDropTarget] = ImColor(0xffa1a193);
		colors[ImGuiCol_NavHighlight] = ImColor();
		colors[ImGuiCol_NavWindowingHighlight] = ImColor(0xee837b65); // Customized
		colors[ImGuiCol_NavWindowingDimBg] = ImColor(0x20362b00); // Customized
		colors[ImGuiCol_ModalWindowDimBg] = ImColor(0x20362b00); // Customized
		break;
	default:
		for (ImGuiCol i = 0; i < ImGuiCol_COUNT; i++)
			config.get("STYLE", ImGui::GetStyleColorName(i), (float(&)[4])colors[i]);
		break;
	}

	switch (_editor_style_index)
	{
	case 0: // Dark
		_editor_palette[imgui::code_editor::color_default] = 0xffffffff;
		_editor_palette[imgui::code_editor::color_keyword] = 0xffd69c56;
		_editor_palette[imgui::code_editor::color_number_literal] = 0xff00ff00;
		_editor_palette[imgui::code_editor::color_string_literal] = 0xff7070e0;
		_editor_palette[imgui::code_editor::color_punctuation] = 0xffffffff;
		_editor_palette[imgui::code_editor::color_preprocessor] = 0xff409090;
		_editor_palette[imgui::code_editor::color_identifier] = 0xffaaaaaa;
		_editor_palette[imgui::code_editor::color_known_identifier] = 0xff9bc64d;
		_editor_palette[imgui::code_editor::color_preprocessor_identifier] = 0xffc040a0;
		_editor_palette[imgui::code_editor::color_comment] = 0xff206020;
		_editor_palette[imgui::code_editor::color_multiline_comment] = 0xff406020;
		_editor_palette[imgui::code_editor::color_background] = 0xff101010;
		_editor_palette[imgui::code_editor::color_cursor] = 0xffe0e0e0;
		_editor_palette[imgui::code_editor::color_selection] = 0x80a06020;
		_editor_palette[imgui::code_editor::color_error_marker] = 0x800020ff;
		_editor_palette[imgui::code_editor::color_warning_marker] = 0x8000ffff;
		_editor_palette[imgui::code_editor::color_line_number] = 0xff707000;
		_editor_palette[imgui::code_editor::color_current_line_fill] = 0x40000000;
		_editor_palette[imgui::code_editor::color_current_line_fill_inactive] = 0x40808080;
		_editor_palette[imgui::code_editor::color_current_line_edge] = 0x40a0a0a0;
		break;
	case 1: // Light
		_editor_palette[imgui::code_editor::color_default] = 0xff000000;
		_editor_palette[imgui::code_editor::color_keyword] = 0xffff0c06;
		_editor_palette[imgui::code_editor::color_number_literal] = 0xff008000;
		_editor_palette[imgui::code_editor::color_string_literal] = 0xff2020a0;
		_editor_palette[imgui::code_editor::color_punctuation] = 0xff000000;
		_editor_palette[imgui::code_editor::color_preprocessor] = 0xff409090;
		_editor_palette[imgui::code_editor::color_identifier] = 0xff404040;
		_editor_palette[imgui::code_editor::color_known_identifier] = 0xff606010;
		_editor_palette[imgui::code_editor::color_preprocessor_identifier] = 0xffc040a0;
		_editor_palette[imgui::code_editor::color_comment] = 0xff205020;
		_editor_palette[imgui::code_editor::color_multiline_comment] = 0xff405020;
		_editor_palette[imgui::code_editor::color_background] = 0xffffffff;
		_editor_palette[imgui::code_editor::color_cursor] = 0xff000000;
		_editor_palette[imgui::code_editor::color_selection] = 0x80600000;
		_editor_palette[imgui::code_editor::color_error_marker] = 0xa00010ff;
		_editor_palette[imgui::code_editor::color_warning_marker] = 0x8000ffff;
		_editor_palette[imgui::code_editor::color_line_number] = 0xff505000;
		_editor_palette[imgui::code_editor::color_current_line_fill] = 0x40000000;
		_editor_palette[imgui::code_editor::color_current_line_fill_inactive] = 0x40808080;
		_editor_palette[imgui::code_editor::color_current_line_edge] = 0x40000000;
		break;
	case 3: // Solarized Dark
		_editor_palette[imgui::code_editor::color_default] = 0xff969483;
		_editor_palette[imgui::code_editor::color_keyword] = 0xff0089b5;
		_editor_palette[imgui::code_editor::color_number_literal] = 0xff98a12a;
		_editor_palette[imgui::code_editor::color_string_literal] = 0xff98a12a;
		_editor_palette[imgui::code_editor::color_punctuation] = 0xff969483;
		_editor_palette[imgui::code_editor::color_preprocessor] = 0xff164bcb;
		_editor_palette[imgui::code_editor::color_identifier] = 0xff969483;
		_editor_palette[imgui::code_editor::color_known_identifier] = 0xff969483;
		_editor_palette[imgui::code_editor::color_preprocessor_identifier] = 0xffc4716c;
		_editor_palette[imgui::code_editor::color_comment] = 0xff756e58;
		_editor_palette[imgui::code_editor::color_multiline_comment] = 0xff756e58;
		_editor_palette[imgui::code_editor::color_background] = 0xff362b00;
		_editor_palette[imgui::code_editor::color_cursor] = 0xff969483;
		_editor_palette[imgui::code_editor::color_selection] = 0xa0756e58;
		_editor_palette[imgui::code_editor::color_error_marker] = 0x7f2f32dc;
		_editor_palette[imgui::code_editor::color_warning_marker] = 0x7f0089b5;
		_editor_palette[imgui::code_editor::color_line_number] = 0xff756e58;
		_editor_palette[imgui::code_editor::color_current_line_fill] = 0x7f423607;
		_editor_palette[imgui::code_editor::color_current_line_fill_inactive] = 0x7f423607;
		_editor_palette[imgui::code_editor::color_current_line_edge] = 0x7f423607;
		break;
	case 4: // Solarized Light
		_editor_palette[imgui::code_editor::color_default] = 0xff837b65;
		_editor_palette[imgui::code_editor::color_keyword] = 0xff0089b5;
		_editor_palette[imgui::code_editor::color_number_literal] = 0xff98a12a;
		_editor_palette[imgui::code_editor::color_string_literal] = 0xff98a12a;
		_editor_palette[imgui::code_editor::color_punctuation] = 0xff756e58;
		_editor_palette[imgui::code_editor::color_preprocessor] = 0xff164bcb;
		_editor_palette[imgui::code_editor::color_identifier] = 0xff837b65;
		_editor_palette[imgui::code_editor::color_known_identifier] = 0xff837b65;
		_editor_palette[imgui::code_editor::color_preprocessor_identifier] = 0xffc4716c;
		_editor_palette[imgui::code_editor::color_comment] = 0xffa1a193;
		_editor_palette[imgui::code_editor::color_multiline_comment] = 0xffa1a193;
		_editor_palette[imgui::code_editor::color_background] = 0xffe3f6fd;
		_editor_palette[imgui::code_editor::color_cursor] = 0xff837b65;
		_editor_palette[imgui::code_editor::color_selection] = 0x60a1a193;
		_editor_palette[imgui::code_editor::color_error_marker] = 0x7f2f32dc;
		_editor_palette[imgui::code_editor::color_warning_marker] = 0x7f0089b5;
		_editor_palette[imgui::code_editor::color_line_number] = 0xffa1a193;
		_editor_palette[imgui::code_editor::color_current_line_fill] = 0x7fd5e8ee;
		_editor_palette[imgui::code_editor::color_current_line_fill_inactive] = 0x7fd5e8ee;
		_editor_palette[imgui::code_editor::color_current_line_edge] = 0x7fd5e8ee;
		break;
	case 2:
	default:
		ImVec4 value;
		for (ImGuiCol i = 0; i < imgui::code_editor::color_palette_max; i++)
			value = ImGui::ColorConvertU32ToFloat4(_editor_palette[i]), // Get default value first
			config.get("STYLE",  imgui::code_editor::get_palette_color_name(i), (float(&)[4])value),
			_editor_palette[i] = ImGui::ColorConvertFloat4ToU32(value);
		break;
	}
}
void reshade::runtime::save_custom_style() const
{
	ini_file &config = ini_file::load_cache(_config_path);

	if (_style_index == 3 || _style_index == 4) // Custom Simple, Custom Advanced
	{
		for (ImGuiCol i = 0; i < ImGuiCol_COUNT; i++)
			config.set("STYLE", ImGui::GetStyleColorName(i), (const float(&)[4])_imgui_context->Style.Colors[i]);
	}

	if (_editor_style_index == 2) // Custom
	{
		ImVec4 value;
		for (ImGuiCol i = 0; i < imgui::code_editor::color_palette_max; i++)
			value = ImGui::ColorConvertU32ToFloat4(_editor_palette[i]),
			config.set("STYLE",  imgui::code_editor::get_palette_color_name(i), (const float(&)[4])value);
	}
}

void reshade::runtime::draw_gui()
{
	assert(_is_initialized);

#if RESHADE_FX
	bool show_splash = _show_splash && (is_loading() || (_reload_count <= 1 && (_last_present_time - _last_reload_time) < std::chrono::seconds(5)) || (!_show_overlay && _tutorial_index == 0));
#else
	bool show_splash = _show_splash && (_last_present_time - _last_reload_time) < std::chrono::seconds(5);
#endif

	const bool show_stats_window = _show_clock || _show_fps || _show_frametime;
	// Do not show this message in the same frame the screenshot is taken (so that it won't show up on the GUI screenshot)
	const bool show_screenshot_message = (_show_screenshot_message || !_last_screenshot_save_successfull) && !_should_save_screenshot && (_last_present_time - _last_screenshot_time) < std::chrono::seconds(_last_screenshot_save_successfull ? 3 : 5);
	if (show_screenshot_message || !_preset_save_successfull)
		show_splash = true;

	if (_show_overlay && !_ignore_shortcuts && !_imgui_context->IO.NavVisible && _input->is_key_pressed(0x1B /* VK_ESCAPE */))
		_show_overlay = false; // Close when pressing the escape button and not currently navigating with the keyboard
	else if (!_ignore_shortcuts && _input->is_key_pressed(_overlay_key_data, _force_shortcut_modifiers) && _imgui_context->ActiveId == 0)
		_show_overlay = !_show_overlay;

	_ignore_shortcuts = false;
	_gather_gpu_statistics = false;
#if RESHADE_FX
	_effects_expanded_state &= 2;
#endif

	if (!show_splash && !show_stats_window && !_show_overlay && _preview_texture == 0)
	{
		_input->block_mouse_input(false);
		_input->block_keyboard_input(false);
		return; // Early-out to avoid costly ImGui calls when no GUI elements are on the screen
	}

	if (_rebuild_font_atlas)
		build_font_atlas();
	if (_font_atlas_srv == 0)
		return; // Cannot render GUI without font atlas

	ImGuiContext *const backup_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(_imgui_context);

	auto &imgui_io = _imgui_context->IO;
	imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
	imgui_io.MouseDrawCursor = _show_overlay && (!_should_save_screenshot || !_screenshot_save_gui);
	imgui_io.MousePos.x = static_cast<float>(_input->mouse_position_x());
	imgui_io.MousePos.y = static_cast<float>(_input->mouse_position_y());
	imgui_io.DisplaySize.x = static_cast<float>(_width);
	imgui_io.DisplaySize.y = static_cast<float>(_height);
	imgui_io.Fonts->TexID = _font_atlas_srv.handle;

	// Add wheel delta to the current absolute mouse wheel position
	imgui_io.MouseWheel += _input->mouse_wheel_delta();

	// Scale mouse position in case render resolution does not match the window size
	if (_window_width != 0 && _window_height != 0)
	{
		imgui_io.MousePos.x *= imgui_io.DisplaySize.x / _window_width;
		imgui_io.MousePos.y *= imgui_io.DisplaySize.y / _window_height;
	}

	// Update all the button states
	imgui_io.KeyAlt = _input->is_key_down(0x12); // VK_MENU
	imgui_io.KeyCtrl = _input->is_key_down(0x11); // VK_CONTROL
	imgui_io.KeyShift = _input->is_key_down(0x10); // VK_SHIFT
	for (unsigned int i = 0; i < 256; i++)
		imgui_io.KeysDown[i] = _input->is_key_down(i);
	for (unsigned int i = 0; i < 5; i++)
		imgui_io.MouseDown[i] = _input->is_mouse_button_down(i);
	for (wchar_t c : _input->text_input())
		imgui_io.AddInputCharacter(c);

	ImGui::NewFrame();

	ImVec2 viewport_offset = ImVec2(0, 0);

	// Create ImGui widgets and windows
	if (show_splash)
	{
		ImGui::SetNextWindowPos(ImVec2(10, 10));
		ImGui::SetNextWindowSize(ImVec2(imgui_io.DisplaySize.x - 20.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.862745f, 0.862745f, 0.862745f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.117647f, 0.117647f, 0.117647f, 0.7f));
		ImGui::Begin("Splash Screen", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoFocusOnAppearing);

		if (!_preset_save_successfull)
		{
#if RESHADE_FX
			ImGui::TextColored(COLOR_RED, "Unable to save current preset. Make sure you have write permissions to %s.", _current_preset_path.u8string().c_str());
#else
			ImGui::TextColored(COLOR_RED, "Unable to save configuration.");
#endif
		}
		else if (show_screenshot_message)
		{
			if (!_last_screenshot_save_successfull)
				if (_screenshot_directory_creation_successfull)
					ImGui::TextColored(COLOR_RED, "Unable to save screenshot because of an internal error (the format may not be supported).");
				else
					ImGui::TextColored(COLOR_RED, "Unable to save screenshot because path could not be created: %s.", (g_reshade_base_path / _screenshot_path).u8string().c_str());
			else
				ImGui::Text("Screenshot successfully saved to %s", _last_screenshot_file.u8string().c_str());
		}
		else
		{
			ImGui::TextUnformatted("ReShade " VERSION_STRING_PRODUCT);

			if (_needs_update)
			{
				ImGui::TextColored(COLOR_YELLOW,
					"An update is available! Please visit https://reshade.me and install the new version (v%lu.%lu.%lu).",
					_latest_version[0], _latest_version[1], _latest_version[2]);
			}
			else
			{
				ImGui::TextUnformatted("Visit https://reshade.me for news, updates, effects and discussion.");
			}

			ImGui::Spacing();

#if RESHADE_FX
			if (_reload_remaining_effects != 0 && _reload_remaining_effects != std::numeric_limits<size_t>::max())
			{
				ImGui::ProgressBar((_effects.size() - _reload_remaining_effects) / float(_effects.size()), ImVec2(-1, 0), "");
				ImGui::SameLine(15);
				ImGui::Text(
					"Compiling (%zu effects remaining) ... "
					"This might take a while. The application could become unresponsive for some time.",
					_reload_remaining_effects.load());
			}
			else if (_tutorial_index == 0)
			{
				ImGui::ProgressBar(0.0f, ImVec2(-1, 0), "");
				ImGui::SameLine(15);
				ImGui::TextUnformatted("ReShade is now installed successfully! Press '");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", input::key_name(_overlay_key_data).c_str());
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextUnformatted("' to start the tutorial.");
			}
			else
#endif
			{
				ImGui::ProgressBar(0.0f, ImVec2(-1, 0), "");
				ImGui::SameLine(15);
				ImGui::TextUnformatted("Press '");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", input::key_name(_overlay_key_data).c_str());
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextUnformatted("' to open the configuration overlay.");
			}

#if RESHADE_FX
			if (!_last_reload_successfull)
			{
				ImGui::Spacing();
				ImGui::TextColored(COLOR_RED,
					"There were errors compiling some effects. Check the log for more details.");
			}
#endif
		}

		viewport_offset.y += ImGui::GetWindowHeight() + 10; // Add small space between windows

		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}
	else if (show_stats_window)
	{
		ImVec2 fps_window_pos(5, 5);
		ImVec2 fps_window_size(200, 0);

		// Get last calculated window size (because of 'ImGuiWindowFlags_AlwaysAutoResize')
		if (ImGuiWindow *const fps_window = ImGui::FindWindowByName("OSD"))
		{
			fps_window_size  = fps_window->Size;
			fps_window_size.y = std::max(fps_window_size.y, _imgui_context->Style.FramePadding.y * 4.0f + _imgui_context->Style.ItemSpacing.y +
				(_imgui_context->Style.ItemSpacing.y + _imgui_context->FontBaseSize * _fps_scale) * ((_show_clock ? 1 : 0) + (_show_fps ? 1 : 0) + (_show_frametime ? 1 : 0)));
		}

		if (_fps_pos % 2)
			fps_window_pos.x = imgui_io.DisplaySize.x - fps_window_size.x - 5;
		if (_fps_pos > 1)
			fps_window_pos.y = imgui_io.DisplaySize.y - fps_window_size.y - 5;

		ImGui::SetNextWindowPos(fps_window_pos);
		ImGui::PushStyleColor(ImGuiCol_Text, (const ImVec4 &)_fps_col);
		ImGui::Begin("OSD", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::SetWindowFontScale(_fps_scale);

		char temp[512];

		if (_show_clock)
		{
			const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			tm tm; localtime_s(&tm, &t);

			ImFormatString(temp, sizeof(temp), _clock_format != 0 ? "%02u:%02u:%02u" : "%02u:%02u", tm.tm_hour, tm.tm_min, tm.tm_sec);
			if (_fps_pos % 2) // Align text to the right of the window
				ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(temp).x + _imgui_context->Style.ItemSpacing.x);
			ImGui::TextUnformatted(temp);
		}
		if (_show_fps)
		{
			ImFormatString(temp, sizeof(temp), "%.0f fps", imgui_io.Framerate);
			if (_fps_pos % 2)
				ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(temp).x + _imgui_context->Style.ItemSpacing.x);
			ImGui::TextUnformatted(temp);
		}
		if (_show_frametime)
		{
			ImFormatString(temp, sizeof(temp), "%5.2f ms", 1000.0f / imgui_io.Framerate);
			if (_fps_pos % 2)
				ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(temp).x + _imgui_context->Style.ItemSpacing.x);
			ImGui::TextUnformatted(temp);
		}

		ImGui::Dummy(ImVec2(200, 0)); // Force a minimum window width

		ImGui::End();
		ImGui::PopStyleColor();
	}

	if (_show_overlay)
	{
		// Change font size if user presses the control key and moves the mouse wheel
		if (imgui_io.KeyCtrl && imgui_io.MouseWheel != 0 && !_no_font_scaling)
		{
			_font_size = ImClamp(_font_size + static_cast<int>(imgui_io.MouseWheel), 8, 32);
			_editor_font_size = ImClamp(_editor_font_size + static_cast<int>(imgui_io.MouseWheel), 8, 32);
			_rebuild_font_atlas = true;
			save_config();
		}

		static constexpr std::pair<const char *, void(runtime::*)()> overlay_callbacks[] = {
#if RESHADE_FX
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

		const ImGuiID root_space_id = ImGui::GetID("Dockspace");
		const ImGuiViewport *const viewport = ImGui::GetMainViewport();

		// Set up default dock layout if this was not done yet
		const bool init_window_layout = !ImGui::DockBuilderGetNode(root_space_id);
		if (init_window_layout)
		{
			// Add the root node
			ImGui::DockBuilderAddNode(root_space_id, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(root_space_id, viewport->Size);

			// Split root node into two spaces
			ImGuiID main_space_id = 0;
			ImGuiID right_space_id = 0;
			ImGui::DockBuilderSplitNode(root_space_id, ImGuiDir_Left, 0.35f, &main_space_id, &right_space_id);

			// Attach most windows to the main dock space
			for (const auto &widget : overlay_callbacks)
				ImGui::DockBuilderDockWindow(widget.first, main_space_id);

			// Attach editor window to the remaining dock space
			ImGui::DockBuilderDockWindow("###editor", right_space_id);

			// Commit the layout
			ImGui::DockBuilderFinish(root_space_id);
		}

		ImGui::SetNextWindowPos(viewport->Pos + viewport_offset);
		ImGui::SetNextWindowSize(viewport->Size - viewport_offset);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::Begin("Viewport", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoDocking | // This is the background viewport, the docking space is a child of it
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoBackground);
		ImGui::DockSpace(root_space_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::End();

		for (const auto &widget : overlay_callbacks)
		{
			if (ImGui::Begin(widget.first, nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) // No focus so that window state is preserved between opening/closing the GUI
				(this->*widget.second)();
			ImGui::End();
		}

#if RESHADE_FX
		if (!_editors.empty())
		{
			if (ImGui::Begin("Edit###editor", nullptr, ImGuiWindowFlags_NoFocusOnAppearing) &&
				ImGui::BeginTabBar("editor_tabs"))
			{
				for (auto it = _editors.begin(); it != _editors.end();)
				{
					std::string title = it->entry_point_name.empty() ? it->file_path.filename().u8string() : it->entry_point_name;
					title += " ###editor" + std::to_string(std::distance(_editors.begin(), it));

					bool is_open = true;
					ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
					if (it->editor.is_modified())
						flags |= ImGuiTabItemFlags_UnsavedDocument;
					if (it->selected)
						flags |= ImGuiTabItemFlags_SetSelected;

					if (ImGui::BeginTabItem(title.c_str(), &is_open, flags))
					{
						draw_code_editor(*it);
						ImGui::EndTabItem();
					}

					it->selected = false;

					if (!is_open)
						it = _editors.erase(it);
					else
						++it;
				}

				ImGui::EndTabBar();
			}
			ImGui::End();
		}
#endif
	}

#if RESHADE_ADDON
	// Do not show add-on overlays while loading in case they are still referencing any variable or technique handles
	if (!is_loading()
#if RESHADE_ADDON_LITE
		&& addon_enabled
#endif
		)
	{
		for (const addon_info &info : addon_loaded_info)
		{
			for (const auto &widget : info.overlay_callbacks)
			{
				if (widget.first == "OSD" ? show_splash || (!show_stats_window && !_show_overlay) : !_show_overlay)
					continue;

				if (ImGui::Begin(widget.first.c_str(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
					widget.second(this);
				ImGui::End();
			}
		}
	}
#endif

#if RESHADE_FX
	if (_preview_texture != 0 && _effects_enabled)
	{
		if (!_show_overlay)
		{
			// Create a temporary viewport window to attach image to when overlay is not open
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImVec2(imgui_io.DisplaySize.x, imgui_io.DisplaySize.y));
			ImGui::Begin("Viewport", nullptr,
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoNav |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoDocking |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoBackground);
			ImGui::End();
		}

		// The preview texture is unset in 'destroy_effect', so should not be able to reach this while loading
		assert(!is_loading() && _reload_create_queue.empty());

		// Scale image to fill the entire viewport by default
		ImVec2 preview_min = ImVec2(0, 0);
		ImVec2 preview_max = imgui_io.DisplaySize;

		// Positing image in the middle of the viewport when using original size
		if (_preview_size[0])
		{
			preview_min.x = (preview_max.x * 0.5f) - (_preview_size[0] * 0.5f);
			preview_max.x = (preview_max.x * 0.5f) + (_preview_size[0] * 0.5f);
		}
		if (_preview_size[1])
		{
			preview_min.y = (preview_max.y * 0.5f) - (_preview_size[1] * 0.5f);
			preview_max.y = (preview_max.y * 0.5f) + (_preview_size[1] * 0.5f);
		}

		ImGui::FindWindowByName("Viewport")->DrawList->AddImage(_preview_texture.handle, preview_min, preview_max, ImVec2(0, 0), ImVec2(1, 1), _preview_size[2]);
	}
#endif

	// Disable keyboard shortcuts while typing into input boxes
	_ignore_shortcuts |= ImGui::IsAnyItemActive();

	// Render ImGui widgets and windows
	ImGui::Render();

	_input->block_mouse_input(_input_processing_mode != 0 && _show_overlay && (imgui_io.WantCaptureMouse || _input_processing_mode == 2));
	_input->block_keyboard_input(_input_processing_mode != 0 && _show_overlay && (imgui_io.WantCaptureKeyboard || _input_processing_mode == 2));

	if (_input->is_blocking_mouse_input())
	{
		// Some games setup ClipCursor with a tiny area which could make the cursor stay in that area instead of the whole window
		ClipCursor(nullptr);
	}

	if (ImDrawData *const draw_data = ImGui::GetDrawData();
		draw_data != nullptr && draw_data->CmdListsCount != 0 && draw_data->TotalVtxCount != 0)
	{
		api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();

		if (_back_buffer_resolved != 0)
		{
			render_imgui_draw_data(cmd_list, draw_data, _back_buffer_targets[0]);
		}
		else
		{
			uint32_t back_buffer_index = get_current_back_buffer_index();
			const api::resource back_buffer_resource = get_back_buffer(back_buffer_index);

			cmd_list->barrier(back_buffer_resource, api::resource_usage::present, api::resource_usage::render_target);
			render_imgui_draw_data(cmd_list, draw_data, _back_buffer_targets[back_buffer_index * 2]);
			cmd_list->barrier(back_buffer_resource, api::resource_usage::render_target, api::resource_usage::present);
		}
	}

	ImGui::SetCurrentContext(backup_context);
}

#if RESHADE_FX
void reshade::runtime::draw_gui_home()
{
	const char *tutorial_text =
		"Welcome! Since this is the first time you start ReShade, we'll go through a quick tutorial covering the most important features.\n\n"
		"If you have difficulties reading this text, press the 'Ctrl' key and adjust the font size with your mouse wheel. "
		"The window size is variable as well, just grab the right edge and move it around.\n\n"
		"You can also use the keyboard for navigation in case mouse input does not work. Use the arrow keys to navigate, space bar to confirm an action or enter a control and the 'Esc' key to leave a control. "
		"Press 'Ctrl + Tab' to switch between tabs and windows (use this to focus this page in case the other navigation keys do not work at first).\n\n"
		"Click on the 'Continue' button to continue the tutorial.";

	// It is not possible to follow some of the tutorial steps while performance mode is active, so skip them
	if (_performance_mode && _tutorial_index <= 3)
		_tutorial_index = 4;

	if (_tutorial_index > 0)
	{
		if (_tutorial_index == 1)
		{
			tutorial_text =
				"This is the preset selection. All changes will be saved to the selected preset file.\n\n"
				"Click on the '+' button to name and add a new one.\n\n"
				"Make sure you always have a preset selected here before starting to tweak any values later, or else your changes won't be saved!";

			ImGui::PushStyleColor(ImGuiCol_FrameBg, COLOR_RED);
			ImGui::PushStyleColor(ImGuiCol_Button, COLOR_RED);
		}

		const float button_size = ImGui::GetFrameHeight();
		const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;
		const float browse_button_width = ImGui::GetContentRegionAvail().x - (button_spacing + button_size) * 4;

		bool reload_preset = false;

		if (is_loading())
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		}

		if (ImGui::ArrowButtonEx("<", ImGuiDir_Left, ImVec2(button_size, button_size), ImGuiButtonFlags_NoNavFocus))
			if (switch_to_next_preset(_current_preset_path.parent_path(), true))
				reload_preset = true;
		ImGui::SameLine(0, button_spacing);
		if (ImGui::ArrowButtonEx(">", ImGuiDir_Right, ImVec2(button_size, button_size), ImGuiButtonFlags_NoNavFocus))
			if (switch_to_next_preset(_current_preset_path.parent_path(), false))
				reload_preset = true;

		ImGui::SameLine(0, button_spacing);
		const ImVec2 popup_pos = ImGui::GetCursorScreenPos() + ImVec2(-_imgui_context->Style.WindowPadding.x, ImGui::GetFrameHeightWithSpacing());

		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
		if (ImGui::ButtonEx(_current_preset_path.stem().u8string().c_str(), ImVec2(browse_button_width, 0), ImGuiButtonFlags_NoNavFocus))
		{
			_file_selection_path = _current_preset_path;
			ImGui::OpenPopup("##browse");
		}
		ImGui::PopStyleVar();

		ImGui::SameLine(0, button_spacing);
		if (ImGui::ButtonEx(ICON_FK_FLOPPY, ImVec2(button_size, 0), ImGuiButtonFlags_NoNavFocus))
		{
			std::error_code ec; std::filesystem::remove(_current_preset_path, ec);
			save_current_preset();
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Clean up and save the current preset (removes all settings for disabled techniques)");

		ImGui::SameLine(0, button_spacing);
		if (ImGui::ButtonEx(ICON_FK_PLUS, ImVec2(button_size, 0), ImGuiButtonFlags_NoNavFocus | ImGuiButtonFlags_PressedOnClick))
		{
			_file_selection_path = _current_preset_path.parent_path();
			ImGui::OpenPopup("##create");
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Add a new preset");

		if (is_loading())
		{
			ImGui::PopStyleColor();
			ImGui::PopItemFlag();
		}

		ImGui::SetNextWindowPos(popup_pos);
		if (imgui::file_dialog("##browse", _file_selection_path, browse_button_width, { L".ini", L".txt" }))
		{
			// Check that this is actually a valid preset file
			if (ini_file::load_cache(_file_selection_path).has({}, "Techniques"))
			{
				reload_preset = true;
				_current_preset_path = _file_selection_path;
			}
		}

		if (ImGui::BeginPopup("##create"))
		{
			ImGui::Checkbox("Duplicate current preset", &_duplicate_current_preset);

			char preset_name[260] = "";
			if (ImGui::InputText("Name", preset_name, sizeof(preset_name), ImGuiInputTextFlags_EnterReturnsTrue) && preset_name[0] != '\0')
			{
				std::filesystem::path new_preset_path = _file_selection_path / std::filesystem::u8path(preset_name);
				if (new_preset_path.extension() != L".ini" && new_preset_path.extension() != L".txt")
					new_preset_path += L".ini";

				std::error_code ec;
				const std::filesystem::file_type file_type = std::filesystem::status(new_preset_path, ec).type();
				if (file_type != std::filesystem::file_type::directory)
				{
					reload_preset =
						file_type == std::filesystem::file_type::not_found ||
						ini_file::load_cache(new_preset_path).has({}, "Techniques");

					if (_duplicate_current_preset && file_type == std::filesystem::file_type::not_found)
						std::filesystem::copy_file(_current_preset_path, new_preset_path, std::filesystem::copy_options::overwrite_existing, ec);
				}

				if (reload_preset)
				{
					ImGui::CloseCurrentPopup();
					_current_preset_path = new_preset_path;
				}
				else
				{
					ImGui::SetKeyboardFocusHere();
				}
			}

			if (ImGui::IsWindowAppearing())
				ImGui::SetKeyboardFocusHere();

			if (preset_name[0] == '\0' && ImGui::IsKeyPressedMap(ImGuiKey_Backspace))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		if (reload_preset)
		{
			_show_splash = true;

			save_config();
			load_current_preset();
		}

		if (_tutorial_index == 1)
			ImGui::PopStyleColor(2);
	}

	if (_tutorial_index > 1)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}

	if (is_loading())
	{
		const char *const loading_message = ICON_FK_REFRESH " Loading ... ";
		ImGui::SetCursorPos((ImGui::GetWindowSize() - ImGui::CalcTextSize(loading_message)) * 0.5f);
		ImGui::TextUnformatted(loading_message);
		return; // Cannot show techniques and variables while effects are loading, since they are being modified in other threads during that time
	}

	if (!_effects_enabled)
		ImGui::Text("Effects are disabled. Press '%s' to enable them again.", input::key_name(_effects_key_data).c_str());

	if (!_last_reload_successfull)
	{
		std::string filename_list;
		for (const effect &effect : _effects)
			if (!effect.compiled && !effect.skipped)
				filename_list += ' ' + effect.source_file.filename().u8string() + ',';

		// Make sure there are actually effects that failed to compile, since the last reload flag may not have been reset
		if (filename_list.empty())
		{
			_last_reload_successfull = true;
		}
		else
		{
			filename_list.pop_back();
			ImGui::TextColored(COLOR_RED, "Some effects failed to compile:%s", filename_list.c_str());
			ImGui::Spacing();
		}
	}
	if (!_last_texture_reload_successfull)
	{
		std::string texture_list;
		for (const texture &tex : _textures)
			if (tex.resource != 0 && !tex.loaded && !tex.annotation_as_string("source").empty())
				texture_list += ' ' + tex.unique_name + ',';

		if (texture_list.empty())
		{
			_last_texture_reload_successfull = true;
		}
		else
		{
			texture_list.pop_back();
			ImGui::TextColored(COLOR_RED, "Some textures failed to load:%s", texture_list.c_str());
			ImGui::Spacing();
		}
	}

	if (_tutorial_index > 1)
	{
		if (imgui::search_input_box(_effect_filter, sizeof(_effect_filter), -((_variable_editor_tabs ? 10.0f : 20.0f) * _font_size + (_variable_editor_tabs ? 1.0f : 2.0f) * _imgui_context->Style.ItemSpacing.x)))
		{
			_effects_expanded_state = 3;

			for (technique &tech : _techniques)
			{
				std::string_view label = tech.annotation_as_string("ui_label");
				if (label.empty())
					label = tech.name;

				tech.hidden = tech.annotation_as_int("hidden") != 0 || !filter_text(label, _effect_filter);
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Active to top", ImVec2(10.0f * _font_size, 0)))
		{
			for (auto i = _techniques.begin(); i != _techniques.end(); ++i)
			{
				if (!i->enabled && i->toggle_key_data[0] == 0)
				{
					for (auto k = i + 1; k != _techniques.end(); ++k)
					{
						if (k->enabled || k->toggle_key_data[0] != 0)
						{
							std::iter_swap(i, k);
							break;
						}
					}
				}
			}

			if (const auto it = std::find_if_not(_techniques.begin(), _techniques.end(),
					[](const technique &a) { return a.enabled || a.toggle_key_data[0] != 0; }); it != _techniques.end())
			{
				std::stable_sort(it, _techniques.end(),
					[](const technique &lhs, const technique &rhs) {
						std::string lhs_label(lhs.annotation_as_string("ui_label"));
						if (lhs_label.empty()) lhs_label = lhs.name;
						std::transform(lhs_label.begin(), lhs_label.end(), lhs_label.begin(), [](char c) { return static_cast<char>(toupper(c)); });
						std::string rhs_label(rhs.annotation_as_string("ui_label"));
						if (rhs_label.empty()) rhs_label = rhs.name;
						std::transform(rhs_label.begin(), rhs_label.end(), rhs_label.begin(), [](char c) { return static_cast<char>(toupper(c)); });
						return lhs_label < rhs_label;
					});
			}

			save_current_preset();
		}

		ImGui::SameLine();

		if (ImGui::Button((_effects_expanded_state & 2) ? "Collapse all" : "Expand all", ImVec2(10.0f * _font_size, 0)))
			_effects_expanded_state = (~_effects_expanded_state & 2) | 1;

		if (_tutorial_index == 2)
		{
			tutorial_text =
				"This is the list of effects. It contains all techniques found in the effect files (*.fx) from the effect search paths as specified in the settings.\n\n"
				"Enter text in the box at the top to filter it and search for specific techniques.\n\n"
				"Click on a technique to enable or disable it or drag it to a new location in the list to change the order in which the effects are applied.\n"
				"Use the right mouse button and click on an item to open the context menu with additional options.\n\n";

			ImGui::PushStyleColor(ImGuiCol_Border, COLOR_RED);
		}

		ImGui::Spacing();

		float bottom_height = ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y;
		if (!_performance_mode)
			bottom_height += 17 /* splitter */ + (_variable_editor_height + (_tutorial_index == 3 ? 175 : 0));

		if (ImGui::BeginChild("##techniques", ImVec2(0, -bottom_height), true))
		{
			if (_effect_load_skipping && _show_force_load_effects_button)
			{
				if (size_t skipped_effects = std::count_if(_effects.begin(), _effects.end(),
						[](const effect &effect) { return effect.skipped; }); skipped_effects > 0)
				{
					char buf[60];
					ImFormatString(buf, sizeof(buf), "Force load all effects (%zu remaining)", skipped_effects);
					if (ImGui::ButtonEx(buf, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
					{
						_load_option_disable_skipping = true;
						reload_effects();

						ImGui::EndChild();
						return;
					}
				}
			}

			draw_technique_editor();
		}
		ImGui::EndChild();

		if (_tutorial_index == 2)
			ImGui::PopStyleColor();
	}

	if (_tutorial_index > 2 && !_performance_mode)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::ButtonEx("##splitter", ImVec2(ImGui::GetContentRegionAvail().x, 5));
		ImGui::PopStyleVar();

		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		if (ImGui::IsItemActive())
		{
			_variable_editor_height = std::max(_variable_editor_height - _imgui_context->IO.MouseDelta.y, 0.0f);
			save_config();
		}

		if (_tutorial_index == 3)
		{
			tutorial_text =
				"This is the list of variables. It contains all tweakable options the active effects expose. Values here apply in real-time.\n\n"
				"Enter text in the box at the top to filter it and search for specific variables.\n\n"
				"Press 'Ctrl' and click on a widget to manually edit the value.\n"
				"Use the right mouse button and click on an item to open the context menu with additional options.\n\n"
				"Once you have finished tweaking your preset, be sure to enable the 'Performance Mode' check box. "
				"This will recompile all effects into a more optimal representation that can give a performance boost, but will disable variable tweaking and this list.";

			ImGui::PushStyleColor(ImGuiCol_Border, COLOR_RED);
		}

		const float bottom_height = ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y + (_tutorial_index == 3 ? 175 : 0);

		if (ImGui::BeginChild("##variables", ImVec2(0, -bottom_height), true))
			draw_variable_editor();
		ImGui::EndChild();

		if (_tutorial_index == 3)
			ImGui::PopStyleColor();
	}

	if (_tutorial_index > 3)
	{
		ImGui::Spacing();

		if (ImGui::Button(ICON_FK_REFRESH " Reload", ImVec2(-11.5f * _font_size, 0)))
		{
			load_config(); // Reload configuration too

			if (!_no_effect_cache && (_imgui_context->IO.KeyCtrl || _imgui_context->IO.KeyShift))
				clear_effect_cache();

			reload_effects();
		}

		ImGui::SameLine();

		if (ImGui::Checkbox("Performance Mode", &_performance_mode))
		{
			save_config();
			reload_effects(); // Reload effects after switching
		}
	}
	else
	{
		ImGui::BeginChildFrame(ImGui::GetID("tutorial"), ImVec2(0, 175));
		ImGui::TextWrapped(tutorial_text);
		ImGui::EndChildFrame();

		const float max_button_width = ImGui::GetContentRegionAvail().x;

		if (_tutorial_index == 0)
		{
			if (ImGui::Button("Continue", ImVec2(max_button_width * 0.66666666f, 0)))
			{
				_tutorial_index++;

				save_config();
			}

			ImGui::SameLine();

			if (ImGui::Button("Skip Tutorial", ImVec2(max_button_width * 0.33333333f - _imgui_context->Style.ItemSpacing.x, 0)))
			{
				_tutorial_index = 4;
				_no_font_scaling = true;

				save_config();
			}
		}
		else
		{
			if (ImGui::Button(_tutorial_index == 3 ? "Finish" : "Continue", ImVec2(max_button_width, 0)))
			{
				_tutorial_index++;

				if (_tutorial_index == 4)
				{
					// Disable font scaling after tutorial
					_no_font_scaling = true;

					save_config();
				}
			}
		}
	}
}
#endif
void reshade::runtime::draw_gui_settings()
{
	bool modified = false;
	bool modified_custom_style = false;

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		modified |= imgui::key_input_box("Overlay key", _overlay_key_data, *_input);

#if RESHADE_FX
		modified |= imgui::key_input_box("Effect toggle key", _effects_key_data, *_input);
		modified |= imgui::key_input_box("Effect reload key", _reload_key_data, *_input);

		modified |= imgui::key_input_box("Performance mode toggle key", _performance_mode_key_data, *_input);

		modified |= imgui::key_input_box("Previous preset key", _prev_preset_key_data, *_input);
		modified |= imgui::key_input_box("Next preset key", _next_preset_key_data, *_input);

		modified |= ImGui::SliderInt("Preset transition delay", reinterpret_cast<int *>(&_preset_transition_delay), 0, 10 * 1000);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Makes a smooth transition, but only for floating point values.\nRecommended for multiple presets that contain the same effects, otherwise set this to zero.\nValues are in milliseconds.");
#endif

		modified |= ImGui::Combo("Input processing", reinterpret_cast<int *>(&_input_processing_mode),
			"Pass on all input\0"
			"Block input when cursor is on overlay\0"
			"Block all input when overlay is visible\0");

#if RESHADE_FX
		ImGui::Spacing();

		modified |= imgui::path_list("Effect search paths", _effect_search_paths, _file_selection_path, g_reshade_base_path);
		modified |= imgui::path_list("Texture search paths", _texture_search_paths, _file_selection_path, g_reshade_base_path);

		if (ImGui::Checkbox("Load only enabled effects", &_effect_load_skipping))
		{
			modified = true;

			// Force load all effects in case some where skipped after load skipping was disabled
			_load_option_disable_skipping = !_effect_load_skipping;
			reload_effects();
		}

		if (ImGui::Button("Clear effect cache", ImVec2(ImGui::CalcItemWidth(), 0)))
			clear_effect_cache();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Clear effect cache located in \"%s\".", _intermediate_cache_path.u8string().c_str());
#endif
	}

	if (ImGui::CollapsingHeader("Screenshots", ImGuiTreeNodeFlags_DefaultOpen))
	{
		modified |= imgui::key_input_box("Screenshot key", _screenshot_key_data, *_input);
		modified |= imgui::directory_input_box("Screenshot path", _screenshot_path, _file_selection_path);

		char name[260] = "";
		_screenshot_name.copy(name, sizeof(name) - 1);
		if (ImGui::InputText("Screenshot name", name, sizeof(name)))
		{
			modified = true;
			_screenshot_name = name;
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(
				"Macros you can add that are resolved during saving:\n"
				"  %%AppName%%         Name of the application (%s)\n"
#if RESHADE_FX
				"  %%PresetName%%      File name without extension of the current preset file (%s)\n"
#endif
				"  %%Date%%            Current date in format '%s'\n"
				"  %%Time%%            Current time in format '%s'\n"
				"  %%TimeMS%%          Milliseconds fraction of current time",
				g_target_executable_path.stem().string().c_str(),
#if RESHADE_FX
				_current_preset_path.stem().string().c_str(),
#endif
				"yyyy-MM-dd",
				"HH-mm-ss");
		}

		modified |= ImGui::Combo("Screenshot format", reinterpret_cast<int *>(&_screenshot_format), "Bitmap (*.bmp)\0Portable Network Graphics (*.png)\0JPEG (*.jpeg)\0");

		if (_screenshot_format == 2)
			modified |= ImGui::SliderInt("JPEG quality", reinterpret_cast<int *>(&_screenshot_jpeg_quality), 1, 100);
		else
			modified |= ImGui::Checkbox("Clear alpha channel", &_screenshot_clear_alpha);

#if RESHADE_FX
		modified |= ImGui::Checkbox("Save current preset file", &_screenshot_include_preset);
		modified |= ImGui::Checkbox("Save before and after images", &_screenshot_save_before);
#endif
		modified |= ImGui::Checkbox("Save separate image with the overlay visible", &_screenshot_save_gui);

		modified |= imgui::file_input_box("Post-save command", _screenshot_post_save_command, _screenshot_post_save_command, { L".exe" });

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Executable that is called after saving a screenshot.\nThis can be used to perform additional processing on the image (e.g. compressing it with an image optimizer).");

		char arguments[260] = "";
		_screenshot_post_save_command_arguments.copy(arguments, sizeof(arguments) - 1);
		if (ImGui::InputText("Post-save command arguments", arguments, sizeof(arguments)))
		{
			modified = true;
			_screenshot_post_save_command_arguments = arguments;
		}

		if (ImGui::IsItemHovered())
		{
			const std::string extension = _screenshot_format == 0 ? ".bmp" : _screenshot_format == 1 ? ".png" : ".jpg";

			ImGui::SetTooltip(
				"Macros you can add that are resolved during command execution:\n"
				"  %%AppName%%         Name of the application (%s)\n"
#if RESHADE_FX
				"  %%PresetName%%      File name without extension of the current preset file (%s)\n"
#endif
				"  %%Date%%            Current date in format 'yyyy-MM-dd'\n"
				"  %%Time%%            Current time in format 'HH-mm-ss'\n"
				"  %%TimeMS%%          Milliseconds fraction of current time\n"
				"  %%TargetPath%%      Full path to the screenshot file (%s%s%s)\n"
				"  %%TargetDir%%       Full path to the screenshot directory (%s)\n"
				"  %%TargetFileName%%  File name of the screenshot file (%s%s)\n"
				"  %%TargetExt%%       File extension of the screenshot file (%s)\n"
				"  %%TargetName%%      File name without extension of the screenshot file (%s)",
				g_target_executable_path.stem().string().c_str(),
#if RESHADE_FX
				_current_preset_path.stem().string().c_str(),
#endif
				_screenshot_path.u8string().c_str(), _screenshot_name.c_str(), extension.c_str(),
				_screenshot_path.u8string().c_str(),
				_screenshot_name.c_str(), extension.c_str(),
				extension.c_str(),
				_screenshot_name.c_str());
		}

		modified |= imgui::directory_input_box("Post-save command working directory", _screenshot_post_save_command_working_directory, _file_selection_path);
		modified |= ImGui::Checkbox("Hide post-save command window", &_screenshot_post_save_command_no_window);
	}

	if (ImGui::CollapsingHeader("Overlay & Styling", ImGuiTreeNodeFlags_DefaultOpen))
	{
#if RESHADE_FX
		if (ImGui::Button("Restart tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
			_tutorial_index = 0;
#endif

		modified |= ImGui::Checkbox("Show screenshot message", &_show_screenshot_message);

#if RESHADE_FX
		if (_effect_load_skipping)
			modified |= ImGui::Checkbox("Show \"Force load all effects\" button", &_show_force_load_effects_button);
#endif

		modified |= ImGui::Checkbox("Save window state (ReShadeGUI.ini)", &_save_imgui_window_state);
#if RESHADE_FX
		modified |= ImGui::Checkbox("Group effect files with tabs instead of a tree", &_variable_editor_tabs);
#endif

		#pragma region Style
		if (ImGui::Combo("Global style", &_style_index, "Dark\0Light\0Default\0Custom Simple\0Custom Advanced\0Solarized Dark\0Solarized Light\0"))
		{
			modified = true;
			load_custom_style();
		}

		if (_style_index == 3) // Custom Simple
		{
			ImVec4 *const colors = _imgui_context->Style.Colors;

			if (ImGui::BeginChild("##colors", ImVec2(0, 105), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened))
			{
				ImGui::PushItemWidth(-160);
				modified_custom_style |= ImGui::ColorEdit3("Background", &colors[ImGuiCol_WindowBg].x);
				modified_custom_style |= ImGui::ColorEdit3("ItemBackground", &colors[ImGuiCol_FrameBg].x);
				modified_custom_style |= ImGui::ColorEdit3("Text", &colors[ImGuiCol_Text].x);
				modified_custom_style |= ImGui::ColorEdit3("ActiveItem", &colors[ImGuiCol_ButtonActive].x);
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();

			// Change all colors using the above as base
			if (modified_custom_style)
			{
				colors[ImGuiCol_PopupBg] = colors[ImGuiCol_WindowBg]; colors[ImGuiCol_PopupBg].w = 0.92f;

				colors[ImGuiCol_ChildBg] = colors[ImGuiCol_FrameBg]; colors[ImGuiCol_ChildBg].w = 0.00f;
				colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_FrameBg]; colors[ImGuiCol_MenuBarBg].w = 0.57f;
				colors[ImGuiCol_ScrollbarBg] = colors[ImGuiCol_FrameBg]; colors[ImGuiCol_ScrollbarBg].w = 1.00f;

				colors[ImGuiCol_TextDisabled] = colors[ImGuiCol_Text]; colors[ImGuiCol_TextDisabled].w = 0.58f;
				colors[ImGuiCol_Border] = colors[ImGuiCol_Text]; colors[ImGuiCol_Border].w = 0.30f;
				colors[ImGuiCol_Separator] = colors[ImGuiCol_Text]; colors[ImGuiCol_Separator].w = 0.32f;
				colors[ImGuiCol_SeparatorHovered] = colors[ImGuiCol_Text]; colors[ImGuiCol_SeparatorHovered].w = 0.78f;
				colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_Text]; colors[ImGuiCol_SeparatorActive].w = 1.00f;
				colors[ImGuiCol_PlotLines] = colors[ImGuiCol_Text]; colors[ImGuiCol_PlotLines].w = 0.63f;
				colors[ImGuiCol_PlotHistogram] = colors[ImGuiCol_Text]; colors[ImGuiCol_PlotHistogram].w = 0.63f;

				colors[ImGuiCol_FrameBgHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_FrameBgHovered].w = 0.68f;
				colors[ImGuiCol_FrameBgActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_FrameBgActive].w = 1.00f;
				colors[ImGuiCol_TitleBg] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_TitleBg].w = 0.45f;
				colors[ImGuiCol_TitleBgCollapsed] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_TitleBgCollapsed].w = 0.35f;
				colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_TitleBgActive].w = 0.58f;
				colors[ImGuiCol_ScrollbarGrab] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ScrollbarGrab].w = 0.31f;
				colors[ImGuiCol_ScrollbarGrabHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ScrollbarGrabHovered].w = 0.78f;
				colors[ImGuiCol_ScrollbarGrabActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ScrollbarGrabActive].w = 1.00f;
				colors[ImGuiCol_CheckMark] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_CheckMark].w = 0.80f;
				colors[ImGuiCol_SliderGrab] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_SliderGrab].w = 0.24f;
				colors[ImGuiCol_SliderGrabActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_SliderGrabActive].w = 1.00f;
				colors[ImGuiCol_Button] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_Button].w = 0.44f;
				colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ButtonHovered].w = 0.86f;
				colors[ImGuiCol_Header] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_Header].w = 0.76f;
				colors[ImGuiCol_HeaderHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_HeaderHovered].w = 0.86f;
				colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_HeaderActive].w = 1.00f;
				colors[ImGuiCol_ResizeGrip] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ResizeGrip].w = 0.20f;
				colors[ImGuiCol_ResizeGripHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ResizeGripHovered].w = 0.78f;
				colors[ImGuiCol_ResizeGripActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_ResizeGripActive].w = 1.00f;
				colors[ImGuiCol_PlotLinesHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_PlotLinesHovered].w = 1.00f;
				colors[ImGuiCol_PlotHistogramHovered] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_PlotHistogramHovered].w = 1.00f;
				colors[ImGuiCol_TextSelectedBg] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_TextSelectedBg].w = 0.43f;

				colors[ImGuiCol_Tab] = colors[ImGuiCol_Button];
				colors[ImGuiCol_TabActive] = colors[ImGuiCol_ButtonActive];
				colors[ImGuiCol_TabHovered] = colors[ImGuiCol_ButtonHovered];
				colors[ImGuiCol_TabUnfocused] = ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
				colors[ImGuiCol_TabUnfocusedActive] = ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);
				colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_Header] * ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
				colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
			}
		}
		if (_style_index == 4) // Custom Advanced
		{
			if (ImGui::BeginChild("##colors", ImVec2(0, 300), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened))
			{
				ImGui::PushItemWidth(-160);
				for (ImGuiCol i = 0; i < ImGuiCol_COUNT; i++)
				{
					ImGui::PushID(i);
					modified_custom_style |= ImGui::ColorEdit4("##color", &_imgui_context->Style.Colors[i].x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
					ImGui::SameLine(); ImGui::TextUnformatted(ImGui::GetStyleColorName(i));
					ImGui::PopID();
				}
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
		}
		#pragma endregion

		#pragma region Editor Style
		if (ImGui::Combo("Text editor style", &_editor_style_index, "Dark\0Light\0Custom\0Solarized Dark\0Solarized Light\0"))
		{
			modified = true;
			load_custom_style();
		}

		if (_editor_style_index == 2)
		{
			if (ImGui::BeginChild("##editor_colors", ImVec2(0, 300), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened))
			{
				ImGui::PushItemWidth(-160);
				for (ImGuiCol i = 0; i < imgui::code_editor::color_palette_max; i++)
				{
					ImVec4 color = ImGui::ColorConvertU32ToFloat4(_editor_palette[i]);
					ImGui::PushID(i);
					modified_custom_style |= ImGui::ColorEdit4("##editor_color", &color.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
					ImGui::SameLine(); ImGui::TextUnformatted(imgui::code_editor::get_palette_color_name(i));
					ImGui::PopID();
					_editor_palette[i] = ImGui::ColorConvertFloat4ToU32(color);
				}
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
		}
		#pragma endregion

		if (imgui::font_input_box("Global font", _font, _file_selection_path, _font_size))
		{
			modified = true;
			_rebuild_font_atlas = true;
		}

		if (imgui::font_input_box("Text editor font", _editor_font, _file_selection_path, _editor_font_size))
		{
			modified = true;
			_rebuild_font_atlas = true;
		}

		if (float &alpha = _imgui_context->Style.Alpha; ImGui::SliderFloat("Global alpha", &alpha, 0.1f, 1.0f, "%.2f"))
		{
			// Prevent user from setting alpha to zero
			alpha = std::max(alpha, 0.1f);
			modified = true;
		}

		if (float &rounding = _imgui_context->Style.FrameRounding; ImGui::SliderFloat("Frame rounding", &rounding, 0.0f, 12.0f, "%.0f"))
		{
			// Apply the same rounding to everything
			_imgui_context->Style.GrabRounding = _imgui_context->Style.TabRounding = _imgui_context->Style.ScrollbarRounding = rounding;
			_imgui_context->Style.WindowRounding = _imgui_context->Style.ChildRounding = _imgui_context->Style.PopupRounding = rounding;
			modified = true;
		}

		modified |= ImGui::Checkbox("Show clock", &_show_clock);
		ImGui::SameLine(0, 10); modified |= ImGui::Checkbox("Show FPS", &_show_fps);
		ImGui::SameLine(0, 10); modified |= ImGui::Checkbox("Show frame time", &_show_frametime);
		modified |= ImGui::Combo("Clock format", reinterpret_cast<int *>(&_clock_format), "HH:mm\0HH:mm:ss\0");
		modified |= ImGui::SliderFloat("OSD text size", &_fps_scale, 0.2f, 2.5f, "%.1f");
		modified |= ImGui::ColorEdit4("OSD text color", _fps_col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
		modified |= ImGui::Combo("Position on screen", reinterpret_cast<int *>(&_fps_pos), "Top Left\0Top Right\0Bottom Left\0Bottom Right\0");
	}

	if (modified)
		save_config();
	if (modified_custom_style)
		save_custom_style();
}
void reshade::runtime::draw_gui_statistics()
{
	unsigned int gpu_digits = 1;
#if RESHADE_FX
	unsigned int cpu_digits = 1;
	uint64_t post_processing_time_cpu = 0;
	uint64_t post_processing_time_gpu = 0;

	if (!is_loading() && _effects_enabled)
	{
		for (const technique &tech : _techniques)
		{
			cpu_digits = std::max(cpu_digits, tech.average_cpu_duration >= 100'000'000 ? 3u : tech.average_cpu_duration >= 10'000'000 ? 2u : 1u);
			post_processing_time_cpu += tech.average_cpu_duration;
			gpu_digits = std::max(gpu_digits, tech.average_gpu_duration >= 100'000'000 ? 3u : tech.average_gpu_duration >= 10'000'000 ? 2u : 1u);
			post_processing_time_gpu += tech.average_gpu_duration;
		}
	}
#endif

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		_gather_gpu_statistics = true;

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PlotLines("##framerate",
			_imgui_context->FramerateSecPerFrame, 120,
			_imgui_context->FramerateSecPerFrameIdx,
			nullptr,
			_imgui_context->FramerateSecPerFrameAccum / 120 * 0.5f,
			_imgui_context->FramerateSecPerFrameAccum / 120 * 1.5f,
			ImVec2(0, 50));

		const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		tm tm; localtime_s(&tm, &t);

		ImGui::BeginGroup();

		ImGui::TextUnformatted("Hardware:");
		ImGui::TextUnformatted("Application:");
		ImGui::TextUnformatted("Time:");
		ImGui::Text("Frame %llu:", _framecount + 1);
#if RESHADE_FX
		ImGui::TextUnformatted("Post-Processing:");
#endif

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.33333333f);
		ImGui::BeginGroup();

		if (_vendor_id != 0)
			ImGui::Text("VEN_%X", _vendor_id);
		else
			ImGui::TextUnformatted("Unknown");
		ImGui::TextUnformatted(g_target_executable_path.filename().u8string().c_str());
		ImGui::Text("%d-%d-%d %d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
		ImGui::Text("%.2f fps", _imgui_context->IO.Framerate);
#if RESHADE_FX
		ImGui::Text("%*.3f ms CPU", cpu_digits + 4, post_processing_time_cpu * 1e-6f);
#endif

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.66666666f);
		ImGui::BeginGroup();

		if (_device_id != 0)
			ImGui::Text("DEV_%X", _device_id);
		else
			ImGui::TextUnformatted("Unknown");
		ImGui::Text("0x%X", std::hash<std::string>()(g_target_executable_path.stem().u8string()) & 0xFFFFFFFF);
		ImGui::Text("%.0f ms", std::chrono::duration_cast<std::chrono::nanoseconds>(_last_present_time - _start_time).count() * 1e-6f);
		ImGui::Text("%*.3f ms", gpu_digits + 4, _last_frame_duration.count() * 1e-6f);
#if RESHADE_FX
		if (_gather_gpu_statistics && post_processing_time_gpu != 0)
			ImGui::Text("%*.3f ms GPU", gpu_digits + 4, (post_processing_time_gpu * 1e-6f));
#endif

		ImGui::EndGroup();
	}

#if RESHADE_FX
	if (ImGui::CollapsingHeader("Techniques", ImGuiTreeNodeFlags_DefaultOpen) && !is_loading() && _effects_enabled)
	{
		// Only need to gather GPU statistics if the statistics are actually visible
		_gather_gpu_statistics = true;

		ImGui::BeginGroup();

		for (const technique &tech : _techniques)
		{
			if (!tech.enabled)
				continue;

			if (tech.passes.size() > 1)
				ImGui::Text("%s (%zu passes)", tech.name.c_str(), tech.passes.size());
			else
				ImGui::TextUnformatted(tech.name.c_str());
		}

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.33333333f);
		ImGui::BeginGroup();

		for (const technique &tech : _techniques)
		{
			if (!tech.enabled)
				continue;

			if (tech.average_cpu_duration != 0)
				ImGui::Text("%*.3f ms CPU", cpu_digits + 4, tech.average_cpu_duration * 1e-6f);
			else
				ImGui::NewLine();
		}

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.66666666f);
		ImGui::BeginGroup();

		for (const technique &tech : _techniques)
		{
			if (!tech.enabled)
				continue;

			// GPU timings are not available for all APIs
			if (_gather_gpu_statistics && tech.average_gpu_duration != 0)
				ImGui::Text("%*.3f ms GPU", gpu_digits + 4, tech.average_gpu_duration * 1e-6f);
			else
				ImGui::NewLine();
		}

		ImGui::EndGroup();
	}

	if (ImGui::CollapsingHeader("Render Targets & Textures", ImGuiTreeNodeFlags_DefaultOpen) && !is_loading())
	{
		static const char *texture_formats[] = {
			"unknown",
			"R8", "R16F", "R32F", "RG8", "RG16", "RG16F", "RG32F", "RGBA8", "RGBA16", "RGBA16F", "RGBA32F", "RGB10A2"
		};
		static constexpr uint32_t pixel_sizes[] = {
			0,
			1 /*R8*/, 2 /*R16F*/, 4 /*R32F*/, 2 /*RG8*/, 4 /*RG16*/, 4 /*RG16F*/, 8 /*RG32F*/, 4 /*RGBA8*/, 8 /*RGBA16*/, 8 /*RGBA16F*/, 16 /*RGBA32F*/, 4 /*RGB10A2*/
		};

		static_assert((std::size(texture_formats) - 1) == static_cast<size_t>(reshadefx::texture_format::rgb10a2));

		const float total_width = ImGui::GetContentRegionAvail().x;
		int texture_index = 0;
		const unsigned int num_columns = static_cast<unsigned int>(std::ceilf(total_width / (55.0f * _font_size)));
		const float single_image_width = (total_width / num_columns) - 5.0f;

		// Variables used to calculate memory size of textures
		lldiv_t memory_view;
		int64_t post_processing_memory_size = 0;
		const char *memory_size_unit;

		for (const texture &tex : _textures)
		{
			if (tex.resource == 0 || !tex.semantic.empty() || !std::any_of(tex.shared.begin(), tex.shared.end(), [this](size_t effect_index) { return _effects[effect_index].rendering; }))
				continue;

			ImGui::PushID(texture_index);
			ImGui::BeginGroup();

			int64_t memory_size = 0;
			for (uint32_t level = 0, width = tex.width, height = tex.height; level < tex.levels; ++level, width /= 2, height /= 2)
				memory_size += static_cast<size_t>(width) * static_cast<size_t>(height) * pixel_sizes[static_cast<int>(tex.format)];

			post_processing_memory_size += memory_size;

			if (memory_size >= 1024 * 1024)
			{
				memory_view = std::lldiv(memory_size, 1024 * 1024);
				memory_view.rem /= 1000;
				memory_size_unit = "MiB";
			}
			else
			{
				memory_view = std::lldiv(memory_size, 1024);
				memory_size_unit = "KiB";
			}

			ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s%s", tex.unique_name.c_str(), tex.shared.size() > 1 ? " (Pooled)" : "");
			ImGui::Text("%ux%u | %u mipmap(s) | %s | %lld.%03lld %s",
				tex.width,
				tex.height,
				tex.levels - 1,
				texture_formats[static_cast<int>(tex.format)],
				memory_view.quot, memory_view.rem, memory_size_unit);

			size_t num_referenced_passes = 0;
			std::vector<std::pair<size_t, std::vector<std::string>>> references;
			for (const technique &tech : _techniques)
			{
				if (std::find(tex.shared.begin(), tex.shared.end(), tech.effect_index) == tex.shared.end())
					continue;

				auto &reference = references.emplace_back();
				reference.first = tech.effect_index;

				for (size_t pass_index = 0; pass_index < tech.passes.size(); ++pass_index)
				{
					std::string pass_name = tech.passes[pass_index].name;
					if (pass_name.empty())
						pass_name = "pass " + std::to_string(pass_index);
					pass_name = tech.name + ' ' + pass_name;

					bool referenced = false;
					for (const reshadefx::sampler_info &sampler : tech.passes[pass_index].samplers)
					{
						if (sampler.texture_name == tex.unique_name)
						{
							referenced = true;
							reference.second.emplace_back(pass_name + " (sampler)");
							break;
						}
					}

					for (const reshadefx::storage_info &storage : tech.passes[pass_index].storages)
					{
						if (storage.texture_name == tex.unique_name)
						{
							referenced = true;
							reference.second.emplace_back(pass_name + " (storage)");
							break;
						}
					}

					for (const std::string &render_target : tech.passes[pass_index].render_target_names)
					{
						if (render_target == tex.unique_name)
						{
							referenced = true;
							reference.second.emplace_back(pass_name + " (render target)");
							break;
						}
					}

					if (referenced)
						num_referenced_passes++;
				}
			}

			const auto button_size = ImGui::GetFrameHeight();
			const auto button_spacing = _imgui_context->Style.ItemInnerSpacing.x;
			const bool supports_saving = (
				tex.format == reshadefx::texture_format::r8 ||
				tex.format == reshadefx::texture_format::rg8 ||
				tex.format == reshadefx::texture_format::rgba8 ||
				tex.format == reshadefx::texture_format::rgb10a2);

			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
			if (const std::string label = "Referenced by " + std::to_string(num_referenced_passes) + " pass(es) in " + std::to_string(tex.shared.size()) + " effect(s) ...";
				ImGui::ButtonEx(label.c_str(), ImVec2(single_image_width - (supports_saving ? button_spacing + button_size : 0), 0)))
				ImGui::OpenPopup("##references");
			if (supports_saving)
			{
				ImGui::SameLine(0, button_spacing);
				if (ImGui::Button(ICON_FK_FLOPPY, ImVec2(button_size, 0)))
					save_texture(tex);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Save %s", tex.unique_name.c_str());
			}
			ImGui::PopStyleVar();

			if (!references.empty() && ImGui::BeginPopup("##references"))
			{
				bool is_open = false;
				size_t effect_index = std::numeric_limits<size_t>::max();

				for (const auto &reference : references)
				{
					if (effect_index != reference.first)
					{
						effect_index  = reference.first;
						is_open = ImGui::TreeNodeEx(_effects[effect_index].source_file.filename().u8string().c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen);
					}

					if (is_open)
					{
						for (const auto &pass : reference.second)
						{
							ImGui::Dummy(ImVec2(_imgui_context->Style.IndentSpacing, 0.0f));
							ImGui::SameLine(0.0f, 0.0f);
							ImGui::TextUnformatted(pass.c_str(), pass.c_str() + pass.size());
						}
					}
				}

				ImGui::EndPopup();
			}

			if (bool check = _preview_texture == tex.srv[0] && _preview_size[0] == 0; ImGui::RadioButton("Preview scaled", check))
			{
				_preview_size[0] = 0;
				_preview_size[1] = 0;
				_preview_texture = !check ? tex.srv[0] : api::resource_view { 0 };
			}
			ImGui::SameLine();
			if (bool check = _preview_texture == tex.srv[0] && _preview_size[0] != 0; ImGui::RadioButton("Preview original", check))
			{
				_preview_size[0] = tex.width;
				_preview_size[1] = tex.height;
				_preview_texture = !check ? tex.srv[0] : api::resource_view { 0 };
			}

			bool r = (_preview_size[2] & 0x000000FF) != 0;
			bool g = (_preview_size[2] & 0x0000FF00) != 0;
			bool b = (_preview_size[2] & 0x00FF0000) != 0;
			ImGui::SameLine();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(_imgui_context->Style.FramePadding.x, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 0, 0, 1));
			imgui::toggle_button("R", r, 0.0f, ImGuiButtonFlags_AlignTextBaseLine);
			ImGui::PopStyleColor();
			if (tex.format >= reshadefx::texture_format::rg8)
			{
				ImGui::SameLine(0, 1);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 1, 0, 1));
				imgui::toggle_button("G", g, 0.0f, ImGuiButtonFlags_AlignTextBaseLine);
				ImGui::PopStyleColor();
				if (tex.format >= reshadefx::texture_format::rgba8)
				{
					ImGui::SameLine(0, 1);
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 1, 1));
					imgui::toggle_button("B", b, 0.0f, ImGuiButtonFlags_AlignTextBaseLine);
					ImGui::PopStyleColor();
				}
			}
			ImGui::PopStyleVar();
			_preview_size[2] = (r ? 0x000000FF : 0) | (g ? 0x0000FF00 : 0) | (b ? 0x00FF0000 : 0) | 0xFF000000;

			const float aspect_ratio = static_cast<float>(tex.width) / static_cast<float>(tex.height);
			imgui::image_with_checkerboard_background(tex.srv[0].handle, ImVec2(single_image_width, single_image_width / aspect_ratio), _preview_size[2]);

			ImGui::EndGroup();
			ImGui::PopID();

			if ((texture_index++ % num_columns) != (num_columns - 1))
				ImGui::SameLine(0.0f, 5.0f);
			else
				ImGui::Spacing();
		}

		if ((texture_index % num_columns) != 0)
			ImGui::NewLine(); // Reset ImGui::SameLine() so the following starts on a new line

		ImGui::Separator();

		if (post_processing_memory_size >= 1024 * 1024)
		{
			memory_view = std::lldiv(post_processing_memory_size, 1024 * 1024);
			memory_view.rem /= 1000;
			memory_size_unit = "MiB";
		}
		else
		{
			memory_view = std::lldiv(post_processing_memory_size, 1024);
			memory_size_unit = "KiB";
		}

		ImGui::Text("Total memory usage: %lld.%03lld %s", memory_view.quot, memory_view.rem, memory_size_unit);
	}
#endif
}
void reshade::runtime::draw_gui_log()
{
	const std::filesystem::path log_path = g_reshade_base_path / L"ReShade.log";

	const bool filter_changed = imgui::search_input_box(_log_filter, sizeof(_log_filter), -(16.0f * _font_size + 2 * _imgui_context->Style.ItemSpacing.x));

	ImGui::SameLine();

	imgui::toggle_button("Word Wrap", _log_wordwrap, 8.0f * _font_size);

	ImGui::SameLine();

	if (ImGui::Button("Clear Log", ImVec2(8.0f * _font_size, 0.0f)))
		// Close and open the stream again, which will clear the file too
		log::open_log_file(log_path);

	ImGui::Spacing();

	if (ImGui::BeginChild("log", ImVec2(0, 0), true, _log_wordwrap ? 0 : ImGuiWindowFlags_AlwaysHorizontalScrollbar))
	{
		// Limit number of log lines to read, to avoid stalling when log gets too big
		constexpr size_t LINE_LIMIT = 1000;

		std::error_code ec;
		const auto last_log_size = std::filesystem::file_size(log_path, ec);
		if (filter_changed || _last_log_size != last_log_size)
		{
			_log_lines.clear();
			std::ifstream log_file(log_path);
			for (std::string line; std::getline(log_file, line) && _log_lines.size() < LINE_LIMIT;)
				if (filter_text(line, _log_filter))
					_log_lines.push_back(line);
			_last_log_size = last_log_size;

			if (_log_lines.size() == LINE_LIMIT)
				_log_lines.push_back("Log was truncated to reduce memory footprint!");
		}

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(_log_lines.size()), ImGui::GetTextLineHeightWithSpacing());
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				ImVec4 textcol = ImGui::GetStyleColorVec4(ImGuiCol_Text);

				if (_log_lines[i].find("ERROR |") != std::string::npos || _log_lines[i].find("error") != std::string::npos)
					textcol = COLOR_RED;
				else if (_log_lines[i].find("WARN  |") != std::string::npos || _log_lines[i].find("warning") != std::string::npos || i == LINE_LIMIT)
					textcol = COLOR_YELLOW;
				else if (_log_lines[i].find("DEBUG |") != std::string::npos)
					textcol = ImColor(100, 100, 255);

				ImGui::PushStyleColor(ImGuiCol_Text, textcol);
				if (_log_wordwrap) ImGui::PushTextWrapPos();

				ImGui::TextUnformatted(_log_lines[i].c_str());

				if (_log_wordwrap) ImGui::PopTextWrapPos();
				ImGui::PopStyleColor();
			}
		}
	}
	ImGui::EndChild();
}
void reshade::runtime::draw_gui_about()
{
	ImGui::TextUnformatted("ReShade " VERSION_STRING_PRODUCT);
	ImGui::Separator();

	ImGui::PushTextWrapPos();

	ImGui::TextUnformatted("Developed and maintained by crosire.");
	ImGui::TextUnformatted("Shout-out to CeeJay.dk and Marty McFly for their involvement.");
	ImGui::TextUnformatted("This project makes use of several open source libraries, licenses of which are listed below:");

	if (ImGui::CollapsingHeader("ReShade", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const auto resource = resources::load_data_resource(IDR_LICENSE_RESHADE);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("MinHook"))
	{
		const auto resource = resources::load_data_resource(IDR_LICENSE_MINHOOK);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("Dear ImGui"))
	{
		const auto resource = resources::load_data_resource(IDR_LICENSE_IMGUI);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("ImGuiColorTextEdit"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2017 BalazsJako

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)");
	}
	if (ImGui::CollapsingHeader("gl3w"))
	{
		const auto resource = resources::load_data_resource(IDR_LICENSE_GL3W);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("UTF8-CPP"))
	{
		const auto resource = resources::load_data_resource(IDR_LICENSE_UTFCPP);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("stb_image, stb_image_write"))
	{
		const auto resource = resources::load_data_resource(IDR_LICENSE_STB);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("DDS loading from SOIL"))
	{
		ImGui::TextUnformatted("Jonathan \"lonesock\" Dummer");
	}
	if (ImGui::CollapsingHeader("fpng"))
	{
		ImGui::TextUnformatted("Public Domain (https://github.com/richgel999/fpng)");
	}
	if (ImGui::CollapsingHeader("SPIR-V"))
	{
		const auto resource = resources::load_data_resource(IDR_LICENSE_SPIRV);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("Vulkan & Vulkan-Loader"))
	{
		const auto resource = resources::load_data_resource(IDR_LICENSE_VULKAN);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("Vulkan Memory Allocator"))
	{
		const auto resource = resources::load_data_resource(IDR_LICENSE_VMA);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("Solarized"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2011 Ethan Schoonover

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)");
	}
	if (ImGui::CollapsingHeader("Fork Awesome"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2018 Fork Awesome (https://forkawesome.github.io)

This Font Software is licensed under the SIL Open Font License, Version 1.1. (http://scripts.sil.org/OFL))");
	}

	ImGui::PopTextWrapPos();
}
#if RESHADE_ADDON
void reshade::runtime::draw_gui_addons()
{
#if RESHADE_ADDON_LITE
	if (!addon_enabled)
	{
		ImGui::TextColored(COLOR_YELLOW, "High network activity discovered.\nAll add-ons are disabled to prevent exploitation.");
		return;
	}

	ImGui::AlignTextToFramePadding();
	ImGui::TextColored(COLOR_YELLOW, "Only limited add-on functionality is available.");
#else
	std::filesystem::path addon_search_path = g_reshade_base_path;
	global_config().get("INSTALL", "AddonPath", addon_search_path);
	if (imgui::directory_input_box("Add-on search path", addon_search_path, _file_selection_path))
	{
		global_config().set("INSTALL", "AddonPath", addon_search_path);
		global_config().save();
	}
#endif

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	imgui::search_input_box(_addons_filter, sizeof(_addons_filter));

	ImGui::Spacing();

	std::vector<std::string> disabled_addons;
	global_config().get("ADDON", "DisabledAddons", disabled_addons);

	const float child_window_width = ImGui::GetContentRegionAvail().x;

	for (addon_info &info : addon_loaded_info)
	{
		if (!filter_text(info.name, _addons_filter))
			continue;

		const ImGuiID settings_id = ImGui::GetID(info.name.c_str());
		float settings_height = ImGui::GetStateStorage()->GetFloat(settings_id);

		ImGui::BeginChild(info.name.c_str(), ImVec2(child_window_width, settings_height + _imgui_context->Style.FramePadding.y * 2), true, ImGuiWindowFlags_NoScrollbar);

		bool open = ImGui::GetStateStorage()->GetBool(ImGui::GetID("##addon_open"));
		if (ImGui::ArrowButton("##addon_open", open ? ImGuiDir_Down : ImGuiDir_Right))
			ImGui::GetStateStorage()->SetBool(ImGui::GetID("##addon_open"), open = !open);

		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Text, _imgui_context->Style.Colors[info.handle != nullptr ? ImGuiCol_Text : ImGuiCol_TextDisabled]);

		bool enabled = std::find(disabled_addons.begin(), disabled_addons.end(), info.name) == disabled_addons.end();
		if (ImGui::Checkbox(info.name.c_str(), &enabled))
		{
			if (enabled)
				disabled_addons.erase(std::remove(disabled_addons.begin(), disabled_addons.end(), info.name), disabled_addons.end());
			else
				disabled_addons.push_back(info.name);

			global_config().set("ADDON", "DisabledAddons", disabled_addons);
			global_config().save();
		}

		ImGui::PopStyleColor();

		if (enabled == (info.handle == nullptr))
		{
			ImGui::SameLine();
			ImGui::Text("(will be %s on next application restart)", enabled ? "enabled" : "disabled");
		}

		if (open)
		{
			ImGui::Spacing();

			ImGui::BeginGroup();
			ImGui::Text("File:");
			if (!info.author.empty())
				ImGui::Text("Author:");
			ImGui::Text("Version:");
			if (!info.description.empty())
				ImGui::Text("Description:");
			ImGui::EndGroup();
			ImGui::SameLine(ImGui::GetWindowWidth() * 0.25f);
			ImGui::BeginGroup();
			ImGui::TextUnformatted(info.file.c_str());
			if (!info.author.empty())
				ImGui::TextUnformatted(info.author.c_str());
			ImGui::TextUnformatted(info.version.c_str());
			if (!info.description.empty())
			{
				ImGui::PushTextWrapPos();
				ImGui::TextUnformatted(info.description.c_str());
				ImGui::PopTextWrapPos();
			}
			ImGui::EndGroup();

			if (info.settings_overlay_callback != nullptr)
			{
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				info.settings_overlay_callback(this);
			}
		}

		settings_height = ImGui::GetCursorPosY();

		ImGui::EndChild();

		ImGui::GetStateStorage()->SetFloat(settings_id, settings_height);
	}
}
#endif

#if RESHADE_FX
void reshade::runtime::draw_variable_editor()
{
	const ImVec2 popup_pos = ImGui::GetCursorScreenPos() + ImVec2(std::max(0.f, (ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x) * 0.5f - 200.0f), ImGui::GetFrameHeightWithSpacing());

	if (imgui::popup_button("Edit global preprocessor definitions", ImGui::GetContentRegionAvail().x, ImGuiWindowFlags_NoMove))
	{
		ImGui::SetWindowPos(popup_pos);

		bool modified = false;
		float popup_height = (std::max(_global_preprocessor_definitions.size(), _preset_preprocessor_definitions.size()) + 2) * ImGui::GetFrameHeightWithSpacing();
		popup_height = std::min(popup_height, _window_height - popup_pos.y - 20.0f);
		popup_height = std::max(popup_height, 42.0f); // Ensure window always has a minimum height
		const float button_size = ImGui::GetFrameHeight();
		const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;

		ImGui::BeginChild("##definitions", ImVec2(400.0f, popup_height), false, ImGuiWindowFlags_NoScrollWithMouse);

		if (ImGui::BeginTabBar("##definition_types", ImGuiTabBarFlags_NoTooltip))
		{
			if (ImGui::BeginTabItem("Global"))
			{
				const float content_region_width = ImGui::GetContentRegionAvail().x;

				for (size_t i = 0; i < _global_preprocessor_definitions.size(); ++i)
				{
					char name[128] = "";
					char value[128] = "";

					const size_t equals_index = _global_preprocessor_definitions[i].find('=');
					_global_preprocessor_definitions[i].copy(name, std::min(equals_index, sizeof(name) - 1));
					if (equals_index != std::string::npos)
						_global_preprocessor_definitions[i].copy(value, sizeof(value) - 1, equals_index + 1);

					ImGui::PushID(static_cast<int>(i));

					ImGui::SetNextItemWidth(content_region_width * 0.66666666f - (button_spacing));
					modified |= ImGui::InputText("##name", name, sizeof(name), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter,
						[](ImGuiInputTextCallbackData *data) -> int { return data->EventChar == '=' || (data->EventChar != '_' && !isalnum(data->EventChar)); }); // Filter out invalid characters

					ImGui::SameLine(0, button_spacing);

					ImGui::SetNextItemWidth(content_region_width * 0.33333333f - (button_spacing + button_size) + 1);
					modified |= ImGui::InputText("##value", value, sizeof(value));

					ImGui::SameLine(0, button_spacing);

					if (ImGui::Button(ICON_FK_MINUS, ImVec2(button_size, 0)))
					{
						modified = true;
						_global_preprocessor_definitions.erase(_global_preprocessor_definitions.begin() + i--);
					}
					else if (modified)
					{
						_global_preprocessor_definitions[i] = std::string(name) + '=' + std::string(value);
					}

					ImGui::PopID();
				}

				ImGui::Dummy(ImVec2());
				ImGui::SameLine(0, content_region_width - button_size);
				if (ImGui::Button(ICON_FK_PLUS, ImVec2(button_size, 0)))
					_global_preprocessor_definitions.emplace_back();

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Current Preset"))
			{
				const float content_region_width = ImGui::GetContentRegionAvail().x;

				for (size_t i = 0; i < _preset_preprocessor_definitions.size(); ++i)
				{
					char name[128] = "";
					char value[128] = "";

					const size_t equals_index = _preset_preprocessor_definitions[i].find('=');
					_preset_preprocessor_definitions[i].copy(name, std::min(equals_index, sizeof(name) - 1));
					if (equals_index != std::string::npos)
						_preset_preprocessor_definitions[i].copy(value, sizeof(value) - 1, equals_index + 1);

					ImGui::PushID(static_cast<int>(i));

					ImGui::SetNextItemWidth(content_region_width * 0.66666666f - (button_spacing));
					modified |= ImGui::InputText("##name", name, sizeof(name), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter,
						[](ImGuiInputTextCallbackData *data) -> int { return data->EventChar == '=' || (data->EventChar != '_' && !isalnum(data->EventChar)); }); // Filter out invalid characters

					ImGui::SameLine(0, button_spacing);

					ImGui::SetNextItemWidth(content_region_width * 0.33333333f - (button_spacing + button_size) + 1);
					modified |= ImGui::InputText("##value", value, sizeof(value), ImGuiInputTextFlags_AutoSelectAll);

					ImGui::SameLine(0, button_spacing);

					if (ImGui::Button(ICON_FK_MINUS, ImVec2(button_size, 0)))
					{
						modified = true;
						_preset_preprocessor_definitions.erase(_preset_preprocessor_definitions.begin() + i--);
					}
					else if (modified)
					{
						_preset_preprocessor_definitions[i] = std::string(name) + '=' + std::string(value);
					}

					ImGui::PopID();
				}

				ImGui::Dummy(ImVec2());
				ImGui::SameLine(0, content_region_width - button_size);
				if (ImGui::Button(ICON_FK_PLUS, ImVec2(button_size, 0)))
					_preset_preprocessor_definitions.emplace_back();

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::EndChild();

		if (modified)
		{
			save_config();
			save_current_preset();
			_was_preprocessor_popup_edited = true;
		}

		ImGui::EndPopup();
	}
	else if (_was_preprocessor_popup_edited)
	{
		reload_effects();
		_was_preprocessor_popup_edited = false;
	}

	const auto find_definition_value = [](auto &list, const auto &name, char value[128] = nullptr)
	{
		for (auto it = list.begin(); it != list.end(); ++it)
		{
			char current_name[128] = "";
			const size_t equals_index = it->find('=');
			it->copy(current_name, std::min(equals_index, sizeof(current_name) - 1));

			if (name == current_name)
			{
				if (equals_index != std::string::npos && value != nullptr)
					value[it->copy(value, 127, equals_index + 1)] = '\0';
				return it;
			}
		}

		return list.end();
	};

	ImGui::BeginChild("##variables");
	if (_variable_editor_tabs)
		ImGui::BeginTabBar("##variables");

	for (size_t effect_index = 0, id = 0; effect_index < _effects.size(); ++effect_index)
	{
		reshade::effect &effect = _effects[effect_index];

		// Hide variables that are not currently used in any of the active effects
		// Also skip showing this effect in the variable list if it doesn't have any uniform variables to show
		if (!effect.rendering || (effect.uniforms.empty() && effect.definitions.empty() && effect.preprocessed))
			continue;
		assert(effect.compiled);

		bool force_reload_effect = false;
		const bool is_focused = _focused_effect == effect_index;
		const std::string effect_name = effect.source_file.filename().u8string();

		// Create separate tab for every effect file
		if (_variable_editor_tabs)
		{
			ImGuiTabItemFlags flags = 0;
			if (is_focused)
				flags |= ImGuiTabItemFlags_SetSelected;

			if (!ImGui::BeginTabItem(effect_name.c_str(), nullptr, flags))
				continue;
			// Begin a new child here so scrolling through variables does not move the tab itself too
			ImGui::BeginChild("##tab");
		}
		else
		{
			if (is_focused || _effects_expanded_state & 1)
				ImGui::SetNextItemOpen(is_focused || (_effects_expanded_state >> 1) != 0);

			if (!ImGui::TreeNodeEx(effect_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				continue; // Skip rendering invisible items
		}

		if (is_focused)
		{
			ImGui::SetScrollHereY(0.0f);
			_focused_effect = std::numeric_limits<size_t>::max();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(_imgui_context->Style.FramePadding.x, 0));
		if (imgui::popup_button(ICON_FK_UNDO " Reset all to default", _variable_editor_tabs ? ImGui::GetContentRegionAvail().x : ImGui::CalcItemWidth()))
		{
			ImGui::Text("Do you really want to reset all values in '%s' to their defaults?", effect_name.c_str());

			if (ImGui::Button("Yes", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				// Reset all uniform variables
				for (uniform &variable_it : effect.uniforms)
					if (variable_it.special == special_uniform::none)
						reset_uniform_value(variable_it);

				// Reset all preprocessor definitions
				for (const std::pair<std::string, std::string> &definition : effect.definitions)
					if (const auto preset_it = find_definition_value(_preset_preprocessor_definitions, definition.first);
						preset_it != _preset_preprocessor_definitions.end())
						force_reload_effect = true, // Need to reload after changing preprocessor defines so to get accurate defaults again
						_preset_preprocessor_definitions.erase(preset_it);

				save_current_preset();

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();

		bool category_closed = false;
		std::string current_category;
		auto modified_definition = _preset_preprocessor_definitions.end();

		size_t active_variable = 0;
		size_t active_variable_index = std::numeric_limits<size_t>::max();
		size_t hovered_variable = 0;
		size_t hovered_variable_index = std::numeric_limits<size_t>::max();

		for (size_t variable_index = 0; variable_index < effect.uniforms.size(); ++variable_index)
		{
			reshade::uniform &variable = effect.uniforms[variable_index];

			// Skip hidden and special variables
			if (variable.annotation_as_int("hidden") || variable.special != special_uniform::none)
			{
				if (variable.special == special_uniform::overlay_active)
					active_variable_index = variable_index;
				else if (variable.special == special_uniform::overlay_hovered)
					hovered_variable_index = variable_index;
				continue;
			}

			if (const std::string_view category = variable.annotation_as_string("ui_category");
				category != current_category)
			{
				current_category = category;

				if (!category.empty())
				{
					std::string category_label(category.data(), category.size());
					if (!_variable_editor_tabs)
						for (float x = 0, space_x = ImGui::CalcTextSize(" ").x, width = (ImGui::CalcItemWidth() - ImGui::CalcTextSize(category_label.data()).x - 45) / 2; x < width; x += space_x)
							category_label.insert(0, " ");

					ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_NoTreePushOnOpen;
					if (!variable.annotation_as_int("ui_category_closed"))
						flags |= ImGuiTreeNodeFlags_DefaultOpen;

					category_closed = !ImGui::TreeNodeEx(category_label.c_str(), flags);

					if (ImGui::BeginPopupContextItem(category_label.c_str()))
					{
						std::string reset_button_label(category.data(), category.size());
						reset_button_label = ICON_FK_UNDO " Reset all in '" + reset_button_label + "' to default";

						if (ImGui::Button(reset_button_label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)))
						{
							for (uniform &variable_it : effect.uniforms)
								if (variable_it.special == special_uniform::none &&
									variable_it.annotation_as_string("ui_category") == category)
									reset_uniform_value(variable_it);

							save_current_preset();

							ImGui::CloseCurrentPopup();
						}

						ImGui::EndPopup();
					}
				}
				else
				{
					category_closed = false;
				}
			}

			// Skip rendering invisible items
			if (category_closed)
				continue;

			// Add spacing before variable widget
			for (int i = 0, spacing = variable.annotation_as_int("ui_spacing"); i < spacing; ++i)
				ImGui::Spacing();

			// Add user-configurable text before variable widget
			if (const std::string_view text = variable.annotation_as_string("ui_text");
				!text.empty())
			{
				ImGui::PushTextWrapPos();
				ImGui::TextUnformatted(text.data());
				ImGui::PopTextWrapPos();
			}

			bool modified = false;
			std::string_view label = variable.annotation_as_string("ui_label");
			if (label.empty())
				label = variable.name;
			const std::string_view ui_type = variable.annotation_as_string("ui_type");

			ImGui::PushID(static_cast<int>(id++));

			switch (variable.type.base)
			{
			case reshadefx::type::t_bool:
			{
				bool data;
				get_uniform_value(variable, &data);

				if (ui_type == "combo")
					modified = imgui::combo_with_buttons(label.data(), data);
				else
					modified = ImGui::Checkbox(label.data(), &data);

				if (modified)
					set_uniform_value(variable, &data);
				break;
			}
			case reshadefx::type::t_int:
			case reshadefx::type::t_uint:
			{
				int data[16];
				get_uniform_value(variable, data, 16);

				const auto ui_min_val = variable.annotation_as_int("ui_min", 0, ui_type == "slider" ? 0 : std::numeric_limits<int>::lowest());
				const auto ui_max_val = variable.annotation_as_int("ui_max", 0, ui_type == "slider" ? 1 : std::numeric_limits<int>::max());
				const auto ui_stp_val = std::max(1, variable.annotation_as_int("ui_step"));

				if (ui_type == "slider")
					modified = imgui::slider_with_buttons(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val);
				else if (ui_type == "drag")
					modified = variable.annotation_as_int("ui_step") == 0 ?
						ImGui::DragScalarN(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, 1.0f, &ui_min_val, &ui_max_val) :
						imgui::drag_with_buttons(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val);
				else if (ui_type == "list")
					modified = imgui::list_with_buttons(label.data(), variable.annotation_as_string("ui_items"), data[0]);
				else if (ui_type == "combo")
					modified = imgui::combo_with_buttons(label.data(), variable.annotation_as_string("ui_items"), data[0]);
				else if (ui_type == "radio")
					modified = imgui::radio_list(label.data(), variable.annotation_as_string("ui_items"), data[0]);
				else if (variable.type.is_matrix())
					for (unsigned int row = 0; row < variable.type.rows; ++row)
						modified = ImGui::InputScalarN((std::string(label) + " [row " + std::to_string(row) + ']').c_str(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, &data[0] + row * variable.type.cols, variable.type.cols) || modified;
				else
					modified = ImGui::InputScalarN(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows);

				if (modified)
					set_uniform_value(variable, data, 16);
				break;
			}
			case reshadefx::type::t_float:
			{
				float data[16];
				get_uniform_value(variable, data, 16);

				const auto ui_min_val = variable.annotation_as_float("ui_min", 0, ui_type == "slider" ? 0.0f : std::numeric_limits<float>::lowest());
				const auto ui_max_val = variable.annotation_as_float("ui_max", 0, ui_type == "slider" ? 1.0f : std::numeric_limits<float>::max());
				const auto ui_stp_val = std::max(0.001f, variable.annotation_as_float("ui_step"));

				// Calculate display precision based on step value
				char precision_format[] = "%.0f";
				for (float x = 1.0f; x * ui_stp_val < 1.0f && precision_format[2] < '9'; x *= 10.0f)
					++precision_format[2]; // This changes the text to "%.1f", "%.2f", "%.3f", ...

				if (ui_type == "slider")
					modified = imgui::slider_with_buttons(label.data(), ImGuiDataType_Float, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, precision_format);
				else if (ui_type == "drag")
					modified = variable.annotation_as_float("ui_step") == 0 ?
						ImGui::DragScalarN(label.data(), ImGuiDataType_Float, data, variable.type.rows, ui_stp_val, &ui_min_val, &ui_max_val, precision_format) :
						imgui::drag_with_buttons(label.data(), ImGuiDataType_Float, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, precision_format);
				else if (ui_type == "color" && variable.type.rows == 1)
					modified = imgui::slider_for_alpha_value(label.data(), data);
				else if (ui_type == "color" && variable.type.rows == 3)
					modified = ImGui::ColorEdit3(label.data(), data, ImGuiColorEditFlags_NoOptions);
				else if (ui_type == "color" && variable.type.rows == 4)
					modified = ImGui::ColorEdit4(label.data(), data, ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaBar);
				else if (variable.type.is_matrix())
					for (unsigned int row = 0; row < variable.type.rows; ++row)
						modified = ImGui::InputScalarN((std::string(label) + " [row " + std::to_string(row) + ']').c_str(), ImGuiDataType_Float, &data[0] + row * variable.type.cols, variable.type.cols) || modified;
				else
					modified = ImGui::InputScalarN(label.data(), ImGuiDataType_Float, data, variable.type.rows);

				if (modified)
					set_uniform_value(variable, data, 16);
				break;
			}
			}

			if (ImGui::IsItemActive())
				active_variable = variable_index + 1;
			if (ImGui::IsItemHovered())
				hovered_variable = variable_index + 1;

			// Display tooltip
			if (const std::string_view tooltip = variable.annotation_as_string("ui_tooltip");
				!tooltip.empty() && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", tooltip.data());

			// Create context menu
			if (ImGui::BeginPopupContextItem("##context"))
			{
				if (variable.supports_toggle_key() &&
					imgui::key_input_box("##toggle_key", variable.toggle_key_data, *_input))
					modified = true;

				const float button_width = ImGui::CalcItemWidth();

				if (ImGui::Button(ICON_FK_UNDO " Reset to default", ImVec2(button_width, 0)))
				{
					modified = true;
					reset_uniform_value(variable);
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if (variable.toggle_key_data[0] != 0)
			{
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
				ImGui::TextDisabled("%s", input::key_name(variable.toggle_key_data).c_str());
			}

			ImGui::PopID();

			// A value has changed, so save the current preset
			if (modified)
				save_current_preset();
		}

		if (active_variable_index < effect.uniforms.size())
			set_uniform_value(effect.uniforms[active_variable_index], static_cast<uint32_t>(active_variable));
		if (hovered_variable_index < effect.uniforms.size())
			set_uniform_value(effect.uniforms[hovered_variable_index], static_cast<uint32_t>(hovered_variable));

		// Draw preprocessor definition list after all uniforms of an effect file
		if (!effect.definitions.empty() || !effect.preprocessed)
		{
			std::string category_label = "Preprocessor definitions";
			if (!_variable_editor_tabs)
				for (float x = 0, space_x = ImGui::CalcTextSize(" ").x, width = (ImGui::CalcItemWidth() - ImGui::CalcTextSize(category_label.c_str()).x - 45) / 2; x < width; x += space_x)
					category_label.insert(0, " ");

			ImGuiTreeNodeFlags tree_flags = ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (effect.preprocessed) // Do not open tree by default is not yet pre-processed, since that would case an immediate recompile
				tree_flags |= ImGuiTreeNodeFlags_DefaultOpen;

			if (ImGui::TreeNodeEx(category_label.c_str(), tree_flags))
			{
				if (!effect.preprocessed)
					force_reload_effect = true;

				for (const std::pair<std::string, std::string> &definition : effect.definitions)
				{
					char value[128] = "";
					const auto global_it = find_definition_value(_global_preprocessor_definitions, definition.first, value);
					const auto preset_it = find_definition_value(_preset_preprocessor_definitions, definition.first, value);

					if (global_it == _global_preprocessor_definitions.end() &&
						preset_it == _preset_preprocessor_definitions.end())
						definition.second.copy(value, sizeof(value) - 1); // Fill with default value

					if (ImGui::InputText(definition.first.c_str(), value, sizeof(value),
						global_it != _global_preprocessor_definitions.end() ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
					{
						if (value[0] == '\0') // An empty value removes the definition
						{
							if (preset_it != _preset_preprocessor_definitions.end())
							{
								force_reload_effect = true;
								_preset_preprocessor_definitions.erase(preset_it);
							}
						}
						else
						{
							force_reload_effect = true;

							if (preset_it != _preset_preprocessor_definitions.end())
							{
								*preset_it = definition.first + '=' + value;
								modified_definition = preset_it;
							}
							else
							{
								_preset_preprocessor_definitions.push_back(definition.first + '=' + value);
								modified_definition = _preset_preprocessor_definitions.end() - 1;
							}
						}
					}

					if (!force_reload_effect && // Cannot compare iterators if definitions were just modified above
						ImGui::BeginPopupContextItem())
					{
						const float button_width = ImGui::CalcItemWidth();

						if (ImGui::Button(ICON_FK_UNDO " Reset to default", ImVec2(button_width, 0)))
						{
							if (preset_it != _preset_preprocessor_definitions.end())
							{
								force_reload_effect = true;
								_preset_preprocessor_definitions.erase(preset_it);
							}

							ImGui::CloseCurrentPopup();
						}

						ImGui::EndPopup();
					}
				}
			}
		}

		if (_variable_editor_tabs)
		{
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		else
		{
			ImGui::TreePop();
		}

		if (force_reload_effect)
		{
			save_current_preset();

			const bool reload_successful_before = _last_reload_successfull;

			// Reload current effect file
			if (!reload_effect(effect_index, true) &&
				modified_definition != _preset_preprocessor_definitions.end())
			{
				// The preprocessor definition that was just modified caused the effect to not compile, so reset to default and try again
				_preset_preprocessor_definitions.erase(modified_definition);

				if (reload_effect(effect_index, true))
				{
					_last_reload_successfull = reload_successful_before;
					ImGui::OpenPopup("##pperror"); // Notify the user about this

					// Update preset again now, so that the removed preprocessor definition does not reappear on a reload
					// The preset is actually loaded again next frame to update the technique status (see 'update_and_render_effects'), so cannot use 'save_current_preset' here
					ini_file::load_cache(_current_preset_path).set({}, "PreprocessorDefinitions", _preset_preprocessor_definitions);
				}
			}

			// Reloading an effect file invalidates all textures, but the statistics window may already have drawn references to those, so need to reset it
			if (ImGuiWindow *const statistics_window = ImGui::FindWindowByName("Statistics"))
				statistics_window->DrawList->CmdBuffer.clear();
		}
	}

	if (ImGui::BeginPopup("##pperror"))
	{
		ImGui::TextColored(COLOR_RED, "The effect failed to compile after this change, so it was reverted back to the default.");
		ImGui::EndPopup();
	}

	if (_variable_editor_tabs)
		ImGui::EndTabBar();
	ImGui::EndChild();
}
void reshade::runtime::draw_technique_editor()
{
	if (!_last_reload_successfull)
	{
		// Add fake items at the top for effects that failed to compile
		for (size_t effect_index = 0; effect_index < _effects.size(); ++effect_index)
		{
			const reshade::effect &effect = _effects[effect_index];

			if (effect.compiled || effect.skipped)
				continue;

			ImGui::PushID(static_cast<int>(_techniques.size() + effect_index));

			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);

			const std::string label = '[' + effect.source_file.filename().u8string() + ']' + " failed to compile";

			bool value = false;
			ImGui::Checkbox(label.c_str(), &value);

			ImGui::PopStyleColor();
			ImGui::PopItemFlag();

			if (!effect.errors.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::BeginTooltip();
				ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);
				ImGui::TextUnformatted(effect.errors.c_str());
				ImGui::PopStyleColor();
				ImGui::EndTooltip();
			}

			if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::OpenPopup("##context");

			if (ImGui::BeginPopup("##context"))
			{
				if (ImGui::Button("Open folder in explorer", ImVec2(230.0f, 0)))
					open_explorer(effect.source_file);

				ImGui::Separator();

				if (imgui::popup_button(ICON_FK_PENCIL " Edit source code", 230.0f))
				{
					std::filesystem::path source_file;
					if (ImGui::MenuItem(effect.source_file.filename().u8string().c_str()))
						source_file = effect.source_file;

					if (!effect.included_files.empty())
					{
						ImGui::Separator();

						for (const std::filesystem::path &included_file : effect.included_files)
							if (ImGui::MenuItem(included_file.filename().u8string().c_str()))
								source_file = included_file;
					}

					ImGui::EndPopup();

					if (!source_file.empty())
					{
						open_code_editor(effect_index, source_file);
						ImGui::CloseCurrentPopup();
					}
				}

				if (!effect.module.hlsl.empty() && // Hide if using SPIR-V, since that cannot easily be shown here
					imgui::popup_button("Show compiled results", 230.0f))
				{
					std::string entry_point_name;
					if (ImGui::MenuItem("Generated code"))
						entry_point_name = "Generated code";
					ImGui::EndPopup();

					if (!entry_point_name.empty())
					{
						open_code_editor(effect_index, entry_point_name);
						ImGui::CloseCurrentPopup();
					}
				}

				ImGui::EndPopup();
			}

			ImGui::PopID();
		}
	}

	size_t force_reload_effect = std::numeric_limits<size_t>::max();
	size_t hovered_technique_index = std::numeric_limits<size_t>::max();

	for (size_t index = 0; index < _techniques.size(); ++index)
	{
		reshade::technique &tech = _techniques[index];

		// Skip hidden techniques
		if (tech.hidden || !_effects[tech.effect_index].compiled)
			continue;

		ImGui::PushID(static_cast<int>(index));

		// Look up effect that contains this technique
		const reshade::effect &effect = _effects[tech.effect_index];

		// Draw border around the item if it is selected
		const bool draw_border = _selected_technique == index;
		if (draw_border)
			ImGui::Separator();

		// Prevent user from disabling the technique when it is set to always be enabled via annotation
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, tech.annotation_as_int("enabled"));
		// Gray out disabled techniques and mark those with warnings yellow
		ImGui::PushStyleColor(ImGuiCol_Text,
			effect.errors.empty() || tech.enabled ?
				_imgui_context->Style.Colors[tech.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled] : COLOR_YELLOW);

		std::string label(tech.annotation_as_string("ui_label"));
		if (label.empty())
			label = tech.name;
		label += " [" + effect.source_file.filename().u8string() + ']';

		if (bool status = tech.enabled;
			ImGui::Checkbox(label.c_str(), &status))
		{
			if (status)
				enable_technique(tech);
			else
				disable_technique(tech);
			save_current_preset();
		}

		ImGui::PopStyleColor();
		ImGui::PopItemFlag();

		if (ImGui::IsItemActive())
			_selected_technique = index;
		if (ImGui::IsItemClicked())
			_focused_effect = tech.effect_index;
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly | ImGuiHoveredFlags_AllowWhenDisabled))
			hovered_technique_index = index;

		// Display tooltip
		if (const std::string_view tooltip = tech.annotation_as_string("ui_tooltip");
			ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && (!tooltip.empty() || !effect.errors.empty()))
		{
			ImGui::BeginTooltip();
			if (!tooltip.empty())
			{
				ImGui::TextUnformatted(tooltip.data());
				ImGui::Spacing();
			}
			if (!effect.errors.empty())
			{
				ImGui::PushStyleColor(ImGuiCol_Text, COLOR_YELLOW);
				ImGui::TextUnformatted(effect.errors.c_str());
				ImGui::PopStyleColor();
			}
			ImGui::EndTooltip();
		}

		// Create context menu
		if (ImGui::BeginPopupContextItem("##context"))
		{
			ImGui::TextUnformatted(tech.name.c_str());
			ImGui::Separator();

			ImGui::SetNextItemWidth(230.0f);
			if (imgui::key_input_box("##toggle_key", tech.toggle_key_data, *_input))
				save_current_preset();

			const bool is_not_top = index > 0;
			const bool is_not_bottom = index < _techniques.size() - 1;

			if (is_not_top && ImGui::Button("Move to top", ImVec2(230.0f, 0)))
			{
				_techniques.insert(_techniques.begin(), std::move(_techniques[index]));
				_techniques.erase(_techniques.begin() + 1 + index);
				save_current_preset();
				ImGui::CloseCurrentPopup();
			}
			if (is_not_bottom && ImGui::Button("Move to bottom", ImVec2(230.0f, 0)))
			{
				_techniques.push_back(std::move(_techniques[index]));
				_techniques.erase(_techniques.begin() + index);
				save_current_preset();
				ImGui::CloseCurrentPopup();
			}

			ImGui::Separator();

			if (ImGui::Button("Open folder in explorer", ImVec2(230.0f, 0)))
				open_explorer(effect.source_file);

			ImGui::Separator();

			if (imgui::popup_button(ICON_FK_PENCIL " Edit source code", 230.0f))
			{
				std::filesystem::path source_file;
				if (ImGui::MenuItem(effect.source_file.filename().u8string().c_str()))
					source_file = effect.source_file;

				if (!effect.preprocessed)
				{
					// Force preprocessor to run to update included files
					force_reload_effect = tech.effect_index;
				}
				else if (!effect.included_files.empty())
				{
					ImGui::Separator();

					for (const std::filesystem::path &included_file : effect.included_files)
						if (ImGui::MenuItem(included_file.filename().u8string().c_str()))
							source_file = included_file;
				}

				ImGui::EndPopup();

				if (!source_file.empty())
				{
					open_code_editor(tech.effect_index, source_file);
					ImGui::CloseCurrentPopup();
				}
			}

			if (!effect.module.hlsl.empty() && // Hide if using SPIR-V, since that cannot easily be shown here
				imgui::popup_button("Show compiled results", 230.0f))
			{
				std::string entry_point_name;
				if (ImGui::MenuItem("Generated code"))
					entry_point_name = "Generated code";

				if (!effect.assembly.empty())
				{
					ImGui::Separator();

					for (const reshadefx::entry_point &entry_point : effect.module.entry_points)
						if (const auto assembly_it = effect.assembly.find(entry_point.name);
							assembly_it != effect.assembly.end() && ImGui::MenuItem(entry_point.name.c_str()))
							entry_point_name = entry_point.name;
				}

				ImGui::EndPopup();

				if (!entry_point_name.empty())
				{
					open_code_editor(tech.effect_index, entry_point_name);
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		if (tech.toggle_key_data[0] != 0)
		{
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
			ImGui::TextDisabled("%s", input::key_name(tech.toggle_key_data).c_str());
		}

		if (draw_border)
			ImGui::Separator();

		ImGui::PopID();
	}

	// Move the selected technique to the position of the mouse in the list
	if (_selected_technique < _techniques.size() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		if (hovered_technique_index < _techniques.size() && hovered_technique_index != _selected_technique)
		{
			const auto move_technique = [this](size_t from_index, size_t to_index) {
				if (to_index < from_index) // Up
					for (size_t i = from_index; to_index < i; --i)
						std::swap(_techniques[i - 1], _techniques[i]);
				else // Down
					for (size_t i = from_index; i < to_index; ++i)
						std::swap(_techniques[i], _techniques[i + 1]);
			};

			move_technique(_selected_technique, hovered_technique_index);

			// Pressing shift moves all techniques from the same effect file to the new location as well
			if (_imgui_context->IO.KeyShift)
			{
				for (size_t i = hovered_technique_index + 1, offset = 1; i < _techniques.size(); ++i)
				{
					if (_techniques[i].effect_index == _focused_effect)
					{
						if ((i - hovered_technique_index) > offset)
							move_technique(i, hovered_technique_index + offset);
						offset++;
					}
				}
				for (size_t i = hovered_technique_index - 1, offset = 0; i >= 0 && i != std::numeric_limits<size_t>::max(); --i)
				{
					if (_techniques[i].effect_index == _focused_effect)
					{
						offset++;
						if ((hovered_technique_index - i) > offset)
							move_technique(i, hovered_technique_index - offset);
					}
				}
			}

			_selected_technique = hovered_technique_index;
			save_current_preset();
			return;
		}
	}
	else
	{
		_selected_technique = std::numeric_limits<size_t>::max();
	}

	if (force_reload_effect != std::numeric_limits<size_t>::max())
	{
		reload_effect(force_reload_effect, true);

		// Reloading an effect file invalidates all textures, but the statistics window may already have drawn references to those, so need to reset it
		if (ImGuiWindow *const statistics_window = ImGui::FindWindowByName("Statistics"))
			statistics_window->DrawList->CmdBuffer.clear();
	}
}

void reshade::runtime::open_code_editor(size_t effect_index, const std::string &entry_point)
{
	assert(effect_index < _effects.size());
	const std::filesystem::path path = _effects[effect_index].source_file;

	auto it = std::find_if(_editors.begin(), _editors.end(), [&](const auto &instance) {
		return instance.effect_index == effect_index && instance.file_path == path && instance.entry_point_name == entry_point; });
	if (it == _editors.end())
	{
		_editors.push_back({ effect_index, path, entry_point });
		it = std::prev(_editors.end());
	}

	it->selected = true;

	open_code_editor(*it);
}
void reshade::runtime::open_code_editor(size_t effect_index, const std::filesystem::path &path)
{
	assert(effect_index < _effects.size());

	auto it = std::find_if(_editors.begin(), _editors.end(), [&](const auto &instance) {
		return instance.effect_index == effect_index && instance.file_path == path && instance.entry_point_name.empty(); });
	if (it == _editors.end())
	{
		_editors.push_back({ effect_index, path });
		it = std::prev(_editors.end());
	}

	it->selected = true;

	open_code_editor(*it);
}
void reshade::runtime::open_code_editor(editor_instance &instance)
{
	const effect &effect = _effects[instance.effect_index];

	if (!instance.entry_point_name.empty())
	{
		instance.editor.set_text(instance.entry_point_name == "Generated code" ?
			effect.module.hlsl : effect.assembly.at(instance.entry_point_name).second);
		instance.editor.set_readonly(true);
	}
	else if (!instance.editor.is_modified())
	{
		// Only update text if there is no undo history (in which case it can be assumed that the text is already up-to-date)
		if (!instance.editor.can_undo())
		{
			instance.editor.set_text(
				std::string(std::istreambuf_iterator<char>(std::ifstream(instance.file_path).rdbuf()), std::istreambuf_iterator<char>()));
			instance.editor.set_readonly(false);
		}

		instance.editor.clear_errors();

		for (size_t offset = 0, next; offset != std::string::npos; offset = next)
		{
			const size_t pos_error = effect.errors.find(": ", offset);
			const size_t pos_error_line = effect.errors.rfind('(', pos_error); // Paths can contain '(', but no ": ", so search backwards from th error location to find the line info
			if (pos_error == std::string::npos || pos_error_line == std::string::npos)
				break;

			const size_t pos_linefeed = effect.errors.find('\n', pos_error);

			next = pos_linefeed != std::string::npos ? pos_linefeed + 1 : std::string::npos;

			// Ignore errors that aren't in the current source file
			if (const std::string_view error_file(effect.errors.c_str() + offset, pos_error_line - offset);
				error_file != instance.file_path.u8string())
				continue;

			const int error_line = std::strtol(effect.errors.c_str() + pos_error_line + 1, nullptr, 10);
			const std::string error_text = effect.errors.substr(pos_error + 2 /* skip space */, pos_linefeed - pos_error - 2);

			instance.editor.add_error(error_line, error_text, error_text.find("warning") != std::string::npos);
		}
	}
}
void reshade::runtime::draw_code_editor(editor_instance &instance)
{
	if (instance.entry_point_name.empty() && (
		ImGui::Button(ICON_FK_FLOPPY " Save", ImVec2(ImGui::GetContentRegionAvail().x, 0)) || _input->is_key_pressed('S', true, false, false)))
	{
		// Write current editor text to file
		const std::string text = instance.editor.get_text();
		std::ofstream(instance.file_path, std::ios::trunc).write(text.c_str(), text.size());

		if (!is_loading() && instance.effect_index < _effects.size())
		{
			// Clear modified flag, so that errors are updated next frame (see 'update_and_render_effects')
			instance.editor.clear_modified();

			reload_effect(instance.effect_index);

			// Reloading an effect file invalidates all textures, but the statistics window may already have drawn references to those, so need to reset it
			if (ImGuiWindow *const statistics_window = ImGui::FindWindowByName("Statistics"))
				statistics_window->DrawList->CmdBuffer.clear();
		}
	}

	instance.editor.render("##editor", _editor_palette, false, _imgui_context->IO.Fonts->Fonts[1]);

	// Disable keyboard shortcuts when the window is focused so they don't get triggered while editing text
	const bool is_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
	_ignore_shortcuts |= is_focused;

	// Disable keyboard navigation starting with next frame when editor is focused so that the Alt key can be used without it switching focus to the menu bar
	if (is_focused)
		_imgui_context->IO.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
	else // Enable navigation again if focus is lost
		_imgui_context->IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
}
#endif

bool reshade::runtime::init_imgui_resources()
{
	const bool has_combined_sampler_and_view = _device->check_capability(api::device_caps::sampler_with_resource_view);

	if (_imgui_sampler_state == 0)
	{
		api::sampler_desc sampler_desc = {};
		sampler_desc.filter = api::filter_mode::min_mag_mip_linear;
		sampler_desc.address_u = api::texture_address_mode::wrap;
		sampler_desc.address_v = api::texture_address_mode::wrap;
		sampler_desc.address_w = api::texture_address_mode::wrap;

		if (!_device->create_sampler(sampler_desc, &_imgui_sampler_state))
		{
			LOG(ERROR) << "Failed to create ImGui sampler object!";
			return false;
		}
	}

	if (_imgui_pipeline_layout == 0)
	{
		uint32_t num_layout_params = 0;
		api::pipeline_layout_param layout_params[3];

		if (has_combined_sampler_and_view)
		{
			layout_params[num_layout_params++] = api::descriptor_range { 0, 0, 0, 1, api::shader_stage::pixel, 1, api::descriptor_type::sampler_with_resource_view }; // s0
		}
		else
		{
			layout_params[num_layout_params++] = api::descriptor_range { 0, 0, 0, 1, api::shader_stage::pixel, 1, api::descriptor_type::sampler }; // s0
			layout_params[num_layout_params++] = api::descriptor_range { 0, 0, 0, 1, api::shader_stage::pixel, 1, api::descriptor_type::shader_resource_view }; // t0
		}

		layout_params[num_layout_params++] = api::constant_range { 0, 0, 0, 16, api::shader_stage::vertex }; // b0

		if (!_device->create_pipeline_layout(num_layout_params, layout_params, &_imgui_pipeline_layout))
		{
			LOG(ERROR) << "Failed to create ImGui pipeline layout!";
			return false;
		}
	}

	if (_imgui_pipeline != 0)
		return true;

	api::shader_desc vs_desc, ps_desc;

	if ((_renderer_id & 0xF0000) == 0 || _renderer_id >= 0x20000)
	{
		const resources::data_resource vs_res = resources::load_data_resource(_renderer_id >= 0x20000 ? IDR_IMGUI_VS_SPIRV : _renderer_id < 0xa000 ? IDR_IMGUI_VS_3_0 : IDR_IMGUI_VS_4_0);
		vs_desc.code = vs_res.data;
		vs_desc.code_size = vs_res.data_size;

		const resources::data_resource ps_res = resources::load_data_resource(_renderer_id >= 0x20000 ? IDR_IMGUI_PS_SPIRV : _renderer_id < 0xa000 ? IDR_IMGUI_PS_3_0 : IDR_IMGUI_PS_4_0);
		ps_desc.code = ps_res.data;
		ps_desc.code_size = ps_res.data_size;
	}
	else
	{
		assert(_device->get_api() == api::device_api::opengl);

		// These need to be static so that the shader source memory doesn't fall out of scope before pipeline creation below
		static constexpr char vertex_shader_code[] =
			"#version 430\n"
			"layout(binding = 0) uniform Buf { mat4 proj; };\n"
			"layout(location = 0) in vec2 pos;\n"
			"layout(location = 1) in vec2 tex;\n"
			"layout(location = 2) in vec4 col;\n"
			"out vec4 frag_col;\n"
			"out vec2 frag_tex;\n"
			"void main()\n"
			"{\n"
			"	frag_col = col;\n"
			"	frag_tex = tex;\n"
			"	gl_Position = proj * vec4(pos.xy, 0, 1);\n"
			"}\n";
		static constexpr char fragment_shader_code[] =
			"#version 430\n"
			"layout(binding = 0) uniform sampler2D s0;\n"
			"in vec4 frag_col;\n"
			"in vec2 frag_tex;\n"
			"out vec4 col;\n"
			"void main()\n"
			"{\n"
			"	col = frag_col * texture(s0, frag_tex.st);\n"
			"}\n";

		vs_desc.code = vertex_shader_code;
		vs_desc.code_size = sizeof(vertex_shader_code);
		vs_desc.entry_point = "main";

		ps_desc.code = fragment_shader_code;
		ps_desc.code_size = sizeof(fragment_shader_code);
		ps_desc.entry_point = "main";
	}

	std::vector<api::pipeline_subobject> subobjects;
	subobjects.push_back({ api::pipeline_subobject_type::vertex_shader, 1, &vs_desc });
	subobjects.push_back({ api::pipeline_subobject_type::pixel_shader, 1, &ps_desc });

	const api::input_element input_layout[3] = {
		{ 0, "POSITION", 0, api::format::r32g32_float,   0, offsetof(ImDrawVert, pos), sizeof(ImDrawVert), 0 },
		{ 1, "TEXCOORD", 0, api::format::r32g32_float,   0, offsetof(ImDrawVert, uv ), sizeof(ImDrawVert), 0 },
		{ 2, "COLOR",    0, api::format::r8g8b8a8_unorm, 0, offsetof(ImDrawVert, col), sizeof(ImDrawVert), 0 }
	};

	subobjects.push_back({ api::pipeline_subobject_type::input_layout, 3, (void *)input_layout });

	api::blend_desc blend_state;
	blend_state.blend_enable[0] = true;
	blend_state.source_color_blend_factor[0] = api::blend_factor::source_alpha;
	blend_state.dest_color_blend_factor[0] = api::blend_factor::one_minus_source_alpha;
	blend_state.color_blend_op[0] = api::blend_op::add;
	blend_state.source_alpha_blend_factor[0] = api::blend_factor::one;
	blend_state.dest_alpha_blend_factor[0] = api::blend_factor::one_minus_source_alpha;
	blend_state.alpha_blend_op[0] = api::blend_op::add;
	blend_state.render_target_write_mask[0] = 0xF;

	subobjects.push_back({ api::pipeline_subobject_type::blend_state, 1, &blend_state });

	api::rasterizer_desc rasterizer_state;
	rasterizer_state.cull_mode = api::cull_mode::none;
	rasterizer_state.scissor_enable = true;

	subobjects.push_back({ api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_state });

	api::depth_stencil_desc depth_stencil_state;
	depth_stencil_state.depth_enable = false;
	depth_stencil_state.stencil_enable = false;

	subobjects.push_back({ api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_state });

	subobjects.push_back({ api::pipeline_subobject_type::render_target_formats, 1, &_back_buffer_format });

	if (_device->create_pipeline(_imgui_pipeline_layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &_imgui_pipeline))
	{
		return true;
	}
	else
	{
		LOG(ERROR) << "Failed to create ImGui pipeline!";
		return false;
	}
}
void reshade::runtime::render_imgui_draw_data(api::command_list *cmd_list, ImDrawData *draw_data, api::resource_view rtv)
{
	// Need to multi-buffer vertex data so not to modify data below when the previous frame is still in flight
	const size_t buffer_index = _framecount % std::size(_imgui_vertices);

	// Create and grow vertex/index buffers if needed
	if (_imgui_num_indices[buffer_index] < draw_data->TotalIdxCount)
	{
		_graphics_queue->wait_idle(); // Be safe and ensure nothing still uses this buffer

		if (_imgui_indices[buffer_index] != 0)
			_device->destroy_resource(_imgui_indices[buffer_index]);

		const int new_size = draw_data->TotalIdxCount + 10000;
		if (!_device->create_resource(api::resource_desc(new_size * sizeof(ImDrawIdx), api::memory_heap::cpu_to_gpu, api::resource_usage::index_buffer), nullptr, api::resource_usage::cpu_access, &_imgui_indices[buffer_index]))
		{
			LOG(ERROR) << "Failed to create ImGui index buffer!";
			return;
		}

		_device->set_resource_name(_imgui_indices[buffer_index], "ImGui index buffer");

		_imgui_num_indices[buffer_index] = new_size;
	}
	if (_imgui_num_vertices[buffer_index] < draw_data->TotalVtxCount)
	{
		_graphics_queue->wait_idle();

		if (_imgui_vertices[buffer_index] != 0)
			_device->destroy_resource(_imgui_vertices[buffer_index]);

		const int new_size = draw_data->TotalVtxCount + 5000;
		if (!_device->create_resource(api::resource_desc(new_size * sizeof(ImDrawVert), api::memory_heap::cpu_to_gpu, api::resource_usage::vertex_buffer), nullptr, api::resource_usage::cpu_access, &_imgui_vertices[buffer_index]))
		{
			LOG(ERROR) << "Failed to create ImGui vertex buffer!";
			return;
		}

		_device->set_resource_name(_imgui_vertices[buffer_index], "ImGui vertex buffer");

		_imgui_num_vertices[buffer_index] = new_size;
	}

	if (ImDrawIdx *idx_dst;
		_device->map_buffer_region(_imgui_indices[buffer_index], 0, UINT64_MAX, api::map_access::write_only, reinterpret_cast<void **>(&idx_dst)))
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			idx_dst += draw_list->IdxBuffer.Size;
		}

		_device->unmap_buffer_region(_imgui_indices[buffer_index]);
	}
	if (ImDrawVert *vtx_dst;
		_device->map_buffer_region(_imgui_vertices[buffer_index], 0, UINT64_MAX, api::map_access::write_only, reinterpret_cast<void **>(&vtx_dst)))
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
			vtx_dst += draw_list->VtxBuffer.Size;
		}

		_device->unmap_buffer_region(_imgui_vertices[buffer_index]);
	}

	api::render_pass_render_target_desc render_target = {};
	render_target.view = rtv;

	cmd_list->begin_render_pass(1, &render_target, nullptr);

	// Setup render state
	cmd_list->bind_pipeline(api::pipeline_stage::all_graphics, _imgui_pipeline);

	cmd_list->bind_index_buffer(_imgui_indices[buffer_index], 0, sizeof(ImDrawIdx));
	cmd_list->bind_vertex_buffer(0, _imgui_vertices[buffer_index], 0, sizeof(ImDrawVert));

	const api::viewport viewport = { 0, 0, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.0f, 1.0f };
	cmd_list->bind_viewports(0, 1, &viewport);

	// Setup orthographic projection matrix
	const bool flip_y = (_renderer_id & 0x10000) != 0 && !_is_vr;
	const bool adjust_half_pixel = _renderer_id < 0xa000; // Bake half-pixel offset into matrix in D3D9
	const bool depth_clip_zero_to_one = (_renderer_id & 0x10000) == 0;

	const float ortho_projection[16] = {
		2.0f / draw_data->DisplaySize.x, 0.0f, 0.0f, 0.0f,
		0.0f, (flip_y ? 2.0f : -2.0f) / draw_data->DisplaySize.y, 0.0f, 0.0f,
		0.0f,                            0.0f, depth_clip_zero_to_one ? 0.5f : -1.0f, 0.0f,
		                   -(2 * draw_data->DisplayPos.x + draw_data->DisplaySize.x + (adjust_half_pixel ? 1.0f : 0.0f)) / draw_data->DisplaySize.x,
		(flip_y ? -1 : 1) * (2 * draw_data->DisplayPos.y + draw_data->DisplaySize.y + (adjust_half_pixel ? 1.0f : 0.0f)) / draw_data->DisplaySize.y, depth_clip_zero_to_one ? 0.5f : 0.0f, 1.0f,
	};

	const bool has_combined_sampler_and_view = _device->check_capability(api::device_caps::sampler_with_resource_view);
	cmd_list->push_constants(api::shader_stage::vertex, _imgui_pipeline_layout, has_combined_sampler_and_view ? 1 : 2, 0, sizeof(ortho_projection) / 4, ortho_projection);
	if (!has_combined_sampler_and_view)
		cmd_list->push_descriptors(api::shader_stage::pixel, _imgui_pipeline_layout, 0, api::descriptor_set_update { {}, 0, 0, 1, api::descriptor_type::sampler, &_imgui_sampler_state });

	int vtx_offset = 0, idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; ++n)
	{
		const ImDrawList *const draw_list = draw_data->CmdLists[n];

		for (const ImDrawCmd &cmd : draw_list->CmdBuffer)
		{
			assert(cmd.TextureId != 0);
			assert(cmd.UserCallback == nullptr);

			const api::rect scissor_rect = {
				static_cast<int32_t>(cmd.ClipRect.x - draw_data->DisplayPos.x),
				flip_y ? static_cast<int32_t>(_height - cmd.ClipRect.w + draw_data->DisplayPos.y) : static_cast<int32_t>(cmd.ClipRect.y - draw_data->DisplayPos.y),
				static_cast<int32_t>(cmd.ClipRect.z - draw_data->DisplayPos.x),
				flip_y ? static_cast<int32_t>(_height - cmd.ClipRect.y + draw_data->DisplayPos.y) : static_cast<int32_t>(cmd.ClipRect.w - draw_data->DisplayPos.y)
			};

			cmd_list->bind_scissor_rects(0, 1, &scissor_rect);

			const api::resource_view srv = { (uint64_t)cmd.TextureId };
			if (has_combined_sampler_and_view)
			{
				api::sampler_with_resource_view sampler_and_view = { _imgui_sampler_state, srv };
				cmd_list->push_descriptors(api::shader_stage::pixel, _imgui_pipeline_layout, 0, api::descriptor_set_update { {}, 0, 0, 1, api::descriptor_type::sampler_with_resource_view, &sampler_and_view });
			}
			else
			{
				cmd_list->push_descriptors(api::shader_stage::pixel, _imgui_pipeline_layout, 1, api::descriptor_set_update { {}, 0, 0, 1, api::descriptor_type::shader_resource_view, &srv });
			}

			cmd_list->draw_indexed(cmd.ElemCount, 1, cmd.IdxOffset + idx_offset, cmd.VtxOffset + vtx_offset, 0);
		}

		idx_offset += draw_list->IdxBuffer.Size;
		vtx_offset += draw_list->VtxBuffer.Size;
	}

	cmd_list->end_render_pass();
}
void reshade::runtime::destroy_imgui_resources()
{
	_device->destroy_resource(_font_atlas_tex);
	_font_atlas_tex = {};
	_device->destroy_resource_view(_font_atlas_srv);
	_font_atlas_srv = {};
	_rebuild_font_atlas = true;

	for (size_t i = 0; i < std::size(_imgui_vertices); ++i)
	{
		_device->destroy_resource(_imgui_indices[i]);
		_imgui_indices[i] = {};
		_imgui_num_indices[i] = 0;
		_device->destroy_resource(_imgui_vertices[i]);
		_imgui_vertices[i] = {};
		_imgui_num_vertices[i] = 0;
	}

	_device->destroy_sampler(_imgui_sampler_state);
	_imgui_sampler_state = {};
	_device->destroy_pipeline(_imgui_pipeline);
	_imgui_pipeline = {};
	_device->destroy_pipeline_layout(_imgui_pipeline_layout);
	_imgui_pipeline_layout = {};
}

#endif
