/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "version.h"
#include "dll_log.hpp"
#include "ini_file.hpp"
#include "addon_manager.hpp"
#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include "effect_preprocessor.hpp"
#include "input.hpp"
#include "input_freepie.hpp"
#include "com_ptr.hpp"
#include <set>
#include <thread>
#include <algorithm>
#include <cstring>
#include <stb_image.h>
#include <stb_image_dds.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <malloc.h>
#include <d3dcompiler.h>

#if RESHADE_FX
bool resolve_path(std::filesystem::path &path)
{
	std::error_code ec;
	// First convert path to an absolute path
	// Ignore the working directory and instead start relative paths at the DLL location
	if (!path.is_absolute())
		path = std::filesystem::absolute(g_reshade_base_path / path, ec);
	// Finally try to canonicalize the path too
	if (auto canonical_path = std::filesystem::canonical(path, ec); !ec)
		path = std::move(canonical_path);
	return !ec; // The canonicalization step fails if the path does not exist
}
bool resolve_preset_path(std::filesystem::path &path)
{
	// First make sure the extension matches, before diving into the file system
	if (const std::filesystem::path ext = path.extension();
		ext != L".ini" && ext != L".txt")
		return false;
	// A non-existent path is valid for a new preset
	// Otherwise ensure the file has a technique list, which should make it a preset
	return !resolve_path(path) || ini_file::load_cache(path).has({}, "Techniques");
}

static bool find_file(const std::vector<std::filesystem::path> &search_paths, std::filesystem::path &path)
{
	std::error_code ec;
	// Do not have to perform a search if the path is already absolute
	if (path.is_absolute())
		return std::filesystem::exists(path, ec);
	for (std::filesystem::path search_path : search_paths)
		// Append relative file path to absolute search path
		if (search_path /= path; resolve_path(search_path))
			return path = std::move(search_path), true;
	return false;
}
static std::vector<std::filesystem::path> find_files(const std::vector<std::filesystem::path> &search_paths, std::initializer_list<std::filesystem::path> extensions)
{
	std::error_code ec;
	std::vector<std::filesystem::path> files;
	for (std::filesystem::path search_path : search_paths)
		if (resolve_path(search_path))
			for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(search_path, std::filesystem::directory_options::skip_permission_denied, ec))
				if (!entry.is_directory(ec) &&
					std::find(extensions.begin(), extensions.end(), entry.path().extension()) != extensions.end())
					files.emplace_back(entry); // Construct path from directory entry in-place
	return files;
}

static inline int format_color_bit_depth(reshade::api::format value)
{
	// Only need to handle swap chain formats
	switch (value)
	{
	default:
		return 0;
	case reshade::api::format::b5g6r5_unorm:
	case reshade::api::format::b5g5r5a1_unorm:
		return 5;
	case reshade::api::format::r8g8b8a8_unorm:
	case reshade::api::format::r8g8b8a8_unorm_srgb:
	case reshade::api::format::r8g8b8x8_unorm:
	case reshade::api::format::r8g8b8x8_unorm_srgb:
	case reshade::api::format::b8g8r8a8_unorm:
	case reshade::api::format::b8g8r8a8_unorm_srgb:
	case reshade::api::format::b8g8r8x8_unorm:
	case reshade::api::format::b8g8r8x8_unorm_srgb:
		return 8;
	case reshade::api::format::r10g10b10a2_unorm:
	case reshade::api::format::r10g10b10a2_xr_bias:
	case reshade::api::format::b10g10r10a2_unorm:
		return 10;
	case reshade::api::format::r16g16b16a16_float:
		return 16;
	}
}
#endif

reshade::runtime::runtime(api::device *device, api::command_queue *graphics_queue) :
	_device(device),
	_graphics_queue(graphics_queue),
	_start_time(std::chrono::high_resolution_clock::now()),
	_last_present_time(std::chrono::high_resolution_clock::now()),
	_last_frame_duration(std::chrono::milliseconds(1)),
#if RESHADE_FX
	_effect_search_paths({ L".\\" }),
	_texture_search_paths({ L".\\" }),
#endif
	_config_path(g_reshade_base_path / L"ReShade.ini"),
	_screenshot_path(g_reshade_base_path)
{
	assert(device != nullptr && graphics_queue != nullptr);

	_needs_update = check_for_update(_latest_version);

	// Default shortcut PrtScrn
	_screenshot_key_data[0] = 0x2C;

	// Fall back to alternative configuration file name if it exists
	std::error_code ec;
	if (std::filesystem::path config_path_alt = g_reshade_base_path / g_reshade_dll_path.filename().replace_extension(L".ini");
		std::filesystem::exists(config_path_alt, ec) && !std::filesystem::exists(_config_path, ec))
		_config_path = std::move(config_path_alt);

#if RESHADE_GUI
	init_gui();
#endif

	load_config();
}
reshade::runtime::~runtime()
{
#if RESHADE_FX
	assert(_worker_threads.empty());
	assert(!_is_initialized && _techniques.empty());
#endif

	// Save configuration before shutting down to ensure the current window state is written to disk
	save_config();

#if RESHADE_GUI
	 deinit_gui();
#endif
}

bool reshade::runtime::on_init(input::window_handle window)
{
	assert(!_is_initialized);

#if RESHADE_FX
	// Create an empty texture, which is bound to shader resource view slots with an unknown semantic (since it is not valid to bind a zero handle in Vulkan, unless the 'VK_EXT_robustness2' extension is enabled)
	if (_empty_tex == 0)
	{
		// Use VK_FORMAT_R16_SFLOAT format, since it is mandatory according to the spec (see https://www.khronos.org/registry/vulkan/specs/1.1/html/vkspec.html#features-required-format-support)
		if (!_device->create_resource(
				api::resource_desc(1, 1, 1, 1, api::format::r16_float, 1, api::memory_heap::gpu_only, api::resource_usage::shader_resource),
				nullptr, api::resource_usage::shader_resource, &_empty_tex))
		{
			LOG(ERROR) << "Failed to create empty texture resource!";
			goto exit_failure;
		}

		_device->set_resource_name(_empty_tex, "ReShade empty texture");

		if (!_device->create_resource_view(_empty_tex, api::resource_usage::shader_resource, api::resource_view_desc(api::format::r16_float), &_empty_srv))
		{
			LOG(ERROR) << "Failed to create empty texture resource view!";
			goto exit_failure;
		}
	}

	// Create back buffer resource
	if (_effect_color_tex == 0)
	{
		if (!_device->create_resource(
				api::resource_desc(_width, _height, 1, 1, api::format_to_typeless(_back_buffer_format), 1, api::memory_heap::gpu_only, api::resource_usage::copy_dest | api::resource_usage::shader_resource),
				nullptr, api::resource_usage::shader_resource, &_effect_color_tex))
		{
			LOG(ERROR) << "Failed to create back buffer resource!";
			goto exit_failure;
		}

		_device->set_resource_name(_effect_color_tex, "ReShade back buffer");

		if (!_device->create_resource_view(_effect_color_tex, api::resource_usage::shader_resource, api::resource_view_desc(api::format_to_default_typed(_back_buffer_format, 0)), &_effect_color_srv[0]) ||
			!_device->create_resource_view(_effect_color_tex, api::resource_usage::shader_resource, api::resource_view_desc(api::format_to_default_typed(_back_buffer_format, 1)), &_effect_color_srv[1]))
		{
			LOG(ERROR) << "Failed to create back buffer resource view!";
			goto exit_failure;
		}
	}

	// Create effect stencil resource
	if (_effect_stencil_tex == 0)
	{
		// Find a supported stencil format with the smallest footprint (since the depth component is not used)
		constexpr api::format possible_stencil_formats[] = {
			api::format::s8_uint,
			api::format::d16_unorm_s8_uint,
			api::format::d24_unorm_s8_uint,
			api::format::d32_float_s8_uint
		};

		for (const api::format format : possible_stencil_formats)
		{
			if (_device->check_format_support(format, api::resource_usage::depth_stencil))
			{
				_effect_stencil_format = format;
				break;
			}
		}

		assert(_effect_stencil_format != api::format::unknown);

		if (!_device->create_resource(
				api::resource_desc(_width, _height, 1, 1, _effect_stencil_format, 1, api::memory_heap::gpu_only, api::resource_usage::depth_stencil),
				nullptr, api::resource_usage::depth_stencil_write, &_effect_stencil_tex))
		{
			LOG(ERROR) << "Failed to create effect stencil resource!";
			goto exit_failure;
		}

		_device->set_resource_name(_effect_stencil_tex, "ReShade effect stencil");

		if (!_device->create_resource_view(_effect_stencil_tex, api::resource_usage::depth_stencil, api::resource_view_desc(_effect_stencil_format), &_effect_stencil_dsv))
		{
			LOG(ERROR) << "Failed to create effect stencil resource view!";
			goto exit_failure;
		}
	}
#endif

	// Create render targets for the back buffer resources
	for (uint32_t i = 0; i < get_back_buffer_count(); ++i)
	{
		const api::resource back_buffer_resource = get_back_buffer_resolved(i);

		if (!_device->create_resource_view(back_buffer_resource, api::resource_usage::render_target, api::resource_view_desc(api::format_to_default_typed(_back_buffer_format, 0)), &_back_buffer_targets.emplace_back()) ||
			!_device->create_resource_view(back_buffer_resource, api::resource_usage::render_target, api::resource_view_desc(api::format_to_default_typed(_back_buffer_format, 1)), &_back_buffer_targets.emplace_back()))
		{
			LOG(ERROR) << "Failed to create back buffer render targets!";
			goto exit_failure;
		}
	}

#if RESHADE_GUI
	if (window != nullptr)
	{
		RECT window_rect = {};
		GetClientRect(static_cast<HWND>(window), &window_rect);

		_window_width = window_rect.right;
		_window_height = window_rect.bottom;
	}
	else
	{
		_window_width = _width;
		_window_height = _height;
	}

	if (!init_imgui_resources())
		goto exit_failure;

	if (_is_vr)
		init_gui_vr();
#endif

	if (window != nullptr && !_is_vr)
		_input = input::register_window(window);
	else
		_input = std::make_shared<input>(nullptr);

	// Reset frame count to zero so effects are loaded in 'update_effects'
	_framecount = 0;

	_is_initialized = true;
	_last_reload_time = std::chrono::high_resolution_clock::now();

	_preset_save_success = true;
	_screenshot_save_success = true;

#if RESHADE_FX && RESHADE_ADDON
	invoke_addon_event<addon_event::init_effect_runtime>(this);
#endif

#if RESHADE_FX && RESHADE_VERBOSE_LOG
	LOG(INFO) << "Recreated runtime environment on runtime " << this << '.';
#endif

	return true;

exit_failure:
#if RESHADE_FX
	_device->destroy_resource(_empty_tex);
	_empty_tex = {};
	_device->destroy_resource_view(_empty_srv);
	_empty_srv = {};

	_device->destroy_resource(_effect_color_tex);
	_effect_color_tex = {};
	_device->destroy_resource_view(_effect_color_srv[0]);
	_effect_color_srv[0] = {};
	_device->destroy_resource_view(_effect_color_srv[1]);
	_effect_color_srv[1] = {};

	_device->destroy_resource(_effect_stencil_tex);
	_effect_stencil_tex = {};
	_device->destroy_resource_view(_effect_stencil_dsv);
	_effect_stencil_dsv = {};
#endif

	for (const auto view : _back_buffer_targets)
		_device->destroy_resource_view(view);
	_back_buffer_targets.clear();

#if RESHADE_GUI
	if (_is_vr)
		deinit_gui_vr();

	destroy_imgui_resources();
#endif

	return false;
}
void reshade::runtime::on_reset()
{
	if (_is_initialized)
		// Update initialization state immediately, so that any effect loading still in progress can abort early
		_is_initialized = false;
	else
		return; // Nothing to do if the runtime was already destroyed or not successfully initialized in the first place

#if RESHADE_FX
	// Already performs a wait for idle, so no need to do it again before destroying resources below
	destroy_effects();

	_device->destroy_resource(_empty_tex);
	_empty_tex = {};
	_device->destroy_resource_view(_empty_srv);
	_empty_srv = {};

	_device->destroy_resource(_effect_color_tex);
	_effect_color_tex = {};
	_device->destroy_resource_view(_effect_color_srv[0]);
	_effect_color_srv[0] = {};
	_device->destroy_resource_view(_effect_color_srv[1]);
	_effect_color_srv[1] = {};

	_device->destroy_resource(_effect_stencil_tex);
	_effect_stencil_tex = {};
	_device->destroy_resource_view(_effect_stencil_dsv);
	_effect_stencil_dsv = {};
#endif

	for (const auto view : _back_buffer_targets)
		_device->destroy_resource_view(view);
	_back_buffer_targets.clear();

	_width = _height = 0;

#if RESHADE_GUI
	if (_is_vr)
		deinit_gui_vr();

	destroy_imgui_resources();
#endif

#if RESHADE_FX && RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_effect_runtime>(this);
#endif

#if RESHADE_FX && RESHADE_VERBOSE_LOG
	LOG(INFO) << "Destroyed runtime environment on runtime " << this << '.';
#endif
}
void reshade::runtime::on_present()
{
	assert(is_initialized());

#if RESHADE_FX
	update_effects();

	if (_effects_enabled && !_effects_rendered_this_frame)
	{
		if (_should_save_screenshot && _screenshot_save_before)
			save_screenshot(L" original");

		uint32_t back_buffer_index = get_current_back_buffer_index();
		const api::resource back_buffer_resource = get_back_buffer_resolved(back_buffer_index);

		api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();
		cmd_list->barrier(back_buffer_resource, api::resource_usage::present, api::resource_usage::render_target);
		runtime::render_effects(cmd_list, _back_buffer_targets[back_buffer_index * 2], _back_buffer_targets[back_buffer_index * 2 + 1]);
		cmd_list->barrier(back_buffer_resource, api::resource_usage::render_target, api::resource_usage::present);
	}
#endif

	if (_should_save_screenshot)
		save_screenshot(std::wstring());

	_framecount++;
	const auto current_time = std::chrono::high_resolution_clock::now();
	_last_frame_duration = current_time - _last_present_time; _last_present_time = current_time;
	_effects_rendered_this_frame = false;

#ifdef NDEBUG
	// Lock input so it cannot be modified by other threads while we are reading it here
	const auto input_lock = _input->lock();
#endif

#if RESHADE_GUI
	// Draw overlay
	if (_is_vr)
		draw_gui_vr();
	else
		draw_gui();

	if (_should_save_screenshot && _screenshot_save_gui && (_show_overlay || (_preview_texture != 0 && _effects_enabled)))
		save_screenshot(L" overlay");
#endif

	// All screenshots were created at this point, so reset request
	_should_save_screenshot = false;

	// Handle keyboard shortcuts
	if (!_ignore_shortcuts)
	{
		if (_input->is_key_pressed(_effects_key_data, _force_shortcut_modifiers))
			_effects_enabled = !_effects_enabled;

		if (_input->is_key_pressed(_screenshot_key_data, _force_shortcut_modifiers))
			_should_save_screenshot = true; // Remember that we want to save a screenshot next frame

#if RESHADE_FX
		// Do not allow the next shortcuts while effects are being loaded or initialized (since they affect that state)
		if (!is_loading() && _reload_create_queue.empty())
		{
			if (_input->is_key_pressed(_reload_key_data, _force_shortcut_modifiers))
				reload_effects();

			if (_input->is_key_pressed(_performance_mode_key_data, _force_shortcut_modifiers))
			{
				_performance_mode = !_performance_mode;
				save_config();
				reload_effects();
			}

			if (const bool reversed = _input->is_key_pressed(_prev_preset_key_data, _force_shortcut_modifiers);
				reversed || _input->is_key_pressed(_next_preset_key_data, _force_shortcut_modifiers))
			{
				// The preset shortcut key was pressed down, so start the transition
				if (switch_to_next_preset(_current_preset_path.parent_path(), reversed))
				{
					_last_preset_switching_time = current_time;
					_is_in_between_presets_transition = true;
					save_config();
				}
			}

			// Continuously update preset values while a transition is in progress
			if (_is_in_between_presets_transition)
				load_current_preset();
		}
#endif
	}

	// Reset input status
	_input->next_frame();

	// Save modified INI files
	if (!ini_file::flush_cache())
		_preset_save_success = false;

#if RESHADE_LITE
	// Detect high network traffic
	extern volatile long g_network_traffic;

	static int cooldown = 0, traffic = 0;
	if (cooldown-- > 0)
	{
		traffic += g_network_traffic > 0;
	}
	else
	{
		const bool was_enabled = addon::enabled;
		addon::enabled = traffic < 10;
		traffic = 0;
		cooldown = 60;

#if RESHADE_FX
		if (addon::enabled != was_enabled)
		{
			if (was_enabled)
				_backup_texture_semantic_bindings = _texture_semantic_bindings;

			for (const auto &info : _backup_texture_semantic_bindings)
			{
				update_texture_bindings(info.first.c_str(), addon::enabled ? info.second.first : api::resource_view { 0 }, addon::enabled ? info.second.second : api::resource_view { 0 });
			}
		}
#endif
	}

	g_network_traffic = 0;
#endif
}

void reshade::runtime::load_config()
{
	const ini_file &config = ini_file::load_cache(_config_path);

	config.get("INPUT", "ForceShortcutModifiers", _force_shortcut_modifiers);
	config.get("INPUT", "KeyScreenshot", _screenshot_key_data);
#if RESHADE_FX
	config.get("INPUT", "KeyEffects", _effects_key_data);
	config.get("INPUT", "KeyNextPreset", _next_preset_key_data);
	config.get("INPUT", "KeyPerformanceMode", _performance_mode_key_data);
	config.get("INPUT", "KeyPreviousPreset", _prev_preset_key_data);
	config.get("INPUT", "KeyReload", _reload_key_data);

	config.get("GENERAL", "NoDebugInfo", _no_debug_info);
	config.get("GENERAL", "NoEffectCache", _no_effect_cache);
	config.get("GENERAL", "NoReloadOnInit", _no_reload_on_init);
	config.get("GENERAL", "NoReloadOnInitForNonVR", _no_reload_for_non_vr);

	config.get("GENERAL", "EffectSearchPaths", _effect_search_paths);
	config.get("GENERAL", "PerformanceMode", _performance_mode);
	config.get("GENERAL", "PreprocessorDefinitions", _global_preprocessor_definitions);
	config.get("GENERAL", "SkipLoadingDisabledEffects", _effect_load_skipping);
	config.get("GENERAL", "TextureSearchPaths", _texture_search_paths);
	config.get("GENERAL", "IntermediateCachePath", _intermediate_cache_path);

	config.get("GENERAL", "PresetPath", _current_preset_path);
	config.get("GENERAL", "PresetTransitionDelay", _preset_transition_delay);

	// Fall back to temp directory if cache path does not exist
	if (_intermediate_cache_path.empty() || !resolve_path(_intermediate_cache_path))
	{
		WCHAR temp_path[MAX_PATH] = L"";
		GetTempPathW(MAX_PATH, temp_path);
		_intermediate_cache_path = temp_path;
	}

	// Use default if the preset file does not exist yet
	if (!resolve_preset_path(_current_preset_path))
		_current_preset_path = g_reshade_base_path / L"ReShadePreset.ini";
#endif

	config.get("SCREENSHOT", "ClearAlpha", _screenshot_clear_alpha);
	config.get("SCREENSHOT", "FileFormat", _screenshot_format);
	config.get("SCREENSHOT", "FileNamingFormat", _screenshot_naming);
	config.get("SCREENSHOT", "JPEGQuality", _screenshot_jpeg_quality);
	config.get("SCREENSHOT", "SaveBeforeShot", _screenshot_save_before);
	config.get("SCREENSHOT", "SaveOverlayShot", _screenshot_save_gui);
	config.get("SCREENSHOT", "SavePath", _screenshot_path);
	config.get("SCREENSHOT", "SavePresetFile", _screenshot_include_preset);
	config.get("SCREENSHOT", "PostSaveCommand", _screenshot_post_save_command);
	config.get("SCREENSHOT", "PostSaveCommandArguments", _screenshot_post_save_command_arguments);
	config.get("SCREENSHOT", "PostSaveCommandWorkingDirectory", _screenshot_post_save_command_working_directory);
	config.get("SCREENSHOT", "PostSaveCommandShowWindow", _screenshot_post_save_command_show_window);

#if RESHADE_GUI
	load_config_gui(config);
#endif
}
void reshade::runtime::save_config() const
{
	ini_file &config = ini_file::load_cache(_config_path);

	config.set("INPUT", "ForceShortcutModifiers", _force_shortcut_modifiers);
	config.set("INPUT", "KeyScreenshot", _screenshot_key_data);
#if RESHADE_FX
	config.set("INPUT", "KeyEffects", _effects_key_data);
	config.set("INPUT", "KeyNextPreset", _next_preset_key_data);
	config.set("INPUT", "KeyPerformanceMode", _performance_mode_key_data);
	config.set("INPUT", "KeyPreviousPreset", _prev_preset_key_data);
	config.set("INPUT", "KeyReload", _reload_key_data);

	config.set("GENERAL", "NoDebugInfo", _no_debug_info);
	config.set("GENERAL", "NoEffectCache", _no_effect_cache);
	config.set("GENERAL", "NoReloadOnInit", _no_reload_on_init);
	config.set("GENERAL", "NoReloadOnInitForNonVR", _no_reload_for_non_vr);

	config.set("GENERAL", "EffectSearchPaths", _effect_search_paths);
	config.set("GENERAL", "PerformanceMode", _performance_mode);
	config.set("GENERAL", "PreprocessorDefinitions", _global_preprocessor_definitions);
	config.set("GENERAL", "SkipLoadingDisabledEffects", _effect_load_skipping);
	config.set("GENERAL", "TextureSearchPaths", _texture_search_paths);
	config.set("GENERAL", "IntermediateCachePath", _intermediate_cache_path);

	// Use ReShade DLL directory as base for relative preset paths (see 'resolve_preset_path')
	std::filesystem::path relative_preset_path = _current_preset_path.lexically_proximate(g_reshade_base_path);
	if (relative_preset_path.wstring().rfind(L"..", 0) != std::wstring::npos)
		relative_preset_path = _current_preset_path; // Do not use relative path if preset is in a parent directory
	if (relative_preset_path.is_relative()) // Prefix preset path with dot character to better indicate it being a relative path
		relative_preset_path = L"." / relative_preset_path;
	config.set("GENERAL", "PresetPath", relative_preset_path);
	config.set("GENERAL", "PresetTransitionDelay", _preset_transition_delay);
#endif

	config.set("SCREENSHOT", "ClearAlpha", _screenshot_clear_alpha);
	config.set("SCREENSHOT", "FileFormat", _screenshot_format);
	config.set("SCREENSHOT", "FileNamingFormat", _screenshot_naming);
	config.set("SCREENSHOT", "JPEGQuality", _screenshot_jpeg_quality);
	config.set("SCREENSHOT", "SaveBeforeShot", _screenshot_save_before);
	config.set("SCREENSHOT", "SaveOverlayShot", _screenshot_save_gui);
	config.set("SCREENSHOT", "SavePath", _screenshot_path);
	config.set("SCREENSHOT", "SavePresetFile", _screenshot_include_preset);
	config.set("SCREENSHOT", "PostSaveCommand", _screenshot_post_save_command);
	config.set("SCREENSHOT", "PostSaveCommandArguments", _screenshot_post_save_command_arguments);
	config.set("SCREENSHOT", "PostSaveCommandWorkingDirectory", _screenshot_post_save_command_working_directory);
	config.set("SCREENSHOT", "PostSaveCommandShowWindow", _screenshot_post_save_command_show_window);

#if RESHADE_GUI
	save_config_gui(config);
#endif
}

#if RESHADE_FX
void reshade::runtime::load_current_preset()
{
	_preset_save_success = true;

	ini_file config = ini_file::load_cache(_config_path); // Copy config, because reference becomes invalid in the next line
	const ini_file &preset = ini_file::load_cache(_current_preset_path);

	std::vector<std::string> technique_list;
	preset.get({}, "Techniques", technique_list);
	std::vector<std::string> sorted_technique_list;
	preset.get({}, "TechniqueSorting", sorted_technique_list);
	std::vector<std::string> preset_preprocessor_definitions;
	preset.get({}, "PreprocessorDefinitions", preset_preprocessor_definitions);

	// Recompile effects if preprocessor definitions have changed or running in performance mode (in which case all preset values are compile-time constants)
	if (_reload_remaining_effects != 0) // ... unless this is the 'load_current_preset' call in 'update_effects'
	{
		if (_performance_mode || preset_preprocessor_definitions != _preset_preprocessor_definitions)
		{
			_preset_preprocessor_definitions = std::move(preset_preprocessor_definitions);
			reload_effects();
			return; // Preset values are loaded in 'update_effects' during effect loading
		}

		if (std::find_if(technique_list.begin(), technique_list.end(), [this](const std::string &technique_name) {
				if (const size_t at_pos = technique_name.find('@'); at_pos == std::string::npos)
					return true;
				else if (const auto it = std::find_if(_effects.begin(), _effects.end(),
					[effect_name = static_cast<std::string_view>(technique_name).substr(at_pos + 1)](const effect &effect) { return effect_name == effect.source_file.filename().u8string(); }); it == _effects.end())
					return true;
				else
					return it->skipped; }) != technique_list.end())
		{
			reload_effects();
			return;
		}
	}

	if (sorted_technique_list.empty())
		config.get("GENERAL", "TechniqueSorting", sorted_technique_list);
	if (sorted_technique_list.empty())
		sorted_technique_list = technique_list;

	// Reorder techniques
	std::sort(_techniques.begin(), _techniques.end(), [this, &sorted_technique_list](const technique &lhs, const technique &rhs) {
		const std::string lhs_unique = lhs.name + '@' + _effects[lhs.effect_index].source_file.filename().u8string();
		const std::string rhs_unique = rhs.name + '@' + _effects[rhs.effect_index].source_file.filename().u8string();
		auto lhs_it = std::find(sorted_technique_list.begin(), sorted_technique_list.end(), lhs_unique);
		auto rhs_it = std::find(sorted_technique_list.begin(), sorted_technique_list.end(), rhs_unique);
		if (lhs_it == sorted_technique_list.end())
			lhs_it = std::find(sorted_technique_list.begin(), sorted_technique_list.end(), lhs.name);
		if (rhs_it == sorted_technique_list.end())
			rhs_it = std::find(sorted_technique_list.begin(), sorted_technique_list.end(), rhs.name);
		return lhs_it < rhs_it; });

	// Compute times since the transition has started and how much is left till it should end
	auto transition_time = std::chrono::duration_cast<std::chrono::microseconds>(_last_present_time - _last_preset_switching_time).count();
	auto transition_ms_left = _preset_transition_delay - transition_time / 1000;
	auto transition_ms_left_from_last_frame = transition_ms_left + std::chrono::duration_cast<std::chrono::microseconds>(_last_frame_duration).count() / 1000;

	if (_is_in_between_presets_transition && transition_ms_left <= 0)
		_is_in_between_presets_transition = false;

	for (effect &effect : _effects)
	{
		for (uniform &variable : effect.uniforms)
		{
			if (variable.special != special_uniform::none)
				continue;

			const std::string section = effect.source_file.filename().u8string();

			if (variable.supports_toggle_key())
			{
				if (!preset.get(section, "Key" + variable.name, variable.toggle_key_data))
					std::memset(variable.toggle_key_data, 0, sizeof(variable.toggle_key_data));
			}

			// Reset values to defaults before loading from a new preset
			if (!_is_in_between_presets_transition)
				reset_uniform_value(variable);

			reshadefx::constant values, values_old;
			switch (variable.type.base)
			{
			case reshadefx::type::t_int:
				get_uniform_value(variable, values.as_int, variable.type.components());
				preset.get(section, variable.name, values.as_int);
				set_uniform_value(variable, values.as_int, variable.type.components());
				break;
			case reshadefx::type::t_bool:
			case reshadefx::type::t_uint:
				get_uniform_value(variable, values.as_uint, variable.type.components());
				preset.get(section, variable.name, values.as_uint);
				set_uniform_value(variable, values.as_uint, variable.type.components());
				break;
			case reshadefx::type::t_float:
				get_uniform_value(variable, values.as_float, variable.type.components());
				values_old = values;
				preset.get(section, variable.name, values.as_float);
				if (_is_in_between_presets_transition)
				{
					// Perform smooth transition on floating point values
					for (unsigned int i = 0; i < variable.type.components(); i++)
					{
						const auto transition_ratio = (values.as_float[i] - values_old.as_float[i]) / transition_ms_left_from_last_frame;
						values.as_float[i] = values.as_float[i] - transition_ratio * transition_ms_left;
					}
				}
				set_uniform_value(variable, values.as_float, variable.type.components());
				break;
			}
		}
	}

	for (technique &tech : _techniques)
	{
		const std::string unique_name =
			tech.name + '@' + _effects[tech.effect_index].source_file.filename().u8string();

		// Ignore preset if "enabled" annotation is set
		if (tech.annotation_as_int("enabled") ||
			std::find(technique_list.begin(), technique_list.end(), unique_name) != technique_list.end() ||
			std::find(technique_list.begin(), technique_list.end(), tech.name) != technique_list.end())
			enable_technique(tech);
		else
			disable_technique(tech);

		if (!preset.get({}, "Key" + unique_name, tech.toggle_key_data) &&
			!preset.get({}, "Key" + tech.name, tech.toggle_key_data))
		{
			tech.toggle_key_data[0] = tech.annotation_as_int("toggle");
			tech.toggle_key_data[1] = tech.annotation_as_int("togglectrl");
			tech.toggle_key_data[2] = tech.annotation_as_int("toggleshift");
			tech.toggle_key_data[3] = tech.annotation_as_int("togglealt");
		}
	}

	// Reverse queue so that effects are enabled in the order they are defined in the preset (since the queue is worked from back to front)
	std::reverse(_reload_create_queue.begin(), _reload_create_queue.end());
}
void reshade::runtime::save_current_preset() const
{
	ini_file &preset = ini_file::load_cache(_current_preset_path);

	// Build list of active techniques and effects
	std::vector<std::string> technique_list, sorted_technique_list;
	std::unordered_set<size_t> effect_list;
	effect_list.reserve(_techniques.size());
	technique_list.reserve(_techniques.size());
	sorted_technique_list.reserve(_techniques.size());

	for (const technique &tech : _techniques)
	{
		const std::string unique_name =
			tech.name + '@' + _effects[tech.effect_index].source_file.filename().u8string();

		if (tech.enabled)
			technique_list.push_back(unique_name);
		if (tech.enabled || tech.toggle_key_data[0] != 0)
			effect_list.insert(tech.effect_index);

		// Keep track of the order of all techniques and not just the enabled ones
		sorted_technique_list.push_back(unique_name);

		if (tech.toggle_key_data[0] != 0)
			preset.set({}, "Key" + unique_name, tech.toggle_key_data);
		else if (tech.annotation_as_int("toggle") != 0)
			preset.set({}, "Key" + unique_name, 0); // Overwrite default toggle key to none
		else
			preset.remove_key({}, "Key" + unique_name);
	}

	preset.set({}, "Techniques", std::move(technique_list));
	preset.set({}, "TechniqueSorting", std::move(sorted_technique_list));
	preset.set({}, "PreprocessorDefinitions", _preset_preprocessor_definitions);

	// TODO: Do we want to save spec constants here too? The preset will be rather empty in performance mode otherwise.
	for (size_t effect_index = 0; effect_index < _effects.size(); ++effect_index)
	{
		if (effect_list.find(effect_index) == effect_list.end())
			continue;

		const effect &effect = _effects[effect_index];

		for (const uniform &variable : effect.uniforms)
		{
			if (variable.special != special_uniform::none)
				continue;

			const std::string section = effect.source_file.filename().u8string();
			const unsigned int components = variable.type.components();
			reshadefx::constant values;

			if (variable.supports_toggle_key())
			{
				// save the shortcut key into the preset files
				if (variable.toggle_key_data[0] != 0)
					preset.set(section, "Key" + variable.name, variable.toggle_key_data);
				else
					preset.remove_key(section, "Key" + variable.name);
			}

			switch (variable.type.base)
			{
			case reshadefx::type::t_int:
				get_uniform_value(variable, values.as_int, components);
				preset.set(section, variable.name, values.as_int, components);
				break;
			case reshadefx::type::t_bool:
			case reshadefx::type::t_uint:
				get_uniform_value(variable, values.as_uint, components);
				preset.set(section, variable.name, values.as_uint, components);
				break;
			case reshadefx::type::t_float:
				get_uniform_value(variable, values.as_float, components);
				preset.set(section, variable.name, values.as_float, components);
				break;
			}
		}
	}
}

bool reshade::runtime::switch_to_next_preset(std::filesystem::path filter_path, bool reversed)
{
	std::error_code ec; // This is here to ignore file system errors below

	std::filesystem::path filter_text;
	if (resolve_path(filter_path); !std::filesystem::is_directory(filter_path, ec))
		if (filter_text = filter_path.filename(); !filter_text.empty())
			filter_path = filter_path.parent_path();

	size_t current_preset_index = std::numeric_limits<size_t>::max();
	std::vector<std::filesystem::path> preset_paths;

	for (std::filesystem::path preset_path : std::filesystem::directory_iterator(filter_path, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		// Skip anything that is not a valid preset file
		if (!resolve_preset_path(preset_path))
			continue;

		// Keep track of the index of the current preset in the list of found preset files that is being build
		if (std::filesystem::equivalent(preset_path, _current_preset_path, ec)) {
			current_preset_index = preset_paths.size();
			preset_paths.push_back(std::move(preset_path));
			continue;
		}

		const std::wstring preset_name = preset_path.stem();
		// Only add those files that are matching the filter text
		if (filter_text.empty() || std::search(preset_name.begin(), preset_name.end(), filter_text.native().begin(), filter_text.native().end(),
			[](wchar_t c1, wchar_t c2) { return towlower(c1) == towlower(c2); }) != preset_name.end())
			preset_paths.push_back(std::move(preset_path));
	}

	if (preset_paths.begin() == preset_paths.end())
		return false; // No valid preset files were found, so nothing more to do

	if (current_preset_index == std::numeric_limits<size_t>::max())
	{
		// Current preset was not in the container path, so just use the first or last file
		if (reversed)
			_current_preset_path = preset_paths.back();
		else
			_current_preset_path = preset_paths.front();
	}
	else
	{
		// Current preset was found in the container path, so use the file before or after it
		if (auto it = std::next(preset_paths.begin(), current_preset_index); reversed)
			_current_preset_path = it == preset_paths.begin() ? preset_paths.back() : *--it;
		else
			_current_preset_path = it == std::prev(preset_paths.end()) ? preset_paths.front() : *++it;
	}

	return true;
}

bool reshade::runtime::load_effect(const std::filesystem::path &source_file, const ini_file &preset, size_t effect_index, bool preprocess_required)
{
	// Generate a unique string identifying this effect
	std::string attributes;
	attributes += "app=" + g_target_executable_path.stem().u8string() + ';';
	attributes += "width=" + std::to_string(_width) + ';';
	attributes += "height=" + std::to_string(_height) + ';';
	attributes += "color_bit_depth=" + std::to_string(format_color_bit_depth(_back_buffer_format)) + ';';
	attributes += "version=" + std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION) + ';';
	attributes += "performance_mode=" + std::string(_performance_mode ? "1" : "0") + ';';
	attributes += "vendor=" + std::to_string(_vendor_id) + ';';
	attributes += "device=" + std::to_string(_device_id) + ';';

	std::set<std::filesystem::path> include_paths;
	if (source_file.is_absolute())
		include_paths.emplace(source_file.parent_path());
	for (std::filesystem::path include_path : _effect_search_paths)
		if (resolve_path(include_path))
			include_paths.emplace(std::move(include_path));

	for (const std::filesystem::path &include_path : include_paths)
	{
		std::error_code ec;
		attributes += include_path.u8string();
		for (const auto &entry : std::filesystem::directory_iterator(include_path, std::filesystem::directory_options::skip_permission_denied, ec))
		{
			const std::filesystem::path filename = entry.path().filename();
			if (filename == source_file.filename() || filename.extension() == L".fxh")
			{
				attributes += ',';
				attributes += filename.u8string();
				attributes += '?';
				attributes += std::to_string(entry.last_write_time(ec).time_since_epoch().count());
			}
		}
		attributes += ';';
	}

	std::vector<std::string> preprocessor_definitions = _global_preprocessor_definitions;
	// Insert preset preprocessor definitions before global ones, so that if there are duplicates, the preset ones are used (since 'add_macro_definition' succeeds only for the first occurance)
	preprocessor_definitions.insert(preprocessor_definitions.begin(), _preset_preprocessor_definitions.begin(), _preset_preprocessor_definitions.end());
	for (const std::string &definition : preprocessor_definitions)
		attributes += definition + ';';

	const size_t source_hash = std::hash<std::string>()(attributes);

	effect &effect = _effects[effect_index];
	const std::string effect_name = source_file.filename().u8string();
	if (source_file != effect.source_file || source_hash != effect.source_hash)
	{
		// Source hash has changed, reset effect and load from scratch, rather than updating
		effect = {};
		effect.source_file = source_file;
		effect.source_hash = source_hash;
	}

	if (_effect_load_skipping && !_load_option_disable_skipping && !_worker_threads.empty()) // Only skip during 'load_effects'
	{
		if (std::vector<std::string> techniques;
			preset.get({}, "Techniques", techniques))
		{
			effect.skipped = std::find_if(techniques.cbegin(), techniques.cend(), [&effect_name](const std::string &technique) {
				const size_t at_pos = technique.find('@') + 1;
				return at_pos == 0 || technique.find(effect_name, at_pos) == at_pos; }) == techniques.cend();

			if (effect.skipped)
			{
				if (_reload_remaining_effects != 0 && _reload_remaining_effects != std::numeric_limits<size_t>::max())
					_reload_remaining_effects--;
				return false;
			}
		}
	}

	bool source_cached = false; std::string source;
	if (!effect.preprocessed && (preprocess_required || (source_cached = load_effect_cache(source_file.stem().u8string() + '-' + std::to_string(_renderer_id) + '-' + std::to_string(source_hash), "i", source)) == false))
	{
		reshadefx::preprocessor pp;
		pp.add_macro_definition("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
		pp.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", _performance_mode ? "1" : "0");
		pp.add_macro_definition("__VENDOR__", std::to_string(_vendor_id));
		pp.add_macro_definition("__DEVICE__", std::to_string(_device_id));
		pp.add_macro_definition("__RENDERER__", std::to_string(_renderer_id));
		pp.add_macro_definition("__APPLICATION__", std::to_string( // Truncate hash to 32-bit, since lexer currently only supports 32-bit numbers anyway
			std::hash<std::string>()(g_target_executable_path.stem().u8string()) & 0xFFFFFFFF));
		pp.add_macro_definition("BUFFER_WIDTH", std::to_string(_width));
		pp.add_macro_definition("BUFFER_HEIGHT", std::to_string(_height));
		pp.add_macro_definition("BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)");
		pp.add_macro_definition("BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)");
		pp.add_macro_definition("BUFFER_COLOR_BIT_DEPTH", std::to_string(format_color_bit_depth(_back_buffer_format)));

		for (const std::string &definition : preprocessor_definitions)
		{
			if (definition.empty() || definition == "=")
				continue; // Skip invalid definitions

			const size_t equals_index = definition.find('=');
			if (equals_index != std::string::npos)
				pp.add_macro_definition(
					definition.substr(0, equals_index),
					definition.substr(equals_index + 1));
			else
				pp.add_macro_definition(definition);
		}

		for (const std::filesystem::path &include_path : include_paths)
			pp.add_include_path(include_path);

		// Add some conversion macros for compatibility with older versions of ReShade
		pp.append_string(
			"#define tex2Doffset(s, coords, offset) tex2D(s, coords, offset)\n"
			"#define tex2Dlodoffset(s, coords, offset) tex2Dlod(s, coords, offset)\n"
			"#define tex2Dgather(s, t, c) tex2Dgather##c(s, t)\n"
			"#define tex2Dgatheroffset(s, t, o, c) tex2Dgather##c(s, t, o)\n"
			"#define tex2Dgather0 tex2DgatherR\n"
			"#define tex2Dgather1 tex2DgatherG\n"
			"#define tex2Dgather2 tex2DgatherB\n"
			"#define tex2Dgather3 tex2DgatherA\n");

		// Load and preprocess the source file
		effect.preprocessed = pp.append_file(source_file);

		// Append preprocessor errors to the error list
		effect.errors      += pp.errors();

		if (effect.preprocessed)
		{
			source = std::move(pp.output());
			source_cached = save_effect_cache(source_file.stem().u8string() + '-' + std::to_string(_renderer_id) + '-' + std::to_string(source_hash), "i", source);

			// Keep track of used preprocessor definitions (so they can be displayed in the overlay)
			effect.definitions.clear();
			for (const auto &definition : pp.used_macro_definitions())
			{
				if (definition.first.size() <= 10 || definition.first[0] == '_' || !definition.first.compare(0, 8, "RESHADE_") || !definition.first.compare(0, 7, "BUFFER_"))
					continue;

				effect.definitions.emplace_back(definition.first, trim(definition.second));
			}

			std::sort(effect.definitions.begin(), effect.definitions.end());

			// Keep track of included files
			effect.included_files = pp.included_files();
			std::sort(effect.included_files.begin(), effect.included_files.end()); // Sort file names alphabetically
		}
	}

	if (!effect.compiled && !source.empty())
	{
		unsigned shader_model;
		if (_renderer_id == 0x9000)
			shader_model = 30; // D3D9
		else if (_renderer_id < 0xa100)
			shader_model = 40; // D3D10 (including feature level 9)
		else if (_renderer_id < 0xb000)
			shader_model = 41; // D3D10.1
		else if (_renderer_id < 0xc000)
			shader_model = 50; // D3D11
		else
			shader_model = 51; // D3D12

		std::unique_ptr<reshadefx::codegen> codegen;
		if ((_renderer_id & 0xF0000) == 0)
			codegen.reset(reshadefx::create_codegen_hlsl(shader_model, !_no_debug_info, _performance_mode));
		else if (_renderer_id < 0x20000)
			codegen.reset(reshadefx::create_codegen_glsl(!_no_debug_info, _performance_mode, false, true));
		else // Vulkan uses SPIR-V input
			codegen.reset(reshadefx::create_codegen_spirv(true, !_no_debug_info, _performance_mode, false, false));

		reshadefx::parser parser;

		// Compile the pre-processed source code (try the compile even if the preprocessor step failed to get additional error information)
		effect.compiled = parser.parse(std::move(source), codegen.get());

		// Append parser errors to the error list
		effect.errors  += parser.errors();

		// Write result to effect module
		codegen->write_result(effect.module);

		if (effect.compiled)
		{
			effect.uniforms.clear();

			// Create space for all variables (aligned to 16 bytes)
			effect.uniform_data_storage.resize((effect.module.total_uniform_size + 15) & ~15);

			for (uniform variable : effect.module.uniforms)
			{
				variable.effect_index = effect_index;

				// Copy initial data into uniform storage area
				reset_uniform_value(variable);

				const std::string_view special = variable.annotation_as_string("source");
				if (special.empty()) /* Ignore if annotation is missing */
					variable.special = special_uniform::none;
				else if (special == "frametime")
					variable.special = special_uniform::frame_time;
				else if (special == "framecount")
					variable.special = special_uniform::frame_count;
				else if (special == "random")
					variable.special = special_uniform::random;
				else if (special == "pingpong")
					variable.special = special_uniform::ping_pong;
				else if (special == "date")
					variable.special = special_uniform::date;
				else if (special == "timer")
					variable.special = special_uniform::timer;
				else if (special == "key")
					variable.special = special_uniform::key;
				else if (special == "mousepoint")
					variable.special = special_uniform::mouse_point;
				else if (special == "mousedelta")
					variable.special = special_uniform::mouse_delta;
				else if (special == "mousebutton")
					variable.special = special_uniform::mouse_button;
				else if (special == "mousewheel")
					variable.special = special_uniform::mouse_wheel;
				else if (special == "freepie")
					variable.special = special_uniform::freepie;
				else if (special == "ui_open" || special == "overlay_open")
					variable.special = special_uniform::overlay_open;
				else if (special == "ui_active" || special == "overlay_active")
					variable.special = special_uniform::overlay_active;
				else if (special == "ui_hovered" || special == "overlay_hovered")
					variable.special = special_uniform::overlay_hovered;
				else
					variable.special = special_uniform::unknown;

				effect.uniforms.push_back(std::move(variable));
			}

			// Fill all specialization constants with values from the current preset
			if (_performance_mode)
			{
				std::string preamble;

				for (reshadefx::uniform_info &constant : effect.module.spec_constants)
				{
					switch (constant.type.base)
					{
					case reshadefx::type::t_int:
						preset.get(effect_name, constant.name, constant.initializer_value.as_int);
						break;
					case reshadefx::type::t_bool:
					case reshadefx::type::t_uint:
						preset.get(effect_name, constant.name, constant.initializer_value.as_uint);
						break;
					case reshadefx::type::t_float:
						preset.get(effect_name, constant.name, constant.initializer_value.as_float);
						break;
					}

					// Check if this is a split specialization constant and move data accordingly
					if (constant.type.is_scalar() && constant.offset != 0)
						constant.initializer_value.as_uint[0] = constant.initializer_value.as_uint[constant.offset];

					if (effect.module.hlsl.empty())
						continue;

					preamble += "#define SPEC_CONSTANT_" + constant.name + ' ';

					for (unsigned int i = 0; i < constant.type.components(); ++i)
					{
						switch (constant.type.base)
						{
						case reshadefx::type::t_bool:
							preamble += constant.initializer_value.as_uint[i] ? "true" : "false";
							break;
						case reshadefx::type::t_int:
							preamble += std::to_string(constant.initializer_value.as_int[i]);
							break;
						case reshadefx::type::t_uint:
							preamble += std::to_string(constant.initializer_value.as_uint[i]);
							break;
						case reshadefx::type::t_float:
							preamble += std::to_string(constant.initializer_value.as_float[i]);
							break;
						}

						if (i + 1 < constant.type.components())
							preamble += ", ";
					}

					preamble += '\n';
				}

				effect.module.hlsl = preamble + effect.module.hlsl;
			}
		}
	}

	if ( effect.compiled && (effect.preprocessed || source_cached))
	{
		// Compile shader modules
		for (const reshadefx::entry_point &entry_point : effect.module.entry_points)
		{
			if (entry_point.type == reshadefx::shader_type::cs && !_device->check_capability(api::device_caps::compute_shader))
			{
				effect.errors += "Compute shaders are not supported in D3D9.";
				effect.compiled = false;
				break;
			}

			auto &assembly = effect.assembly[entry_point.name];
			std::string &cso = assembly.first;
			std::string &cso_text = assembly.second;

			if (!effect.module.spirv.empty())
			{
				assert(_renderer_id >= 0x14600); // Core since OpenGL 4.6 (see https://www.khronos.org/opengl/wiki/SPIR-V)

				// There are various issues with SPIR-V modules that have multiple entry points on all major GPU vendors.
				// On AMD for instance creating a graphics pipeline just fails with a generic VK_ERROR_OUT_OF_HOST_MEMORY. On NVIDIA artifacts occur on some driver versions.
				// To work around these problems, create a separate shader module for every entry point and rewrite the SPIR-V module for each to removes all but a single entry point (and associated functions/variables).
				uint32_t current_function = 0, current_function_offset = 0;
				std::vector<uint32_t> spirv = effect.module.spirv;
				std::vector<uint32_t> functions_to_remove, variables_to_remove;

				for (uint32_t inst = 5 /* Skip SPIR-V header information */; inst < spirv.size();)
				{
					const uint32_t op = spirv[inst] & 0xFFFF;
					const uint32_t len = (spirv[inst] >> 16) & 0xFFFF;
					assert(len != 0);

					switch (op)
					{
					case 15 /* OpEntryPoint */:
						// Look for any non-matching entry points
						if (entry_point.name != reinterpret_cast<const char *>(&spirv[inst + 3]))
						{
							functions_to_remove.push_back(spirv[inst + 2]);

							// Get interface variables
							for (uint32_t k = inst + 3 + static_cast<uint32_t>((strlen(reinterpret_cast<const char *>(&spirv[inst + 3])) + 4) / 4); k < inst + len; ++k)
								variables_to_remove.push_back(spirv[k]);

							// Remove this entry point from the module
							spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
							continue;
						}
						break;
					case 16 /* OpExecutionMode */:
						if (std::find(functions_to_remove.begin(), functions_to_remove.end(), spirv[inst + 1]) != functions_to_remove.end())
						{
							spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
							continue;
						}
						break;
					case 59 /* OpVariable */:
						// Remove all declarations of the interface variables for non-matching entry points
						if (std::find(variables_to_remove.begin(), variables_to_remove.end(), spirv[inst + 2]) != variables_to_remove.end())
						{
							spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
							continue;
						}
						break;
					case 71 /* OpDecorate */:
						// Remove all decorations targeting any of the interface variables for non-matching entry points
						if (std::find(variables_to_remove.begin(), variables_to_remove.end(), spirv[inst + 1]) != variables_to_remove.end())
						{
							spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
							continue;
						}
						break;
					case 54 /* OpFunction */:
						current_function = spirv[inst + 2];
						current_function_offset = inst;
						break;
					case 56 /* OpFunctionEnd */:
						// Remove all function definitions for non-matching entry points
						if (std::find(functions_to_remove.begin(), functions_to_remove.end(), current_function) != functions_to_remove.end())
						{
							spirv.erase(spirv.begin() + current_function_offset, spirv.begin() + inst + len);
							inst = current_function_offset;
							continue;
						}
						break;
					}

					inst += len;
				}

				cso.resize(spirv.size() * sizeof(uint32_t));
				std::memcpy(cso.data(), spirv.data(), cso.size());
			}
			else if (_renderer_id < 0x10000)
			{
				// Add specialization constant defines to source code
				const std::string hlsl =
					"#define COLOR_PIXEL_SIZE 1.0 / " + std::to_string(_width) + ", 1.0 / " + std::to_string(_height) + "\n"
					"#define DEPTH_PIXEL_SIZE COLOR_PIXEL_SIZE\n"
					"#define SV_DEPTH_PIXEL_SIZE DEPTH_PIXEL_SIZE\n"
					"#define SV_TARGET_PIXEL_SIZE COLOR_PIXEL_SIZE\n"
					"#line 1\n" + // Reset line number, so it matches what is shown when viewing the generated code
					effect.module.hlsl;

				// Overwrite position semantic in pixel shaders
				const D3D_SHADER_MACRO ps_defines[] = {
					{ "POSITION", "VPOS" }, { nullptr, nullptr }
				};

				std::string profile;
				switch (entry_point.type)
				{
				case reshadefx::shader_type::vs:
					profile = "vs";
					break;
				case reshadefx::shader_type::ps:
					profile = "ps";
					break;
				case reshadefx::shader_type::cs:
					profile = "cs";
					break;
				}

				switch (_renderer_id)
				{
				default:
				case D3D_FEATURE_LEVEL_11_0:
					profile += "_5_0";
					break;
				case D3D_FEATURE_LEVEL_10_1:
					profile += "_4_1";
					break;
				case D3D_FEATURE_LEVEL_10_0:
					profile += "_4_0";
					break;
				case D3D_FEATURE_LEVEL_9_1:
				case D3D_FEATURE_LEVEL_9_2:
					profile += "_4_0_level_9_1";
					break;
				case D3D_FEATURE_LEVEL_9_3:
					profile += "_4_0_level_9_3";
					break;
				case 0x9000:
					profile += "_3_0";
					break;
				}

				UINT compile_flags = (_performance_mode ? D3DCOMPILE_OPTIMIZATION_LEVEL3 : D3DCOMPILE_OPTIMIZATION_LEVEL1);
				if (_renderer_id >= D3D_FEATURE_LEVEL_10_0)
					compile_flags |= D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
				compile_flags |= D3DCOMPILE_DEBUG;
#endif

				HMODULE d3d_compiler_module = nullptr;
				const auto ensure_d3d_compiler_loaded = [&d3d_compiler_module]() {
					// Ensure HLSL compiler is loaded before trying to compile effects
					if (nullptr == d3d_compiler_module)
						d3d_compiler_module = LoadLibraryW(L"d3dcompiler_47.dll");
					if (nullptr == d3d_compiler_module)
						d3d_compiler_module = LoadLibraryW(L"d3dcompiler_43.dll");
					return d3d_compiler_module != nullptr;
				};

				std::string hlsl_attributes;
				hlsl_attributes += "entrypoint=" + entry_point.name + ';';
				hlsl_attributes += "profile=" + profile + ';';
				hlsl_attributes += "flags=" + std::to_string(compile_flags) + ';';

				const std::string cache_id =
					effect.source_file.stem().u8string() + '-' + entry_point.name + '-' + std::to_string(_renderer_id) + '-' +
					std::to_string(std::hash<std::string_view>()(hlsl_attributes) ^ std::hash<std::string_view>()(hlsl));

				if (load_effect_cache(cache_id, "cso", cso) == false)
				{
					if (!ensure_d3d_compiler_loaded())
					{
						effect.errors += "Unable to load HLSL compiler (\"d3dcompiler_47.dll\")!";
						effect.compiled = false;
						break;
					}

					const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(d3d_compiler_module, "D3DCompile"));

					com_ptr<ID3DBlob> d3d_compiled, d3d_errors;
					const HRESULT hr = D3DCompile(
						hlsl.data(), hlsl.size(),
						nullptr, entry_point.type == reshadefx::shader_type::ps ? ps_defines : nullptr, nullptr,
						entry_point.name.c_str(),
						profile.c_str(),
						compile_flags, 0,
						&d3d_compiled, &d3d_errors);

					std::string d3d_errors_string;
					if (d3d_errors != nullptr) // Append warnings to the output error string as well
						d3d_errors_string.assign(static_cast<const char *>(d3d_errors->GetBufferPointer()), d3d_errors->GetBufferSize() - 1); // Subtracting one to not append the null-terminator as well

					// De-duplicate error lines (D3DCompiler sometimes repeats the same error multiple times)
					for (size_t line_offset = 0, next_line_offset;
						(next_line_offset = d3d_errors_string.find('\n', line_offset)) != std::string::npos; line_offset = next_line_offset + 1)
					{
						const std::string_view cur_line(d3d_errors_string.c_str() + line_offset, next_line_offset - line_offset);

						if (const size_t end_offset = d3d_errors_string.find('\n', next_line_offset + 1);
							end_offset != std::string::npos)
						{
							const std::string_view next_line(d3d_errors_string.c_str() + next_line_offset + 1, end_offset - next_line_offset - 1);
							if (cur_line == next_line)
							{
								d3d_errors_string.erase(next_line_offset, end_offset - next_line_offset);
								next_line_offset = line_offset - 1;
							}
						}

						// Also remove D3DCompiler warnings about 'groupshared' specifier used in VS/PS modules
						if (cur_line.find("X3579") != std::string_view::npos)
						{
							d3d_errors_string.erase(line_offset, next_line_offset + 1 - line_offset);
							next_line_offset = line_offset - 1;
						}
					}

					effect.errors += d3d_errors_string;

					if (FAILED(hr))
					{
						effect.compiled = false;

						d3d_errors.reset();
						d3d_compiled.reset();
						FreeLibrary(d3d_compiler_module);
						break;
					}

					cso.resize(d3d_compiled->GetBufferSize());
					std::memcpy(cso.data(), d3d_compiled->GetBufferPointer(), cso.size());

					save_effect_cache(cache_id, "cso", cso);
				}

				if (load_effect_cache(cache_id, "asm", cso_text) == false && ensure_d3d_compiler_loaded())
				{
					const auto D3DDisassemble = reinterpret_cast<pD3DDisassemble>(GetProcAddress(d3d_compiler_module, "D3DDisassemble"));

					if (com_ptr<ID3DBlob> d3d_disassembled; SUCCEEDED(D3DDisassemble(cso.data(), cso.size(), 0, nullptr, &d3d_disassembled)))
						cso_text.assign(static_cast<const char *>(d3d_disassembled->GetBufferPointer()), d3d_disassembled->GetBufferSize() - 1);

					save_effect_cache(cache_id, "asm", cso_text);
				}

				if (d3d_compiler_module != nullptr)
					FreeLibrary(d3d_compiler_module);
			}
			else
			{
				cso = "#version 430\n#define ENTRY_POINT_" + entry_point.name + " 1\n";

				if (entry_point.type == reshadefx::shader_type::vs)
				{
					// OpenGL does not allow using 'discard' in the vertex shader profile
					cso += "#define discard\n";
					// 'dFdx', 'dFdx' and 'fwidth' too are only available in fragment shaders
					cso += "#define dFdx(x) x\n";
					cso += "#define dFdy(y) y\n";
					cso += "#define fwidth(p) p\n";
				}
				if (entry_point.type != reshadefx::shader_type::cs)
				{
					// OpenGL does not allow using 'shared' in vertex/fragment shader profile
					cso += "#define shared\n";
					cso += "#define atomicAdd(a, b) a\n";
					cso += "#define atomicAnd(a, b) a\n";
					cso += "#define atomicOr(a, b) a\n";
					cso += "#define atomicXor(a, b) a\n";
					cso += "#define atomicMin(a, b) a\n";
					cso += "#define atomicMax(a, b) a\n";
					cso += "#define atomicExchange(a, b) a\n";
					cso += "#define atomicCompSwap(a, b, c) a\n";
					// Barrier intrinsics are only available in compute shaders
					cso += "#define barrier()\n";
					cso += "#define memoryBarrier()\n";
					cso += "#define groupMemoryBarrier()\n";
				}

				cso += "#line 1 0\n"; // Reset line number, so it matches what is shown when viewing the generated code
				cso += effect.module.hlsl;
			}
		}

		const std::unique_lock<std::mutex> lock(_reload_mutex);

		for (texture new_texture : effect.module.textures)
		{
			new_texture.effect_index = effect_index;

			// Try to share textures with the same name across effects
			if (const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
				[&new_texture](const auto &item) { return item.unique_name == new_texture.unique_name; });
				existing_texture != _textures.end())
			{
				// Cannot share texture if this is a normal one, but the existing one is a reference and vice versa
				if (new_texture.semantic != existing_texture->semantic)
				{
					effect.errors += "error: " + new_texture.unique_name + ": another effect (";
					effect.errors += _effects[existing_texture->effect_index].source_file.filename().u8string();
					effect.errors += ") already created a texture with the same name but different semantic\n";
					effect.compiled = false;
					break;
				}

				if (new_texture.semantic.empty() && !existing_texture->matches_description(new_texture))
				{
					effect.errors += "warning: " + new_texture.unique_name + ": another effect (";
					effect.errors += _effects[existing_texture->effect_index].source_file.filename().u8string();
					effect.errors += ") already created a texture with the same name but different dimensions\n";
				}
				if (new_texture.semantic.empty() && (existing_texture->annotation_as_string("source") != new_texture.annotation_as_string("source")))
				{
					effect.errors += "warning: " + new_texture.unique_name + ": another effect (";
					effect.errors += _effects[existing_texture->effect_index].source_file.filename().u8string();
					effect.errors += ") already created a texture with a different image file\n";
				}

				if (existing_texture->semantic == "COLOR" && format_color_bit_depth(_back_buffer_format) != 8)
				{
					for (const auto &sampler_info : effect.module.samplers)
					{
						if (sampler_info.srgb && sampler_info.texture_name == new_texture.unique_name)
						{
							effect.errors += "warning: " + sampler_info.unique_name + ": texture does not support sRGB sampling (back buffer format is not RGBA8)";
						}
					}
				}

				if (std::find(existing_texture->shared.begin(), existing_texture->shared.end(), effect_index) == existing_texture->shared.end())
					existing_texture->shared.push_back(effect_index);

				// Always make shared textures render targets, since they may be used as such in a different effect
				existing_texture->render_target = true;
				existing_texture->storage_access = true;
				continue;
			}

			if (new_texture.annotation_as_int("pooled") && new_texture.semantic.empty())
			{
				// Try to find another pooled texture to share with (and do not share within the same effect)
				if (const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
					[&new_texture](const auto &item) { return item.annotation_as_int("pooled") && item.effect_index != new_texture.effect_index && item.matches_description(new_texture); });
					existing_texture != _textures.end())
				{
					// Overwrite referenced texture in samplers with the pooled one
					for (auto &sampler_info : effect.module.samplers)
						if (sampler_info.texture_name == new_texture.unique_name)
							sampler_info.texture_name  = existing_texture->unique_name;
					// Overwrite referenced texture in storages with the pooled one
					for (auto &storage_info : effect.module.storages)
						if (storage_info.texture_name == new_texture.unique_name)
							storage_info.texture_name  = existing_texture->unique_name;
					// Overwrite referenced texture in render targets with the pooled one
					for (auto &technique_info : effect.module.techniques)
					{
						for (auto &pass_info : technique_info.passes)
						{
							std::replace(std::begin(pass_info.render_target_names), std::end(pass_info.render_target_names),
								new_texture.unique_name, existing_texture->unique_name);

							for (auto &sampler_info : pass_info.samplers)
								if (sampler_info.texture_name == new_texture.unique_name)
									sampler_info.texture_name  = existing_texture->unique_name;
							for (auto &storage_info : pass_info.storages)
								if (storage_info.texture_name == new_texture.unique_name)
									storage_info.texture_name  = existing_texture->unique_name;
						}
					}

					if (std::find(existing_texture->shared.begin(), existing_texture->shared.end(), effect_index) == existing_texture->shared.end())
						existing_texture->shared.push_back(effect_index);

					existing_texture->render_target = true;
					existing_texture->storage_access = true;
					continue;
				}
			}

			if (!new_texture.semantic.empty() && (new_texture.semantic != "COLOR" && new_texture.semantic != "DEPTH"))
				effect.errors += "warning: " + new_texture.unique_name + ": unknown semantic '" + new_texture.semantic + "'\n";

			// This is the first effect using this texture
			new_texture.shared.push_back(effect_index);

			_textures.push_back(std::move(new_texture));
		}

		for (technique new_technique : effect.module.techniques)
		{
			new_technique.effect_index = effect_index;

			new_technique.hidden = new_technique.annotation_as_int("hidden") != 0;

			if (new_technique.annotation_as_int("enabled"))
				enable_technique(new_technique);

			_techniques.push_back(std::move(new_technique));
		}
	}

	if (_reload_remaining_effects != 0 && _reload_remaining_effects != std::numeric_limits<size_t>::max())
		_reload_remaining_effects--;
	else
		_reload_remaining_effects = 0; // Force effect initialization in 'update_effects'

	if ( effect.compiled && (effect.preprocessed || source_cached))
	{
		if (effect.errors.empty())
			LOG(INFO) << "Successfully compiled " << source_file << '.';
		else
			LOG(WARN) << "Successfully compiled " << source_file << " with warnings:\n" << effect.errors;
		return true;
	}
	else
	{
		_last_reload_successfull = false;

		if (effect.errors.empty())
			LOG(ERROR) << "Failed to compile " << source_file << '!';
		else
			LOG(ERROR) << "Failed to compile " << source_file << ":\n" << effect.errors;
		return false;
	}
}
bool reshade::runtime::create_effect(size_t effect_index)
{
	effect &effect = _effects[effect_index];

	// Create textures now, since they are referenced when building samplers below
	for (texture &tex : _textures)
	{
		if (tex.resource != 0 || std::find(tex.shared.begin(), tex.shared.end(), effect_index) == tex.shared.end())
			continue;

		if (!create_texture(tex))
		{
			effect.errors += "Failed to create texture " + tex.unique_name + '.';
			effect.compiled = false;
			_last_reload_successfull = false;
			return false;
		}
	}

	// Build specialization constants
	std::vector<uint32_t> spec_data;
	std::vector<uint32_t> spec_constants;
	for (const reshadefx::uniform_info &constant : effect.module.spec_constants)
	{
		uint32_t id = static_cast<uint32_t>(spec_constants.size());
		spec_data.push_back(constant.initializer_value.as_uint[0]);
		spec_constants.push_back(id);
	}

	// Create query pool for time measurements
	if (!_device->create_query_pool(api::query_type::timestamp, static_cast<uint32_t>(effect.module.techniques.size() * 2 * 4), &effect.query_pool))
	{
		effect.compiled = false;
		_last_reload_successfull = false;

		LOG(ERROR) << "Failed to create query pool for effect file " << effect.source_file << '!';
		return false;
	}

	const bool sampler_with_resource_view = _device->check_capability(api::device_caps::sampler_with_resource_view);

	api::descriptor_range layout_ranges[4];
	layout_ranges[0].binding = 0;
	layout_ranges[0].dx_register_index = 0; // b0 (global constant buffer)
	layout_ranges[0].dx_register_space = 0;
	layout_ranges[0].count = 1;
	layout_ranges[0].array_size = 1;
	layout_ranges[0].type = api::descriptor_type::constant_buffer;
	layout_ranges[0].visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	layout_ranges[1].binding = 0;
	layout_ranges[1].dx_register_index = 0; // s#
	layout_ranges[1].dx_register_space = 0;
	layout_ranges[1].count = effect.module.num_sampler_bindings;
	layout_ranges[1].array_size = 1;
	layout_ranges[1].type = sampler_with_resource_view ? api::descriptor_type::sampler_with_resource_view : api::descriptor_type::sampler;
	layout_ranges[1].visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	layout_ranges[2].binding = 0;
	layout_ranges[2].dx_register_index = 0; // t#
	layout_ranges[2].dx_register_space = 0;
	layout_ranges[2].count = effect.module.num_texture_bindings;
	layout_ranges[2].array_size = 1;
	layout_ranges[2].type = api::descriptor_type::shader_resource_view;
	layout_ranges[2].visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	layout_ranges[3].binding = 0;
	layout_ranges[3].dx_register_index = 0; // u#
	layout_ranges[3].dx_register_space = 0;
	layout_ranges[3].count = effect.module.num_storage_bindings;
	layout_ranges[3].array_size = 1;
	layout_ranges[3].type = api::descriptor_type::unordered_access_view;
	layout_ranges[3].visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	api::pipeline_layout_param layout_params[4];
	layout_params[0].type = api::pipeline_layout_param_type::descriptor_set;
	layout_params[0].descriptor_set.count = 1;
	layout_params[0].descriptor_set.ranges = &layout_ranges[0];

	layout_params[1].type = api::pipeline_layout_param_type::descriptor_set;
	layout_params[1].descriptor_set.count = 1;
	layout_params[1].descriptor_set.ranges = &layout_ranges[1];

	if (sampler_with_resource_view)
	{
		layout_params[2].type = api::pipeline_layout_param_type::descriptor_set;
		layout_params[2].descriptor_set.count = 1;
		layout_params[2].descriptor_set.ranges = &layout_ranges[3];
	}
	else
	{
		layout_params[2].type = api::pipeline_layout_param_type::descriptor_set;
		layout_params[2].descriptor_set.count = 1;
		layout_params[2].descriptor_set.ranges = &layout_ranges[2];
		layout_params[3].type = api::pipeline_layout_param_type::descriptor_set;
		layout_params[3].descriptor_set.count = 1;
		layout_params[3].descriptor_set.ranges = &layout_ranges[3];
	}

	// Create pipeline layout for this effect
	if (!_device->create_pipeline_layout(sampler_with_resource_view ? 3 : 4, layout_params, &effect.layout))
	{
		effect.compiled = false;
		_last_reload_successfull = false;

		LOG(ERROR) << "Failed to create pipeline layout for effect file " << effect.source_file << '!';
		return false;
	}

	api::buffer_range cb_range = {};
	std::vector<api::descriptor_set_update> descriptor_writes;
	descriptor_writes.reserve(effect.module.num_sampler_bindings + effect.module.num_texture_bindings + effect.module.num_storage_bindings + 1);
	std::vector<api::sampler_with_resource_view> sampler_descriptors;
	sampler_descriptors.resize(effect.module.num_sampler_bindings + effect.module.num_texture_bindings);

	// Create global constant buffer (except in D3D9, which does not have constant buffers)
	if (_renderer_id != 0x9000 && !effect.uniform_data_storage.empty())
	{
		if (!_device->create_resource(
				api::resource_desc(effect.uniform_data_storage.size(), api::memory_heap::cpu_to_gpu, api::resource_usage::constant_buffer),
				nullptr, api::resource_usage::cpu_access, &effect.cb))
		{
			effect.compiled = false;
			_last_reload_successfull = false;

			LOG(ERROR) << "Failed to create constant buffer for effect file " << effect.source_file << '!';
			return false;
		}

		_device->set_resource_name(effect.cb, "ReShade constant buffer");

		if (!_device->allocate_descriptor_set(effect.layout, 0, &effect.cb_set))
		{
			effect.compiled = false;
			_last_reload_successfull = false;

			LOG(ERROR) << "Failed to create constant buffer descriptor set for effect file " << effect.source_file << '!';
			return false;
		}

		cb_range.buffer = effect.cb;

		api::descriptor_set_update &write = descriptor_writes.emplace_back();
		write.set = effect.cb_set;
		write.binding = 0;
		write.type = api::descriptor_type::constant_buffer;
		write.count = 1;
		write.descriptors = &cb_range;
	}

	// Initialize bindings
	uint32_t total_pass_count = 0;
	for (const reshadefx::technique_info &info : effect.module.techniques)
		total_pass_count += static_cast<uint32_t>(info.passes.size());

	std::vector<api::descriptor_set> texture_sets(total_pass_count);
	std::vector<api::descriptor_set> storage_sets(total_pass_count);

	if (effect.module.num_sampler_bindings != 0)
	{
		if (!_device->allocate_descriptor_sets(sampler_with_resource_view ? total_pass_count : 1, effect.layout, 1, sampler_with_resource_view ? texture_sets.data() : &effect.sampler_set))
		{
			effect.compiled = false;
			_last_reload_successfull = false;

			LOG(ERROR) << "Failed to create sampler descriptor set for effect file " << effect.source_file << '!';
			return false;
		}

		if (!sampler_with_resource_view)
		{
			uint16_t sampler_list = 0;
			for (const reshadefx::sampler_info &info : effect.module.samplers)
			{
				// Only initialize sampler if it has not been created before
				if (0 != (sampler_list & (1 << info.binding)))
					continue;

				sampler_list |= (1 << info.binding); // Maximum sampler slot count is 16, so a 16-bit integer is enough to hold all bindings

				api::sampler_desc desc;
				desc.filter = static_cast<api::filter_mode>(info.filter);
				desc.address_u = static_cast<api::texture_address_mode>(info.address_u);
				desc.address_v = static_cast<api::texture_address_mode>(info.address_v);
				desc.address_w = static_cast<api::texture_address_mode>(info.address_w);
				desc.mip_lod_bias = info.lod_bias;
				desc.max_anisotropy = 1;
				desc.compare_op = api::compare_op::always;
				desc.border_color[0] = 0.0f;
				desc.border_color[1] = 0.0f;
				desc.border_color[2] = 0.0f;
				desc.border_color[3] = 0.0f;
				desc.min_lod = info.min_lod;
				desc.max_lod = info.max_lod;

				api::sampler &sampler_handle = sampler_descriptors[info.binding].sampler;
				if (!create_effect_sampler_state(desc, sampler_handle))
				{
					effect.compiled = false;
					_last_reload_successfull = false;

					LOG(ERROR) << "Failed to create sampler object '" << info.unique_name << "' in " << effect.source_file << '!';
					return false;
				}

				api::descriptor_set_update &write = descriptor_writes.emplace_back();
				write.set = effect.sampler_set;
				write.binding = info.binding;
				write.type = api::descriptor_type::sampler;
				write.count = 1;
				write.descriptors = &sampler_handle;
			}
		}
	}

	if (effect.module.num_texture_bindings != 0)
	{
		assert(!sampler_with_resource_view);

		if (!_device->allocate_descriptor_sets(total_pass_count, effect.layout, 2, texture_sets.data()))
		{
			effect.compiled = false;
			_last_reload_successfull = false;

			LOG(ERROR) << "Failed to create texture descriptor set for effect file " << effect.source_file << '!';
			return false;
		}
	}

	if (effect.module.num_storage_bindings != 0)
	{
		if (!_device->allocate_descriptor_sets(total_pass_count, effect.layout, sampler_with_resource_view ? 2 : 3, storage_sets.data()))
		{
			effect.compiled = false;
			_last_reload_successfull = false;

			LOG(ERROR) << "Failed to create storage descriptor set for effect file " << effect.source_file << '!';
			return false;
		}
	}

	// Initialize techniques and passes
	size_t total_pass_index = 0;
	size_t technique_index_in_effect = 0;

	for (technique &tech : _techniques)
	{
		if (!tech.passes_data.empty() || tech.effect_index != effect_index)
			continue;

		tech.passes_data.resize(tech.passes.size());

		// Offset index so that a query exists for each command frame and two subsequent ones are used for before/after stamps
		tech.query_base_index = static_cast<uint32_t>(technique_index_in_effect++ * 2 * 4);

		for (size_t pass_index = 0; pass_index < tech.passes.size(); ++pass_index, ++total_pass_index)
		{
			reshadefx::pass_info &pass_info = tech.passes[pass_index];
			technique::pass_data &pass_data = tech.passes_data[pass_index];

			if (!pass_info.cs_entry_point.empty())
			{
				api::pipeline_desc desc = { api::pipeline_stage::all_compute };
				desc.layout = effect.layout;

				const auto &cs = effect.assembly.at(pass_info.cs_entry_point).first;
				desc.compute.shader.code = cs.data();
				desc.compute.shader.code_size = cs.size();
				if (_renderer_id & 0x20000)
				{
					desc.compute.shader.entry_point = pass_info.cs_entry_point.c_str();
					desc.compute.shader.spec_constants = static_cast<uint32_t>(effect.module.spec_constants.size());
					desc.compute.shader.spec_constant_ids = spec_constants.data();
					desc.compute.shader.spec_constant_values = spec_data.data();
				}

				if (!_device->create_pipeline(desc, 0, nullptr, &pass_data.pipeline))
				{
					effect.compiled = false;
					_last_reload_successfull = false;

					LOG(ERROR) << "Failed to create compute pipeline for pass " << pass_index << " in technique '" << tech.name << "' in " << effect.source_file << '!';
					return false;
				}
			}
			else
			{
				api::pipeline_desc desc = { api::pipeline_stage::all_graphics };
				desc.layout = effect.layout;

				const auto &vs = effect.assembly.at(pass_info.vs_entry_point).first;
				desc.graphics.vertex_shader.code = vs.data();
				desc.graphics.vertex_shader.code_size = vs.size();
				if (_renderer_id & 0x20000)
				{
					desc.graphics.vertex_shader.entry_point = pass_info.vs_entry_point.c_str();
					desc.graphics.vertex_shader.spec_constants = static_cast<uint32_t>(effect.module.spec_constants.size());
					desc.graphics.vertex_shader.spec_constant_ids = spec_constants.data();
					desc.graphics.vertex_shader.spec_constant_values = spec_data.data();
				}

				const auto &ps = effect.assembly.at(pass_info.ps_entry_point).first;
				desc.graphics.pixel_shader.code = ps.data();
				desc.graphics.pixel_shader.code_size = ps.size();
				if (_renderer_id & 0x20000)
				{
					desc.graphics.pixel_shader.entry_point = pass_info.ps_entry_point.c_str();
					desc.graphics.pixel_shader.spec_constants = static_cast<uint32_t>(effect.module.spec_constants.size());
					desc.graphics.pixel_shader.spec_constant_ids = spec_constants.data();
					desc.graphics.pixel_shader.spec_constant_values = spec_data.data();
				}

				if (pass_info.render_target_names[0].empty())
				{
					pass_info.viewport_width = _width;
					pass_info.viewport_height = _height;

					desc.graphics.depth_stencil_format = _effect_stencil_format;
					desc.graphics.render_target_formats[0] = api::format_to_default_typed(_back_buffer_format, pass_info.srgb_write_enable);
				}
				else
				{
					// Only need to attach stencil if stencil is actually used in this pass
					if (pass_info.stencil_enable &&
						pass_info.viewport_width == _width &&
						pass_info.viewport_height == _height)
					{
						desc.graphics.depth_stencil_format = _effect_stencil_format;
					}

					for (int k = 0; k < 8 && !pass_info.render_target_names[k].empty(); ++k)
					{
						const auto texture = std::find_if(_textures.begin(), _textures.end(),
							[&unique_name = pass_info.render_target_names[k]](const auto &item) { return item.unique_name == unique_name && (item.resource != 0 || !item.semantic.empty()); });
						assert(texture != _textures.end());

						if (std::find(pass_data.modified_resources.begin(), pass_data.modified_resources.end(), texture->resource) == pass_data.modified_resources.end())
						{
							pass_data.modified_resources.push_back(texture->resource);

							if (texture->levels > 1)
								pass_data.generate_mipmap_views.push_back(texture->srv[pass_info.srgb_write_enable]);
						}

						const api::resource_desc res_desc = _device->get_resource_desc(texture->resource);

						pass_data.render_target_views[k] = texture->rtv[pass_info.srgb_write_enable];
						desc.graphics.render_target_formats[k] = api::format_to_default_typed(res_desc.texture.format, pass_info.srgb_write_enable);
					}
				}

				desc.graphics.topology = static_cast<api::primitive_topology>(pass_info.topology);

				const auto convert_blend_op = [](reshadefx::pass_blend_op value) {
					switch (value)
					{
					default:
					case reshadefx::pass_blend_op::add: return api::blend_op::add;
					case reshadefx::pass_blend_op::subtract: return api::blend_op::subtract;
					case reshadefx::pass_blend_op::rev_subtract: return api::blend_op::reverse_subtract;
					case reshadefx::pass_blend_op::min: return api::blend_op::min;
					case reshadefx::pass_blend_op::max: return api::blend_op::max;
					}
				};
				const auto convert_blend_func = [](reshadefx::pass_blend_func value) {
					switch (value) {
					case reshadefx::pass_blend_func::zero: return api::blend_factor::zero;
					default:
					case reshadefx::pass_blend_func::one: return api::blend_factor::one;
					case reshadefx::pass_blend_func::src_color: return api::blend_factor::source_color;
					case reshadefx::pass_blend_func::src_alpha: return api::blend_factor::source_alpha;
					case reshadefx::pass_blend_func::inv_src_color: return api::blend_factor::one_minus_source_color;
					case reshadefx::pass_blend_func::inv_src_alpha: return api::blend_factor::one_minus_source_alpha;
					case reshadefx::pass_blend_func::dst_color: return api::blend_factor::dest_color;
					case reshadefx::pass_blend_func::dst_alpha: return api::blend_factor::dest_alpha;
					case reshadefx::pass_blend_func::inv_dst_color: return api::blend_factor::one_minus_dest_color;
					case reshadefx::pass_blend_func::inv_dst_alpha: return api::blend_factor::one_minus_dest_alpha;
					}
				};

				// Technically should check for 'api::device_caps::independent_blend' support, but render target write masks are supported in D3D9, when rest is not, so just always set ...
				auto &blend_state = desc.graphics.blend_state;
				for (int i = 0; i < 8; ++i)
				{
					blend_state.blend_enable[i] = pass_info.blend_enable[i];
					blend_state.source_color_blend_factor[i] = convert_blend_func(pass_info.src_blend[i]);
					blend_state.dest_color_blend_factor[i] = convert_blend_func(pass_info.dest_blend[i]);
					blend_state.color_blend_op[i] = convert_blend_op(pass_info.blend_op[i]);
					blend_state.source_alpha_blend_factor[i] = convert_blend_func(pass_info.src_blend_alpha[i]);
					blend_state.dest_alpha_blend_factor[i] = convert_blend_func(pass_info.dest_blend_alpha[i]);
					blend_state.alpha_blend_op[i] = convert_blend_op(pass_info.blend_op_alpha[i]);
					blend_state.render_target_write_mask[i] = pass_info.color_write_mask[i];
				}

				auto &rasterizer_state = desc.graphics.rasterizer_state;
				rasterizer_state.cull_mode = api::cull_mode::none;

				const auto convert_stencil_op = [](reshadefx::pass_stencil_op value) {
					switch (value) {
					case reshadefx::pass_stencil_op::zero: return api::stencil_op::zero;
					default:
					case reshadefx::pass_stencil_op::keep: return api::stencil_op::keep;
					case reshadefx::pass_stencil_op::invert: return api::stencil_op::invert;
					case reshadefx::pass_stencil_op::replace: return api::stencil_op::replace;
					case reshadefx::pass_stencil_op::incr: return api::stencil_op::increment;
					case reshadefx::pass_stencil_op::incr_sat: return api::stencil_op::increment_saturate;
					case reshadefx::pass_stencil_op::decr: return api::stencil_op::decrement;
					case reshadefx::pass_stencil_op::decr_sat: return api::stencil_op::decrement_saturate;
					}
				};
				const auto convert_stencil_func = [](reshadefx::pass_stencil_func value) {
					switch (value)
					{
					case reshadefx::pass_stencil_func::never: return api::compare_op::never;
					case reshadefx::pass_stencil_func::equal: return api::compare_op::equal;
					case reshadefx::pass_stencil_func::not_equal: return api::compare_op::not_equal;
					case reshadefx::pass_stencil_func::less: return api::compare_op::less;
					case reshadefx::pass_stencil_func::less_equal: return api::compare_op::less_equal;
					case reshadefx::pass_stencil_func::greater: return api::compare_op::greater;
					case reshadefx::pass_stencil_func::greater_equal: return api::compare_op::greater_equal;
					default:
					case reshadefx::pass_stencil_func::always: return api::compare_op::always;
					}
				};

				auto &depth_stencil_state = desc.graphics.depth_stencil_state;
				depth_stencil_state.depth_enable = false;
				depth_stencil_state.depth_write_mask = false;
				depth_stencil_state.depth_func = api::compare_op::always;
				depth_stencil_state.stencil_enable = pass_info.stencil_enable;
				depth_stencil_state.stencil_read_mask = pass_info.stencil_read_mask;
				depth_stencil_state.stencil_write_mask = pass_info.stencil_write_mask;
				depth_stencil_state.back_stencil_fail_op = convert_stencil_op(pass_info.stencil_op_fail);
				depth_stencil_state.back_stencil_depth_fail_op = convert_stencil_op(pass_info.stencil_op_depth_fail);
				depth_stencil_state.back_stencil_pass_op = convert_stencil_op(pass_info.stencil_op_pass);
				depth_stencil_state.back_stencil_func = convert_stencil_func(pass_info.stencil_comparison_func);
				depth_stencil_state.front_stencil_fail_op = depth_stencil_state.back_stencil_fail_op;
				depth_stencil_state.front_stencil_depth_fail_op = depth_stencil_state.back_stencil_depth_fail_op;
				depth_stencil_state.front_stencil_pass_op = depth_stencil_state.back_stencil_pass_op;
				depth_stencil_state.front_stencil_func = depth_stencil_state.back_stencil_func;

				if (!_device->create_pipeline(desc, 0, nullptr, &pass_data.pipeline))
				{
					effect.compiled = false;
					_last_reload_successfull = false;

					LOG(ERROR) << "Failed to create graphics pipeline for pass " << pass_index << " in technique '" << tech.name << "' in " << effect.source_file << '!';
					return false;
				}
			}

			if (effect.module.num_sampler_bindings != 0 ||
				effect.module.num_texture_bindings != 0)
			{
				pass_data.texture_set = texture_sets[total_pass_index];

				for (const reshadefx::sampler_info &info : pass_info.samplers)
				{
					const auto texture = std::find_if(_textures.begin(), _textures.end(),
						[&unique_name = info.texture_name](const auto &item) { return item.unique_name == unique_name && (item.resource != 0 || !item.semantic.empty()); });
					assert(texture != _textures.end());

					api::resource_view &srv = sampler_descriptors[sampler_with_resource_view ? info.binding : effect.module.num_sampler_bindings + info.texture_binding].view;

					api::descriptor_set_update &write = descriptor_writes.emplace_back();
					write.set = pass_data.texture_set;
					write.count = 1;

					if (sampler_with_resource_view)
					{
						write.binding = info.binding;
						write.type = api::descriptor_type::sampler_with_resource_view;
						write.descriptors = &sampler_descriptors[info.binding];

						api::sampler_desc desc;
						desc.filter = static_cast<api::filter_mode>(info.filter);
						desc.address_u = static_cast<api::texture_address_mode>(info.address_u);
						desc.address_v = static_cast<api::texture_address_mode>(info.address_v);
						desc.address_w = static_cast<api::texture_address_mode>(info.address_w);
						desc.mip_lod_bias = info.lod_bias;
						desc.max_anisotropy = 1;
						desc.compare_op = api::compare_op::always;
						desc.border_color[0] = 0.0f;
						desc.border_color[1] = 0.0f;
						desc.border_color[2] = 0.0f;
						desc.border_color[3] = 0.0f;
						desc.min_lod = info.min_lod;
						desc.max_lod = info.max_lod;

						if (!create_effect_sampler_state(desc, sampler_descriptors[info.binding].sampler))
						{
							effect.compiled = false;
							_last_reload_successfull = false;

							LOG(ERROR) << "Failed to create sampler object '" << info.unique_name << "' in " << effect.source_file << '!';
							return false;
						}
					}
					else
					{
						write.binding = info.texture_binding;
						write.type = api::descriptor_type::shader_resource_view;
						write.descriptors = &srv;
					}

					if (texture->semantic == "COLOR")
					{
						srv = _effect_color_srv[info.srgb];
					}
					else if (!texture->semantic.empty())
					{
						if (const auto it = _texture_semantic_bindings.find(texture->semantic);
							it != _texture_semantic_bindings.end())
							srv = info.srgb ? it->second.second : it->second.first;
						else
							srv = _empty_srv;

						// Keep track of the texture descriptor to simplify updating it
						effect.texture_semantic_to_binding.push_back({ texture->semantic, write.set, write.binding, sampler_with_resource_view ? sampler_descriptors[info.binding].sampler : api::sampler { 0 }, !!info.srgb });
					}
					else
					{
						srv = texture->srv[info.srgb];
					}

					assert(srv != 0);
				}
			}

			if (effect.module.num_storage_bindings != 0)
			{
				pass_data.storage_set = storage_sets[total_pass_index];

				for (const reshadefx::storage_info &info : pass_info.storages)
				{
					const auto texture = std::find_if(_textures.begin(), _textures.end(),
						[&unique_name = info.texture_name](const auto &item) { return item.unique_name == unique_name && (item.resource != 0 || !item.semantic.empty()); });
					assert(texture != _textures.end());
					assert(texture->semantic.empty() && texture->uav != 0);

					if (std::find(pass_data.modified_resources.begin(), pass_data.modified_resources.end(), texture->resource) == pass_data.modified_resources.end())
					{
						pass_data.modified_resources.push_back(texture->resource);

						if (texture->levels > 1)
							pass_data.generate_mipmap_views.push_back(texture->srv[0]);
					}

					api::descriptor_set_update &write = descriptor_writes.emplace_back();
					write.set = pass_data.storage_set;
					write.binding = info.binding;
					write.type = api::descriptor_type::unordered_access_view;
					write.count = 1;
					write.descriptors = &texture->uav;
				}
			}
		}
	}

	if (!descriptor_writes.empty())
		_device->update_descriptor_sets(static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data());

	return true;
}
bool reshade::runtime::create_effect_sampler_state(const api::sampler_desc &desc, api::sampler &sampler)
{
	// Generate hash for sampler description
	size_t desc_hash = 2166136261;
	for (int i = 0; i < sizeof(desc); ++i)
		desc_hash = (desc_hash * 16777619) ^ reinterpret_cast<const uint8_t *>(&desc)[i];

	if (const auto it = _effect_sampler_states.find(desc_hash);
		it != _effect_sampler_states.end())
	{
		sampler = it->second;
		return true;
	}

	if (_device->create_sampler(desc, &sampler))
	{
		_effect_sampler_states.emplace(desc_hash, sampler);
		return true;
	}
	else
	{
		return false;
	}
}
void reshade::runtime::destroy_effect(size_t effect_index)
{
	assert(effect_index < _effects.size());

	// Make sure no effect resources are currently in use
	_graphics_queue->wait_idle();

	for (technique &tech : _techniques)
	{
		if (tech.effect_index != effect_index)
			continue;

		for (const technique::pass_data &pass : tech.passes_data)
		{
			_device->destroy_pipeline(pass.pipeline);

			_device->free_descriptor_set(pass.texture_set);
			_device->free_descriptor_set(pass.storage_set);
		}

		tech.passes_data.clear();
	}

	{	effect &effect = _effects[effect_index];

		_device->destroy_resource(effect.cb);
		effect.cb = {};

		_device->free_descriptor_set(effect.cb_set);
		effect.cb_set = {};
		_device->free_descriptor_set(effect.sampler_set);
		effect.sampler_set = {};

		_device->destroy_pipeline_layout(effect.layout);
		effect.layout = {};

		_device->destroy_query_pool(effect.query_pool);
		effect.query_pool = {};

		effect.texture_semantic_to_binding.clear();
	}

#if RESHADE_GUI
	_preview_texture.handle = 0;
	_effect_filter[0] = '\0'; // And reset filter too, since the list of techniques might have changed
#endif

	// Lock here to be safe in case another effect is still loading
	const std::unique_lock<std::mutex> lock(_reload_mutex);

	// No techniques from this effect are rendering anymore
	_effects[effect_index].rendering = 0;

	// Destroy textures belonging to this effect
	_textures.erase(std::remove_if(_textures.begin(), _textures.end(),
		[this, effect_index](texture &tex) {
			tex.shared.erase(std::remove(tex.shared.begin(), tex.shared.end(), effect_index), tex.shared.end());
			if (tex.shared.empty()) {
				destroy_texture(tex);
				return true;
			}
			return false;
		}), _textures.end());
	// Clean up techniques belonging to this effect
	_techniques.erase(std::remove_if(_techniques.begin(), _techniques.end(),
		[effect_index](const technique &tech) {
			return tech.effect_index == effect_index;
		}), _techniques.end());

	// Do not clear effect here, since it is common to be re-used immediately
}

bool reshade::runtime::create_texture(texture &tex)
{
	// Do not create resource if it is a special reference, those are set in 'render_technique' and 'update_texture_bindings'
	if (!tex.semantic.empty())
		return true;

	api::format format = api::format::unknown;
	api::format view_format = api::format::unknown;
	api::format view_format_srgb = api::format::unknown;

	switch (tex.format)
	{
	case reshadefx::texture_format::r8:
		format = api::format::r8_unorm;
		break;
	case reshadefx::texture_format::r16f:
		format = api::format::r16_float;
		break;
	case reshadefx::texture_format::r32f:
		format = api::format::r32_float;
		break;
	case reshadefx::texture_format::rg8:
		format = api::format::r8g8_unorm;
		break;
	case reshadefx::texture_format::rg16:
		format = api::format::r16g16_unorm;
		break;
	case reshadefx::texture_format::rg16f:
		format = api::format::r16g16_float;
		break;
	case reshadefx::texture_format::rg32f:
		format = api::format::r32g32_float;
		break;
	case reshadefx::texture_format::rgba8:
		format = api::format::r8g8b8a8_typeless;
		view_format = api::format::r8g8b8a8_unorm;
		view_format_srgb = api::format::r8g8b8a8_unorm_srgb;
		break;
	case reshadefx::texture_format::rgba16:
		format = api::format::r16g16b16a16_unorm;
		break;
	case reshadefx::texture_format::rgba16f:
		format = api::format::r16g16b16a16_float;
		break;
	case reshadefx::texture_format::rgba32f:
		format = api::format::r32g32b32a32_float;
		break;
	case reshadefx::texture_format::rgb10a2:
		format = api::format::r10g10b10a2_unorm;
		break;
	}

	if (view_format == api::format::unknown)
		view_format_srgb = view_format = format;

	api::resource_usage usage = api::resource_usage::shader_resource;
	usage |= api::resource_usage::copy_source; // For texture data download
	if (tex.semantic.empty())
		usage |= api::resource_usage::copy_dest; // For texture data upload
	if (tex.render_target)
		usage |= api::resource_usage::render_target;
	if (tex.storage_access && _renderer_id >= 0xb000)
		usage |= api::resource_usage::unordered_access;

	api::resource_flags flags = api::resource_flags::none;
	if (tex.levels > 1)
		flags |= api::resource_flags::generate_mipmaps;

	// Clear texture to zero since by default its contents are undefined
	std::vector<uint8_t> zero_data(static_cast<size_t>(tex.width) * static_cast<size_t>(tex.height) * 16);
	std::vector<api::subresource_data> initial_data(tex.levels);
	for (uint32_t level = 0, width = tex.width; level < tex.levels; ++level, width /= 2)
	{
		initial_data[level].data = zero_data.data();
		initial_data[level].row_pitch = width * 16;
	}

	if (!_device->create_resource(api::resource_desc(tex.width, tex.height, 1, tex.levels, format, 1, api::memory_heap::gpu_only, usage, flags), initial_data.data(), api::resource_usage::shader_resource, &tex.resource))
	{
		LOG(ERROR) << "Failed to create texture '" << tex.unique_name << "'!";
		LOG(DEBUG) << "> Details: Width = " << tex.width << ", Height = " << tex.height << ", Levels = " << tex.levels << ", Format = " << static_cast<uint32_t>(format) << ", Usage = " << std::hex << static_cast<uint32_t>(usage) << std::dec;
		return false;
	}

	_device->set_resource_name(tex.resource, tex.unique_name.c_str());

	// Always create shader resource views
	{
		if (!_device->create_resource_view(tex.resource, api::resource_usage::shader_resource, api::resource_view_desc(view_format, 0, tex.levels, 0, 1), &tex.srv[0]))
		{
			LOG(ERROR) << "Failed to create shader resource view for texture '" << tex.unique_name << "'!";
			LOG(DEBUG) << "> Details: Format = " << static_cast<uint32_t>(view_format) << ", Levels = " << tex.levels;
			return false;
		}
		if (view_format_srgb == view_format || tex.storage_access) // sRGB formats do not support storage usage
		{
			tex.srv[1] = tex.srv[0];
		}
		else if (!_device->create_resource_view(tex.resource, api::resource_usage::shader_resource, api::resource_view_desc(view_format_srgb, 0, tex.levels, 0, 1), &tex.srv[1]))
		{
			LOG(ERROR) << "Failed to create shader resource view for texture '" << tex.unique_name << "'!";
			LOG(DEBUG) << "> Details: Format = " << static_cast<uint32_t>(view_format_srgb) << ", Levels = " << tex.levels;
			return false;
		}
	}

	// Create render target views (with a single level)
	if (tex.render_target)
	{
		if (!_device->create_resource_view(tex.resource, api::resource_usage::render_target, api::resource_view_desc(view_format), &tex.rtv[0]))
		{
			LOG(ERROR) << "Failed to create render target view for texture '" << tex.unique_name << "'!";
			LOG(DEBUG) << "> Details: Format = " << static_cast<uint32_t>(view_format);
			return false;
		}
		if (view_format_srgb == view_format)
		{
			tex.rtv[1] = tex.rtv[0];
		}
		else if (!_device->create_resource_view(tex.resource, api::resource_usage::render_target, api::resource_view_desc(view_format_srgb), &tex.rtv[1]))
		{
			LOG(ERROR) << "Failed to create render target view for texture '" << tex.unique_name << "'!";
			LOG(DEBUG) << "> Details: Format = " << static_cast<uint32_t>(view_format_srgb);
			return false;
		}
	}

	if (tex.storage_access && _renderer_id >= 0xb000)
	{
		if (!_device->create_resource_view(tex.resource, api::resource_usage::unordered_access, api::resource_view_desc(view_format), &tex.uav))
		{
			LOG(ERROR) << "Failed to create unordered access view for texture '" << tex.unique_name << "'!";
			LOG(DEBUG) << "> Details: Format = " << static_cast<uint32_t>(view_format);
			return false;
		}
	}

	return true;
}
void reshade::runtime::destroy_texture(texture &tex)
{
	_device->destroy_resource(tex.resource);
	tex.resource = {};

	_device->destroy_resource_view(tex.srv[0]);
	if (tex.srv[1] != tex.srv[0])
		_device->destroy_resource_view(tex.srv[1]);
	tex.srv[0] = {};
	tex.srv[1] = {};

	_device->destroy_resource_view(tex.rtv[0]);
	if (tex.rtv[1] != tex.rtv[0])
		_device->destroy_resource_view(tex.rtv[1]);
	tex.rtv[0] = {};
	tex.rtv[1] = {};

	_device->destroy_resource_view(tex.uav);
	tex.uav = {};
}

void reshade::runtime::enable_technique(technique &tech)
{
	assert(tech.effect_index < _effects.size());

	if (!_effects[tech.effect_index].compiled)
		return; // Cannot enable techniques that failed to compile

	const bool status_changed = !tech.enabled;
	tech.enabled = true;
	tech.time_left = tech.annotation_as_int("timeout");

	// Queue effect file for initialization if it was not fully loaded yet
	if (tech.passes_data.empty() &&
		// Avoid adding the same effect multiple times to the queue if it contains multiple techniques that were enabled simultaneously
		std::find(_reload_create_queue.begin(), _reload_create_queue.end(), tech.effect_index) == _reload_create_queue.end())
		_reload_create_queue.push_back(tech.effect_index);

	if (status_changed) // Increase rendering reference count
		_effects[tech.effect_index].rendering++;
}
void reshade::runtime::disable_technique(technique &tech)
{
	assert(tech.effect_index < _effects.size());

	const bool status_changed = tech.enabled;
	tech.enabled = false;
	tech.time_left = 0;
	tech.average_cpu_duration.clear();
	tech.average_gpu_duration.clear();

	if (status_changed) // Decrease rendering reference count
		_effects[tech.effect_index].rendering--;
}

void reshade::runtime::load_effects()
{
	// Reload preprocessor definitions from current preset before compiling
	_preset_preprocessor_definitions.clear();
	ini_file &preset = ini_file::load_cache(_current_preset_path);
	preset.get({}, "PreprocessorDefinitions", _preset_preprocessor_definitions);

	// Build a list of effect files by walking through the effect search paths
	const std::vector<std::filesystem::path> effect_files =
		find_files(_effect_search_paths, { L".fx" });

	if (effect_files.empty())
		return; // No effect files found, so nothing more to do

	// Have to be initialized at this point or else the threads spawned below will immediately exit without reducing the remaining effects count
	assert(_is_initialized);

	// Allocate space for effects which are placed in this array during the 'load_effect' call
	const size_t offset = _effects.size();
	_effects.resize(offset + effect_files.size());
	_reload_remaining_effects = effect_files.size();

	// Now that we have a list of files, load them in parallel
	// Split workload into batches instead of launching a thread for every file to avoid launch overhead and stutters due to too many threads being in flight
	const size_t num_splits = std::min<size_t>(effect_files.size(), std::max<size_t>(std::thread::hardware_concurrency(), 2u) - 1);

	// Keep track of the spawned threads, so the runtime cannot be destroyed while they are still running
	for (size_t n = 0; n < num_splits; ++n)
		// Create copy of preset instead of reference, so it stays valid even if 'ini_file::load_cache' is called while effects are still being loaded
		_worker_threads.emplace_back([this, effect_files, offset, num_splits, n, preset]() {
			// Abort loading when initialization state changes (indicating that 'on_reset' was called in the meantime)
			for (size_t i = 0; i < effect_files.size() && _is_initialized; ++i)
				if (i * num_splits / effect_files.size() == n)
					load_effect(effect_files[i], preset, offset + i);
		});
}
void reshade::runtime::load_textures()
{
	_last_texture_reload_successfull = true;

	LOG(INFO) << "Loading image files for textures ...";

	for (texture &texture : _textures)
	{
		if (texture.resource == 0 || !texture.semantic.empty())
			continue; // Ignore textures that are not created yet and those that are handled in the runtime implementation

		std::filesystem::path source_path = std::filesystem::u8path(texture.annotation_as_string("source"));
		// Ignore textures that have no image file attached to them (e.g. plain render targets)
		if (source_path.empty())
			continue;

		// Search for image file using the provided search paths unless the path provided is already absolute
		if (!find_file(_texture_search_paths, source_path))
		{
			LOG(ERROR) << "Source " << source_path << " for texture '" << texture.unique_name << "' could not be found in any of the texture search paths!";
			_last_texture_reload_successfull = false;
			continue;
		}

		stbi_uc *filedata = nullptr;
		int width = 0, height = 0, channels = 0;

		if (FILE *file; _wfopen_s(&file, source_path.c_str(), L"rb") == 0)
		{
			// Read texture data into memory in one go since that is faster than reading chunk by chunk
			std::vector<uint8_t> mem(static_cast<size_t>(std::filesystem::file_size(source_path)));
			fread(mem.data(), 1, mem.size(), file);
			fclose(file);

			if (stbi_dds_test_memory(mem.data(), static_cast<int>(mem.size())))
				filedata = stbi_dds_load_from_memory(mem.data(), static_cast<int>(mem.size()), &width, &height, &channels, STBI_rgb_alpha);
			else
				filedata = stbi_load_from_memory(mem.data(), static_cast<int>(mem.size()), &width, &height, &channels, STBI_rgb_alpha);
		}

		if (filedata == nullptr)
		{
			LOG(ERROR) << "Source " << source_path << " for texture '" << texture.unique_name << "' could not be loaded! Make sure it is of a compatible file format.";
			_last_texture_reload_successfull = false;
			continue;
		}

		update_texture({ reinterpret_cast<uintptr_t>(&texture) }, width, height, filedata);

		stbi_image_free(filedata);

		texture.loaded = true;
	}

	_textures_loaded = true;
}
bool reshade::runtime::reload_effect(size_t effect_index, bool preprocess_required)
{
#if RESHADE_GUI
	_show_splash = false; // Hide splash bar when reloading a single effect file
#endif

	const std::filesystem::path source_file = _effects[effect_index].source_file;
	destroy_effect(effect_index);
	return load_effect(source_file, ini_file::load_cache(_current_preset_path), effect_index, preprocess_required);
}
void reshade::runtime::reload_effects()
{
	// Clear out any previous effects
	destroy_effects();

#if RESHADE_GUI
	_show_splash = true; // Always show splash bar when reloading everything
	_reload_count++;
#endif
	_last_reload_successfull = true;

	load_effects();
}
void reshade::runtime::destroy_effects()
{
	// Make sure no threads are still accessing effect data
	for (std::thread &thread : _worker_threads)
		if (thread.joinable())
			thread.join();
	_worker_threads.clear();

	for (size_t effect_index = 0; effect_index < _effects.size(); ++effect_index)
		destroy_effect(effect_index);

	// Clean up sampler objects
	for (const auto &[hash, sampler] : _effect_sampler_states)
		_device->destroy_sampler(sampler);
	_effect_sampler_states.clear();

	// Reset the effect list after all resources have been destroyed
	_effects.clear();

	// Textures and techniques should have been cleaned up by the calls to 'destroy_effect' above
	assert(_textures.empty());
	assert(_techniques.empty());

	_textures_loaded = false;
}

bool reshade::runtime::load_effect_cache(const std::string &id, const std::string &type, std::string &data) const
{
	if (_no_effect_cache)
		return false;

	std::filesystem::path path = g_reshade_base_path / _intermediate_cache_path;
	path /= std::filesystem::u8path("reshade-" + id + '.' + type);

	{	const HANDLE file = CreateFileW(path.c_str(), FILE_GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return false;
		DWORD size = GetFileSize(file, nullptr);
		data.resize(size);
		const BOOL result = ReadFile(file, data.data(), size, &size, nullptr);
		CloseHandle(file);
		return result != FALSE;
	}
}
bool reshade::runtime::save_effect_cache(const std::string &id, const std::string &type, const std::string &data) const
{
	if (_no_effect_cache)
		return false;

	std::filesystem::path path = g_reshade_base_path / _intermediate_cache_path;
	path /= std::filesystem::u8path("reshade-" + id + '.' + type);

	{	const HANDLE file = CreateFileW(path.c_str(), FILE_GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_ARCHIVE | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return false;
		DWORD size = static_cast<DWORD>(data.size());
		const BOOL result = WriteFile(file, data.data(), size, &size, nullptr);
		CloseHandle(file);
		return result != FALSE;
	}
}
void reshade::runtime::clear_effect_cache()
{
	// Find all cached effect files and delete them
	std::error_code ec;
	for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(g_reshade_base_path / _intermediate_cache_path, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		if (entry.is_directory(ec))
			continue;

		const std::filesystem::path filename = entry.path().filename();
		const std::filesystem::path extension = entry.path().extension();
		if (filename.native().compare(0, 8, L"reshade-") != 0 || (extension != L".i" && extension != L".cso" && extension != L".asm"))
			continue;

		DeleteFileW(entry.path().c_str());
	}
}

void reshade::runtime::update_effects()
{
	// Delay first load to the first render call to avoid loading while the application is still initializing
	if (_framecount == 0 && !_no_reload_on_init && !(_no_reload_for_non_vr && !_is_vr))
		reload_effects();

	if (_reload_remaining_effects == 0)
	{
		// Clear the thread list now that they all have finished
		for (std::thread &thread : _worker_threads)
			if (thread.joinable())
				thread.join(); // Threads have exited, but still need to join them prior to destruction
		_worker_threads.clear();

		// Finished loading effects, so apply preset to figure out which ones need compiling
		load_current_preset();

		_last_reload_time = std::chrono::high_resolution_clock::now();
		_reload_remaining_effects = std::numeric_limits<size_t>::max();

		// Reset all effect loading options
		_load_option_disable_skipping = false;

#if RESHADE_GUI
		// Update all editors after a reload
		for (editor_instance &instance : _editors)
		{
			if (const auto it = std::find_if(_effects.begin(), _effects.end(),
				[this, &instance](const auto &effect) { return effect.source_file == instance.file_path; }); it != _effects.end())
			{
				instance.effect_index = std::distance(_effects.begin(), it);

				if (instance.entry_point_name.empty() || instance.entry_point_name == "Generated code")
					open_code_editor(instance);
				else
					instance.editor.clear_text();
			}
		}
#endif
	}
	else if (_reload_remaining_effects != std::numeric_limits<size_t>::max())
	{
		return; // Cannot render while effects are still being loaded
	}
	else if (!_reload_create_queue.empty())
	{
		// Pop an effect from the queue
		const size_t effect_index = _reload_create_queue.back();
		_reload_create_queue.pop_back();

		if (!create_effect(effect_index))
		{
			// Destroy all textures belonging to this effect
			for (texture &tex : _textures)
				if (tex.effect_index == effect_index && tex.shared.size() <= 1)
					destroy_texture(tex);
			// Disable all techniques belonging to this effect
			for (technique &tech : _techniques)
				if (tech.effect_index == effect_index)
					disable_technique(tech);

			_last_reload_successfull = false;
		}

		// An effect has changed, need to reload textures
		_textures_loaded = false;

#if RESHADE_GUI
		effect &effect = _effects[effect_index];

		// Update assembly in all editors after a reload
		for (editor_instance &instance : _editors)
		{
			if (instance.entry_point_name.empty() || instance.file_path != effect.source_file)
				continue;
			assert(instance.effect_index == effect_index);

			if (const auto assembly_it = effect.assembly.find(instance.entry_point_name);
				assembly_it != effect.assembly.end())
				open_code_editor(instance);
		}
#endif

#if RESHADE_ADDON
		if (_reload_create_queue.empty())
			invoke_addon_event<addon_event::reshade_reloaded_effects>(this);
#endif
	}
	else if (!_textures_loaded)
	{
		// Now that all effects were compiled, load all textures
		load_textures();
	}
}
void reshade::runtime::render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	_effects_rendered_this_frame = true;

	if (is_loading() || rtv == 0)
		return;

	if (rtv_srgb == 0)
		rtv_srgb = rtv;

#ifdef NDEBUG
	// Lock input so it cannot be modified by other threads while we are reading it here
	// TODO: This does not catch input happening between now and 'on_present'
	const auto input_lock = _input->lock();
#endif

	// Nothing to do here if effects are disabled globally
	if (!_effects_enabled || _techniques.empty())
		return;

	// Update special uniform variables
	for (effect &effect : _effects)
	{
		if (!effect.rendering)
			continue;

		for (uniform &variable : effect.uniforms)
		{
			if (!_ignore_shortcuts && _input->is_key_pressed(variable.toggle_key_data, _force_shortcut_modifiers))
			{
				assert(variable.supports_toggle_key());

				// Change to next value if the associated shortcut key was pressed
				switch (variable.type.base)
				{
					case reshadefx::type::t_bool:
					{
						bool data;
						get_uniform_value(variable, &data);
						set_uniform_value(variable, !data);
						break;
					}
					case reshadefx::type::t_int:
					case reshadefx::type::t_uint:
					{
						int data[4];
						get_uniform_value(variable, data, 4);
						const std::string_view ui_items = variable.annotation_as_string("ui_items");
						int num_items = 0;
						for (size_t offset = 0, next; (next = ui_items.find('\0', offset)) != std::string::npos; offset = next + 1)
							num_items++;
						data[0] = (data[0] + 1 >= num_items) ? 0 : data[0] + 1;
						set_uniform_value(variable, data, 4);
						break;
					}
				}
				save_current_preset();
			}

			switch (variable.special)
			{
				case special_uniform::frame_time:
				{
					set_uniform_value(variable, _last_frame_duration.count() * 1e-6f);
					break;
				}
				case special_uniform::frame_count:
				{
					if (variable.type.is_boolean())
						set_uniform_value(variable, (_framecount % 2) == 0);
					else
						set_uniform_value(variable, static_cast<unsigned int>(_framecount % UINT_MAX));
					break;
				}
				case special_uniform::random:
				{
					const int min = variable.annotation_as_int("min", 0, 0);
					const int max = variable.annotation_as_int("max", 0, RAND_MAX);
					set_uniform_value(variable, min + (std::rand() % (std::abs(max - min) + 1)));
					break;
				}
				case special_uniform::ping_pong:
				{
					const float min = variable.annotation_as_float("min", 0, 0.0f);
					const float max = variable.annotation_as_float("max", 0, 1.0f);
					const float step_min = variable.annotation_as_float("step", 0);
					const float step_max = variable.annotation_as_float("step", 1);
					float increment = step_max == 0 ? step_min : (step_min + std::fmodf(static_cast<float>(std::rand()), step_max - step_min + 1));
					const float smoothing = variable.annotation_as_float("smoothing");

					float value[2] = { 0, 0 };
					get_uniform_value(variable, value, 2);
					if (value[1] >= 0)
					{
						increment = std::max(increment - std::max(0.0f, smoothing - (max - value[0])), 0.05f);
						increment *= _last_frame_duration.count() * 1e-9f;

						if ((value[0] += increment) >= max)
							value[0] = max, value[1] = -1;
					}
					else
					{
						increment = std::max(increment - std::max(0.0f, smoothing - (value[0] - min)), 0.05f);
						increment *= _last_frame_duration.count() * 1e-9f;

						if ((value[0] -= increment) <= min)
							value[0] = min, value[1] = +1;
					}
					set_uniform_value(variable, value, 2);
					break;
				}
				case special_uniform::date:
				{
					const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
					tm tm; localtime_s(&tm, &t);

					const int value[4] = {
						tm.tm_year + 1900,
						tm.tm_mon + 1,
						tm.tm_mday,
						tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec
					};
					set_uniform_value(variable, value, 4);
					break;
				}
				case special_uniform::timer:
				{
					const unsigned long long timer_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_last_present_time - _start_time).count();
					set_uniform_value(variable, static_cast<unsigned int>(timer_ms));
					break;
				}
				case special_uniform::key:
				{
					if (const int keycode = variable.annotation_as_int("keycode");
						keycode > 7 && keycode < 256)
					{
						if (const std::string_view mode = variable.annotation_as_string("mode");
							mode == "toggle" || variable.annotation_as_int("toggle"))
						{
							bool current_value = false;
							get_uniform_value(variable, &current_value);
							if (_input->is_key_pressed(keycode))
								set_uniform_value(variable, !current_value);
						}
						else if (mode == "press")
							set_uniform_value(variable, _input->is_key_pressed(keycode));
						else
							set_uniform_value(variable, _input->is_key_down(keycode));
					}
					break;
				}
				case special_uniform::mouse_point:
				{
					uint32_t pos_x = _input->mouse_position_x();
					uint32_t pos_y = _input->mouse_position_y();
#if RESHADE_GUI
					if (pos_x > _window_width)
						pos_x = _window_width;
					if (pos_y > _window_height)
						pos_y = _window_height;
#endif

					set_uniform_value(variable, pos_x, pos_y);
					break;
				}
				case special_uniform::mouse_delta:
				{
					set_uniform_value(variable, _input->mouse_movement_delta_x(), _input->mouse_movement_delta_y());
					break;
				}
				case special_uniform::mouse_button:
				{
					if (const int keycode = variable.annotation_as_int("keycode");
						keycode >= 0 && keycode < 5)
					{
						if (const std::string_view mode = variable.annotation_as_string("mode");
							mode == "toggle" || variable.annotation_as_int("toggle"))
						{
							bool current_value = false;
							get_uniform_value(variable, &current_value);
							if (_input->is_mouse_button_pressed(keycode))
								set_uniform_value(variable, !current_value);
						}
						else if (mode == "press")
							set_uniform_value(variable, _input->is_mouse_button_pressed(keycode));
						else
							set_uniform_value(variable, _input->is_mouse_button_down(keycode));
					}
					break;
				}
				case special_uniform::mouse_wheel:
				{
					const float min = variable.annotation_as_float("min");
					const float max = variable.annotation_as_float("max");
					float step = variable.annotation_as_float("step");
					if (step == 0.0f)
						step  = 1.0f;

					float value[2] = { 0, 0 };
					get_uniform_value(variable, value, 2);
					value[1] = _input->mouse_wheel_delta();
					value[0] = value[0] + value[1] * step;
					if (min != max)
					{
						value[0] = std::max(value[0], min);
						value[0] = std::min(value[0], max);
					}
					set_uniform_value(variable, value, 2);
					break;
				}
				case special_uniform::freepie:
				{
					if (freepie_io_data data;
						freepie_io_read(variable.annotation_as_int("index"), &data))
						set_uniform_value(variable, &data.yaw, 3 * 2);
					break;
				}
#if RESHADE_GUI
				case special_uniform::overlay_open:
				{
					set_uniform_value(variable, _show_overlay);
					break;
				}
				case special_uniform::overlay_active:
				case special_uniform::overlay_hovered:
				{
					// These are set in 'draw_variable_editor' when overlay is open
					if (!_show_overlay)
						set_uniform_value(variable, 0);
					break;
				}
#endif
			}
		}
	}

#if RESHADE_ADDON
	invoke_addon_event<addon_event::reshade_begin_effects>(this, cmd_list, rtv, rtv_srgb);
#endif

	// Render all enabled techniques
	for (technique &tech : _techniques)
	{
		if (!_ignore_shortcuts && _input->is_key_pressed(tech.toggle_key_data, _force_shortcut_modifiers))
		{
			if (!tech.enabled)
				enable_technique(tech);
			else
				disable_technique(tech);
		}

		if (tech.passes_data.empty() || !tech.enabled)
			continue; // Ignore techniques that are not fully loaded or currently disabled

		const auto time_technique_started = std::chrono::high_resolution_clock::now();
		render_technique(cmd_list, tech, rtv, rtv_srgb);
		const auto time_technique_finished = std::chrono::high_resolution_clock::now();

		tech.average_cpu_duration.append(std::chrono::duration_cast<std::chrono::nanoseconds>(time_technique_finished - time_technique_started).count());

		if (tech.time_left > 0)
		{
			tech.time_left -= std::chrono::duration_cast<std::chrono::milliseconds>(_last_frame_duration).count();
			if (tech.time_left <= 0)
				disable_technique(tech);
		}
	}

#if RESHADE_ADDON
	invoke_addon_event<addon_event::reshade_finish_effects>(this, cmd_list, rtv, rtv_srgb);
#endif
}
void reshade::runtime::render_technique(api::command_list *cmd_list, technique &tech, api::resource_view back_buffer_rtv, api::resource_view back_buffer_rtv_srgb)
{
	const effect &effect = _effects[tech.effect_index];

#if RESHADE_GUI
	if (_gather_gpu_statistics)
	{
		// Evaluate queries from oldest frame in queue
		if (uint64_t timestamps[2];
			_device->get_query_pool_results(effect.query_pool, tech.query_base_index + ((_framecount + 1) % 4) * 2, 2, timestamps, sizeof(uint64_t)))
			tech.average_gpu_duration.append(timestamps[1] - timestamps[0]);

		cmd_list->end_query(effect.query_pool, api::query_type::timestamp, tech.query_base_index + (_framecount % 4) * 2);
	}
#endif

#ifndef NDEBUG
	const float debug_event_col[4] = { 1.0f, 0.8f, 0.8f, 1.0f };
	cmd_list->begin_debug_event(tech.name.c_str(), debug_event_col);
#endif

	// Update shader constants
	if (void *mapped_uniform_data; effect.cb != 0 &&
		_device->map_buffer_region(effect.cb, 0, std::numeric_limits<uint64_t>::max(), api::map_access::write_discard, &mapped_uniform_data))
	{
		std::memcpy(mapped_uniform_data, effect.uniform_data_storage.data(), effect.uniform_data_storage.size());
		_device->unmap_buffer_region(effect.cb);
	}
	else if (_renderer_id == 0x9000)
	{
		cmd_list->push_constants(api::shader_stage::all, effect.layout, 0, 0, static_cast<uint32_t>(effect.uniform_data_storage.size() / sizeof(uint32_t)), effect.uniform_data_storage.data());
	}

	const bool sampler_with_resource_view = _device->check_capability(api::device_caps::sampler_with_resource_view);

	bool is_effect_stencil_cleared = false;
	bool needs_implicit_back_buffer_copy = true; // First pass always needs the back buffer updated

	const api::resource back_buffer_resource = _device->get_resource_from_view(back_buffer_rtv);

	for (size_t pass_index = 0; pass_index < tech.passes.size(); ++pass_index)
	{
		if (needs_implicit_back_buffer_copy)
		{
			// Save back buffer of previous pass
			const api::resource resources[2] = { back_buffer_resource, _effect_color_tex };
			const api::resource_usage state_old[2] = { api::resource_usage::render_target, api::resource_usage::shader_resource };
			const api::resource_usage state_new[2] = { api::resource_usage::copy_source, api::resource_usage::copy_dest };

			cmd_list->barrier(2, resources, state_old, state_new);
			cmd_list->copy_resource(back_buffer_resource, _effect_color_tex);
			cmd_list->barrier(2, resources, state_new, state_old);
		}

		const reshadefx::pass_info &pass_info = tech.passes[pass_index];
		const technique::pass_data &pass_data = tech.passes_data[pass_index];

#ifndef NDEBUG
		cmd_list->begin_debug_event((pass_info.name.empty() ? "Pass " + std::to_string(pass_index) : pass_info.name).c_str(), debug_event_col);
#endif

		const uint32_t num_barriers = static_cast<uint32_t>(pass_data.modified_resources.size());

		if (!pass_info.cs_entry_point.empty())
		{
			// Compute shaders do not write to the back buffer, so no update necessary
			needs_implicit_back_buffer_copy = false;

			cmd_list->bind_pipeline(api::pipeline_stage::all_compute, pass_data.pipeline);

			std::vector<api::resource_usage> state_old(num_barriers, api::resource_usage::shader_resource);
			std::vector<api::resource_usage> state_new(num_barriers, api::resource_usage::unordered_access);
			cmd_list->barrier(num_barriers, pass_data.modified_resources.data(), state_old.data(), state_new.data());

			// Reset bindings on every pass (since they get invalidated by the call to 'generate_mipmaps' below)
			if (effect.cb != 0)
				cmd_list->bind_descriptor_set(api::shader_stage::all_compute, effect.layout, 0, effect.cb_set);
			if (effect.sampler_set != 0)
				assert(!sampler_with_resource_view),
				cmd_list->bind_descriptor_set(api::shader_stage::all_compute, effect.layout, 1, effect.sampler_set);
			if (pass_data.texture_set != 0)
				cmd_list->bind_descriptor_set(api::shader_stage::all_compute, effect.layout, sampler_with_resource_view ? 1 : 2, pass_data.texture_set);
			if (pass_data.storage_set != 0)
				cmd_list->bind_descriptor_set(api::shader_stage::all_compute, effect.layout, sampler_with_resource_view ? 2 : 3, pass_data.storage_set);

			cmd_list->dispatch(pass_info.viewport_width, pass_info.viewport_height, pass_info.viewport_dispatch_z);

			cmd_list->barrier(num_barriers, pass_data.modified_resources.data(), state_new.data(), state_old.data());
		}
		else
		{
			cmd_list->bind_pipeline(api::pipeline_stage::all_graphics, pass_data.pipeline);

			// Transition resource state for render targets
			std::vector<api::resource_usage> state_old(num_barriers, api::resource_usage::shader_resource);
			std::vector<api::resource_usage> state_new(num_barriers, api::resource_usage::render_target);
			cmd_list->barrier(num_barriers, pass_data.modified_resources.data(), state_old.data(), state_new.data());

			// Setup render targets
			uint32_t render_target_count = 0;
			api::render_pass_depth_stencil_desc depth_stencil = {};
			api::render_pass_render_target_desc render_target[8] = {};

			if (pass_info.clear_render_targets)
			{
				for (int i = 0; i < 8; ++i)
					render_target[i].load_op = api::render_pass_load_op::clear;
			}

			// First pass to use the stencil buffer should clear it
			if (pass_info.stencil_enable && !is_effect_stencil_cleared)
			{
				is_effect_stencil_cleared = true;

				depth_stencil.stencil_load_op = api::render_pass_load_op::clear;
			}

			if (pass_info.render_target_names[0].empty())
			{
				needs_implicit_back_buffer_copy = true;

				depth_stencil.view = _effect_stencil_dsv;
				render_target[0].view = pass_info.srgb_write_enable ? back_buffer_rtv_srgb : back_buffer_rtv;
				render_target_count = 1;
			}
			else
			{
				needs_implicit_back_buffer_copy = false;

				if (pass_info.stencil_enable &&
					pass_info.viewport_width == _width &&
					pass_info.viewport_height == _height)
					depth_stencil.view = _effect_stencil_dsv;

				for (int i = 0; i < 8 && pass_data.render_target_views[i] != 0; ++i, ++render_target_count)
					render_target[i].view = pass_data.render_target_views[i];
			}

			cmd_list->begin_render_pass(render_target_count, render_target, depth_stencil.view != 0 ? &depth_stencil : nullptr);

			// Reset bindings on every pass (since they get invalidated by the call to 'generate_mipmaps' below)
			if (effect.cb != 0)
				cmd_list->bind_descriptor_set(api::shader_stage::all_graphics, effect.layout, 0, effect.cb_set);
			if (effect.sampler_set != 0)
				assert(!sampler_with_resource_view),
				cmd_list->bind_descriptor_set(api::shader_stage::all_graphics, effect.layout, 1, effect.sampler_set);
			// Setup shader resources after binding render targets, to ensure any OM bindings by the application are unset at this point (e.g. a depth buffer that was bound to the OM and is now bound as shader resource)
			if (pass_data.texture_set != 0)
				cmd_list->bind_descriptor_set(api::shader_stage::all_graphics, effect.layout, sampler_with_resource_view ? 1 : 2, pass_data.texture_set);

			const api::viewport viewport = {
				0.0f, 0.0f,
				static_cast<float>(pass_info.viewport_width),
				static_cast<float>(pass_info.viewport_height),
				0.0f, 1.0f
			};
			cmd_list->bind_viewports(0, 1, &viewport);
			const api::rect scissor_rect = {
				0, 0,
				static_cast<int32_t>(pass_info.viewport_width),
				static_cast<int32_t>(pass_info.viewport_height)
			};
			cmd_list->bind_scissor_rects(0, 1, &scissor_rect);

			if (_renderer_id == 0x9000)
			{
				// Set __TEXEL_SIZE__ constant (see effect_codegen_hlsl.cpp)
				const float texel_size[4] = {
					-1.0f / pass_info.viewport_width,
					 1.0f / pass_info.viewport_height
				};
				cmd_list->push_constants(api::shader_stage::vertex, effect.layout, 0, 255 * 4, 4, texel_size);
			}

			// Draw primitives
			cmd_list->draw(pass_info.num_vertices, 1, 0, 0);

			cmd_list->end_render_pass();

			// Transition resource state back to shader access
			cmd_list->barrier(num_barriers, pass_data.modified_resources.data(), state_new.data(), state_old.data());
		}

		// Generate mipmaps for modified resources
		for (const api::resource_view modified_texture : pass_data.generate_mipmap_views)
			cmd_list->generate_mipmaps(modified_texture);

#ifndef NDEBUG
		cmd_list->end_debug_event();
#endif
	}

#ifndef NDEBUG
	cmd_list->end_debug_event();
#endif

#if RESHADE_GUI
	if (_gather_gpu_statistics)
		cmd_list->end_query(effect.query_pool, api::query_type::timestamp, tech.query_base_index + (_framecount % 4) * 2 + 1);
#endif
}

void reshade::runtime::save_texture(const texture &tex)
{
	char timestamp[21];
	const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	tm tm; localtime_s(&tm, &t);
	sprintf_s(timestamp, " %.4d-%.2d-%.2d %.2d-%.2d-%.2d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	std::wstring filename = std::filesystem::path(tex.unique_name).concat(timestamp);
	filename += _screenshot_format == 0 ? L".bmp" : _screenshot_format == 1 ? L".png" : L".jpg";

	std::filesystem::path screenshot_path = g_reshade_base_path / _screenshot_path / filename;

	_screenshot_save_success = false; // Default to a save failure unless it is reported to succeed below

	if (std::vector<uint8_t> data(static_cast<size_t>(tex.width) * static_cast<size_t>(tex.height * 4));
		get_texture_data(tex.resource, api::resource_usage::shader_resource, data.data()))
	{
		if (FILE *file; _wfopen_s(&file, screenshot_path.c_str(), L"wb") == 0)
		{
			const auto write_callback = [](void *context, void *data, int size) {
				fwrite(data, 1, size, static_cast<FILE *>(context));
			};

			switch (_screenshot_format)
			{
			case 0:
				_screenshot_save_success = stbi_write_bmp_to_func(write_callback, file, tex.width, tex.height, 4, data.data()) != 0;
				break;
			case 1:
				_screenshot_save_success = stbi_write_png_to_func(write_callback, file, tex.width, tex.height, 4, data.data(), 0) != 0;
				break;
			case 2:
				_screenshot_save_success = stbi_write_jpg_to_func(write_callback, file, tex.width, tex.height, 4, data.data(), _screenshot_jpeg_quality) != 0;
				break;
			}

			fclose(file);
		}
	}

	_last_screenshot_file = screenshot_path;
	_last_screenshot_time = std::chrono::high_resolution_clock::now();
}
#endif

void reshade::runtime::save_screenshot(const std::wstring &postfix)
{
	char timestamp[21];
	const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	tm tm; localtime_s(&tm, &t);
	sprintf_s(timestamp, " %.4d-%.2d-%.2d %.2d-%.2d-%.2d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	std::wstring filename = g_target_executable_path.stem().concat(timestamp);
#if RESHADE_FX
	if (_screenshot_naming == 1)
		filename += L' ' + _current_preset_path.stem().wstring();
#endif

	filename += postfix;
	filename += _screenshot_format == 0 ? L".bmp" : _screenshot_format == 1 ? L".png" : L".jpg";

	std::filesystem::path screenshot_dir = g_reshade_base_path / _screenshot_path;
	std::filesystem::path screenshot_path = screenshot_dir / filename;

	LOG(INFO) << "Saving screenshot to " << screenshot_path << " ...";

	_screenshot_save_success = false; // Default to a save failure unless it is reported to succeed below
	_failed_to_create_screenshot_dir = false;

	std::error_code _;
	std::filesystem::create_directories(screenshot_dir, _);

	if (!std::filesystem::exists(screenshot_dir, _))
	{
		_failed_to_create_screenshot_dir = true;
	}
	else if (std::vector<uint8_t> data(static_cast<size_t>(_width) * static_cast<size_t>(_height) * 4);
		capture_screenshot(data.data()))
	{
		// Remove alpha channel
		int comp = 4;
		if (_screenshot_clear_alpha)
		{
			comp = 3;
			for (size_t h = 0; h < _height; ++h)
			{
				for (size_t w = 0; w < _width; ++w)
				{
					const size_t index = h * _width + w;
					data[index * 3 + 0] = data[index * 4 + 0];
					data[index * 3 + 1] = data[index * 4 + 1];
					data[index * 3 + 2] = data[index * 4 + 2];
				}
			}
		}

		if (FILE *file; _wfopen_s(&file, screenshot_path.c_str(), L"wb") == 0)
		{
			const auto write_callback = [](void *context, void *data, int size) {
				fwrite(data, 1, size, static_cast<FILE *>(context));
			};

			switch (_screenshot_format)
			{
			case 0:
				_screenshot_save_success = stbi_write_bmp_to_func(write_callback, file, _width, _height, comp, data.data()) != 0;
				break;
			case 1:
				_screenshot_save_success = stbi_write_png_to_func(write_callback, file, _width, _height, comp, data.data(), 0) != 0;
				break;
			case 2:
				_screenshot_save_success = stbi_write_jpg_to_func(write_callback, file, _width, _height, comp, data.data(), _screenshot_jpeg_quality) != 0;
				break;
			}

			fclose(file);
			execute_screenshot_post_save_command(screenshot_path);
		}
	}

	_last_screenshot_file = screenshot_path;
	_last_screenshot_time = std::chrono::high_resolution_clock::now();

	if (!_screenshot_save_success)
	{
		LOG(ERROR) << "Failed to write screenshot to " << screenshot_path << '!';
	}
#if RESHADE_FX
	else if (_screenshot_include_preset && postfix.empty() && ini_file::flush_cache(_current_preset_path))
	{
		// Preset was flushed to disk, so can just copy it over to the new location
		std::error_code ec; std::filesystem::copy_file(_current_preset_path, screenshot_path.replace_extension(L".ini"), std::filesystem::copy_options::overwrite_existing, ec);
	}
#endif
}
bool reshade::runtime::execute_screenshot_post_save_command(const std::filesystem::path &screenshot_path)
{
	int ec = 0;

	if (_screenshot_post_save_command.empty())
	{
		ec = -1;
		return false;
	}

	if (_screenshot_post_save_command.extension() != L".exe" || !std::filesystem::is_regular_file(g_reshade_base_path / _screenshot_post_save_command))
	{
		ec = 1;
		LOG(ERROR) << "Executing the post-save command was blocked. Application path must be executable.";
		return false;
	}

	std::string command_line;
	command_line.append(1, '\"');
	command_line.append((g_reshade_base_path / _screenshot_post_save_command).u8string());
	command_line.append(1, '\"');

	if (!_screenshot_post_save_command_arguments.empty())
	{
		command_line.append(1, ' ');

		for (size_t cursor = 0; cursor < _screenshot_post_save_command_arguments.size();)
		{
			size_t brace_begin = _screenshot_post_save_command_arguments.find('<', cursor);
			size_t brace_end = _screenshot_post_save_command_arguments.find('>', cursor + 1);

			if (brace_begin == std::string::npos || brace_end == std::string::npos)
			{
				command_line.append(_screenshot_post_save_command_arguments.substr(cursor));
				break;
			}
			else
			{
				command_line.append(_screenshot_post_save_command_arguments.substr(cursor, brace_begin - cursor));
			}

			std::string_view replacing = ((std::string_view)_screenshot_post_save_command_arguments).substr(brace_begin + 1, brace_end - (brace_begin + 1));
			size_t colon_pos = replacing.find(':');

			std::string name;
			if (colon_pos == std::string::npos)
				name = replacing;
			else
				name = replacing.substr(0, colon_pos);

			static const auto stringicmp = [](const std::string_view& a, const std::string_view& b) {
				return a.size() == b.size() && _strnicmp(a.data(), b.data(), a.size()) == 0;
			};

			std::string value;
			if (stringicmp(name, "TargetScreenshotPath") || stringicmp(name, "PATH") || stringicmp(name, "TARGET"))
			{
				value = screenshot_path.u8string();
			}
			else if (stringicmp(name, "TargetScreenshotDir") || stringicmp(name, "DIR"))
			{
				value = screenshot_path.parent_path().u8string();
			}
			else if (stringicmp(name, "TargetScreenshotFileName") || stringicmp(name, "FILENAME"))
			{
				value = screenshot_path.filename().u8string();
			}
			else if (stringicmp(name, "TargetScreenshotFileNameWithoutExtension") || stringicmp(name, "STEM"))
			{
				value = screenshot_path.stem().u8string();
			}

			std::string parameter;
			if (colon_pos == std::string::npos)
			{
				parameter = value;
			}
			else
			{
				parameter = replacing.substr(colon_pos + 1);
				size_t insert_pos = parameter.find('$');

				if (insert_pos != std::string::npos)
					parameter = parameter.substr(0, insert_pos) + value + parameter.substr(insert_pos + 1);
			}

			command_line.append(parameter);
			cursor = brace_end + 1;
		}
	}

	std::wstring command_line_wide;
	utf8::unchecked::utf8to16(command_line.cbegin(), command_line.cend(), std::back_inserter(command_line_wide));

	std::wstring working_directory;
	if (_screenshot_post_save_command_working_directory.empty() || !std::filesystem::is_directory(_screenshot_post_save_command_working_directory))
		working_directory = g_reshade_base_path;
	else
		working_directory = _screenshot_post_save_command_working_directory;

	DWORD process_creation_flags = NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP;
	STARTUPINFO si = { 0 };
	si.cb = sizeof(STARTUPINFO);

	if (!_screenshot_post_save_command_show_window)
	{
		process_creation_flags |= CREATE_NO_WINDOW;

		si.dwFlags |= STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
	}

	PROCESS_INFORMATION pi = { 0 };
	if (HANDLE process_token_handle = nullptr;
		!ec && (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &process_token_handle) != FALSE))
	{
		DWORD state_buffer_size;
		GetTokenInformation(process_token_handle, TOKEN_INFORMATION_CLASS::TokenPrivileges,  nullptr, 0, &state_buffer_size);

		PTOKEN_PRIVILEGES previous_state = (PTOKEN_PRIVILEGES)calloc(1, state_buffer_size);
		PTOKEN_PRIVILEGES new_state = (PTOKEN_PRIVILEGES)calloc(1, state_buffer_size);
		new_state->PrivilegeCount = 1;
		new_state->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		if (LookupPrivilegeValueW(nullptr, SE_INCREASE_QUOTA_NAME, &new_state->Privileges[0].Luid) != FALSE
		 && AdjustTokenPrivileges(process_token_handle, FALSE, new_state, state_buffer_size, previous_state, &state_buffer_size) != FALSE
		 && GetLastError() != ERROR_NOT_ALL_ASSIGNED)
		{
			// Current process is elevated

			HWND shell_window_handle = GetShellWindow();
			if (!ec && (shell_window_handle == nullptr))
			{
				ec = 2;
				LOG(ERROR) << "Failed to execute the post-save command, Application-user must have desktop shell.";
			}

			DWORD shell_process_id = 0;
			GetWindowThreadProcessId(shell_window_handle, &shell_process_id);

			HANDLE shell_process_handle = nullptr;
			if (!ec && (shell_process_id == 0 || (shell_process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, shell_process_id)) == nullptr))
			{
				ec = 3;
				LOG(ERROR) << "Failed to execute the post-save command, Application-user must have desktop shell, error code " << ec << ':' << GetLastError() << '.';
			}

			HANDLE desktop_token_handle = nullptr;
			if (!ec && (OpenProcessToken(shell_process_handle, TOKEN_DUPLICATE, &desktop_token_handle) == FALSE))
			{
				ec = 4;
				LOG(ERROR) << "Failed to execute the post-save command, Application-user must have desktop shell, error code " << ec << ':' << GetLastError() << '.';
			}

			HANDLE duplicated_token_handle = nullptr;
			if (!ec && (DuplicateTokenEx(desktop_token_handle, TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID, nullptr, SECURITY_IMPERSONATION_LEVEL::SecurityImpersonation, TOKEN_TYPE::TokenPrimary, &duplicated_token_handle) == FALSE))
			{
				ec = 5;
				LOG(ERROR) << "Failed to execute the post-save command, Application-user must have desktop shell, error code " << ec << ':' << GetLastError() << '.';
			}

			if (CreateProcessWithTokenW(duplicated_token_handle, 0, nullptr, command_line_wide.data(), process_creation_flags, nullptr, working_directory.data(), &si, &pi) == FALSE)
			{
				ec = 6;
				LOG(ERROR) << "Failed to execute the post-save command, Application-user must have desktop shell, error code " << ec << ':' << GetLastError() << '.';
			}

			if (duplicated_token_handle)
				CloseHandle(duplicated_token_handle);

			if (desktop_token_handle)
				CloseHandle(desktop_token_handle);

			if (shell_process_handle)
				CloseHandle(shell_process_handle);
		}
		else
		{
			// Current process is not elevated

			if (CreateProcessW(nullptr, command_line_wide.data(), nullptr, nullptr, FALSE, process_creation_flags, nullptr, working_directory.data(), &si, &pi) == FALSE)
			{
				ec = 7;
				LOG(ERROR) << "Failed to execute the post-save command, Application-user must have desktop shell, error code " << ec << ':' << GetLastError() << '.';
			}
		}

		if (previous_state->PrivilegeCount > 0)
			AdjustTokenPrivileges(process_token_handle, FALSE, previous_state, 0, nullptr, nullptr);

		if (process_token_handle)
			CloseHandle(process_token_handle);

		if (previous_state)
			free(previous_state);

		if (new_state)
			free(new_state);
	}

	return ec == 0;
}

bool reshade::runtime::get_texture_data(api::resource resource, api::resource_usage state, uint8_t *pixels)
{
	const api::resource_desc desc = _device->get_resource_desc(resource);
	const api::format view_format = api::format_to_default_typed(desc.texture.format, 0);

	if (view_format != api::format::r8_unorm &&
		view_format != api::format::r8g8_unorm &&
		view_format != api::format::r8g8b8a8_unorm &&
		view_format != api::format::b8g8r8a8_unorm &&
		view_format != api::format::r8g8b8x8_unorm &&
		view_format != api::format::b8g8r8x8_unorm &&
		view_format != api::format::r10g10b10a2_unorm &&
		view_format != api::format::b10g10r10a2_unorm)
	{
		LOG(ERROR) << "Screenshots are not supported for format " << static_cast<uint32_t>(desc.texture.format) << '!';
		return false;
	}

	uint32_t row_pitch = api::format_row_pitch(view_format, desc.texture.width);
	if (_device->get_api() == api::device_api::d3d12) // See D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
		row_pitch = (row_pitch + 255) & ~255;
	const uint32_t slice_pitch = api::format_slice_pitch(view_format, row_pitch, desc.texture.height);
	const uint32_t pixels_row_pitch = desc.texture.width * 4;

	// Copy back buffer data into system memory buffer
	api::resource intermediate;
	if (_device->check_capability(api::device_caps::copy_buffer_to_texture))
	{
		if (!_device->create_resource(api::resource_desc(slice_pitch, api::memory_heap::gpu_to_cpu, api::resource_usage::copy_dest), nullptr, api::resource_usage::copy_dest, &intermediate))
		{
			LOG(ERROR) << "Failed to create system memory buffer for screenshot capture!";
			return false;
		}

		_device->set_resource_name(intermediate, "ReShade screenshot buffer");

		api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();
		cmd_list->barrier(resource, state, api::resource_usage::copy_source);
		cmd_list->copy_texture_to_buffer(resource, 0, nullptr, intermediate, 0, desc.texture.width, desc.texture.height);
		cmd_list->barrier(resource, api::resource_usage::copy_source, state);
	}
	else
	{
		if (!_device->create_resource(api::resource_desc(desc.texture.width, desc.texture.height, 1, 1, view_format, 1, api::memory_heap::gpu_to_cpu, api::resource_usage::copy_dest), nullptr, api::resource_usage::copy_dest, &intermediate))
		{
			LOG(ERROR) << "Failed to create system memory texture for screenshot capture!";
			return false;
		}

		_device->set_resource_name(intermediate, "ReShade screenshot texture");

		api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();
		cmd_list->barrier(resource, state, api::resource_usage::copy_source);
		cmd_list->copy_texture_region(resource, 0, nullptr, intermediate, 0, nullptr);
		cmd_list->barrier(resource, api::resource_usage::copy_source, state);
	}

	// Wait for any rendering by the application finish before submitting
	// It may have submitted that to a different queue, so simply wait for all to idle here
	_graphics_queue->wait_idle();

	// Copy data from intermediate image into output buffer
	api::subresource_data mapped_data = {};
	if (_device->check_capability(api::device_caps::copy_buffer_to_texture))
	{
		_device->map_buffer_region(intermediate, 0, std::numeric_limits<uint64_t>::max(), api::map_access::read_only, &mapped_data.data);

		mapped_data.row_pitch = row_pitch;
		mapped_data.slice_pitch = slice_pitch;
	}
	else
	{
		_device->map_texture_region(intermediate, 0, nullptr, api::map_access::read_only, &mapped_data);
	}

	if (mapped_data.data != nullptr)
	{
		auto mapped_pixels = static_cast<const uint8_t *>(mapped_data.data);

		for (uint32_t y = 0; y < desc.texture.height; ++y, pixels += pixels_row_pitch, mapped_pixels += mapped_data.row_pitch)
		{
			switch (view_format)
			{
			case api::format::r8_unorm:
				for (uint32_t x = 0; x < desc.texture.width; ++x)
				{
					pixels[x * 4 + 0] = mapped_pixels[x];
					pixels[x * 4 + 1] = 0;
					pixels[x * 4 + 2] = 0;
					pixels[x * 4 + 3] = 0xFF;
				}
				break;
			case api::format::r8g8_unorm:
				for (uint32_t x = 0; x < desc.texture.width; ++x)
				{
					pixels[x * 4 + 0] = mapped_pixels[x * 2 + 0];
					pixels[x * 4 + 1] = mapped_pixels[x * 2 + 1];
					pixels[x * 4 + 2] = 0;
					pixels[x * 4 + 3] = 0xFF;
				}
				break;
			case api::format::r8g8b8a8_unorm:
			case api::format::r8g8b8x8_unorm:
				std::memcpy(pixels, mapped_pixels, pixels_row_pitch);
				if (view_format == api::format::r8g8b8x8_unorm)
					for (uint32_t x = 0; x < pixels_row_pitch; x += 4)
						pixels[x + 3] = 0xFF;
				break;
			case api::format::b8g8r8a8_unorm:
			case api::format::b8g8r8x8_unorm:
				std::memcpy(pixels, mapped_pixels, pixels_row_pitch);
				// Format is BGRA, but output should be RGBA, so flip channels
				for (uint32_t x = 0; x < pixels_row_pitch; x += 4)
					std::swap(pixels[x + 0], pixels[x + 2]);
				if (view_format == api::format::b8g8r8x8_unorm)
					for (uint32_t x = 0; x < pixels_row_pitch; x += 4)
						pixels[x + 3] = 0xFF;
				break;
			case api::format::r10g10b10a2_unorm:
			case api::format::b10g10r10a2_unorm:
				for (uint32_t x = 0; x < pixels_row_pitch; x += 4)
				{
					const uint32_t rgba = *reinterpret_cast<const uint32_t *>(mapped_pixels + x);
					// Divide by 4 to get 10-bit range (0-1023) into 8-bit range (0-255)
					pixels[x + 0] = (( rgba & 0x000003FF)        /  4) & 0xFF;
					pixels[x + 1] = (((rgba & 0x000FFC00) >> 10) /  4) & 0xFF;
					pixels[x + 2] = (((rgba & 0x3FF00000) >> 20) /  4) & 0xFF;
					pixels[x + 3] = (((rgba & 0xC0000000) >> 30) * 85) & 0xFF;
					if (view_format == api::format::b10g10r10a2_unorm)
						std::swap(pixels[x + 0], pixels[x + 2]);
				}
				break;
			}
		}

		if (_device->check_capability(api::device_caps::copy_buffer_to_texture))
			_device->unmap_buffer_region(intermediate);
		else
			_device->unmap_texture_region(intermediate, 0);
	}

	_device->destroy_resource(intermediate);

	return mapped_data.data != nullptr;
}

#if RESHADE_FX
void reshade::runtime::reset_uniform_value(uniform &variable)
{
	if (!variable.has_initializer_value)
	{
		std::memset(_effects[variable.effect_index].uniform_data_storage.data() + variable.offset, 0, variable.size);
		return;
	}

	// Need to use typed setters, to ensure values are properly forced to floating point in D3D9
	for (size_t i = 0, array_length = (variable.type.is_array() ? variable.type.array_length : 1);
		 i < array_length; ++i)
	{
		const reshadefx::constant &value = variable.type.is_array() ? variable.initializer_value.array_data[i] : variable.initializer_value;

		switch (variable.type.base)
		{
		case reshadefx::type::t_int:
			set_uniform_value(variable, value.as_int, variable.type.components(), i);
			break;
		case reshadefx::type::t_bool:
		case reshadefx::type::t_uint:
			set_uniform_value(variable, value.as_uint, variable.type.components(), i);
			break;
		case reshadefx::type::t_float:
			set_uniform_value(variable, value.as_float, variable.type.components(), i);
			break;
		}
	}
}
#endif

bool reshade::runtime::is_key_down(uint32_t keycode) const
{
	return _input != nullptr && _input->is_key_down(keycode);
}
bool reshade::runtime::is_key_pressed(uint32_t keycode) const
{
	return _input != nullptr && _input->is_key_pressed(keycode);
}
bool reshade::runtime::is_key_released(uint32_t keycode) const
{
	return _input != nullptr && _input->is_key_released(keycode);
}
bool reshade::runtime::is_mouse_button_down(uint32_t button) const
{
	return _input != nullptr && _input->is_mouse_button_down(button);
}
bool reshade::runtime::is_mouse_button_pressed(uint32_t button) const
{
	return _input != nullptr && _input->is_mouse_button_pressed(button);
}
bool reshade::runtime::is_mouse_button_released(uint32_t button) const
{
	return _input != nullptr && _input->is_mouse_button_released(button);
}

void reshade::runtime::get_cursor_position(uint32_t *out_x, uint32_t *out_y, int16_t *out_wheel_delta) const
{
	if (out_x != nullptr)
		*out_x = (_input != nullptr) ? _input->mouse_position_x() : 0;
	if (out_y != nullptr)
		*out_y = (_input != nullptr) ? _input->mouse_position_y() : 0;
	if (out_wheel_delta != nullptr)
		*out_wheel_delta = (_input != nullptr) ? _input->mouse_wheel_delta() : 0;
}

void reshade::runtime::enumerate_uniform_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, api::effect_uniform_variable variable, void *user_data), void *user_data)
{
#if RESHADE_FX
	if (is_loading())
		return;

	for (const effect &effect : _effects)
	{
		if (effect_name != nullptr && effect.source_file.filename() != effect_name)
			continue;

		for (const uniform &variable : effect.uniforms)
			callback(this, { reinterpret_cast<uintptr_t>(&variable) }, user_data);

		if (effect_name != nullptr)
			break;
	}
#endif
}

reshade::api::effect_uniform_variable reshade::runtime::find_uniform_variable(const char *effect_name, const char *variable_name) const
{
#if RESHADE_FX
	if (is_loading())
		return { 0 };

	for (const effect &effect : _effects)
	{
		if (effect_name != nullptr && effect.source_file.filename() != effect_name)
			continue;

		for (const uniform &variable : effect.uniforms)
		{
			if (variable.name == variable_name)
				return { reinterpret_cast<uintptr_t>(&variable) };
		}

		if (effect_name != nullptr)
			break;
	}
#endif
	return { 0 };
}

void reshade::runtime::get_uniform_variable_name(api::effect_uniform_variable handle, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable != nullptr)
	{
		if (value != nullptr && *length != 0)
			value[variable->name.copy(value, *length - 1)] = '\0';

		*length = variable->name.size();
}
	else
	{
		*length = 0;
	}
#endif
}

void reshade::runtime::get_uniform_annotation_value(api::effect_uniform_variable handle, const char *name, bool *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_uint(name, array_index + i) != 0;
#endif
}
void reshade::runtime::get_uniform_annotation_value(api::effect_uniform_variable handle, const char *name, float *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_float(name, array_index + i);
#endif
}
void reshade::runtime::get_uniform_annotation_value(api::effect_uniform_variable handle, const char *name, int32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_int(name, array_index + i);
#endif
}
void reshade::runtime::get_uniform_annotation_value(api::effect_uniform_variable handle, const char *name, uint32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_uint(name, array_index + i);
#endif
}
void reshade::runtime::get_uniform_annotation_value(api::effect_uniform_variable handle, const char *name, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	std::string_view annotation;

	if (variable != nullptr && !(annotation = variable->annotation_as_string(name)).empty())
	{
		if (value != nullptr && *length != 0)
			value[annotation.copy(value, *length - 1)] = '\0';

		*length = annotation.size();
	}
	else
	{
		*length = 0;
	}
#endif
}

#if RESHADE_FX
static inline bool force_floating_point_value(const reshadefx::type &type, uint32_t renderer_id)
{
	if (renderer_id == 0x9000)
		return true; // All uniform variables are floating-point in D3D9
	if (type.is_matrix() && (renderer_id & 0x10000))
		return true; // All matrices are floating-point in GLSL
	return false;
}

void reshade::runtime::get_uniform_value(const uniform &variable, uint8_t *data, size_t size, size_t base_index) const
{
	size = std::min(size, static_cast<size_t>(variable.size));
	assert(data != nullptr && (size % 4) == 0);

	auto &data_storage = _effects[variable.effect_index].uniform_data_storage;
	assert(variable.offset + size <= data_storage.size());

	const size_t array_length = (variable.type.is_array() ? variable.type.array_length : 1);
	if (assert(base_index < array_length); base_index >= array_length)
		return;

	if (variable.type.is_matrix())
	{
		for (size_t a = base_index, i = 0; a < array_length; ++a)
			// Each row of a matrix is 16-byte aligned, so needs special handling
			for (size_t row = 0; row < variable.type.rows; ++row)
				for (size_t col = 0; i < (size / 4) && col < variable.type.cols; ++col, ++i)
					std::memcpy(
						data + ((a - base_index) * variable.type.components() + (row * variable.type.cols + col)) * 4,
						data_storage.data() + variable.offset + (a * (variable.type.rows * 4) + (row * 4 + col)) * 4, 4);
	}
	else if (array_length > 1)
	{
		for (size_t a = base_index, i = 0; a < array_length; ++a)
			// Each element in the array is 16-byte aligned, so needs special handling
			for (size_t row = 0; i < (size / 4) && row < variable.type.rows; ++row, ++i)
				std::memcpy(
					data + ((a - base_index) * variable.type.components() + row) * 4,
					data_storage.data() + variable.offset + (a * 4 + row) * 4, 4);
	}
	else
	{
		std::memcpy(data, data_storage.data() + variable.offset, size);
	}
}
#endif

void reshade::runtime::get_uniform_value(api::effect_uniform_variable handle, bool *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	count = std::min(count, static_cast<size_t>(variable->size / 4));
	assert(values != nullptr);

	const auto data = static_cast<uint8_t *>(alloca(variable->size));
	get_uniform_value(*variable, data, variable->size, array_index);

	for (size_t i = 0; i < count; i++)
		values[i] = reinterpret_cast<const uint32_t *>(data)[i] != 0;
#endif
}
void reshade::runtime::get_uniform_value(api::effect_uniform_variable handle, float *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	if (variable->type.is_floating_point() || force_floating_point_value(variable->type, _renderer_id))
	{
		get_uniform_value(*variable, reinterpret_cast<uint8_t *>(values), count * sizeof(float), array_index);
		return;
	}

	count = std::min(count, static_cast<size_t>(variable->size / 4));
	assert(values != nullptr);

	const auto data = static_cast<uint8_t *>(alloca(variable->size));
	get_uniform_value(*variable, data, variable->size, array_index);

	for (size_t i = 0; i < count; ++i)
		if (variable->type.is_signed())
			values[i] = static_cast<float>(reinterpret_cast<const int32_t *>(data)[i]);
		else
			values[i] = static_cast<float>(reinterpret_cast<const uint32_t *>(data)[i]);
#endif
}
void reshade::runtime::get_uniform_value(api::effect_uniform_variable handle, int32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	if (variable->type.is_integral() && !force_floating_point_value(variable->type, _renderer_id))
	{
		get_uniform_value(*variable, reinterpret_cast<uint8_t *>(values), count * sizeof(int32_t), array_index);
		return;
	}

	count = std::min(count, static_cast<size_t>(variable->size / 4));
	assert(values != nullptr);

	const auto data = static_cast<uint8_t *>(alloca(variable->size));
	get_uniform_value(*variable, data, variable->size, array_index);

	for (size_t i = 0; i < count; i++)
		values[i] = static_cast<int32_t>(reinterpret_cast<const float *>(data)[i]);
#endif
}
void reshade::runtime::get_uniform_value(api::effect_uniform_variable handle, uint32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	get_uniform_value(handle, reinterpret_cast<int32_t *>(values), count, array_index);
#endif
}

#if RESHADE_FX
void reshade::runtime::set_uniform_value(uniform &variable, const uint8_t *data, size_t size, size_t base_index)
{
	size = std::min(size, static_cast<size_t>(variable.size));
	assert(data != nullptr && (size % 4) == 0);

	auto &data_storage = _effects[variable.effect_index].uniform_data_storage;
	assert(variable.offset + size <= data_storage.size());

	const size_t array_length = (variable.type.is_array() ? variable.type.array_length : 1);
	if (assert(base_index < array_length); base_index >= array_length)
		return;

	if (variable.type.is_matrix())
	{
		for (size_t a = base_index, i = 0; a < array_length; ++a)
			// Each row of a matrix is 16-byte aligned, so needs special handling
			for (size_t row = 0; row < variable.type.rows; ++row)
				for (size_t col = 0; i < (size / 4) && col < variable.type.cols; ++col, ++i)
					std::memcpy(
						data_storage.data() + variable.offset + (a * variable.type.rows * 4 + (row * 4 + col)) * 4,
						data + ((a - base_index) * variable.type.components() + (row * variable.type.cols + col)) * 4, 4);
	}
	else if (array_length > 1)
	{
		for (size_t a = base_index, i = 0; a < array_length; ++a)
			// Each element in the array is 16-byte aligned, so needs special handling
			for (size_t row = 0; i < (size / 4) && row < variable.type.rows; ++row, ++i)
				std::memcpy(
					data_storage.data() + variable.offset + (a * 4 + row) * 4,
					data + ((a - base_index) * variable.type.components() + row) * 4, 4);
	}
	else
	{
		std::memcpy(data_storage.data() + variable.offset, data, size);
	}
}
#endif

void reshade::runtime::set_uniform_value(api::effect_uniform_variable handle, const bool *values, size_t count, size_t array_index)
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	if (variable->type.is_floating_point() || force_floating_point_value(variable->type, _renderer_id))
	{
		const auto data = static_cast<float *>(alloca(count * sizeof(float)));
		for (size_t i = 0; i < count; ++i)
			data[i] = values[i] ? 1.0f : 0.0f;

		set_uniform_value(*variable, reinterpret_cast<const uint8_t *>(data), count * sizeof(float), array_index);
	}
	else
	{
		const auto data = static_cast<uint32_t *>(alloca(count * sizeof(uint32_t)));
		for (size_t i = 0; i < count; ++i)
			data[i] = values[i] ? 1 : 0;

		set_uniform_value(*variable, reinterpret_cast<const uint8_t *>(data), count * sizeof(uint32_t), array_index);
	}
#endif
}
void reshade::runtime::set_uniform_value(api::effect_uniform_variable handle, const float *values, size_t count, size_t array_index)
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	if (variable->type.is_floating_point() || force_floating_point_value(variable->type, _renderer_id))
	{
		set_uniform_value(*variable, reinterpret_cast<const uint8_t *>(values), count * sizeof(float), array_index);
	}
	else
	{
		const auto data = static_cast<int32_t *>(alloca(count * sizeof(int32_t)));
		for (size_t i = 0; i < count; ++i)
			data[i] = static_cast<int32_t>(values[i]);

		set_uniform_value(*variable, reinterpret_cast<const uint8_t *>(data), count * sizeof(int32_t), array_index);
	}
#endif
}
void reshade::runtime::set_uniform_value(api::effect_uniform_variable handle, const int32_t *values, size_t count, size_t array_index)
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	if (variable->type.is_floating_point() || force_floating_point_value(variable->type, _renderer_id))
	{
		const auto data = static_cast<float *>(alloca(count * sizeof(float)));
		for (size_t i = 0; i < count; ++i)
			data[i] = static_cast<float>(values[i]);

		set_uniform_value(*variable, reinterpret_cast<const uint8_t *>(data), count * sizeof(float), array_index);
	}
	else
	{
		set_uniform_value(*variable, reinterpret_cast<const uint8_t *>(values), count * sizeof(int32_t), array_index);
	}
#endif
}
void reshade::runtime::set_uniform_value(api::effect_uniform_variable handle, const uint32_t *values, size_t count, size_t array_index)
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<uniform *>(handle.handle);
	if (variable == nullptr)
		return;

	if (variable->type.is_floating_point() || force_floating_point_value(variable->type, _renderer_id))
	{
		const auto data = static_cast<float *>(alloca(count * sizeof(float)));
		for (size_t i = 0; i < count; ++i)
			data[i] = static_cast<float>(values[i]);

		set_uniform_value(*variable, reinterpret_cast<const uint8_t *>(data), count * sizeof(float), array_index);
	}
	else
	{
		set_uniform_value(*variable, reinterpret_cast<const uint8_t *>(values), count * sizeof(uint32_t), array_index);
	}
#endif
}

void reshade::runtime::enumerate_texture_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, api::effect_texture_variable variable, void *user_data), void *user_data)
{
#if RESHADE_FX
	if (is_loading() || !_reload_create_queue.empty())
		return;

	for (const texture &variable : _textures)
	{
		if (effect_name != nullptr &&
			std::find_if(variable.shared.begin(), variable.shared.end(),
				[this, effect_name](size_t effect_index) { return _effects[effect_index].source_file.filename() == effect_name; }) == variable.shared.end())
			continue;

		callback(this, { reinterpret_cast<uintptr_t>(&variable) }, user_data);
	}
#endif
}

reshade::api::effect_texture_variable reshade::runtime::find_texture_variable(const char *effect_name, const char *variable_name) const
{
#if RESHADE_FX
	if (is_loading() || !_reload_create_queue.empty())
		return { 0 };

	for (const texture &variable : _textures)
	{
		if (effect_name != nullptr &&
			std::find_if(variable.shared.begin(), variable.shared.end(),
				[this, effect_name](size_t effect_index) { return _effects[effect_index].source_file.filename() == effect_name; }) == variable.shared.end())
			continue;

		if (variable.name == variable_name || variable.unique_name == variable_name)
			return { reinterpret_cast<uintptr_t>(&variable) };
	}
#endif
	return { 0 };
}

void reshade::runtime::get_texture_variable_name(api::effect_texture_variable handle, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable != nullptr)
	{
		if (value != nullptr && *length != 0)
			value[variable->name.copy(value, *length - 1)] = '\0';

		*length = variable->name.size();
}
	else
	{
		*length = 0;
	}
#endif
}

void reshade::runtime::get_texture_annotation_value(api::effect_texture_variable handle, const char *name, bool *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_uint(name, array_index + i) != 0;
#endif
}
void reshade::runtime::get_texture_annotation_value(api::effect_texture_variable handle, const char *name, float *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_float(name, array_index + i);
#endif
}
void reshade::runtime::get_texture_annotation_value(api::effect_texture_variable handle, const char *name, int32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_int(name, array_index + i);
#endif
}
void reshade::runtime::get_texture_annotation_value(api::effect_texture_variable handle, const char *name, uint32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	if (variable == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = variable->annotation_as_uint(name, array_index + i);
#endif
}
void reshade::runtime::get_texture_annotation_value(api::effect_texture_variable handle, const char *name, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto variable = reinterpret_cast<const texture *>(handle.handle);
	std::string_view annotation;

	if (variable != nullptr && !(annotation = variable->annotation_as_string(name)).empty())
	{
		if (value != nullptr && *length != 0)
			value[annotation.copy(value, *length - 1)] = '\0';

		*length = annotation.size();
	}
	else
	{
		*length = 0;
	}
#endif
}

void reshade::runtime::update_texture(api::effect_texture_variable handle, const uint32_t width, const uint32_t height, const uint8_t *pixels)
{
#if RESHADE_FX
	if (handle == 0)
		return;
	const auto &variable = *reinterpret_cast<texture *>(handle.handle);
	if (variable.resource == 0)
		return;

	std::vector<uint8_t> resized(variable.width * variable.height * 4);
	// Need to potentially resize image data to the texture dimensions
	if (variable.width != width || variable.height != height)
	{
		LOG(INFO) << "Resizing image data for texture '" << variable.unique_name << "' from " << width << "x" << height << " to " << variable.width << "x" << variable.height << " ...";

		stbir_resize_uint8(pixels, width, height, 0, resized.data(), variable.width, variable.height, 0, 4);
	}
	else
	{
		std::memcpy(resized.data(), pixels, resized.size());
	}

	// Collapse data to the correct number of components per pixel based on the texture format
	uint32_t row_pitch = variable.width;
	switch (variable.format)
	{
	case reshadefx::texture_format::r8:
		for (size_t i = 4, k = 1; i < resized.size(); i += 4, k += 1)
			resized[k] = resized[i];
		break;
	case reshadefx::texture_format::rg8:
		for (size_t i = 4, k = 2; i < resized.size(); i += 4, k += 2)
			resized[k + 0] = resized[i + 0],
			resized[k + 1] = resized[i + 1];
		row_pitch *= 2;
		break;
	case reshadefx::texture_format::rgba8:
		row_pitch *= 4;
		break;
	default:
		LOG(ERROR) << "Texture upload is not supported for format " << static_cast<int>(variable.format) << " of texture '" << variable.unique_name << "'!";
		return;
	}

	api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();
	cmd_list->barrier(variable.resource, api::resource_usage::shader_resource, api::resource_usage::copy_dest);
	_device->update_texture_region({ resized.data(), row_pitch, row_pitch * variable.height }, variable.resource, 0);
	cmd_list->barrier(variable.resource, api::resource_usage::copy_dest, api::resource_usage::shader_resource);

	if (variable.levels > 1)
		cmd_list->generate_mipmaps(variable.srv[0]);
#endif
}

void reshade::runtime::get_texture_binding(api::effect_texture_variable variable, api::resource_view *out_srv, api::resource_view *out_srv_srgb) const
{
#if RESHADE_FX
	if (variable == 0)
		return;

	if (out_srv != nullptr)
		*out_srv = reinterpret_cast<const texture *>(variable.handle)->srv[0];
	if (out_srv_srgb != nullptr)
		*out_srv_srgb = reinterpret_cast<const texture *>(variable.handle)->srv[1];
#endif
}

void reshade::runtime::update_texture_bindings(const char *semantic, api::resource_view srv, api::resource_view srv_srgb)
{
#if RESHADE_FX
	if (srv_srgb == 0)
		srv_srgb = srv;

	if (srv != 0)
	{
		_texture_semantic_bindings[semantic] = { srv, srv_srgb };
	}
	else
	{
		_texture_semantic_bindings.erase(semantic);

		// Overwrite with empty texture, since it is not valid to bind a zero handle
		srv = srv_srgb = _empty_srv;
	}

	// Make sure all previous frames have finished before freeing the image view and updating descriptors (since they may be in use otherwise)
	_graphics_queue->wait_idle();

	// Update texture bindings
	size_t num_bindings = 0;
	for (effect &effect_data : _effects)
		num_bindings += effect_data.texture_semantic_to_binding.size();

	std::vector<api::descriptor_set_update> descriptor_writes;
	std::vector<api::sampler_with_resource_view> sampler_descriptors(num_bindings);

	for (effect &effect_data : _effects)
	{
		for (const auto &binding : effect_data.texture_semantic_to_binding)
		{
			if (binding.semantic != semantic)
				continue;

			assert(num_bindings != 0);

			api::descriptor_set_update &write = descriptor_writes.emplace_back();
			write.set = binding.set;
			write.binding = binding.index;
			write.count = 1;

			if (binding.sampler != 0)
			{
				write.type = api::descriptor_type::sampler_with_resource_view;
				write.descriptors = &sampler_descriptors[--num_bindings];
			}
			else
			{
				write.type = api::descriptor_type::shader_resource_view;
				write.descriptors = &sampler_descriptors[--num_bindings].view;
			}

			sampler_descriptors[num_bindings].sampler = binding.sampler;
			sampler_descriptors[num_bindings].view = binding.srgb ? srv_srgb : srv;
		}
	}

	_device->update_descriptor_sets(static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data());
#endif
}

void reshade::runtime::enumerate_techniques(const char *effect_name, void(*callback)(effect_runtime *runtime, api::effect_technique technique, void *user_data), void *user_data)
{
#if RESHADE_FX
	if (is_loading())
		return;

	for (const technique &technique : _techniques)
	{
		if (effect_name != nullptr && _effects[technique.effect_index].source_file.filename() != effect_name)
			continue;

		callback(this, { reinterpret_cast<uintptr_t>(&technique) }, user_data);
	}
#endif
}

reshade::api::effect_technique reshade::runtime::find_technique(const char *effect_name, const char *technique_name)
{
#if RESHADE_FX
	if (is_loading())
		return { 0 };

	for (const technique &technique : _techniques)
	{
		if (effect_name != nullptr && _effects[technique.effect_index].source_file.filename() != effect_name)
			continue;

		if (technique.name == technique_name)
			return { reinterpret_cast<uintptr_t>(&technique) };
	}
#endif
	return { 0 };
}

void reshade::runtime::get_technique_name(api::effect_technique handle, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech != nullptr)
	{
		if (value != nullptr && *length != 0)
			value[tech->name.copy(value, *length - 1)] = '\0';

		*length = tech->name.size();
	}
	else
	{
		*length = 0;
	}
#endif
}

void reshade::runtime::get_technique_annotation_value(api::effect_technique handle, const char *name, bool *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = tech->annotation_as_uint(name, array_index + i) != 0;
#endif
}
void reshade::runtime::get_technique_annotation_value(api::effect_technique handle, const char *name, float *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = tech->annotation_as_float(name, array_index + i);
#endif
}
void reshade::runtime::get_technique_annotation_value(api::effect_technique handle, const char *name, int32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = tech->annotation_as_int(name, array_index + i);
#endif
}
void reshade::runtime::get_technique_annotation_value(api::effect_technique handle, const char *name, uint32_t *values, size_t count, size_t array_index) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech == nullptr)
		return;

	for (size_t i = 0; i < count; ++i)
		values[i] = tech->annotation_as_uint(name, array_index + i);
#endif
}
void reshade::runtime::get_technique_annotation_value(api::effect_technique handle, const char *name, char *value, size_t *length) const
{
#if RESHADE_FX
	if (length == nullptr)
		return;

	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	std::string_view annotation;

	if (tech != nullptr && !(annotation = tech->annotation_as_string(name)).empty())
	{
		if (value != nullptr && *length != 0)
			value[annotation.copy(value, *length - 1)] = '\0';

		*length = annotation.size();
	}
	else
	{
		*length = 0;
	}
#endif
}

bool reshade::runtime::get_technique_enabled(api::effect_technique handle) const
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<const technique *>(handle.handle);
	if (tech == nullptr)
		return false;

	return tech->enabled;
#else
	return false;
#endif
}
void reshade::runtime::set_technique_enabled(api::effect_technique handle, bool enabled)
{
#if RESHADE_FX
	const auto tech = reinterpret_cast<technique *>(handle.handle);
	if (tech == nullptr)
		return;

	if (enabled)
		enable_technique(*tech);
	else
		disable_technique(*tech);
#endif
}

void reshade::runtime::set_preprocessor_definition(const char *name, const char *value)
{
#if RESHADE_FX
	if (name == nullptr)
		return;

	auto preset_it = _preset_preprocessor_definitions.begin();
	for (; preset_it != _preset_preprocessor_definitions.end(); ++preset_it)
	{
		char current_name[128] = "";
		const size_t equals_index = preset_it->find('=');
		preset_it->copy(current_name, std::min(equals_index, sizeof(current_name) - 1));

		if (strcmp(name, current_name) == 0 && equals_index != std::string::npos)
			break;
	}

	if (value == nullptr || value[0] == '\0') // An empty value removes the definition
	{
		if (preset_it != _preset_preprocessor_definitions.end())
		{
			_preset_preprocessor_definitions.erase(preset_it);
		}
	}
	else
	{
		if (preset_it != _preset_preprocessor_definitions.end())
		{
			*preset_it = std::string(name) + '=' + value;
		}
		else
		{
			_preset_preprocessor_definitions.push_back(std::string(name) + '=' + value);
		}
	}

	reload_effects();
#endif
}
bool reshade::runtime::get_preprocessor_definition(const char *name, char *value, size_t *length) const
{
#if RESHADE_FX
	if (name == nullptr || length == nullptr)
		return false;

	for (auto it = _preset_preprocessor_definitions.begin(); it != _preset_preprocessor_definitions.end(); ++it)
	{
		char current_name[128] = "";
		const size_t equals_index = it->find('=');
		it->copy(current_name, std::min(equals_index, sizeof(current_name) - 1));

		if (strcmp(name, current_name) == 0 && equals_index != std::string::npos)
		{
			if (value != nullptr && *length != 0)
				value[it->copy(value, *length - 1, equals_index + 1)] = '\0';

			*length = it->size() - (equals_index + 1);
			return true;
		}
	}
	for (auto it = _global_preprocessor_definitions.begin(); it != _global_preprocessor_definitions.end(); ++it)
	{
		char current_name[128] = "";
		const size_t equals_index = it->find('=');
		it->copy(current_name, std::min(equals_index, sizeof(current_name) - 1));

		if (strcmp(name, current_name) == 0 && equals_index != std::string::npos)
		{
			if (value != nullptr && *length != 0)
				value[it->copy(value, *length - 1, equals_index + 1)] = '\0';

			*length = it->size() - (equals_index + 1);
			return true;
		}
	}
#endif
	return false;
}
