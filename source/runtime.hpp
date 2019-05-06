/**
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
#include "gui_code_editor.hpp"

struct ImDrawData;
struct ImGuiContext;
#endif

namespace reshade
{
	class ini_file; // Some forward declarations to keep number of includes small
	struct uniform;
	struct texture;
	struct technique;
	struct effect_data;

	/// <summary>
	/// Platform independent base class for the main ReShade runtime.
	/// This class needs to be implemented for all supported rendering APIs.
	/// </summary>
	class runtime abstract
	{
	public:
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
		virtual void capture_screenshot(uint8_t *buffer) const = 0;

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
		virtual void upload_texture(texture &texture, const uint8_t *pixels) = 0;

		/// <summary>
		/// Get the value of a uniform variable.
		/// </summary>
		/// <param name="variable">The variable to retrieve the value from.</param>
		/// <param name="values">The buffer to store the value data in.</param>
		/// <param name="count">The number of components the value.</param>
		void get_uniform_value(const uniform &variable, bool *values, size_t count) const;
		void get_uniform_value(const uniform &variable, int32_t *values, size_t count) const;
		void get_uniform_value(const uniform &variable, uint32_t *values, size_t count) const;
		void get_uniform_value(const uniform &variable, float *values, size_t count) const;
		/// <summary>
		/// Update the value of a uniform variable.
		/// </summary>
		/// <param name="variable">The variable to update.</param>
		/// <param name="values">The value data to update the variable to.</param>
		/// <param name="count">The number of components the value.</param>
		void set_uniform_value(uniform &variable, const bool *values, size_t count);
		void set_uniform_value(uniform &variable, bool x, bool y = false, bool z = false, bool w = false) { const bool data[4] = { x, y, z, w }; set_uniform_value(variable, data, 4); }
		void set_uniform_value(uniform &variable, const int32_t *values, size_t count);
		void set_uniform_value(uniform &variable, int32_t  x, int32_t y = 0, int32_t z = 0, int32_t w = 0) { const int32_t data[4] = { x, y, z, w }; set_uniform_value(variable, data, 4); }
		void set_uniform_value(uniform &variable, const uint32_t *values, size_t count);
		void set_uniform_value(uniform &variable, uint32_t x, uint32_t y = 0u, uint32_t z = 0u, uint32_t w = 0u) { const uint32_t data[4] = { x, y, z, w }; set_uniform_value(variable, data, 4); }
		void set_uniform_value(uniform &variable, const float *values, size_t count);
		void set_uniform_value(uniform &variable, float x, float y = 0.0f, float z = 0.0f, float w = 0.0f) { const float data[4] = { x, y, z, w }; set_uniform_value(variable, data, 4); }

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
		void subscribe_to_ui(std::string label, std::function<void()> function) { _menu_callables.push_back({ label, function }); }
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
		/// Load all effects found in the effect search paths.
		/// </summary>
		virtual void load_effects();
		/// <summary>
		/// Unload all effects currently loaded.
		/// </summary>
		virtual void unload_effects();
		/// <summary>
		/// Load image files and update textures with image data.
		/// </summary>
		void load_textures();

		/// <summary>
		/// Compile effect from the specified effect module.
		/// </summary>
		/// <param name="effect">The effect module to compile.</param>
		virtual bool compile_effect(effect_data &effect) = 0;

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

		bool _is_initialized = false;
		bool _has_high_network_activity = false;
		unsigned int _width = 0;
		unsigned int _height = 0;
		unsigned int _window_width = 0;
		unsigned int _window_height = 0;
		unsigned int _vendor_id = 0;
		unsigned int _device_id = 0;
		unsigned int _renderer_id = 0;
		uint64_t _framecount = 0;
		unsigned int _vertices = 0;
		unsigned int _drawcalls = 0;
		std::vector<texture> _textures;
		std::vector<uniform> _uniforms;
		std::vector<technique> _techniques;
		std::vector<unsigned char> _uniform_data_storage;

	private:
		/// <summary>
		/// Compare current version against the latest published one.
		/// </summary>
		/// <param name="latest_version">Contains the latest version after this function returned.</param>
		/// <returns><c>true</c> if an update is available, <c>false</c> otherwise</returns>
		static bool check_for_update(unsigned long latest_version[3]);

		/// <summary>
		/// Compile effect from the specified source file and initialize textures, uniforms and techniques.
		/// </summary>
		/// <param name="path">The path to an effect source code file.</param>
		/// <param name="out_id">The ID of the effect.</param>
		void load_effect(const std::filesystem::path &path, size_t &out_id);
		/// <summary>
		/// Unload the specified effect.
		/// </summary>
		/// <param name="id">The ID of the effect.</param>
		void unload_effect(size_t id);

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
		/// Save user configuration to the specified file on disk.
		/// </summary>
		/// <param name="path">Output configuration path.</param>
		void save_config(const std::filesystem::path &path) const;

		/// <summary>
		/// Load a preset from the specified file and apply it.
		/// </summary>
		/// <param name="path">The preset file to load.</param>
		void load_preset(const std::filesystem::path &path);
		/// <summary>
		/// Load the selected preset and apply it.
		/// </summary>
		void load_current_preset();
		/// <summary>
		/// Save the current value configuration as a preset to the specified file.
		/// </summary>
		/// <param name="path">The preset file to save to.</param>
		void save_preset(const std::filesystem::path &path) const;
		/// <summary>
		/// Save the current value configuration to the currently selected preset.
		/// </summary>
		void save_current_preset() const;

		/// <summary>
		/// Create a copy of the current frame and write it to an image file on disk.
		/// </summary>
		void save_screenshot();

		void get_uniform_value(const uniform &variable, uint8_t *data, size_t size) const;
		void set_uniform_value(uniform &variable, const uint8_t *data, size_t size);

		void set_current_preset();
		void set_current_preset(std::filesystem::path path);

		bool _needs_update = false;
		unsigned long _latest_version[3] = {};
		std::shared_ptr<class input> _input;

		bool _effects_enabled = true;
		bool _ignore_shortcuts = false;
		unsigned int _reload_key_data[4];
		unsigned int _effects_key_data[4];
		unsigned int _screenshot_key_data[4];
		int _screenshot_format = 1;
		std::filesystem::path _screenshot_path;
		std::filesystem::path _configuration_path;
		std::filesystem::path _last_screenshot_file;
		bool _screenshot_save_success = false;
		bool _screenshot_include_preset = false;

		std::filesystem::path _current_preset_path;

		std::vector<std::string> _global_preprocessor_definitions;
		std::vector<std::string> _preset_preprocessor_definitions;
		std::vector<std::filesystem::path> _effect_search_paths;
		std::vector<std::filesystem::path> _texture_search_paths;

		bool _textures_loaded = false;
		bool _performance_mode = false;
		bool _no_reload_on_init = false;
		bool _last_reload_successful = true;
		std::mutex _reload_mutex;
		size_t _reload_total_effects = 1;
		std::vector<size_t> _reload_compile_queue;
		std::atomic<size_t> _reload_remaining_effects = 0;
		std::vector<effect_data> _loaded_effects;
		std::vector<std::thread> _worker_threads;

		int _date[4] = {};
		std::chrono::high_resolution_clock::duration _last_frame_duration;
		std::chrono::high_resolution_clock::time_point _start_time;
		std::chrono::high_resolution_clock::time_point _last_reload_time;
		std::chrono::high_resolution_clock::time_point _last_present_time;
		std::chrono::high_resolution_clock::time_point _last_screenshot_time;

		std::vector<std::function<void(ini_file &)>> _save_config_callables;
		std::vector<std::function<void(const ini_file &)>> _load_config_callables;

#if RESHADE_GUI
		void init_ui();
		void deinit_ui();
		void build_font_atlas();
		void destroy_font_atlas();

		void draw_ui();

		void draw_overlay_menu_home();
		void draw_overlay_menu_settings();
		void draw_overlay_menu_statistics();
		void draw_overlay_menu_log();
		void draw_overlay_menu_about();
		void draw_code_editor();
		void draw_overlay_variable_editor();
		void draw_overlay_technique_editor();
		void draw_preset_explorer();

		std::vector<const texture *> _texture_previews;
		std::vector<std::pair<std::string, std::function<void()>>> _menu_callables;
		std::unique_ptr<texture> _imgui_font_atlas;
		ImGuiContext *_imgui_context = nullptr;
		size_t _focused_effect = std::numeric_limits<size_t>::max();
		size_t _selected_effect = std::numeric_limits<size_t>::max();
		size_t _selected_technique = std::numeric_limits<size_t>::max();
		int _input_processing_mode = 2;
		int _style_index = 2;
		int _editor_style_index = 0;
		int _font_size = 13;
		int _editor_font_size = 13;
		int _clock_format = 0;
		std::filesystem::path _font;
		std::filesystem::path _editor_font;
		unsigned int _menu_key_data[4];
		bool _show_menu = false;
		bool _show_clock = false;
		bool _show_fps = false;
		bool _show_frametime = false;
		bool _show_splash = true;
		bool _show_code_editor = false;
		bool _show_screenshot_message = true;
		bool _no_font_scaling = false;
		bool _log_wordwrap = false;
		bool _variable_editor_tabs = false;
		bool _selected_effect_changed = false;
		bool _rebuild_font_atlas = false;
		bool _was_preprocessor_popup_edited = false;
		bool _statistics_effects_show_enabled = false;
		float _fps_col[4] = { 1.0f, 1.0f, 0.784314f, 1.0f };
		float _fps_scale = 1.0f;
		float _variable_editor_height = 0.0f;
		unsigned int _tutorial_index = 0;
		unsigned int _effects_expanded_state = 2;
		char _effect_filter_buffer[64] = {};
		std::filesystem::path _file_selection_path;
		imgui_code_editor _editor;

		// used by preset explorer
		bool _browse_path_is_input_mode = false;
		bool _current_preset_is_covered = true;
		std::filesystem::path _current_browse_path;
#endif
	};
}
