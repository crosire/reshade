/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_GUI

#include "runtime.hpp"
#include "runtime_internal.hpp"
#include "version.h"
#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "ini_file.hpp"
#include "addon_manager.hpp"
#include "input.hpp"
#include "input_gamepad.hpp"
#include "imgui_widgets.hpp"
#include "localization.hpp"
#include "platform_utils.hpp"
#include "fonts/forkawesome.inl"
#include "fonts/glyph_ranges.hpp"
#include <fstream>
#include <algorithm>

static bool filter_text(const std::string_view text, const std::string_view filter)
{
	return filter.empty() ||
		std::search(text.cbegin(), text.cend(), filter.cbegin(), filter.cend(),
			[](const char c1, const char c2) { // Search case-insensitive
				return (('a' <= c1 && c1 <= 'z') ? static_cast<char>(c1 - ' ') : c1) == (('a' <= c2 && c2 <= 'z') ? static_cast<char>(c2 - ' ') : c2);
			}) != text.cend();
}
static auto filter_name(ImGuiInputTextCallbackData *data) -> int
{
	// A file name cannot contain any of the following characters
	return data->EventChar == L'\"' || data->EventChar == L'*' || data->EventChar == L'/' || data->EventChar == L':' || data->EventChar == L'<' || data->EventChar == L'>' || data->EventChar == L'?' || data->EventChar == L'\\' || data->EventChar == L'|';
}

template <typename F>
static void parse_errors(const std::string_view errors, F &&callback)
{
	for (size_t offset = 0, next; offset != std::string_view::npos; offset = next)
	{
		const size_t pos_error = errors.find(": ", offset);
		const size_t pos_error_line = errors.rfind('(', pos_error); // Paths can contain '(', but no ": ", so search backwards from the error location to find the line info
		if (pos_error == std::string_view::npos || pos_error_line == std::string_view::npos || pos_error_line < offset)
			break;

		const size_t pos_linefeed = errors.find('\n', pos_error);

		next = pos_linefeed != std::string_view::npos ? pos_linefeed + 1 : std::string_view::npos;

		const std::string_view error_file = errors.substr(offset, pos_error_line - offset);
		int error_line = static_cast<int>(std::strtol(errors.data() + pos_error_line + 1, nullptr, 10));
		const std::string_view error_text = errors.substr(pos_error + 2 /* skip space */, pos_linefeed - pos_error - 2);

		callback(error_file, error_line, error_text);
	}
}

template <typename T>
static std::string_view get_localized_annotation(T &object, const std::string_view ann_name, [[maybe_unused]] std::string language)
{
#if RESHADE_LOCALIZATION
	if (language.size() >= 2)
	{
		// Transform language name from e.g. 'en-US' to 'en_us'
		std::replace(language.begin(), language.end(), '-', '_');
		std::transform(language.begin(), language.end(), language.begin(),
			[](std::string::value_type c) {
				return static_cast<std::string::value_type>(tolower(c));
			});

		for (int attempt = 0; attempt < 2; ++attempt)
		{
			const std::string_view localized_result = object.annotation_as_string(std::string(ann_name) + '_' + language);
			if (!localized_result.empty())
				return localized_result;
			else if (attempt == 0)
				language.erase(2); // Remove location information from language name, so that it e.g. becomes 'en'
		}
	}
#endif
	return object.annotation_as_string(ann_name);
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

	ImGuiContext *const backup_context = ImGui::GetCurrentContext();
	_imgui_context = ImGui::CreateContext();

	ImGuiIO &imgui_io = _imgui_context->IO;
	imgui_io.IniFilename = nullptr;
	imgui_io.ConfigFlags = ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
	imgui_io.BackendFlags = ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_RendererHasVtxOffset;

	ImGuiStyle &imgui_style = _imgui_context->Style;
	// Disable rounding by default
	imgui_style.GrabRounding = 0.0f;
	imgui_style.FrameRounding = 0.0f;
	imgui_style.ChildRounding = 0.0f;
	imgui_style.ScrollbarRounding = 0.0f;
	imgui_style.WindowRounding = 0.0f;
	imgui_style.WindowBorderSize = 0.0f;

	// Restore previous context in case this was called from a new runtime being created from an add-on event triggered by an existing runtime
	ImGui::SetCurrentContext(backup_context);
}
void reshade::runtime::deinit_gui()
{
	ImGui::DestroyContext(_imgui_context);
}

void reshade::runtime::build_font_atlas()
{
	ImFontAtlas *const atlas = _imgui_context->IO.Fonts;

	if (atlas->IsBuilt())
		return;

	ImGuiContext *const backup_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(_imgui_context);

	// Remove any existing fonts from atlas first
	atlas->Clear();

	std::error_code ec;
	const ImWchar *glyph_ranges = nullptr;
	std::filesystem::path resolved_font_path;

#if RESHADE_LOCALIZATION
	std::string language = _selected_language;
	if (language.empty())
		language = resources::get_current_language();

	if (language.find("bg") == 0)
	{
		glyph_ranges = atlas->GetGlyphRangesCyrillic();

		_default_font_path = L"C:\\Windows\\Fonts\\calibri.ttf";
	}
	else
	if (language.find("ja") == 0)
	{
		glyph_ranges = atlas->GetGlyphRangesJapanese();

		// Morisawa BIZ UDGothic Regular, available since Windows 10 October 2018 Update (1809) Build 17763.1
		_default_font_path = L"C:\\Windows\\Fonts\\BIZ-UDGothicR.ttc";
		if (!std::filesystem::exists(_default_font_path, ec))
			_default_font_path = L"C:\\Windows\\Fonts\\msgothic.ttc"; // MS Gothic
	}
	else
	if (language.find("ko") == 0)
	{
		glyph_ranges = atlas->GetGlyphRangesKorean();

		_default_font_path = L"C:\\Windows\\Fonts\\malgun.ttf"; // Malgun Gothic
	}
	else
	if (language.find("zh") == 0)
	{
		glyph_ranges = GetGlyphRangesChineseSimplifiedGB2312();

		_default_font_path = L"C:\\Windows\\Fonts\\msyh.ttc"; // Microsoft YaHei
		if (!std::filesystem::exists(_default_font_path, ec))
			_default_font_path = L"C:\\Windows\\Fonts\\simsun.ttc"; // SimSun
	}
	else
#endif
	{
		glyph_ranges = atlas->GetGlyphRangesDefault();

		_default_font_path.clear();
	}

	extern bool resolve_path(std::filesystem::path &path, std::error_code &ec);

	ImFontConfig cfg;
	cfg.GlyphOffset.y = std::floor(_font_size / 13.0f); // Not used in AddFontDefault()
	cfg.SizePixels = static_cast<float>(_font_size);

#if RESHADE_LOCALIZATION
	// Add latin font
	resolved_font_path = _latin_font_path;
	if (!_default_font_path.empty())
	{
		if (!resolved_font_path.empty() && !(resolve_path(resolved_font_path, ec) && atlas->AddFontFromFileTTF(resolved_font_path.u8string().c_str(), cfg.SizePixels, &cfg, atlas->GetGlyphRangesDefault()) != nullptr))
		{
			LOG(ERROR) << "Failed to load latin font from " << resolved_font_path << " with error code " << ec.value() << '!';
			resolved_font_path.clear();
		}

		if (resolved_font_path.empty())
			atlas->AddFontDefault(&cfg);

		cfg.MergeMode = true;
	}
#endif

	// Add main font
	resolved_font_path = _font_path.empty() ? _default_font_path : _font_path;
	{
		if (!resolved_font_path.empty() && !(resolve_path(resolved_font_path, ec) && atlas->AddFontFromFileTTF(resolved_font_path.u8string().c_str(), cfg.SizePixels, &cfg, glyph_ranges) != nullptr))
		{
			LOG(ERROR) << "Failed to load font from " << resolved_font_path << " with error code " << ec.value() << '!';
			resolved_font_path.clear();
		}

		// Use default font if custom font failed to load
		if (resolved_font_path.empty())
			atlas->AddFontDefault(&cfg);

		// Merge icons into main font
		cfg.MergeMode = true;
		cfg.PixelSnapH = true;

		// This need to be static so that it doesn't fall out of scope before the atlas is built below
		static constexpr ImWchar icon_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 }; // Zero-terminated list

		atlas->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_FK, cfg.SizePixels, &cfg, icon_ranges);
	}

	// Add editor font
	resolved_font_path = _editor_font_path.empty() ? _default_editor_font_path : _editor_font_path;
	if (resolved_font_path != _font_path || _editor_font_size != _font_size)
	{
		cfg = ImFontConfig();
		cfg.SizePixels = static_cast<float>(_editor_font_size);

		if (!resolved_font_path.empty() && !(resolve_path(resolved_font_path, ec) && atlas->AddFontFromFileTTF(resolved_font_path.u8string().c_str(), cfg.SizePixels, &cfg, glyph_ranges) != nullptr))
		{
			LOG(ERROR) << "Failed to load editor font from " << resolved_font_path << " with error code " << ec.value() << '!';
			resolved_font_path.clear();
		}

		if (resolved_font_path.empty())
			atlas->AddFontDefault(&cfg);
	}

	if (atlas->Build())
	{
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Font atlas size: " << atlas->TexWidth << 'x' << atlas->TexHeight;
#endif
	}
	else
	{
		LOG(ERROR) << "Failed to build font atlas!";

		_font_path.clear();
		_latin_font_path.clear();
		_editor_font_path.clear();

		atlas->Clear();

		// If unable to build font atlas due to an invalid custom font, revert to the default font
		for (int i = 0; i < (_editor_font_size != _font_size ? 2 : 1); ++i)
		{
			cfg = ImFontConfig();
			cfg.SizePixels = static_cast<float>(i == 0 ? _font_size : _editor_font_size);

			atlas->AddFontDefault(&cfg);
		}
	}

	ImGui::SetCurrentContext(backup_context);

	_show_splash = true;

	int width, height;
	unsigned char *pixels;
	// This will also build the font atlas again if that previously failed above
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

	// Texture data is now uploaded, so can free the memory
	atlas->ClearTexData();

	if (!_device->create_resource_view(_font_atlas_tex, api::resource_usage::shader_resource, api::resource_view_desc(api::format::r8g8b8a8_unorm), &_font_atlas_srv))
	{
		LOG(ERROR) << "Failed to create font atlas resource view!";
		return;
	}

	_device->set_resource_name(_font_atlas_tex, "ImGui font atlas");
}

void reshade::runtime::load_config_gui(const ini_file &config)
{
	if (_input_gamepad != nullptr)
		_imgui_context->IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	else
		_imgui_context->IO.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;

	const auto config_get = [&config](const std::string &section, const std::string &key, auto &values) {
		if (config.get(section, key, values))
			return true;
		// Fall back to global configuration when an entry does not exist in the local configuration
		return global_config().get(section, key, values);
	};

	config_get("INPUT", "KeyOverlay", _overlay_key_data);
	config_get("INPUT", "KeyFPS", _fps_key_data);
	config_get("INPUT", "KeyFrameTime", _frametime_key_data);
	config_get("INPUT", "InputProcessing", _input_processing_mode);

#if RESHADE_LOCALIZATION
	config_get("OVERLAY", "Language", _selected_language);
#endif

	config.get("OVERLAY", "ClockFormat", _clock_format);
	config.get("OVERLAY", "FPSPosition", _fps_pos);
	config.get("OVERLAY", "NoFontScaling", _no_font_scaling);
	config.get("OVERLAY", "ShowClock", _show_clock);
#if RESHADE_FX
	config.get("OVERLAY", "ShowForceLoadEffectsButton", _show_force_load_effects_button);
#endif
	config.get("OVERLAY", "ShowFPS", _show_fps);
	config.get("OVERLAY", "ShowFrameTime", _show_frametime);
	config.get("OVERLAY", "ShowPresetName", _show_preset_name);
	config.get("OVERLAY", "ShowScreenshotMessage", _show_screenshot_message);
#if RESHADE_FX
	if (!global_config().get("OVERLAY", "TutorialProgress", _tutorial_index))
		config.get("OVERLAY", "TutorialProgress", _tutorial_index);
	config.get("OVERLAY", "VariableListHeight", _variable_editor_height);
	config.get("OVERLAY", "VariableListUseTabs", _variable_editor_tabs);
	config.get("OVERLAY", "AutoSavePreset", _auto_save_preset);
	config.get("OVERLAY", "ShowPresetTransitionMessage", _show_preset_transition_message);
#endif

	ImGuiStyle &imgui_style = _imgui_context->Style;
	config.get("STYLE", "Alpha", imgui_style.Alpha);
	config.get("STYLE", "ChildRounding", imgui_style.ChildRounding);
	config.get("STYLE", "ColFPSText", _fps_col);
	config.get("STYLE", "EditorFont", _editor_font_path);
	config.get("STYLE", "EditorFontSize", _editor_font_size);
	config.get("STYLE", "EditorStyleIndex", _editor_style_index);
	config.get("STYLE", "Font", _font_path);
	config.get("STYLE", "FontSize", _font_size);
	config.get("STYLE", "FPSScale", _fps_scale);
	config.get("STYLE", "FrameRounding", imgui_style.FrameRounding);
	config.get("STYLE", "GrabRounding", imgui_style.GrabRounding);
	config.get("STYLE", "LatinFont", _latin_font_path);
	config.get("STYLE", "PopupRounding", imgui_style.PopupRounding);
	config.get("STYLE", "ScrollbarRounding", imgui_style.ScrollbarRounding);
	config.get("STYLE", "StyleIndex", _style_index);
	config.get("STYLE", "TabRounding", imgui_style.TabRounding);
	config.get("STYLE", "WindowRounding", imgui_style.WindowRounding);
	config.get("STYLE", "HdrOverlayBrightness", _hdr_overlay_brightness);
	config.get("STYLE", "HdrOverlayOverwriteColorSpaceTo", reinterpret_cast<int &>(_hdr_overlay_overwrite_color_space));

	// For compatibility with older versions, set the alpha value if it is missing
	if (_fps_col[3] == 0.0f)
		_fps_col[3]  = 1.0f;

	load_custom_style();

	if (_imgui_context->SettingsLoaded)
		return;

	ImGuiContext *const backup_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(_imgui_context);

	// Call all pre-read handlers, before reading config data (since they affect state that is then updated in the read handlers below)
	for (ImGuiSettingsHandler &handler : _imgui_context->SettingsHandlers)
		if (handler.ReadInitFn)
			handler.ReadInitFn(_imgui_context, &handler);

	for (ImGuiSettingsHandler &handler : _imgui_context->SettingsHandlers)
	{
		if (std::vector<std::string> lines;
			config.get("OVERLAY", handler.TypeName, lines))
		{
			void *entry_data = nullptr;

			for (const std::string &line : lines)
			{
				if (line.empty())
					continue;

				if (line[0] == '[')
				{
					const size_t name_beg = line.find('[', 1) + 1;
					const size_t name_end = line.rfind(']');

					entry_data = handler.ReadOpenFn(_imgui_context, &handler, line.substr(name_beg, name_end - name_beg).c_str());
				}
				else
				{
					assert(entry_data != nullptr);
					handler.ReadLineFn(_imgui_context, &handler, entry_data, line.c_str());
				}
			}
		}
	}

	_imgui_context->SettingsLoaded = true;

	for (ImGuiSettingsHandler &handler : _imgui_context->SettingsHandlers)
		if (handler.ApplyAllFn)
			handler.ApplyAllFn(_imgui_context, &handler);

	ImGui::SetCurrentContext(backup_context);
}
void reshade::runtime::save_config_gui(ini_file &config) const
{
	config.set("INPUT", "KeyOverlay", _overlay_key_data);
	config.set("INPUT", "KeyFPS", _fps_key_data);
	config.set("INPUT", "KeyFrametime", _frametime_key_data);
	config.set("INPUT", "InputProcessing", _input_processing_mode);

#if RESHADE_LOCALIZATION
	config.set("OVERLAY", "Language", _selected_language);
#endif

	config.set("OVERLAY", "ClockFormat", _clock_format);
	config.set("OVERLAY", "FPSPosition", _fps_pos);
	config.set("OVERLAY", "ShowClock", _show_clock);
#if RESHADE_FX
	config.set("OVERLAY", "ShowForceLoadEffectsButton", _show_force_load_effects_button);
#endif
	config.set("OVERLAY", "ShowFPS", _show_fps);
	config.set("OVERLAY", "ShowFrameTime", _show_frametime);
	config.set("OVERLAY", "ShowPresetName", _show_preset_name);
	config.set("OVERLAY", "ShowScreenshotMessage", _show_screenshot_message);
#if RESHADE_FX
	global_config().set("OVERLAY", "TutorialProgress", _tutorial_index);
	config.set("OVERLAY", "TutorialProgress", _tutorial_index);
	config.set("OVERLAY", "VariableListHeight", _variable_editor_height);
	config.set("OVERLAY", "VariableListUseTabs", _variable_editor_tabs);
	config.set("OVERLAY", "AutoSavePreset", _auto_save_preset);
	config.set("OVERLAY", "ShowPresetTransitionMessage", _show_preset_transition_message);
#endif

	const ImGuiStyle &imgui_style = _imgui_context->Style;
	config.set("STYLE", "Alpha", imgui_style.Alpha);
	config.set("STYLE", "ChildRounding", imgui_style.ChildRounding);
	config.set("STYLE", "ColFPSText", _fps_col);
	config.set("STYLE", "EditorFont", _editor_font_path);
	config.set("STYLE", "EditorFontSize", _editor_font_size);
	config.set("STYLE", "EditorStyleIndex", _editor_style_index);
	config.set("STYLE", "Font", _font_path);
	config.set("STYLE", "FontSize", _font_size);
	config.set("STYLE", "FPSScale", _fps_scale);
	config.set("STYLE", "FrameRounding", imgui_style.FrameRounding);
	config.set("STYLE", "GrabRounding", imgui_style.GrabRounding);
	config.set("STYLE", "LatinFont", _latin_font_path);
	config.set("STYLE", "PopupRounding", imgui_style.PopupRounding);
	config.set("STYLE", "ScrollbarRounding", imgui_style.ScrollbarRounding);
	config.set("STYLE", "StyleIndex", _style_index);
	config.set("STYLE", "TabRounding", imgui_style.TabRounding);
	config.set("STYLE", "WindowRounding", imgui_style.WindowRounding);
	config.set("STYLE", "HdrOverlayBrightness", _hdr_overlay_brightness);
	config.set("STYLE", "HdrOverlayOverwriteColorSpaceTo", static_cast<int>(_hdr_overlay_overwrite_color_space));

	// Do not save custom style colors by default, only when actually used and edited

	ImGuiContext *const backup_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(_imgui_context);

	for (ImGuiSettingsHandler &handler : _imgui_context->SettingsHandlers)
	{
		ImGuiTextBuffer buffer;
		handler.WriteAllFn(_imgui_context, &handler, &buffer);

		std::vector<std::string> lines;
		for (int i = 0, offset = 0; i < buffer.size(); ++i)
		{
			if (buffer[i] == '\n')
			{
				lines.emplace_back(buffer.c_str() + offset, i - offset);
				offset = i + 1;
			}
		}

		if (!lines.empty())
			config.set("OVERLAY", handler.TypeName, lines);
	}

	ImGui::SetCurrentContext(backup_context);
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

	bool show_overlay = _show_overlay;
	api::input_source show_overlay_source = api::input_source::keyboard;

	if (_input != nullptr)
	{
		if (_show_overlay && !_ignore_shortcuts && !_imgui_context->IO.NavVisible && _input->is_key_pressed(0x1B /* VK_ESCAPE */))
			show_overlay = false; // Close when pressing the escape button and not currently navigating with the keyboard
		else if (!_ignore_shortcuts && _input->is_key_pressed(_overlay_key_data, _force_shortcut_modifiers) && _imgui_context->ActiveId == 0)
			show_overlay = !_show_overlay;

		if (!_ignore_shortcuts)
		{
			if (_input->is_key_pressed(_fps_key_data, _force_shortcut_modifiers))
				_show_fps = _show_fps ? 0 : 1;
			if (_input->is_key_pressed(_frametime_key_data, _force_shortcut_modifiers))
				_show_frametime = _show_frametime ? 0 : 1;
		}
	}

	if (_input_gamepad != nullptr)
	{
		if (_input_gamepad->is_button_down(input_gamepad::button_left_shoulder) &&
			_input_gamepad->is_button_down(input_gamepad::button_right_shoulder) &&
			_input_gamepad->is_button_pressed(input_gamepad::button_start))
		{
			show_overlay = !_show_overlay;
			show_overlay_source = api::input_source::gamepad;
		}
	}

	if (show_overlay != _show_overlay)
		open_overlay(show_overlay, show_overlay_source);

#if RESHADE_FX
	const bool show_splash_window = _show_splash && (is_loading() || (_reload_count <= 1 && (_last_present_time - _last_reload_time) < std::chrono::seconds(5)) || (!_show_overlay && _tutorial_index == 0 && _input != nullptr));
#else
	const bool show_splash_window = _show_splash && (_last_present_time - _last_reload_time) < std::chrono::seconds(5);
#endif

	// Do not show this message in the same frame the screenshot is taken (so that it won't show up on the GUI screenshot)
	const bool show_screenshot_message = (_show_screenshot_message || !_last_screenshot_save_successful) && !_should_save_screenshot && (_last_present_time - _last_screenshot_time) < std::chrono::seconds(_last_screenshot_save_successful ? 3 : 5);
#if RESHADE_FX
	const bool show_preset_transition_message = _show_preset_transition_message && _is_in_preset_transition;
#else
	const bool show_preset_transition_message = false;
#endif
	const bool show_message_window = show_screenshot_message || show_preset_transition_message || !_preset_save_successful;

	const bool show_clock = _show_clock == 1 || (_show_overlay && _show_clock > 1);
	const bool show_fps = _show_fps == 1 || (_show_overlay && _show_fps > 1);
	const bool show_frametime = _show_frametime == 1 || (_show_overlay && _show_frametime > 1);
	const bool show_preset_name = _show_preset_name == 1 || (_show_overlay && _show_preset_name > 1);
	bool show_statistics_window = show_clock || show_fps || show_frametime || show_preset_name;
#if RESHADE_ADDON
	for (const addon_info &info : addon_loaded_info)
	{
		for (const addon_info::overlay_callback &widget : info.overlay_callbacks)
		{
			if (widget.title == "OSD")
			{
				show_statistics_window = true;
				break;
			}
		}
	}
#endif

	_ignore_shortcuts = false;
	_block_input_next_frame = false;
#if RESHADE_FX
	_gather_gpu_statistics = false;
	_effects_expanded_state &= 2;
#endif

	if (!show_splash_window && !show_message_window && !show_statistics_window && !_show_overlay
#if RESHADE_FX
		&& _preview_texture == 0
#endif
#if RESHADE_ADDON
		&& !has_addon_event<addon_event::reshade_overlay>()
#endif
		)
	{
		if (_input != nullptr)
		{
			_input->block_mouse_input(false);
			_input->block_keyboard_input(false);
		}
		return; // Early-out to avoid costly ImGui calls when no GUI elements are on the screen
	}

	build_font_atlas();
	if (_font_atlas_srv == 0)
		return; // Cannot render GUI without font atlas

	ImGuiContext *const backup_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(_imgui_context);

	ImGuiIO &imgui_io = _imgui_context->IO;
	imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
	imgui_io.DisplaySize.x = static_cast<float>(_width);
	imgui_io.DisplaySize.y = static_cast<float>(_height);
	imgui_io.Fonts->TexID = _font_atlas_srv.handle;

	if (_input != nullptr)
	{
		imgui_io.MouseDrawCursor = _show_overlay && (!_should_save_screenshot || !_screenshot_save_gui);

		// Scale mouse position in case render resolution does not match the window size
		unsigned int max_position[2];
		_input->max_mouse_position(max_position);
		imgui_io.AddMousePosEvent(
			_input->mouse_position_x() * (imgui_io.DisplaySize.x / max_position[0]),
			_input->mouse_position_y() * (imgui_io.DisplaySize.y / max_position[1]));

		// Add wheel delta to the current absolute mouse wheel position
		imgui_io.AddMouseWheelEvent(0.0f, _input->mouse_wheel_delta());

		// Update all the button states
		constexpr std::pair<ImGuiKey, unsigned int> key_mappings[] = {
			{ ImGuiKey_Tab, 0x09 /* VK_TAB */ },
			{ ImGuiKey_LeftArrow, 0x25 /* VK_LEFT */ },
			{ ImGuiKey_RightArrow, 0x27 /* VK_RIGHT */ },
			{ ImGuiKey_UpArrow, 0x26 /* VK_UP */ },
			{ ImGuiKey_DownArrow, 0x28 /* VK_DOWN */ },
			{ ImGuiKey_PageUp, 0x21 /* VK_PRIOR */ },
			{ ImGuiKey_PageDown, 0x22 /* VK_NEXT */ },
			{ ImGuiKey_End, 0x23 /* VK_END */ },
			{ ImGuiKey_Home, 0x24 /* VK_HOME */ },
			{ ImGuiKey_Insert, 0x2D /* VK_INSERT */ },
			{ ImGuiKey_Delete, 0x2E /* VK_DELETE */ },
			{ ImGuiKey_Backspace, 0x08 /* VK_BACK */ },
			{ ImGuiKey_Space, 0x20 /* VK_SPACE */ },
			{ ImGuiKey_Enter, 0x0D /* VK_RETURN */ },
			{ ImGuiKey_Escape, 0x1B /* VK_ESCAPE */ },
			{ ImGuiKey_LeftCtrl, 0xA2 /* VK_LCONTROL */ },
			{ ImGuiKey_LeftShift, 0xA0 /* VK_LSHIFT */ },
			{ ImGuiKey_LeftAlt, 0xA4 /* VK_LMENU */ },
			{ ImGuiKey_LeftSuper, 0x5B /* VK_LWIN */ },
			{ ImGuiKey_RightCtrl, 0xA3 /* VK_RCONTROL */ },
			{ ImGuiKey_RightShift, 0xA1 /* VK_RSHIFT */ },
			{ ImGuiKey_RightAlt, 0xA5 /* VK_RMENU */ },
			{ ImGuiKey_RightSuper, 0x5C /* VK_RWIN */ },
			{ ImGuiKey_Menu, 0x5D /* VK_APPS */ },
			{ ImGuiKey_0, '0' },
			{ ImGuiKey_1, '1' },
			{ ImGuiKey_2, '2' },
			{ ImGuiKey_3, '3' },
			{ ImGuiKey_4, '4' },
			{ ImGuiKey_5, '5' },
			{ ImGuiKey_6, '6' },
			{ ImGuiKey_7, '7' },
			{ ImGuiKey_8, '8' },
			{ ImGuiKey_9, '9' },
			{ ImGuiKey_A, 'A' },
			{ ImGuiKey_B, 'B' },
			{ ImGuiKey_C, 'C' },
			{ ImGuiKey_D, 'D' },
			{ ImGuiKey_E, 'E' },
			{ ImGuiKey_F, 'F' },
			{ ImGuiKey_G, 'G' },
			{ ImGuiKey_H, 'H' },
			{ ImGuiKey_I, 'I' },
			{ ImGuiKey_J, 'J' },
			{ ImGuiKey_K, 'K' },
			{ ImGuiKey_L, 'L' },
			{ ImGuiKey_M, 'M' },
			{ ImGuiKey_N, 'N' },
			{ ImGuiKey_O, 'O' },
			{ ImGuiKey_P, 'P' },
			{ ImGuiKey_Q, 'Q' },
			{ ImGuiKey_R, 'R' },
			{ ImGuiKey_S, 'S' },
			{ ImGuiKey_T, 'T' },
			{ ImGuiKey_U, 'U' },
			{ ImGuiKey_V, 'V' },
			{ ImGuiKey_W, 'W' },
			{ ImGuiKey_X, 'X' },
			{ ImGuiKey_Y, 'Y' },
			{ ImGuiKey_Z, 'Z' },
			{ ImGuiKey_F1, 0x70 /* VK_F1 */ },
			{ ImGuiKey_F2, 0x71 /* VK_F2 */ },
			{ ImGuiKey_F3, 0x72 /* VK_F3 */ },
			{ ImGuiKey_F4, 0x73 /* VK_F4 */ },
			{ ImGuiKey_F5, 0x74 /* VK_F5 */ },
			{ ImGuiKey_F6, 0x75 /* VK_F6 */ },
			{ ImGuiKey_F7, 0x76 /* VK_F7 */ },
			{ ImGuiKey_F8, 0x77 /* VK_F8 */ },
			{ ImGuiKey_F9, 0x78 /* VK_F9 */ },
			{ ImGuiKey_F10, 0x79 /* VK_F10 */ },
			{ ImGuiKey_F11, 0x80 /* VK_F11 */ },
			{ ImGuiKey_F12, 0x81 /* VK_F12 */ },
			{ ImGuiKey_Apostrophe, 0xDE /* VK_OEM_7 */ },
			{ ImGuiKey_Comma, 0xBC /* VK_OEM_COMMA */ },
			{ ImGuiKey_Minus, 0xBD /* VK_OEM_MINUS */ },
			{ ImGuiKey_Period, 0xBE /* VK_OEM_PERIOD */ },
			{ ImGuiKey_Slash, 0xBF /* VK_OEM_2 */ },
			{ ImGuiKey_Semicolon, 0xBA /* VK_OEM_1 */ },
			{ ImGuiKey_Equal, 0xBB /* VK_OEM_PLUS */ },
			{ ImGuiKey_LeftBracket, 0xDB /* VK_OEM_4 */ },
			{ ImGuiKey_Backslash, 0xDC /* VK_OEM_5 */ },
			{ ImGuiKey_RightBracket, 0xDD /* VK_OEM_6 */ },
			{ ImGuiKey_GraveAccent, 0xC0 /* VK_OEM_3 */ },
			{ ImGuiKey_CapsLock, 0x14 /* VK_CAPITAL */ },
			{ ImGuiKey_ScrollLock, 0x91 /* VK_SCROLL */ },
			{ ImGuiKey_NumLock, 0x90 /* VK_NUMLOCK */ },
			{ ImGuiKey_PrintScreen, 0x2C /* VK_SNAPSHOT */ },
			{ ImGuiKey_Pause, 0x13 /* VK_PAUSE */ },
			{ ImGuiKey_Keypad0, 0x60 /* VK_NUMPAD0 */ },
			{ ImGuiKey_Keypad1, 0x61 /* VK_NUMPAD1 */ },
			{ ImGuiKey_Keypad2, 0x62 /* VK_NUMPAD2 */ },
			{ ImGuiKey_Keypad3, 0x63 /* VK_NUMPAD3 */ },
			{ ImGuiKey_Keypad4, 0x64 /* VK_NUMPAD4 */ },
			{ ImGuiKey_Keypad5, 0x65 /* VK_NUMPAD5 */ },
			{ ImGuiKey_Keypad6, 0x66 /* VK_NUMPAD6 */ },
			{ ImGuiKey_Keypad7, 0x67 /* VK_NUMPAD7 */ },
			{ ImGuiKey_Keypad8, 0x68 /* VK_NUMPAD8 */ },
			{ ImGuiKey_Keypad9, 0x69 /* VK_NUMPAD9 */ },
			{ ImGuiKey_KeypadDecimal, 0x6E /* VK_DECIMAL */ },
			{ ImGuiKey_KeypadDivide, 0x6F /* VK_DIVIDE */ },
			{ ImGuiKey_KeypadMultiply, 0x6A /* VK_MULTIPLY */ },
			{ ImGuiKey_KeypadSubtract, 0x6D /* VK_SUBTRACT */ },
			{ ImGuiKey_KeypadAdd, 0x6B /* VK_ADD */ },
			{ ImGuiMod_Ctrl, 0x11 /* VK_CONTROL */ },
			{ ImGuiMod_Shift, 0x10 /* VK_SHIFT */ },
			{ ImGuiMod_Alt, 0x12 /* VK_MENU */ },
			{ ImGuiMod_Super, 0x5D /* VK_APPS */ },
		};

		for (const std::pair<ImGuiKey, unsigned int> &mapping : key_mappings)
			imgui_io.AddKeyEvent(mapping.first, _input->is_key_down(mapping.second));
		for (ImGuiMouseButton i = 0; i < ImGuiMouseButton_COUNT; i++)
			imgui_io.AddMouseButtonEvent(i, _input->is_mouse_button_down(i));
		for (ImWchar16 c : _input->text_input())
			imgui_io.AddInputCharacterUTF16(c);
	}

	if (_input_gamepad != nullptr)
	{
		if (_input_gamepad->is_connected())
		{
			imgui_io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

			constexpr std::pair<ImGuiKey, input_gamepad::button> button_mappings[] = {
				{ ImGuiKey_GamepadStart, input_gamepad::button_start },
				{ ImGuiKey_GamepadBack, input_gamepad::button_back },
				{ ImGuiKey_GamepadFaceLeft, input_gamepad::button_x },
				{ ImGuiKey_GamepadFaceRight, input_gamepad::button_b },
				{ ImGuiKey_GamepadFaceUp, input_gamepad::button_y },
				{ ImGuiKey_GamepadFaceDown, input_gamepad::button_a },
				{ ImGuiKey_GamepadDpadLeft, input_gamepad::button_dpad_left },
				{ ImGuiKey_GamepadDpadRight, input_gamepad::button_dpad_right },
				{ ImGuiKey_GamepadDpadUp, input_gamepad::button_dpad_up },
				{ ImGuiKey_GamepadDpadDown, input_gamepad::button_dpad_down },
				{ ImGuiKey_GamepadL1, input_gamepad::button_left_shoulder },
				{ ImGuiKey_GamepadR1, input_gamepad::button_right_shoulder },
				{ ImGuiKey_GamepadL3, input_gamepad::button_left_thumb },
				{ ImGuiKey_GamepadR3, input_gamepad::button_right_thumb },
			};

			for (const std::pair<ImGuiKey, input_gamepad::button> &mapping : button_mappings)
				imgui_io.AddKeyEvent(mapping.first, _input_gamepad->is_button_down(mapping.second));

			imgui_io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, _input_gamepad->left_trigger_position() != 0.0f, _input_gamepad->left_trigger_position());
			imgui_io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, _input_gamepad->right_trigger_position() != 0.0f, _input_gamepad->right_trigger_position());
			imgui_io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, _input_gamepad->left_thumb_axis_x() < 0.0f, -std::min(_input_gamepad->left_thumb_axis_x(), 0.0f));
			imgui_io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, _input_gamepad->left_thumb_axis_x() > 0.0f, std::max(_input_gamepad->left_thumb_axis_x(), 0.0f));
			imgui_io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, _input_gamepad->left_thumb_axis_y() > 0.0f, std::max(_input_gamepad->left_thumb_axis_y(), 0.0f));
			imgui_io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, _input_gamepad->left_thumb_axis_y() < 0.0f, -std::min(_input_gamepad->left_thumb_axis_y(), 0.0f));
		}
		else
		{
			imgui_io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
		}
	}

	ImGui::NewFrame();

#if RESHADE_LOCALIZATION
	const std::string prev_language = resources::set_current_language(_selected_language);
	_current_language = resources::get_current_language();
#endif

	ImVec2 viewport_offset = ImVec2(0, 0);

	// Create ImGui widgets and windows
	if (show_splash_window)
	{
		ImGui::SetNextWindowPos(_imgui_context->Style.WindowPadding);
		ImGui::SetNextWindowSize(ImVec2(imgui_io.DisplaySize.x - 20.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.862745f, 0.862745f, 0.862745f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.117647f, 0.117647f, 0.117647f, 0.7f));
		ImGui::Begin("Splash Window", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoFocusOnAppearing);

		ImGui::TextUnformatted("ReShade " VERSION_STRING_PRODUCT);

		if ((s_latest_version[0] > VERSION_MAJOR) ||
			(s_latest_version[0] == VERSION_MAJOR && s_latest_version[1] > VERSION_MINOR) ||
			(s_latest_version[0] == VERSION_MAJOR && s_latest_version[1] == VERSION_MINOR && s_latest_version[2] > VERSION_REVISION))
		{
			ImGui::TextColored(COLOR_YELLOW, _(
				"An update is available! Please visit %s and install the new version (v%u.%u.%u)."),
				"https://reshade.me",
				s_latest_version[0], s_latest_version[1], s_latest_version[2]);
		}
		else
		{
			ImGui::Text(_("Visit %s for news, updates, effects and discussion."), "https://reshade.me");
		}

		ImGui::Spacing();

#if RESHADE_FX
		if (_reload_remaining_effects != 0 && _reload_remaining_effects != std::numeric_limits<size_t>::max())
		{
			ImGui::ProgressBar((_effects.size() - _reload_remaining_effects) / float(_effects.size()), ImVec2(-1, 0), "");
			ImGui::SameLine(15);
			ImGui::Text(_(
				"Compiling (%zu effects remaining) ... "
				"This might take a while. The application could become unresponsive for some time."),
				_reload_remaining_effects.load());
		}
		else
#endif
		{
			ImGui::ProgressBar(0.0f, ImVec2(-1, 0), "");
			ImGui::SameLine(15);

			if (_input == nullptr)
			{
				ImGui::TextColored(COLOR_YELLOW, _("No keyboard or mouse input available."));
				if (_input_gamepad != nullptr)
				{
					ImGui::SameLine();
					ImGui::TextColored(COLOR_YELLOW, _("Use gamepad instead: Press 'left + right shoulder + start button' to open the configuration overlay."));
				}
			}
#if RESHADE_FX
			else if (_tutorial_index == 0)
			{
				const std::string label = _("ReShade is now installed successfully! Press '%s' to start the tutorial.");
				const size_t key_offset = label.find("%s");

				ImGui::TextUnformatted(label.c_str(), label.c_str() + key_offset);
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", input::key_name(_overlay_key_data).c_str());
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextUnformatted(label.c_str() + key_offset + 2, label.c_str() + label.size());
			}
#endif
			else
			{
				const std::string label = _("Press '%s' to open the configuration overlay.");
				const size_t key_offset = label.find("%s");

				ImGui::TextUnformatted(label.c_str(), label.c_str() + key_offset);
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", input::key_name(_overlay_key_data).c_str());
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::TextUnformatted(label.c_str() + key_offset + 2, label.c_str() + label.size());
			}
		}

		std::string error_message;
#if RESHADE_ADDON
		if (!addon_all_loaded)
			error_message += _("There were errors loading some add-ons."),
			error_message += ' ';
#endif
#if RESHADE_FX
		if (!_last_reload_successful)
			error_message += _("There were errors loading some effects."),
			error_message += ' ';
#endif
		if (!error_message.empty())
		{
			error_message += _("Check the log for more details.");
			ImGui::Spacing();
			ImGui::TextColored(COLOR_RED, error_message.c_str());
		}

		viewport_offset.y += ImGui::GetWindowHeight() + _imgui_context->Style.WindowPadding.x; // Add small space between windows

		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}

	if (show_message_window)
	{
		ImGui::SetNextWindowPos(_imgui_context->Style.WindowPadding + viewport_offset);
		ImGui::SetNextWindowSize(ImVec2(imgui_io.DisplaySize.x - 20.0f, 0.0f));
		ImGui::Begin("Message Window", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoFocusOnAppearing);

		if (!_preset_save_successful)
		{
#if RESHADE_FX
			ImGui::TextColored(COLOR_RED, _("Unable to save configuration and/or current preset. Make sure file permissions are set up to allow writing to these paths and their parent directories:\n%s\n%s"), _config_path.u8string().c_str(), _current_preset_path.u8string().c_str());
#else
			ImGui::TextColored(COLOR_RED, _("Unable to save configuration. Make sure file permissions are set up to allow writing to %s."), _config_path.u8string().c_str());
#endif
		}
		else if (show_screenshot_message)
		{
			if (!_last_screenshot_save_successful)
				if (_screenshot_directory_creation_successful)
					ImGui::TextColored(COLOR_RED, _("Unable to save screenshot because of an internal error (the format may not be supported or the drive may be full)."));
				else
					ImGui::TextColored(COLOR_RED, _("Unable to save screenshot because path could not be created: %s"), (g_reshade_base_path / _screenshot_path).u8string().c_str());
			else
				ImGui::Text(_("Screenshot successfully saved to %s"), _last_screenshot_file.u8string().c_str());
		}
#if RESHADE_FX
		else if (show_preset_transition_message)
		{
			ImGui::Text(_("Switching preset to %s ..."), _current_preset_path.stem().u8string().c_str());
		}
#endif

		viewport_offset.y += ImGui::GetWindowHeight() + _imgui_context->Style.WindowPadding.x; // Add small space between windows

		ImGui::End();
	}

	if (show_statistics_window && !show_splash_window && !show_message_window)
	{
		ImVec2 fps_window_pos(5, 5);
		ImVec2 fps_window_size(200, 0);

		// Get last calculated window size (because of 'ImGuiWindowFlags_AlwaysAutoResize')
		if (ImGuiWindow *const fps_window = ImGui::FindWindowByName("OSD"))
		{
			fps_window_size  = fps_window->Size;
			fps_window_size.y = std::max(fps_window_size.y, _imgui_context->Style.FramePadding.y * 4.0f + _imgui_context->Style.ItemSpacing.y +
				(_imgui_context->Style.ItemSpacing.y + _imgui_context->FontBaseSize * _fps_scale) * ((show_clock ? 1 : 0) + (show_fps ? 1 : 0) + (show_frametime ? 1 : 0) + (show_preset_name ? 1 : 0)));
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

		const float content_width = ImGui::GetContentRegionAvail().x;
		char temp[512];

		if (show_clock)
		{
			const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			struct tm tm; localtime_s(&tm, &t);

			const int temp_size = ImFormatString(temp, sizeof(temp), _clock_format != 0 ? "%02u:%02u:%02u" : "%02u:%02u", tm.tm_hour, tm.tm_min, tm.tm_sec);
			if (_fps_pos % 2) // Align text to the right of the window
				ImGui::SetCursorPosX(content_width - ImGui::CalcTextSize(temp, temp + temp_size).x + _imgui_context->Style.ItemSpacing.x);
			ImGui::TextUnformatted(temp, temp + temp_size);
		}
		if (show_fps)
		{
			const int temp_size = ImFormatString(temp, sizeof(temp), "%.0f fps", imgui_io.Framerate);
			if (_fps_pos % 2)
				ImGui::SetCursorPosX(content_width - ImGui::CalcTextSize(temp, temp + temp_size).x + _imgui_context->Style.ItemSpacing.x);
			ImGui::TextUnformatted(temp, temp + temp_size);
		}
		if (show_frametime)
		{
			const int temp_size = ImFormatString(temp, sizeof(temp), "%5.2f ms", 1000.0f / imgui_io.Framerate);
			if (_fps_pos % 2)
				ImGui::SetCursorPosX(content_width - ImGui::CalcTextSize(temp, temp + temp_size).x + _imgui_context->Style.ItemSpacing.x);
			ImGui::TextUnformatted(temp, temp + temp_size);
		}
		if (show_preset_name)
		{
			const std::string preset_name = _current_preset_path.stem().u8string();
			if (_fps_pos % 2)
				ImGui::SetCursorPosX(content_width - ImGui::CalcTextSize(preset_name.c_str(), preset_name.c_str() + preset_name.size()).x + _imgui_context->Style.ItemSpacing.x);
			ImGui::TextUnformatted(preset_name.c_str(), preset_name.c_str() + preset_name.size());
		}

		ImGui::Dummy(ImVec2(200, 0)); // Force a minimum window width

		ImGui::End();
		ImGui::PopStyleColor();
	}

	if (_show_overlay)
	{
		const ImGuiViewport *const viewport = ImGui::GetMainViewport();

		// Change font size if user presses the control key and moves the mouse wheel
		if (!_no_font_scaling && imgui_io.KeyCtrl && imgui_io.MouseWheel != 0 && ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
		{
			_font_size = ImClamp(_font_size + static_cast<int>(imgui_io.MouseWheel), 8, 64);
			_editor_font_size = ImClamp(_editor_font_size + static_cast<int>(imgui_io.MouseWheel), 8, 64);
			imgui_io.Fonts->TexReady = false;
			save_config();

			_is_font_scaling = true;
		}

		if (_is_font_scaling)
		{
			if (!imgui_io.KeyCtrl)
				_is_font_scaling = false;

			ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, _imgui_context->Style.WindowPadding * 2.0f);
			ImGui::Begin("FontScaling", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
			ImGui::Text(_("Scaling font size (%d) with 'Ctrl' + mouse wheel"), _font_size);
			ImGui::End();
			ImGui::PopStyleVar();
		}

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

		const ImGuiID root_space_id = ImGui::GetID("ViewportDockspace");

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
			for (const std::pair<std::string, void(runtime::*)()> &widget : overlay_callbacks)
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
		ImGui::DockSpace(root_space_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::End();

		if (_imgui_context->NavInputSource > ImGuiInputSource_Mouse && _imgui_context->NavWindowingTarget == nullptr)
		{
			// Reset input source to mouse when the cursor is moved
			if (_input != nullptr && (_input->mouse_movement_delta_x() != 0 || _input->mouse_movement_delta_y() != 0))
				_imgui_context->NavInputSource = ImGuiInputSource_Mouse;
			// Ensure there is always a window that has navigation focus when keyboard or gamepad navigation is used (choose the first overlay window created next)
			else if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
				ImGui::SetNextWindowFocus();
		}

		for (const std::pair<std::string, void(runtime:: *)()> &widget : overlay_callbacks)
		{
			if (ImGui::Begin(widget.first.c_str(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) // No focus so that window state is preserved between opening/closing the GUI
				(this->*widget.second)();
			ImGui::End();
		}

#if RESHADE_FX
		if (!_editors.empty())
		{
			if (ImGui::Begin(_("Edit###editor"), nullptr, ImGuiWindowFlags_NoFocusOnAppearing) &&
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

#if RESHADE_ADDON == 1
	if (addon_enabled)
#endif
#if RESHADE_ADDON
	{
		for (const addon_info &info : addon_loaded_info)
		{
			for (const addon_info::overlay_callback &widget : info.overlay_callbacks)
			{
				if (widget.title == "OSD" ? show_splash_window : !_show_overlay)
					continue;

				if (ImGui::Begin(widget.title.c_str(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
					widget.callback(this);
				ImGui::End();
			}
		}

		invoke_addon_event<addon_event::reshade_overlay>(this);
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

#if RESHADE_LOCALIZATION
	resources::set_current_language(prev_language);
#endif

	// Disable keyboard shortcuts while typing into input boxes
	_ignore_shortcuts |= ImGui::IsAnyItemActive();

	// Render ImGui widgets and windows
	ImGui::Render();

	if (_input != nullptr)
	{
		const bool block_input = _input_processing_mode != 0 && (_show_overlay || _block_input_next_frame);

		_input->block_mouse_input(block_input && (imgui_io.WantCaptureMouse || _input_processing_mode == 2));
		_input->block_keyboard_input(block_input && (imgui_io.WantCaptureKeyboard || _input_processing_mode == 2));
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
			uint32_t back_buffer_index = get_current_back_buffer_index() * 2;
			const api::resource back_buffer_resource = _device->get_resource_from_view(_back_buffer_targets[back_buffer_index]);

			cmd_list->barrier(back_buffer_resource, api::resource_usage::present, api::resource_usage::render_target);
			render_imgui_draw_data(cmd_list, draw_data, _back_buffer_targets[back_buffer_index]);
			cmd_list->barrier(back_buffer_resource, api::resource_usage::render_target, api::resource_usage::present);
		}
	}

	ImGui::SetCurrentContext(backup_context);
}

#if RESHADE_FX
void reshade::runtime::draw_gui_home()
{
	std::string tutorial_text;

	// It is not possible to follow some of the tutorial steps while performance mode is active, so skip them
	if (_performance_mode && _tutorial_index <= 3)
		_tutorial_index = 4;

	const float auto_save_button_spacing = 2.0f;
	const float button_width = 12.5f * _font_size;

	if (_tutorial_index > 0)
	{
		if (_tutorial_index == 1)
		{
			tutorial_text = _(
				"This is the preset selection. All changes will be saved to the selected preset file.\n\n"
				"Click on the '+' button to add a new one.\n"
				"Use the right mouse button and click on the preset button to open a context menu with additional options.");

			ImGui::PushStyleColor(ImGuiCol_FrameBg, COLOR_RED);
			ImGui::PushStyleColor(ImGuiCol_Button, COLOR_RED);
		}

		const float button_height = ImGui::GetFrameHeight();
		const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;

		bool reload_preset = false;

		// Loading state may change below, so keep track of current state so that 'ImGui::Push/Pop*' is executed the correct amount of times
		const bool was_loading = is_loading();
		if (was_loading)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		}

		if (ImGui::ArrowButtonEx("<", ImGuiDir_Left, ImVec2(button_height, button_height), ImGuiButtonFlags_NoNavFocus))
			if (switch_to_next_preset(_current_preset_path.parent_path(), true))
				reload_preset = true;
		ImGui::SetItemTooltip(_("Previous preset"));

		ImGui::SameLine(0, button_spacing);

		if (ImGui::ArrowButtonEx(">", ImGuiDir_Right, ImVec2(button_height, button_height), ImGuiButtonFlags_NoNavFocus))
			if (switch_to_next_preset(_current_preset_path.parent_path(), false))
				reload_preset = true;
		ImGui::SetItemTooltip(_("Next preset"));

		ImGui::SameLine();

		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));

		const auto browse_button_pos = ImGui::GetCursorScreenPos();
		const auto browse_button_width = ImGui::GetContentRegionAvail().x - (button_height + button_spacing + _imgui_context->Style.ItemSpacing.x + auto_save_button_spacing + button_width);

		if (ImGui::ButtonEx((_current_preset_path.stem().u8string() + "###browse_button").c_str(), ImVec2(browse_button_width, 0), ImGuiButtonFlags_NoNavFocus))
		{
			_file_selection_path = _current_preset_path;
			ImGui::OpenPopup("##browse");
		}

		if (_preset_is_modified)
			ImGui::RenderBullet(ImGui::GetWindowDrawList(), browse_button_pos + ImVec2(browse_button_width - _font_size * 0.5f - _imgui_context->Style.FramePadding.x, button_height * 0.5f), ImGui::GetColorU32(ImGuiCol_Text));

		ImGui::PopStyleVar();

		if (_input != nullptr &&
			ImGui::BeginPopupContextItem())
		{
			auto preset_shortcut_it = std::find_if(_preset_shortcuts.begin(), _preset_shortcuts.end(),
				[this](const preset_shortcut &shortcut) { return shortcut.preset_path == _current_preset_path; });

			preset_shortcut shortcut;
			if (preset_shortcut_it != _preset_shortcuts.end())
				shortcut = *preset_shortcut_it;
			else
				shortcut.preset_path = _current_preset_path;

			ImGui::SetNextItemWidth(18.0f * _font_size);
			if (imgui::key_input_box("##toggle_key", shortcut.key_data, *_input))
			{
				if (preset_shortcut_it != _preset_shortcuts.end())
					*preset_shortcut_it = std::move(shortcut);
				else
					_preset_shortcuts.push_back(std::move(shortcut));
			}

			ImGui::EndPopup();
		}

		ImGui::SameLine(0, button_spacing);

		if (ImGui::Button(ICON_FK_FOLDER, ImVec2(button_height, button_height)))
			utils::open_explorer(_current_preset_path);
		ImGui::SetItemTooltip(_("Open folder in explorer"));

		ImGui::SameLine();

		const bool was_auto_save_preset = _auto_save_preset;

		if (imgui::toggle_button(
				(std::string(was_auto_save_preset ? _("Auto Save on") : _("Auto Save")) + "###auto_save").c_str(),
				_auto_save_preset,
				(was_auto_save_preset ? 0.0f : auto_save_button_spacing) + button_width - (button_spacing + button_height) * (was_auto_save_preset ? 2 : 3)))
		{
			_preset_is_modified = false;

			if (!was_auto_save_preset)
				save_current_preset();
			save_config();
		}

		ImGui::SetItemTooltip(_("Save current preset automatically on every modification"));

		if (was_auto_save_preset)
		{
			ImGui::SameLine(0, button_spacing + auto_save_button_spacing);
		}
		else
		{
			ImGui::SameLine(0, button_spacing);

			ImGui::BeginDisabled(!_preset_is_modified);

			if (imgui::confirm_button(ICON_FK_UNDO, button_height, _("Do you really want to reset all techniques and values?")))
				reload_preset = true;

			ImGui::SetItemTooltip(_("Reset all techniques and values to those of the current preset"));

			ImGui::EndDisabled();

			ImGui::SameLine(0, button_spacing);
		}

		// Cannot save in performance mode, since there are no variables to retrieve values from then
		ImGui::BeginDisabled(_performance_mode || _is_in_preset_transition);

		const auto save_and_clean_preset = _auto_save_preset || (_imgui_context->IO.KeyCtrl || _imgui_context->IO.KeyShift);

		if (ImGui::ButtonEx(ICON_FK_FLOPPY, ImVec2(button_height, button_height), ImGuiButtonFlags_NoNavFocus))
		{
			if (save_and_clean_preset)
				ini_file::load_cache(_current_preset_path).clear();
			save_current_preset();
			ini_file::flush_cache(_current_preset_path);

			_preset_is_modified = false;
		}

		ImGui::SetItemTooltip(save_and_clean_preset ?
			_("Clean up and save the current preset (removes all values for disabled techniques)") : _("Save the current preset"));

		ImGui::EndDisabled();

		ImGui::SameLine(0, button_spacing);
		if (ImGui::ButtonEx(ICON_FK_PLUS, ImVec2(button_height, button_height), ImGuiButtonFlags_NoNavFocus | ImGuiButtonFlags_PressedOnClick))
		{
			_inherit_current_preset = false;
			_template_preset_path.clear();
			_file_selection_path = _current_preset_path.parent_path();
			ImGui::OpenPopup("##create");
		}

		ImGui::SetItemTooltip(_("Add a new preset"));

		if (was_loading)
		{
			ImGui::PopStyleColor();
			ImGui::PopItemFlag();
		}

		ImGui::SetNextWindowPos(browse_button_pos + ImVec2(-_imgui_context->Style.WindowPadding.x, ImGui::GetFrameHeightWithSpacing()));
		if (imgui::file_dialog("##browse", _file_selection_path, std::max(browse_button_width, 450.0f), { L".ini", L".txt" }, { _config_path, global_config().path() }))
		{
			// Check that this is actually a valid preset file
			if (ini_file::load_cache(_file_selection_path).has({}, "Techniques"))
			{
				reload_preset = true;
				_current_preset_path = _file_selection_path;
			}
			else
			{
				ini_file::clear_cache(_file_selection_path);
				ImGui::OpenPopup("##preseterror");
			}
		}

		if (ImGui::BeginPopup("##create"))
		{
			ImGui::Checkbox(_("Inherit current preset"), &_inherit_current_preset);

			if (!_inherit_current_preset)
				imgui::file_input_box(_("Template"), nullptr, _template_preset_path, _file_selection_path, { L".ini", L".txt" });

			if (ImGui::IsWindowAppearing())
				ImGui::SetKeyboardFocusHere();

			char preset_name[260] = "";
			if (ImGui::InputText(_("Preset name"), preset_name, sizeof(preset_name), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCharFilter, filter_name) && preset_name[0] != '\0')
			{
				std::filesystem::path new_preset_path = _current_preset_path.parent_path() / std::filesystem::u8path(preset_name);
				if (new_preset_path.extension() != L".ini" && new_preset_path.extension() != L".txt")
					new_preset_path += L".ini";

				std::error_code ec;
				if (const std::filesystem::file_type file_type = std::filesystem::status(new_preset_path, ec).type();
					file_type != std::filesystem::file_type::directory)
				{
					reload_preset =
						file_type == std::filesystem::file_type::not_found ||
						ini_file::load_cache(new_preset_path).has({}, "Techniques");

					if (file_type == std::filesystem::file_type::not_found)
					{
						if (_inherit_current_preset)
						{
							_current_preset_path = new_preset_path;
							save_current_preset();
						}
						else if (!_template_preset_path.empty() && !std::filesystem::copy_file(_template_preset_path, new_preset_path, std::filesystem::copy_options::overwrite_existing, ec))
						{
							LOG(ERROR) << "Failed to copy preset template " << _template_preset_path << " to " << new_preset_path << " with error code " << ec.value() << '!';
						}
					}
				}

				if (reload_preset)
				{
					ImGui::CloseCurrentPopup();
					_current_preset_path = new_preset_path;
				}
				else
				{
					ImGui::SetKeyboardFocusHere(-1);
				}
			}

			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("##preseterror"))
		{
			ImGui::TextColored(COLOR_RED, _("The selected file is not a valid preset!"));
			ImGui::EndPopup();
		}

		if (reload_preset)
		{
			save_config();
			load_current_preset();

			_show_splash = true;
			_preset_is_modified = false;
			_last_preset_switching_time = _last_present_time;
			_is_in_preset_transition = true;

#if RESHADE_ADDON
			if (!is_loading()) // Will be called by 'update_effects' when 'load_current_preset' forced a reload
				invoke_addon_event<addon_event::reshade_set_current_preset_path>(this, _current_preset_path.u8string().c_str());
#endif
		}

		if (_tutorial_index == 1)
			ImGui::PopStyleColor(2);
	}
	else
	{
		tutorial_text = _(
			"Welcome! Since this is the first time you start ReShade, we'll go through a quick tutorial covering the most important features.\n\n"
			"If you have difficulties reading this text, press the 'Ctrl' key and adjust the font size with your mouse wheel. "
			"The window size is variable as well, just grab the right edge and move it around.\n\n"
			"You can also use the keyboard for navigation in case mouse input does not work. Use the arrow keys to navigate, space bar to confirm an action or enter a control and the 'Esc' key to leave a control. "
			"Press 'Ctrl + Tab' to switch between tabs and windows (use this to focus this page in case the other navigation keys do not work at first).\n\n"
			"Click on the 'Continue' button to continue the tutorial.");
	}

	if (_tutorial_index > 1)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}

	if (_reload_remaining_effects != std::numeric_limits<size_t>::max())
	{
		std::string loading_message = ICON_FK_REFRESH " ";
		loading_message += _("Loading ...");
		loading_message += " ";
		ImGui::SetCursorPos((ImGui::GetWindowSize() - ImGui::CalcTextSize(loading_message.c_str())) * 0.5f);
		ImGui::TextUnformatted(loading_message.c_str(), loading_message.c_str() + loading_message.size());
		return; // Cannot show techniques and variables while effects are loading, since they are being modified in other threads during that time
	}

	if (_tutorial_index > 1)
	{
		if (imgui::search_input_box(_effect_filter, sizeof(_effect_filter), -((_variable_editor_tabs ? 1 : 2) * (_imgui_context->Style.ItemSpacing.x + 2.0f + button_width))))
		{
			_effects_expanded_state = 3;

			for (technique &tech : _techniques)
			{
				std::string_view label = tech.annotation_as_string("ui_label");
				if (label.empty())
					label = tech.name;

				tech.hidden = tech.annotation_as_int("hidden") != 0 || !(filter_text(label, _effect_filter) || filter_text(_effects[tech.effect_index].source_file.filename().u8string(), _effect_filter));
			}
		}

		ImGui::SameLine();

		ImGui::BeginDisabled(_is_in_preset_transition);

		if (ImGui::Button(_("Active to top"), ImVec2(auto_save_button_spacing + button_width, 0)))
		{
			std::vector<size_t> technique_indices = _technique_sorting;

			for (auto it = technique_indices.begin(), target_it = it; it != technique_indices.end(); ++it)
			{
				const technique &tech = _techniques[*it];

				if (tech.enabled || tech.toggle_key_data[0] != 0)
				{
					target_it = std::rotate(target_it, it, std::next(it));
				}
			}

			reorder_techniques(std::move(technique_indices));

			if (_auto_save_preset)
				save_current_preset();
			else
				_preset_is_modified = true;
		}

		ImGui::EndDisabled();

		if (!_variable_editor_tabs)
		{
			ImGui::SameLine();

			if (ImGui::Button((_effects_expanded_state & 2) ? _("Collapse all") : _("Expand all"), ImVec2(auto_save_button_spacing + button_width, 0)))
				_effects_expanded_state = (~_effects_expanded_state & 2) | 1;
		}

		if (_tutorial_index == 2)
		{
			tutorial_text = _(
				"This is the list of effects. It contains all techniques exposed by effect files (.fx) found in the effect search paths specified in the settings.\n\n"
				"Enter text in the \"Search\" box at the top to filter it and search for specific techniques.\n\n"
				"Click on a technique to enable or disable it or drag it to a new location in the list to change the order in which the effects are applied (from top to bottom).\n"
				"Use the right mouse button and click on an item to open a context menu with additional options.");

			ImGui::PushStyleColor(ImGuiCol_Border, COLOR_RED);
		}

		ImGui::Spacing();

		if (!_last_reload_successful)
		{
			ImGui::PushTextWrapPos();
			ImGui::TextColored(COLOR_RED, _("There were errors loading some effects."));
			ImGui::TextColored(COLOR_RED, _("Hover the cursor over each red entry below to see the error messages or check the log for more details."));
			ImGui::PopTextWrapPos();
			ImGui::Spacing();
		}

		if (!_effects_enabled)
		{
			ImGui::Text(_("Effects are disabled. Press '%s' to enable them again."), input::key_name(_effects_key_data).c_str());
			ImGui::Spacing();
		}

		float bottom_height = _variable_editor_height;
		bottom_height = std::max(bottom_height, 20.0f);
		bottom_height = ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y + (
			_performance_mode ? 0 : (17 /* splitter */ + (bottom_height + (_tutorial_index == 3 ? 175 : 0))));
		bottom_height = std::min(bottom_height, ImGui::GetContentRegionAvail().y - 20.0f);

		if (ImGui::BeginChild("##techniques", ImVec2(0, -bottom_height), ImGuiChildFlags_Border))
		{
			if (_effect_load_skipping && _show_force_load_effects_button)
			{
				const size_t skipped_effects = std::count_if(_effects.cbegin(), _effects.cend(),
					[](const effect &effect) { return effect.skipped; });

				if (skipped_effects > 0)
				{
					char temp[64];
					const int temp_size = ImFormatString(temp, sizeof(temp), _("Force load all effects (%zu remaining)"), skipped_effects);

					if (ImGui::ButtonEx((std::string(temp, temp_size) + "###force_reload_button").c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)))
					{
						reload_effects(true);

						ImGui::EndChild();
						return;
					}
				}
			}

			ImGui::BeginDisabled(_is_in_preset_transition);
			draw_technique_editor();
			ImGui::EndDisabled();
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
			ImVec2 move_delta = _imgui_context->IO.MouseDelta;
			move_delta += ImGui::GetKeyMagnitude2d(ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, ImGuiKey_GamepadLStickUp, ImGuiKey_GamepadLStickDown) * _imgui_context->IO.DeltaTime * 500.0f;

			_variable_editor_height = std::max(_variable_editor_height - move_delta.y, 0.0f);
			save_config();
		}

		if (_tutorial_index == 3)
		{
			tutorial_text = _(
				"This is the list of variables. It contains all tweakable options the active effects expose. Values here apply in real-time.\n\n"
				"Press 'Ctrl' and click on a widget to manually edit the value (can also hold 'Ctrl' while adjusting the value in a widget to have it ignore any minimum or maximum values).\n"
				"Use the right mouse button and click on an item to open a context menu with additional options.\n\n"
				"Once you have finished tweaking your preset, be sure to enable the 'Performance Mode' check box. "
				"This will reload all effects into a more optimal representation that can give a performance boost, but disables variable tweaking and this list.");

			ImGui::PushStyleColor(ImGuiCol_Border, COLOR_RED);
		}

		const float bottom_height = ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y + (_tutorial_index == 3 ? 175 : 0);

		if (ImGui::BeginChild("##variables", ImVec2(0, -bottom_height), ImGuiChildFlags_Border))
		{
			ImGui::BeginDisabled(_is_in_preset_transition);
			draw_variable_editor();
			ImGui::EndDisabled();
		}
		ImGui::EndChild();

		if (_tutorial_index == 3)
			ImGui::PopStyleColor();
	}

	if (_tutorial_index > 3)
	{
		ImGui::Spacing();

		if (ImGui::Button((ICON_FK_REFRESH " " + std::string(_("Reload"))).c_str(), ImVec2(-(auto_save_button_spacing + button_width), 0)))
		{
			load_config(); // Reload configuration too

			if (!_no_effect_cache && (_imgui_context->IO.KeyCtrl || _imgui_context->IO.KeyShift))
				clear_effect_cache();

			reload_effects();
		}

		ImGui::SameLine();

		if (ImGui::Checkbox(_("Performance Mode"), &_performance_mode))
		{
			save_config();
			reload_effects(); // Reload effects after switching
		}

		ImGui::SetItemTooltip(_("Reload all effects into a more optimal representation that can give a performance boost (disables variable tweaking)"));
	}
	else
	{
		const float max_frame_width = ImGui::GetContentRegionAvail().x;
		const float max_frame_height = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight() - _imgui_context->Style.ItemSpacing.y;
		const float required_frame_height =
			ImGui::CalcTextSize(tutorial_text.data(), tutorial_text.data() + tutorial_text.size(), false, max_frame_width - _imgui_context->Style.FramePadding.x * 2).y +
			_imgui_context->Style.FramePadding.y * 2;

		if (ImGui::BeginChild("##tutorial", ImVec2(max_frame_width, std::min(max_frame_height, required_frame_height)), ImGuiChildFlags_FrameStyle))
		{
			ImGui::PushTextWrapPos();
			ImGui::TextUnformatted(tutorial_text.data(), tutorial_text.data() + tutorial_text.size());
			ImGui::PopTextWrapPos();
		}
		ImGui::EndChild();

		if (_tutorial_index == 0)
		{
			if (ImGui::Button((std::string(_("Continue")) + "###tutorial_button").c_str(), ImVec2(max_frame_width * 0.66666666f, 0)))
			{
				_tutorial_index++;

				save_config();
			}

			ImGui::SameLine();

			if (ImGui::Button(_("Skip Tutorial"), ImVec2(max_frame_width * 0.33333333f - _imgui_context->Style.ItemSpacing.x, 0)))
			{
				_tutorial_index = 4;

				save_config();
			}
		}
		else
		{
			if (ImGui::Button((std::string(_tutorial_index == 3 ? _("Finish") : _("Continue")) + "###tutorial_button").c_str(), ImVec2(max_frame_width, 0)))
			{
				_tutorial_index++;

				if (_tutorial_index == 4)
					save_config();
			}
		}
	}
}
#endif
void reshade::runtime::draw_gui_settings()
{
	if (ImGui::Button((ICON_FK_FOLDER " " + std::string(_("Open base folder in explorer"))).c_str(), ImVec2(-1, 0)))
		utils::open_explorer(_config_path);

	ImGui::Spacing();

	bool modified = false;
	bool modified_custom_style = false;

	if (ImGui::CollapsingHeader(_("General"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (_input != nullptr)
		{
			std::string input_processing_mode_items = _(
				"Pass on all input\n"
				"Block input when cursor is on overlay\n"
				"Block all input when overlay is visible\n");
			std::replace(input_processing_mode_items.begin(), input_processing_mode_items.end(), '\n', '\0');
			modified |= ImGui::Combo(_("Input processing"), reinterpret_cast<int *>(&_input_processing_mode), input_processing_mode_items.c_str());

			modified |= imgui::key_input_box(_("Overlay key"), _overlay_key_data, *_input);

#if RESHADE_FX
			modified |= imgui::key_input_box(_("Effect toggle key"), _effects_key_data, *_input);
			modified |= imgui::key_input_box(_("Effect reload key"), _reload_key_data, *_input);

			modified |= imgui::key_input_box(_("Performance mode toggle key"), _performance_mode_key_data, *_input);

			modified |= imgui::key_input_box(_("Previous preset key"), _prev_preset_key_data, *_input);
			modified |= imgui::key_input_box(_("Next preset key"), _next_preset_key_data, *_input);

			modified |= ImGui::SliderInt(_("Preset transition duration"), reinterpret_cast<int *>(&_preset_transition_duration), 0, 10 * 1000);
			ImGui::SetItemTooltip(_(
				"Make a smooth transition when switching presets, but only for floating point values.\n"
				"Recommended for multiple presets that contain the same effects, otherwise set this to zero.\n"
				"Values are in milliseconds."));
#endif

			ImGui::Spacing();
		}

#if RESHADE_FX
		modified |= imgui::file_input_box(_("Start-up preset"), nullptr, _startup_preset_path, _file_selection_path, { L".ini", L".txt" });
		ImGui::SetItemTooltip(_("When not empty, reset the current preset to this file during reloads."));

		ImGui::Spacing();

		modified |= imgui::path_list(_("Effect search paths"), _effect_search_paths, _file_selection_path, g_reshade_base_path);
		ImGui::SetItemTooltip(_("List of directory paths to be searched for effect files (.fx).\nPaths that end in \"\\**\" are searched recursively."));
		modified |= imgui::path_list(_("Texture search paths"), _texture_search_paths, _file_selection_path, g_reshade_base_path);
		ImGui::SetItemTooltip(_("List of directory paths to be searched for texture image files.\nPaths that end in \"\\**\" are searched recursively."));

		if (ImGui::Checkbox(_("Load only enabled effects"), &_effect_load_skipping))
		{
			modified = true;

			// Force load all effects in case some where skipped after load skipping was disabled
			reload_effects(!_effect_load_skipping);
		}

		if (ImGui::Button(_("Clear effect cache"), ImVec2(ImGui::CalcItemWidth(), 0)))
			clear_effect_cache();
		ImGui::SetItemTooltip(_("Clear effect cache located in \"%s\"."), _effect_cache_path.u8string().c_str());
#endif
	}

	if (ImGui::CollapsingHeader(_("Screenshots"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (_input != nullptr)
		{
			modified |= imgui::key_input_box(_("Screenshot key"), _screenshot_key_data, *_input);
		}

		modified |= imgui::directory_input_box(_("Screenshot path"), _screenshot_path, _file_selection_path);

		char name[260];
		name[_screenshot_name.copy(name, sizeof(name) - 1)] = '\0';
		if (ImGui::InputText(_("Screenshot name"), name, sizeof(name), ImGuiInputTextFlags_CallbackCharFilter, filter_name))
		{
			modified = true;
			_screenshot_name = name;
		}

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
		{
			ImGui::SetTooltip(_(
				"Macros you can add that are resolved during saving:\n"
				"  %%AppName%%         Name of the application (%s)\n"
				"  %%PresetName%%      File name without extension of the current preset file (%s)\n"
				"  %%Date%%            Current date in format '%s'\n"
				"  %%DateYear%%        Year component of current date\n"
				"  %%DateMonth%%       Month component of current date\n"
				"  %%DateDay%%         Day component of current date\n"
				"  %%Time%%            Current time in format '%s'\n"
				"  %%TimeHour%%        Hour component of current time\n"
				"  %%TimeMinute%%      Minute component of current time\n"
				"  %%TimeSecond%%      Second component of current time\n"
				"  %%TimeMS%%          Milliseconds fraction of current time\n"
				"  %%Count%%           Number of screenshots taken this session\n"),
				g_target_executable_path.stem().u8string().c_str(),
#if RESHADE_FX
				_current_preset_path.stem().u8string().c_str(),
#else
				"..."
#endif
				"yyyy-MM-dd",
				"HH-mm-ss");
		}

		modified |= ImGui::Combo(_("Screenshot format"), reinterpret_cast<int *>(&_screenshot_format), "Bitmap (*.bmp)\0Portable Network Graphics (*.png)\0JPEG (*.jpeg)\0");

		if (_screenshot_format == 2)
			modified |= ImGui::SliderInt(_("JPEG quality"), reinterpret_cast<int *>(&_screenshot_jpeg_quality), 1, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
		else
			modified |= ImGui::Checkbox(_("Clear alpha channel"), &_screenshot_clear_alpha);

#if RESHADE_FX
		modified |= ImGui::Checkbox(_("Save current preset file"), &_screenshot_include_preset);
		modified |= ImGui::Checkbox(_("Save before and after images"), &_screenshot_save_before);
#endif
		modified |= ImGui::Checkbox(_("Save separate image with the overlay visible"), &_screenshot_save_gui);

		modified |= imgui::file_input_box(_("Screenshot sound"), "sound.wav", _screenshot_sound_path, _file_selection_path, { L".wav" });
		ImGui::SetItemTooltip(_("Audio file that is played when taking a screenshot."));

		modified |= imgui::file_input_box(_("Post-save command"), "command.exe", _screenshot_post_save_command, _file_selection_path, { L".exe" });
		ImGui::SetItemTooltip(_(
			"Executable that is called after saving a screenshot.\n"
			"This can be used to perform additional processing on the image (e.g. compressing it with an image optimizer)."));

		char arguments[260];
		arguments[_screenshot_post_save_command_arguments.copy(arguments, sizeof(arguments) - 1)] = '\0';
		if (ImGui::InputText(_("Post-save command arguments"), arguments, sizeof(arguments)))
		{
			modified = true;
			_screenshot_post_save_command_arguments = arguments;
		}

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
		{
			const std::string extension = _screenshot_format == 0 ? ".bmp" : _screenshot_format == 1 ? ".png" : ".jpg";

			ImGui::SetTooltip(_(
				"Macros you can add that are resolved during command execution:\n"
				"  %%AppName%%         Name of the application (%s)\n"
				"  %%PresetName%%      File name without extension of the current preset file (%s)\n"
				"  %%Date%%            Current date in format '%s'\n"
				"  %%DateYear%%        Year component of current date\n"
				"  %%DateMonth%%       Month component of current date\n"
				"  %%DateDay%%         Day component of current date\n"
				"  %%Time%%            Current time in format '%s'\n"
				"  %%TimeHour%%        Hour component of current time\n"
				"  %%TimeMinute%%      Minute component of current time\n"
				"  %%TimeSecond%%      Second component of current time\n"
				"  %%TimeMS%%          Milliseconds fraction of current time\n"
				"  %%TargetPath%%      Full path to the screenshot file (%s)\n"
				"  %%TargetDir%%       Full path to the screenshot directory (%s)\n"
				"  %%TargetFileName%%  File name of the screenshot file (%s)\n"
				"  %%TargetExt%%       File extension of the screenshot file (%s)\n"
				"  %%TargetName%%      File name without extension of the screenshot file (%s)\n"
				"  %%Count%%           Number of screenshots taken this session\n"),
				g_target_executable_path.stem().u8string().c_str(),
#if RESHADE_FX
				_current_preset_path.stem().u8string().c_str(),
#else
				"..."
#endif
				"yyyy-MM-dd",
				"HH-mm-ss",
				(_screenshot_path / (_screenshot_name + extension)).u8string().c_str(),
				_screenshot_path.u8string().c_str(),
				(_screenshot_name + extension).c_str(),
				extension.c_str(),
				_screenshot_name.c_str());
		}

		modified |= imgui::directory_input_box(_("Post-save command working directory"), _screenshot_post_save_command_working_directory, _file_selection_path);
		modified |= ImGui::Checkbox(_("Hide post-save command window"), &_screenshot_post_save_command_hide_window);
	}

	if (ImGui::CollapsingHeader(_("Overlay & Styling"), ImGuiTreeNodeFlags_DefaultOpen))
	{
#if RESHADE_LOCALIZATION
		{
			std::vector<std::string> languages = resources::get_languages();

			int lang_index = 0;
			if (const auto it = std::find(languages.begin(), languages.end(), _selected_language); it != languages.end())
				lang_index = static_cast<int>(std::distance(languages.begin(), it) + 1);

			if (ImGui::Combo(_("Language"), &lang_index,
					[](void *data, int idx) -> const char * {
						return idx == 0 ? "System Default" : (*static_cast<const std::vector<std::string> *>(data))[idx - 1].c_str();
					}, &languages, static_cast<int>(languages.size() + 1)))
			{
				modified = true;
				if (lang_index == 0)
					_selected_language.clear();
				else
					_selected_language = languages[lang_index - 1];
				// Rebuild font atlas in case language needs a special font or glyph range
				_imgui_context->IO.Fonts->TexReady = false;
			}
		}
#endif

#if RESHADE_FX
		if (ImGui::Button(_("Restart tutorial"), ImVec2(ImGui::CalcItemWidth(), 0)))
			_tutorial_index = 0;
#endif

		modified |= ImGui::Checkbox(_("Show screenshot message"), &_show_screenshot_message);

#if RESHADE_FX
		ImGui::BeginDisabled(_preset_transition_duration == 0);
		modified |= ImGui::Checkbox(_("Show preset transition message"), &_show_preset_transition_message);
		ImGui::EndDisabled();
		if (_preset_transition_duration == 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_ForTooltip))
			ImGui::SetTooltip(_("Preset transition duration has to be non-zero for the preset transition message to show up."));

		if (_effect_load_skipping)
			modified |= ImGui::Checkbox(_("Show \"Force load all effects\" button"), &_show_force_load_effects_button);
#endif

#if RESHADE_FX
		modified |= ImGui::Checkbox(_("Group effect files with tabs instead of a tree"), &_variable_editor_tabs);
#endif

		#pragma region Style
		if (ImGui::Combo(_("Global style"), &_style_index, "Dark\0Light\0Default\0Custom Simple\0Custom Advanced\0Solarized Dark\0Solarized Light\0"))
		{
			modified = true;
			load_custom_style();
		}

		if (_style_index == 3) // Custom Simple
		{
			ImVec4 *const colors = _imgui_context->Style.Colors;

			if (ImGui::BeginChild("##colors", ImVec2(0, 105), ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened))
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
			if (ImGui::BeginChild("##colors", ImVec2(0, 300), ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened))
			{
				ImGui::PushItemWidth(-160);
				for (ImGuiCol i = 0; i < ImGuiCol_COUNT; i++)
				{
					ImGui::PushID(i);
					modified_custom_style |= ImGui::ColorEdit4("##color", &_imgui_context->Style.Colors[i].x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
					ImGui::SameLine();
					ImGui::TextUnformatted(ImGui::GetStyleColorName(i));
					ImGui::PopID();
				}
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
		}
		#pragma endregion

		#pragma region Editor Style
		if (ImGui::Combo(_("Text editor style"), &_editor_style_index, "Dark\0Light\0Custom\0Solarized Dark\0Solarized Light\0"))
		{
			modified = true;
			load_custom_style();
		}

		if (_editor_style_index == 2)
		{
			if (ImGui::BeginChild("##editor_colors", ImVec2(0, 300), ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened))
			{
				ImGui::PushItemWidth(-160);
				for (ImGuiCol i = 0; i < imgui::code_editor::color_palette_max; i++)
				{
					ImVec4 color = ImGui::ColorConvertU32ToFloat4(_editor_palette[i]);
					ImGui::PushID(i);
					modified_custom_style |= ImGui::ColorEdit4("##editor_color", &color.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
					ImGui::SameLine();
					ImGui::TextUnformatted(imgui::code_editor::get_palette_color_name(i));
					ImGui::PopID();
					_editor_palette[i] = ImGui::ColorConvertFloat4ToU32(color);
				}
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
		}
		#pragma endregion

		if (imgui::font_input_box(_("Global font"), _default_font_path.empty() ? "ProggyClean.ttf" : _default_font_path.u8string().c_str(), _font_path, _file_selection_path, _font_size))
		{
			modified = true;
			_imgui_context->IO.Fonts->TexReady = false;
		}

		if (_imgui_context->IO.Fonts->Fonts[0]->ConfigDataCount > 2 && // Latin font + main font + icon font
			imgui::font_input_box(_("Latin font"), "ProggyClean.ttf", _latin_font_path, _file_selection_path, _font_size))
		{
			modified = true;
			_imgui_context->IO.Fonts->TexReady = false;
		}

		if (imgui::font_input_box(_("Text editor font"), _default_editor_font_path.empty() ? "ProggyClean.ttf" : _default_editor_font_path.u8string().c_str(), _editor_font_path, _file_selection_path, _editor_font_size))
		{
			modified = true;
			_imgui_context->IO.Fonts->TexReady = false;
		}

		if (float &alpha = _imgui_context->Style.Alpha; ImGui::SliderFloat(_("Global alpha"), &alpha, 0.1f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp))
		{
			// Prevent user from setting alpha to zero
			alpha = std::max(alpha, 0.1f);
			modified = true;
		}

		// Only show on possible HDR swap chains
		if (((_renderer_id & 0xB000) == 0xB000 || (_renderer_id & 0xC000) == 0xC000 || (_renderer_id & 0x20000) == 0x20000) &&
			(_back_buffer_format == reshade::api::format::r10g10b10a2_unorm || _back_buffer_format == reshade::api::format::b10g10r10a2_unorm || _back_buffer_format == reshade::api::format::r16g16b16a16_float))
		{
			if (ImGui::SliderFloat(_("HDR overlay brightness"), &_hdr_overlay_brightness, 20.f, 400.f, "%.0f nits", ImGuiSliderFlags_AlwaysClamp))
				modified = true;

			if (ImGui::Combo(_("Overlay color space"), reinterpret_cast<int *>(&_hdr_overlay_overwrite_color_space), "Auto\0SDR\0scRGB\0HDR10\0HLG\0"))
				modified = true;
		}

		if (float &rounding = _imgui_context->Style.FrameRounding; ImGui::SliderFloat(_("Frame rounding"), &rounding, 0.0f, 12.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp))
		{
			// Apply the same rounding to everything
			_imgui_context->Style.WindowRounding = rounding;
			_imgui_context->Style.ChildRounding = rounding;
			_imgui_context->Style.PopupRounding = rounding;
			_imgui_context->Style.ScrollbarRounding = rounding;
			_imgui_context->Style.GrabRounding = rounding;
			_imgui_context->Style.TabRounding = rounding;
			modified = true;
		}

		if (!_is_vr)
		{
			ImGui::Spacing();

			ImGui::BeginGroup();
			modified |= imgui::checkbox_tristate(_("Show clock"), &_show_clock);
			ImGui::SameLine(0, 10);
			modified |= imgui::checkbox_tristate(_("Show FPS"), &_show_fps);
			ImGui::SameLine(0, 10);
			modified |= imgui::checkbox_tristate(_("Show frame time"), &_show_frametime);
			modified |= imgui::checkbox_tristate(_("Show preset name"), &_show_preset_name);
			ImGui::EndGroup();
			ImGui::SetItemTooltip(_("Check to always show, fill out to only show while overlay is open."));

			if (_input != nullptr)
			{
				modified |= imgui::key_input_box(_("FPS key"), _fps_key_data, *_input);
				modified |= imgui::key_input_box(_("Frame time key"), _frametime_key_data, *_input);
			}

			if (_show_clock)
				modified |= ImGui::Combo(_("Clock format"), reinterpret_cast<int *>(&_clock_format), "HH:mm\0HH:mm:ss\0");

			modified |= ImGui::SliderFloat(_("OSD text size"), &_fps_scale, 0.2f, 2.5f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			modified |= ImGui::ColorEdit4(_("OSD text color"), _fps_col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);

			std::string fps_pos_items = _("Top left\nTop right\nBottom left\nBottom right\n");
			std::replace(fps_pos_items.begin(), fps_pos_items.end(), '\n', '\0');
			modified |= ImGui::Combo(_("OSD position on screen"), reinterpret_cast<int *>(&_fps_pos), fps_pos_items.c_str());
		}
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

	if (ImGui::CollapsingHeader(_("General"), ImGuiTreeNodeFlags_DefaultOpen))
	{
#if RESHADE_FX
		_gather_gpu_statistics = true;
#endif

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::PlotLines("##framerate",
			_imgui_context->FramerateSecPerFrame, static_cast<int>(std::size(_imgui_context->FramerateSecPerFrame)),
			_imgui_context->FramerateSecPerFrameIdx,
			nullptr,
			_imgui_context->FramerateSecPerFrameAccum / static_cast<int>(std::size(_imgui_context->FramerateSecPerFrame)) * 0.5f,
			_imgui_context->FramerateSecPerFrameAccum / static_cast<int>(std::size(_imgui_context->FramerateSecPerFrame)) * 1.5f,
			ImVec2(0, 50));

		const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		struct tm tm; localtime_s(&tm, &t);

		ImGui::BeginGroup();

		ImGui::TextUnformatted(_("API:"));
		ImGui::TextUnformatted(_("Hardware:"));
		ImGui::TextUnformatted(_("Application:"));
		ImGui::TextUnformatted(_("Time:"));
		ImGui::Text(_("Frame %llu:"), _frame_count + 1);
#if RESHADE_FX
		ImGui::TextUnformatted(_("Resolution:"));
		ImGui::TextUnformatted(_("Post-Processing:"));
#endif

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.33333333f);
		ImGui::BeginGroup();

		const char *api_name = "Unknown";
		switch (_device->get_api())
		{
		case api::device_api::d3d9:
			api_name = "D3D9";
			break;
		case api::device_api::d3d10:
			api_name = "D3D10";
			break;
		case api::device_api::d3d11:
			api_name = "D3D11";
			break;
		case api::device_api::d3d12:
			api_name = "D3D12";
			break;
		case api::device_api::opengl:
			api_name = "OpenGL";
			break;
		case api::device_api::vulkan:
			api_name = "Vulkan";
			break;
		}

		ImGui::TextUnformatted(api_name);
		if (_vendor_id != 0)
			ImGui::Text("VEN_%X", _vendor_id);
		else
			ImGui::TextUnformatted("Unknown");
		ImGui::TextUnformatted(g_target_executable_path.filename().u8string().c_str());
		ImGui::Text("%d-%d-%d %d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
		ImGui::Text("%.2f fps", _imgui_context->IO.Framerate);
#if RESHADE_FX
		ImGui::Text("%ux%u", _effect_width, _effect_height);
		ImGui::Text("%*.3f ms CPU", cpu_digits + 4, post_processing_time_cpu * 1e-6f);
#endif

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.66666666f);
		ImGui::BeginGroup();

		ImGui::Text("0x%X", _renderer_id);
		if (_device_id != 0)
			ImGui::Text("DEV_%X", _device_id);
		else
			ImGui::TextUnformatted("Unknown");
		ImGui::Text("0x%X", std::hash<std::string>()(g_target_executable_path.stem().u8string()) & 0xFFFFFFFF);
		ImGui::Text("%.0f ms", std::chrono::duration_cast<std::chrono::nanoseconds>(_last_present_time - _start_time).count() * 1e-6f);
		ImGui::Text("%*.3f ms", gpu_digits + 4, _last_frame_duration.count() * 1e-6f);
#if RESHADE_FX
		ImGui::Text("format %u", _effect_color_format);
		if (_gather_gpu_statistics && post_processing_time_gpu != 0)
			ImGui::Text("%*.3f ms GPU", gpu_digits + 4, (post_processing_time_gpu * 1e-6f));
#endif

		ImGui::EndGroup();
	}

#if RESHADE_FX
	if (ImGui::CollapsingHeader(_("Techniques"), ImGuiTreeNodeFlags_DefaultOpen) && !is_loading() && _effects_enabled)
	{
		// Only need to gather GPU statistics if the statistics are actually visible
		_gather_gpu_statistics = true;

		ImGui::BeginGroup();

		std::vector<bool> long_technique_name(_techniques.size());
		for (size_t technique_index : _technique_sorting)
		{
			const reshade::technique &tech = _techniques[technique_index];

			if (!tech.enabled)
				continue;

			if (tech.passes.size() > 1)
				ImGui::Text("%s (%zu passes)", tech.name.c_str(), tech.passes.size());
			else
				ImGui::TextUnformatted(tech.name.c_str(), tech.name.c_str() + tech.name.size());

			long_technique_name[technique_index] = (ImGui::GetItemRectSize().x + 10.0f) > (ImGui::GetWindowWidth() * 0.33333333f);
			if (long_technique_name[technique_index])
				ImGui::NewLine();
		}

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.33333333f);
		ImGui::BeginGroup();

		for (size_t technique_index : _technique_sorting)
		{
			const reshade::technique &tech = _techniques[technique_index];

			if (!tech.enabled)
				continue;

			if (long_technique_name[technique_index])
				ImGui::NewLine();

			if (tech.average_cpu_duration != 0)
				ImGui::Text("%*.3f ms CPU", cpu_digits + 4, tech.average_cpu_duration * 1e-6f);
			else
				ImGui::NewLine();
		}

		ImGui::EndGroup();
		ImGui::SameLine(ImGui::GetWindowWidth() * 0.66666666f);
		ImGui::BeginGroup();

		for (size_t technique_index : _technique_sorting)
		{
			const reshade::technique &tech = _techniques[technique_index];

			if (!tech.enabled)
				continue;

			if (long_technique_name[technique_index])
				ImGui::NewLine();

			// GPU timings are not available for all APIs
			if (_gather_gpu_statistics && tech.average_gpu_duration != 0)
				ImGui::Text("%*.3f ms GPU", gpu_digits + 4, tech.average_gpu_duration * 1e-6f);
			else
				ImGui::NewLine();
		}

		ImGui::EndGroup();
	}

	if (ImGui::CollapsingHeader(_("Render Targets & Textures"), ImGuiTreeNodeFlags_DefaultOpen) && !is_loading())
	{
		static const char *texture_formats[] = {
			"unknown",
			"R8", "R16", "R16F", "R32I", "R32U", "R32F", "RG8", "RG16", "RG16F", "RG32F", "RGBA8", "RGBA16", "RGBA16F", "RGBA32F", "RGB10A2"
		};
		static constexpr uint32_t pixel_sizes[] = {
			0,
			1 /*R8*/, 2 /*R16*/, 2 /*R16F*/, 4 /*R32I*/, 4 /*R32U*/, 4 /*R32F*/, 2 /*RG8*/, 4 /*RG16*/, 4 /*RG16F*/, 8 /*RG32F*/, 4 /*RGBA8*/, 8 /*RGBA16*/, 8 /*RGBA16F*/, 16 /*RGBA32F*/, 4 /*RGB10A2*/
		};

		static_assert((std::size(texture_formats) - 1) == static_cast<size_t>(reshadefx::texture_format::rgb10a2));

		const float total_width = ImGui::GetContentRegionAvail().x;
		int texture_index = 0;
		const unsigned int num_columns = std::max(1u, static_cast<unsigned int>(std::ceilf(total_width / (55.0f * _font_size))));
		const float single_image_width = (total_width / num_columns) - 5.0f;

		// Variables used to calculate memory size of textures
		lldiv_t memory_view;
		int64_t post_processing_memory_size = 0;
		const char *memory_size_unit;

		for (const texture &tex : _textures)
		{
			if (tex.resource == 0 || !tex.semantic.empty() || !std::any_of(tex.shared.cbegin(), tex.shared.cend(), [this](size_t effect_index) { return _effects[effect_index].rendering; }))
				continue;

			ImGui::PushID(texture_index);
			ImGui::BeginGroup();

			int64_t memory_size = 0;
			for (uint32_t level = 0, width = tex.width, height = tex.height, depth = tex.depth; level < tex.levels; ++level, width /= 2, height /= 2, depth /= 2)
				memory_size += static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * pixel_sizes[static_cast<int>(tex.format)];

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

			ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s%s", tex.unique_name.c_str(), tex.shared.size() > 1 ? " (pooled)" : "");
			switch (tex.type)
			{
			case reshadefx::texture_type::texture_1d:
				ImGui::Text("%u | %u mipmap(s) | %s | %lld.%03lld %s",
					tex.width,
					tex.levels - 1,
					texture_formats[static_cast<int>(tex.format)],
					memory_view.quot, memory_view.rem, memory_size_unit);
				break;
			case reshadefx::texture_type::texture_2d:
				ImGui::Text("%ux%u | %u mipmap(s) | %s | %lld.%03lld %s",
					tex.width,
					tex.height,
					tex.levels - 1,
					texture_formats[static_cast<int>(tex.format)],
					memory_view.quot, memory_view.rem, memory_size_unit);
				break;
			case reshadefx::texture_type::texture_3d:
				ImGui::Text("%ux%ux%u | %u mipmap(s) | %s | %lld.%03lld %s",
					tex.width,
					tex.height,
					tex.depth,
					tex.levels - 1,
					texture_formats[static_cast<int>(tex.format)],
					memory_view.quot, memory_view.rem, memory_size_unit);
				break;
			}

			size_t num_referenced_passes = 0;
			std::vector<std::pair<size_t, std::vector<std::string>>> references;
			for (const technique &tech : _techniques)
			{
				if (std::find(tex.shared.cbegin(), tex.shared.cend(), tech.effect_index) == tex.shared.cend())
					continue;

				std::pair<size_t, std::vector<std::string>> &reference = references.emplace_back();
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

			const bool supports_saving =
				tex.type != reshadefx::texture_type::texture_3d && (
				tex.format == reshadefx::texture_format::r8 ||
				tex.format == reshadefx::texture_format::rg8 ||
				tex.format == reshadefx::texture_format::rgba8 ||
				tex.format == reshadefx::texture_format::rgb10a2);

			const float button_size = ImGui::GetFrameHeight();
			const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
			if (const std::string label = "Referenced by " + std::to_string(num_referenced_passes) + " pass(es) in " + std::to_string(tex.shared.size()) + " effect(s) ...";
				ImGui::ButtonEx(label.c_str(), ImVec2(single_image_width - (supports_saving ? button_spacing + button_size : 0), 0)))
				ImGui::OpenPopup("##references");
			if (supports_saving)
			{
				ImGui::SameLine(0, button_spacing);
				if (ImGui::Button(ICON_FK_FLOPPY, ImVec2(button_size, 0)))
					save_texture(tex);
				ImGui::SetItemTooltip(_("Save %s"), tex.unique_name.c_str());
			}
			ImGui::PopStyleVar();

			if (!references.empty() && ImGui::BeginPopup("##references"))
			{
				bool is_open = false;
				size_t effect_index = std::numeric_limits<size_t>::max();

				for (const std::pair<size_t, std::vector<std::string>> &reference : references)
				{
					if (effect_index != reference.first)
					{
						effect_index  = reference.first;
						is_open = ImGui::TreeNodeEx(_effects[effect_index].source_file.filename().u8string().c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen);
					}

					if (is_open)
					{
						for (const std::string &pass : reference.second)
						{
							ImGui::Dummy(ImVec2(_imgui_context->Style.IndentSpacing, 0.0f));
							ImGui::SameLine(0.0f, 0.0f);
							ImGui::TextUnformatted(pass.c_str(), pass.c_str() + pass.size());
						}
					}
				}

				ImGui::EndPopup();
			}

			if (tex.type == reshadefx::texture_type::texture_2d)
			{
				if (bool check = _preview_texture == tex.srv[0] && _preview_size[0] == 0; ImGui::RadioButton(_("Preview scaled"), check))
				{
					_preview_size[0] = 0;
					_preview_size[1] = 0;
					_preview_texture = !check ? tex.srv[0] : api::resource_view { 0 };
				}
				ImGui::SameLine();
				if (bool check = _preview_texture == tex.srv[0] && _preview_size[0] != 0; ImGui::RadioButton(_("Preview original"), check))
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
			}

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

		ImGui::Text(_("Total memory usage: %lld.%03lld %s"), memory_view.quot, memory_view.rem, memory_size_unit);
	}
#endif
}
void reshade::runtime::draw_gui_log()
{
	std::error_code ec;
	std::filesystem::path log_path = global_config().path();
	log_path.replace_extension(L".log");

	const bool filter_changed = imgui::search_input_box(_log_filter, sizeof(_log_filter), -(16.0f * _font_size + 2 * _imgui_context->Style.ItemSpacing.x));

	ImGui::SameLine();

	imgui::toggle_button(_("Word Wrap"), _log_wordwrap, 8.0f * _font_size);

	ImGui::SameLine();

	if (ImGui::Button(_("Clear Log"), ImVec2(8.0f * _font_size, 0.0f)))
		// Close and open the stream again, which will clear the file too
		log::open_log_file(log_path, ec);

	ImGui::Spacing();

	if (ImGui::BeginChild("##log", ImVec2(0, -(ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y)), ImGuiChildFlags_Border, _log_wordwrap ? 0 : ImGuiWindowFlags_AlwaysHorizontalScrollbar))
	{
		// Limit number of log lines to read, to avoid stalling when log gets too big
		constexpr size_t LINE_LIMIT = 1000;

		const uintmax_t file_size = std::filesystem::file_size(log_path, ec);
		if (filter_changed || _last_log_size != file_size)
		{
			_log_lines.clear();
			std::ifstream log_file(log_path);
			for (std::string line; std::getline(log_file, line) && _log_lines.size() < LINE_LIMIT;)
				if (filter_text(line, _log_filter))
					_log_lines.push_back(line);
			_last_log_size = file_size;

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

				ImGui::TextUnformatted(_log_lines[i].c_str(), _log_lines[i].c_str() + _log_lines[i].size());

				if (_log_wordwrap) ImGui::PopTextWrapPos();
				ImGui::PopStyleColor();
			}
		}
	}
	ImGui::EndChild();

	ImGui::Spacing();

	if (ImGui::Button((ICON_FK_FOLDER " " + std::string(_("Open folder in explorer"))).c_str(), ImVec2(-1, 0)))
		utils::open_explorer(log_path);
}
void reshade::runtime::draw_gui_about()
{
	ImGui::TextUnformatted("ReShade " VERSION_STRING_PRODUCT);

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(_(" Open website ")).x);
	if (ImGui::SmallButton(_(" Open website ")))
		utils::execute_command("https://reshade.me");

	ImGui::Separator();

	ImGui::PushTextWrapPos();

	ImGui::TextUnformatted(_("Developed and maintained by crosire."));
	ImGui::TextUnformatted(_("This project makes use of several open source libraries, licenses of which are listed below:"));

	if (ImGui::CollapsingHeader("ReShade", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const resources::data_resource resource = resources::load_data_resource(IDR_LICENSE_RESHADE);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("MinHook"))
	{
		const resources::data_resource resource = resources::load_data_resource(IDR_LICENSE_MINHOOK);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("Dear ImGui"))
	{
		const resources::data_resource resource = resources::load_data_resource(IDR_LICENSE_IMGUI);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("ImGuiColorTextEdit"))
	{
		ImGui::TextUnformatted("Copyright (C) 2017 BalazsJako\
\
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\
\
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\
\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.");
	}
	if (ImGui::CollapsingHeader("gl3w"))
	{
		const resources::data_resource resource = resources::load_data_resource(IDR_LICENSE_GL3W);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("UTF8-CPP"))
	{
		const resources::data_resource resource = resources::load_data_resource(IDR_LICENSE_UTFCPP);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("stb_image, stb_image_write"))
	{
		const resources::data_resource resource = resources::load_data_resource(IDR_LICENSE_STB);
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
		const resources::data_resource resource = resources::load_data_resource(IDR_LICENSE_SPIRV);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("Vulkan & Vulkan-Loader"))
	{
		const resources::data_resource resource = resources::load_data_resource(IDR_LICENSE_VULKAN);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("Vulkan Memory Allocator"))
	{
		const resources::data_resource resource = resources::load_data_resource(IDR_LICENSE_VMA);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("OpenVR"))
	{
		const resources::data_resource resource = resources::load_data_resource(IDR_LICENSE_OPENVR);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("OpenXR"))
	{
		const resources::data_resource resource = resources::load_data_resource(IDR_LICENSE_OPENXR);
		ImGui::TextUnformatted(static_cast<const char *>(resource.data), static_cast<const char *>(resource.data) + resource.data_size);
	}
	if (ImGui::CollapsingHeader("Solarized"))
	{
		ImGui::TextUnformatted("Copyright (C) 2011 Ethan Schoonover\
\
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\
\
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\
\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.");
	}
	if (ImGui::CollapsingHeader("Fork Awesome"))
	{
		ImGui::TextUnformatted("Copyright (C) 2018 Fork Awesome (https://forkawesome.github.io)\
\
This Font Software is licensed under the SIL Open Font License, Version 1.1. (http://scripts.sil.org/OFL)");
	}

	ImGui::PopTextWrapPos();
}
#if RESHADE_ADDON
void reshade::runtime::draw_gui_addons()
{
	ini_file &config = global_config();

#if RESHADE_ADDON == 1
	if (!addon_enabled)
	{
		ImGui::PushTextWrapPos();
		ImGui::TextColored(COLOR_YELLOW, _("High network activity discovered.\nAll add-ons are disabled to prevent exploitation."));
		ImGui::PopTextWrapPos();
		return;
	}

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(_("This build of ReShade has only limited add-on functionality."));
#else
	std::filesystem::path addon_search_path = L".\\";
	config.get("ADDON", "AddonPath", addon_search_path);
	if (imgui::directory_input_box(_("Add-on search path"), addon_search_path, _file_selection_path))
		config.set("ADDON", "AddonPath", addon_search_path);
#endif

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	imgui::search_input_box(_addons_filter, sizeof(_addons_filter));

	ImGui::Spacing();

	if (!addon_all_loaded)
	{
		ImGui::PushTextWrapPos();
#if RESHADE_ADDON == 1
		ImGui::TextColored(COLOR_YELLOW, _("Some add-ons were not loaded because this build of ReShade has only limited add-on functionality."));
#else
		ImGui::TextColored(COLOR_RED, _("There were errors loading some add-ons."));
		ImGui::TextColored(COLOR_RED, _("Check the log for more details."));
#endif
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
	}

	if (ImGui::BeginChild("##addons", ImVec2(0, -(ImGui::GetFrameHeightWithSpacing() + _imgui_context->Style.ItemSpacing.y)), ImGuiChildFlags_None, ImGuiWindowFlags_NavFlattened))
	{
		std::vector<std::string> disabled_addons;
		config.get("ADDON", "DisabledAddons", disabled_addons);

		const float child_window_width = ImGui::GetContentRegionAvail().x;

		for (addon_info &info : addon_loaded_info)
		{
			if (!filter_text(info.name, _addons_filter))
				continue;

			ImGui::BeginChild(info.name.c_str(), ImVec2(child_window_width, 0.0f), ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar);

			const bool builtin = (info.file == g_reshade_dll_path.filename().u8string());

			bool open = ImGui::GetStateStorage()->GetBool(ImGui::GetID("##addon_open"), builtin);
			if (ImGui::ArrowButton("##addon_open", open ? ImGuiDir_Down : ImGuiDir_Right))
				ImGui::GetStateStorage()->SetBool(ImGui::GetID("##addon_open"), open = !open);

			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(info.handle != nullptr ? ImGuiCol_Text : ImGuiCol_TextDisabled));

			const auto disabled_it = std::find_if(disabled_addons.begin(), disabled_addons.end(),
				[&info](const std::string_view addon_name) {
					const size_t at_pos = addon_name.find('@');
					if (at_pos == std::string_view::npos)
						return addon_name == info.name;
					return addon_name.substr(0, at_pos) == info.name && addon_name.substr(at_pos + 1) == info.file;
				});

			bool enabled = (disabled_it == disabled_addons.end());
			if (ImGui::Checkbox(info.name.c_str(), &enabled))
			{
				if (enabled)
					disabled_addons.erase(disabled_it);
				else
					disabled_addons.push_back(builtin ? info.name : info.name + '@' + info.file);

				config.set("ADDON", "DisabledAddons", disabled_addons);
			}

			ImGui::PopStyleColor();

			if (enabled == (info.handle == nullptr))
			{
				ImGui::SameLine();
				ImGui::TextUnformatted(enabled ? _("(will be enabled on next application restart)") : _("(will be disabled on next application restart)"));
			}

			if (open)
			{
				ImGui::Spacing();
				ImGui::BeginGroup();

				if (!builtin)
					ImGui::Text(_("File:"));
				if (!info.author.empty())
					ImGui::Text(_("Author:"));
				if (info.version.value)
					ImGui::Text(_("Version:"));
				if (!info.description.empty())
					ImGui::Text(_("Description:"));

				ImGui::EndGroup();
				ImGui::SameLine(ImGui::GetWindowWidth() * 0.25f);
				ImGui::BeginGroup();

				if (!builtin)
					ImGui::TextUnformatted(info.file.c_str(), info.file.c_str() + info.file.size());
				if (!info.author.empty())
					ImGui::TextUnformatted(info.author.c_str(), info.author.c_str() + info.author.size());
				if (info.version.value)
					ImGui::Text("%u.%u.%u.%u", info.version.number.major, info.version.number.minor, info.version.number.build, info.version.number.revision);
				if (!info.description.empty())
				{
					ImGui::PushTextWrapPos();
					ImGui::TextUnformatted(info.description.c_str(), info.description.c_str() + info.description.size());
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

			ImGui::EndChild();
		}
	}
	ImGui::EndChild();

	ImGui::Spacing();

	if (ImGui::Button(_("Open developer documentation"), ImVec2(-1, 0)))
		utils::execute_command("https://reshade.me/docs");
}
#endif

#if RESHADE_FX
void reshade::runtime::draw_variable_editor()
{
	const ImVec2 popup_pos = ImGui::GetCursorScreenPos() + ImVec2(std::max(0.f, (ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x) * 0.5f - 200.0f), ImGui::GetFrameHeightWithSpacing());

	if (imgui::popup_button(_("Edit global preprocessor definitions"), ImGui::GetContentRegionAvail().x, ImGuiWindowFlags_NoMove))
	{
		ImGui::SetWindowPos(popup_pos);

		bool global_modified = false, preset_modified = false;
		float popup_height = (std::max(_global_preprocessor_definitions.size(), _preset_preprocessor_definitions[{}].size()) + 2) * ImGui::GetFrameHeightWithSpacing();
		popup_height = std::min(popup_height, ImGui::GetWindowViewport()->Size.y - popup_pos.y - 20.0f);
		popup_height = std::max(popup_height, 42.0f); // Ensure window always has a minimum height
		const float button_size = ImGui::GetFrameHeight();
		const float button_spacing = _imgui_context->Style.ItemInnerSpacing.x;

		ImGui::BeginChild("##definitions", ImVec2(30.0f * _font_size, popup_height));

		if (ImGui::BeginTabBar("##definition_types", ImGuiTabBarFlags_NoTooltip))
		{
			const float content_region_width = ImGui::GetContentRegionAvail().x;

			struct
			{
				std::string name;
				std::vector<std::pair<std::string, std::string>> &definitions;
				bool &modified;
			} definition_types[] = {
				{ _("Global"), _global_preprocessor_definitions, global_modified },
				{ _("Current Preset"), _preset_preprocessor_definitions[{}], preset_modified },
			};

			for (const auto &type : definition_types)
			{
				if (ImGui::BeginTabItem(type.name.c_str()))
				{
					for (auto it = type.definitions.begin(); it != type.definitions.end();)
					{
						char name[128];
						name[it->first.copy(name, sizeof(name) - 1)] = '\0';
						char value[256];
						value[it->second.copy(value, sizeof(value) - 1)] = '\0';

						ImGui::PushID(static_cast<int>(std::distance(type.definitions.begin(), it)));

						ImGui::SetNextItemWidth(content_region_width * 0.66666666f - (button_spacing));
						type.modified |= ImGui::InputText("##name", name, sizeof(name), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter,
							[](ImGuiInputTextCallbackData *data) -> int { return data->EventChar == '=' || (data->EventChar != '_' && !isalnum(data->EventChar)); }); // Filter out invalid characters

						ImGui::SameLine(0, button_spacing);

						ImGui::SetNextItemWidth(content_region_width * 0.33333333f - (button_spacing + button_size));
						type.modified |= ImGui::InputText("##value", value, sizeof(value));

						ImGui::SameLine(0, button_spacing);

						if (imgui::confirm_button(ICON_FK_MINUS, button_size, _("Do you really want to remove the preprocessor definition '%s'?"), name))
						{
							type.modified = true;
							it = type.definitions.erase(it);
						}
						else
						{
							if (type.modified)
							{
								it->first = name;
								it->second = value;
							}

							++it;
						}

						ImGui::PopID();
					}

					ImGui::Dummy(ImVec2());
					ImGui::SameLine(0, content_region_width - button_size);
					if (ImGui::Button(ICON_FK_PLUS, ImVec2(button_size, 0)))
						type.definitions.emplace_back();

					ImGui::EndTabItem();
				}
			}

			ImGui::EndTabBar();
		}

		ImGui::EndChild();

		if (global_modified)
			save_config();
		if (preset_modified)
			save_current_preset();
		if (global_modified || preset_modified)
			_was_preprocessor_popup_edited = true;

		ImGui::EndPopup();
	}
	else if (_was_preprocessor_popup_edited)
	{
		reload_effects();
		_was_preprocessor_popup_edited = false;
	}

	ImGui::BeginChild("##variables", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NavFlattened);
	if (_variable_editor_tabs)
		ImGui::BeginTabBar("##variables", ImGuiTabBarFlags_TabListPopupButton | ImGuiTabBarFlags_FittingPolicyScroll);

	for (size_t effect_index = 0, id = 0; effect_index < _effects.size(); ++effect_index)
	{
		reshade::effect &effect = _effects[effect_index];

		// Hide variables that are not currently used in any of the active effects
		// Also skip showing this effect in the variable list if it doesn't have any uniform variables to show
		if (!effect.rendering || (effect.uniforms.empty() && effect.definitions.empty()))
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
		if (imgui::confirm_button(
				(ICON_FK_UNDO " " + std::string(_("Reset all to default"))).c_str(),
				_variable_editor_tabs ? ImGui::GetContentRegionAvail().x : ImGui::CalcItemWidth(),
				_("Do you really want to reset all values in '%s' to their defaults?"), effect_name.c_str()))
		{
			// Reset all uniform variables
			for (uniform &variable_it : effect.uniforms)
				if (variable_it.special == special_uniform::none && !variable_it.annotation_as_uint("noreset"))
					reset_uniform_value(variable_it);

			// Reset all preprocessor definitions
			if (const auto preset_it = _preset_preprocessor_definitions.find({});
				preset_it != _preset_preprocessor_definitions.end() && !preset_it->second.empty())
			{
				for (const std::pair<std::string, std::string> &definition : effect.definitions)
				{
					if (const auto it = std::remove_if(preset_it->second.begin(), preset_it->second.end(),
							[&definition](const std::pair<std::string, std::string> &preset_definition) { return preset_definition.first == definition.first; });
						it != preset_it->second.end())
					{
						preset_it->second.erase(it, preset_it->second.end());
						force_reload_effect = true; // Need to reload after changing preprocessor defines so to get accurate defaults again
					}
				}
			}

			if (const auto preset_it = _preset_preprocessor_definitions.find(effect_name);
				preset_it != _preset_preprocessor_definitions.end() && !preset_it->second.empty())
			{
				_preset_preprocessor_definitions.erase(preset_it);
				force_reload_effect = true;
			}

			if (_auto_save_preset)
				save_current_preset();
			else
				_preset_is_modified = true;
		}
		ImGui::PopStyleVar();

		bool category_closed = false;
		bool category_visible = true;
		std::string current_category;

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

				if (!current_category.empty())
				{
					std::string category_label(get_localized_annotation(variable, "ui_category", _current_language));
					if (!_variable_editor_tabs)
					{
						for (float x = 0, space_x = ImGui::CalcTextSize(" ").x, width = (ImGui::CalcItemWidth() - ImGui::CalcTextSize(category_label.data()).x - 45) / 2; x < width; x += space_x)
							category_label.insert(0, " ");
						category_label += "###" + current_category; // Ensure widget ID does not change with varying width
					}

					if (category_visible = true;
						variable.annotation_as_uint("ui_category_toggle") != 0)
						get_uniform_value(variable, &category_visible);

					if (category_visible)
					{
						ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_NoTreePushOnOpen;
						if (!variable.annotation_as_int("ui_category_closed"))
							flags |= ImGuiTreeNodeFlags_DefaultOpen;

						category_closed = !ImGui::TreeNodeEx(category_label.c_str(), flags);

						if (ImGui::BeginPopupContextItem(category_label.c_str()))
						{
							char temp[64];
							const int temp_size = ImFormatString(temp, sizeof(temp), _("Reset all in '%s' to default"), current_category.c_str());

							if (imgui::confirm_button(
									(ICON_FK_UNDO " " + std::string(temp, temp_size)).c_str(),
									ImGui::GetContentRegionAvail().x,
									_("Do you really want to reset all values in '%s' to their defaults?"), current_category.c_str()))
							{
								for (uniform &variable_it : effect.uniforms)
									if (variable_it.special == special_uniform::none && !variable_it.annotation_as_uint("noreset") &&
										variable_it.annotation_as_string("ui_category") == category)
										reset_uniform_value(variable_it);

								if (_auto_save_preset)
									save_current_preset();
								else
									_preset_is_modified = true;
							}

							ImGui::EndPopup();
						}
					}
					else
					{
						category_closed = false;
					}
				}
				else
				{
					category_closed = false;
					category_visible = true;
				}
			}

			// Skip rendering invisible items
			if (category_closed || (!category_visible && !variable.annotation_as_uint("ui_category_toggle")))
				continue;

			bool modified = false;
			bool is_default_value = true;

			ImGui::PushID(static_cast<int>(id++));

			reshadefx::constant value;
			switch (variable.type.base)
			{
			case reshadefx::type::t_bool:
				get_uniform_value(variable, value.as_uint, variable.type.components());
				for (size_t i = 0; is_default_value && i < variable.type.components(); i++)
					is_default_value = (value.as_uint[i] != 0) == (variable.initializer_value.as_uint[i] != 0);
				break;
			case reshadefx::type::t_int:
			case reshadefx::type::t_uint:
				get_uniform_value(variable, value.as_int, variable.type.components());
				is_default_value = std::memcmp(value.as_int, variable.initializer_value.as_int, variable.type.components() * sizeof(int)) == 0;
				break;
			case reshadefx::type::t_float:
				const float threshold = variable.annotation_as_float("ui_step", 0, 0.001f) * 0.75f + FLT_EPSILON;
				get_uniform_value(variable, value.as_float, variable.type.components());
				for (size_t i = 0; is_default_value && i < variable.type.components(); i++)
					is_default_value = std::abs(value.as_float[i] - variable.initializer_value.as_float[i]) < threshold;
				break;
			}

#if RESHADE_ADDON
			if (invoke_addon_event<addon_event::reshade_overlay_uniform_variable>(this, api::effect_uniform_variable{ reinterpret_cast<uintptr_t>(&variable) }))
			{
				reshadefx::constant new_value;
				switch (variable.type.base)
				{
				case reshadefx::type::t_bool:
					get_uniform_value(variable, new_value.as_uint, variable.type.components());
					for (size_t i = 0; !modified && i < variable.type.components(); i++)
						modified = (new_value.as_uint[i] != 0) != (value.as_uint[i] != 0);
					break;
				case reshadefx::type::t_int:
				case reshadefx::type::t_uint:
					get_uniform_value(variable, new_value.as_int, variable.type.components());
					modified = std::memcmp(new_value.as_int, value.as_int, variable.type.components() * sizeof(int)) != 0;
					break;
				case reshadefx::type::t_float:
					get_uniform_value(variable, new_value.as_float, variable.type.components());
					for (size_t i = 0; !modified && i < variable.type.components(); i++)
						modified = std::abs(new_value.as_float[i] - value.as_float[i]) > FLT_EPSILON;
					break;
				}
			}
			else
#endif
			{
				// Add spacing before variable widget
				for (int i = 0, spacing = variable.annotation_as_int("ui_spacing"); i < spacing; ++i)
					ImGui::Spacing();

				// Add user-configurable text before variable widget
				if (const std::string_view text = get_localized_annotation(variable, "ui_text", _current_language);
					!text.empty())
				{
					ImGui::PushTextWrapPos();
					ImGui::TextUnformatted(text.data(), text.data() + text.size());
					ImGui::PopTextWrapPos();
				}

				ImGui::BeginDisabled(variable.annotation_as_uint("noedit") != 0);

				std::string_view label = get_localized_annotation(variable, "ui_label", _current_language);
				if (label.empty())
					label = variable.name;
				const std::string_view ui_type = variable.annotation_as_string("ui_type");

				switch (variable.type.base)
				{
					case reshadefx::type::t_bool:
					{
						if (ui_type == "combo")
							modified = imgui::combo_with_buttons(label.data(), reinterpret_cast<bool &>(value.as_uint));
						else
							modified = ImGui::Checkbox(label.data(), reinterpret_cast<bool *>(value.as_uint));

						if (modified)
							set_uniform_value(variable, value.as_uint, variable.type.components());
						break;
					}
					case reshadefx::type::t_int:
					case reshadefx::type::t_uint:
					{
						const int ui_min_val = variable.annotation_as_int("ui_min", 0, ui_type == "slider" ? 0 : std::numeric_limits<int>::lowest());
						const int ui_max_val = variable.annotation_as_int("ui_max", 0, ui_type == "slider" ? 1 : std::numeric_limits<int>::max());
						const int ui_stp_val = variable.annotation_as_int("ui_step", 0, 1);

						// Append units
						std::string format = "%d";
						const std::string_view units = get_localized_annotation(variable, "ui_units", _current_language);
						format.append(units);

						if (ui_type == "slider")
							modified = imgui::slider_with_buttons(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, value.as_int, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, format.c_str());
						else if (ui_type == "drag")
							modified = variable.annotation_as_int("ui_step") == 0 ?
								ImGui::DragScalarN(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, value.as_int, variable.type.rows, 1.0f, &ui_min_val, &ui_max_val, format.c_str()) :
								imgui::drag_with_buttons(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, value.as_int, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, format.c_str());
						else if (ui_type == "list")
							modified = imgui::list_with_buttons(label.data(), get_localized_annotation(variable, "ui_items", _current_language), value.as_int[0]);
						else if (ui_type == "combo")
							modified = imgui::combo_with_buttons(label.data(), get_localized_annotation(variable, "ui_items", _current_language), value.as_int[0]);
						else if (ui_type == "radio")
							modified = imgui::radio_list(label.data(), get_localized_annotation(variable, "ui_items", _current_language), value.as_int[0]);
						else if (variable.type.is_matrix())
							for (unsigned int row = 0; row < variable.type.rows; ++row)
								modified = ImGui::InputScalarN((std::string(label) + " [row " + std::to_string(row) + ']').c_str(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, &value.as_int[variable.type.cols * row], variable.type.cols) || modified;
						else
							modified = ImGui::InputScalarN(label.data(), variable.type.is_signed() ? ImGuiDataType_S32 : ImGuiDataType_U32, value.as_int, variable.type.rows);

						if (modified)
							set_uniform_value(variable, value.as_int, variable.type.components());
						break;
					}
					case reshadefx::type::t_float:
					{
						const float ui_min_val = variable.annotation_as_float("ui_min", 0, ui_type == "slider" ? 0.0f : std::numeric_limits<float>::lowest());
						const float ui_max_val = variable.annotation_as_float("ui_max", 0, ui_type == "slider" ? 1.0f : std::numeric_limits<float>::max());
						const float ui_stp_val = variable.annotation_as_float("ui_step", 0, 0.001f);

						// Calculate display precision based on step value
						std::string precision_format = "%.0f";
						for (float x = 1.0f; x * ui_stp_val < 1.0f && precision_format[2] < '9'; x *= 10.0f)
							++precision_format[2]; // This changes the text to "%.1f", "%.2f", "%.3f", ...

						// Append units
						const std::string_view units = get_localized_annotation(variable, "ui_units", _current_language);
						precision_format.append(units);

						if (ui_type == "slider")
							modified = imgui::slider_with_buttons(label.data(), ImGuiDataType_Float, value.as_float, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, precision_format.c_str());
						else if (ui_type == "drag")
							modified = variable.annotation_as_float("ui_step") == 0.0f ?
								ImGui::DragScalarN(label.data(), ImGuiDataType_Float, value.as_float, variable.type.rows, ui_stp_val, &ui_min_val, &ui_max_val, precision_format.c_str()) :
								imgui::drag_with_buttons(label.data(), ImGuiDataType_Float, value.as_float, variable.type.rows, &ui_stp_val, &ui_min_val, &ui_max_val, precision_format.c_str());
						else if (ui_type == "color" && variable.type.rows == 1)
							modified = imgui::slider_for_alpha_value(label.data(), value.as_float);
						else if (ui_type == "color" && variable.type.rows == 3)
							modified = ImGui::ColorEdit3(label.data(), value.as_float, ImGuiColorEditFlags_NoOptions);
						else if (ui_type == "color" && variable.type.rows == 4)
							modified = ImGui::ColorEdit4(label.data(), value.as_float, ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaBar);
						else if (variable.type.is_matrix())
							for (unsigned int row = 0; row < variable.type.rows; ++row)
								modified = ImGui::InputScalarN((std::string(label) + " [row " + std::to_string(row) + ']').c_str(), ImGuiDataType_Float, &value.as_float[variable.type.cols * row], variable.type.cols) || modified;
						else
							modified = ImGui::InputScalarN(label.data(), ImGuiDataType_Float, value.as_float, variable.type.rows);

						if (modified)
							set_uniform_value(variable, value.as_float, variable.type.components());
						break;
					}
				}

				ImGui::EndDisabled();

				// Display tooltip
				if (const std::string_view tooltip = get_localized_annotation(variable, "ui_tooltip", _current_language);
					!tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
				{
					if (ImGui::BeginTooltip())
					{
						ImGui::TextUnformatted(tooltip.data(), tooltip.data() + tooltip.size());
						ImGui::EndTooltip();
					}
				}
			}

			if (ImGui::IsItemActive())
				active_variable = variable_index + 1;
			if (ImGui::IsItemHovered())
				hovered_variable = variable_index + 1;

			// Create context menu
			if (ImGui::BeginPopupContextItem("##context"))
			{
				ImGui::SetNextItemWidth(18.0f * _font_size);
				if (variable.supports_toggle_key() &&
					_input != nullptr &&
					imgui::key_input_box("##toggle_key", variable.toggle_key_data, *_input))
					modified = true;

				if (ImGui::Button((ICON_FK_UNDO " " + std::string(_("Reset to default"))).c_str(), ImVec2(18.0f * _font_size, 0)))
				{
					modified = true;
					reset_uniform_value(variable);
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if (!is_default_value && !variable.annotation_as_uint("noreset"))
			{
				ImGui::SameLine();
				if (ImGui::SmallButton(ICON_FK_UNDO))
				{
					modified = true;
					reset_uniform_value(variable);
				}
			}

			if (variable.toggle_key_data[0] != 0)
			{
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
				ImGui::TextDisabled("%s", input::key_name(variable.toggle_key_data).c_str());
			}

			ImGui::PopID();

			// A value has changed, so save the current preset
			if (modified && !variable.annotation_as_uint("nosave"))
			{
				if (_auto_save_preset)
					save_current_preset();
				else
					_preset_is_modified = true;
			}
		}

		if (active_variable_index < effect.uniforms.size())
			set_uniform_value(effect.uniforms[active_variable_index], static_cast<uint32_t>(active_variable));
		if (hovered_variable_index < effect.uniforms.size())
			set_uniform_value(effect.uniforms[hovered_variable_index], static_cast<uint32_t>(hovered_variable));

		// Draw preprocessor definition list after all uniforms of an effect file
		std::vector<std::pair<std::string, std::string>> &effect_definitions = _preset_preprocessor_definitions[effect_name];
		std::vector<std::pair<std::string, std::string>>::iterator modified_definition;

		if (!effect.definitions.empty())
		{
			std::string category_label = _("Preprocessor definitions");
			if (!_variable_editor_tabs)
			{
				for (float x = 0, space_x = ImGui::CalcTextSize(" ").x, width = (ImGui::CalcItemWidth() - ImGui::CalcTextSize(category_label.c_str()).x - 45) / 2; x < width; x += space_x)
					category_label.insert(0, " ");
				category_label += "###ppdefinitions";
			}

			if (ImGui::TreeNodeEx(category_label.c_str(), ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const std::pair<std::string, std::string> &definition : effect.definitions)
				{
					std::vector<std::pair<std::string, std::string>> *definition_scope = nullptr;
					std::vector<std::pair<std::string, std::string>>::iterator definition_it;

					char value[256];
					if (get_preprocessor_definition(effect_name, definition.first, 0b111, definition_scope, definition_it))
						value[definition_it->second.copy(value, sizeof(value) - 1)] = '\0';
					else
						value[0] = '\0';

					if (ImGui::InputTextWithHint(definition.first.c_str(), definition.second.c_str(), value, sizeof(value), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
					{
						if (value[0] == '\0') // An empty value removes the definition
						{
							if (definition_scope == &effect_definitions)
							{
								force_reload_effect = true;
								definition_scope->erase(definition_it);
							}
						}
						else
						{
							force_reload_effect = true;

							if (definition_scope == &effect_definitions)
							{
								definition_it->second = value;
								modified_definition = definition_it;
							}
							else
							{
								effect_definitions.emplace_back(definition.first, value);
								modified_definition = effect_definitions.end() - 1;
							}
						}
					}

					if (force_reload_effect) // Cannot compare iterators if definitions were just modified above
						continue;

					if (ImGui::BeginPopupContextItem())
					{
						if (ImGui::Button((ICON_FK_UNDO " " + std::string(_("Reset to default"))).c_str(), ImVec2(18.0f * _font_size, 0)))
						{
							if (definition_scope != nullptr)
							{
								force_reload_effect = true;
								definition_scope->erase(definition_it);
							}

							ImGui::CloseCurrentPopup();
						}

						ImGui::EndPopup();
					}

					if (definition_scope == &effect_definitions)
					{
						ImGui::SameLine();
						if (ImGui::SmallButton(ICON_FK_UNDO))
						{
							force_reload_effect = true;
							definition_scope->erase(definition_it);
						}
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

			const bool reload_successful_before = _last_reload_successful;

			// Reload current effect file
			if (!reload_effect(effect_index) && modified_definition != std::vector<std::pair<std::string, std::string>>::iterator())
			{
				// The preprocessor definition that was just modified caused the effect to not compile, so reset to default and try again
				effect_definitions.erase(modified_definition);

				if (reload_effect(effect_index))
				{
					_last_reload_successful = reload_successful_before;
					ImGui::OpenPopup("##pperror"); // Notify the user about this

					// Update preset again now, so that the removed preprocessor definition does not reappear on a reload
					// The preset is actually loaded again next frame to update the technique status (see 'update_effects'), so cannot use 'save_current_preset' here
					ini_file::load_cache(_current_preset_path).set(effect_name, "PreprocessorDefinitions", effect_definitions);
				}
			}

			// Reloading an effect file invalidates all textures, but the statistics window may already have drawn references to those, so need to reset it
			if (ImGuiWindow *const statistics_window = ImGui::FindWindowByName("###statistics"))
				statistics_window->DrawList->CmdBuffer.clear();
		}
	}

	if (ImGui::BeginPopup("##pperror"))
	{
		ImGui::TextColored(COLOR_RED, _("The effect failed to compile after this change, so reverted back to the default value."));
		ImGui::EndPopup();
	}

	if (_variable_editor_tabs)
		ImGui::EndTabBar();
	ImGui::EndChild();
}
void reshade::runtime::draw_technique_editor()
{
	if (_reload_count != 0 && _effects.empty())
	{
		ImGui::TextColored(COLOR_YELLOW, _("No effect files (.fx) found in the configured effect search paths%c"), _effect_search_paths.empty() ? '.' : ':');
		for (const std::filesystem::path &search_path : _effect_search_paths)
			ImGui::TextColored(COLOR_YELLOW, "  %s", (g_reshade_base_path / search_path).lexically_normal().u8string().c_str());
		ImGui::Spacing();
		ImGui::TextColored(COLOR_YELLOW, _("Please verify they are set up correctly in the settings and hit 'Reload'!"));
		return;
	}

	if (!_last_reload_successful)
	{
		// Add fake items at the top for effects that failed to compile
		for (size_t effect_index = 0; effect_index < _effects.size(); ++effect_index)
		{
			const reshade::effect &effect = _effects[effect_index];

			if (effect.compiled || effect.skipped)
				continue;

			ImGui::PushID(static_cast<int>(_technique_sorting.size() + effect_index));

			ImGui::PushStyleColor(ImGuiCol_Text, COLOR_RED);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

			char label[128] = "";
			ImFormatString(label, sizeof(label), _("[%s] failed to compile"), effect.source_file.filename().u8string().c_str());

			bool value = false;
			ImGui::Checkbox(label, &value);

			ImGui::PopItemFlag();

			// Display tooltip
			if (!effect.errors.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_ForTooltip))
			{
				if (ImGui::BeginTooltip())
				{
					ImGui::TextUnformatted(effect.errors.c_str(), effect.errors.c_str() + effect.errors.size());
					ImGui::EndTooltip();
				}
			}

			ImGui::PopStyleColor();

			// Create context menu
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::OpenPopup("##context", ImGuiPopupFlags_MouseButtonRight);

			if (ImGui::BeginPopup("##context"))
			{
				if (ImGui::Button((ICON_FK_FOLDER " " + std::string(_("Open folder in explorer"))).c_str(), ImVec2(18.0f * _font_size, 0)))
					utils::open_explorer(effect.source_file);

				ImGui::Separator();

				if (imgui::popup_button((ICON_FK_PENCIL " " + std::string(_("Edit source code"))).c_str(), 18.0f * _font_size))
				{
					std::unordered_map<std::string_view, std::string> file_errors_lookup;
					parse_errors(effect.errors,
						[&file_errors_lookup](const std::string_view file, int line, const std::string_view message) {
							file_errors_lookup[file] += std::string(file) + '(' + std::to_string(line) + "): " + std::string(message) + '\n';
						});

					const auto source_file_errors_it = file_errors_lookup.find(effect.source_file.u8string());
					if (source_file_errors_it != file_errors_lookup.end())
						ImGui::PushStyleColor(ImGuiCol_Text, source_file_errors_it->second.find("error") != std::string::npos ? COLOR_RED : COLOR_YELLOW);

					std::filesystem::path source_file;
					if (ImGui::MenuItem(effect.source_file.filename().u8string().c_str()))
						source_file = effect.source_file;

					if (source_file_errors_it != file_errors_lookup.end())
					{
						if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
						{
							if (ImGui::BeginTooltip())
							{
								ImGui::TextUnformatted(source_file_errors_it->second.c_str(), source_file_errors_it->second.c_str() + source_file_errors_it->second.size());
								ImGui::EndTooltip();
							}
						}

						ImGui::PopStyleColor();
					}

					if (!effect.included_files.empty())
					{
						ImGui::Separator();

						for (const std::filesystem::path &included_file : effect.included_files)
						{
							// Color file entries that contain warnings or errors
							const auto included_file_errors_it = file_errors_lookup.find(included_file.u8string());
							if (included_file_errors_it != file_errors_lookup.end())
								ImGui::PushStyleColor(ImGuiCol_Text, included_file_errors_it->second.find("error") != std::string::npos ? COLOR_RED : COLOR_YELLOW);

							std::filesystem::path display_path = included_file.lexically_relative(effect.source_file.parent_path());
							if (display_path.empty())
								display_path = included_file.filename();
							if (ImGui::MenuItem(display_path.u8string().c_str()))
								source_file = included_file;

							if (included_file_errors_it != file_errors_lookup.end())
							{
								if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
								{
									if (ImGui::BeginTooltip())
									{
										ImGui::TextUnformatted(included_file_errors_it->second.c_str(), included_file_errors_it->second.c_str() + included_file_errors_it->second.size());
										ImGui::EndTooltip();
									}
								}

								ImGui::PopStyleColor();
							}
						}
					}

					ImGui::EndPopup();

					if (!source_file.empty())
					{
						open_code_editor(effect_index, source_file);
						ImGui::CloseCurrentPopup();
					}
				}

				if (_renderer_id < 0x20000 && // Hide if using SPIR-V, since that cannot easily be shown here
					imgui::popup_button(_("Show compiled results"), 18.0f * _font_size))
				{
					const bool open_generated_code = ImGui::MenuItem(_("Generated code"));

					ImGui::EndPopup();

					if (open_generated_code)
					{
						open_code_editor(effect_index, std::string());
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

	for (size_t index = 0; index < _technique_sorting.size(); ++index)
	{
		const size_t technique_index = _technique_sorting[index];
		{
			reshade::technique &tech = _techniques[technique_index];

			// Skip hidden techniques
			if (tech.hidden || !_effects[tech.effect_index].compiled)
				continue;

			bool modified = false;

			ImGui::PushID(static_cast<int>(index));

			// Look up effect that contains this technique
			const reshade::effect &effect = _effects[tech.effect_index];

			// Draw border around the item if it is selected
			const bool draw_border = _selected_technique == index;
			if (draw_border)
				ImGui::Separator();

			// Prevent user from disabling the technique when it is set to always be enabled via annotation
			const bool force_enabled = tech.annotation_as_int("enabled");

#if RESHADE_ADDON
			if (bool was_enabled = tech.enabled;
				invoke_addon_event<addon_event::reshade_overlay_technique>(this, api::effect_technique { reinterpret_cast<uintptr_t>(&tech) }))
			{
				modified = tech.enabled != was_enabled;
			}
			else
#endif
			{
				ImGui::BeginDisabled(tech.annotation_as_uint("noedit") != 0);

				// Gray out disabled techniques
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(tech.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled));

				std::string label(get_localized_annotation(tech, "ui_label", _current_language));
				if (label.empty())
					label = tech.name;
				label += " [" + effect.source_file.filename().u8string() + ']';

				if (bool status = tech.enabled;
					ImGui::Checkbox(label.c_str(), &status) && !force_enabled)
				{
					modified = true;

					if (status)
						enable_technique(tech);
					else
						disable_technique(tech);
				}

				ImGui::PopStyleColor();

				ImGui::EndDisabled();

				// Display tooltip
				if (const std::string_view tooltip = get_localized_annotation(tech, "ui_tooltip", _current_language);
					!tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
				{
					if (ImGui::BeginTooltip())
					{
						ImGui::TextUnformatted(tooltip.data(), tooltip.data() + tooltip.size());
						ImGui::EndTooltip();
					}
				}
			}

			if (ImGui::IsItemActive())
				_selected_technique = index;
			if (ImGui::IsItemClicked())
				_focused_effect = tech.effect_index;
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly | ImGuiHoveredFlags_AllowWhenDisabled))
				hovered_technique_index = index;

			// Create context menu
			if (ImGui::BeginPopupContextItem("##context"))
			{
				ImGui::TextUnformatted(tech.name.c_str(), tech.name.c_str() + tech.name.size());
				ImGui::Separator();

				ImGui::SetNextItemWidth(18.0f * _font_size);
				if (_input != nullptr && !force_enabled &&
					imgui::key_input_box("##toggle_key", tech.toggle_key_data, *_input))
				{
					if (_auto_save_preset)
						save_current_preset();
					else
						_preset_is_modified = true;
				}

				const bool is_not_top = index > 0;
				const bool is_not_bottom = index < _technique_sorting.size() - 1;

				if (is_not_top && ImGui::Button(_("Move to top"), ImVec2(18.0f * _font_size, 0)))
				{
					std::vector<size_t> technique_indices = _technique_sorting;
					technique_indices.insert(technique_indices.begin(), technique_indices[index]);
					technique_indices.erase(technique_indices.begin() + 1 + index);
					reorder_techniques(std::move(technique_indices));

					if (_auto_save_preset)
						save_current_preset();
					else
						_preset_is_modified = true;

					ImGui::CloseCurrentPopup();
				}
				if (is_not_bottom && ImGui::Button(_("Move to bottom"), ImVec2(18.0f * _font_size, 0)))
				{
					std::vector<size_t> technique_indices = _technique_sorting;
					technique_indices.push_back(technique_indices[index]);
					technique_indices.erase(technique_indices.begin() + index);
					reorder_techniques(std::move(technique_indices));

					if (_auto_save_preset)
						save_current_preset();
					else
						_preset_is_modified = true;

					ImGui::CloseCurrentPopup();
				}

				if (is_not_top || is_not_bottom || (_input != nullptr && !force_enabled))
					ImGui::Separator();

				if (ImGui::Button((ICON_FK_FOLDER " " + std::string(_("Open folder in explorer"))).c_str(), ImVec2(18.0f * _font_size, 0)))
					utils::open_explorer(effect.source_file);

				ImGui::Separator();

				if (imgui::popup_button((ICON_FK_PENCIL " " + std::string(_("Edit source code"))).c_str(), 18.0f * _font_size))
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
						{
							std::filesystem::path display_path = included_file.lexically_relative(effect.source_file.parent_path());
							if (display_path.empty())
								display_path = included_file.filename();
							if (ImGui::MenuItem(display_path.u8string().c_str()))
								source_file = included_file;
						}
					}

					ImGui::EndPopup();

					if (!source_file.empty())
					{
						open_code_editor(tech.effect_index, source_file);
						ImGui::CloseCurrentPopup();
					}
				}

				if (_renderer_id < 0x20000 && // Hide if using SPIR-V, since that cannot easily be shown here
					imgui::popup_button(_("Show compiled results"), 18.0f * _font_size))
				{
					const bool open_generated_code = ImGui::MenuItem(_("Generated code"));

					ImGui::Separator();

					std::string entry_point_name;
					for (const reshadefx::entry_point &entry_point : effect.module.entry_points)
						if (const auto assembly_it = effect.assembly_text.find(entry_point.name);
							assembly_it != effect.assembly_text.end() && ImGui::MenuItem(entry_point.name.c_str()))
							entry_point_name = entry_point.name;

					ImGui::EndPopup();

					if (open_generated_code || !entry_point_name.empty())
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

			if (modified)
			{
				if (_auto_save_preset)
					save_current_preset();
				else
					_preset_is_modified = true;
			}
		}
	}

	// Move the selected technique to the position of the mouse in the list
	if (_selected_technique < _technique_sorting.size() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		if (hovered_technique_index < _technique_sorting.size() && hovered_technique_index != _selected_technique)
		{
			std::vector<size_t> technique_indices = _technique_sorting;

			const auto move_technique = [this, &technique_indices](size_t from_index, size_t to_index) {
				if (to_index < from_index) // Up
					for (size_t i = from_index; to_index < i; --i)
						std::swap(technique_indices[i - 1], technique_indices[i]);
				else // Down
					for (size_t i = from_index; i < to_index; ++i)
						std::swap(technique_indices[i], technique_indices[i + 1]);
			};

			move_technique(_selected_technique, hovered_technique_index);

			// Pressing shift moves all techniques from the same effect file to the new location as well
			if (_imgui_context->IO.KeyShift)
			{
				for (size_t i = hovered_technique_index + 1, offset = 1; i < technique_indices.size(); ++i)
				{
					if (_techniques[technique_indices[i]].effect_index == _focused_effect)
					{
						if ((i - hovered_technique_index) > offset)
							move_technique(i, hovered_technique_index + offset);
						offset++;
					}
				}
				for (size_t i = hovered_technique_index - 1, offset = 0; i >= 0 && i != std::numeric_limits<size_t>::max(); --i)
				{
					if (_techniques[technique_indices[i]].effect_index == _focused_effect)
					{
						offset++;
						if ((hovered_technique_index - i) > offset)
							move_technique(i, hovered_technique_index - offset);
					}
				}
			}

			reorder_techniques(std::move(technique_indices));
			_selected_technique = hovered_technique_index;

			if (_auto_save_preset)
				save_current_preset();
			else
				_preset_is_modified = true;
			return;
		}
	}
	else
	{
		_selected_technique = std::numeric_limits<size_t>::max();
	}

	if (force_reload_effect != std::numeric_limits<size_t>::max())
	{
		reload_effect(force_reload_effect);

		// Reloading an effect file invalidates all textures, but the statistics window may already have drawn references to those, so need to reset it
		if (ImGuiWindow *const statistics_window = ImGui::FindWindowByName("###statistics"))
			statistics_window->DrawList->CmdBuffer.clear();
	}
}

void reshade::runtime::open_code_editor(size_t effect_index, const std::string &entry_point)
{
	assert(effect_index < _effects.size());

	const std::filesystem::path &path = _effects[effect_index].source_file;

	if (const auto it = std::find_if(_editors.begin(), _editors.end(),
			[effect_index, &path, &entry_point](const editor_instance &instance) {
				return instance.effect_index == effect_index && instance.file_path == path && instance.generated && instance.entry_point_name == entry_point;
			});
		it != _editors.end())
	{
		it->selected = true;
		open_code_editor(*it);
	}
	else
	{
		editor_instance instance { effect_index, path, entry_point, true, true };
		open_code_editor(instance);
		_editors.push_back(std::move(instance));
	}
}
void reshade::runtime::open_code_editor(size_t effect_index, const std::filesystem::path &path)
{
	assert(effect_index < _effects.size());

	if (const auto it = std::find_if(_editors.begin(), _editors.end(),
			[effect_index, &path](const editor_instance &instance) {
				return instance.effect_index == effect_index && instance.file_path == path && !instance.generated;
			});
		it != _editors.end())
	{
		it->selected = true;
		open_code_editor(*it);
	}
	else
	{
		editor_instance instance { effect_index, path, std::string(), true, false };
		open_code_editor(instance);
		_editors.push_back(std::move(instance));
	}
}
void reshade::runtime::open_code_editor(editor_instance &instance) const
{
	const effect &effect = _effects[instance.effect_index];

	if (instance.generated)
	{
		if (instance.entry_point_name.empty())
			instance.editor.set_text(std::string_view(effect.module.code.data(), effect.module.code.size()));
		else
			instance.editor.set_text(effect.assembly_text.at(instance.entry_point_name));
		instance.editor.set_readonly(true);
		return; // Errors only apply to the effect source, not generated code
	}

	// Only update text if there is no undo history (in which case it can be assumed that the text is already up-to-date)
	if (!instance.editor.is_modified() && !instance.editor.can_undo())
	{
		if (auto file = std::ifstream(instance.file_path))
		{
			instance.editor.set_text(std::string(std::istreambuf_iterator<char>(file), {}));
			instance.editor.set_readonly(false);
		}
	}

	instance.editor.clear_errors();

	parse_errors(effect.errors,
		[&instance](const std::string_view file, int line, const std::string_view message) {
			// Ignore errors that aren't in the current source file
			if (file != instance.file_path.u8string())
				return;

			instance.editor.add_error(line, message, message.find("error") == std::string::npos);
		});
}
void reshade::runtime::draw_code_editor(editor_instance &instance)
{
	if (!instance.generated && (
			ImGui::Button((ICON_FK_FLOPPY " " + std::string(_("Save"))).c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)) || (
			_input != nullptr && _input->is_key_pressed('S', true, false, false))))
	{
		// Write current editor text to file
		if (auto file = std::ofstream(instance.file_path, std::ios::trunc))
		{
			const std::string text = instance.editor.get_text();
			file.write(text.data(), text.size());
		}

		if (!is_loading() && instance.effect_index < _effects.size())
		{
			// Clear modified flag, so that errors are updated next frame (see 'update_and_render_effects')
			instance.editor.clear_modified();

			reload_effect(instance.effect_index);

			// Reloading an effect file invalidates all textures, but the statistics window may already have drawn references to those, so need to reset it
			if (ImGuiWindow *const statistics_window = ImGui::FindWindowByName("###statistics"))
				statistics_window->DrawList->CmdBuffer.clear();
		}
	}

	instance.editor.render("##editor", _editor_palette, false, _imgui_context->IO.Fonts->Fonts[_imgui_context->IO.Fonts->Fonts.Size - 1]);

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
	// Adjust default font size based on the vertical resolution
	if (_font_size == 0)
		_editor_font_size = _font_size = _height >= 2160 ? 26 : _height >= 1440 ? 20 : 13;

	const bool has_combined_sampler_and_view = _device->check_capability(api::device_caps::sampler_with_resource_view);

	if (_imgui_sampler_state == 0)
	{
		api::sampler_desc sampler_desc = {};
		sampler_desc.filter = api::filter_mode::min_mag_mip_linear;
		sampler_desc.address_u = api::texture_address_mode::clamp;
		sampler_desc.address_v = api::texture_address_mode::clamp;
		sampler_desc.address_w = api::texture_address_mode::clamp;

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

		uint32_t num_push_constants = 16;
		reshade::api::shader_stage shader_stage = api::shader_stage::vertex;

		// Add HDR push constants for possible HDR swap chains
		if (((_renderer_id & 0xB000) == 0xB000 || (_renderer_id & 0xC000) == 0xC000 || (_renderer_id & 0x20000) == 0x20000) &&
			(_back_buffer_format == reshade::api::format::r10g10b10a2_unorm || _back_buffer_format == reshade::api::format::b10g10r10a2_unorm || _back_buffer_format == reshade::api::format::r16g16b16a16_float))
		{
			num_push_constants += 4;
			shader_stage |= api::shader_stage::pixel;
		}

		layout_params[num_layout_params++] = api::constant_range { 0, 0, 0, num_push_constants, shader_stage }; // b0

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
		const bool is_possibe_hdr_swapchain =
			((_renderer_id & 0xB000) == 0xB000 || (_renderer_id & 0xC000) == 0xC000 || (_renderer_id & 0x20000) == 0x20000) &&
			(_back_buffer_format == reshade::api::format::r10g10b10a2_unorm || _back_buffer_format == reshade::api::format::b10g10r10a2_unorm || _back_buffer_format == reshade::api::format::r16g16b16a16_float);

		const resources::data_resource vs_res = resources::load_data_resource(_renderer_id >= 0x20000 ? IDR_IMGUI_VS_SPIRV : _renderer_id < 0xa000 ? IDR_IMGUI_VS_3_0 : IDR_IMGUI_VS_4_0);
		vs_desc.code = vs_res.data;
		vs_desc.code_size = vs_res.data_size;

		const resources::data_resource ps_res = resources::load_data_resource(_renderer_id >= 0x20000 ? !is_possibe_hdr_swapchain ? IDR_IMGUI_PS_SPIRV : IDR_IMGUI_PS_SPIRV_HDR : _renderer_id < 0xa000 ? IDR_IMGUI_PS_3_0 : !is_possibe_hdr_swapchain ? IDR_IMGUI_PS_4_0 : IDR_IMGUI_PS_4_0_HDR);
		ps_desc.code = ps_res.data;
		ps_desc.code_size = ps_res.data_size;
	}
	else
	{
		assert(_device->get_api() == api::device_api::opengl);

		// These need to be static so that the shader source memory doesn't fall out of scope before pipeline creation below
		static const char vertex_shader_code[] =
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
		static const char fragment_shader_code[] =
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

	api::primitive_topology topology = api::primitive_topology::triangle_list;
	subobjects.push_back({ api::pipeline_subobject_type::primitive_topology, 1, &topology });

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

	// Always choose non-sRGB format variant, since 'render_imgui_draw_data' is called with the non-sRGB render target (see 'draw_gui')
	api::format render_target_format = api::format_to_default_typed(_back_buffer_format, 0);
	subobjects.push_back({ api::pipeline_subobject_type::render_target_formats, 1, &render_target_format });

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
	const size_t buffer_index = _frame_count % std::size(_imgui_vertices);

	// Create and grow vertex/index buffers if needed
	if (_imgui_num_indices[buffer_index] < draw_data->TotalIdxCount)
	{
		if (_imgui_indices[buffer_index] != 0)
		{
			_graphics_queue->wait_idle(); // Be safe and ensure nothing still uses this buffer

			_device->destroy_resource(_imgui_indices[buffer_index]);
		}

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
		if (_imgui_vertices[buffer_index] != 0)
		{
			_graphics_queue->wait_idle();

			_device->destroy_resource(_imgui_vertices[buffer_index]);
		}

		const int new_size = draw_data->TotalVtxCount + 5000;
		if (!_device->create_resource(api::resource_desc(new_size * sizeof(ImDrawVert), api::memory_heap::cpu_to_gpu, api::resource_usage::vertex_buffer), nullptr, api::resource_usage::cpu_access, &_imgui_vertices[buffer_index]))
		{
			LOG(ERROR) << "Failed to create ImGui vertex buffer!";
			return;
		}

		_device->set_resource_name(_imgui_vertices[buffer_index], "ImGui vertex buffer");

		_imgui_num_vertices[buffer_index] = new_size;
	}

#ifndef NDEBUG
	cmd_list->begin_debug_event("ReShade overlay");
#endif

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
		cmd_list->push_descriptors(api::shader_stage::pixel, _imgui_pipeline_layout, 0, api::descriptor_table_update { {}, 0, 0, 1, api::descriptor_type::sampler, &_imgui_sampler_state });

	// Add HDR push constants for possible HDR swap chains
	if (((_renderer_id & 0xB000) == 0xB000 || (_renderer_id & 0xC000) == 0xC000 || (_renderer_id & 0x20000) == 0x20000) &&
		(_back_buffer_format == reshade::api::format::r10g10b10a2_unorm || _back_buffer_format == reshade::api::format::b10g10r10a2_unorm || _back_buffer_format == reshade::api::format::r16g16b16a16_float))
	{
		const struct {
			api::format back_buffer_format;
			api::color_space back_buffer_color_space;
			float hdr_overlay_brightness;
			api::color_space hdr_overlay_overwrite_color_space;
		} hdr_push_constants = {
			_back_buffer_format,
			_back_buffer_color_space,
			_hdr_overlay_brightness,
			_hdr_overlay_overwrite_color_space
		};

		cmd_list->push_constants(api::shader_stage::pixel, _imgui_pipeline_layout, has_combined_sampler_and_view ? 1 : 2, sizeof(ortho_projection) / 4, sizeof(hdr_push_constants) / 4, &hdr_push_constants);
	}

	int vtx_offset = 0, idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; ++n)
	{
		const ImDrawList *const draw_list = draw_data->CmdLists[n];

		for (const ImDrawCmd &cmd : draw_list->CmdBuffer)
		{
			if (cmd.UserCallback != nullptr)
			{
				cmd.UserCallback(draw_list, &cmd);
				continue;
			}

			assert(cmd.TextureId != 0);

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
				cmd_list->push_descriptors(api::shader_stage::pixel, _imgui_pipeline_layout, 0, api::descriptor_table_update { {}, 0, 0, 1, api::descriptor_type::sampler_with_resource_view, &sampler_and_view });
			}
			else
			{
				cmd_list->push_descriptors(api::shader_stage::pixel, _imgui_pipeline_layout, 1, api::descriptor_table_update { {}, 0, 0, 1, api::descriptor_type::shader_resource_view, &srv });
			}

			cmd_list->draw_indexed(cmd.ElemCount, 1, cmd.IdxOffset + idx_offset, cmd.VtxOffset + vtx_offset, 0);
		}

		idx_offset += draw_list->IdxBuffer.Size;
		vtx_offset += draw_list->VtxBuffer.Size;
	}

	cmd_list->end_render_pass();

#ifndef NDEBUG
	cmd_list->end_debug_event();
#endif
}
void reshade::runtime::destroy_imgui_resources()
{
	_imgui_context->IO.Fonts->Clear();

	_device->destroy_resource(_font_atlas_tex);
	_font_atlas_tex = {};
	_device->destroy_resource_view(_font_atlas_srv);
	_font_atlas_srv = {};

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

bool reshade::runtime::open_overlay(bool open, api::input_source source)
{
#if RESHADE_ADDON
	if (!_is_in_api_call)
	{
		_is_in_api_call = true;
		const bool skip = invoke_addon_event<addon_event::reshade_open_overlay>(this, open, source);
		_is_in_api_call = false;
		if (skip)
			return false;
	}
#endif

	_show_overlay = open;

	if (open)
		_imgui_context->NavInputSource = static_cast<ImGuiInputSource>(source);

	return true;
}

#endif
