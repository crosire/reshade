/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <chrono>
#include <functional>
#include <filesystem>
#include <atomic>
#include "ini_file.hpp"
#include "runtime_objects.hpp"

#pragma region Forward Declarations
struct ImDrawData;
struct ImFontAtlas;
struct ImGuiContext;

namespace reshade
{
	class input;
}
namespace reshadefx
{
	struct module;
}

extern volatile long g_network_traffic;

extern std::filesystem::path g_reshade_dll_path;
extern std::filesystem::path g_target_executable_path;
extern std::filesystem::path g_system_path;
extern std::filesystem::path g_windows_path;
#pragma endregion

namespace reshade
{
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
		bool is_effect_loaded() const { return _technique_count > 0 && _reload_remaining_effects == 0; }

		/// <summary>
		/// Find the texture with the specified name.
		/// </summary>
		/// <param name="unique_name">The name of the texture.</param>
		texture *find_texture(const std::string &unique_name);

		/// <summary>
		/// Get the value of a uniform variable.
		/// </summary>
		/// <param name="variable">The variable to retrieve the value from.</param>
		/// <param name="data">The buffer to store the value in.</param>
		/// <param name="size">The size of the buffer.</param>
		void get_uniform_value(const uniform &variable, unsigned char *data, size_t size) const;
		void get_uniform_value(const uniform &variable, bool *values, size_t count) const;
		void get_uniform_value(const uniform &variable, int *values, size_t count) const;
		void get_uniform_value(const uniform &variable, unsigned int *values, size_t count) const;
		void get_uniform_value(const uniform &variable, float *values, size_t count) const;
		/// <summary>
		/// Update the value of a uniform variable.
		/// </summary>
		/// <param name="variable">The variable to update.</param>
		/// <param name="data">The value data to update the variable to.</param>
		/// <param name="size">The size of the value.</param>
		void set_uniform_value(uniform &variable, const unsigned char *data, size_t size);
		void set_uniform_value(uniform &variable, const bool *values, size_t count);
		void set_uniform_value(uniform &variable, const int *values, size_t count);
		void set_uniform_value(uniform &variable, const unsigned int *values, size_t count);
		void set_uniform_value(uniform &variable, const float *values, size_t count);

		void reset_uniform_value(uniform &variable);

		/// <summary>
		/// Register a function to be called when the menu is drawn.
		/// </summary>
		/// <param name="label">Name of the tab in the menu bar.</param>
		/// <param name="function">The callback function.</param>
		void subscribe_to_menu(std::string label, std::function<void()> function)
		{
			_menu_callables.push_back({ label, function });
		}
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
		/// <summary>
		/// Compile effect from the specified abstract syntax tree and initialize textures, constants and techniques.
		/// </summary>
		/// <param name="module">The effect module to compile.</param>
		/// <param name="errors">A reference to a buffer to store errors which occur during compilation.</param>
		virtual bool load_effect(const reshadefx::module &module, std::string &errors) = 0;

		/// <summary>
		/// Loads image files and updates all textures with image data.
		/// </summary>
		void load_textures();
		/// <summary>
		/// Update the image data of a texture.
		/// </summary>
		/// <param name="texture">The texture to update.</param>
		/// <param name="data">The 32bpp RGBA image data to update the texture to.</param>
		virtual bool update_texture(texture &texture, const uint8_t *data) = 0;

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
		std::shared_ptr<input> _input;
		ImGuiContext *_imgui_context = nullptr;
		std::unique_ptr<base_object> _imgui_font_atlas_texture;
		std::vector<texture> _textures;
		std::vector<uniform> _uniforms;
		std::vector<technique> _techniques;
		std::vector<unsigned char> _uniform_data_storage;
		std::vector<unsigned char> _uniform_init_storage;

	private:
		static bool check_for_update(unsigned long latest_version[3]);

		void active_technique(const std::string &name);
		void deactive_technique(const std::string &name);

		void reload();
		void load_preset(const std::filesystem::path &path);
		void load_current_preset();
		void save_preset(const std::filesystem::path &path) const;
		void save_preset(const std::filesystem::path &path, const std::filesystem::path &save_path) const;
		void save_current_preset() const;
		void save_screenshot() const;

		void draw_overlay();
		void draw_overlay_menu();
		void draw_overlay_menu_home();
		void draw_overlay_menu_settings();
		void draw_overlay_menu_statistics();
		void draw_overlay_menu_log();
		void draw_overlay_menu_about();
		void draw_overlay_variable_editor();
		void draw_overlay_technique_editor();

		void filter_techniques(const std::string &filter);

		const unsigned int _renderer_id;
		bool _is_initialized = false;
		std::vector<reshadefx::module> _effect_modules;
		std::vector<std::filesystem::path> _effect_files;
		std::unordered_map<std::string, size_t> _technique_to_effect;
		std::vector<std::filesystem::path> _preset_files;
		std::vector<std::filesystem::path> _effect_search_paths;
		std::vector<std::filesystem::path> _texture_search_paths;
		std::chrono::high_resolution_clock::time_point _start_time;
		std::chrono::high_resolution_clock::time_point _last_reload_time;
		std::chrono::high_resolution_clock::time_point _last_present_time;
		std::chrono::high_resolution_clock::duration _last_frame_duration;
		int _date[4] = { };
		std::vector<std::string> _preprocessor_definitions;
		std::vector<std::pair<std::string, std::function<void()>>> _menu_callables;
		std::vector<std::function<void(const ini_file &)>> _load_config_callables;
		std::vector<std::function<void(ini_file &)>> _save_config_callables;
		size_t _menu_index = 0;
		int _screenshot_format = 0;
		int _current_preset = -1;
		int _selected_technique = -1;
		int _input_processing_mode = 2;
		unsigned int _menu_key_data[4];
		unsigned int _screenshot_key_data[4];
		unsigned int _reload_key_data[4];
		unsigned int _effects_key_data[4];
		std::filesystem::path _configuration_path;
		std::filesystem::path _screenshot_path;
		std::string _focus_effect;
		bool _needs_update = false;
		unsigned long _latest_version[3] = { };
		bool _show_menu = false;
		bool _show_clock = false;
		bool _show_framerate = false;
		bool _effects_enabled = true;
		bool _is_fast_loading = false;
		bool _screenshot_include_preset = false;
		bool _screenshot_include_configuration = false;
		bool _no_font_scaling = false;
		bool _no_reload_on_init = false;
		bool _performance_mode = false;
		bool _save_imgui_window_state = false;
		bool _overlay_key_setting_active = false;
		bool _screenshot_key_setting_active = false;
		bool _toggle_key_setting_active = false;
		bool _log_wordwrap = false;
		bool _last_reload_successful = true;
		unsigned char _switched_menu = 0;
		float _imgui_col_background[3] = { 0.117647f, 0.117647f, 0.117647f };
		float _imgui_col_item_background[3] = { 0.156863f, 0.156863f, 0.156863f };
		float _imgui_col_active[3] = { 0.392157f, 0.588235f, 0.941176f };
		float _imgui_col_text[3] = { 0.862745f, 0.862745f, 0.862745f };
		float _imgui_col_text_fps[3] = { 1.0f, 1.0f, 0.784314f };
		float _variable_editor_height = 0.0f;
		unsigned int _tutorial_index = 0;
		unsigned int _effects_expanded_state = 2;
		char _effect_filter_buffer[64] = { };
		std::atomic<size_t> _reload_remaining_effects = 0;
		size_t _texture_count = 0;
		size_t _uniform_count = 0;
		size_t _technique_count = 0;
	};
}
