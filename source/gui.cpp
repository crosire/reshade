/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_GUI

#include "log.hpp"
#include "version.h"
#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "input.hpp"
#include "ini_file.hpp"
#include "gui_widgets.hpp"
#include <assert.h>
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>

extern volatile long g_network_traffic;
extern std::filesystem::path g_reshade_dll_path;
extern std::filesystem::path g_target_executable_path;
static char g_reshadegui_ini_path[260 * 3] = {};

const ImVec4 COLOR_RED = ImColor(240, 100, 100);
const ImVec4 COLOR_YELLOW = ImColor(204, 204, 0);

void reshade::runtime::init_ui()
{
	(g_reshade_dll_path.parent_path() / "ReShadeGUI.ini").u8string()
		.copy(g_reshadegui_ini_path, sizeof(g_reshadegui_ini_path));

	// Default shortcut: Home
	_menu_key_data[0] = 0x24;
	_menu_key_data[1] = false;
	_menu_key_data[2] = false;
	_menu_key_data[3] = false;

	_variable_editor_height = 300;

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
	imgui_io.BackendFlags = ImGuiBackendFlags_HasMouseCursors;

	// Disable rounding by default
	imgui_style.GrabRounding = 0.0f;
	imgui_style.FrameRounding = 0.0f;
	imgui_style.ChildRounding = 0.0f;
	imgui_style.ScrollbarRounding = 0.0f;
	imgui_style.WindowRounding = 0.0f;
	imgui_style.WindowBorderSize = 0.0f;

	ImGui::SetCurrentContext(nullptr);

	subscribe_to_ui("Home", [this]() { draw_overlay_menu_home(); });
	subscribe_to_ui("Settings", [this]() { draw_overlay_menu_settings(); });
	subscribe_to_ui("Statistics", [this]() { draw_overlay_menu_statistics(); });
	subscribe_to_ui("Log", [this]() { draw_overlay_menu_log(); });
	subscribe_to_ui("About", [this]() { draw_overlay_menu_about(); });

	_load_config_callables.push_back([this](const ini_file &config) {
		bool save_imgui_window_state = false;

		config.get("INPUT", "KeyMenu", _menu_key_data);
		config.get("INPUT", "InputProcessing", _input_processing_mode);

		config.get("GENERAL", "ShowClock", _show_clock);
		config.get("GENERAL", "ShowFPS", _show_fps);
		config.get("GENERAL", "ShowFrameTime", _show_frametime);
		config.get("GENERAL", "ShowScreenshotMessage", _show_screenshot_message);
		config.get("GENERAL", "ClockFormat", _clock_format);
		config.get("GENERAL", "NoFontScaling", _no_font_scaling);
		config.get("GENERAL", "SaveWindowState", save_imgui_window_state);
		config.get("GENERAL", "TutorialProgress", _tutorial_index);
		config.get("GENERAL", "NewVariableUI", _variable_editor_tabs);

		config.get("STYLE", "Alpha", _imgui_context->Style.Alpha);
		config.get("STYLE", "GrabRounding", _imgui_context->Style.GrabRounding);
		config.get("STYLE", "FrameRounding", _imgui_context->Style.FrameRounding);
		config.get("STYLE", "ChildRounding", _imgui_context->Style.ChildRounding);
		config.get("STYLE", "PopupRounding", _imgui_context->Style.PopupRounding);
		config.get("STYLE", "WindowRounding", _imgui_context->Style.WindowRounding);
		config.get("STYLE", "ScrollbarRounding", _imgui_context->Style.ScrollbarRounding);
		config.get("STYLE", "TabRounding", _imgui_context->Style.TabRounding);
		config.get("STYLE", "FPSScale", _fps_scale);
		config.get("STYLE", "ColFPSText", _fps_col);
		config.get("STYLE", "Font", _font);
		config.get("STYLE", "FontSize", _font_size);
		config.get("STYLE", "EditorFont", _editor_font);
		config.get("STYLE", "EditorFontSize", _editor_font_size);
		config.get("STYLE", "StyleIndex", _style_index);
		config.get("STYLE", "EditorStyleIndex", _editor_style_index);

		_imgui_context->IO.IniFilename = save_imgui_window_state ? g_reshadegui_ini_path : nullptr;

		// For compatibility with older versions, set the alpha value if it is missing
		if (_fps_col[3] == 0.0f) _fps_col[3] = 1.0f;

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
		case 0:
			_editor.set_palette({ // Dark
				0xffffffff, 0xffd69c56, 0xff00ff00, 0xff7070e0, 0xffffffff, 0xff409090, 0xffaaaaaa,
				0xff9bc64d, 0xffc040a0, 0xff206020, 0xff406020, 0xff101010, 0xffe0e0e0, 0x80a06020,
				0x800020ff, 0x8000ffff, 0xff707000, 0x40000000, 0x40808080, 0x40a0a0a0 });
			break;
		case 1:
			_editor.set_palette({ // Light
				0xff000000, 0xffff0c06, 0xff008000, 0xff2020a0, 0xff000000, 0xff409090, 0xff404040,
				0xff606010, 0xffc040a0, 0xff205020, 0xff405020, 0xffffffff, 0xff000000, 0x80600000,
				0xa00010ff, 0x8000ffff, 0xff505000, 0x40000000, 0x40808080, 0x40000000 });
			break;
		case 3:
			_editor.set_palette({ // Solarized Dark
				0xff969483, 0xff0089b5, 0xff98a12a, 0xff98a12a, 0xff969483, 0xff164bcb, 0xff969483,
				0xff969483, 0xffc4716c, 0xff756e58, 0xff756e58, 0xff362b00, 0xff969483, 0xA0756e58,
				0x7f2f32dc, 0x7f0089b5, 0xff756e58, 0x7f423607, 0x7f423607, 0x7f423607 });
			break;
		case 4:
			_editor.set_palette({ // Solarized Light
				0xff837b65, 0xff0089b5, 0xff98a12a, 0xff98a12a, 0xff756e58, 0xff164bcb, 0xff837b65,
				0xff837b65, 0xffc4716c, 0xffa1a193, 0xffa1a193, 0xffe3f6fd, 0xff837b65, 0x60a1a193,
				0x7f2f32dc, 0x7f0089b5, 0xffa1a193, 0x7fd5e8ee, 0x7fd5e8ee, 0x7fd5e8ee });
			break;
		default:
		case 2:
			ImVec4 value; // Note: This expects that all colors exist in the config
			for (ImGuiCol i = 0; i < imgui_code_editor::color_palette_max; i++)
				config.get("STYLE", imgui_code_editor::get_palette_color_name(i), (float(&)[4])value),
				_editor.get_palette_index(i) = ImGui::ColorConvertFloat4ToU32(value);
			break;
		}
	});
	_save_config_callables.push_back([this](ini_file &config) {
		config.set("INPUT", "KeyMenu", _menu_key_data);
		config.set("INPUT", "InputProcessing", _input_processing_mode);

		config.set("GENERAL", "ShowClock", _show_clock);
		config.set("GENERAL", "ShowFPS", _show_fps);
		config.set("GENERAL", "ShowFrameTime", _show_frametime);
		config.set("GENERAL", "ShowScreenshotMessage", _show_screenshot_message);
		config.set("GENERAL", "ClockFormat", _clock_format);
		config.set("GENERAL", "NoFontScaling", _no_font_scaling);
		config.set("GENERAL", "SaveWindowState", _imgui_context->IO.IniFilename != nullptr);
		config.set("GENERAL", "TutorialProgress", _tutorial_index);
		config.set("GENERAL", "NewVariableUI", _variable_editor_tabs);

		config.set("STYLE", "Alpha", _imgui_context->Style.Alpha);
		config.set("STYLE", "GrabRounding", _imgui_context->Style.GrabRounding);
		config.set("STYLE", "FrameRounding", _imgui_context->Style.FrameRounding);
		config.set("STYLE", "ChildRounding", _imgui_context->Style.ChildRounding);
		config.set("STYLE", "PopupRounding", _imgui_context->Style.PopupRounding);
		config.set("STYLE", "WindowRounding", _imgui_context->Style.WindowRounding);
		config.set("STYLE", "ScrollbarRounding", _imgui_context->Style.ScrollbarRounding);
		config.set("STYLE", "TabRounding", _imgui_context->Style.TabRounding);
		config.set("STYLE", "FPSScale", _fps_scale);
		config.set("STYLE", "ColFPSText", _fps_col);
		config.set("STYLE", "Font", _font);
		config.set("STYLE", "FontSize", _font_size);
		config.set("STYLE", "EditorFont", _editor_font);
		config.set("STYLE", "EditorFontSize", _editor_font_size);
		config.set("STYLE", "StyleIndex", _style_index);
		config.set("STYLE", "EditorStyleIndex", _editor_style_index);

		if (_style_index > 2)
		{
			for (ImGuiCol i = 0; i < ImGuiCol_COUNT; i++)
				config.set("STYLE", ImGui::GetStyleColorName(i), (const float(&)[4])_imgui_context->Style.Colors[i]);
		}

		if (_editor_style_index > 1)
		{
			ImVec4 value;
			for (ImGuiCol i = 0; i < imgui_code_editor::color_palette_max; i++)
				value = ImGui::ColorConvertU32ToFloat4(_editor.get_palette_index(i)),
				config.set("STYLE", imgui_code_editor::get_palette_color_name(i), (const float(&)[4])value);
		}
	});
}
void reshade::runtime::deinit_ui()
{
	ImGui::DestroyContext(_imgui_context);
}

void reshade::runtime::build_font_atlas()
{
	ImGui::SetCurrentContext(_imgui_context);

	const auto atlas = _imgui_context->IO.Fonts;

	atlas->Clear();

	for (unsigned int i = 0; i < 2; ++i)
	{
		ImFontConfig cfg;
		cfg.SizePixels = static_cast<float>(i == 0 ? _font_size : _editor_font_size);

		const std::filesystem::path &font_path = i == 0 ? _font : _editor_font;

		if (std::error_code ec; !std::filesystem::is_regular_file(font_path, ec) || !atlas->AddFontFromFileTTF(font_path.u8string().c_str(), cfg.SizePixels))
			atlas->AddFontDefault(&cfg); // Use default font if custom font failed to load or does not exist
	}

	// If unable to build font atlas due to an invalid font, revert to the default font
	if (!atlas->Build())
	{
		_font.clear();
		_editor_font.clear();

		atlas->Clear();

		for (unsigned int i = 0; i < 2; ++i)
		{
			ImFontConfig cfg;
			cfg.SizePixels = static_cast<float>(i == 0 ? _font_size : _editor_font_size);

			atlas->AddFontDefault(&cfg);
			atlas->AddFontDefault(&cfg);
		}
	}

	destroy_font_atlas();

	_show_splash = true;
	_rebuild_font_atlas = false;

	int width, height;
	unsigned char *pixels;
	atlas->GetTexDataAsRGBA32(&pixels, &width, &height);

	ImGui::SetCurrentContext(nullptr);

	_imgui_font_atlas = std::make_unique<texture>();
	_imgui_font_atlas->width = width;
	_imgui_font_atlas->height = height;
	_imgui_font_atlas->format = reshadefx::texture_format::rgba8;
	_imgui_font_atlas->unique_name = "ImGUI Font Atlas";
	if (init_texture(*_imgui_font_atlas))
		upload_texture(*_imgui_font_atlas, pixels);
}
void reshade::runtime::destroy_font_atlas()
{
	_imgui_font_atlas.reset();
}

void reshade::runtime::draw_ui()
{
	const bool show_splash = _show_splash && (is_loading() || !_reload_compile_queue.empty() || (_last_present_time - _last_reload_time) < std::chrono::seconds(5));
	const bool show_screenshot_message = _show_screenshot_message && _last_present_time - _last_screenshot_time < std::chrono::seconds(_screenshot_save_success ? 3 : 5);

	if (_show_menu && !_ignore_shortcuts && !_imgui_context->IO.NavVisible && _input->is_key_pressed(0x1B /* VK_ESCAPE */))
		_show_menu = false; // Close when pressing the escape button and not currently navigating with the keyboard
	else if (!_ignore_shortcuts && _input->is_key_pressed(_menu_key_data) && _imgui_context->ActiveId == 0)
		_show_menu = !_show_menu;

	_ignore_shortcuts = false;
	_effects_expanded_state &= 2;

	if (_rebuild_font_atlas)
		build_font_atlas();

	ImGui::SetCurrentContext(_imgui_context);
	auto &imgui_io = _imgui_context->IO;
	imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
	imgui_io.MouseDrawCursor = _show_menu;
	imgui_io.DisplaySize.x = static_cast<float>(_width);
	imgui_io.DisplaySize.y = static_cast<float>(_height);
	imgui_io.Fonts->TexID = _imgui_font_atlas->impl.get();

	// Scale mouse position in case render resolution does not match the window size
	imgui_io.MousePos.x = _input->mouse_position_x() * (imgui_io.DisplaySize.x / _window_width);
	imgui_io.MousePos.y = _input->mouse_position_y() * (imgui_io.DisplaySize.y / _window_height);

	// Add wheel delta to the current absolute mouse wheel position
	imgui_io.MouseWheel += _input->mouse_wheel_delta();

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
	if (show_splash || show_screenshot_message || (!_show_menu && _tutorial_index == 0))
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

		if (show_screenshot_message)
		{
			if (!_screenshot_save_success)
				ImGui::TextColored(COLOR_RED, "Unable to save screenshot because path doesn't exist: %s", _screenshot_path.u8string().c_str());
			else
				ImGui::Text("Screenshot successfully saved to %s", _last_screenshot_file.u8string().c_str());
		}
		else
		{
			ImGui::TextUnformatted("ReShade " VERSION_STRING_FILE " by crosire");

			if (_needs_update)
			{
				ImGui::TextColored(COLOR_YELLOW,
					"An update is available! Please visit https://reshade.me and install the new version (v%lu.%lu.%lu).",
					_latest_version[0], _latest_version[1], _latest_version[2]);
			}
			else
			{
				ImGui::TextUnformatted("Visit https://reshade.me for news, updates, shaders and discussion.");
			}

			ImGui::Spacing();

			ImGui::ProgressBar(1.0f - _reload_remaining_effects / float(_reload_total_effects), ImVec2(-1, 0), "");
			ImGui::SameLine(15);

			if (_reload_remaining_effects != 0 && _reload_remaining_effects != std::numeric_limits<size_t>::max())
			{
				ImGui::Text(
					"Loading (%zu effects remaining) ... "
					"This might take a while. The application could become unresponsive for some time.",
					_reload_remaining_effects.load());
			}
			else if (!_reload_compile_queue.empty())
			{
				ImGui::Text(
					"Compiling (%zu effects remaining) ... "
					"This might take a while. The application could become unresponsive for some time.",
					_reload_compile_queue.size());
			}
			else if (_tutorial_index == 0)
			{
				ImGui::TextUnformatted("ReShade is now installed successfully! Press '");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", input::key_name(_menu_key_data).c_str());
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextUnformatted("' to start the tutorial.");
			}
			else
			{
				ImGui::TextUnformatted("Press '");
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", input::key_name(_menu_key_data).c_str());
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextUnformatted("' to open the configuration menu.");
			}

			if (!_last_reload_successful)
			{
				ImGui::Spacing();
				ImGui::TextColored(COLOR_RED,
					"There were errors compiling some shaders. Check the log for more details.");
			}
		}

		viewport_offset.y += ImGui::GetWindowHeight() + 10; // Add small space between windows

		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}
	else if (_show_clock || _show_fps || _show_frametime)
	{
		ImGui::SetNextWindowPos(ImVec2(imgui_io.DisplaySize.x - 200.0f, 5));
		ImGui::SetNextWindowSize(ImVec2(200.0f, 200.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, (const ImVec4 &)_fps_col);
		ImGui::Begin("FPS", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBackground);

		ImGui::SetWindowFontScale(_fps_scale);

		char temp[512];

		if (_show_clock)
		{
			const int hour = _date[3] / 3600;
			const int minute = (_date[3] - hour * 3600) / 60;
			const int seconds = _date[3] - hour * 3600 - minute * 60;

			ImFormatString(temp, sizeof(temp), _clock_format != 0 ? " %02u:%02u:%02u" : " %02u:%02u", hour, minute, seconds);
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}
		if (_show_fps)
		{
			ImFormatString(temp, sizeof(temp), "%.0f fps", imgui_io.Framerate);
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}
		if (_show_frametime)
		{
			ImFormatString(temp, sizeof(temp), "%5.2f ms", 1000.0f / imgui_io.Framerate);
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}

		ImGui::End();
		ImGui::PopStyleColor();
	}

	if (_show_menu)
	{
		// Change font size if user presses the control key and moves the mouse wheel
		if (imgui_io.KeyCtrl && imgui_io.MouseWheel != 0 && !_no_font_scaling)
		{
			_font_size = ImClamp(_font_size + static_cast<int>(imgui_io.MouseWheel), 8, 32);
			_editor_font_size = ImClamp(_editor_font_size + static_cast<int>(imgui_io.MouseWheel), 8, 32);
			_rebuild_font_atlas = true;
			save_config();
		}

		const ImGuiID root_space_id = ImGui::GetID("Dockspace");
		const ImGuiViewport *const viewport = ImGui::GetMainViewport();

		// Set up default dock layout if this was not done yet
		const bool init_window_layout = !ImGui::DockBuilderGetNode(root_space_id);
		if (init_window_layout)
		{
			// Add the root node
			ImGui::DockBuilderAddNode(root_space_id, ImGuiDockNodeFlags_Dockspace);
			ImGui::DockBuilderSetNodeSize(root_space_id, viewport->Size);

			// Split root node into two spaces
			ImGuiID main_space_id = 0;
			ImGuiID right_space_id = 0;
			ImGui::DockBuilderSplitNode(root_space_id, ImGuiDir_Left, 0.35f, &main_space_id, &right_space_id);

			// Attach most windows to the main dock space
			for (const auto &widget : _menu_callables)
				ImGui::DockBuilderDockWindow(widget.first.c_str(), main_space_id);

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
		ImGui::DockSpace(root_space_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruDockspace);
		ImGui::End();

		for (const auto &widget : _menu_callables)
		{
			if (ImGui::Begin(widget.first.c_str(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) // No focus so that window state is preserved between opening/closing the UI
				widget.second();
			ImGui::End();
		}

		if (_show_code_editor && !is_loading())
		{
			const std::string title = _selected_effect < _loaded_effects.size() ?
				"Editing " + _loaded_effects[_selected_effect].source_file.filename().u8string() + " ###editor" : "Viewing code###editor";

			if (ImGui::Begin(title.c_str(), &_show_code_editor))
				draw_code_editor();
			ImGui::End();
		}

		if (_preview_texture != nullptr && !is_loading() && _reload_compile_queue.empty())
		{
			ImVec2 preview_size = imgui_io.DisplaySize;
			if (_preview_size[0])
				preview_size.x = static_cast<float>(_preview_size[0]);
			if (_preview_size[1])
				preview_size.y = static_cast<float>(_preview_size[1]);

			ImGui::FindWindowByName("Viewport")->DrawList->AddImage(_preview_texture, ImVec2(0, 0), preview_size);
		}
	}

	// Render ImGui widgets and windows
	ImGui::Render();

	_input->block_mouse_input(_input_processing_mode != 0 && _show_menu && (imgui_io.WantCaptureMouse || _input_processing_mode == 2));
	_input->block_keyboard_input(_input_processing_mode != 0 && _show_menu && (imgui_io.WantCaptureKeyboard || _input_processing_mode == 2));

	if (const auto draw_data = ImGui::GetDrawData(); draw_data != nullptr && draw_data->CmdListsCount != 0 && draw_data->TotalVtxCount != 0)
	{
		render_imgui_draw_data(draw_data);
	}
}

void reshade::runtime::draw_overlay_menu_home()
{
	if (!_effects_enabled)
		ImGui::Text("Effects are disabled. Press '%s' to enable them again.", input::key_name(_effects_key_data).c_str());

	const char *tutorial_text =
		"Welcome! Since this is the first time you start ReShade, we'll go through a quick tutorial covering the most important features.\n\n"
		"Before we continue: If you have difficulties reading this text, press the 'Ctrl' key and adjust the font size with your mouse wheel. "
		"The window size is variable as well, just grab the bottom right corner and move it around.\n\n"
		"You can also use the keyboard for navigation in case mouse input does not work. Use the arrow keys to navigate, space bar to confirm an action or enter a control and the 'Esc' key to leave a control. "
		"Press 'Ctrl + Tab' to switch between tabs and windows.\n\n"
		"Click on the 'Continue' button to continue the tutorial.";

	// It is not possible to follow some of the tutorial steps while performance mode is active, so skip them
	if (_performance_mode && _tutorial_index <= 3)
		_tutorial_index = 4;

	if (_tutorial_index > 0)
	{
		if (_tutorial_index == 1)
		{
			tutorial_text =
				"This is the preset selection. All changes will be saved to the selected file.\n\n"
				"Click on the '+' button to name and add a new one.\n"
				"Make sure you always have a preset selected here before starting to tweak any values later, or else your changes won't be saved!";

			ImGui::PushStyleColor(ImGuiCol_FrameBg, COLOR_RED);
			ImGui::PushStyleColor(ImGuiCol_Button, COLOR_RED);
		}

		draw_preset_explorer();

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
		const char *const loading_message = "Loading ...";
		ImGui::SetCursorPos((ImGui::GetWindowSize() - ImGui::CalcTextSize(loading_message)) * 0.5f);
		ImGui::TextUnformatted(loading_message);
		return;
	}

	if (_tutorial_index > 1)
	{
		const bool show_clear_button = strcmp(_effect_filter_buffer, "Search") != 0 && _effect_filter_buffer[0] != '\0';
		ImGui::PushItemWidth((_variable_editor_tabs ? -130.0f : -260.0f) - (show_clear_button ? ImGui::GetFrameHeight() + _imgui_context->Style.ItemSpacing.x : 0));

		if (ImGui::InputText("##filter", _effect_filter_buffer, sizeof(_effect_filter_buffer), ImGuiInputTextFlags_AutoSelectAll))
		{
			_effects_expanded_state = 3;

			if (_effect_filter_buffer[0] == '\0')
			{
				// Reset visibility state
				for (technique &technique : _techniques)
					technique.hidden = technique.annotation_as_int("hidden") != 0;
			}
			else
			{
				const std::string filter = _effect_filter_buffer;

				for (technique &technique : _techniques)
					technique.hidden = technique.annotation_as_int("hidden") != 0 ||
						std::search(technique.name.begin(), technique.name.end(), filter.begin(), filter.end(),
							[](auto c1, auto c2) { return tolower(c1) == tolower(c2); }) == technique.name.end() && _loaded_effects[technique.effect_index].source_file.filename().u8string().find(filter) == std::string::npos;
			}
		}
		else if (!ImGui::IsItemActive() && _effect_filter_buffer[0] == '\0')
		{
			strcpy(_effect_filter_buffer, "Search");
		}

		ImGui::PopItemWidth();

		ImGui::SameLine();

		if (show_clear_button && ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0)))
		{
			strcpy(_effect_filter_buffer, "Search");
			// Reset visibility state
			for (technique &technique : _techniques)
				technique.hidden = technique.annotation_as_int("hidden") != 0;
		}

		ImGui::SameLine();

		if (ImGui::Button("Active to top", ImVec2(130 - _imgui_context->Style.ItemSpacing.x, 0)))
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

			if (const auto it = std::find_if_not(_techniques.begin(), _techniques.end(), [](const reshade::technique &a) {
					return a.enabled || a.toggle_key_data[0] != 0;
				}); it != _techniques.end())
			{
				std::stable_sort(it, _techniques.end(), [](const reshade::technique &lhs, const reshade::technique &rhs) {
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

		if (ImGui::Button(_effects_expanded_state & 2 ? "Collapse all" : "Expand all", ImVec2(130 - _imgui_context->Style.ItemSpacing.x, 0)))
			_effects_expanded_state = (~_effects_expanded_state & 2) | 1;

		if (_tutorial_index == 2)
		{
			tutorial_text =
				"This is the list of techniques. It contains all techniques in the effect files (*.fx) that were found in the effect search paths as specified in the settings.\n"
				"Enter text in the box at the top to filter it and search for specific techniques.\n\n"
				"Click on a technique to enable or disable it or drag it to a new location in the list to change the order in which the effects are applied.\n"
				"Use the right mouse button and click on an item to open the context menu with additional options.\n\n";

			ImGui::PushStyleColor(ImGuiCol_Border, COLOR_RED);
		}

		ImGui::Spacing();

		const float bottom_height = _performance_mode ? ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y : (_variable_editor_height + (_tutorial_index == 3 ? 175 : 0));

		if (ImGui::BeginChild("##techniques", ImVec2(0, -bottom_height), true))
			draw_overlay_technique_editor();
		ImGui::EndChild();

		if (_tutorial_index == 2)
			ImGui::PopStyleColor();
	}

	if (_tutorial_index > 2 && !_performance_mode)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::ButtonEx("##splitter", ImVec2(ImGui::GetContentRegionAvailWidth(), 5));
		ImGui::PopStyleVar();

		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		if (ImGui::IsItemActive())
			_variable_editor_height -= _imgui_context->IO.MouseDelta.y;

		if (_tutorial_index == 3)
		{
			tutorial_text =
				"This is the list of variables. It contains all tweakable options the effects expose. All values here apply in real-time. Press 'Ctrl' and click on a widget to manually edit the value.\n\n"
				"Enter text in the box at the top to filter it and search for specific variables.\n\n"
				"Use the right mouse button and click on an item to open the context menu with additional options.\n\n"
				"Once you have finished tweaking your preset, be sure to enable the 'Performance Mode' check box. "
				"This will recompile all shaders into a more optimal representation that can give a performance boost, but will disable variable tweaking and this list.";

			ImGui::PushStyleColor(ImGuiCol_Border, COLOR_RED);
		}

		const float bottom_height = ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y + (_tutorial_index == 3 ? 175 : 0);

		if (ImGui::BeginChild("##variables", ImVec2(0, -bottom_height), true))
			draw_overlay_variable_editor();
		ImGui::EndChild();

		if (_tutorial_index == 3)
			ImGui::PopStyleColor();
	}

	if (_tutorial_index > 3)
	{
		ImGui::Spacing();

		if (ImGui::Button("Reload", ImVec2(-150, 0)))
		{
			_show_splash = true;
			_effect_filter_buffer[0] = '\0'; // Reset filter

			load_effects();
		}

		ImGui::SameLine();

		if (ImGui::Checkbox("Performance Mode", &_performance_mode))
		{
			_show_splash = true;
			_effect_filter_buffer[0] = '\0'; // Reset filter

			save_config();
			load_effects(); // Reload effects after switching
		}
	}
	else
	{
		ImGui::BeginChildFrame(ImGui::GetID("tutorial"), ImVec2(0, 175));
		ImGui::TextWrapped(tutorial_text);
		ImGui::EndChildFrame();

		const float max_button_width = ImGui::GetContentRegionAvailWidth();

		if (ImGui::Button(_tutorial_index == 3 ? "Finish" : "Continue", ImVec2(max_button_width * 0.66666666f, 0)))
		{
			// Disable font scaling after tutorial
			if (_tutorial_index++ == 3)
				_no_font_scaling = true;

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
}

void reshade::runtime::draw_overlay_menu_settings()
{
	bool modified = false;
	bool reload_style = false;
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		modified |= imgui_key_input("Overlay Key", _menu_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();

		modified |= imgui_key_input("Effect Reload Key", _reload_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();
		modified |= imgui_key_input("Effect Toggle Key", _effects_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();

		modified |= imgui_key_input("Previous Preset Key", _previous_preset_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();
		modified |= imgui_key_input("Next Preset Key", _next_preset_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();

		modified |= ImGui::SliderInt("Preset transition\ndelay (ms)", &_preset_transition_delay, 0, 10 * 1000);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", "Makes a smooth transition, but only for floating point values.\nRecommended for multiple presets that contain the same shaders, otherwise set this to 0");

		modified |= ImGui::Combo("Input Processing", &_input_processing_mode,
			"Pass on all input\0"
			"Block input when cursor is on overlay\0"
			"Block all input when overlay is visible\0");

		modified |= imgui_path_list("Effect Search Paths", _effect_search_paths, _file_selection_path, g_reshade_dll_path.parent_path());
		modified |= imgui_path_list("Texture Search Paths", _texture_search_paths, _file_selection_path, g_reshade_dll_path.parent_path());

		if (ImGui::Button("Restart Tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
			_tutorial_index = 0;
	}

	if (ImGui::CollapsingHeader("Screenshots", ImGuiTreeNodeFlags_DefaultOpen))
	{
		modified |= imgui_key_input("Screenshot Key", _screenshot_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();

		modified |= imgui_directory_input_box("Screenshot Path", _screenshot_path, _file_selection_path);
		modified |= ImGui::Combo("Screenshot Format", &_screenshot_format, "Bitmap (*.bmp)\0Portable Network Graphics (*.png)\0");
		modified |= ImGui::Checkbox("Include current preset", &_screenshot_include_preset);
		modified |= ImGui::Checkbox("Save before and after images", &_screenshot_save_before);
	}

	if (ImGui::CollapsingHeader("User Interface", ImGuiTreeNodeFlags_DefaultOpen))
	{
		modified |= ImGui::Checkbox("Show screenshot message", &_show_screenshot_message);

		bool save_imgui_window_state = _imgui_context->IO.IniFilename != nullptr;
		if (ImGui::Checkbox("Save window state (ReShadeGUI.ini)", &save_imgui_window_state))
		{
			modified = true;
			_imgui_context->IO.IniFilename = save_imgui_window_state ? g_reshadegui_ini_path : nullptr;
		}

		modified |= ImGui::Checkbox("Group effect files with tabs instead of a tree", &_variable_editor_tabs);

		#pragma region Style
		if (ImGui::Combo("Style", &_style_index, "Dark\0Light\0Default\0Custom Simple\0Custom Advanced\0Solarized Dark\0Solarized Light\0"))
		{
			modified = true;
			reload_style = true;
		}

		if (_style_index == 3) // Custom Simple
		{
			ImVec4 *const colors = _imgui_context->Style.Colors;

			if (ImGui::BeginChild("##colors", ImVec2(0, 105), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened))
			{
				ImGui::PushItemWidth(-160);
				modified |= ImGui::ColorEdit3("Background", &colors[ImGuiCol_WindowBg].x);
				modified |= ImGui::ColorEdit3("ItemBackground", &colors[ImGuiCol_FrameBg].x);
				modified |= ImGui::ColorEdit3("Text", &colors[ImGuiCol_Text].x);
				modified |= ImGui::ColorEdit3("ActiveItem", &colors[ImGuiCol_ButtonActive].x);
				ImGui::PopItemWidth();
			} ImGui::EndChild();

			// Change all colors using the above as base
			if (modified)
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
					modified |= ImGui::ColorEdit4("##color", &_imgui_context->Style.Colors[i].x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
					ImGui::SameLine(); ImGui::TextUnformatted(ImGui::GetStyleColorName(i));
					ImGui::PopID();
				}
				ImGui::PopItemWidth();
			} ImGui::EndChild();
		}
		#pragma endregion

		#pragma region Editor Style
		if (ImGui::Combo("Editor Style", &_editor_style_index, "Dark\0Light\0Custom\0Solarized Dark\0Solarized Light\0"))
		{
			modified = true;
			reload_style = true;
		}

		if (_editor_style_index == 2)
		{
			if (ImGui::BeginChild("##editor_colors", ImVec2(0, 300), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened))
			{
				ImGui::PushItemWidth(-160);
				for (ImGuiCol i = 0; i < imgui_code_editor::color_palette_max; i++)
				{
					ImVec4 color = ImGui::ColorConvertU32ToFloat4(_editor.get_palette_index(i));
					ImGui::PushID(i);
					modified |= ImGui::ColorEdit4("##editor_color", &color.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
					ImGui::SameLine(); ImGui::TextUnformatted(imgui_code_editor::get_palette_color_name(i));
					ImGui::PopID();
					_editor.get_palette_index(i) = ImGui::ColorConvertFloat4ToU32(color);
				}
				ImGui::PopItemWidth();
			} ImGui::EndChild();
		}
		#pragma endregion

		if (imgui_font_select("Font", _font, _font_size))
		{
			modified = true;
			_rebuild_font_atlas = true;
		}

		if (imgui_font_select("Editor Font", _editor_font, _editor_font_size))
		{
			modified = true;
			_rebuild_font_atlas = true;
		}

		if (float &alpha = _imgui_context->Style.Alpha; ImGui::SliderFloat("Global Alpha", &alpha, 0.1f, 1.0f, "%.2f"))
		{
			// Prevent user from setting alpha to zero
			alpha = std::max(alpha, 0.1f);
			modified = true;
		}

		if (float &rounding = _imgui_context->Style.FrameRounding; ImGui::SliderFloat("Frame Rounding", &rounding, 0.0f, 12.0f, "%.0f"))
		{
			// Apply the same rounding to everything
			_imgui_context->Style.GrabRounding = _imgui_context->Style.TabRounding = _imgui_context->Style.ScrollbarRounding = rounding;
			_imgui_context->Style.WindowRounding = _imgui_context->Style.ChildRounding = _imgui_context->Style.PopupRounding = rounding;
			modified = true;
		}

		modified |= ImGui::Checkbox("Show Clock", &_show_clock);
		ImGui::SameLine(0, 10); modified |= ImGui::Checkbox("Show FPS", &_show_fps);
		ImGui::SameLine(0, 10); modified |= ImGui::Checkbox("Show Frame Time", &_show_frametime);
		modified |= ImGui::Combo("Clock Format", &_clock_format, "HH:MM\0HH:MM:SS\0");
		modified |= ImGui::SliderFloat("FPS Text Size", &_fps_scale, 0.2f, 2.5f, "%.1f");
		modified |= ImGui::ColorEdit4("FPS Text Color", _fps_col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
	}

	if (modified)
		save_config();
	if (reload_style) // Style is applied in "load_config()".
		load_config();
}

void reshade::runtime::draw_overlay_menu_statistics()
{
	unsigned int cpu_digits = 1;
	unsigned int gpu_digits = 1;
	uint64_t post_processing_time_cpu = 0;
	uint64_t post_processing_time_gpu = 0;

	// Variables used to calculate memory size of textures
	ldiv_t memory_view;
	const char *memory_size_unit;
	uint32_t post_processing_memory_size = 0;

	if (!is_loading() && _effects_enabled)
	{
		for (const auto &technique : _techniques)
		{
			cpu_digits = std::max(cpu_digits, technique.average_cpu_duration >= 100'000'000 ? 3u : technique.average_cpu_duration >= 10'000'000 ? 2u : 1u);
			post_processing_time_cpu += technique.average_cpu_duration;
			gpu_digits = std::max(gpu_digits, technique.average_gpu_duration >= 100'000'000 ? 3u : technique.average_gpu_duration >= 10'000'000 ? 2u : 1u);
			post_processing_time_gpu += technique.average_gpu_duration;
		}
	}

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth());
		ImGui::PlotLines("##framerate",
			_imgui_context->FramerateSecPerFrame, 120,
			_imgui_context->FramerateSecPerFrameIdx,
			nullptr,
			_imgui_context->FramerateSecPerFrameAccum / 120 * 0.5f,
			_imgui_context->FramerateSecPerFrameAccum / 120 * 1.5f,
			ImVec2(0, 50));
		ImGui::PopItemWidth();

		ImGui::BeginGroup();

		ImGui::TextUnformatted("Hardware:");
		ImGui::TextUnformatted("Application:");
		ImGui::TextUnformatted("Time:");
		ImGui::TextUnformatted("Network:");
		ImGui::Text("Frame %llu:", _framecount + 1);
		ImGui::NewLine();
		ImGui::TextUnformatted("Post-Processing:");

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.33333333f);
		ImGui::BeginGroup();

		if (_vendor_id != 0)
			ImGui::Text("VEN_%X", _vendor_id);
		else
			ImGui::TextUnformatted("Unknown");
		ImGui::TextUnformatted(g_target_executable_path.filename().u8string().c_str());
		ImGui::Text("%d-%d-%d %d", _date[0], _date[1], _date[2], _date[3]);
		ImGui::Text("%u B", g_network_traffic);
		ImGui::Text("%.2f fps", _imgui_context->IO.Framerate);
		ImGui::Text("%u draw calls", _drawcalls);
		ImGui::Text("%*.3f ms (CPU)", cpu_digits + 4, post_processing_time_cpu * 1e-6f);

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.66666666f);
		ImGui::BeginGroup();

		if (_device_id != 0)
			ImGui::Text("DEV_%X", _device_id);
		else
			ImGui::TextUnformatted("Unknown");
		ImGui::Text("0x%X", std::hash<std::string>()(g_target_executable_path.stem().u8string()) & 0xFFFFFFFF);
		ImGui::Text("%.0f ms", std::chrono::duration_cast<std::chrono::nanoseconds>(_last_present_time - _start_time).count() * 1e-6f);
		ImGui::NewLine();
		ImGui::Text("%*.3f ms", gpu_digits + 4, _last_frame_duration.count() * 1e-6f);
		ImGui::Text("%u vertices", _vertices);
		if (post_processing_time_gpu != 0)
			ImGui::Text("%*.3f ms (GPU)", gpu_digits + 4, (post_processing_time_gpu * 1e-6f));

		ImGui::EndGroup();
	}

	if (ImGui::CollapsingHeader("Techniques", ImGuiTreeNodeFlags_DefaultOpen) && !is_loading() && _effects_enabled)
	{
		ImGui::BeginGroup();

		for (const auto &technique : _techniques)
		{
			if (!technique.enabled)
				continue;

			if (technique.passes.size() > 1)
				ImGui::Text("%s (%zu passes)", technique.name.c_str(), technique.passes.size());
			else
				ImGui::TextUnformatted(technique.name.c_str());
		}

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.33333333f);
		ImGui::BeginGroup();

		for (const auto &technique : _techniques)
		{
			if (!technique.enabled)
				continue;

			if (technique.average_cpu_duration != 0)
				ImGui::Text("%*.3f ms (CPU) (%.0f%%)", cpu_digits + 4, technique.average_cpu_duration * 1e-6f, 100 * (technique.average_cpu_duration * 1e-6f) / (post_processing_time_cpu * 1e-6f));
			else
				ImGui::NewLine();
		}

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.66666666f);
		ImGui::BeginGroup();

		for (const auto &technique : _techniques)
		{
			if (!technique.enabled)
				continue;

			// GPU timings are not available for all APIs
			if (technique.average_gpu_duration != 0)
				ImGui::Text("%*.3f ms (GPU) (%.0f%%)", gpu_digits + 4, technique.average_gpu_duration * 1e-6f, 100 * (technique.average_gpu_duration * 1e-6f) / (post_processing_time_gpu * 1e-6f));
			else
				ImGui::NewLine();
		}

		ImGui::EndGroup();
	}

	if (ImGui::CollapsingHeader("Render Targets & Textures", ImGuiTreeNodeFlags_DefaultOpen) && !is_loading())
	{
		const char *texture_formats[] = {
			"unknown",
			"R8", "R16F", "R32F", "RG8", "RG16", "RG16F", "RG32F", "RGBA8", "RGBA16", "RGBA16F", "RGBA32F", "RGB10A2"
		};
		const unsigned int pixel_sizes[] = {
			0,
			1 /*R8*/, 2 /*R16F*/, 4 /*R32F*/, 2 /*RG8*/, 4 /*RG16*/, 4 /*RG16F*/, 8 /*RG32F*/, 4 /*RGBA8*/, 8 /*RGBA16*/, 8 /*RGBA16F*/, 16 /*RGBA32F*/, 4 /*RGB10A2*/
		};

		static_assert(_countof(texture_formats) - 1 == static_cast<unsigned int>(reshadefx::texture_format::rgb10a2));

		const float total_width = ImGui::GetWindowContentRegionWidth();
		unsigned int texture_index = 0;
		const unsigned int num_columns = static_cast<unsigned int>(std::ceilf(total_width / 500.0f));
		const float single_image_width = (total_width / num_columns) - 5.0f;

		for (const auto &texture : _textures)
		{
			if (!_loaded_effects[texture.effect_index].rendering || texture.impl == nullptr || texture.impl_reference != texture_reference::none)
				continue;

			ImGui::PushID(texture_index);
			ImGui::BeginGroup();

			uint32_t memory_size = 0;
			for (uint32_t level = 0, width = texture.width, height = texture.height; level < texture.levels; ++level, width /= 2, height /= 2)
				memory_size += width * height * pixel_sizes[static_cast<unsigned int>(texture.format)];

			post_processing_memory_size += memory_size;

			if (memory_size >= 1024 * 1024) {
				memory_view = std::ldiv(memory_size, 1024 * 1024);
				memory_view.rem /= 1000;
				memory_size_unit = "MiB";
			}
			else {
				memory_view = std::ldiv(memory_size, 1024);
				memory_size_unit = "KiB";
			}

			ImGui::TextColored(ImVec4(1, 1, 1, 1), texture.unique_name.c_str());
			ImGui::Text("%ux%u | %u mipmap(s) | %s | %ld.%03ld%s",
				texture.width,
				texture.height,
				texture.levels - 1,
				texture_formats[static_cast<unsigned int>(texture.format)],
				memory_view.quot, memory_view.rem, memory_size_unit);

			std::string target_info = "Read only texture";
			for (const auto &technique : _techniques)
			{
				if (technique.effect_index != texture.effect_index)
					continue;

				for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
				{
					for (const auto &target : technique.passes[pass_index].render_target_names)
					{
						if (target == texture.unique_name)
						{
							if (target_info[0] != 'W') // Check if this texture was written by another pass already
								target_info = "Written in " + technique.name + " pass ";
							else
								target_info += " and pass ";

							target_info += std::to_string(pass_index);
						}
					}
				}
			}

			ImGui::TextUnformatted(target_info.c_str());

			if (bool check = _preview_texture == texture.impl.get() && _preview_size[0] == 0; ImGui::RadioButton("Preview scaled", check)) {
				_preview_size[0] = 0;
				_preview_size[1] = 0;
				_preview_texture = !check ? texture.impl.get() : nullptr;
			}
			ImGui::SameLine();
			if (bool check = _preview_texture == texture.impl.get() && _preview_size[0] != 0; ImGui::RadioButton("Preview original", check)) {
				_preview_size[0] = texture.width;
				_preview_size[1] = texture.height;
				_preview_texture = !check ? texture.impl.get() : nullptr;
			}

			const float aspect_ratio = static_cast<float>(texture.width) / static_cast<float>(texture.height);
			imgui_image_with_checkerboard_background(texture.impl.get(), ImVec2(single_image_width, single_image_width / aspect_ratio));

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

		if (post_processing_memory_size >= 1024 * 1024) {
			memory_view = std::ldiv(post_processing_memory_size, 1024 * 1024);
			memory_view.rem /= 1000;
			memory_size_unit = "MiB";
		}
		else {
			memory_view = std::ldiv(post_processing_memory_size, 1024);
			memory_size_unit = "KiB";
		}

		ImGui::Text("Total memory usage: %ld.%03ld%s", memory_view.quot, memory_view.rem, memory_size_unit);
	}
}

void reshade::runtime::draw_overlay_menu_log()
{
	if (ImGui::Button("Clear Log"))
		reshade::log::lines.clear();

	ImGui::SameLine();
	ImGui::Checkbox("Word Wrap", &_log_wordwrap);
	ImGui::SameLine();

	static ImGuiTextFilter filter; // TODO: Better make this a member of the runtime class, in case there are multiple instances.
	filter.Draw("Filter (inc, -exc)", -150);

	if (ImGui::BeginChild("log", ImVec2(0, 0), true, _log_wordwrap ? 0 : ImGuiWindowFlags_AlwaysHorizontalScrollbar))
	{
		std::vector<std::string> lines;
		for (auto &line : reshade::log::lines)
			if (filter.PassFilter(line.c_str()))
				lines.push_back(line);

		ImGuiListClipper clipper(static_cast<int>(lines.size()), ImGui::GetTextLineHeightWithSpacing());

		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				ImVec4 textcol = _imgui_context->Style.Colors[ImGuiCol_Text];

				if (lines[i].find("ERROR |") != std::string::npos)
					textcol = COLOR_RED;
				else if (lines[i].find("WARN  |") != std::string::npos)
					textcol = COLOR_YELLOW;
				else if (lines[i].find("DEBUG |") != std::string::npos)
					textcol = ImColor(100, 100, 255);

				ImGui::PushStyleColor(ImGuiCol_Text, textcol);
				if (_log_wordwrap) ImGui::PushTextWrapPos();

				ImGui::TextUnformatted(lines[i].c_str());

				if (_log_wordwrap) ImGui::PopTextWrapPos();
				ImGui::PopStyleColor();
			}
		}
	} ImGui::EndChild();
}

void reshade::runtime::draw_overlay_menu_about()
{
	ImGui::TextUnformatted("ReShade " VERSION_STRING_FILE);

	ImGui::PushTextWrapPos();
	ImGui::TextUnformatted(R"(Copyright (C) 2014 Patrick Mours. All rights reserved.

https://github.com/crosire/reshade

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)");

	if (ImGui::CollapsingHeader("MinHook"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2009-2016 Tsuda Kageyu. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)");
	}
	if (ImGui::CollapsingHeader("Hacker Disassembler Engine 32/64 C"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2008-2009 Vyacheslav Patkov. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)");
	}
	if (ImGui::CollapsingHeader("dear imgui"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2014-2015 Omar Cornut and ImGui contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)");
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
		ImGui::TextUnformatted("Slavomir Kaslev");
	}
	if (ImGui::CollapsingHeader("stb_image, stb_image_write"))
	{
		ImGui::TextUnformatted("Sean Barrett and contributors");
	}
	if (ImGui::CollapsingHeader("DDS loading from SOIL"))
	{
		ImGui::TextUnformatted("Jonathan \"lonesock\" Dummer");
	}
	if (ImGui::CollapsingHeader("Solarized"))
	{
		ImGui::TextUnformatted(R"(Copyright (c) 2011 Ethan Schoonover

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.)");
	}

	ImGui::PopTextWrapPos();
}

void reshade::runtime::draw_code_editor()
{
	const auto parse_errors = [this](const std::string &errors) {
		_editor.clear_errors();

		for (size_t offset = 0, next; offset != std::string::npos; offset = next)
		{
			const size_t pos_error = errors.find(": ", offset);
			const size_t pos_error_line = errors.rfind('(', pos_error); // Paths can contain '(', but no ": ", so search backwards from th error location to find the line info
			if (pos_error == std::string::npos || pos_error_line == std::string::npos)
				break;

			const size_t pos_linefeed = errors.find('\n', pos_error);

			next = pos_linefeed != std::string::npos ? pos_linefeed + 1 : std::string::npos;

			// Ignore errors that aren't in the main source file
			if (const std::string_view error_file(errors.c_str() + offset, pos_error_line - offset);
				error_file != _loaded_effects[_selected_effect].source_file.u8string())
				continue;

			const int error_line = std::strtol(errors.c_str() + pos_error_line + 1, nullptr, 10);
			const std::string error_text = errors.substr(pos_error + 2 /* skip space */, pos_linefeed - pos_error - 2);

			_editor.add_error(error_line, error_text, error_text.find("warning") != std::string::npos);
		}
	};

	if (_selected_effect < _loaded_effects.size() && (ImGui::Button("Save") || _input->is_key_pressed('S', true, false, false)))
	{
		// Hide splash bar during compile
		_show_splash = false;

		const std::string text = _editor.get_text();

		const std::filesystem::path source_file = _loaded_effects[_selected_effect].source_file;

		// Write editor text to file
		std::ofstream(source_file, std::ios::trunc).write(text.c_str(), text.size());

		// Reload effect file
		_textures_loaded = false;
		_reload_total_effects = 1;
		_reload_remaining_effects = 1;
		unload_effect(_selected_effect);
		load_effect(source_file, _selected_effect);
		assert(_reload_remaining_effects == 0);

		// Reloading an effect file invalidates all textures, but the statistics window may already have drawn references to those, so need to reset it
		ImGui::FindWindowByName("Statistics")->DrawList->CmdBuffer.clear();

		parse_errors(_loaded_effects[_selected_effect].errors);
	}

	ImGui::SameLine();

	ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth());

	if (ImGui::BeginCombo("##file", _selected_effect < _loaded_effects.size() ? _loaded_effects[_selected_effect].source_file.u8string().c_str() : "", ImGuiComboFlags_HeightLarge))
	{
		for (size_t i = 0; i < _loaded_effects.size(); ++i)
		{
			const auto &effect = _loaded_effects[i];

			// Ignore unloaded effects
			if (effect.source_file.empty())
				continue;

			if (ImGui::Selectable(effect.source_file.u8string().c_str(), _selected_effect == i))
			{
				_selected_effect = i;
				_selected_effect_changed = true;
			}
		}

		ImGui::EndCombo();
	}

	ImGui::PopItemWidth();

	if (_selected_effect_changed)
	{
		_editor.set_readonly(false);

		if (_selected_effect < _loaded_effects.size())
		{
			const auto &effect = _loaded_effects[_selected_effect];

			// Load file to string and update editor text
			_editor.set_text(std::string(std::istreambuf_iterator<char>(std::ifstream(effect.source_file).rdbuf()), std::istreambuf_iterator<char>()));

			parse_errors(effect.errors);
		}
		else
		{
			_editor.clear_text();
		}

		_selected_effect_changed = false;
	}

	// Select editor font
	ImGui::PushFont(_imgui_context->IO.Fonts->Fonts[1]);

	_editor.render("##editor");

	ImGui::PopFont();

	// Disable keyboard shortcuts when the window is focused so they don't get triggered while editing text
	const bool is_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
	_ignore_shortcuts |= is_focused;

	// Disable keyboard navigation starting with next frame when editor is focused so that the Alt key can be used without it switching focus to the menu bar
	if (is_focused)
		_imgui_context->IO.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
	else // Enable navigation again if focus is lost
		_imgui_context->IO.ConfigFlags |=  ImGuiConfigFlags_NavEnableKeyboard;
}

void reshade::runtime::draw_overlay_variable_editor()
{
	const ImVec2 popup_pos = ImGui::GetCursorScreenPos() + ImVec2(std::max(0.f, ImGui::GetWindowContentRegionWidth() * 0.5f - 200.0f), ImGui::GetFrameHeightWithSpacing());

	if (imgui_popup_button("Edit global preprocessor definitions", ImGui::GetContentRegionAvailWidth(), ImGuiWindowFlags_NoMove))
	{
		ImGui::SetWindowPos(popup_pos);

		bool modified = false;
		float popup_height = (std::max(_global_preprocessor_definitions.size(), _preset_preprocessor_definitions.size()) + 2) * ImGui::GetFrameHeightWithSpacing();
		popup_height = std::min(popup_height, _window_height - popup_pos.y - 20.0f);
		const float button_size = ImGui::GetFrameHeight();
		const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;

		ImGui::BeginChild("##definitions", ImVec2(400.0f, popup_height), false, ImGuiWindowFlags_NoScrollWithMouse);

		if (ImGui::BeginTabBar("##definition_types", ImGuiTabBarFlags_NoTooltip))
		{
			if (ImGui::BeginTabItem("Global"))
			{
				for (size_t i = 0; i < _global_preprocessor_definitions.size(); ++i)
				{
					char name[128] = "";
					char value[128] = "";

					const size_t equals_index = _global_preprocessor_definitions[i].find('=');
					_global_preprocessor_definitions[i].copy(name, std::min(equals_index, sizeof(name) - 1));
					if (equals_index != std::string::npos)
						_global_preprocessor_definitions[i].copy(value, sizeof(value) - 1, equals_index + 1);

					ImGui::PushID(static_cast<int>(i));

					ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() * 0.66666666f - (button_spacing));
					modified |= ImGui::InputText("##name", name, sizeof(name), ImGuiInputTextFlags_CharsNoBlank);
					ImGui::PopItemWidth();

					ImGui::SameLine(0, button_spacing);

					ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() * 0.33333333f - (button_spacing + button_size) + 1);
					modified |= ImGui::InputText("##value", value, sizeof(value));
					ImGui::PopItemWidth();

					ImGui::SameLine(0, button_spacing);

					if (ImGui::Button("-", ImVec2(button_size, 0)))
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
				ImGui::SameLine(0, ImGui::GetWindowContentRegionWidth() - button_size);
				if (ImGui::Button("+", ImVec2(button_size, 0)))
					_global_preprocessor_definitions.emplace_back();

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Current Preset"))
			{
				for (size_t i = 0; i < _preset_preprocessor_definitions.size(); ++i)
				{
					char name[128] = "";
					char value[128] = "";

					const size_t equals_index = _preset_preprocessor_definitions[i].find('=');
					_preset_preprocessor_definitions[i].copy(name, std::min(equals_index, sizeof(name) - 1));
					if (equals_index != std::string::npos)
						_preset_preprocessor_definitions[i].copy(value, sizeof(value) - 1, equals_index + 1);

					ImGui::PushID(static_cast<int>(i));

					ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() * 0.66666666f - (button_spacing));
					modified |= ImGui::InputText("##name", name, sizeof(name), ImGuiInputTextFlags_CharsNoBlank);
					ImGui::PopItemWidth();

					ImGui::SameLine(0, button_spacing);

					ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() * 0.33333333f - (button_spacing + button_size) + 1);
					modified |= ImGui::InputText("##value", value, sizeof(value));
					ImGui::PopItemWidth();

					ImGui::SameLine(0, button_spacing);

					if (ImGui::Button("-", ImVec2(button_size, 0)))
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
				ImGui::SameLine(0, ImGui::GetWindowContentRegionWidth() - button_size);
				if (ImGui::Button("+", ImVec2(button_size, 0)))
					_preset_preprocessor_definitions.emplace_back();

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::EndChild();

		if (modified)
			save_config(), save_current_preset(), _was_preprocessor_popup_edited = true;

		ImGui::EndPopup();
	}
	else if (_was_preprocessor_popup_edited)
	{
		_was_preprocessor_popup_edited = false;

		_show_splash = true;
		_effect_filter_buffer[0] = '\0'; // Reset filter

		load_effects();
	}

	ImGui::BeginChild("##variables");

	bool current_tree_is_open = false;
	bool current_category_is_closed = false;
	size_t current_effect = std::numeric_limits<size_t>::max();
	std::string current_category;

	if (_variable_editor_tabs)
	{
		ImGui::BeginTabBar("##variables");
	}

	ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.5f);

	for (size_t index = 0; index < _uniforms.size(); ++index)
	{
		uniform &variable = _uniforms[index];

		// Skip hidden and special variables
		if (variable.annotation_as_int("hidden") || variable.special != special_uniform::none)
			continue;

		// Hide variables that are not currently used in any of the active effects
		if (!_loaded_effects[variable.effect_index].rendering)
			continue;
		assert(_loaded_effects[variable.effect_index].compile_sucess);

		// Create separate tab for every effect file
		if (variable.effect_index != current_effect)
		{
			current_effect = variable.effect_index;

			const bool is_focused = _focused_effect == variable.effect_index;

			const std::string filename = _loaded_effects[current_effect].source_file.filename().u8string();

			if (_variable_editor_tabs)
			{
				if (current_tree_is_open)
					ImGui::EndTabItem();

				current_tree_is_open = ImGui::BeginTabItem(filename.c_str());
			}
			else
			{
				if (current_tree_is_open)
					ImGui::TreePop();

				if (is_focused || _effects_expanded_state & 1)
					ImGui::SetNextTreeNodeOpen(is_focused || (_effects_expanded_state >> 1) != 0);

				current_tree_is_open = ImGui::TreeNodeEx(filename.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			}

			if (is_focused)
			{
				ImGui::SetScrollHereY(0.0f);
				_focused_effect = std::numeric_limits<size_t>::max();
			}

			if (current_tree_is_open)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(_imgui_context->Style.FramePadding.x, 0));
				if (imgui_popup_button("Reset all to default", _variable_editor_tabs ? ImGui::GetContentRegionAvailWidth() : ImGui::CalcItemWidth()))
				{
					ImGui::Text("Do you really want to reset all values in '%s' to their defaults?", filename.c_str());

					if (ImGui::Button("Yes", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
					{
						for (uniform &reset_variable : _uniforms)
							if (reset_variable.effect_index == current_effect)
								reset_uniform_value(reset_variable);

						save_current_preset();

						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}
				ImGui::PopStyleVar();
			}
		}

		// Skip rendering invisible items
		if (!current_tree_is_open)
			continue;

		if (const std::string_view category = variable.annotation_as_string("ui_category");
			category != current_category)
		{
			current_category = category;

			if (!category.empty())
			{
				std::string label(category.data(), category.size());
				if (!_variable_editor_tabs)
					for (float x = 0, space_x = ImGui::CalcTextSize(" ").x, width = (ImGui::CalcItemWidth() - ImGui::CalcTextSize(label.data()).x - 45) / 2; x < width; x += space_x)
						label.insert(0, " ");

				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_NoTreePushOnOpen;
				if (!variable.annotation_as_int("ui_category_closed"))
					flags |= ImGuiTreeNodeFlags_DefaultOpen;

				current_category_is_closed = !ImGui::TreeNodeEx(label.c_str(), flags);
			}
			else
			{
				current_category_is_closed = false;
			}
		}

		// Skip rendering invisible items
		if (current_category_is_closed)
			continue;

		// Add spacing before variable widget
		for (int i = 0, spacing = variable.annotation_as_int("ui_spacing"); i < spacing; ++i)
			ImGui::Spacing();

		bool modified = false;
		std::string_view label = variable.annotation_as_string("ui_label");
		if (label.empty()) label = variable.name;
		const std::string_view ui_type = variable.annotation_as_string("ui_type");

		ImGui::PushID(static_cast<int>(index));

		switch (variable.type.base)
		{
		case reshadefx::type::t_bool: {
			bool data;
			get_uniform_value(variable, &data, 1);

			if (ui_type == "combo")
			{
				int current_item = data ? 1 : 0;
				modified = ImGui::Combo(label.data(), &current_item, "Off\0On\0");
				data = current_item != 0;
			}
			else
			{
				modified = ImGui::Checkbox(label.data(), &data);
			}

			if (modified)
				set_uniform_value(variable, &data, 1);
			break; }
		case reshadefx::type::t_int:
		case reshadefx::type::t_uint: {
			int data[4];
			get_uniform_value(variable, data, 4);

			const auto ui_min_val = variable.annotation_as_int("ui_min");
			const auto ui_max_val = variable.annotation_as_int("ui_max");
			const auto ui_stp_val = std::max(1, variable.annotation_as_int("ui_step"));

			if (ui_type == "slider")
				modified = imgui_slider_with_buttons(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val);
			else if (ui_type == "drag")
				modified = variable.annotations.find("ui_step") == variable.annotations.end() ?
					ImGui::DragScalarN(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, 1.0f, &ui_min_val, &ui_max_val) :
					imgui_drag_with_buttons(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val);
			else if (ui_type == "list")
				modified = imgui_list_with_buttons(label.data(), variable.annotation_as_string("ui_items"), data[0]);
			else if (ui_type == "combo") {
				const std::string_view ui_items = variable.annotation_as_string("ui_items");
				std::string items(ui_items.data(), ui_items.size());
				// Make sure list is terminated with a zero in case user forgot so no invalid memory is read accidentally
				if (ui_items.empty() || ui_items.back() != '\0')
					items.push_back('\0');

				modified = ImGui::Combo(label.data(), data, items.c_str());
			}
			else if (ui_type == "radio") {
				const std::string_view ui_items = variable.annotation_as_string("ui_items");
				ImGui::BeginGroup();
				for (size_t offset = 0, next, i = 0; (next = ui_items.find('\0', offset)) != std::string::npos; offset = next + 1, ++i)
					modified |= ImGui::RadioButton(ui_items.data() + offset, data, static_cast<int>(i));
				ImGui::EndGroup();
			}
			else
				modified = ImGui::InputScalarN(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows);

			if (modified)
				set_uniform_value(variable, data, 4);
			break; }
		case reshadefx::type::t_float: {
			float data[4];
			get_uniform_value(variable, data, 4);

			const auto ui_min_val = variable.annotation_as_float("ui_min");
			const auto ui_max_val = variable.annotation_as_float("ui_max");
			const auto ui_stp_val = std::max(0.001f, variable.annotation_as_float("ui_step"));

			// Calculate display precision based on step value
			char precision_format[] = "%.0f";
			for (float x = ui_stp_val; x < 1.0f && precision_format[2] < '9'; x *= 10.0f)
				++precision_format[2]; // This changes the text to "%.1f", "%.2f", "%.3f", ...

			if (ui_type == "slider")
				modified = imgui_slider_with_buttons(label.data(), ImGuiDataType_Float, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, precision_format);
			else if (ui_type == "drag")
				modified = variable.annotations.find("ui_step") == variable.annotations.end() ?
					ImGui::DragScalarN(label.data(), ImGuiDataType_Float, data, variable.type.rows, ui_stp_val, &ui_min_val, &ui_max_val, precision_format) :
					imgui_drag_with_buttons(label.data(), ImGuiDataType_Float, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, precision_format);
			else if (ui_type == "color" && variable.type.rows == 1)
				modified = imgui_slider_for_alpha(label.data(), data);
			else if (ui_type == "color" && variable.type.rows == 3)
				modified = ImGui::ColorEdit3(label.data(), data, ImGuiColorEditFlags_NoOptions);
			else if (ui_type == "color" && variable.type.rows == 4)
				modified = ImGui::ColorEdit4(label.data(), data, ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaBar);
			else
				modified = ImGui::InputScalarN(label.data(), ImGuiDataType_Float, data, variable.type.rows);

			if (modified)
				set_uniform_value(variable, data, 4);
			break; }
		}

		// Display tooltip
		if (const std::string_view tooltip = variable.annotation_as_string("ui_tooltip");
			!tooltip.empty() && ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", tooltip.data());

		// Create context menu
		if (ImGui::BeginPopupContextItem("##context"))
		{
			if (ImGui::Button("Reset to default", ImVec2(200, 0)))
			{
				modified = true;
				reset_uniform_value(variable);
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::PopID();

		// A value has changed, so save the current preset
		if (modified)
			save_current_preset();
	}

	ImGui::PopItemWidth();

	if (_variable_editor_tabs)
	{
		if (current_tree_is_open)
			ImGui::EndTabItem();

		ImGui::EndTabBar();
	}
	else
	{
		if (current_tree_is_open)
			ImGui::TreePop();
	}

	ImGui::EndChild();
}

void reshade::runtime::draw_overlay_technique_editor()
{
	size_t hovered_technique_index = std::numeric_limits<size_t>::max();

	for (size_t index = 0; index < _techniques.size(); ++index)
	{
		technique &technique = _techniques[index];

		// Skip hidden techniques
		if (technique.hidden)
			continue;

		ImGui::PushID(static_cast<int>(index));

		// Look up effect that contains this technique
		const effect_data &effect = _loaded_effects[technique.effect_index];

		// Draw border around the item if it is selected
		const bool draw_border = _selected_technique == index;
		if (draw_border)
			ImGui::Separator();

		const bool clicked = _imgui_context->IO.MouseClicked[0];
		const bool compile_success = effect.compile_sucess;
		assert(compile_success || !technique.enabled);

		// Prevent user from enabling the technique when the effect failed to compile
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !compile_success);
		// Gray out disabled techniques and mark techniques which failed to compile red
		ImGui::PushStyleColor(ImGuiCol_Text, compile_success ? _imgui_context->Style.Colors[technique.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled] : COLOR_RED);

		std::string_view ui_label = technique.annotation_as_string("ui_label");
		if (ui_label.empty() || !compile_success) ui_label = technique.name;
		std::string label(ui_label.data(), ui_label.size());
		label += " [" + effect.source_file.filename().u8string() + ']' + (!compile_success ? " (failed to compile)" : "");

		if (bool status = technique.enabled; ImGui::Checkbox(label.data(), &status))
		{
			if (status)
				enable_technique(technique);
			else
				disable_technique(technique);
			save_current_preset();
		}

		ImGui::PopStyleColor();
		ImGui::PopItemFlag();

		if (ImGui::IsItemActive())
			_selected_technique = index;
		if (ImGui::IsItemClicked())
			_focused_effect = technique.effect_index;
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
			hovered_technique_index = index;

		// Display tooltip
		if (const std::string_view tooltip = compile_success ? technique.annotation_as_string("ui_tooltip") : effect.errors;
			!tooltip.empty() && ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			if (!compile_success) ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);
			ImGui::TextUnformatted(tooltip.data());
			if (!compile_success) ImGui::PopStyleColor();
			ImGui::EndTooltip();
		}

		// Create context menu
		if (ImGui::BeginPopupContextItem("##context"))
		{
			ImGui::TextUnformatted(technique.name.c_str());
			ImGui::Separator();

			if (imgui_key_input("##toggle_key", technique.toggle_key_data, *_input))
				save_current_preset();
			_ignore_shortcuts |= ImGui::IsItemActive();

			const bool is_not_top = index > 0;
			const bool is_not_bottom = index < _techniques.size() - 1;
			const float button_width = ImGui::CalcItemWidth();

			if (is_not_top && ImGui::Button("Move up", ImVec2(button_width, 0)))
			{
				std::swap(_techniques[index], _techniques[index - 1]);
				save_current_preset();
				ImGui::CloseCurrentPopup();
			}
			if (is_not_bottom && ImGui::Button("Move down", ImVec2(button_width, 0)))
			{
				std::swap(_techniques[index], _techniques[index + 1]);
				save_current_preset();
				ImGui::CloseCurrentPopup();
			}

			if (is_not_top && ImGui::Button("Move to top", ImVec2(button_width, 0)))
			{
				_techniques.insert(_techniques.begin(), std::move(_techniques[index]));
				_techniques.erase(_techniques.begin() + 1 + index);
				save_current_preset();
				ImGui::CloseCurrentPopup();
			}
			if (is_not_bottom && ImGui::Button("Move to bottom", ImVec2(button_width, 0)))
			{
				_techniques.push_back(std::move(_techniques[index]));
				_techniques.erase(_techniques.begin() + index);
				save_current_preset();
				ImGui::CloseCurrentPopup();
			}

			ImGui::Separator();

			const std::string edit_label = "Edit " + effect.source_file.filename().u8string() + "##edit";
			if (ImGui::Button(edit_label.c_str(), ImVec2(button_width, 0)))
			{
				_selected_effect = technique.effect_index;
				_selected_effect_changed = true;
				_show_code_editor = true;
				ImGui::CloseCurrentPopup();
			}

			enum class condition { pass, input, output } condition = condition::pass;
			size_t selected_index = 0;

			if (effect.runtime_loaded && (_renderer_id & 0xF0000) == 0)
			{
				if (imgui_popup_button("Show Results..", button_width))
				{
					if (ImGui::MenuItem("Generated HLSL"))
						condition = condition::input, selected_index = 0;

					ImGui::Separator();

					for (size_t i = 0; effect.module.entry_points.size() > i; ++i)
						if (ImGui::MenuItem(effect.module.entry_points[i].name.c_str()))
							condition = condition::output, selected_index = i;

					ImGui::EndPopup();
				}
			}
			else if ((_renderer_id & 0x10000) && ImGui::Button("Show Results..", ImVec2(button_width, 0)))
			{
				condition = condition::input;
				selected_index = 0;
			}

			if (condition != condition::pass)
			{
				std::string source_code;
				if (condition == condition::input)
					source_code = effect.preamble + effect.module.hlsl;
				else if (condition == condition::output)
					source_code = effect.module.entry_points[selected_index].assembly;

				_editor.set_text(source_code);
				_editor.set_readonly(true);
				_selected_effect = std::numeric_limits<size_t>::max();
				_selected_effect_changed = false; // Prevent editor from being cleared, since we already set the text here
				_show_code_editor = true;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		if (technique.toggle_key_data[0] != 0 && compile_success)
		{
			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 180);
			ImGui::TextDisabled("%s", reshade::input::key_name(technique.toggle_key_data).c_str());
		}

		if (draw_border)
			ImGui::Separator();

		ImGui::PopID();
	}

	// Move the selected technique to the position of the mouse in the list
	if (_selected_technique < _techniques.size() && ImGui::IsMouseDragging())
	{
		if (hovered_technique_index < _techniques.size() && hovered_technique_index != _selected_technique)
		{
			std::swap(_techniques[hovered_technique_index], _techniques[_selected_technique]);
			_selected_technique = hovered_technique_index;
			save_current_preset();
		}
	}
	else
	{
		_selected_technique = std::numeric_limits<size_t>::max();
	}
}

void reshade::runtime::draw_preset_explorer()
{
	static const std::filesystem::path reshade_container_path = g_reshade_dll_path.parent_path();
	const ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;
	const float root_window_width = ImGui::GetWindowContentRegionWidth();

	std::error_code ec;

	enum class condition { pass, select, popup_add, create, backward, forward, reload, cancel };
	condition condition = condition::pass;

	if (is_loading())
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

	if (ImGui::ButtonEx("<", ImVec2(button_size, 0), ImGuiButtonFlags_NoNavFocus | (is_loading() ? ImGuiButtonFlags_Disabled : 0)))
		if (condition = condition::backward, _current_browse_path = _current_preset_path.parent_path().lexically_proximate(reshade_container_path);
			_current_browse_path == L".")
			_current_browse_path.clear();

	if (ImGui::SameLine(0, button_spacing);
		ImGui::ButtonEx(">", ImVec2(button_size, 0), ImGuiButtonFlags_NoNavFocus | (is_loading() ? ImGuiButtonFlags_Disabled : 0)))
		if (condition = condition::forward, _current_browse_path = _current_preset_path.parent_path().lexically_proximate(reshade_container_path);
			_current_browse_path == L".")
			_current_browse_path.clear();

	if (is_loading())
		ImGui::PopStyleColor();

	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
	if (ImGui::SameLine(0, button_spacing);
		ImGui::ButtonEx(_current_preset_path.stem().u8string().c_str(), ImVec2(root_window_width - (button_spacing + button_size) * 3, 0), ImGuiButtonFlags_NoNavFocus))
		if (ImGui::OpenPopup("##explore"), _browse_path_is_input_mode = _imgui_context->IO.KeyCtrl,
			_current_browse_path = _current_preset_path.parent_path().lexically_proximate(reshade_container_path);
			_current_browse_path == L".")
			_current_browse_path.clear();
		else if (_current_browse_path.has_filename())
			_current_browse_path += L'\\';
	ImGui::PopStyleVar();

	if (ImGui::SameLine(0, button_spacing); ImGui::ButtonEx("+", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_NoNavFocus))
		condition = condition::popup_add, _current_browse_path = _current_preset_path.parent_path();

	ImGui::SetNextWindowPos(cursor_pos - _imgui_context->Style.WindowPadding);
	const bool is_explore_open = ImGui::BeginPopup("##explore");

	if (_browse_path_is_input_mode && !is_explore_open)
		_browse_path_is_input_mode = false;

	if (is_explore_open)
	{
		const bool is_popup_open = ImGui::IsPopupOpen("##name");
		bool browse_path_is_editing = false;

		if (is_loading())
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

		if (ImGui::ButtonEx("<", ImVec2(button_size, 0), ImGuiButtonFlags_NoNavFocus | (is_loading() ? ImGuiButtonFlags_Disabled : 0)))
			condition = condition::backward;

		if (ImGui::SameLine(0, button_spacing);
			ImGui::ButtonEx(">", ImVec2(button_size, 0), ImGuiButtonFlags_NoNavFocus | (is_loading() ? ImGuiButtonFlags_Disabled : 0)))
			condition = condition::forward;

		if (is_loading())
			ImGui::PopStyleColor();

		ImGui::SameLine(0, button_spacing);
		if (_browse_path_is_input_mode)
		{
			if (ImGui::IsWindowAppearing())
				ImGui::SetKeyboardFocusHere();

			char buf[_MAX_PATH]{};
			_current_browse_path.u8string().copy(buf, sizeof(buf) - 1);

			const bool is_edited = ImGui::InputTextEx("##path", buf, sizeof(buf), ImVec2(root_window_width - (button_spacing + button_size) * 3, 0), ImGuiInputTextFlags_None);
			const bool is_returned = ImGui::IsKeyPressedMap(ImGuiKey_Enter);

			if (ImGui::IsItemActivated())
				_imgui_context->InputTextState.ClearSelection();

			browse_path_is_editing = ImGui::IsItemActive();

			if (!is_popup_open && (is_edited || is_returned))
			{
				std::filesystem::path input_preset_path = std::filesystem::u8path(buf);
				std::filesystem::file_type file_type = std::filesystem::status(reshade_container_path / input_preset_path, ec).type();

				if (is_edited && ec.value() != 0x7b) // 0x7b: ERROR_INVALID_NAME
					if (_current_browse_path = input_preset_path; buf[0] == '\0')
						if (_browse_path_is_input_mode = false,
							_current_browse_path = _current_preset_path.parent_path().lexically_proximate(reshade_container_path);
							_current_browse_path == L".")
							_current_browse_path.clear();

				if (is_returned)
				{
					if (is_loading())
						condition = condition::pass;
					else if (_current_browse_path.empty())
						condition = condition::cancel;
					else if (ec.value() == 0x7b) // 0x7b: ERROR_INVALID_NAME
						condition = condition::pass;
					else if (file_type == std::filesystem::file_type::directory)
						condition = condition::popup_add;
					else if (_current_browse_path.extension() == L".ini" || _current_browse_path.extension() == L".txt")
						if (file_type == std::filesystem::file_type::not_found)
							condition = condition::create;
						else
							condition = condition::select;
					else if (input_preset_path += L".ini", file_type = std::filesystem::status(reshade_container_path / input_preset_path, ec).type(); ec.value() == 0x7b) // 0x7b: ERROR_INVALID_NAME
						condition = condition::pass;
					else if (file_type == std::filesystem::file_type::directory)
						condition = condition::popup_add;
					else if (file_type == std::filesystem::file_type::not_found)
						condition = condition::create;
					else
						condition = condition::select;

					if (condition == condition::select)
						if (reshade::ini_file(reshade_container_path / input_preset_path).has("", "Techniques"))
							_current_preset_path = reshade_container_path / input_preset_path;
						else
							condition = condition::pass;
					else if (condition == condition::create)
						_current_preset_path = reshade_container_path / input_preset_path;
					else if (condition == condition::popup_add)
						_current_browse_path = input_preset_path;

					if (condition == condition::pass)
						ImGui::ActivateItem(ImGui::GetID("##path"));
				}
			}
		}
		else
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
			if (ImGui::ButtonEx(_current_preset_path.stem().u8string().c_str(), ImVec2(root_window_width - (button_spacing + button_size) * 3, 0), ImGuiButtonFlags_NoNavFocus))
				if (ImGui::ActivateItem(ImGui::GetID("##path")), _browse_path_is_input_mode = _imgui_context->IO.KeyCtrl; _browse_path_is_input_mode)
					if (_current_browse_path.has_filename() && std::filesystem::is_directory(reshade_container_path / _current_browse_path, ec))
						_current_browse_path += L'\\';
					else
						condition = condition::pass;
				else
					condition = condition::cancel;
			else if (is_loading())
				condition = condition::pass;
			else if (ImGui::IsKeyPressedMap(ImGuiKey_Enter))
				condition = condition::reload;
			ImGui::PopStyleVar();
		}

		if (!is_popup_open && (!_browse_path_is_input_mode || !browse_path_is_editing))
		{
			bool activate = true;

			if (const std::wstring &ch = _input->text_input(); !ch.empty() && L'~' >= ch[0] && ch[0] >= L'!')
				if (ch[0] != L'\\' && ch[0] != L'/' && _current_browse_path.has_filename() && std::filesystem::is_directory(reshade_container_path / _current_browse_path, ec))
					_current_browse_path += L'\\' + ch;
				else
					_current_browse_path += ch;
			else
				activate = !_current_browse_path.empty() && ImGui::IsKeyPressedMap(ImGuiKey_Backspace, false);

			if (activate)
				ImGui::ActivateItem(ImGui::GetID("##path")), _browse_path_is_input_mode = true;
		}
	}

	if (condition == condition::backward || condition == condition::forward)
		if (!switch_to_next_preset(condition == condition::backward))
			condition = condition::pass;

	if (is_explore_open)
	{
		std::filesystem::path preset_container_path = (reshade_container_path / _current_browse_path).lexically_normal();
		std::filesystem::path presets_filter_text = _current_browse_path.filename();

		if (std::filesystem::is_directory(preset_container_path, ec))
			presets_filter_text.clear();
		else if (!presets_filter_text.empty())
			preset_container_path = preset_container_path.parent_path();

		if (ImGui::SameLine(0, button_spacing); ImGui::ButtonEx("+", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_NoNavFocus))
			if (const std::filesystem::file_type file_type = std::filesystem::status(reshade_container_path / _current_browse_path, ec).type(); ec.value() != 0x7b) // 0x7b: ERROR_INVALID_NAME
				if (condition = condition::popup_add; _current_browse_path.has_filename())
					if (file_type == std::filesystem::file_type::not_found || file_type != std::filesystem::file_type::directory)
						_current_browse_path = _current_browse_path.parent_path();

		if (ImGui::IsWindowAppearing() || condition == condition::backward || condition == condition::forward)
			ImGui::SetNextWindowFocus();

		if (ImGui::BeginChild("##paths", ImVec2(0, 300), true, ImGuiWindowFlags_NavFlattened))
		{
			if (ImGui::Selectable(".."))
			{
				if (_current_browse_path = std::filesystem::absolute(reshade_container_path / _current_browse_path, ec); !_current_browse_path.has_filename())
					_current_browse_path = _current_browse_path.parent_path();

				while (!std::filesystem::is_directory(_current_browse_path, ec) && _current_browse_path.parent_path() != _current_browse_path)
					_current_browse_path = _current_browse_path.parent_path();
				_current_browse_path = _current_browse_path.parent_path();

				if (std::filesystem::equivalent(reshade_container_path, _current_browse_path, ec))
					_current_browse_path.clear();
				else if (std::equal(reshade_container_path.begin(), reshade_container_path.end(), _current_browse_path.begin()))
					_current_browse_path = _current_browse_path.lexically_proximate(reshade_container_path);
			}

			std::vector<std::filesystem::directory_entry> preset_paths;
			for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(preset_container_path, std::filesystem::directory_options::skip_permission_denied, ec))
				if (entry.is_directory(ec))
					if (ImGui::Selectable(("<DIR> " + entry.path().filename().u8string()).c_str()))
						if (std::filesystem::equivalent(reshade_container_path, entry, ec))
							_current_browse_path.clear();
						else if (std::equal(reshade_container_path.begin(), reshade_container_path.end(), entry.path().begin()))
							_current_browse_path = entry.path().lexically_proximate(reshade_container_path);
						else
							_current_browse_path = entry;
					else {}
				else if (entry.path().extension() == L".ini" || entry.path().extension() == L".txt")
					preset_paths.push_back(entry);

			if (is_loading())
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true),
				ImGui::PushStyleColor(ImGuiCol_Text, _imgui_context->Style.Colors[ImGuiCol_TextDisabled]);

			for (const std::filesystem::directory_entry &entry : preset_paths)
			{
				const bool is_current_preset = std::filesystem::equivalent(entry, _current_preset_path, ec);

				if (!is_current_preset && !presets_filter_text.empty())
					if (const std::wstring preset_name(entry.path().stem()), &filter_text = presets_filter_text.native();
						std::search(preset_name.begin(), preset_name.end(), filter_text.begin(), filter_text.end(), [](const wchar_t c1, const wchar_t c2) { return towlower(c1) == towlower(c2); }) == preset_name.end())
						continue;

				if (ImGui::Selectable(entry.path().filename().u8string().c_str(), static_cast<bool>(is_current_preset)))
					if (reshade::ini_file(entry).has("", "Techniques"))
						condition = condition::select, _current_preset_path = entry;
					else
						condition = condition::pass;

				if (is_current_preset)
					if (_current_preset_is_covered && !ImGui::IsWindowAppearing())
						_current_preset_is_covered = false, ImGui::SetScrollHereY();
					else if (condition == condition::backward || condition == condition::forward)
						ImGui::SetScrollHereY();
			}

			if (is_loading())
				ImGui::PopStyleColor(),
				ImGui::PopItemFlag();
		}
		ImGui::EndChild();
	}

	if (condition == condition::popup_add)
		condition = condition::pass, ImGui::OpenPopup("##name");

	ImGui::SetNextWindowPos(cursor_pos + ImVec2(root_window_width + button_size - 230, button_size));
	if (ImGui::BeginPopup("##name"))
	{
		char filename[_MAX_PATH]{};
		ImGui::InputText("Name", filename, sizeof(filename));

		if (filename[0] == '\0' && ImGui::IsKeyPressedMap(ImGuiKey_Backspace))
			ImGui::CloseCurrentPopup();
		else if (ImGui::IsWindowAppearing())
			ImGui::SetKeyboardFocusHere();
		else if (ImGui::IsKeyPressedMap(ImGuiKey_Enter))
		{
			std::filesystem::path focus_preset_path = reshade_container_path / _current_browse_path / std::filesystem::u8path(filename);

			if (focus_preset_path.extension() != L".ini" && focus_preset_path.extension() != L".txt")
				focus_preset_path += L".ini";

			if (is_loading())
				condition = condition::pass;
			else if (!focus_preset_path.has_stem())
				condition = condition::pass;
			else if (const std::filesystem::file_type file_type = std::filesystem::status(focus_preset_path, ec).type(); ec.value() == 0x7b) // 0x7b: ERROR_INVALID_NAME
				condition = condition::pass;
			else if (file_type == std::filesystem::file_type::directory)
				condition = condition::pass;
			else if (file_type == std::filesystem::file_type::not_found)
				condition = condition::create, _current_preset_path = focus_preset_path;
			else if (reshade::ini_file(focus_preset_path).has("", "Techniques"))
				condition = condition::select, _current_preset_path = focus_preset_path;
			else
				condition = condition::pass;

			if (condition == condition::pass)
				ImGui::SetKeyboardFocusHere();
			else
				ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (condition != condition::pass)
	{
		if (condition != condition::cancel)
		{
			_show_splash = true;

			save_config();
			load_current_preset();
		}
		if (is_explore_open && condition != condition::backward && condition != condition::forward)
			ImGui::CloseCurrentPopup();
	}

	if (is_explore_open)
		ImGui::EndPopup();
	else if (!_current_preset_is_covered)
		_current_preset_is_covered = true;
}

#endif
