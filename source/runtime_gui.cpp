/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_GUI

#include "dll_log.hpp"
#include "version.h"
#include "runtime.hpp"
#include "runtime_config.hpp"
#include "runtime_objects.hpp"
#include "input.hpp"
#include "imgui_widgets.hpp"
#include <cassert>
#include <fstream>
#include <algorithm>
#include <shellapi.h>

static std::string g_window_state_path;
extern volatile long g_network_traffic;
extern std::filesystem::path g_reshade_dll_path;
extern std::filesystem::path g_target_executable_path;

static const ImVec4 COLOR_RED = ImColor(240, 100, 100);
static const ImVec4 COLOR_YELLOW = ImColor(204, 204, 0);

void reshade::runtime::init_ui()
{
	if (g_window_state_path.empty())
	{
		std::filesystem::path reshadegui_ini_path = g_reshade_config_path;
		reshadegui_ini_path.replace_filename("ReShadeGUI.ini");
		g_window_state_path = reshadegui_ini_path.u8string();
	}

	// Default shortcut: Home
	_menu_key_data[0] = 0x24;
	_menu_key_data[1] = false;
	_menu_key_data[2] = false;
	_menu_key_data[3] = false;

	_editor.set_readonly(true);
	_viewer.set_readonly(true); // Viewer is always read-only
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
	imgui_io.BackendFlags = ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_RendererHasVtxOffset;

	// Disable rounding by default
	imgui_style.GrabRounding = 0.0f;
	imgui_style.FrameRounding = 0.0f;
	imgui_style.ChildRounding = 0.0f;
	imgui_style.ScrollbarRounding = 0.0f;
	imgui_style.WindowRounding = 0.0f;
	imgui_style.WindowBorderSize = 0.0f;

	ImGui::SetCurrentContext(nullptr);

	subscribe_to_ui("Home", [this]() { draw_ui_home(); });
	subscribe_to_ui("Settings", [this]() { draw_ui_settings(); });
	subscribe_to_ui("Statistics", [this]() { draw_ui_statistics(); });
	subscribe_to_ui("Log", [this]() { draw_ui_log(); });
	subscribe_to_ui("About", [this]() { draw_ui_about(); });

	_load_config_callables.push_back([this](const ini_file &config) {
		bool save_imgui_window_state = false;

		config.get("INPUT", "KeyMenu", _menu_key_data);
		config.get("INPUT", "InputProcessing", _input_processing_mode);

		config.get("GENERAL", "ShowClock", _show_clock);
		config.get("GENERAL", "ShowFPS", _show_fps);
		config.get("GENERAL", "ShowFrameTime", _show_frametime);
		config.get("GENERAL", "ShowScreenshotMessage", _show_screenshot_message);
		config.get("GENERAL", "FPSPosition", _fps_pos);
		config.get("GENERAL", "ClockFormat", _clock_format);
		config.get("GENERAL", "NoFontScaling", _no_font_scaling);
		config.get("GENERAL", "SaveWindowState", save_imgui_window_state);
		config.get("GENERAL", "TutorialProgress", _tutorial_index);
		config.get("GENERAL", "NewVariableUI", _variable_editor_tabs);
		config.get("GENERAL", "VariableUIHeight", _variable_editor_height);

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

		_imgui_context->IO.IniFilename = save_imgui_window_state ? g_window_state_path.c_str() : nullptr;

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

		_viewer.set_palette(_editor.get_palette());
	});
	_save_config_callables.push_back([this](ini_file &config) {
		config.set("INPUT", "KeyMenu", _menu_key_data);
		config.set("INPUT", "InputProcessing", _input_processing_mode);

		config.set("GENERAL", "ShowClock", _show_clock);
		config.set("GENERAL", "ShowFPS", _show_fps);
		config.set("GENERAL", "ShowFrameTime", _show_frametime);
		config.set("GENERAL", "ShowScreenshotMessage", _show_screenshot_message);
		config.set("GENERAL", "FPSPosition", _fps_pos);
		config.set("GENERAL", "ClockFormat", _clock_format);
		config.set("GENERAL", "NoFontScaling", _no_font_scaling);
		config.set("GENERAL", "SaveWindowState", _imgui_context->IO.IniFilename != nullptr);
		config.set("GENERAL", "TutorialProgress", _tutorial_index);
		config.set("GENERAL", "NewVariableUI", _variable_editor_tabs);
		config.set("GENERAL", "VariableUIHeight", _variable_editor_height);

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
	ImFontAtlas *const atlas = _imgui_context->IO.Fonts;
	// Remove any existing fonts from atlas first
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
		}
	}

	_show_splash = true;
	_rebuild_font_atlas = false;

	int width, height;
	unsigned char *pixels;
	atlas->GetTexDataAsRGBA32(&pixels, &width, &height);

	// Create font atlas texture and upload it
	if (_imgui_font_atlas != nullptr)
		destroy_texture(*_imgui_font_atlas);
	if (_imgui_font_atlas == nullptr)
		_imgui_font_atlas = std::make_unique<texture>();

	_imgui_font_atlas->width = width;
	_imgui_font_atlas->height = height;
	_imgui_font_atlas->format = reshadefx::texture_format::rgba8;
	_imgui_font_atlas->unique_name = "ImGUI Font Atlas";
	if (init_texture(*_imgui_font_atlas))
		upload_texture(*_imgui_font_atlas, pixels);
	else
		_imgui_font_atlas.reset();
}

void reshade::runtime::draw_ui()
{
	assert(_is_initialized);

	const bool show_splash = _show_splash && (is_loading() || !_reload_compile_queue.empty() || (_reload_count <= 1 && (_last_present_time - _last_reload_time) < std::chrono::seconds(5)));
	// Do not show this message in the same frame the screenshot is taken (so that it won't show up on the UI screenshot)
	const bool show_screenshot_message = (_show_screenshot_message || !_screenshot_save_success) && !_should_save_screenshot && (_last_present_time - _last_screenshot_time) < std::chrono::seconds(_screenshot_save_success ? 3 : 5);

	if (_show_menu && !_ignore_shortcuts && !_imgui_context->IO.NavVisible && _input->is_key_pressed(0x1B /* VK_ESCAPE */))
		_show_menu = false; // Close when pressing the escape button and not currently navigating with the keyboard
	else if (!_ignore_shortcuts && _input->is_key_pressed(_menu_key_data, _force_shortcut_modifiers) && _imgui_context->ActiveId == 0)
		_show_menu = !_show_menu;

	_ignore_shortcuts = false;
	_effects_expanded_state &= 2;

	if (_rebuild_font_atlas)
		build_font_atlas();
	if (_imgui_font_atlas == nullptr)
		return; // Cannot render UI without font atlas

	ImGui::SetCurrentContext(_imgui_context);
	auto &imgui_io = _imgui_context->IO;
	imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
	imgui_io.MouseDrawCursor = _show_menu && (!_should_save_screenshot || !_screenshot_save_ui);
	imgui_io.MousePos.x = static_cast<float>(_input->mouse_position_x());
	imgui_io.MousePos.y = static_cast<float>(_input->mouse_position_y());
	imgui_io.DisplaySize.x = static_cast<float>(_width);
	imgui_io.DisplaySize.y = static_cast<float>(_height);
	imgui_io.Fonts->TexID = _imgui_font_atlas->impl;

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
	if (show_splash || show_screenshot_message || !_preset_save_success || (!_show_menu && _tutorial_index == 0))
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

		if (!_preset_save_success)
		{
			ImGui::TextColored(COLOR_RED, "Unable to save current preset. Make sure you have write permissions to %s.", _current_preset_path.u8string().c_str());
		}
		else if (show_screenshot_message)
		{
			if (!_screenshot_save_success)
				if (std::error_code ec; std::filesystem::exists(_screenshot_path, ec))
					ImGui::TextColored(COLOR_RED, "Unable to save screenshot because of an internal error (the format may not be supported).");
				else
					ImGui::TextColored(COLOR_RED, "Unable to save screenshot because path doesn't exist: %s.", _screenshot_path.u8string().c_str());
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

			if (!_last_shader_reload_successful)
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
		float window_height = _imgui_context->FontBaseSize * _fps_scale + _imgui_context->Style.ItemSpacing.y;
		window_height *= (_show_clock ? 1 : 0) + (_show_fps ? 1 : 0) + (_show_frametime ? 1 : 0);
		window_height += _imgui_context->Style.FramePadding.y * 4.0f;

		ImVec2 fps_window_pos(5, 5);
		if (_fps_pos % 2)
			fps_window_pos.x = imgui_io.DisplaySize.x - 200.0f;
		if (_fps_pos > 1)
			fps_window_pos.y = imgui_io.DisplaySize.y - window_height - 5;

		ImGui::SetNextWindowPos(fps_window_pos);
		ImGui::SetNextWindowSize(ImVec2(200.0f, window_height));
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

			ImFormatString(temp, sizeof(temp), _clock_format != 0 ? "%02u:%02u:%02u" : "%02u:%02u", hour, minute, seconds);
			if (_fps_pos % 2) // Align text to the right of the window
				ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}
		if (_show_fps)
		{
			ImFormatString(temp, sizeof(temp), "%.0f fps", imgui_io.Framerate);
			if (_fps_pos % 2)
				ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}
		if (_show_frametime)
		{
			ImFormatString(temp, sizeof(temp), "%5.2f ms", 1000.0f / imgui_io.Framerate);
			if (_fps_pos % 2)
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
			ImGui::DockBuilderAddNode(root_space_id, ImGuiDockNodeFlags_DockSpace);
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
			ImGui::DockBuilderDockWindow("###viewer", right_space_id);

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

		for (const auto &widget : _menu_callables)
		{
			if (ImGui::Begin(widget.first.c_str(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) // No focus so that window state is preserved between opening/closing the UI
				widget.second();
			ImGui::End();
		}

		if (_show_code_editor)
		{
			const std::string title = "Editing " + _editor_file.filename().u8string() + " ###editor";
			if (ImGui::Begin(title.c_str(), &_show_code_editor))
				draw_code_editor();
			ImGui::End();
		}
		if (_show_code_viewer)
		{
			if (ImGui::Begin("Viewing generated code###viewer", &_show_code_viewer))
				draw_code_viewer();
			ImGui::End();
		}
	}

	if (_preview_texture != nullptr && _effects_enabled)
	{
		if (!_show_menu)
		{
			// Create a temporary viewport window to attach image to when menu is not open
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

		// The preview texture is unset in 'unload_effects', so should not be able to reach this while loading
		assert(!is_loading() && _reload_compile_queue.empty());

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

		ImGui::FindWindowByName("Viewport")->DrawList->AddImage(_preview_texture, preview_min, preview_max, ImVec2(0, 0), ImVec2(1, 1), _preview_size[2]);
	}

	// Render ImGui widgets and windows
	ImGui::Render();

	_input->block_mouse_input(_input_processing_mode != 0 && _show_menu && (imgui_io.WantCaptureMouse || _input_processing_mode == 2));
	_input->block_keyboard_input(_input_processing_mode != 0 && _show_menu && (imgui_io.WantCaptureKeyboard || _input_processing_mode == 2));

	if (ImDrawData *const draw_data = ImGui::GetDrawData();
		draw_data != nullptr && draw_data->CmdListsCount != 0 && draw_data->TotalVtxCount != 0)
		render_imgui_draw_data(draw_data);
}

void reshade::runtime::draw_ui_home()
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
		return; // Cannot show techniques and variables while effects are loading, since they are being modified in other different threads during that time
	}

	if (!_effects_enabled)
		ImGui::Text("Effects are disabled. Press '%s' to enable them again.", input::key_name(_effects_key_data).c_str());

	if (!_last_shader_reload_successful)
	{
		std::string shader_list;
		for (const effect &effect : _effects)
			if (!effect.compile_sucess)
				shader_list += ' ' + effect.source_file.filename().u8string() + ',';

		// Make sure there are actually effects that failed to compile, since the last reload flag may not have been reset
		if (shader_list.empty())
		{
			_last_shader_reload_successful = true;
		}
		else
		{
			shader_list.pop_back();
			ImGui::TextColored(COLOR_RED, "Some shaders failed to compile:%s", shader_list.c_str());
			ImGui::Spacing();
		}
	}
	if (!_last_texture_reload_successful)
	{
		std::string texture_list;
		for (const texture &texture : _textures)
			if (!texture.loaded && !texture.annotation_as_string("source").empty())
				texture_list += ' ' + texture.unique_name + ',';

		if (texture_list.empty())
		{
			_last_texture_reload_successful = true;
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
		const bool show_clear_button = strcmp(_effect_filter, "Search") != 0 && _effect_filter[0] != '\0';

		ImGui::PushItemWidth((_variable_editor_tabs ? -10.0f : -20.0f) * _font_size - (show_clear_button ? ImGui::GetFrameHeight() + _imgui_context->Style.ItemSpacing.x : 0));
		if (ImGui::InputText("##filter", _effect_filter, sizeof(_effect_filter), ImGuiInputTextFlags_AutoSelectAll))
		{
			_effects_expanded_state = 3;

			const std::string_view filter_view = _effect_filter;
			for (technique &technique : _techniques)
				technique.hidden = technique.annotation_as_int("hidden") != 0 || (
					!filter_view.empty() && // Reset visibility state if filter is empty
					std::search(technique.name.begin(), technique.name.end(), filter_view.begin(), filter_view.end(), [](auto c1, auto c2) { return tolower(c1) == tolower(c2); }) == technique.name.end() &&
					_effects[technique.effect_index].source_file.filename().u8string().find(filter_view) == std::string::npos);
		}
		else if (!ImGui::IsItemActive() && _effect_filter[0] == '\0')
		{
			strcpy_s(_effect_filter, "Search");
		}
		ImGui::PopItemWidth();

		ImGui::SameLine();

		if (show_clear_button && ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0)))
		{
			strcpy_s(_effect_filter, "Search");
			// Reset visibility state
			for (technique &technique : _techniques)
				technique.hidden = technique.annotation_as_int("hidden") != 0;
		}

		ImGui::SameLine();

		if (ImGui::Button("Active to top", ImVec2(10 * _font_size - _imgui_context->Style.ItemSpacing.x, 0)))
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

		if (ImGui::Button((_effects_expanded_state & 2) ? "Collapse all" : "Expand all", ImVec2(10 * _font_size - _imgui_context->Style.ItemSpacing.x, 0)))
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

		const float bottom_height = _performance_mode ? ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y : (_variable_editor_height + (_tutorial_index == 3 ? 175 : 0));

		if (ImGui::BeginChild("##techniques", ImVec2(0, -bottom_height), true))
			draw_technique_editor();
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
			_variable_editor_height -= _imgui_context->IO.MouseDelta.y;
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
				"This will recompile all shaders into a more optimal representation that can give a performance boost, but will disable variable tweaking and this list.";

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

		if (ImGui::Button("Reload", ImVec2(-11.5f * _font_size, 0)))
		{
			load_effects();
		}

		ImGui::SameLine();

		if (ImGui::Checkbox("Performance Mode", &_performance_mode))
		{
			save_config();
			load_effects(); // Reload effects after switching
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
void reshade::runtime::draw_ui_settings()
{
	bool modified = false;
	bool reload_style = false;

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		modified |= imgui_key_input("Overlay Key", _menu_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();

		modified |= imgui_key_input("Effect Toggle Key", _effects_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();
		modified |= imgui_key_input("Effect Reload Key", _reload_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();

		modified |= imgui_key_input("Previous Preset Key", _prev_preset_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();
		modified |= imgui_key_input("Next Preset Key", _next_preset_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();

		modified |= ImGui::SliderInt("Preset transition", reinterpret_cast<int *>(&_preset_transition_delay), 0, 10 * 1000);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Makes a smooth transition, but only for floating point values.\nRecommended for multiple presets that contain the same shaders, otherwise set this to zero.\nValues are in milliseconds.");

		modified |= ImGui::Combo("Input processing", &_input_processing_mode,
			"Pass on all input\0"
			"Block input when cursor is on overlay\0"
			"Block all input when overlay is visible\0");

		ImGui::Spacing();

		modified |= imgui_path_list("Effect search paths", _effect_search_paths, _file_selection_path, g_reshade_dll_path.parent_path());
		modified |= imgui_path_list("Texture search paths", _texture_search_paths, _file_selection_path, g_reshade_dll_path.parent_path());

		if (ImGui::Button("Restart tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
			_tutorial_index = 0;
	}

	if (ImGui::CollapsingHeader("Screenshots", ImGuiTreeNodeFlags_DefaultOpen))
	{
		modified |= imgui_key_input("Screenshot Key", _screenshot_key_data, *_input);
		_ignore_shortcuts |= ImGui::IsItemActive();

		modified |= imgui_directory_input_box("Screenshot Path", _screenshot_path, _file_selection_path);

		const int hour = _date[3] / 3600;
		const int minute = (_date[3] - hour * 3600) / 60;
		const int seconds = _date[3] - hour * 3600 - minute * 60;

		char timestamp[21];
		sprintf_s(timestamp, " %.4d-%.2d-%.2d %.2d-%.2d-%.2d", _date[0], _date[1], _date[2], hour, minute, seconds);

		std::string screenshot_naming_items;
		screenshot_naming_items += g_target_executable_path.stem().string() + timestamp + '\0';
		screenshot_naming_items += g_target_executable_path.stem().string() + timestamp + ' ' + _current_preset_path.stem().string() + '\0';
		modified |= ImGui::Combo("Screenshot Name", reinterpret_cast<int *>(&_screenshot_naming), screenshot_naming_items.c_str());

		modified |= ImGui::Combo("Screenshot Format", reinterpret_cast<int *>(&_screenshot_format), "Bitmap (*.bmp)\0Portable Network Graphics (*.png)\0JPEG (*.jpeg)\0");

		if (_screenshot_format == 2)
			modified |= ImGui::SliderInt("JPEG Quality", reinterpret_cast<int *>(&_screenshot_jpeg_quality), 1, 100);
		else
			modified |= ImGui::Checkbox("Clear alpha channel", &_screenshot_clear_alpha);

		modified |= ImGui::Checkbox("Include current preset", &_screenshot_include_preset);
		modified |= ImGui::Checkbox("Save before and after images", &_screenshot_save_before);
		modified |= ImGui::Checkbox("Save separate user interface image", &_screenshot_save_ui);
	}

	if (ImGui::CollapsingHeader("User Interface", ImGuiTreeNodeFlags_DefaultOpen))
	{
		modified |= ImGui::Checkbox("Show screenshot message", &_show_screenshot_message);

		bool save_imgui_window_state = _imgui_context->IO.IniFilename != nullptr;
		if (ImGui::Checkbox("Save window state (ReShadeGUI.ini)", &save_imgui_window_state))
		{
			modified = true;
			_imgui_context->IO.IniFilename = save_imgui_window_state ? g_window_state_path.c_str() : nullptr;
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
					_viewer.get_palette_index(i) = _editor.get_palette_index(i) = ImGui::ColorConvertFloat4ToU32(color);
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
		modified |= ImGui::Combo("Position on Screen", &_fps_pos, "Top Left\0Top Right\0Bottom Left\0Bottom Right\0");
	}

	if (modified)
		save_config();
	if (reload_style) // Style is applied in "load_config()".
		load_config();
}
void reshade::runtime::draw_ui_statistics()
{
	unsigned int cpu_digits = 1;
	unsigned int gpu_digits = 1;
	uint64_t post_processing_time_cpu = 0;
	uint64_t post_processing_time_gpu = 0;

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
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
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
		ImGui::Text("%*.3f ms CPU", cpu_digits + 4, post_processing_time_cpu * 1e-6f);

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
			ImGui::Text("%*.3f ms GPU", gpu_digits + 4, (post_processing_time_gpu * 1e-6f));

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
				ImGui::Text("%*.3f ms CPU", cpu_digits + 4, technique.average_cpu_duration * 1e-6f);
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
				ImGui::Text("%*.3f ms GPU", gpu_digits + 4, technique.average_gpu_duration * 1e-6f);
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

		static_assert(std::size(texture_formats) - 1 == static_cast<size_t>(reshadefx::texture_format::rgb10a2));

		const float total_width = ImGui::GetWindowContentRegionWidth();
		unsigned int texture_index = 0;
		const unsigned int num_columns = static_cast<unsigned int>(std::ceilf(total_width / (50.0f * _font_size)));
		const float single_image_width = (total_width / num_columns) - 5.0f;

		// Variables used to calculate memory size of textures
		ldiv_t memory_view;
		const char *memory_size_unit;
		uint32_t post_processing_memory_size = 0;

		for (const auto &texture : _textures)
		{
			if (!_effects[texture.effect_index].rendering || texture.impl == nullptr || texture.impl_reference != texture_reference::none)
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

			ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s%s", texture.unique_name.c_str(), texture.shared ? " (Shared)" : "");
			ImGui::Text("%ux%u | %u mipmap(s) | %s | %ld.%03ld %s",
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

			if (bool check = _preview_texture == texture.impl && _preview_size[0] == 0; ImGui::RadioButton("Preview scaled", check)) {
				_preview_size[0] = 0;
				_preview_size[1] = 0;
				_preview_texture = !check ? texture.impl : nullptr;
			}
			ImGui::SameLine();
			if (bool check = _preview_texture == texture.impl && _preview_size[0] != 0; ImGui::RadioButton("Preview original", check)) {
				_preview_size[0] = texture.width;
				_preview_size[1] = texture.height;
				_preview_texture = !check ? texture.impl : nullptr;
			}

			bool r = (_preview_size[2] & 0x000000FF) != 0;
			bool g = (_preview_size[2] & 0x0000FF00) != 0;
			bool b = (_preview_size[2] & 0x00FF0000) != 0;
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 0, 0, 1));
			imgui_toggle_button("R", r);
			ImGui::PopStyleColor();
			if (texture.format >= reshadefx::texture_format::rg8)
			{
				ImGui::SameLine(0, 1);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 1, 0, 1));
				imgui_toggle_button("G", g);
				ImGui::PopStyleColor();
				if (texture.format >= reshadefx::texture_format::rgba8)
				{
					ImGui::SameLine(0, 1);
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 1, 1));
					imgui_toggle_button("B", b);
					ImGui::PopStyleColor();
				}
			}
			_preview_size[2] = (r ? 0x000000FF : 0) | (g ? 0x0000FF00 : 0) | (b ? 0x00FF0000 : 0) | 0xFF000000;

			const float aspect_ratio = static_cast<float>(texture.width) / static_cast<float>(texture.height);
			imgui_image_with_checkerboard_background(texture.impl, ImVec2(single_image_width, single_image_width / aspect_ratio), _preview_size[2]);

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

		ImGui::Text("Total memory usage: %ld.%03ld %s", memory_view.quot, memory_view.rem, memory_size_unit);
	}
}
void reshade::runtime::draw_ui_log()
{
	std::filesystem::path log_path = g_reshade_dll_path;
	log_path.replace_extension(".log");

	if (ImGui::Button("Clear Log"))
		// Close and open the stream again, which will clear the file too
		log::open(log_path);

	ImGui::SameLine();
	ImGui::Checkbox("Word Wrap", &_log_wordwrap);
	ImGui::SameLine();

	static ImGuiTextFilter filter; // TODO: Better make this a member of the runtime class, in case there are multiple instances.
	const bool filter_changed = filter.Draw("Filter (inc, -exc)", -150);

	if (ImGui::BeginChild("log", ImVec2(0, 0), true, _log_wordwrap ? 0 : ImGuiWindowFlags_AlwaysHorizontalScrollbar))
	{
		// Limit number of log lines to read, to avoid stalling when log gets too big
		const size_t line_limit = 1000;

		std::error_code ec;
		const auto last_log_size = std::filesystem::file_size(log_path, ec);
		if (filter_changed || _last_log_size != last_log_size)
		{
			_log_lines.clear();
			std::ifstream log_file(log_path);
			for (std::string line; std::getline(log_file, line) && _log_lines.size() < line_limit;)
				if (filter.PassFilter(line.c_str()))
					_log_lines.push_back(line);
			_last_log_size = last_log_size;

			if (_log_lines.size() == line_limit)
				_log_lines.push_back("Log was truncated to reduce memory footprint!");
		}

		ImGuiListClipper clipper(static_cast<int>(_log_lines.size()), ImGui::GetTextLineHeightWithSpacing());
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				ImVec4 textcol = ImGui::GetStyleColorVec4(ImGuiCol_Text);

				if (_log_lines[i].find("ERROR |") != std::string::npos || _log_lines[i].find("error") != std::string::npos)
					textcol = COLOR_RED;
				else if (_log_lines[i].find("WARN  |") != std::string::npos || _log_lines[i].find("warning") != std::string::npos || i == line_limit)
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
	} ImGui::EndChild();
}
void reshade::runtime::draw_ui_about()
{
	ImGui::TextUnformatted("ReShade " VERSION_STRING_PRODUCT);

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
		ImGui::TextUnformatted(R"(Copyright (C) 2009-2017 Tsuda Kageyu. All rights reserved.

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
		ImGui::TextUnformatted(R"(Copyright (C) 2014-2020 Omar Cornut and ImGui contributors

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
	if (ImGui::CollapsingHeader("UTF8-CPP"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2006 Nemanja Trifunovic

Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute, and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the Software is furnished to do so, all subject to the following:

The copyright notices in the Software and this entire statement, including the above license grant, this restriction and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all derivative works of the Software, unless such copies or derivative works are solely in the form of machine-executable object code generated by a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)");
	}
	if (ImGui::CollapsingHeader("stb_image, stb_image_write"))
	{
		ImGui::TextUnformatted("Sean Barrett and contributors");
	}
	if (ImGui::CollapsingHeader("DDS loading from SOIL"))
	{
		ImGui::TextUnformatted("Jonathan \"lonesock\" Dummer");
	}
	if (ImGui::CollapsingHeader("SPIR-V"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2015-2018 The Khronos Group Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and/or associated documentation files (the "Materials"), to deal in the Materials without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Materials, and to permit persons to whom the Materials are furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Materials.)");
	}
	if (ImGui::CollapsingHeader("Vulkan & Vulkan-Loader"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2015-2017 The Khronos Group Inc.
Copyright (C) 2015-2017 Valve Corporation
Copyright (C) 2015-2017 LunarG, Inc.

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.)");
	}
	if (ImGui::CollapsingHeader("Vulkan Memory Allocator"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2017-2020 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)");
	}
	if (ImGui::CollapsingHeader("Solarized"))
	{
		ImGui::TextUnformatted(R"(Copyright (C) 2011 Ethan Schoonover

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)");
	}

	ImGui::PopTextWrapPos();
}

void reshade::runtime::draw_code_editor()
{
	if (ImGui::Button("Save", ImVec2(ImGui::GetContentRegionAvail().x, 0)) || _input->is_key_pressed('S', true, false, false))
	{
		// Write current editor text to file
		const std::string text = _editor.get_text();
		std::ofstream(_editor_file, std::ios::trunc).write(text.c_str(), text.size());

		if (!is_loading() && _selected_effect < _effects.size())
		{
			// Hide splash bar when reloading a single effect file
			_show_splash = false;

			// Reload effect file
			_reload_total_effects = 1;
			_reload_remaining_effects = 1;
			unload_effect(_selected_effect);
			load_effect(_effects[_selected_effect].source_file, _selected_effect);
			assert(_reload_remaining_effects == 0);

			// Re-open current file so that errors are updated
			open_file_in_code_editor(_selected_effect, _editor_file);

			// Reloading an effect file invalidates all textures, but the statistics window may already have drawn references to those, so need to reset it
			ImGui::FindWindowByName("Statistics")->DrawList->CmdBuffer.clear();
		}
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
void reshade::runtime::draw_code_viewer()
{
	ImGui::PushFont(_imgui_context->IO.Fonts->Fonts[1]);
	_viewer.render("##viewer");
	ImGui::PopFont();

	const bool is_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
	_ignore_shortcuts |= is_focused;

	if (is_focused)
		_imgui_context->IO.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
	else
		_imgui_context->IO.ConfigFlags |=  ImGuiConfigFlags_NavEnableKeyboard;
}

void reshade::runtime::draw_preset_explorer()
{
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;
	const float browse_button_width = ImGui::GetWindowContentRegionWidth() - (button_spacing + button_size) * 3;

	std::error_code ec;
	bool reload_preset = false;

	ImGuiButtonFlags button_flags = ImGuiButtonFlags_NoNavFocus;
	if (is_loading())
	{
		button_flags |= ImGuiButtonFlags_Disabled;
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	}

	if (ImGui::ButtonEx("<", ImVec2(button_size, 0), button_flags))
		switch_to_next_preset(_current_browse_path, true);
	ImGui::SameLine(0, button_spacing);
	if (ImGui::ButtonEx(">", ImVec2(button_size, 0), button_flags))
		switch_to_next_preset(_current_browse_path, false);

	ImGui::SameLine(0, button_spacing);
	const ImVec2 popup_pos = ImGui::GetCursorScreenPos() + ImVec2(-_imgui_context->Style.WindowPadding.x, ImGui::GetFrameHeightWithSpacing());

	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
	if (ImGui::ButtonEx(_current_preset_path.stem().u8string().c_str(), ImVec2(browse_button_width, 0), button_flags))
		ImGui::OpenPopup("##explore");
	ImGui::PopStyleVar();

	ImGui::SameLine(0, button_spacing);
	if (ImGui::ButtonEx("+", ImVec2(button_size, 0), button_flags | ImGuiButtonFlags_PressedOnClick))
	{
		ImGui::OpenPopup("##name");

		// Reset base path for new preset
		_current_browse_path = _current_preset_path.parent_path();
	}

	if (is_loading())
		ImGui::PopStyleColor();

	if (ImGui::BeginPopup("##explore"))
	{
		ImGui::SetWindowPos(popup_pos);

		char buf[260] = "";
		_current_browse_path.u8string().copy(buf, sizeof(buf) - 1);
		if (ImGui::InputTextEx("##path", nullptr, buf, sizeof(buf), ImVec2(browse_button_width, 0), ImGuiInputTextFlags_None))
			_current_browse_path = std::filesystem::u8path(buf);

		if (ImGui::IsItemActivated())
			_imgui_context->InputTextState.ClearSelection();
		if (ImGui::IsWindowAppearing())
			ImGui::SetKeyboardFocusHere();

		// This sets the browse path the first time the popup is opened too
		if (_current_browse_path.empty() || !_current_browse_path.is_absolute())
			_current_browse_path = _current_preset_path.empty() ?
				_configuration_path.parent_path() : _current_preset_path.parent_path();

		if (ImGui::IsKeyPressedMap(ImGuiKey_Enter))
		{
			const std::filesystem::file_type file_type = std::filesystem::status(_current_browse_path, ec).type();
			if (file_type != std::filesystem::file_type::directory &&
				_current_browse_path.extension() == L".ini" || _current_browse_path.extension() == L".txt")
			{
				reload_preset =
					file_type == std::filesystem::file_type::not_found ||
					ini_file::load_cache(_current_browse_path).has("", "Techniques");
			}

			if (reload_preset)
			{
				ImGui::CloseCurrentPopup();
				_current_preset_path = _current_browse_path;
			}
			else
			{
				ImGui::SetKeyboardFocusHere();
			}
		}

		std::filesystem::path base_path = _current_browse_path;
		std::wstring presets_filter_text;
		if (!std::filesystem::is_directory(base_path, ec))
		{
			presets_filter_text = base_path.filename().wstring();
			base_path = base_path.parent_path();
		}

		ImGui::BeginChild("##paths", ImVec2(browse_button_width, 300), true, ImGuiWindowFlags_NavFlattened);

		if (_current_browse_path.has_parent_path() && ImGui::Selectable(".."))
			_current_browse_path = _current_browse_path.parent_path();

		std::vector<std::filesystem::path> preset_paths;
		for (const auto &entry : std::filesystem::directory_iterator(base_path, std::filesystem::directory_options::skip_permission_denied, ec))
		{
			if (entry.is_directory(ec))
			{
				std::string label = entry.path().filename().u8string();
				label = "<DIR> " + label;

				if (ImGui::Selectable(label.c_str()))
					_current_browse_path = entry.path();
				continue;
			}

			if (entry.path().extension() == L".ini" || entry.path().extension() == L".txt")
				preset_paths.push_back(entry);
		}
		for (const std::filesystem::path &preset_path : preset_paths)
		{
			const bool is_current_preset = std::filesystem::equivalent(preset_path, _current_preset_path, ec);

			if (!presets_filter_text.empty() && !is_current_preset)
			{
				const std::wstring preset_name = preset_path.filename().wstring();
				if (std::search(preset_name.begin(), preset_name.end(), presets_filter_text.begin(), presets_filter_text.end(),
					[](const wchar_t c1, const wchar_t c2) { return towlower(c1) == towlower(c2); }) == preset_name.end())
					continue;
			}

			if (ImGui::Selectable(preset_path.filename().u8string().c_str(), is_current_preset) &&
				ini_file::load_cache(preset_path).has("", "Techniques"))
			{
				reload_preset = true;
				_current_preset_path = preset_path;
				ImGui::CloseCurrentPopup();
			}

			if (is_current_preset && ImGui::IsWindowAppearing())
				ImGui::SetScrollHereY();
		}

		ImGui::EndChild();
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("##name"))
	{
		ImGui::Checkbox("Duplicate current preset", &_duplicate_current_preset);

		char preset_name[260] = "";
		if (ImGui::InputText("Name", preset_name, sizeof(preset_name), ImGuiInputTextFlags_EnterReturnsTrue) && preset_name[0] != '\0')
		{
			std::filesystem::path new_preset_path = _current_browse_path / std::filesystem::u8path(preset_name);
			if (new_preset_path.extension() != L".ini" && new_preset_path.extension() != L".txt")
				new_preset_path += L".ini";

			const std::filesystem::file_type file_type = std::filesystem::status(new_preset_path, ec).type();
			if (file_type != std::filesystem::file_type::directory)
			{
				reload_preset =
					file_type == std::filesystem::file_type::not_found ||
					ini_file::load_cache(new_preset_path).has("", "Techniques");

				if (_duplicate_current_preset && file_type == std::filesystem::file_type::not_found)
					CopyFileW(_current_preset_path.c_str(), new_preset_path.c_str(), TRUE);
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
}

void reshade::runtime::draw_variable_editor()
{
	const ImVec2 popup_pos = ImGui::GetCursorScreenPos() + ImVec2(std::max(0.f, ImGui::GetWindowContentRegionWidth() * 0.5f - 200.0f), ImGui::GetFrameHeightWithSpacing());

	if (imgui_popup_button("Edit global preprocessor definitions", ImGui::GetContentRegionAvail().x, ImGuiWindowFlags_NoMove))
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
					modified |= ImGui::InputText("##value", value, sizeof(value), ImGuiInputTextFlags_AutoSelectAll);
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
		{
			save_config();
			save_current_preset();
			_was_preprocessor_popup_edited = true;
		}

		ImGui::EndPopup();
	}
	else if (_was_preprocessor_popup_edited)
	{
		load_effects();
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
	ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.5f);

	for (size_t effect_index = 0, id = 0; effect_index < _effects.size(); ++effect_index)
	{
		// Hide variables that are not currently used in any of the active effects
		if (!_effects[effect_index].rendering ||
			// Skip showing this effect in the variable list if it doesn't have any uniform variables to show
			(_effects[effect_index].uniforms.empty() && _effects[effect_index].definitions.empty()))
			continue;
		assert(_effects[effect_index].compile_sucess);

		bool reload_effect = false;
		const bool is_focused = _focused_effect == effect_index;
		const std::string source_file = _effects[effect_index].source_file.filename().u8string();

		// Create separate tab for every effect file
		if (_variable_editor_tabs)
		{
			if (!ImGui::BeginTabItem(source_file.c_str()))
				continue;
			// Begin a new child here so scrolling through variables does not move the tab itself too
			ImGui::BeginChild("##tab");
		}
		else
		{
			if (is_focused || _effects_expanded_state & 1)
				ImGui::SetNextItemOpen(is_focused || (_effects_expanded_state >> 1) != 0);

			if (!ImGui::TreeNodeEx(source_file.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				continue; // Skip rendering invisible items
		}

		if (is_focused)
		{
			ImGui::SetScrollHereY(0.0f);
			_focused_effect = std::numeric_limits<size_t>::max();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(_imgui_context->Style.FramePadding.x, 0));
		if (imgui_popup_button("Reset all to default", _variable_editor_tabs ? ImGui::GetContentRegionAvail().x : ImGui::CalcItemWidth()))
		{
			ImGui::Text("Do you really want to reset all values in '%s' to their defaults?", source_file.c_str());

			if (ImGui::Button("Yes", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				// Reset all uniform variables
				for (uniform &variable : _effects[effect_index].uniforms)
					reset_uniform_value(variable);

				// Reset all preprocessor definitions
				for (const std::pair<std::string, std::string> &definition : _effects[effect_index].definitions)
					if (const auto preset_it = find_definition_value(_preset_preprocessor_definitions, definition.first);
						preset_it != _preset_preprocessor_definitions.end())
						reload_effect = true, // Need to reload after changing preprocessor defines so to get accurate defaults again
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

		for (uniform &variable : _effects[effect_index].uniforms)
		{
			// Skip hidden and special variables
			if (variable.annotation_as_int("hidden") || variable.special != special_uniform::none)
				continue;

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
				get_uniform_value(variable, &data, 1);

				if (ui_type == "combo")
					modified = imgui_combo_with_buttons(label.data(), data);
				else
					modified = ImGui::Checkbox(label.data(), &data);

				if (modified)
					set_uniform_value(variable, &data, 1);
				break;
			}
			case reshadefx::type::t_int:
			case reshadefx::type::t_uint:
			{
				int data[16];
				get_uniform_value(variable, data, 16);

				const auto ui_min_val = variable.annotation_as_int("ui_min");
				const auto ui_max_val = variable.annotation_as_int("ui_max");
				const auto ui_stp_val = std::max(1, variable.annotation_as_int("ui_step"));

				if (ui_type == "slider")
					modified = imgui_slider_with_buttons(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val);
				else if (ui_type == "drag")
					modified = variable.annotation_as_int("ui_step") == 0 ?
						ImGui::DragScalarN(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, 1.0f, &ui_min_val, &ui_max_val) :
						imgui_drag_with_buttons(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val);
				else if (ui_type == "list")
					modified = imgui_list_with_buttons(label.data(), variable.annotation_as_string("ui_items"), data[0]);
				else if (ui_type == "combo")
					modified = imgui_combo_with_buttons(label.data(), variable.annotation_as_string("ui_items"), data[0]);
				else if (ui_type == "radio")
					modified = imgui_radio_list(label.data(), variable.annotation_as_string("ui_items"), data[0]);
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

				const auto ui_min_val = variable.annotation_as_float("ui_min");
				const auto ui_max_val = variable.annotation_as_float("ui_max");
				const auto ui_stp_val = std::max(0.001f, variable.annotation_as_float("ui_step"));

				// Calculate display precision based on step value
				char precision_format[] = "%.0f";
				for (float x = 1.0f; x * ui_stp_val < 1.0f && precision_format[2] < '9'; x *= 10.0f)
					++precision_format[2]; // This changes the text to "%.1f", "%.2f", "%.3f", ...

				if (ui_type == "slider")
					modified = imgui_slider_with_buttons(label.data(), ImGuiDataType_Float, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, precision_format);
				else if (ui_type == "drag")
					modified = variable.annotation_as_float("ui_step") == 0 ?
						ImGui::DragScalarN(label.data(), ImGuiDataType_Float, data, variable.type.rows, ui_stp_val, &ui_min_val, &ui_max_val, precision_format) :
						imgui_drag_with_buttons(label.data(), ImGuiDataType_Float, data, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, precision_format);
				else if (ui_type == "color" && variable.type.rows == 1)
					modified = imgui_slider_for_alpha(label.data(), data);
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

			// Display tooltip
			if (const std::string_view tooltip = variable.annotation_as_string("ui_tooltip");
				!tooltip.empty() && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", tooltip.data());

			// Create context menu
			if (ImGui::BeginPopupContextItem("##context"))
			{
				if (variable.supports_toggle_key() &&
					imgui_key_input("##toggle_key", variable.toggle_key_data, *_input))
					modified = true;

				const float button_width = ImGui::CalcItemWidth();

				if (ImGui::Button("Reset to default", ImVec2(button_width, 0)))
				{
					modified = true;
					reset_uniform_value(variable);
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if (variable.toggle_key_data[0] != 0)
			{
				ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 120);
				ImGui::TextDisabled("%s", reshade::input::key_name(variable.toggle_key_data).c_str());
			}

			ImGui::PopID();

			// A value has changed, so save the current preset
			if (modified)
				save_current_preset();
		}

		// Draw preprocessor definition list after all uniforms of an effect file
		std::string category_label = "Preprocessor definitions";
		if (!_variable_editor_tabs)
			for (float x = 0, space_x = ImGui::CalcTextSize(" ").x, width = (ImGui::CalcItemWidth() - ImGui::CalcTextSize(category_label.data()).x - 45) / 2; x < width; x += space_x)
				category_label.insert(0, " ");

		if (!_effects[effect_index].definitions.empty() &&
			ImGui::TreeNodeEx(category_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen))
		{
			for (const std::pair<std::string, std::string> &definition : _effects[effect_index].definitions)
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
							reload_effect = true;
							_preset_preprocessor_definitions.erase(preset_it);
						}
					}
					else
					{
						reload_effect = true;

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

				if (!reload_effect && // Cannot compare iterators if definitions were just modified above
					ImGui::BeginPopupContextItem())
				{
					const float button_width = ImGui::CalcItemWidth();

					if (ImGui::Button("Reset to default", ImVec2(button_width, 0)))
					{
						if (preset_it != _preset_preprocessor_definitions.end())
						{
							reload_effect = true;
							_preset_preprocessor_definitions.erase(preset_it);
						}

						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
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

		if (reload_effect)
		{
			save_current_preset();

			const bool reload_successful_before = _last_shader_reload_successful;

			// Reload current effect file
			_reload_total_effects = 1;
			_reload_remaining_effects = 1;
			unload_effect(effect_index);
			if (!load_effect(_effects[effect_index].source_file, effect_index) &&
				modified_definition != _preset_preprocessor_definitions.end())
			{
				// The preprocessor definition that was just modified caused the shader to not compile, so reset to default and try again
				_preset_preprocessor_definitions.erase(modified_definition);

				_reload_total_effects = 1;
				_reload_remaining_effects = 1;
				unload_effect(effect_index);
				if (load_effect(_effects[effect_index].source_file, effect_index))
				{
					_last_shader_reload_successful = reload_successful_before;
					ImGui::OpenPopup("##pperror"); // Notify the user about this

					// Update preset again now, so that the removed preprocessor definition does not reappear on a reload
					// The preset is actually loaded again next frame to update the technique status (see 'update_and_render_effects'), so cannot use 'save_current_preset' here
					ini_file::load_cache(_current_preset_path).set({}, "PreprocessorDefinitions", _preset_preprocessor_definitions);
				}

				// Re-open file in editor so that errors are updated
				if (_selected_effect == effect_index)
					open_file_in_code_editor(_selected_effect, _editor_file);
			}

			assert(_reload_remaining_effects == 0);

			// Reloading an effect file invalidates all textures, but the statistics window may already have drawn references to those, so need to reset it
			ImGui::FindWindowByName("Statistics")->DrawList->CmdBuffer.clear();
		}
	}

	if (ImGui::BeginPopup("##pperror"))
	{
		ImGui::TextColored(COLOR_RED, "The shader failed to compile after this change, so it was reverted back to the default.");
		ImGui::EndPopup();
	}

	ImGui::PopItemWidth();
	if (_variable_editor_tabs)
		ImGui::EndTabBar();
	ImGui::EndChild();
}
void reshade::runtime::draw_technique_editor()
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
		const effect &effect = _effects[technique.effect_index];

		// Draw border around the item if it is selected
		const bool draw_border = _selected_technique == index;
		if (draw_border)
			ImGui::Separator();

		const bool clicked = _imgui_context->IO.MouseClicked[0];
		const bool compile_success = effect.compile_sucess;
		assert(compile_success || !technique.enabled);

		// Prevent user from enabling the technique when the effect failed to compile
		// Also prevent disabling it for when the technique is set to always be enabled via annotation
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !compile_success || technique.annotation_as_int("enabled"));
		// Gray out disabled techniques and mark techniques which failed to compile red
		ImGui::PushStyleColor(ImGuiCol_Text, compile_success ? _imgui_context->Style.Colors[technique.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled] : COLOR_RED);

		std::string_view ui_label = technique.annotation_as_string("ui_label");
		if (ui_label.empty() || !compile_success) ui_label = technique.name;
		std::string label(ui_label.data(), ui_label.size());
		label += " [" + effect.source_file.filename().u8string() + ']' + (!compile_success ? " failed to compile" : "");

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

			if (ImGui::Button("Open folder in explorer", ImVec2(button_width, 0)))
			{
				// Use absolute path to explorer to avoid potential security issues when executable is replaced
				WCHAR explorer_path[260] = L"";
				GetWindowsDirectoryW(explorer_path, ARRAYSIZE(explorer_path));
				wcscat_s(explorer_path, L"\\explorer.exe");

				ShellExecuteW(nullptr, L"open", explorer_path, (L"/select,\"" + effect.source_file.wstring() + L"\"").c_str(), nullptr, SW_SHOWDEFAULT);
			}

			ImGui::Separator();

			if (imgui_popup_button("Edit source code", button_width))
			{
				std::filesystem::path source_file;
				if (ImGui::MenuItem(effect.source_file.filename().u8string().c_str()))
					source_file = effect.source_file;

				if (!effect.included_files.empty())
				{
					ImGui::Separator();

					for (const std::filesystem::path &included_file : effect.included_files)
					{
						if (ImGui::MenuItem(included_file.filename().u8string().c_str()))
						{
							source_file = included_file;
						}
					}
				}

				ImGui::EndPopup();

				if (!source_file.empty())
				{
					open_file_in_code_editor(technique.effect_index, source_file);
					ImGui::CloseCurrentPopup();
				}
			}

			if (!effect.module.hlsl.empty() && // Hide if using SPIR-V, since that cannot easily be shown here
				imgui_popup_button("Show compiled results", button_width))
			{
				std::string source_code;
				if (ImGui::MenuItem("Generated code"))
				{
					source_code = effect.preamble + effect.module.hlsl;
					_viewer_entry_point.clear();
				}

				if (!effect.assembly.empty())
				{
					ImGui::Separator();

					for (const reshadefx::entry_point &entry_point : effect.module.entry_points)
					{
						if (const auto assembly_it = effect.assembly.find(entry_point.name);
							assembly_it != effect.assembly.end() && ImGui::MenuItem(entry_point.name.c_str()))
						{
							source_code = assembly_it->second;
							_viewer_entry_point = entry_point.name;
						}
					}
				}

				ImGui::EndPopup();

				if (!source_code.empty())
				{
					_show_code_viewer = true;
					_viewer.set_text(source_code);
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		if (technique.toggle_key_data[0] != 0 && compile_success)
		{
			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - 120);
			ImGui::TextDisabled("%s", reshade::input::key_name(technique.toggle_key_data).c_str());
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

void reshade::runtime::open_file_in_code_editor(size_t effect_index, const std::filesystem::path &path)
{
	_selected_effect = effect_index;

	if (effect_index >= _effects.size())
	{
		_editor.clear_text();
		_editor.set_readonly(true);
		_editor_file.clear();
		return;
	}

	// Force code editor to become visible
	_show_code_editor = true;

	// Only reload text if another file is opened (to keep undo history intact)
	if (path != _editor_file)
	{
		_editor_file = path;

		// Load file to string and update editor text
		_editor.set_text(std::string(std::istreambuf_iterator<char>(std::ifstream(path).rdbuf()), std::istreambuf_iterator<char>()));
		_editor.set_readonly(false);
	}

	// Update generated code in viewer after a reload
	if (_show_code_viewer && _viewer_entry_point.empty())
	{
		_viewer.set_text(_effects[effect_index].preamble + _effects[effect_index].module.hlsl);
	}

	_editor.clear_errors();
	const std::string &errors = _effects[effect_index].errors;

	for (size_t offset = 0, next; offset != std::string::npos; offset = next)
	{
		const size_t pos_error = errors.find(": ", offset);
		const size_t pos_error_line = errors.rfind('(', pos_error); // Paths can contain '(', but no ": ", so search backwards from th error location to find the line info
		if (pos_error == std::string::npos || pos_error_line == std::string::npos)
			break;

		const size_t pos_linefeed = errors.find('\n', pos_error);

		next = pos_linefeed != std::string::npos ? pos_linefeed + 1 : std::string::npos;

		// Ignore errors that aren't in the current source file
		if (const std::string_view error_file(errors.c_str() + offset, pos_error_line - offset);
			error_file != path.u8string())
			continue;

		const int error_line = std::strtol(errors.c_str() + pos_error_line + 1, nullptr, 10);
		const std::string error_text = errors.substr(pos_error + 2 /* skip space */, pos_linefeed - pos_error - 2);

		_editor.add_error(error_line, error_text, error_text.find("warning") != std::string::npos);
	}
}

#endif
