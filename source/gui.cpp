/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_GUI

#include "log.hpp"
#include "version.h"
#include "runtime.hpp"
#include "input.hpp"
#include "gui_widgets.hpp"
#include <assert.h>
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>

extern volatile long g_network_traffic;
extern std::filesystem::path g_reshade_dll_path;
extern std::filesystem::path g_target_executable_path;
extern std::filesystem::path g_system_path;
extern std::filesystem::path g_windows_path;

static const char keyboard_keys[256][16] = {
	"", "", "", "Cancel", "", "", "", "", "Backspace", "Tab", "", "", "Clear", "Enter", "", "",
	"Shift", "Control", "Alt", "Pause", "Caps Lock", "", "", "", "", "", "", "Escape", "", "", "", "",
	"Space", "Page Up", "Page Down", "End", "Home", "Left Arrow", "Up Arrow", "Right Arrow", "Down Arrow", "Select", "", "", "Print Screen", "Insert", "Delete", "Help",
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "", "", "", "", "", "",
	"", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
	"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Left Windows", "Right Windows", "", "", "Sleep",
	"Numpad 0", "Numpad 1", "Numpad 2", "Numpad 3", "Numpad 4", "Numpad 5", "Numpad 6", "Numpad 7", "Numpad 8", "Numpad 9", "Numpad *", "Numpad +", "", "Numpad -", "Numpad Decimal", "Numpad /",
	"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16",
	"F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "", "", "", "", "", "", "", "",
	"Num Lock", "Scroll Lock",
};

static inline std::vector<std::string> split(const std::string &str, char delim)
{
	std::vector<std::string> result;

	for (size_t i = 0, len = str.size(), found; i < len; i = found + 1)
	{
		found = str.find_first_of(delim, i);

		if (found == std::string::npos)
			found = len;

		result.push_back(str.substr(i, found - i));
	}

	return result;
}

const ImVec4 COLOR_RED = ImColor(240, 100, 100);
const ImVec4 COLOR_YELLOW = ImColor(222, 222, 111);

void reshade::runtime::init_ui()
{
	// Default shortcut Ctrl + Home
	_menu_key_data[0] = 0x24;
	_menu_key_data[1] = true;
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
	imgui_io.ConfigFlags = ImGuiConfigFlags_DockingEnable;
	imgui_io.ConfigResizeWindowsFromEdges = true;
	imgui_style.WindowRounding = 0.0f;
	imgui_style.WindowBorderSize = 0.0f;
	imgui_style.ChildRounding = 0.0f;
	imgui_style.FrameRounding = 0.0f;
	imgui_style.ScrollbarRounding = 0.0f;
	imgui_style.GrabRounding = 0.0f;

	ImGui::SetCurrentContext(nullptr);

	imgui_io.Fonts->AddFontDefault();
	const auto font_path = g_windows_path / "Fonts" / "consolab.ttf";
	if (std::error_code ec; std::filesystem::exists(font_path, ec))
		imgui_io.Fonts->AddFontFromFileTTF(font_path.u8string().c_str(), 18.0f);
	else
		imgui_io.Fonts->AddFontDefault();

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
		config.get("GENERAL", "FontGlobalScale", _imgui_context->IO.FontGlobalScale);
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
		config.get("STYLE", "FPSScale", _fps_scale);
		config.get("STYLE", "ColFPSText", _fps_col);
		config.get("STYLE", "StyleIndex", _style_index);
		config.get("STYLE", "EditorStyleIndex", _editor_style_index);

		_imgui_context->IO.IniFilename = save_imgui_window_state ? "ReShadeGUI.ini" : nullptr;

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
			colors[ImGuiCol_TitleBgActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.78f);
			colors[ImGuiCol_MenuBarBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 0.57f);
			colors[ImGuiCol_ScrollbarBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 1.00f);
			colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.31f);
			colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.78f);
			colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
			colors[ImGuiCol_PopupBg] = ImVec4(0.156863f, 0.156863f, 0.156863f, 0.92f);
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
			colors[ImGuiCol_Tab] = ImLerp(colors[ImGuiCol_Header], colors[ImGuiCol_TitleBgActive], 0.80f);
			colors[ImGuiCol_TabActive] = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
			colors[ImGuiCol_TabUnfocused] = ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
			colors[ImGuiCol_TabUnfocusedActive] = ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);
			colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
			colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_Header] * ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
			colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
			colors[ImGuiCol_PlotLines] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.63f);
			colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
			colors[ImGuiCol_PlotHistogram] = ImVec4(0.862745f, 0.862745f, 0.862745f, 0.63f);
			colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.392157f, 0.588235f, 0.941176f, 1.00f);
			colors[ImGuiCol_TextSelectedBg] = ImVec4(0.392157f, 0.588235f, 0.941176f, 0.43f);
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
		}
	});
	_save_config_callables.push_back([this](ini_file &config) {
		config.set("INPUT", "KeyMenu", _menu_key_data);
		config.set("INPUT", "InputProcessing", _input_processing_mode);

		config.set("GENERAL", "ShowClock", _show_clock);
		config.set("GENERAL", "ShowFPS", _show_fps);
		config.set("GENERAL", "ShowFrameTime", _show_frametime);
		config.set("GENERAL", "FontGlobalScale", _imgui_context->IO.FontGlobalScale);
		config.set("GENERAL", "NoReloadOnInit", _no_reload_on_init);
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
		config.set("STYLE", "FPSScale", _fps_scale);
		config.set("STYLE", "ColFPSTex", _fps_col);
		config.set("STYLE", "StyleIndex", _style_index);
		config.set("STYLE", "EditorStyleIndex", _editor_style_index);

		if (_style_index > 2)
			for (ImGuiCol i = 0; i < ImGuiCol_COUNT; i++)
				config.set("STYLE", ImGui::GetStyleColorName(i), (const float(&)[4])_imgui_context->Style.Colors[i]);
	});
}
void reshade::runtime::init_ui_font_atlas()
{
	_show_splash = true;

	ImGui::SetCurrentContext(_imgui_context);

	int width, height;
	unsigned char *pixels;
	ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	_imgui_font_atlas.width = width;
	_imgui_font_atlas.height = height;
	_imgui_font_atlas.format = reshadefx::texture_format::rgba8;
	_imgui_font_atlas.unique_name = "ImGUI Font Atlas";
	init_texture(_imgui_font_atlas);
	update_texture(_imgui_font_atlas, pixels);

	ImGui::SetCurrentContext(nullptr);
}
void reshade::runtime::deinit_ui()
{
	ImGui::DestroyContext(_imgui_context);
}
void reshade::runtime::deinit_ui_font_atlas()
{
	_imgui_font_atlas.impl.reset();
}

void reshade::runtime::draw_ui()
{
	const bool show_splash = _show_splash && (_reload_remaining_effects != std::numeric_limits<size_t>::max() || !_reload_compile_queue.empty() || (_last_present_time - _last_reload_time) < std::chrono::seconds(5));

	if (!_overlay_key_setting_active && _input->is_key_pressed(_menu_key_data))
		_show_menu = !_show_menu;

	_effects_expanded_state &= 2;

	// Update ImGui configuration
	ImGui::SetCurrentContext(_imgui_context);
	auto &imgui_io = _imgui_context->IO;
	imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
	imgui_io.MouseDrawCursor = _show_menu;
	imgui_io.DisplaySize.x = static_cast<float>(_width);
	imgui_io.DisplaySize.y = static_cast<float>(_height);
	imgui_io.Fonts->TexID = _imgui_font_atlas.impl.get();

	imgui_io.KeyCtrl = _input->is_key_down(0x11); // VK_CONTROL
	imgui_io.KeyShift = _input->is_key_down(0x10); // VK_SHIFT
	imgui_io.KeyAlt = _input->is_key_down(0x12); // VK_MENU
	imgui_io.MousePos.x = static_cast<float>(_input->mouse_position_x());
	imgui_io.MousePos.y = static_cast<float>(_input->mouse_position_y());
	imgui_io.MouseWheel += _input->mouse_wheel_delta();

	for (unsigned int i = 0; i < 256; i++)
		imgui_io.KeysDown[i] = _input->is_key_down(i);
	for (unsigned int i = 0; i < 5; i++)
		imgui_io.MouseDown[i] = _input->is_mouse_button_down(i);
	for (wchar_t c : _input->text_input())
		imgui_io.AddInputCharacter(c);

	if (imgui_io.KeyCtrl && !_no_font_scaling)
		// Change global font scale if user presses the control key and moves the mouse wheel
		imgui_io.FontGlobalScale = ImClamp(imgui_io.FontGlobalScale + imgui_io.MouseWheel * 0.10f, 0.2f, 2.50f);

	imgui_io.NavInputs[ImGuiNavInput_Activate] = ImGui::IsKeyDown(ImGuiKey_Space);
	imgui_io.NavInputs[ImGuiNavInput_Input] = ImGui::IsKeyDown(ImGuiKey_Enter);
	imgui_io.NavInputs[ImGuiNavInput_Cancel] = ImGui::IsKeyDown(ImGuiKey_Escape);
	imgui_io.NavInputs[ImGuiNavInput_KeyLeft_] = ImGui::IsKeyDown(ImGuiKey_LeftArrow);
	imgui_io.NavInputs[ImGuiNavInput_KeyRight_] = ImGui::IsKeyDown(ImGuiKey_RightArrow);
	imgui_io.NavInputs[ImGuiNavInput_KeyUp_] = ImGui::IsKeyDown(ImGuiKey_UpArrow);
	imgui_io.NavInputs[ImGuiNavInput_KeyDown_] = ImGui::IsKeyDown(ImGuiKey_DownArrow);
	imgui_io.NavInputs[ImGuiNavInput_TweakSlow] = imgui_io.KeyCtrl;
	imgui_io.NavInputs[ImGuiNavInput_TweakFast] = imgui_io.KeyShift;

	ImGui::NewFrame();

	ImVec2 viewport_offset = ImVec2(0, 0);

	// Create ImGui widgets and windows
	if (show_splash)
	{
		ImGui::SetNextWindowPos(ImVec2(10, 10));
		ImGui::SetNextWindowSize(ImVec2(_width - 20.0f, viewport_offset.y = ImGui::GetFrameHeightWithSpacing() * (_last_reload_successful ? 3.2f : 4.2f)));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.862745f, 0.862745f, 0.862745f, 1.00f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.117647f, 0.117647f, 0.117647f, 0.5f));
		ImGui::Begin("Splash Screen", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoFocusOnAppearing);

		viewport_offset.y += 10;

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

		if (_reload_remaining_effects != 0 && _reload_remaining_effects != std::numeric_limits<size_t>::max())
		{
			ImGui::ProgressBar(1.0f - _reload_remaining_effects / float(_reload_total_effects), ImVec2(-1, 0), "");
			ImGui::SameLine(15);
			ImGui::Text(
				"Loading (%zu effects remaining) ... "
				"This might take a while. The application could become unresponsive for some time.",
				_reload_remaining_effects.load());
		}
		else if (!_reload_compile_queue.empty())
		{
			ImGui::ProgressBar(1.0f - _reload_compile_queue.size() / float(_reload_total_effects), ImVec2(-1, 0), "");
			ImGui::SameLine(15);
			ImGui::Text(
				"Compiling (%zu effects remaining) ... "
				"This might take a while. The application could become unresponsive for some time.",
				_reload_compile_queue.size());
		}
		else
		{
			ImGui::Text(
				"Press '%s%s%s%s' to open the configuration menu.",
				_menu_key_data[1] ? "Ctrl + " : "",
				_menu_key_data[2] ? "Shift + " : "",
				_menu_key_data[3] ? "Alt + " : "",
				keyboard_keys[_menu_key_data[0]]);
		}

		if (!_last_reload_successful)
		{
			ImGui::Spacing();
			ImGui::TextColored(COLOR_RED,
				"There were errors compiling some shaders. "
				"Open the configuration menu and switch to the 'Statistics' tab for more details.");
		}

		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}
	else if (_show_clock || _show_fps || _show_frametime)
	{
		ImGui::SetNextWindowPos(ImVec2(_width - 200.0f, 5));
		ImGui::SetNextWindowSize(ImVec2(200.0f, 200.0f));
		ImGui::Begin("FPS", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBackground);

		ImGui::SetWindowFontScale(_fps_scale);

		ImGui::PushFont(_imgui_context->IO.Fonts->Fonts[1]);
		ImGui::PushStyleColor(ImGuiCol_Text, (const ImVec4 &)_fps_col);

		char temp[512];

		if (_show_clock)
		{
			const int hour = _date[3] / 3600;
			const int minute = (_date[3] - hour * 3600) / 60;
			const int seconds = _date[3] - hour * 3600 - minute * 60;

			ImFormatString(temp, sizeof(temp), " %02u:%02u:%02u", hour, minute, seconds);
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}
		if (_show_fps)
		{
			ImFormatString(temp, sizeof(temp), "%.0f fps", _imgui_context->IO.Framerate);
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}
		if (_show_frametime)
		{
			ImFormatString(temp, sizeof(temp), "%5.2f ms", 1000.0f / _imgui_context->IO.Framerate);
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}

		ImGui::PopStyleColor();
		ImGui::PopFont();

		ImGui::End();
	}

	const ImGuiID root_space_id = ImGui::GetID("Dockspace");
	const ImGuiViewport *const viewport = ImGui::GetMainViewport();

	// Set up default dock layout if this was not done yet
	if (ImGui::DockBuilderGetNode(root_space_id) == nullptr)
	{
		// Add root node
		ImGui::DockBuilderAddNode(root_space_id, viewport->Size);
		// Split root node into two spaces
		ImGuiID main_space_id = 0;
		ImGuiID right_space_id = 0;
		ImGui::DockBuilderSplitNode(root_space_id, ImGuiDir_Left, 0.35f, &main_space_id, &right_space_id);

		// Attach windows to the main dock space
		ImGui::DockBuilderDockWindow("Home", main_space_id);
		ImGui::DockBuilderDockWindow("Settings", main_space_id);
		ImGui::DockBuilderDockWindow("Statistics", main_space_id);
		ImGui::DockBuilderDockWindow("Log", main_space_id);
		ImGui::DockBuilderDockWindow("About", main_space_id);
		ImGui::DockBuilderDockWindow("###editor", right_space_id);
		ImGui::DockBuilderDockWindow("DX9", main_space_id);
		ImGui::DockBuilderDockWindow("DX10", main_space_id);
		ImGui::DockBuilderDockWindow("DX11", main_space_id);

		ImGui::DockBuilderGetNode(main_space_id)->SelectedTabID = ImHash("Home", 0); // Select 'Home' tab

		ImGui::DockBuilderFinish(root_space_id);
	}

	ImGui::SetNextWindowPos(viewport->Pos + viewport_offset);
	ImGui::SetNextWindowSize(viewport->Size - viewport_offset);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::Begin("Viewport", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground);
	ImGui::DockSpace(root_space_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruDockspace);
	ImGui::End();

	if (_show_menu && _reload_remaining_effects == std::numeric_limits<size_t>::max())
	{
		for (const auto &widget : _menu_callables)
		{
			if (ImGui::Begin(widget.first.c_str()))
				widget.second();
			ImGui::End();
		}

		if (_show_code_editor)
		{
			const std::string title = _selected_effect < _loaded_effects.size() ? "Editing " + _loaded_effects[_selected_effect].source_file.filename().u8string() + " ###editor" : "Viewing Code###editor";

			if (ImGui::Begin(title.c_str(), &_show_code_editor))
				draw_code_editor();
			ImGui::End();
		}
	}

	// Render ImGui widgets and windows
	ImGui::Render();

	_input->block_mouse_input(_input_processing_mode != 0 && _show_menu && (_imgui_context->IO.WantCaptureMouse || _input_processing_mode == 2));
	_input->block_keyboard_input(_input_processing_mode != 0 && _show_menu && (_imgui_context->IO.WantCaptureKeyboard || _input_processing_mode == 2));

	if (const auto draw_data = ImGui::GetDrawData(); draw_data != nullptr && draw_data->CmdListsCount != 0 && draw_data->TotalVtxCount != 0)
	{
		render_imgui_draw_data(draw_data);
	}
}

void reshade::runtime::draw_overlay_menu_home()
{
	if (!_effects_enabled)
		ImGui::Text("Effects are disabled. Press '%s%s%s%s' to enable them again.",
			_effects_key_data[1] ? "Ctrl + " : "",
			_effects_key_data[2] ? "Shift + " : "",
			_effects_key_data[3] ? "Alt + " : "",
			keyboard_keys[_effects_key_data[0]]);

	bool continue_tutorial = false;
	const char *tutorial_text =
		"Welcome! Since this is the first time you start ReShade, we'll go through a quick tutorial covering the most important features.\n\n"
		"Before we continue: If you have difficulties reading this text, press the 'Ctrl' key and adjust the text size with your mouse wheel. "
		"The window size is variable as well, just grab the bottom right corner and move it around.\n\n"
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
				"You can add a new one by clicking on the '+' button. Simply enter a new preset name or the full path to an existing file (*.ini) in the text box that opens.\n"
				"To delete the currently selected preset file, click on the '-' button.\n"
				"Make sure a valid file is selected here before starting to tweak any values later, or else your changes won't be saved!\n\n"
				"Add a new preset by clicking on the '+' button to continue the tutorial.";

			ImGui::PushStyleColor(ImGuiCol_FrameBg, COLOR_RED);
			ImGui::PushStyleColor(ImGuiCol_Button, COLOR_RED);
		}

		const float button_size = ImGui::GetFrameHeight();
		const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;

		ImGui::PushItemWidth((button_size + button_spacing) * -2.0f - 1.0f);

		if (ImGui::BeginCombo("##presets", _current_preset < _preset_files.size() ? _preset_files[_current_preset].u8string().c_str() : ""))
		{
			for (size_t i = 0; i < _preset_files.size(); ++i)
			{
				if (ImGui::Selectable(_preset_files[i].u8string().c_str(), _current_preset == i))
				{
					_current_preset = i;

					save_config();

					_show_splash = true;
					_effect_filter_buffer[0] = '\0'; // Reset filter

					// Need to reload effects in performance mode, so values are applied
					if (_performance_mode)
						load_effects();
					else
						load_preset(_preset_files[_current_preset]);
				}
			}

			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();

		ImGui::SameLine(0, button_spacing);

		if (ImGui::Button("+", ImVec2(button_size, 0)))
			ImGui::OpenPopup("Add Preset");

		if (ImGui::BeginPopup("Add Preset"))
		{
			char buf[260] = "";
			if (ImGui::InputText("Name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				std::error_code ec;
				auto path = std::filesystem::absolute(g_reshade_dll_path.parent_path() / buf, ec);
				path.replace_extension(".ini");

				if (!ec && (std::filesystem::exists(path, ec) || std::filesystem::exists(path.parent_path(), ec)))
				{
					_preset_files.push_back(path);

					_current_preset = _preset_files.size() - 1;

					save_config();
					load_current_preset(); // Load the new preset

					ImGui::CloseCurrentPopup();

					if (_tutorial_index == 1)
						continue_tutorial = true;
				}
			}

			ImGui::EndPopup();
		}

		// Only show the remove preset button if a preset is selected
		if (_current_preset < _preset_files.size())
		{
			ImGui::SameLine(0, button_spacing);

			if (ImGui::Button("-", ImVec2(button_size, 0)))
				ImGui::OpenPopup("Remove Preset");

			if (ImGui::BeginPopup("Remove Preset"))
			{
				ImGui::Text("Do you really want to remove this preset?");

				if (ImGui::Button("Yes", ImVec2(-1, 0)))
				{
					_preset_files.erase(_preset_files.begin() + _current_preset);

					// If the removed preset is the last in the list, try to selected the one before that next
					if (_current_preset == _preset_files.size())
						_current_preset--;

					save_config();
					load_current_preset(); // Load the now selected preset

					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
		}

		if (_tutorial_index == 1)
			ImGui::PopStyleColor(2);
	}

	if (_tutorial_index > 1)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PushItemWidth(_variable_editor_tabs ? -130.0f : -260.0f);

		if (ImGui::InputText("##filter", _effect_filter_buffer, sizeof(_effect_filter_buffer), ImGuiInputTextFlags_AutoSelectAll))
		{
			_effects_expanded_state = 3;

			if (_effect_filter_buffer[0] == '\0')
			{
				// Reset visibility state
				for (uniform &variable : _uniforms)
					variable.hidden = variable.annotations["hidden"].second.as_uint[0];
				for (technique &technique : _techniques)
					technique.hidden = technique.annotations["hidden"].second.as_uint[0];
			}
			else
			{
				const std::string filter = _effect_filter_buffer;

				for (uniform &variable : _uniforms)
					variable.hidden = variable.annotations["hidden"].second.as_uint[0] ||
						std::search(variable.name.begin(), variable.name.end(), filter.begin(), filter.end(),
							[](auto c1, auto c2) { return tolower(c1) == tolower(c2); }) == variable.name.end() && variable.effect_filename.find(filter) == std::string::npos;
				for (technique &technique : _techniques)
					technique.hidden = technique.annotations["hidden"].second.as_uint[0] ||
						std::search(technique.name.begin(), technique.name.end(), filter.begin(), filter.end(),
							[](auto c1, auto c2) { return tolower(c1) == tolower(c2); }) == technique.name.end() && technique.effect_filename.find(filter) == std::string::npos;
			}
		}
		else if (!ImGui::IsItemActive() && _effect_filter_buffer[0] == '\0')
		{
			strcpy(_effect_filter_buffer, "Search");
		}

		ImGui::PopItemWidth();

		ImGui::SameLine();

		if (ImGui::Button("Active to top", ImVec2(130 - _imgui_context->Style.ItemSpacing.x, 0)))
		{
			for (size_t i = 0; i < _techniques.size(); ++i)
			{
				if (!_techniques[i].enabled)
				{
					for (size_t k = i + 1; k < _techniques.size(); ++k)
					{
						if (_techniques[k].enabled)
						{
							std::swap(_techniques[i], _techniques[k]);
							break;
						}
					}
				}
			}
		}

		ImGui::SameLine();

		if (ImGui::Button(_effects_expanded_state & 2 ? "Collapse all" : "Expand all", ImVec2(130 - _imgui_context->Style.ItemSpacing.x, 0)))
			_effects_expanded_state = (~_effects_expanded_state & 2) | 1;

		if (_tutorial_index == 2)
		{
			tutorial_text =
				"This is the list of techniques. It contains all techniques in the effect files (*.fx) that were found in the effect search paths as specified on the 'Settings' tab.\n\n"
				"Enter text in the box at the top to filter it and search for specific techniques.\n\n"
				"Click on a technique to enable or disable it or drag it to a new location in the list to change the order in which the effects are applied.";

			ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_RED);
		}

		ImGui::Spacing();

		const float bottom_height = _performance_mode ? ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y : _variable_editor_height;

		if (ImGui::BeginChild("##techniques", ImVec2(-1, -bottom_height), true))
			draw_overlay_technique_editor();
		ImGui::EndChild();

		if (_tutorial_index == 2)
			ImGui::PopStyleColor();
	}

	if (_tutorial_index > 2 && !_performance_mode)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::ButtonEx("##splitter", ImVec2(-1, 5));
		ImGui::PopStyleVar();

		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		if (ImGui::IsItemActive())
			_variable_editor_height -= _imgui_context->IO.MouseDelta.y;

		if (_tutorial_index == 3)
		{
			tutorial_text =
				"This is the list of variables. It contains all tweakable options the effects expose. All values here apply in real-time. Double click on a widget to manually edit the value.\n\n"
				"Enter text in the box at the top to filter it and search for specific variables.\n\n"
				"Once you have finished tweaking your preset, be sure to enable the 'Performance Mode' check box. "
				"This will recompile all shaders into a more optimal representation that gives a significant performance boost, but will disable variable tweaking and this list.";

			ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_RED);
		}

		const float bottom_height = ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y + (_tutorial_index == 3 ? 125 : 0);

		if (ImGui::BeginChild("##variables", ImVec2(-1, -bottom_height), true))
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
		ImGui::BeginChildFrame(ImGui::GetID("tutorial"), ImVec2(-1, 125));
		ImGui::TextWrapped(tutorial_text);
		ImGui::EndChildFrame();

		if ((_tutorial_index != 1 || _current_preset < _preset_files.size()) &&
			ImGui::Button(_tutorial_index == 3 ? "Finish" : "Continue", ImVec2(-1, 0)) || continue_tutorial)
		{
			_tutorial_index++;

			save_config();
		}
	}
}
void reshade::runtime::draw_overlay_menu_settings()
{
	char edit_buffer[2048];

	bool modified = false;
	bool reload_style = false;
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;

	const auto update_key_data = [&](unsigned int key_data[4]) {
		const unsigned int last_key_pressed = _input->last_key_pressed();

		if (last_key_pressed != 0)
		{
			if (last_key_pressed == 0x08) // Backspace
			{
				key_data[0] = 0;
				key_data[1] = 0;
				key_data[2] = 0;
				key_data[3] = 0;
			}
			else if (last_key_pressed < 0x10 || last_key_pressed > 0x12)
			{
				key_data[0] = last_key_pressed;
				key_data[1] = _input->is_key_down(0x11);
				key_data[2] = _input->is_key_down(0x10);
				key_data[3] = _input->is_key_down(0x12);
			}

			modified = true;
		}
	};

	const auto copy_key_shortcut_to_edit_buffer = [&edit_buffer](const unsigned int shortcut[4]) {
		assert(shortcut[0] < 256);
		size_t offset = 0;
		if (shortcut[1]) memcpy(edit_buffer, "Ctrl + ", 8), offset += 7;
		if (shortcut[2]) memcpy(edit_buffer + offset, "Shift + ", 9), offset += 8;
		if (shortcut[3]) memcpy(edit_buffer + offset, "Alt + ", 7), offset += 6;
		memcpy(edit_buffer + offset, keyboard_keys[shortcut[0]], sizeof(*keyboard_keys));
	};

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		copy_key_shortcut_to_edit_buffer(_menu_key_data);
		ImGui::InputText("Overlay Key", edit_buffer, sizeof(edit_buffer), ImGuiInputTextFlags_ReadOnly);

		_overlay_key_setting_active = false;

		if (ImGui::IsItemActive())
		{
			_overlay_key_setting_active = true;

			update_key_data(_menu_key_data);
		}
		else if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
		}

		copy_key_shortcut_to_edit_buffer(_reload_key_data);
		ImGui::InputText("Effect Reload Key", edit_buffer, sizeof(edit_buffer), ImGuiInputTextFlags_ReadOnly);

		if (ImGui::IsItemActive())
		{
			update_key_data(_reload_key_data);
		}
		else if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
		}

		copy_key_shortcut_to_edit_buffer(_effects_key_data);
		ImGui::InputText("Effect Toggle Key", edit_buffer, sizeof(edit_buffer), ImGuiInputTextFlags_ReadOnly);

		_toggle_key_setting_active = false;

		if (ImGui::IsItemActive())
		{
			_toggle_key_setting_active = true;

			update_key_data(_effects_key_data);
		}
		else if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
		}

		if (ImGui::Combo("Input Processing", &_input_processing_mode, "Pass on all input\0Block input when cursor is on overlay\0Block all input when overlay is visible\0"))
		{
			modified = true;
		}

		const float item_width = ImGui::CalcItemWidth();
		const float item_height = ImGui::GetFrameHeightWithSpacing();

		if (ImGui::BeginChild("##effect_search_paths", ImVec2(item_width, (_effect_search_paths.size() + 1) * item_height), false, ImGuiWindowFlags_NoScrollWithMouse))
		{
			for (size_t i = 0; i < _effect_search_paths.size(); ++i)
			{
				ImGui::PushID(static_cast<int>(i));
				ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - (button_spacing + button_size));

				memset(edit_buffer, 0, sizeof(edit_buffer));
				_effect_search_paths[i].u8string().copy(edit_buffer, sizeof(edit_buffer) - 1);
				if (ImGui::InputText("##path", edit_buffer, sizeof(edit_buffer)))
				{
					modified = true;
					_effect_search_paths[i] = edit_buffer;
				}

				ImGui::PopItemWidth();
				ImGui::SameLine(0, button_spacing);

				if (ImGui::Button("-", ImVec2(button_size, 0)))
				{
					modified = true;
					_effect_search_paths.erase(_effect_search_paths.begin() + i--);
				}

				ImGui::PopID();
			}

			ImGui::Dummy(ImVec2());
			ImGui::SameLine(0, ImGui::GetWindowContentRegionWidth() - button_size);
			if (ImGui::Button("+", ImVec2(button_size, 0)))
			{
				_file_selection_path = g_reshade_dll_path.parent_path();
				ImGui::OpenPopup("##select");
			}

			if (imgui_directory_dialog("##select", _file_selection_path))
			{
				modified = true;
				_effect_search_paths.push_back(_file_selection_path);
			}
		} ImGui::EndChild();
		ImGui::SameLine(0, button_spacing);
		ImGui::TextUnformatted("Effect Search Paths");

		if (ImGui::BeginChild("##texture_search_paths", ImVec2(item_width, (_texture_search_paths.size() + 1) * item_height), false, ImGuiWindowFlags_NoScrollWithMouse))
		{
			for (size_t i = 0; i < _texture_search_paths.size(); ++i)
			{
				ImGui::PushID(static_cast<int>(i));
				ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - (button_spacing + button_size));

				memset(edit_buffer, 0, sizeof(edit_buffer));
				_texture_search_paths[i].u8string().copy(edit_buffer, sizeof(edit_buffer) - 1);
				if (ImGui::InputText("##value", edit_buffer, sizeof(edit_buffer)))
				{
					modified = true;
					_texture_search_paths[i] = edit_buffer;
				}

				ImGui::PopItemWidth();
				ImGui::SameLine(0, button_spacing);

				if (ImGui::Button("-", ImVec2(button_size, 0)))
				{
					modified = true;
					_texture_search_paths.erase(_texture_search_paths.begin() + i--);
				}

				ImGui::PopID();
			}

			ImGui::Dummy(ImVec2());
			ImGui::SameLine(0, ImGui::GetWindowContentRegionWidth() - button_size);
			if (ImGui::Button("+", ImVec2(button_size, 0)))
			{
				_file_selection_path = g_reshade_dll_path.parent_path();
				ImGui::OpenPopup("##select");
			}

			if (imgui_directory_dialog("##select", _file_selection_path))
			{
				modified = true;
				_texture_search_paths.push_back(_file_selection_path);
			}
		} ImGui::EndChild();
		ImGui::SameLine(0, button_spacing);
		ImGui::TextUnformatted("Texture Search Paths");

		if (ImGui::BeginChild("##definitions", ImVec2(item_width, (_preprocessor_definitions.size() + 1) * item_height), false, ImGuiWindowFlags_NoScrollWithMouse))
		{
			for (size_t i = 0; i < _preprocessor_definitions.size(); ++i)
			{
				std::vector<std::string> definition = split(_preprocessor_definitions[i], '=');

				if (definition.size() < 2)
				{
					definition.emplace_back();
					definition.emplace_back();
				}

				ImGui::PushID(static_cast<int>(i));
				ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() * 0.66666666f - (button_spacing));

				memset(edit_buffer, 0, sizeof(edit_buffer));
				definition[0].copy(edit_buffer, sizeof(edit_buffer) - 1);
				if (ImGui::InputText("##name", edit_buffer, sizeof(edit_buffer)))
				{
					modified = true;
					definition[0] = edit_buffer;
				}

				ImGui::PopItemWidth();
				ImGui::SameLine(0, button_spacing);
				ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() * 0.3333333f - (button_spacing + button_size) + 1);

				memset(edit_buffer, 0, sizeof(edit_buffer));
				definition[1].copy(edit_buffer, sizeof(edit_buffer) - 1);
				if (ImGui::InputText("##value", edit_buffer, sizeof(edit_buffer)))
				{
					modified = true;
					definition[1] = edit_buffer;
				}

				ImGui::PopItemWidth();
				ImGui::SameLine(0, button_spacing);

				if (ImGui::Button("-", ImVec2(button_size, 0)))
				{
					modified = true;
					_preprocessor_definitions.erase(_preprocessor_definitions.begin() + i--);
				}
				else if (modified)
				{
					_preprocessor_definitions[i] = definition[0] + '=' + definition[1];
				}

				ImGui::PopID();
			}

			ImGui::Dummy(ImVec2());
			ImGui::SameLine(0, ImGui::GetWindowContentRegionWidth() - button_size);
			if (ImGui::Button("+", ImVec2(button_size, 0)))
				_preprocessor_definitions.emplace_back();
		} ImGui::EndChild();
		ImGui::SameLine(0, button_spacing);
		ImGui::TextUnformatted("Preprocessor Definitions");

		if (ImGui::Button("Restart Tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
		{
			_tutorial_index = 0;
		}
	}

	if (ImGui::CollapsingHeader("Screenshots", ImGuiTreeNodeFlags_DefaultOpen))
	{
		assert(_screenshot_key_data[0] < 256);

		copy_key_shortcut_to_edit_buffer(_screenshot_key_data);

		ImGui::InputText("Screenshot Key", edit_buffer, sizeof(edit_buffer), ImGuiInputTextFlags_ReadOnly);

		_screenshot_key_setting_active = false;

		if (ImGui::IsItemActive())
		{
			_screenshot_key_setting_active = true;

			update_key_data(_screenshot_key_data);
		}
		else if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
		}

		memset(edit_buffer, 0, sizeof(edit_buffer));
		_screenshot_path.u8string().copy(edit_buffer, sizeof(edit_buffer) - 1);

		ImGui::PushItemWidth(ImGui::CalcItemWidth() - (button_spacing + button_size));
		if (ImGui::InputText("##path", edit_buffer, sizeof(edit_buffer)))
		{
			modified = true;
			_screenshot_path = edit_buffer;
		}
		ImGui::PopItemWidth();
		ImGui::SameLine(0, button_spacing);
		if (ImGui::Button("..", ImVec2(button_size, 0)))
		{
			_file_selection_path = _screenshot_path;
			ImGui::OpenPopup("##select");
		}
		ImGui::SameLine(0, button_spacing);
		ImGui::TextUnformatted("Screenshot Path");

		if (imgui_directory_dialog("##select", _file_selection_path))
		{
			modified = true;
			_screenshot_path = _file_selection_path;
		}

		if (ImGui::Combo("Screenshot Format", &_screenshot_format, "Bitmap (*.bmp)\0Portable Network Graphics (*.png)\0"))
		{
			modified = true;
		}

		if (ImGui::Checkbox("Include Preset (*.ini)", &_screenshot_include_preset))
		{
			modified = true;
			_screenshot_include_configuration = false;
		}

		if (_screenshot_include_preset)
		{
			ImGui::SameLine();

			if (ImGui::Checkbox("Include Configuration (*.ini)", &_screenshot_include_configuration))
			{
				modified = true;
			}
		}
	}

	if (ImGui::CollapsingHeader("User Interface", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool save_imgui_window_state = _imgui_context->IO.IniFilename != nullptr;
		if (ImGui::Checkbox("Save Window State (ReShadeGUI.ini)", &save_imgui_window_state))
		{
			modified = true;
			_imgui_context->IO.IniFilename = save_imgui_window_state ? "ReShadeGUI.ini" : nullptr;
		}

		modified |= ImGui::Checkbox("Experimental Variable Editing UI", &_variable_editor_tabs);

		modified |= ImGui::Checkbox("Show Clock", &_show_clock);
		ImGui::SameLine(0, 10);
		modified |= ImGui::Checkbox("Show FPS", &_show_fps);
		ImGui::SameLine(0, 10);
		modified |= ImGui::Checkbox("Show Frame Time", &_show_frametime);

		if (ImGui::Combo("Style", &_style_index, "Dark\0Light\0Default\0Custom Simple\0Custom Advanced\0"))
		{
			modified = true;
			reload_style = true;
		}

		if (_style_index == 3) // Custom Simple
		{
			ImVec4 *const colors = _imgui_context->Style.Colors;

			modified |= ImGui::ColorEdit3("Background Color", &colors[ImGuiCol_WindowBg].x);
			modified |= ImGui::ColorEdit3("Item Background Color", &colors[ImGuiCol_FrameBg].x);
			modified |= ImGui::ColorEdit3("Text Color", &colors[ImGuiCol_Text].x);
			modified |= ImGui::ColorEdit3("Active Item Color", &colors[ImGuiCol_ButtonActive].x);

			if (modified)
			{
				colors[ImGuiCol_ChildBg] = colors[ImGuiCol_FrameBg]; colors[ImGuiCol_ChildBg].w = 0.00f;
				colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_FrameBg]; colors[ImGuiCol_MenuBarBg].w = 0.57f;
				colors[ImGuiCol_ScrollbarBg] = colors[ImGuiCol_FrameBg]; colors[ImGuiCol_ScrollbarBg].w = 1.00f;
				colors[ImGuiCol_PopupBg] = colors[ImGuiCol_FrameBg]; colors[ImGuiCol_PopupBg].w = 0.92f;

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
				colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_ButtonActive]; colors[ImGuiCol_TitleBgActive].w = 0.78f;
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
			}
		}
		if (_style_index == 4) // Custom Advanced
		{
			if (ImGui::BeginChild("##colors", ImVec2(0, 300), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened))
			{
				ImGui::PushItemWidth(-160);
				for (ImGuiCol i = 0; i < ImGuiCol_COUNT; i++)
				{
					ImGui::PushID(i);
					if (ImGui::ColorEdit4("##color", &_imgui_context->Style.Colors[i].x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview))
						modified = true;
					ImGui::SameLine();
					ImGui::TextUnformatted(ImGui::GetStyleColorName(i));
					ImGui::PopID();
				}
				ImGui::PopItemWidth();
			} ImGui::EndChild();
		}

		if (ImGui::Combo("Editor Style", &_editor_style_index, "Dark\0Light\0"))
		{
			modified = true;
			reload_style = true;
		}

		if (ImGui::SliderFloat("Global Alpha", &_imgui_context->Style.Alpha, 0.1f, 1.0f, "%.2f"))
		{
			_imgui_context->Style.Alpha = std::max(_imgui_context->Style.Alpha, 0.1f);
			modified = true;
		}

		if (ImGui::SliderFloat("Frame Rounding", &_imgui_context->Style.FrameRounding, 0.0f, 12.0f, "%.0f"))
		{
			_imgui_context->Style.GrabRounding = _imgui_context->Style.FrameRounding;
			_imgui_context->Style.ChildRounding = _imgui_context->Style.FrameRounding;
			_imgui_context->Style.PopupRounding = _imgui_context->Style.FrameRounding;
			_imgui_context->Style.WindowRounding = _imgui_context->Style.FrameRounding;
			_imgui_context->Style.ScrollbarRounding = _imgui_context->Style.FrameRounding;
			modified = true;
		}

		if (ImGui::SliderFloat("FPS Text Size", &_fps_scale, 0.2f, 2.5f, "%.1f"))
		{
			modified = true;
		}

		modified |= ImGui::ColorEdit4("FPS Text Color", _fps_col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
	}

	if (modified)
		save_config();
	if (reload_style) // Style is applied in "load_config()".
		load_config();
}
void reshade::runtime::draw_overlay_menu_statistics()
{
	uint64_t post_processing_time_cpu = 0;
	uint64_t post_processing_time_gpu = 0;

	for (const auto &technique : _techniques)
	{
		post_processing_time_cpu += technique.average_cpu_duration;
		post_processing_time_gpu += technique.average_gpu_duration;
	}

	if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushItemWidth(-1);
		ImGui::PlotLines("##framerate",
			_imgui_context->FramerateSecPerFrame, 120,
			_imgui_context->FramerateSecPerFrameIdx,
			nullptr,
			_imgui_context->FramerateSecPerFrameAccum / 120 * 0.5f,
			_imgui_context->FramerateSecPerFrameAccum / 120 * 1.5f,
			ImVec2(0, 50));
		ImGui::PopItemWidth();

		ImGui::BeginGroup();
		ImGui::TextUnformatted("Application:");
		ImGui::TextUnformatted("Date:");
		ImGui::TextUnformatted("Device:");
		ImGui::TextUnformatted("FPS:");
		ImGui::TextUnformatted("Post-Processing:");
		ImGui::TextUnformatted("Draw Calls:");
		ImGui::Text("Frame %llu:", _framecount + 1);
		ImGui::TextUnformatted("Timer:");
		ImGui::TextUnformatted("Network:");
		ImGui::EndGroup();

		ImGui::SameLine(ImGui::GetWindowWidth() * 0.333f);

		ImGui::BeginGroup();
		ImGui::Text("%X", std::hash<std::string>()(g_target_executable_path.stem().u8string()));
		ImGui::Text("%d-%d-%d %d", _date[0], _date[1], _date[2], _date[3]);
		ImGui::Text("%X %d", _vendor_id, _device_id);
		ImGui::Text("%.2f", _imgui_context->IO.Framerate);
		ImGui::Text("%f ms (CPU)", post_processing_time_cpu * 1e-6f);
		ImGui::Text("%u (%u vertices)", _drawcalls, _vertices);
		ImGui::Text("%f ms", _last_frame_duration.count() * 1e-6f);
		ImGui::Text("%f ms", std::chrono::duration_cast<std::chrono::nanoseconds>(_last_present_time - _start_time).count() * 1e-6f);
		ImGui::Text("%u B", g_network_traffic);
		ImGui::EndGroup();

		ImGui::SameLine(ImGui::GetWindowWidth() * 0.666f);

		ImGui::BeginGroup();
		ImGui::NewLine();
		ImGui::NewLine();
		ImGui::NewLine();
		ImGui::NewLine();
		if (post_processing_time_gpu != 0)
			ImGui::Text("%f ms (GPU)", (post_processing_time_gpu * 1e-6f));
		ImGui::EndGroup();
	}

	if (ImGui::CollapsingHeader("Effects", ImGuiTreeNodeFlags_DefaultOpen))
	{
		std::vector<const texture *> current_textures;
		current_textures.reserve(_textures.size());
		std::vector<const technique *> current_techniques;
		current_techniques.reserve(_techniques.size());

		for (size_t index = 0; index < _loaded_effects.size(); ++index)
		{
			const effect_data &effect = _loaded_effects[index];

			// Ignore unloaded effects
			if (effect.source_file.empty())
				continue;

			ImGui::PushID(static_cast<int>(index));

			ImGui::AlignTextToFramePadding();

			if (_selected_effect == index && _show_code_editor)
			{
				ImGui::TextUnformatted(">");
				ImGui::SameLine();
			}

			ImGui::TextDisabled("%s%lc", effect.source_file.parent_path().u8string().c_str(), std::filesystem::path::preferred_separator);
			ImGui::SameLine(0, 0);
			ImGui::TextUnformatted(effect.source_file.filename().u8string().c_str());

			const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;

			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - (80 + button_spacing + 150) + _imgui_context->Style.ItemSpacing.x, 0);

			if (ImGui::Button("Edit", ImVec2(80, 0)))
			{
				_selected_effect = index;
				_selected_effect_changed = true;
				_show_code_editor = true;
			}

			ImGui::SameLine(0, button_spacing);

			if (ImGui::Button("Show HLSL/GLSL code", ImVec2(150, 0)))
			{
				_editor.set_text(effect.module.hlsl);
				_selected_effect = std::numeric_limits<size_t>::max();
				_show_code_editor = true;
			}

			if (!effect.errors.empty())
			{
				ImGui::PushStyleColor(ImGuiCol_Text, effect.errors.find("error") != std::string::npos ? COLOR_RED : COLOR_YELLOW);
				ImGui::PushTextWrapPos();
				ImGui::TextUnformatted(effect.errors.c_str());
				ImGui::PopTextWrapPos();
				ImGui::PopStyleColor();
				ImGui::Spacing();
			}

			#pragma region Techniques
			current_techniques.clear();
			for (const auto &technique : _techniques)
				if (technique.effect_index == index && technique.impl != nullptr)
					current_techniques.push_back(&technique);

			if (!current_techniques.empty())
			{
				if (ImGui::BeginChild("Techniques", ImVec2(0, current_techniques.size() * ImGui::GetTextLineHeightWithSpacing() + _imgui_context->Style.FramePadding.y * 4), true, ImGuiWindowFlags_NoScrollWithMouse))
				{
					ImGui::BeginGroup();

					for (const auto &technique : current_techniques)
					{
						ImGui::PushStyleColor(ImGuiCol_Text, _imgui_context->Style.Colors[technique->enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled]);

						if (technique->passes.size() > 1)
							ImGui::Text("%s (%zu passes)", technique->name.c_str(), technique->passes.size());
						else
							ImGui::TextUnformatted(technique->name.c_str());

						ImGui::PopStyleColor();
					}

					ImGui::EndGroup();
					ImGui::SameLine(ImGui::GetWindowWidth() * 0.333f);
					ImGui::BeginGroup();

					for (const auto &technique : current_techniques)
						if (technique->enabled)
							ImGui::Text("%f ms (CPU) (%.0f%%)", technique->average_cpu_duration * 1e-6f, 100 * (technique->average_cpu_duration * 1e-6f) / (post_processing_time_cpu * 1e-6f));
						else
							ImGui::NewLine();

					ImGui::EndGroup();
					ImGui::SameLine(ImGui::GetWindowWidth() * 0.666f);
					ImGui::BeginGroup();

					for (const auto &technique : current_techniques)
						if (technique->enabled && technique->average_gpu_duration != 0)
							ImGui::Text("%f ms (GPU) (%.0f%%)", technique->average_gpu_duration * 1e-6f, 100 * (technique->average_gpu_duration * 1e-6f) / (post_processing_time_gpu * 1e-6f));
						else
							ImGui::NewLine();

					ImGui::EndGroup();
				} ImGui::EndChild();
			}
			#pragma endregion

			#pragma region Textures
			current_textures.clear();
			for (const auto &texture : _textures)
				if (texture.effect_index == index && texture.impl != nullptr && texture.impl_reference == texture_reference::none)
					current_textures.push_back(&texture);

			if (!current_textures.empty())
			{
				if (ImGui::BeginChild("Textures", ImVec2(0, current_textures.size() * ImGui::GetTextLineHeightWithSpacing() + _imgui_context->Style.FramePadding.y * 4), true, ImGuiWindowFlags_NoScrollWithMouse))
				{
					const char *texture_formats[] = {
						"unknown",
						"R8", "R16F", "R32F", "RG8", "RG16", "RG16F", "RG32F", "RGBA8", "RGBA16", "RGBA16F", "RGBA32F",
						"DXT1", "DXT3", "DXT5", "LATC1", "LATC2"
					};

					static_assert(_countof(texture_formats) - 1 == static_cast<unsigned int>(reshadefx::texture_format::latc2));

					for (const auto &texture : current_textures)
					{
						ImGui::Text("%s (%ux%u +%u %s)", texture->unique_name.c_str(), texture->width, texture->height, (texture->levels - 1), texture_formats[static_cast<unsigned int>(texture->format)]);

						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::Image(texture->impl.get(), ImVec2(std::max(texture->width * 0.5f, 500.0f), std::max(texture->height * 0.5f, texture->height * 500.0f / texture->width)));
							ImGui::EndTooltip();
						}
					}
				} ImGui::EndChild();
			}
			#pragma endregion

			ImGui::PopID();

			ImGui::Spacing();
		}
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

	if (ImGui::BeginChild("log", ImVec2(), true, _log_wordwrap ? 0 : ImGuiWindowFlags_AlwaysHorizontalScrollbar))
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
				ImVec4 textcol(1, 1, 1, 1);

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

	ImGui::PopTextWrapPos();
}

void reshade::runtime::draw_code_editor()
{
	const auto parse_errors = [this](const std::string &errors) {
		_editor.clear_errors();

		for (size_t offset = 0, next; offset != std::string::npos; offset = next)
		{
			const size_t pos_line = errors.find('(', offset);
			const size_t pos_error = errors.find(':', pos_line);
			const size_t pos_linefeed = errors.find('\n', pos_error);

			next = pos_linefeed != std::string::npos ? pos_linefeed + 1 : std::string::npos;

			if (pos_line == std::string::npos || pos_error == std::string::npos)
				break;

			// Ignore errors that aren't in the main source file
			const std::string error_file = errors.substr(offset, pos_line - offset);

			if (error_file != _loaded_effects[_selected_effect].source_file.u8string())
				continue;

			const int error_line = std::strtol(errors.c_str() + pos_line + 1, nullptr, 10);
			const std::string error_text = errors.substr(pos_error + 2 /* skip space */, pos_linefeed - pos_error - 2);

			_editor.add_error(error_line, error_text, error_text.find("warning") != std::string::npos);
		}
	};

	if (_selected_effect < _loaded_effects.size() && (ImGui::Button("Save & Compile (Ctrl + S)") || _input->is_key_pressed('S', true, false, false)))
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

		parse_errors(_loaded_effects[_selected_effect].errors);
	}

	ImGui::SameLine();

	ImGui::PushItemWidth(-1);

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
		const auto &effect = _loaded_effects[_selected_effect];

		// Load file to string and update editor text
		_editor.set_text(std::string(std::istreambuf_iterator<char>(std::ifstream(effect.source_file).rdbuf()), std::istreambuf_iterator<char>()));

		parse_errors(effect.errors);

		_selected_effect_changed = false;
	}

	_editor.render("Editor");
}

void reshade::runtime::draw_overlay_variable_editor()
{
	int imgui_id = 0;
	bool any_value_modified = false;
	bool current_tree_is_closed = true;
	bool current_category_is_closed = false;
	std::string current_filename;
	std::string current_category;

	if (_variable_editor_tabs)
	{
		ImGui::BeginTabBar("variables");
	}

	ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.5f);

	for (uniform &variable : _uniforms)
	{
		++imgui_id;

		// Skip hidden and special variables
		if (variable.hidden || variable.special != special_uniform::none)
			continue;

		// Hide variables that are not currently used in any of the active effects
		if (!_loaded_effects[variable.effect_index].rendering)
			continue;
		assert(_loaded_effects[variable.effect_index].compile_sucess);

		// Create separate tab for every effect file
		if (variable.effect_filename != current_filename)
		{
			const bool is_focused = _focused_effect == variable.effect_index;

			current_filename = variable.effect_filename;

			if (_variable_editor_tabs)
			{
				if (!current_tree_is_closed)
					ImGui::EndTabItem();

				current_tree_is_closed = !ImGui::BeginTabItem(current_filename.c_str());
			}
			else
			{
				if (!current_tree_is_closed)
					ImGui::TreePop();

				if (is_focused || _effects_expanded_state & 1)
					ImGui::SetNextTreeNodeOpen(is_focused || (_effects_expanded_state >> 1) != 0);

				current_tree_is_closed = !ImGui::TreeNodeEx(variable.effect_filename.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			}

			if (is_focused)
			{
				ImGui::SetScrollHereY(0.0f);

				_focused_effect = std::numeric_limits<size_t>::max();
			}
		}

		// Skip rendering invisible items
		if (current_tree_is_closed)
			continue;

		const std::string &ui_category = variable.annotations["ui_category"].second.string_data;

		if (ui_category != current_category)
		{
			if (!ui_category.empty())
			{
				std::string category;
				for (float x = 0, space_x = ImGui::CalcTextSize(" ").x, width = (ImGui::CalcItemWidth() - ImGui::CalcTextSize(ui_category.c_str()).x) / 2 - 42; x < width; x += space_x)
					category += ' ';
				category += ui_category;

				current_category_is_closed = !ImGui::TreeNodeEx(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen);
			}
			else
			{
				current_category_is_closed = false;
			}

			current_category = ui_category;
		}

		// Skip rendering invisible items
		if (current_category_is_closed)
			continue;

		bool modified = false;
		const auto ui_type = variable.annotations["ui_type"].second.string_data;
		const auto ui_label = variable.annotations.count("ui_label") ? variable.annotations.at("ui_label").second.string_data : variable.name;
		const auto ui_min = variable.annotations["ui_min"];
		const auto ui_max = variable.annotations["ui_max"];
		const auto ui_stp = variable.annotations["ui_step"];

		ImGui::PushID(imgui_id);

		switch (variable.type.base)
		{
		case reshadefx::type::t_bool:
		{
			bool data;
			get_uniform_value(variable, &data, 1);

			if (ui_type == "combo")
			{
				int current_item = data ? 1 : 0;
				modified = ImGui::Combo(ui_label.c_str(), &current_item, "Off\0On\0");
				data = current_item != 0;
			}
			else
			{
				modified = ImGui::Checkbox(ui_label.c_str(), &data);
			}

			if (modified)
				set_uniform_value(variable, &data, 1);
			break;
		}
		case reshadefx::type::t_int:
		case reshadefx::type::t_uint:
		{
			int data[4];
			get_uniform_value(variable, data, 4);

			const auto ui_min_val = ui_min.first.is_floating_point() ? static_cast<int>(ui_min.second.as_float[0]) : ui_min.second.as_int[0];
			const auto ui_max_val = ui_max.first.is_floating_point() ? static_cast<int>(ui_max.second.as_float[0]) : ui_max.second.as_int[0];
			const auto ui_stp_val = std::max(1, ui_stp.first.is_floating_point() ? static_cast<int>(ui_stp.second.as_float[0]) : ui_stp.second.as_int[0]);

			if (ui_type == "drag")
			{
				ImGui::PushItemWidth(ImGui::CalcItemWidth() - ( // same line space * 2 + button width * 2
					_imgui_context->Style.ItemInnerSpacing.x * 2 +
					(ImGui::CalcTextSize("+").x + _imgui_context->Style.FramePadding.x * 2) * 2));

				modified = ImGui::SliderScalarN("", variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, &ui_min_val, &ui_max_val);

				ImGui::PopItemWidth();
				ImGui::SameLine(0, 0);
				if (ImGui::Button("-") && data[0] > ui_min_val)
				{
					for (unsigned int c = 0; c < variable.type.rows; ++c)
						data[c] -= ui_stp_val;
					modified = true;
				}
				ImGui::SameLine(0, _imgui_context->Style.ItemInnerSpacing.x);
				if (ImGui::Button("+") && data[0] < ui_max_val)
				{
					for (unsigned int c = 0; c < variable.type.rows; ++c)
						data[c] += ui_stp_val;
					modified = true;
				}
				ImGui::SameLine(0, _imgui_context->Style.ItemInnerSpacing.x);
				ImGui::TextUnformatted(ui_label.c_str());
			}
			else if (ui_type == "drag2")
			{
				modified = ImGui::DragScalarN(ui_label.c_str(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows, static_cast<float>(ui_stp_val), &ui_min_val, &ui_max_val);
			}
			else if (ui_type == "combo")
			{
				std::string items = variable.annotations["ui_items"].second.string_data;
				// Make sure list is terminated with a zero in case user forgot so no invalid memory is read accidentally
				if (items.empty() || items.back() != '\0')
					items.push_back('\0');

				modified = ImGui::Combo(ui_label.c_str(), data, items.c_str());
			}
			else
			{
				modified = ImGui::InputScalarN(ui_label.c_str(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, data, variable.type.rows);
			}

			if (modified)
				set_uniform_value(variable, data, 4);
			break;
		}
		case reshadefx::type::t_float:
		{
			float data[4];
			get_uniform_value(variable, data, 4);

			const auto ui_min_val = ui_min.first.is_floating_point() ? ui_min.second.as_float[0] : static_cast<float>(ui_min.second.as_int[0]);
			const auto ui_max_val = ui_max.first.is_floating_point() ? ui_max.second.as_float[0] : static_cast<float>(ui_max.second.as_int[0]);
			const auto ui_stp_val = std::max(0.001f, ui_stp.first.is_floating_point() ? ui_stp.second.as_float[0] : static_cast<float>(ui_stp.second.as_int[0]));

			if (ui_type == "drag")
			{
				ImGui::PushItemWidth(ImGui::CalcItemWidth() - ( // same line space * 2 + button width * 2
					_imgui_context->Style.ItemInnerSpacing.x * 2 +
					(ImGui::CalcTextSize("+").x + _imgui_context->Style.FramePadding.x * 2) * 2));

				modified = ImGui::SliderScalarN("", ImGuiDataType_Float, data, variable.type.rows, &ui_min_val, &ui_max_val, "%.3f");

				ImGui::PopItemWidth();

				ImGui::SameLine(0, 0);
				if (ImGui::Button("-") && data[0] > ui_min_val)
				{
					for (unsigned int c = 0; c < variable.type.rows; ++c)
						data[c] -= ui_stp_val;
					modified = true;
				}
				ImGui::SameLine(0, _imgui_context->Style.ItemInnerSpacing.x);
				if (ImGui::Button("+") && data[0] < ui_max_val)
				{
					for (unsigned int c = 0; c < variable.type.rows; ++c)
						data[c] += ui_stp_val;
					modified = true;
				}
				ImGui::SameLine(0, _imgui_context->Style.ItemInnerSpacing.x);
				ImGui::TextUnformatted(ui_label.c_str());
			}
			else if (ui_type == "drag2")
			{
				modified = ImGui::DragScalarN(ui_label.c_str(), ImGuiDataType_Float, data, variable.type.rows, ui_stp_val, &ui_min_val, &ui_max_val, "%.3f");
			}
			else if (ui_type == "combo")
			{
				std::string items = variable.annotations["ui_items"].second.string_data;
				// Make sure list is terminated with a zero in case user forgot so no invalid memory is read accidentally
				if (items.empty() || items.back() != '\0')
					items.push_back('\0');

				int current_item = static_cast<int>(data[0]);
				modified = ImGui::Combo(ui_label.c_str(), &current_item, items.c_str());
				data[0] = static_cast<float>(current_item);
			}
			else if (ui_type == "input" || (ui_type.empty() && variable.type.rows < 3))
			{
				modified = ImGui::InputScalarN(ui_label.c_str(), ImGuiDataType_Float, data, variable.type.rows);
			}
			else if (variable.type.rows == 3)
			{
				modified = ImGui::ColorEdit3(ui_label.c_str(), data, ImGuiColorEditFlags_NoOptions);
			}
			else if (variable.type.rows == 4)
			{
				modified = ImGui::ColorEdit4(ui_label.c_str(), data, ImGuiColorEditFlags_NoOptions);
			}

			if (modified)
				set_uniform_value(variable, data, 4);
			break;
		}
		}

		// Display tooltip
		const auto &ui_tooltip = variable.annotations["ui_tooltip"].second.string_data;
		if (ImGui::IsItemHovered() && !ui_tooltip.empty())
			ImGui::SetTooltip("%s", ui_tooltip.c_str());

		// Create context menu
		if (ImGui::BeginPopupContextItem("context"))
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
		any_value_modified |= modified;
	}

	ImGui::PopItemWidth();

	if (_variable_editor_tabs)
	{
		if (!current_tree_is_closed)
			ImGui::EndTabItem();
		ImGui::EndTabBar();
	}
	else
	{
		if (!current_tree_is_closed)
			ImGui::TreePop();
	}

	if (_tutorial_index == 4)
	{
		if (any_value_modified)
			ImGui::OpenPopup("Performance Mode Hint");

		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

		if (ImGui::BeginPopup("Performance Mode Hint"))
		{
			ImGui::TextUnformatted(
				"Don't forget to enable 'Performance Mode' once you are done editing.\n"
				"This will increase runtime performance and loading times.");

			if (ImGui::Button("Ok", ImVec2(-1, 0)))
			{
				_tutorial_index++;

				save_config();

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar();
	}
}

void reshade::runtime::draw_overlay_technique_editor()
{
	size_t hovered_technique_index = std::numeric_limits<size_t>::max();

	_toggle_key_setting_active = false;

	for (size_t index = 0; index < _techniques.size(); ++index)
	{
		technique &technique = _techniques[index];

		// Skip hidden techniques
		if (technique.hidden)
			continue;

		ImGui::PushID(static_cast<int>(index));

		// Draw border around the item if it is selected
		const bool draw_border = _selected_technique == index;
		if (draw_border)
			ImGui::Separator();

		const bool clicked = _imgui_context->IO.MouseClicked[0];
		const bool compile_success = _loaded_effects[technique.effect_index].compile_sucess;
		if (!compile_success) // Prevent user from enabling the technique when the effect failed to compile
			_imgui_context->IO.MouseClicked[0] = false;
		assert(compile_success || !technique.enabled);

		// Gray out disabled techniques and mark techniques which failed to compile red
		ImGui::PushStyleColor(ImGuiCol_Text, compile_success ? _imgui_context->Style.Colors[technique.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled] : COLOR_RED);

		const std::string label = technique.name + " [" + technique.effect_filename + ']' + (!compile_success ? " (failed to compile)" : "");

		if (ImGui::Checkbox(label.c_str(), &technique.enabled))
		{
			if (technique.enabled)
				enable_technique(technique);
			else
				disable_technique(technique);
			save_current_preset();
		}

		_imgui_context->IO.MouseClicked[0] = clicked; // Reset click status

		ImGui::PopStyleColor();

		if (ImGui::IsItemActive())
			_selected_technique = index;
		if (ImGui::IsItemClicked())
			_focused_effect = technique.effect_index;
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
			hovered_technique_index = index;

		// Display tooltip
		const auto &ui_tooltip = technique.annotations["ui_tooltip"].second.string_data;
		if (ImGui::IsItemHovered() && !ui_tooltip.empty())
			ImGui::SetTooltip("%s", ui_tooltip.c_str());

		// Create context menu
		if (ImGui::BeginPopupContextItem("context"))
		{
			assert(technique.toggle_key_data[0] < 256);

			char edit_buffer[2048]; size_t offset = 0;
			if (technique.toggle_key_data[1]) memcpy(edit_buffer, "Ctrl + ", 8), offset += 7;
			if (technique.toggle_key_data[2]) memcpy(edit_buffer + offset, "Shift + ", 9), offset += 8;
			if (technique.toggle_key_data[3]) memcpy(edit_buffer + offset, "Alt + ", 7), offset += 6;
			memcpy(edit_buffer + offset, keyboard_keys[technique.toggle_key_data[0]], sizeof(*keyboard_keys));

			ImGui::InputTextEx("Toggle Key", edit_buffer, sizeof(edit_buffer), ImVec2(200, 0), ImGuiInputTextFlags_ReadOnly);

			if (ImGui::IsItemActive())
			{
				_toggle_key_setting_active = true;

				const unsigned int last_key_pressed = _input->last_key_pressed();

				if (last_key_pressed != 0)
				{
					if (last_key_pressed == 0x08) // Backspace
					{
						technique.toggle_key_data[0] = 0;
						technique.toggle_key_data[1] = 0;
						technique.toggle_key_data[2] = 0;
						technique.toggle_key_data[3] = 0;
					}
					else if (last_key_pressed < 0x10 || last_key_pressed > 0x12)
					{
						technique.toggle_key_data[0] = last_key_pressed;
						technique.toggle_key_data[1] = _input->is_key_down(0x11);
						technique.toggle_key_data[2] = _input->is_key_down(0x10);
						technique.toggle_key_data[3] = _input->is_key_down(0x12);
					}

					save_current_preset();
				}
			}
			else if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Click in the field and press any key to change the toggle shortcut to that key.\nPress backspace to disable the shortcut.");
			}

			ImGui::Separator();

			const bool is_not_top = index > 0;
			const bool is_not_bottom = index < _techniques.size() - 1;

			if (is_not_top && ImGui::Button("Move up", ImVec2(200, 0)))
			{
				std::swap(_techniques[index], _techniques[index - 1]);
				ImGui::CloseCurrentPopup();
			}
			if (is_not_bottom && ImGui::Button("Move down", ImVec2(200, 0)))
			{
				std::swap(_techniques[index], _techniques[index + 1]);
				ImGui::CloseCurrentPopup();
			}

			if (is_not_top && ImGui::Button("Move to top", ImVec2(200, 0)))
			{
				_techniques.insert(_techniques.begin(), std::move(_techniques[index]));
				_techniques.erase(_techniques.begin() + 1 + index);
				ImGui::CloseCurrentPopup();
			}
			if (is_not_bottom && ImGui::Button("Move to bottom", ImVec2(200, 0)))
			{
				_techniques.push_back(std::move(_techniques[index]));
				_techniques.erase(_techniques.begin() + index);
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
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

#endif
