/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>
#include <filesystem>

#if RESHADE_GUI
#include "imgui_editor.hpp"

struct ImDrawData;
struct ImGuiContext;
#endif

namespace reshade
{
	class ini_file; // Forward declarations to avoid excessive #include
	struct effect;
	struct uniform;
	struct texture;
	struct technique;

	/// <summary>
	/// Platform independent base class for the main ReShade runtime.
	/// This class needs to be implemented for all supported rendering APIs.
	/// </summary>
	class runtime
	{
	public:
		/// <summary>
		/// Return whether the runtime is initialized.
		/// </summary>
		bool is_initialized() const { return _is_initialized; }

		/// <summary>
		/// Return the frame width in pixels.
		/// </summary>
		unsigned int frame_width() const { return _width; }
		/// <summary>
		/// Return the frame height in pixels.
		/// </summary>
		unsigned int frame_height() const { return _height; }

		/// <summary>
		/// Create a copy of the current frame image in system memory.
		/// </summary>
		/// <param name="buffer">The 32bpp RGBA buffer to save the screenshot to.</param>
		virtual bool capture_screenshot(uint8_t *buffer) const = 0;

		/// <summary>
		/// Save user configuration to disk.
		/// </summary>
		void save_config() const;

		/// <summary>
		/// Create a new texture with the specified dimensions.
		/// </summary>
		/// <param name="texture">The texture description.</param>
		virtual bool init_texture(texture &texture) = 0;
		/// <summary>
		/// Upload the image data of a texture.
		/// </summary>
		/// <param name="texture">The texture to update.</param>
		/// <param name="pixels">The 32bpp RGBA image data to update the texture with.</param>
		virtual void upload_texture(const texture &texture, const uint8_t *pixels) = 0;
		/// <summary>
		/// Destroy an existing texture.
		/// </summary>
		/// <param name="texture">The texture to destroy.</param>
		virtual void destroy_texture(texture &texture) = 0;

		/// <summary>
		/// Get the value of a uniform variable.
		/// </summary>
		/// <param name="variable">The variable to retrieve the value from.</param>
		/// <param name="data">The buffer to store the value data in.</param>
		/// <param name="size">The size of the <paramref name="data"/> buffer.</param>
		/// <param name="base_index">Array index to start reading from.</param>
		void get_uniform_value(const uniform &variable, uint8_t *data, size_t size, size_t base_index) const;
		void get_uniform_value(const uniform &variable, bool *values, size_t count, size_t array_index = 0) const;
		void get_uniform_value(const uniform &variable, int32_t *values, size_t count, size_t array_index = 0) const;
		void get_uniform_value(const uniform &variable, uint32_t *values, size_t count, size_t array_index = 0) const;
		void get_uniform_value(const uniform &variable, float *values, size_t count, size_t array_index = 0) const;
		/// <summary>
		/// Update the value of a uniform variable.
		/// </summary>
		/// <param name="variable">The variable to update.</param>
		/// <param name="data">The value data to update the variable to.</param>
		/// <param name="size">The size of the <paramref name="data"/> buffer.</param>
		/// <param name="base_index">Array index to start writing to.</param>
		void set_uniform_value(uniform &variable, const uint8_t *data, size_t size, size_t base_index);
		void set_uniform_value(uniform &variable, const bool *values, size_t count, size_t array_index = 0);
		void set_uniform_value(uniform &variable, const int32_t *values, size_t count, size_t array_index = 0);
		void set_uniform_value(uniform &variable, const uint32_t *values, size_t count, size_t array_index = 0);
		void set_uniform_value(uniform &variable, const float *values, size_t count, size_t array_index = 0);
		template <typename T>
		std::enable_if_t<std::is_same_v<T, bool> || std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float>>
		set_uniform_value(uniform &variable, T x, T y = T(0), T z = T(0), T w = T(0))
		{
			const T data[4] = { x, y, z, w };
			set_uniform_value(variable, data, 4);
		}

		/// <summary>
		/// Reset a uniform variable to its initial value.
		/// </summary>
		/// <param name="variable">The variable to update.</param>
		void reset_uniform_value(uniform &variable);

#if RESHADE_GUI
		/// <summary>
		/// Register a function to be called when the UI is drawn.
		/// </summary>
		/// <param name="label">Name of the widget.</param>
		/// <param name="function">The callback function.</param>
		void subscribe_to_ui(std::string label, std::function<void()> function) { _menu_callables.emplace_back(label, function); }
#endif
		/// <summary>
		/// Register a function to be called when user configuration is loaded.
		/// </summary>
		/// <param name="function">The callback function.</param>
		void subscribe_to_load_config(std::function<void(const ini_file &)> function);
		/// <summary>
		/// Register a function to be called when user configuration is stored.
		/// </summary>
		/// <param name="function">The callback function.</param>
		void subscribe_to_save_config(std::function<void(ini_file &)> function);

	protected:
		runtime();
		virtual ~runtime();

		/// <summary>
		/// Callback function called when the runtime is initialized.
		/// </summary>
		/// <returns>Returns if the initialization succeeded.</returns>
		bool on_init(void *window);
		/// <summary>
		/// Callback function called when the runtime is uninitialized.
		/// </summary>
		void on_reset();
		/// <summary>
		/// Callback function called every frame.
		/// </summary>
		void on_present();

		/// <summary>
		/// Compile effect from the specified source file and initialize textures, uniforms and techniques.
		/// </summary>
		/// <param name="path">The path to an effect source code file.</param>
		/// <param name="index">The ID of the effect.</param>
		bool load_effect(const std::filesystem::path &path, size_t index);
		/// <summary>
		/// Load all effects found in the effect search paths.
		/// </summary>
		void load_effects();
		/// <summary>
		/// Initialize resources for the effect and load the effect module.
		/// </summary>
		/// <param name="index">The ID of the effect.</param>
		virtual bool init_effect(size_t index) = 0;
		/// <summary>
		/// Unload the specified effect.
		/// </summary>
		/// <param name="index">The ID of the effect.</param>
		virtual void unload_effect(size_t index);
		/// <summary>
		/// Unload all effects currently loaded.
		/// </summary>
		virtual void unload_effects();

		/// <summary>
		/// Load image files and update textures with image data.
		/// </summary>
		void load_textures();

		/// <summary>
		/// Apply post-processing effects to the frame.
		/// </summary>
		void update_and_render_effects();
		/// <summary>
		/// Render all passes in a technique.
		/// </summary>
		/// <param name="technique">The technique to render.</param>
		virtual void render_technique(technique &technique) = 0;
#if RESHADE_GUI
		/// <summary>
		/// Render command lists obtained from ImGui.
		/// </summary>
		/// <param name="data">The draw data to render.</param>
		virtual void render_imgui_draw_data(ImDrawData *draw_data) = 0;
#endif

		/// <summary>
		/// Returns the texture object corresponding to the passed <paramref name="unique_name"/>.
		/// </summary>
		/// <param name="unique_name">The name of the texture to find.</param>
		texture &look_up_texture_by_name(const std::string &unique_name);

		bool _is_initialized = false;
		bool _performance_mode = false;
		bool _has_high_network_activity = false;
		bool _has_depth_texture = false;
		unsigned int _width = 0;
		unsigned int _height = 0;
		unsigned int _window_width = 0;
		unsigned int _window_height = 0;
		unsigned int _vendor_id = 0;
		unsigned int _device_id = 0;
		unsigned int _renderer_id = 0;
		unsigned int _color_bit_depth = 8;

		uint64_t _framecount = 0;
		unsigned int _vertices = 0;
		unsigned int _drawcalls = 0;

		std::vector<effect> _effects;
		std::vector<texture> _textures;
		std::vector<technique> _techniques;

	private:
		/// <summary>
		/// Compare current version against the latest published one.
		/// </summary>
		/// <param name="latest_version">Contains the latest version after this function returned.</param>
		/// <returns><c>true</c> if an update is available, <c>false</c> otherwise</returns>
		static bool check_for_update(unsigned long latest_version[3]);

		/// <summary>
		/// Checks whether runtime is currently loading effects.
		/// </summary>
		bool is_loading() const { return _reload_remaining_effects != std::numeric_limits<size_t>::max(); }

		/// <summary>
		/// Enable a technique so it is rendered.
		/// </summary>
		/// <param name="technique"></param>
		void enable_technique(technique &technique);
		/// <summary>
		/// Disable a technique so that it is no longer rendered.
		/// </summary>
		/// <param name="technique"></param>
		void disable_technique(technique &technique);

		/// <summary>
		/// Load user configuration from disk.
		/// </summary>
		void load_config();

		/// <summary>
		/// Load the selected preset and apply it.
		/// </summary>
		void load_current_preset();
		/// <summary>
		/// Save the current value configuration to the currently selected preset.
		/// </summary>
		void save_current_preset() const;

		/// <summary>
		/// Find next preset is the directory and switch to it.
		/// </summary>
		/// <param name="filter_path">Directory base to search in and/or an optional filter to skip preset files.</param>
		/// <param name="reversed">Set to <c>true</c> to switch to previous instead of next preset.</param>
		/// <returns><c>true</c> if there was another preset to switch to, <c>false</c> if not and therefore no changes were made.</returns>
		bool switch_to_next_preset(std::filesystem::path filter_path, bool reversed = false);

		/// <summary>
		/// Create a copy of the current frame and write it to an image file on disk.
		/// </summary>
		void save_screenshot(const std::wstring &postfix = std::wstring(), bool should_save_preset = false);

		// === Status ===
		int _date[4] = {};
		bool _effects_enabled = true;
		bool _ignore_shortcuts = false;
		bool _force_shortcut_modifiers = true;
		unsigned int _effects_key_data[4];
		std::shared_ptr<class input> _input;
		std::chrono::high_resolution_clock::duration _last_frame_duration;
		std::chrono::high_resolution_clock::time_point _start_time;
		std::chrono::high_resolution_clock::time_point _last_present_time;

		// == Configuration ===
		bool _needs_update = false;
		unsigned long _latest_version[3] = {};
		std::filesystem::path _configuration_path;
		std::vector<std::function<void(ini_file &)>> _save_config_callables;
		std::vector<std::function<void(const ini_file &)>> _load_config_callables;

		// === Effect Loading ===
		bool _no_debug_info = 0;
		bool _no_reload_on_init = false;
		bool _last_shader_reload_successful = true;
		bool _last_texture_reload_successful = true;
		bool _textures_loaded = false;
		unsigned int _reload_key_data[4];
		size_t _reload_total_effects = 1;
		std::vector<size_t> _reload_compile_queue;
		std::atomic<size_t> _reload_remaining_effects = 0;
		std::mutex _reload_mutex;
		std::vector<std::thread> _worker_threads;
		std::vector<std::string> _global_preprocessor_definitions;
		std::vector<std::string> _preset_preprocessor_definitions;
		std::vector<std::filesystem::path> _effect_search_paths;
		std::vector<std::filesystem::path> _texture_search_paths;
		std::chrono::high_resolution_clock::time_point _last_reload_time;

		// === Screenshots ===
		bool _should_save_screenshot = false;
		bool _screenshot_save_ui = false;
		bool _screenshot_save_before = false;
		bool _screenshot_save_success = true;
		bool _screenshot_include_preset = false;
		bool _screenshot_clear_alpha = true;
		unsigned int _screenshot_format = 1;
		unsigned int _screenshot_naming = 0;
		unsigned int _screenshot_key_data[4];
		std::filesystem::path _screenshot_path;
		std::filesystem::path _last_screenshot_file;
		std::chrono::high_resolution_clock::time_point _last_screenshot_time;
		unsigned int _screenshot_jpeg_quality = 90;

		// === Preset Switching ===
		bool _preset_save_success = true;
		bool _is_in_between_presets_transition = false;
		unsigned int _prev_preset_key_data[4];
		unsigned int _next_preset_key_data[4];
		unsigned int _preset_transition_delay = 1000;
		std::filesystem::path _current_preset_path;
		std::chrono::high_resolution_clock::time_point _last_preset_switching_time;

#if RESHADE_GUI
		void init_ui();
		void deinit_ui();
		void build_font_atlas();

		void draw_ui();
		void draw_ui_home();
		void draw_ui_settings();
		void draw_ui_statistics();
		void draw_ui_log();
		void draw_ui_about();

		void draw_code_editor();
		void draw_code_viewer();
		void draw_preset_explorer();
		void draw_variable_editor();
		void draw_technique_editor();

		void open_file_in_code_editor(size_t effect_index, const std::filesystem::path &path);

		// === User Interface ===
		ImGuiContext *_imgui_context = nullptr;
		std::unique_ptr<texture> _imgui_font_atlas;
		std::vector<std::pair<std::string, std::function<void()>>> _menu_callables;
		bool _show_menu = false;
		bool _show_clock = false;
		bool _show_fps = false;
		bool _show_frametime = false;
		bool _show_splash = true;
		bool _show_code_editor = false;
		bool _show_code_viewer = false;
		bool _show_screenshot_message = true;
		bool _no_font_scaling = false;
		bool _rebuild_font_atlas = true;
		unsigned int _reload_count = 0;
		unsigned int _menu_key_data[4];
		int _fps_pos = 1;
		int _clock_format = 0;
		int _input_processing_mode = 2;

		// === User Interface - Home ===
		char _effect_filter[64] = {};
		bool _variable_editor_tabs = false;
		bool _duplicate_current_preset = false;
		bool _was_preprocessor_popup_edited = false;
		size_t _focused_effect = std::numeric_limits<size_t>::max();
		size_t _selected_effect = std::numeric_limits<size_t>::max();
		size_t _selected_technique = std::numeric_limits<size_t>::max();
		unsigned int _tutorial_index = 0;
		unsigned int _effects_expanded_state = 2;
		float _variable_editor_height = 0.0f;
		std::filesystem::path _current_browse_path;

		// === User Interface - Settings ===
		int _font_size = 13;
		int _editor_font_size = 13;
		int _style_index = 2;
		int _editor_style_index = 0;
		std::filesystem::path _font;
		std::filesystem::path _editor_font;
		std::filesystem::path _file_selection_path;
		float _fps_col[4] = { 1.0f, 1.0f, 0.784314f, 1.0f };
		float _fps_scale = 1.0f;

		// === User Interface - Statistics ===
		void *_preview_texture = nullptr;
		unsigned int _preview_size[3] = { 0, 0, 0xFFFFFFFF };

		// === User Interface - Log ===
		bool _log_wordwrap = false;
		uintmax_t _last_log_size;
		std::vector<std::string> _log_lines;

		// === User Interface - Code Editor ===
		imgui_code_editor _editor, _viewer;
		std::filesystem::path _editor_file;
		std::string _viewer_entry_point;
#endif
	};
}
