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
		default:
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
	const bool show_splash = _show_splash && (_reload_remaining_effects != std::numeric_limits<size_t>::max() || !_reload_compile_queue.empty() || (_last_present_time - _last_reload_time) < std::chrono::seconds(5));
	const bool show_screenshot_message = _show_screenshot_message && _last_present_time - _last_screenshot_time < std::chrono::seconds(_screenshot_save_success ? 3 : 5);

	if (_show_menu && !_ignore_shortcuts && !_imgui_context->IO.NavVisible && _input->is_key_pressed(0x1B /* VK_ESCAPE */))
		_show_menu = false; // Close when pressing the escape button and not currently navigating with the keyboard
	else if (!_ignore_shortcuts && _input->is_key_pressed(_menu_key_data))
		_show_menu = !_show_menu;

	_ignore_shortcuts = false;
	_effects_expanded_state &= 2;

	if (_rebuild_font_atlas)
		build_font_atlas();
	if (_reload_remaining_effects != std::numeric_limits<size_t>::max())
		_selected_effect = std::numeric_limits<size_t>::max(),
		_selected_effect_changed = true, // Force editor to clear text after effects where reloaded
		_effect_filter_buffer[0] = '\0'; // And reset filter too, since the list of techniques might have changed

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
			else if (_tutorial_index == 0)
			{
				ImGui::TextUnformatted("ReShade is now installed successfully!");
				ImGui::Text(
					"Please press '%s' to start the tutorial =)", input::key_name(_menu_key_data).c_str());
			}
			else
			{
				ImGui::Text(
					"Press '%s' to open the configuration menu.", input::key_name(_menu_key_data).c_str());
				ImGui::Dummy(ImVec2(0, _imgui_context->Style.FramePadding.y - 1.0f)); // Add small offset to align with progress bar from reload screen
			}

			if (!_last_reload_successful)
			{
				ImGui::Spacing();
				ImGui::TextColored(COLOR_RED,
					"There were errors compiling some shaders. "
					"Open the configuration menu and switch to the 'Statistics' tab for more details.");
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

	if (_show_menu && _reload_remaining_effects == std::numeric_limits<size_t>::max())
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

		if (_show_code_editor)
		{
			const std::string title = _selected_effect < _loaded_effects.size() ?
				"Editing " + _loaded_effects[_selected_effect].source_file.filename().u8string() + " ###editor" : "Viewing code###editor";

			if (ImGui::Begin(title.c_str(), &_show_code_editor))
				draw_code_editor();
			ImGui::End();
		}

		for (auto it = _texture_previews.begin(); it != _texture_previews.end();)
		{
			const texture *texture = *it;

			bool open = true;
			char window_name[64];
			sprintf_s(window_name, "%s##%p", texture->unique_name.c_str(), *it);

			ImGui::Begin(window_name, &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
			imgui_image_with_checkerboard_background(texture->impl.get(), ImVec2(std::max(texture->width * 0.5f, 500.0f), std::max(texture->height * 0.5f, texture->height * 500.0f / texture->width)));
			ImGui::End();

			if (!open) // Close window by removing it from the list so it is not rendered next frame
				it = _texture_previews.erase(it);
			else ++it;
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
		"Pree 'Ctrl' + 'Tab' to switch between tabs and windows.\n\n"
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

		draw_preset_explorer();

		if (_tutorial_index == 1)
			ImGui::PopStyleColor(2);
	}

	if (_tutorial_index > 1)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

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
						std::transform(lhs_label.begin(), lhs_label.end(), lhs_label.begin(), tolower);
						std::string rhs_label(rhs.annotation_as_string("ui_label"));
						if (rhs_label.empty()) rhs_label = rhs.name;
						std::transform(rhs_label.begin(), rhs_label.end(), rhs_label.begin(), tolower);
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
				"This is the list of techniques. It contains all techniques in the effect files (*.fx) that were found in the effect search paths as specified on the 'Settings' tab.\n"
				"Additional options are hidden in a context menu that can be opened by clicking on them with the right mouse button.\n\n"
				"Enter text in the box at the top to filter it and search for specific techniques.\n\n"
				"Click on a technique to enable or disable it or drag it to a new location in the list to change the order in which the effects are applied.";

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
				"This is the list of variables. It contains all tweakable options the effects expose. All values here apply in real-time. Press 'Ctrl' and click on a widget to manually edit the value.\n"
				"Additional options are hidden in a context menu that can be opened by clicking on them with the right mouse button.\n\n"
				"Enter text in the box at the top to filter it and search for specific variables.\n\n"
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

		if ((_tutorial_index != 1 || !_current_preset_path.empty()) &&
			ImGui::Button(_tutorial_index == 3 ? "Finish" : "Continue", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
		{
			// Disable font scaling after tutorial
			if (_tutorial_index++ == 3)
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
		modified |= ImGui::Checkbox("Include Preset & Settings", &_screenshot_include_preset);
	}

	if (ImGui::CollapsingHeader("User Interface", ImGuiTreeNodeFlags_DefaultOpen))
	{
		modified |= ImGui::Checkbox("Show Screenshot Message", &_show_screenshot_message);

		bool save_imgui_window_state = _imgui_context->IO.IniFilename != nullptr;
		if (ImGui::Checkbox("Save Window State (ReShadeGUI.ini)", &save_imgui_window_state))
		{
			modified = true;
			_imgui_context->IO.IniFilename = save_imgui_window_state ? g_reshadegui_ini_path : nullptr;
		}

		modified |= ImGui::Checkbox("Experimental Variable Editing UI", &_variable_editor_tabs);

		#pragma region Style
		if (ImGui::Combo("Style", &_style_index, "Dark\0Light\0Default\0Custom Simple\0Custom Advanced\0"))
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
		if (ImGui::Combo("Editor Style", &_editor_style_index, "Dark\0Light\0Custom\0"))
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
	uint64_t post_processing_time_cpu = 0;
	uint64_t post_processing_time_gpu = 0;

	for (const auto &technique : _techniques)
	{
		post_processing_time_cpu += technique.average_cpu_duration;
		post_processing_time_gpu += technique.average_gpu_duration;
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
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.33333333f);
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
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.66666666f);
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

		ImGui::Checkbox("Show only active techniques", &_statistics_effects_show_enabled);

		for (size_t index = 0; index < _loaded_effects.size(); ++index)
		{
			const effect_data &effect = _loaded_effects[index];

			// Ignore unloaded effects, filter for active effects if enabled
			if (effect.source_file.empty() || (_statistics_effects_show_enabled && effect.rendering == 0))
				continue;

			ImGui::PushID(static_cast<int>(index));

			ImGui::AlignTextToFramePadding();

			if (_selected_effect == index && _show_code_editor)
			{
				ImGui::TextUnformatted(">");
				ImGui::SameLine();
			}

			const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;
			const float button_offset = ImGui::GetWindowContentRegionWidth() - (50 + button_spacing + 120);

			auto tree_id = ImGui::GetID("tree_open");
			bool tree_open = ImGui::GetStateStorage()->GetInt(tree_id, true);
			bool tree_toggle = false;

			// Hide parent path if window is small
			if (ImGui::CalcTextSize(effect.source_file.u8string().c_str()).x < button_offset)
			{
				ImGui::TextDisabled("%s%lc", effect.source_file.parent_path().u8string().c_str(), std::filesystem::path::preferred_separator);
				tree_toggle = ImGui::IsItemClicked();
				ImGui::SameLine(0, 0);
			}

			ImGui::TextUnformatted(effect.source_file.filename().u8string().c_str());
			tree_toggle |= ImGui::IsItemClicked();

			// Update tree state
			if (tree_toggle)
				ImGui::GetStateStorage()->SetInt(tree_id, tree_open = !tree_open);

			ImGui::SameLine(button_offset + _imgui_context->Style.ItemSpacing.x, 0);
			if (ImGui::Button("Edit", ImVec2(50, 0)))
			{
				_selected_effect = index;
				_selected_effect_changed = true;
				_show_code_editor = true;
			}

			ImGui::SameLine(0, button_spacing);
			if (ImGui::Button("Show HLSL/GLSL", ImVec2(120, 0)))
			{
				const std::string source_code = effect.preamble + effect.module.hlsl;

				// Act as a toggle when already showing the generated code
				if (_show_code_editor
					&& _selected_effect == std::numeric_limits<size_t>::max()
					&& _editor.get_text() == source_code)
				{
					_show_code_editor = false;
				}
				else
				{
					_editor.set_text(source_code);
					_selected_effect = std::numeric_limits<size_t>::max();
					_selected_effect_changed = false; // Prevent editor from being cleared, since we already set the text here
					_show_code_editor = true;
				}
			}

			if (tree_open)
			{
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
						ImGui::SameLine(ImGui::GetWindowWidth() * 0.33333333f);
						ImGui::BeginGroup();

						for (const technique *technique : current_techniques)
							if (technique->enabled)
								ImGui::Text("%f ms (CPU) (%.0f%%)", technique->average_cpu_duration * 1e-6f, 100 * (technique->average_cpu_duration * 1e-6f) / (post_processing_time_cpu * 1e-6f));
							else
								ImGui::NewLine();

						ImGui::EndGroup();
						ImGui::SameLine(ImGui::GetWindowWidth() * 0.66666666f);
						ImGui::BeginGroup();

						for (const technique *technique : current_techniques)
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

						for (const texture *texture : current_textures)
						{
							ImGui::Text("%s (%ux%u +%u %s)", texture->unique_name.c_str(), texture->width, texture->height, (texture->levels - 1), texture_formats[static_cast<unsigned int>(texture->format)]);

							if (std::find(_texture_previews.begin(), _texture_previews.end(), texture) != _texture_previews.end())
								continue;

							if (ImGui::IsItemClicked())
							{
								_texture_previews.push_back(texture);
							}
							else if (ImGui::IsItemHovered())
							{
								ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
								ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
								ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(204, 204, 204, 255));
								ImGui::BeginTooltip();
								imgui_image_with_checkerboard_background(texture->impl.get(), ImVec2(std::max(texture->width * 0.5f, 500.0f), std::max(texture->height * 0.5f, texture->height * 500.0f / texture->width)));
								ImGui::EndTooltip();
								ImGui::PopStyleColor();
								ImGui::PopStyleVar(2);
							}
						}
					} ImGui::EndChild();
				}
				#pragma endregion
			}

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
		assert(_reload_remaining_effects == 0);

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
		const float button_size = ImGui::GetFrameHeight();
		const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;

		ImGui::BeginChild("##definitions", ImVec2(400.0f, (std::max(_global_preprocessor_definitions.size(), _preset_preprocessor_definitions.size()) + 2) * ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_NoScrollWithMouse);

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
					modified |= ImGui::InputText("##name", name, sizeof(name));
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
					modified |= ImGui::InputText("##name", name, sizeof(name));
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

				current_category_is_closed = !ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen);
			}
			else
			{
				current_category_is_closed = false;
			}
		}

		// Skip rendering invisible items
		if (current_category_is_closed)
			continue;

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

		// Draw border around the item if it is selected
		const bool draw_border = _selected_technique == index;
		if (draw_border)
			ImGui::Separator();

		const bool clicked = _imgui_context->IO.MouseClicked[0];
		const bool compile_success = _loaded_effects[technique.effect_index].compile_sucess;
		assert(compile_success || !technique.enabled);

		// Prevent user from enabling the technique when the effect failed to compile
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !compile_success);
		// Gray out disabled techniques and mark techniques which failed to compile red
		ImGui::PushStyleColor(ImGuiCol_Text, compile_success ? _imgui_context->Style.Colors[technique.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled] : COLOR_RED);

		std::string_view ui_label = technique.annotation_as_string("ui_label");
		if (ui_label.empty() || !compile_success) ui_label = technique.name;
		std::string label(ui_label.data(), ui_label.size());
		label += " [" + _loaded_effects[technique.effect_index].source_file.filename().u8string() + ']' + (!compile_success ? " (failed to compile)" : "");

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
		if (const std::string_view tooltip = compile_success ? technique.annotation_as_string("ui_tooltip") : _loaded_effects[technique.effect_index].errors;
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
			if (imgui_key_input("Toggle Key", technique.toggle_key_data, *_input))
				save_current_preset();
			_ignore_shortcuts |= ImGui::IsItemActive();

			ImGui::Separator();

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

			ImGui::EndPopup();
		}

		if (technique.toggle_key_data[0] != 0 && compile_success)
		{
			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() * 0.75f);
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

	if (ImGui::ButtonEx("<", ImVec2(button_size, 0), ImGuiButtonFlags_NoNavFocus))
		condition = condition::backward, _current_browse_path = _current_preset_path;

	if (ImGui::SameLine(0, button_spacing);
		ImGui::ButtonEx(">", ImVec2(button_size, 0), ImGuiButtonFlags_NoNavFocus))
		condition = condition::forward, _current_browse_path = _current_preset_path;

	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
	if (ImGui::SameLine(0, button_spacing);
		ImGui::ButtonEx(_current_preset_path.stem().u8string().c_str(), ImVec2(root_window_width - (button_spacing + button_size) * 3, 0), ImGuiButtonFlags_NoNavFocus))
		if (ImGui::OpenPopup("##explore"), _imgui_context->IO.KeyCtrl)
			_browse_path_is_input_mode = true;
	ImGui::PopStyleVar();

	if (ImGui::SameLine(0, button_spacing); ImGui::ButtonEx("+", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_NoNavFocus))
		condition = condition::popup_add, _current_browse_path = _current_preset_path.parent_path();

	ImGui::SetNextWindowPos(cursor_pos - _imgui_context->Style.WindowPadding);
	const bool is_explore_open = ImGui::BeginPopup("##explore");
	if (is_explore_open)
	{
		if (ImGui::ButtonEx("<", ImVec2(button_size, 0), ImGuiButtonFlags_NoNavFocus))
			condition = condition::backward, _current_browse_path = _current_preset_path;

		if (ImGui::SameLine(0, button_spacing);
			ImGui::ButtonEx(">", ImVec2(button_size, 0), ImGuiButtonFlags_NoNavFocus))
			condition = condition::forward, _current_browse_path = _current_preset_path;

		ImGui::SameLine(0, button_spacing);
		if (_browse_path_is_input_mode)
		{
			char buf[_MAX_PATH]{};
			_current_browse_path.u8string().copy(buf, sizeof(buf) - 1);

			const bool is_edited = ImGui::InputTextEx("##path", buf, sizeof(buf), ImVec2(root_window_width - (button_spacing + button_size) * 3, 0), ImGuiInputTextFlags_None);
			const bool is_returned = ImGui::IsKeyPressedMap(ImGuiKey_Enter);

			if (ImGui::IsWindowAppearing())
				ImGui::SetKeyboardFocusHere();
			else if (!ImGui::IsItemActive())
				_browse_path_is_input_mode = false;

			if (is_edited || is_returned)
			{
				std::filesystem::path input_preset_path = std::filesystem::u8path(buf);
				std::filesystem::file_type file_type = std::filesystem::status(reshade_container_path / input_preset_path, ec).type();

				if (is_edited && ec.value() != 0x7b) // 0x7b: ERROR_INVALID_NAME
					_current_browse_path = std::move(input_preset_path);

				if (is_returned)
				{
					if (_current_browse_path.empty())
						condition = condition::cancel;
					else if (ec.value() == 0x7b) // 0x7b: ERROR_INVALID_NAME
						condition = condition::pass;
					else if (file_type == std::filesystem::file_type::directory)
						condition = condition::popup_add;
					else
					{
						if (_current_browse_path.has_filename())
							if (const std::wstring extension(_current_browse_path.extension()); extension != L".ini" && extension != L".txt")
								_current_browse_path += L".ini",
								file_type = std::filesystem::status(reshade_container_path / _current_browse_path, ec).type();

						if (ec.value() == 0x7b) // 0x7b: ERROR_INVALID_NAME
							condition = condition::pass;
						else if (file_type == std::filesystem::file_type::directory)
							condition = condition::popup_add;
						else if (file_type == std::filesystem::file_type::not_found)
							if (_current_browse_path.has_filename())
								condition = condition::create;
							else
								condition = condition::popup_add;
						else
							condition = condition::select;
					}
				}
			}
		}
		else
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
			if (ImGui::ButtonEx(_current_preset_path.stem().u8string().c_str(), ImVec2(root_window_width - (button_spacing + button_size) * 3, 0), ImGuiButtonFlags_NoNavFocus))
				if (_imgui_context->IO.KeyCtrl)
					ImGui::ActivateItem(ImGui::GetID("##path")), _browse_path_is_input_mode = true;
				else
					condition = condition::cancel;
			else if (ImGui::IsKeyPressedMap(ImGuiKey_Enter))
				condition = condition::reload, _current_browse_path = _current_preset_path;
			ImGui::PopStyleVar();
		}
	}

	if (is_explore_open || condition == condition::backward || condition == condition::forward)
	{
		std::filesystem::path preset_container_path = std::filesystem::absolute(reshade_container_path / _current_browse_path);
		if (!std::filesystem::is_directory(preset_container_path, ec) && preset_container_path.has_filename())
			preset_container_path = preset_container_path.parent_path();

		std::vector<std::filesystem::directory_entry> preset_container;
		for (const auto &entry : std::filesystem::directory_iterator(preset_container_path, std::filesystem::directory_options::skip_permission_denied, ec))
			preset_container.push_back(entry);

		if (condition == condition::backward || condition == condition::forward)
		{
			std::vector<std::filesystem::directory_entry> preset_paths;
			for (const auto &entry : preset_container)
				if (!entry.is_directory())
					if (const std::wstring extension(entry.path().extension()); extension == L".ini" || extension == L".txt")
						if (const reshade::ini_file preset(entry); preset.has("", "TechniqueSorting"))
							preset_paths.push_back(entry);

			if (preset_paths.begin() == preset_paths.end())
				condition = condition::pass;
			else
			{
				const std::filesystem::path &current_preset_path = _current_preset_path;
				if (auto it = std::find_if(preset_paths.begin(), preset_paths.end(), [&ec, &current_preset_path](const std::filesystem::directory_entry &entry) { return std::filesystem::equivalent(entry, current_preset_path, ec); }); it == preset_paths.end())
					if (condition == condition::backward)
						_current_preset_path = _current_browse_path = preset_paths.back();
					else
						_current_preset_path = _current_browse_path = preset_paths.front();
				else
					if (condition == condition::backward)
						_current_preset_path = _current_browse_path = it == preset_paths.begin() ? preset_paths.back() : *--it;
					else
						_current_preset_path = _current_browse_path = it == preset_paths.end() - 1 ? preset_paths.front() : *++it;
			}
		}

		if (is_explore_open)
		{
			if (ImGui::SameLine(0, button_spacing); ImGui::ButtonEx("+", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_NoNavFocus))
				if (const std::filesystem::file_type file_type = std::filesystem::status(reshade_container_path / _current_browse_path, ec).type(); ec.value() != 0x7b) // 0x7b: ERROR_INVALID_NAME
					if (condition = condition::popup_add; file_type == std::filesystem::file_type::not_found || file_type != std::filesystem::file_type::directory)
						_current_browse_path = _current_browse_path.parent_path();

			if (ImGui::IsWindowAppearing() || condition == condition::backward || condition == condition::forward)
				ImGui::SetNextWindowFocus();

			if (ImGui::BeginChild("##paths", ImVec2(0, 300), true))
			{
				if (ImGui::Selectable(".."))
				{
					for (_current_browse_path = std::filesystem::absolute(reshade_container_path / _current_browse_path);
						!std::filesystem::is_directory(_current_browse_path, ec) && _current_browse_path.parent_path() != _current_browse_path;)
						_current_browse_path = _current_browse_path.parent_path();
					_current_browse_path = _current_browse_path.parent_path();

					if (std::filesystem::equivalent(reshade_container_path, _current_browse_path))
						_current_browse_path = L".";
					else if (std::equal(reshade_container_path.begin(), reshade_container_path.end(), _current_browse_path.begin()))
						_current_browse_path = _current_browse_path.lexically_proximate(reshade_container_path);
				}

				std::vector<std::filesystem::directory_entry> preset_paths;
				for (const auto &entry : preset_container)
					if (const std::filesystem::file_type file_type = entry.status(ec).type(); file_type == std::filesystem::file_type::directory)
						if (ImGui::Selectable((entry.path().filename().u8string() + '\\').c_str()))
							if (std::filesystem::equivalent(reshade_container_path, entry))
								_current_browse_path = L".";
							else if (std::equal(reshade_container_path.begin(), reshade_container_path.end(), entry.path().begin()))
								_current_browse_path = entry.path().lexically_proximate(reshade_container_path);
							else
								_current_browse_path = entry;
						else {}
					else if (const std::wstring extension(entry.path().extension()); extension == L".ini" || extension == L".txt")
						preset_paths.push_back(entry);

				for (const auto &entry : preset_paths)
				{
					const bool is_current_preset = std::filesystem::equivalent(entry, _current_preset_path, ec);

					if (ImGui::Selectable(entry.path().filename().u8string().c_str(), static_cast<bool>(is_current_preset)))
						condition = condition::select, _current_browse_path = entry.path().lexically_proximate(reshade_container_path);

					if (is_current_preset)
						if (_current_preset_is_covered && !ImGui::IsWindowAppearing())
							_current_preset_is_covered = false, ImGui::SetScrollHereY();
						else if (condition == condition::backward || condition == condition::forward)
							ImGui::SetScrollHereY();
				}
			}
			ImGui::EndChild();
		}
	}

	if (condition == condition::select)
		if (const reshade::ini_file preset(reshade_container_path / _current_browse_path); !preset.has("", "TechniqueSorting"))
			condition = condition::pass;

	if (condition == condition::popup_add)
		condition = condition::pass, ImGui::OpenPopup("##name");

	ImGui::SetNextWindowPos(cursor_pos + ImVec2(root_window_width + button_size - 230, button_size));
	if (ImGui::BeginPopup("##name"))
	{
		if (ImGui::IsWindowAppearing())
			ImGui::SetKeyboardFocusHere();

		char filename[_MAX_PATH]{};
		ImGui::InputText("Name", filename, sizeof(filename));

		if (ImGui::IsKeyPressedMap(ImGuiKey_Enter))
		{
			if (std::filesystem::path input_preset_path = std::filesystem::u8path(filename); input_preset_path.has_filename())
			{
				if (const std::wstring extension(input_preset_path.extension()); extension != L".ini" && extension != L".txt")
					input_preset_path += L".ini";
				if (const std::filesystem::file_type file_type = std::filesystem::status(reshade_container_path / _current_browse_path / input_preset_path, ec).type(); ec.value() == 0x7b) // 0x7b: ERROR_INVALID_NAME
					condition = condition::pass;
				else if (file_type == std::filesystem::file_type::directory)
					condition = condition::pass;
				else if (file_type == std::filesystem::file_type::not_found)
					condition = condition::create;
				else if (const reshade::ini_file preset(reshade_container_path / _current_browse_path / input_preset_path); preset.has("", "TechniqueSorting"))
					condition = condition::select;
				else
					condition = condition::pass;

				if (condition != condition::pass)
					_current_browse_path /= input_preset_path;
			}
		}

		if (condition != condition::pass)
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	if (condition != condition::pass)
	{
		if (condition != condition::cancel)
		{
			_show_splash = true;
			set_current_preset();

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
