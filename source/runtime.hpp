/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_api.hpp"
#if RESHADE_GUI
#include "imgui_code_editor.hpp"

struct ImDrawData;
struct ImGuiContext;
#endif

#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>
#include <filesystem>

class ini_file;

namespace reshade
{
	// Forward declarations to avoid excessive #include
	struct effect;
	struct uniform;
	struct texture;
	struct technique;

	/// <summary>
	/// Platform independent base class for the main ReShade effect runtime.
	/// </summary>
	class DECLSPEC_NOVTABLE runtime : public api::effect_runtime
	{
	public:
		/// <summary>
		/// Gets a boolean indicating whether the runtime is initialized.
		/// </summary>
		bool is_initialized() const { return _is_initialized; }

		/// <summary>
		/// Gets the frame width and height in pixels.
		/// </summary>
		void get_frame_width_and_height(uint32_t *width, uint32_t *height) const final { *width = _width; *height = _height; }

		/// <summary>
		/// Creates a copy of the current frame image in system memory.
		/// </summary>
		/// <param name="buffer">The 32bpp RGBA buffer to save the screenshot to.</param>
		bool take_screenshot(uint8_t *buffer);
		/// <summary>
		/// Creates a copy of the current frame and write it to an image file on disk.
		/// </summary>
		void save_screenshot(const std::wstring &postfix = std::wstring(), bool should_save_preset = false);

		/// <summary>
		/// Gets the value of a uniform variable.
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
		/// Updates the value of a uniform variable.
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
		/// Resets a uniform variable to its initial value.
		/// </summary>
		/// <param name="variable">The variable to update.</param>
		void reset_uniform_value(uniform &variable);

		/// <summary>
		/// Updates all effect textures with the specified <paramref name="semantic"/> to the specified shader resource view.
		/// </summary>
		void update_texture_bindings(const char *semantic, api::resource_view srv) final;

		/// <summary>
		/// Updates the values of all uniform variables with a "source" annotation set to <paramref name="source"/> to the specified <paramref name="values"/>.
		/// </summary>
		void update_uniform_variables(const char *source, const bool *values, size_t count, size_t array_index) final;
		void update_uniform_variables(const char *source, const float *values, size_t count, size_t array_index) final;
		void update_uniform_variables(const char *source, const int32_t *values, size_t count, size_t array_index) final;
		void update_uniform_variables(const char *source, const uint32_t *values, size_t count, size_t array_index) final;

	protected:
		runtime(api::device *device, api::command_queue *graphics_queue);
		~runtime();

		api::device *get_device() final { return _device; }
		api::command_queue *get_command_queue() final { return _graphics_queue; }

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

		api::device *const _device;
		api::command_queue *const _graphics_queue;
		unsigned int _width = 0;
		unsigned int _height = 0;
		unsigned int _vendor_id = 0;
		unsigned int _device_id = 0;
		unsigned int _renderer_id = 0;
		api::format  _backbuffer_format = api::format::unknown;
		bool _is_vr = false;

	private:
		/// <summary>
		/// Compare current version against the latest published one.
		/// </summary>
		/// <param name="latest_version">Contains the latest version after this function returned.</param>
		/// <returns><c>true</c> if an update is available, <c>false</c> otherwise</returns>
		static bool check_for_update(unsigned long latest_version[3]);

		/// <summary>
		/// Load user configuration from disk.
		/// </summary>
		void load_config();
		/// <summary>
		/// Save user configuration to disk.
		/// </summary>
		void save_config() const;

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
		/// Checks whether runtime is currently loading effects.
		/// </summary>
		bool is_loading() const { return _reload_remaining_effects != std::numeric_limits<size_t>::max(); }

		/// <summary>
		/// Compile effect from the specified source file and initialize textures, uniforms and techniques.
		/// </summary>
		/// <param name="source_file">The path to an effect source code file.</param>
		/// <param name="preset">The preset to be used to fill specialization constants or check whether loading can be skipped.</param>
		/// <param name="effect_index">The ID of the effect.</param>
		bool load_effect(const std::filesystem::path &source_file, const ini_file &preset, size_t effect_index, bool preprocess_required = false);
		/// <summary>
		/// Load all effects found in the effect search paths.
		/// </summary>
		void load_effects();
		/// <summary>
		/// Load image files and update textures with image data.
		/// </summary>
		void load_textures();
		/// <summary>
		/// Unload the specified effect.
		/// </summary>
		/// <param name="effect_index">The ID of the effect.</param>
		void unload_effect(size_t effect_index);
		/// <summary>
		/// Unload all effects currently loaded.
		/// </summary>
		void unload_effects();
		/// <summary>
		/// Reload only the specified effect.
		/// </summary>
		/// <param name="effect_index">The ID of the effect.</param>
		bool reload_effect(size_t effect_index, bool preprocess_required = false);
		/// <summary>
		/// Unload all effects and then load them again.
		/// </summary>
		void reload_effects();

		/// <summary>
		/// Initialize resources for the effect and load the effect module.
		/// </summary>
		/// <param name="effect_index">The ID of the effect.</param>
		bool init_effect(size_t effect_index);
		/// <summary>
		/// Create a new texture with the specified dimensions.
		/// </summary>
		/// <param name="texture">The texture description.</param>
		bool init_texture(texture &texture);
		/// <summary>
		/// Destroy an existing texture.
		/// </summary>
		/// <param name="texture">The texture to destroy.</param>
		void destroy_texture(texture &texture);

		/// <summary>
		/// Load compiled effect data from the disk cache.
		/// </summary>
		bool load_effect_cache(const std::filesystem::path &source_file, const size_t hash, std::string &source) const;
		bool load_effect_cache(const std::filesystem::path &source_file, const std::string &entry_point, const size_t hash, std::vector<char> &cso, std::string &dasm) const;
		/// <summary>
		/// Save compiled effect data to the disk cache.
		/// </summary>
		bool save_effect_cache(const std::filesystem::path &source_file, const size_t hash, const std::string &source) const;
		bool save_effect_cache(const std::filesystem::path &source_file, const std::string &entry_point, const size_t hash, const std::vector<char> &cso, const std::string &dasm) const;
		/// <summary>
		/// Remove all compiled effect data from disk.
		/// </summary>
		void clear_effect_cache();

		/// <summary>
		/// Apply post-processing effects to the frame.
		/// </summary>
		void update_and_render_effects();
		/// <summary>
		/// Render all passes in a technique.
		/// </summary>
		/// <param name="technique">The technique to render.</param>
		void render_technique(technique &technique);

		/// <summary>
		/// Returns the texture object corresponding to the passed <paramref name="unique_name"/>.
		/// </summary>
		/// <param name="unique_name">The name of the texture to find.</param>
		texture &look_up_texture_by_name(const std::string &unique_name);

		// === Status ===

		bool _is_initialized = false;
		bool _effects_enabled = true;
		bool _ignore_shortcuts = false;
		bool _force_shortcut_modifiers = true;
		unsigned int _effects_key_data[4];
		std::shared_ptr<class input> _input;
		std::chrono::high_resolution_clock::duration _last_frame_duration;
		std::chrono::high_resolution_clock::time_point _start_time;
		std::chrono::high_resolution_clock::time_point _last_present_time;
		uint64_t _framecount = 0;
		unsigned int _color_bit_depth = 8;

		// === Configuration ===

		bool _needs_update = false;
		unsigned long _latest_version[3] = {};
		std::filesystem::path _config_path;

		// === Effect Loading ===

		bool _no_debug_info = 0;
		bool _no_effect_cache = false;
		bool _no_reload_on_init = false;
		bool _no_reload_for_non_vr = false;
		bool _performance_mode = false;
		bool _effect_load_skipping = false;
		bool _load_option_disable_skipping = false;
		std::atomic<int> _last_reload_successfull = true;
		bool _last_texture_reload_successfull = true;
		bool _textures_loaded = false;
		unsigned int _reload_key_data[4];
		unsigned int _performance_mode_key_data[4];
		std::vector<size_t> _reload_compile_queue;
		std::atomic<size_t> _reload_remaining_effects = 0;
		std::mutex _reload_mutex;
		std::vector<std::thread> _worker_threads;
		std::vector<std::string> _global_preprocessor_definitions;
		std::vector<std::string> _preset_preprocessor_definitions;
		std::vector<std::filesystem::path> _effect_search_paths;
		std::vector<std::filesystem::path> _texture_search_paths;
		std::filesystem::path _intermediate_cache_path;
		std::chrono::high_resolution_clock::time_point _last_reload_time;
		void *_d3d_compiler = nullptr;

		std::vector<effect> _effects;
		std::vector<texture> _textures;
		std::vector<technique> _techniques;

		// === Effect Rendering ===

		std::vector<api::framebuffer> _backbuffer_fbos;
		std::vector<api::render_pass> _backbuffer_passes;
		std::vector<api::resource_view> _backbuffer_targets;
		api::resource _backbuffer_texture = {};
		api::resource_view _backbuffer_texture_view[2] = {};
		api::format _effect_stencil_format = api::format::unknown;
		api::resource _effect_stencil = {};
		api::resource_view _effect_stencil_target = {};
		api::resource _empty_texture = {};
		api::resource_view _empty_texture_view = {};
		std::unordered_map<size_t, api::sampler> _effect_sampler_states;
		std::unordered_map<std::string, api::resource_view> _texture_semantic_bindings;

		// === Screenshots ===

		bool _should_save_screenshot = false;
		bool _screenshot_save_gui = false;
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
		// === ImGui ===

		void init_gui();
		void init_gui_vr();
		void deinit_gui();
		void deinit_gui_vr();
		void build_font_atlas();

		void load_config_gui(const ini_file &config);
		void save_config_gui(ini_file &config) const;

		void load_custom_style();
		void save_custom_style() const;

		void draw_gui();
		void draw_gui_vr();

		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *draw_data, api::render_pass pass, api::framebuffer fbo);
		void destroy_imgui_resources();

		ImGuiContext *_imgui_context = nullptr;

		api::resource _font_atlas = {};
		api::resource_view _font_atlas_srv = {};
		api::sampler _imgui_sampler_state = {};
		api::pipeline _imgui_pipeline = {};
		api::pipeline_layout _imgui_pipeline_layout = {};
		api::resource _imgui_indices[4] = {};
		api::resource _imgui_vertices[4] = {};
		int _imgui_num_indices[4] = {};
		int _imgui_num_vertices[4] = {};

		api::resource _vr_overlay_texture = {};
		api::framebuffer _vr_overlay_fbo = {};
		api::render_pass _vr_overlay_pass = {};
		api::resource_view _vr_overlay_target = {};

		// === User Interface ===

		void draw_gui_home();
		void draw_gui_settings();
		void draw_gui_statistics();
		void draw_gui_log();
		void draw_gui_about();
#if RESHADE_ADDON
		void draw_gui_addons();
#endif
		void draw_variable_editor();
		void draw_technique_editor();

		unsigned int _window_width = 0;
		unsigned int _window_height = 0;
		std::vector<std::pair<std::string, std::function<void()>>> _menu_callables;
		bool _show_splash = true;
		bool _show_overlay = false;
		bool _show_fps = false;
		bool _show_clock = false;
		bool _show_frametime = false;
		bool _show_screenshot_message = true;
		bool _no_font_scaling = false;
		bool _rebuild_font_atlas = true;
		bool _gather_gpu_statistics = false;
		unsigned int _reload_count = 0;
		unsigned int _overlay_key_data[4];
		int _fps_pos = 1;
		int _clock_format = 0;
		int _input_processing_mode = 2;
		size_t _selected_menu = 0;

		// === User Interface - Home ===

		char _effect_filter[64] = "";
		bool _variable_editor_tabs = false;
		bool _duplicate_current_preset = false;
		bool _was_preprocessor_popup_edited = false;
		size_t _focused_effect = std::numeric_limits<size_t>::max();
		size_t _selected_technique = std::numeric_limits<size_t>::max();
		unsigned int _tutorial_index = 0;
		unsigned int _effects_expanded_state = 2;
		float _variable_editor_height = 300.0f;

		// === User Interface - Add-ons ===

		char _addons_filter[64] = {};
		size_t _open_addon_index = std::numeric_limits<size_t>::max();

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
		bool _show_force_load_effects_button = true;

		// === User Interface - Statistics ===

		api::resource_view _preview_texture = { 0 };
		unsigned int _preview_size[3] = { 0, 0, 0xFFFFFFFF };

		// === User Interface - Log ===

		bool _log_wordwrap = false;
		uintmax_t _last_log_size;
		std::vector<std::string> _log_lines;

		// === User Interface - Code Editor ===

		struct editor_instance
		{
			size_t effect_index;
			std::filesystem::path file_path;
			std::string entry_point_name;
			gui::code_editor editor;
			bool selected = false;
		};

		void open_code_editor(size_t effect_index, const std::string &entry_point);
		void open_code_editor(size_t effect_index, const std::filesystem::path &path);
		void open_code_editor(editor_instance &instance);
		void draw_code_editor(editor_instance &instance);

		std::vector<editor_instance> _editors;
		uint32_t _editor_palette[gui::code_editor::color_palette_max];
#endif
	};
}
