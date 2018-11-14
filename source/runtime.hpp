/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <chrono>
#include <memory>
#include <functional>
#include <filesystem>
#include <mutex>
#include <atomic>
#include "variant.hpp"
#include "ini_file.hpp"
#include "moving_average.hpp"
#include "effect_codegen.hpp"
#include "gui_text_editor.hpp"

#pragma region Forward Declarations
struct ImDrawData;
struct ImGuiContext;

extern volatile long g_network_traffic;

extern std::filesystem::path g_reshade_dll_path;
extern std::filesystem::path g_target_executable_path;
extern std::filesystem::path g_system_path;
extern std::filesystem::path g_windows_path;
#pragma endregion

namespace reshade
{
	enum class special_uniform
	{
		none,
		frame_time,
		frame_count,
		random,
		ping_pong,
		date,
		timer,
		key,
		mouse_point,
		mouse_delta,
		mouse_button,
	};
	enum class texture_reference
	{
		none,
		back_buffer,
		depth_buffer
	};

	class base_object abstract
	{
	public:
		virtual ~base_object() { }

		template <typename T>
		T *as() { return dynamic_cast<T *>(this); }
		template <typename T>
		const T *as() const { return dynamic_cast<const T *>(this); }
	};

	struct effect_data
	{
		std::string errors;
		reshadefx::module module;
		std::filesystem::path source_file;
		size_t storage_offset, storage_size;
	};

	struct texture final : reshadefx::texture_info
	{
		texture(const reshadefx::texture_info &init) : texture_info(init) { }

		std::string effect_filename;
		texture_reference impl_reference = texture_reference::none;
		std::unique_ptr<base_object> impl;
		bool shared = false;
	};
	struct uniform final : reshadefx::uniform_info
	{
		uniform(const reshadefx::uniform_info &init) : uniform_info(init) { }

		std::string effect_filename;
		size_t storage_offset = 0;
		bool hidden = false;
		special_uniform special = special_uniform::none;
	};
	struct technique final : reshadefx::technique_info
	{
		technique(const reshadefx::technique_info &init) : technique_info(init) { }

		size_t effect_index;
		std::string effect_filename;
		std::vector<std::unique_ptr<base_object>> passes_data;
		bool hidden = false;
		bool enabled = false;
		int32_t timeout = 0;
		int64_t timeleft = 0;
		uint32_t toggle_key_data[4];
		moving_average<uint64_t, 60> average_cpu_duration;
		moving_average<uint64_t, 60> average_gpu_duration;
		std::unique_ptr<base_object> impl;
	};

	class runtime abstract
	{
	public:
		/// <summary>
		/// Construct a new runtime instance.
		/// </summary>
		/// <param name="renderer">A unique number identifying the renderer implementation.</param>
		runtime(uint32_t renderer);
		virtual ~runtime();

		/// <summary>
		/// Returns the frame width in pixels.
		/// </summary>
		unsigned int frame_width() const { return _width; }
		/// <summary>
		/// Returns the frame height in pixels.
		/// </summary>
		unsigned int frame_height() const { return _height; }

		/// <summary>
		/// Create a copy of the current frame.
		/// </summary>
		/// <param name="buffer">The buffer to save the copy to. It has to be the size of at least "frame_width() * frame_height() * 4".</param>
		virtual void capture_frame(uint8_t *buffer) const = 0;

		/// <summary>
		/// Returns the initialization status.
		/// </summary>
		bool is_initialized() const { return _is_initialized; }
		/// <summary>
		/// Returns a boolean indicating whether any effects were loaded.
		/// </summary>
		bool is_effect_loaded() const { return !_techniques.empty() && _reload_remaining_effects == std::numeric_limits<size_t>::max(); }

		/// <summary>
		/// Get the value of a uniform variable.
		/// </summary>
		/// <param name="variable">The variable to retrieve the value from.</param>
		/// <param name="data">The buffer to store the value in.</param>
		/// <param name="size">The size of the buffer.</param>
		void get_uniform_value(const uniform &variable, uint8_t *data, size_t size) const;
		void get_uniform_value(const uniform &variable, bool *values, size_t count) const;
		void get_uniform_value(const uniform &variable, int32_t *values, size_t count) const;
		void get_uniform_value(const uniform &variable, uint32_t *values, size_t count) const;
		void get_uniform_value(const uniform &variable, float *values, size_t count) const;
		/// <summary>
		/// Update the value of a uniform variable.
		/// </summary>
		/// <param name="variable">The variable to update.</param>
		/// <param name="data">The value data to update the variable to.</param>
		/// <param name="size">The size of the value.</param>
		void set_uniform_value(uniform &variable, const uint8_t *data, size_t size);
		void set_uniform_value(uniform &variable, const bool *values, size_t count);
		void set_uniform_value(uniform &variable, const int32_t *values, size_t count);
		void set_uniform_value(uniform &variable, const uint32_t *values, size_t count);
		void set_uniform_value(uniform &variable, const float *values, size_t count);
		void set_uniform_value(uniform &variable, bool     x, bool     y = false, bool     z = false, bool     w = false) { const bool     data[4] = { x, y, z, w }; set_uniform_value(variable, data, 4); }
		void set_uniform_value(uniform &variable, int32_t  x, int32_t  y = 0,     int32_t  z = 0,     int32_t  w = 0    ) { const int32_t  data[4] = { x, y, z, w }; set_uniform_value(variable, data, 4); }
		void set_uniform_value(uniform &variable, uint32_t x, uint32_t y = 0u,    uint32_t z = 0u,    uint32_t w = 0u   ) { const uint32_t data[4] = { x, y, z, w }; set_uniform_value(variable, data, 4); }
		void set_uniform_value(uniform &variable, float    x, float    y = 0.0f,  float    z = 0.0f,  float    w = 0.0f ) { const float    data[4] = { x, y, z, w }; set_uniform_value(variable, data, 4); }

		/// <summary>
		/// Reset a uniform variable to its initial value.
		/// </summary>
		/// <param name="variable">The variable to update.</param>
		void reset_uniform_value(uniform &variable);

		/// <summary>
		/// Register a function to be called when the menu is drawn.
		/// </summary>
		/// <param name="label">Name of the tab in the menu bar.</param>
		/// <param name="function">The callback function.</param>
		void subscribe_to_ui(std::string label, std::function<void()> function) { _menu_callables.push_back({ label, function }); }
		/// <summary>
		/// Register a function to be called when user configuration is loaded.
		/// </summary>
		/// <param name="function">The callback function.</param>
		void subscribe_to_load_config(std::function<void(const ini_file &)> function)
		{
			_load_config_callables.push_back(function);

			const ini_file config(_configuration_path);
			function(config);
		}
		/// <summary>
		/// Register a function to be called when user configuration is stored.
		/// </summary>
		/// <param name="function">The callback function.</param>
		void subscribe_to_save_config(std::function<void(ini_file &)> function)
		{
			_save_config_callables.push_back(function);

			ini_file config(_configuration_path);
			function(config);
		}

	protected:
		/// <summary>
		/// Callback function called when the runtime is initialized.
		/// </summary>
		/// <returns>Returns if the initialization succeeded.</returns>
		bool on_init();
		/// <summary>
		/// Callback function called when the runtime is uninitialized.
		/// </summary>
		void on_reset();
		/// <summary>
		/// Callback function called when the post-processing effects are uninitialized.
		/// </summary>
		virtual void on_reset_effect();
		/// <summary>
		/// Callback function called every frame.
		/// </summary>
		void on_present();
		/// <summary>
		/// Callback function called to apply the post-processing effects to the screen.
		/// </summary>
		void on_present_effect();

		/// <summary>
		/// Compile effect from the specified source file and initialize textures, constants and techniques.
		/// </summary>
		/// <param name="path">The path to an effect source code file.</param>
		void load_effect(const std::filesystem::path &path);
		void unload_effect(const std::filesystem::path &path);
		/// <summary>
		/// Compile effect from the specified effect module.
		/// </summary>
		/// <param name="effect">The effect module to compile.</param>
		virtual bool compile_effect(effect_data &effect) = 0;

		/// <summary>
		/// Loads image files and updates all textures with image data.
		/// </summary>
		void load_textures();
		/// <summary>
		/// Update the image data of a texture.
		/// </summary>
		/// <param name="texture">The texture to update.</param>
		/// <param name="data">The 32bpp RGBA image data to update the texture to.</param>
		virtual void update_texture(texture &texture, const uint8_t *data) = 0;

		/// <summary>
		/// Load user configuration from disk.
		/// </summary>
		void load_config();
		/// <summary>
		/// Save user configuration to disk.
		/// </summary>
		void save_config() const;
		/// <summary>
		/// Save user configuration to disk.
		/// </summary>
		/// <param name="path">Output configuration path.</param>
		void save_config(const std::filesystem::path &path) const;

		/// <summary>
		/// Render all passes in a technique.
		/// </summary>
		/// <param name="technique">The technique to render.</param>
		virtual void render_technique(const technique &technique) = 0;
		/// <summary>
		/// Render command lists obtained from ImGui.
		/// </summary>
		/// <param name="data">The draw data to render.</param>
		virtual void render_imgui_draw_data(ImDrawData *draw_data) = 0;

		unsigned int _width = 0, _height = 0;
		unsigned int _vendor_id = 0, _device_id = 0;
		uint64_t _framecount = 0;
		unsigned int _drawcalls = 0, _vertices = 0;
		std::shared_ptr<class input> _input;
		ImGuiContext *_imgui_context = nullptr;
		std::unique_ptr<base_object> _imgui_font_atlas_texture;
		std::vector<texture> _textures;
		std::vector<uniform> _uniforms;
		std::vector<technique> _techniques;
		std::vector<unsigned char> _uniform_data_storage;

	private:
		static bool check_for_update(unsigned long latest_version[3]);

		void reload();

		void enable_technique(technique &technique);
		void disable_technique(technique &technique);

		void load_preset(const std::filesystem::path &path);
		void load_current_preset();
		void save_preset(const std::filesystem::path &path) const;
		void save_preset(const std::filesystem::path &path, const std::filesystem::path &save_path) const;
		void save_current_preset() const;

		void save_screenshot() const;

		void init_ui();
		void deinit_ui();

		void draw_ui();
		void draw_code_editor();
		void draw_overlay_menu_home();
		void draw_overlay_menu_settings();
		void draw_overlay_menu_statistics();
		void draw_overlay_menu_log();
		void draw_overlay_menu_about();
		void draw_overlay_variable_editor();
		void draw_overlay_technique_editor();

		void filter_techniques(const std::string &filter);

		const unsigned int _renderer_id;
		bool _needs_update = false;
		unsigned long _latest_version[3] = {};
		bool _is_initialized = false;

		bool _effects_enabled = true;
		unsigned int _reload_key_data[4];
		unsigned int _effects_key_data[4];
		unsigned int _screenshot_key_data[4];
		int _screenshot_format = 0;
		std::filesystem::path _screenshot_path;
		std::filesystem::path _configuration_path;

		int _current_preset = -1;
		std::vector<std::filesystem::path> _preset_files;

		std::vector<std::string> _preprocessor_definitions;
		std::vector<std::filesystem::path> _effect_search_paths;
		std::vector<std::filesystem::path> _texture_search_paths;

		bool _textures_loaded = false;
		bool _performance_mode = false;
		bool _last_reload_successful = true;
		std::mutex _reload_mutex;
		size_t _reload_total_effects = 1;
		std::vector<size_t> _reload_compile_queue;
		std::atomic<size_t> _reload_remaining_effects = 0;
		std::vector<struct effect_data> _loaded_effects;

		int _date[4] = {};
		std::chrono::high_resolution_clock::duration _last_frame_duration;
		std::chrono::high_resolution_clock::time_point _start_time;
		std::chrono::high_resolution_clock::time_point _last_reload_time;
		std::chrono::high_resolution_clock::time_point _last_present_time;

		std::vector<std::pair<std::string, std::function<void()>>> _menu_callables;
		std::vector<std::function<void(ini_file &)>> _save_config_callables;
		std::vector<std::function<void(const ini_file &)>> _load_config_callables;

		int _selected_effect = -1;
		int _selected_technique = -1;
		int _input_processing_mode = 2;
		int _style_index = 2;
		int _editor_style_index = 0;
		float _fps_col[4] = { 1.0f, 1.0f, 0.784314f, 1.0f };
		unsigned int _menu_key_data[4];
		std::string _focus_effect;
		bool _show_menu = false;
		bool _show_clock = false;
		bool _show_framerate = false;
		bool _show_splash = true;
		bool _show_code_editor = false;
		bool _screenshot_include_preset = false;
		bool _screenshot_include_configuration = false;
		bool _no_font_scaling = false;
		bool _no_reload_on_init = false;
		bool _overlay_key_setting_active = false;
		bool _screenshot_key_setting_active = false;
		bool _toggle_key_setting_active = false;
		bool _log_wordwrap = false;
		float _variable_editor_height = 0.0f;
		unsigned int _tutorial_index = 0;
		unsigned int _effects_expanded_state = 2;
		char _effect_filter_buffer[64] = {};
		code_editor_widget _editor;
	};
}
