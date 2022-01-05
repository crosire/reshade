/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <memory>
#include <filesystem>
#include "reshade_api.hpp"
#if RESHADE_GUI
#include "imgui_code_editor.hpp"
#endif

class ini_file;

namespace reshade
{
	// Forward declarations to avoid excessive #include
	struct effect;
	struct uniform;
	struct texture;
	struct technique;
	struct history;

	/// <summary>
	/// The main ReShade post-processing effect runtime.
	/// </summary>
	class __declspec(novtable) runtime : public api::effect_runtime
	{
	public:
		/// <summary>
		/// Gets the parent device for this effect runtime.
		/// </summary>
		api::device *get_device() final { return _device; }

		/// <summary>
		/// Gets the main graphics command queue associated with this effect runtime.
		/// </summary>
		api::command_queue *get_command_queue() final { return _graphics_queue; }

		/// <summary>
		/// Gets the path to the configuration file used by this effect runtime.
		/// </summary>
		inline std::filesystem::path get_config_path() const { return _config_path; }

#if RESHADE_EFFECTS
		/// <summary>
		/// Gets a boolean indicating whether effects are being loaded.
		/// </summary>
		bool is_loading() const { return _reload_remaining_effects != std::numeric_limits<size_t>::max(); }
#endif
		/// <summary>
		/// Gets a boolean indicating whether the runtime is initialized.
		/// </summary>
		bool is_initialized() const { return _is_initialized; }

		/// <summary>
		/// Gets a resolved resource for the specified back buffer <paramref name="index"/> in this swap chain.
		/// </summary>
		virtual api::resource get_back_buffer_resolved(uint32_t index) = 0;

#if RESHADE_EFFECTS
		/// <summary>
		/// Applies post-processing effects to the specified render targets.
		/// </summary>
		virtual void render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb) override;
#else
		virtual void render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb) final { cmd_list; rtv; rtv_srgb; }
#endif

		/// <summary>
		/// Captures a screenshot of the current back buffer resource and writes it to an image file on disk.
		/// </summary>
		void save_screenshot(const std::wstring &postfix = std::wstring());
		/// <summary>
		/// Captures a screenshot of the current back buffer resource and returns its image data in 32 bits-per-pixel RGBA format.
		/// </summary>
		bool capture_screenshot(uint8_t *pixels) final { return get_texture_data(get_back_buffer_resolved(get_current_back_buffer_index()), api::resource_usage::present, pixels); }

		/// <summary>
		/// Gets the current buffer dimensions of the swap chain as used with effect rendering.
		/// </summary>
		void get_screenshot_width_and_height(uint32_t *width, uint32_t *height) const final { *width = _width; *height = _height; }

		/// <summary>
		/// Gets the current status of the specified key.
		/// </summary>
		bool is_key_down(uint32_t keycode) const final;
		/// <summary>
		/// Gets whether the specified key was pressed this frame.
		/// </summary>
		bool is_key_pressed(uint32_t keycode) const final;
		/// <summary>
		/// Gets whether the specified key was released this frame.
		/// </summary>
		bool is_key_released(uint32_t keycode) const final;
		/// <summary>
		/// Gets the current status of the specified mouse button.
		/// </summary>
		bool is_mouse_button_down(uint32_t button) const final;
		/// <summary>
		/// Gets whether the specified mouse button was pressed this frame.
		/// </summary>
		bool is_mouse_button_pressed(uint32_t button) const final;
		/// <summary>
		/// Gets whether the specified mouse button was released this frame.
		/// </summary>
		bool is_mouse_button_released(uint32_t button) const final;

		/// <summary>
		/// Gets the current absolute position of the mouse cursor in screen coordinates.
		/// </summary>
		void get_cursor_position(uint32_t *out_x, uint32_t *out_y, int16_t *out_wheel_delta) const final;

		/// <summary>
		/// Enumerates all uniform variables of loaded effects and calls the specified <paramref name="callback"/> function with a handle for each one.
		/// </summary>
		void enumerate_uniform_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, api::effect_uniform_variable variable, void *user_data), void *user_data) final;

		/// <summary>
		/// Finds a specific uniform variable in the loaded effects and returns a handle to it.
		/// </summary>
		api::effect_uniform_variable find_uniform_variable(const char *effect_name, const char *variable_name) const final;

		/// <summary>
		/// Gets the name of a uniform <paramref name="variable"/>.
		/// </summary>
		void get_uniform_variable_name(api::effect_uniform_variable variable, char *name, size_t *length) const final;

		/// <summary>
		/// Gets the value from a named annotation attached to the specified uniform <paramref name="variable"/>.
		/// </summary>
		void get_uniform_annotation_value(api::effect_uniform_variable variable, const char *name, bool *values, size_t count, size_t array_index = 0) const final;
		void get_uniform_annotation_value(api::effect_uniform_variable variable, const char *name, float *values, size_t count, size_t array_index = 0) const final;
		void get_uniform_annotation_value(api::effect_uniform_variable variable, const char *name, int32_t *values, size_t count, size_t array_index = 0) const final;
		void get_uniform_annotation_value(api::effect_uniform_variable variable, const char *name, uint32_t *values, size_t count, size_t array_index = 0) const final;
		void get_uniform_annotation_value(api::effect_uniform_variable variable, const char *name, char *value, size_t *length) const final;

		/// <summary>
		/// Gets the value of the specified uniform <paramref name="variable"/>.
		/// </summary>
		void get_uniform_value(api::effect_uniform_variable variable, bool *values, size_t count, size_t array_index) const final;
		void get_uniform_value(api::effect_uniform_variable variable, float *values, size_t count, size_t array_index) const final;
		void get_uniform_value(api::effect_uniform_variable variable, int32_t *values, size_t count, size_t array_index) const final;
		void get_uniform_value(api::effect_uniform_variable variable, uint32_t *values, size_t count, size_t array_index) const final;

		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/>.
		/// </summary>
		void set_uniform_value(api::effect_uniform_variable variable, const bool *values, size_t count, size_t array_index) final;
		void set_uniform_value(api::effect_uniform_variable variable, const float *values, size_t count, size_t array_index) final;
		void set_uniform_value(api::effect_uniform_variable variable, const int32_t *values, size_t count, size_t array_index) final;
		void set_uniform_value(api::effect_uniform_variable variable, const uint32_t *values, size_t count, size_t array_index) final;

		/// <summary>
		/// Enumerates all texture variables of loaded effects and calls the specified <paramref name="callback"/> function with a handle for each one.
		/// </summary>
		void enumerate_texture_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, api::effect_texture_variable variable, void *user_data), void *user_data) final;

		/// <summary>
		/// Finds a specific texture variable in the loaded effects and returns a handle to it.
		/// </summary>
		api::effect_texture_variable find_texture_variable(const char *effect_name, const char *variable_name) const final;

		/// <summary>
		/// Gets the name of a texture <paramref name="variable"/>.
		/// </summary>
		void get_texture_variable_name(api::effect_texture_variable variable, char *name, size_t *length) const final;

		/// <summary>
		/// Gets the value from a named annotation attached to the specified texture <paramref name="variable"/>.
		/// </summary>
		void get_texture_annotation_value(api::effect_texture_variable variable, const char *name, bool *values, size_t count, size_t array_index = 0) const final;
		void get_texture_annotation_value(api::effect_texture_variable variable, const char *name, float *values, size_t count, size_t array_index = 0) const final;
		void get_texture_annotation_value(api::effect_texture_variable variable, const char *name, int32_t *values, size_t count, size_t array_index = 0) const final;
		void get_texture_annotation_value(api::effect_texture_variable variable, const char *name, uint32_t *values, size_t count, size_t array_index = 0) const final;
		void get_texture_annotation_value(api::effect_texture_variable variable, const char *name, char *value, size_t *length) const final;

		/// <summary>
		/// Uploads 32 bits-per-pixel RGBA image data to the specified texture <paramref name="variable"/>.
		/// </summary>
		void update_texture(api::effect_texture_variable variable, const uint32_t width, const uint32_t height, const uint8_t *pixels) final;

		/// <summary>
		/// Gets the shader resource views that are bound to the specified texture <paramref name="variable"/>.
		/// </summary>
		void get_texture_binding(api::effect_texture_variable variable, api::resource_view *out_srv, api::resource_view *out_srv_srgb) const final;

		/// <summary>
		/// Binds a new shader resource view to all texture variables that use the specified <paramref name="semantic"/>.
		/// </summary>
		void update_texture_bindings(const char *semantic, api::resource_view srv, api::resource_view srv_srgb) final;

		/// <summary>
		/// Enumerates all techniques of loaded effects and calls the specified <paramref name="callback"/> function with a handle for each one.
		/// </summary>
		void enumerate_techniques(const char *effect_name, void(*callback)(effect_runtime *runtime, api::effect_technique technique, void *user_data), void *user_data) final;

		/// <summary>
		/// Finds a specific technique in the loaded effects and returns a handle to it.
		/// </summary>
		api::effect_technique find_technique(const char *effect_name, const char *technique_name) final;

		/// <summary>
		/// Gets the name of a <paramref name="technique"/>.
		/// </summary>
		void get_technique_name(api::effect_technique technique, char *name, size_t *length) const final;

		/// <summary>
		/// Gets the value from an annotation attached to the specified <paramref name="technique"/>.
		/// </summary>
		void get_technique_annotation_value(api::effect_technique technique, const char *name, bool *values, size_t count, size_t array_index = 0) const final;
		void get_technique_annotation_value(api::effect_technique technique, const char *name, float *values, size_t count, size_t array_index = 0) const final;
		void get_technique_annotation_value(api::effect_technique technique, const char *name, int32_t *values, size_t count, size_t array_index = 0) const final;
		void get_technique_annotation_value(api::effect_technique technique, const char *name, uint32_t *values, size_t count, size_t array_index = 0) const final;
		void get_technique_annotation_value(api::effect_technique technique, const char *name, char *value, size_t *length) const final;

		/// <summary>
		/// Gets the status of a <paramref name="technique"/>.
		/// </summary>
		bool get_technique_enabled(api::effect_technique technique) const final;
		/// <summary>
		/// Enables or disable the specified <paramref name="technique"/>.
		/// </summary>
		void set_technique_enabled(api::effect_technique technique, bool enabled) final;

		/// <summary>
		/// Gets the value of global preprocessor definition.
		/// </summary>
		bool get_preprocessor_definition(const char *name, char *value, size_t *length) const final;
		/// <summary>
		/// Defines a global preprocessor definition to the specified <paramref name="value"/>.
		/// </summary>
		void set_preprocessor_definition(const char *name, const char *value) final;

	protected:
		runtime(api::device *device, api::command_queue *graphics_queue);
		~runtime();

		bool on_init(void *window);
		void on_reset();
		void on_present();

		api::device *const _device;
		api::command_queue *const _graphics_queue;
		unsigned int _width = 0;
		unsigned int _height = 0;
		unsigned int _vendor_id = 0;
		unsigned int _device_id = 0;
		unsigned int _renderer_id = 0;
		api::format  _back_buffer_format = api::format::unknown;
		bool _is_vr = false;

	private:
		static bool check_for_update(unsigned long latest_version[3]);

		void load_config();
		void save_config() const;

#if RESHADE_EFFECTS
		void load_current_preset();
		void save_current_preset() const;

		bool switch_to_next_preset(std::filesystem::path filter_path, bool reversed = false);

		bool load_effect(const std::filesystem::path &source_file, const ini_file &preset, size_t effect_index, bool preprocess_required = false);
		bool create_effect(size_t effect_index);
		bool create_effect_sampler_state(const api::sampler_desc &desc, api::sampler &sampler);
		void destroy_effect(size_t effect_index);

		bool create_texture(texture &texture);
		void destroy_texture(texture &texture);

		void enable_technique(technique &technique);
		void disable_technique(technique &technique);

		void load_effects();
		void load_textures();
		bool reload_effect(size_t effect_index, bool preprocess_required = false);
		void reload_effects();
		void destroy_effects();

		bool load_effect_cache(const std::string &id, const std::string &type, std::string &data) const;
		bool save_effect_cache(const std::string &id, const std::string &type, const std::string &data) const;
		void clear_effect_cache();

		void update_effects();
		void render_technique(api::command_list *cmd_list, technique &technique, api::resource_view rtv, api::resource_view rtv_srgb);

		void save_texture(const texture &texture);
		
		bool execute_screenshot_post_save_command(const std::filesystem::path &screenshot_path);

		void get_uniform_value(const uniform &variable, uint8_t *data, size_t size, size_t base_index) const;
		template <typename T>
		std::enable_if_t<std::is_same_v<T, bool> || std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float>>
		get_uniform_value(const uniform &variable, T *values, size_t count = 1, size_t array_index = 0) const
		{
			get_uniform_value({ reinterpret_cast<uintptr_t>(&variable) }, values, count, array_index);
		}

		void set_uniform_value(uniform &variable, const uint8_t *data, size_t size, size_t base_index);
		template <typename T>
		std::enable_if_t<std::is_same_v<T, bool> || std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float>>
		set_uniform_value(uniform &variable, T x, T y = T(0), T z = T(0), T w = T(0))
		{
			const T values[4] = { x, y, z, w };
			set_uniform_value({ reinterpret_cast<uintptr_t>(&variable) }, values, 4, 0);
		}
		template <typename T>
		std::enable_if_t<std::is_same_v<T, bool> || std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float>>
		set_uniform_value(uniform &variable, const T *values, size_t count = 1, size_t array_index = 0)
		{
			set_uniform_value({ reinterpret_cast<uintptr_t>(&variable) }, values, count, array_index);
		}

		void reset_uniform_value(uniform &variable);
#endif

		bool get_texture_data(api::resource resource, api::resource_usage state, uint8_t *pixels);

		#pragma region Status
		bool _needs_update = false;
		unsigned long _latest_version[3] = {};
		std::filesystem::path _config_path;

		bool _is_initialized = false;
		bool _preset_save_success = true;
		bool _effects_enabled = true;
		bool _effects_rendered_this_frame = false;

		bool _ignore_shortcuts = false;
		bool _force_shortcut_modifiers = true;
		unsigned int _effects_key_data[4] = {};
		std::shared_ptr<class input> _input;

		std::chrono::high_resolution_clock::duration _last_frame_duration;
		std::chrono::high_resolution_clock::time_point _start_time;
		std::chrono::high_resolution_clock::time_point _last_present_time;
		unsigned long long _framecount = 0;
		#pragma endregion

		#pragma region Effect Loading
#if RESHADE_EFFECTS
		bool _no_debug_info = 0;
		bool _no_effect_cache = false;
		bool _no_reload_on_init = false;
		bool _no_reload_for_non_vr = false;
		bool _performance_mode = false;
		bool _effect_load_skipping = false;
		bool _load_option_disable_skipping = false;
		unsigned int _reload_key_data[4] = {};
		unsigned int _performance_mode_key_data[4] = {};
		std::vector<std::string> _global_preprocessor_definitions;
		std::vector<std::string> _preset_preprocessor_definitions;
		std::filesystem::path _intermediate_cache_path;
		std::vector<std::filesystem::path> _effect_search_paths;
		std::vector<std::filesystem::path> _texture_search_paths;

		std::atomic<bool> _last_reload_successfull = true;
		bool _textures_loaded = false;
		bool _last_texture_reload_successfull = true;
		std::vector<size_t> _reload_create_queue;
		std::atomic<size_t> _reload_remaining_effects = 0;
		std::mutex _reload_mutex;
		std::vector<std::thread> _worker_threads;

		std::vector<effect> _effects;
		std::vector<texture> _textures;
		std::vector<technique> _techniques;
#endif
		std::chrono::high_resolution_clock::time_point _last_reload_time;
		#pragma endregion

		#pragma region Effect Rendering
#if RESHADE_EFFECTS
		api::resource _empty_tex = {};
		api::resource_view _empty_srv = {};
		api::resource _effect_color_tex = {};
		api::resource_view _effect_color_srv[2] = {};
		api::format _effect_stencil_format = api::format::unknown;
		api::resource _effect_stencil_tex = {};
		api::resource_view _effect_stencil_dsv = {};

		std::unordered_map<size_t, api::sampler> _effect_sampler_states;
		std::unordered_map<std::string, std::pair<api::resource_view, api::resource_view>> _texture_semantic_bindings;
		std::unordered_map<std::string, std::pair<api::resource_view, api::resource_view>> _backup_texture_semantic_bindings;
#endif
		std::vector<api::resource_view> _back_buffer_targets;
		#pragma endregion

		#pragma region Screenshot
		bool _screenshot_save_gui = false;
		bool _screenshot_save_before = false;
		bool _screenshot_include_preset = false;
		bool _screenshot_clear_alpha = true;
		unsigned int _screenshot_format = 1;
		unsigned int _screenshot_naming = 0;
		unsigned int _screenshot_jpeg_quality = 90;
		unsigned int _screenshot_key_data[4] = {};
		std::filesystem::path _screenshot_path;
		std::filesystem::path _screenshot_post_save_command;
		std::string _screenshot_post_save_command_arguments;
		std::filesystem::path _screenshot_post_save_command_working_directory;
		bool _screenshot_post_save_command_show_window;

		bool _should_save_screenshot = false;
		bool _screenshot_save_success = true;
		std::filesystem::path _last_screenshot_file;
		std::chrono::high_resolution_clock::time_point _last_screenshot_time;
		#pragma endregion

		#pragma region Preset Switching
#if RESHADE_EFFECTS
		unsigned int _prev_preset_key_data[4] = {};
		unsigned int _next_preset_key_data[4] = {};
		unsigned int _preset_transition_delay = 1000;
		std::filesystem::path _current_preset_path;

		bool _is_in_between_presets_transition = false;
		std::chrono::high_resolution_clock::time_point _last_preset_switching_time;
#endif
		#pragma endregion

#if RESHADE_GUI
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

#if RESHADE_EFFECTS
		void draw_gui_home();
#endif
		void draw_gui_settings();
		void draw_gui_statistics();
		void draw_gui_log();
		void draw_gui_about();
#if RESHADE_ADDON
		void draw_gui_addons();
#endif
#if RESHADE_EFFECTS
		void draw_variable_editor();
		void draw_technique_editor();
#endif

		bool init_imgui_resources();
		void render_imgui_draw_data(api::command_list *cmd_list, ImDrawData *draw_data, api::resource_view rtv);
		void destroy_imgui_resources();

		#pragma region Overlay
		ImGuiContext *_imgui_context = nullptr;

		bool _show_splash = true;
		bool _show_overlay = false;
		bool _show_fps = false;
		bool _show_clock = false;
		bool _show_frametime = false;
		bool _show_screenshot_message = true;
		bool _rebuild_font_atlas = true;
		unsigned int _reload_count = 0;
		unsigned int _window_width = 0;
		unsigned int _window_height = 0;

		bool _no_font_scaling = false;
		bool _save_imgui_window_state = false;
		unsigned int _overlay_key_data[4];
		unsigned int _fps_pos = 1;
		unsigned int _clock_format = 0;
		unsigned int _input_processing_mode = 2;

		api::resource _font_atlas_tex = {};
		api::resource_view _font_atlas_srv = {};

		api::pipeline _imgui_pipeline = {};
		api::pipeline_layout _imgui_pipeline_layout = {};
		api::sampler  _imgui_sampler_state = {};

		int _imgui_num_indices[4] = {};
		api::resource _imgui_indices[4] = {};
		int _imgui_num_vertices[4] = {};
		api::resource _imgui_vertices[4] = {};

		api::resource _vr_overlay_tex = {};
		api::resource_view _vr_overlay_target = {};
		#pragma endregion

		#pragma region Overlay Home
#if RESHADE_EFFECTS
		char _effect_filter[32] = {};
		bool _variable_editor_tabs = false;
		bool _duplicate_current_preset = false;
		bool _was_preprocessor_popup_edited = false;
		size_t _focused_effect = std::numeric_limits<size_t>::max();
		size_t _selected_technique = std::numeric_limits<size_t>::max();
		unsigned int _tutorial_index = 0;
		unsigned int _effects_expanded_state = 2;
		float _variable_editor_height = 300.0f;
		size_t _variable_editor_modified = std::numeric_limits<size_t>::max();
		unsigned int _variable_editor_edited = 0;

		enum class variable_editor_condition { pass, variable, preset_definition, global_definition };
		variable_editor_condition _variable_editor_condition = variable_editor_condition::pass;
#endif
		#pragma endregion

		#pragma region Overlay Add-ons
		char _addons_filter[32] = {};
		#pragma endregion

		#pragma region Overlay Settings
		int _font_size = 13;
		int _editor_font_size = 13;
		int _style_index = 2;
		int _editor_style_index = 0;
		std::filesystem::path _font;
		std::filesystem::path _editor_font;
		std::filesystem::path _file_selection_path;
		float _fps_col[4] = { 1.0f, 1.0f, 0.784314f, 1.0f };
		float _fps_scale = 1.0f;
		bool  _show_force_load_effects_button = true;
		#pragma endregion

		#pragma region Overlay Statistics
		bool _gather_gpu_statistics = false;
		api::resource_view _preview_texture = { 0 };
		unsigned int _preview_size[3] = { 0, 0, 0xFFFFFFFF };
		#pragma endregion

		#pragma region Overlay Log
		char _log_filter[64] = {};
		bool _log_wordwrap = false;
		uintmax_t _last_log_size;
		std::vector<std::string> _log_lines;
		#pragma endregion

		#pragma region Overlay Code Editor
#if RESHADE_EFFECTS
		struct editor_instance
		{
			size_t effect_index;
			std::filesystem::path file_path;
			std::string entry_point_name;
			imgui::code_editor editor;
			bool selected = false;
		};

		void open_code_editor(size_t effect_index, const std::string &entry_point);
		void open_code_editor(size_t effect_index, const std::filesystem::path &path);
		void open_code_editor(editor_instance &instance);
		void draw_code_editor(editor_instance &instance);

		std::vector<editor_instance> _editors;
#endif
		uint32_t _editor_palette[imgui::code_editor::color_palette_max];
		#pragma endregion

		#pragma region Variable Tweaks History
#if RESHADE_EFFECTS
		bool _show_edit_history = false;
		std::vector<std::string> _technique_names;

		size_t _history_limit = 100;
		std::list<history> _histories;
		bool _history_updated = false;
		size_t _history_position = 0;

		void reshade::runtime::draw_edit_history();
#endif
		#pragma endregion
#endif
	};
}
