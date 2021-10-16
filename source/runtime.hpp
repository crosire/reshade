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
		/// Gets a boolean indicating whether effects are being loaded.
		/// </summary>
		bool is_loading() const { return _reload_remaining_effects != std::numeric_limits<size_t>::max(); }
		/// <summary>
		/// Gets a boolean indicating whether the runtime is initialized.
		/// </summary>
		bool is_initialized() const { return _is_initialized; }

		virtual api::resource get_back_buffer_resolved(uint32_t index) = 0;

		/// <summary>
		/// Applies post-processing effects to the specified render targets.
		/// </summary>
		virtual void render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb) override;

		/// <summary>
		/// Captures a screenshot of the current back buffer resource and writes it to an image file on disk.
		/// </summary>
		void save_screenshot(const std::wstring &postfix = std::wstring(), bool should_save_preset = false);
		/// <summary>
		/// Captures a screenshot of the current back buffer resource and returns its image data in 32 bits-per-pixel RGBA format.
		/// </summary>
		bool capture_screenshot(uint8_t *pixels) final { return get_texture_data(get_back_buffer_resolved(get_current_back_buffer_index()), api::resource_usage::present, pixels); }

		/// <summary>
		/// Gets the current buffer dimensions of the swap chain as used with effect rendering.
		/// </summary>
		void get_screenshot_width_and_height(uint32_t *width, uint32_t *height) const final { *width = _width; *height = _height; }

		void enumerate_uniform_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, api::effect_uniform_variable variable, void *user_data), void *user_data) final;

		api::effect_uniform_variable get_uniform_variable(const char *effect_name, const char *variable_name) const final;

		void get_uniform_binding(api::effect_uniform_variable variable, api::resource *out_buffer, uint64_t *out_offset) const final;

		void get_uniform_annotation(api::effect_uniform_variable variable, const char *name, bool *values, size_t count, size_t array_index = 0) const final;
		void get_uniform_annotation(api::effect_uniform_variable variable, const char *name, float *values, size_t count, size_t array_index = 0) const final;
		void get_uniform_annotation(api::effect_uniform_variable variable, const char *name, int32_t *values, size_t count, size_t array_index = 0) const final;
		void get_uniform_annotation(api::effect_uniform_variable variable, const char *name, uint32_t *values, size_t count, size_t array_index = 0) const final;
		const char *get_uniform_name(api::effect_uniform_variable variable) const final;
		const char *get_uniform_annotation(api::effect_uniform_variable variable, const char *name) const final;

		void get_uniform_data(const uniform &variable, uint8_t *data, size_t size, size_t base_index) const;
		void get_uniform_data(api::effect_uniform_variable variable, bool *values, size_t count, size_t array_index) const final;
		void get_uniform_data(api::effect_uniform_variable variable, float *values, size_t count, size_t array_index) const final;
		void get_uniform_data(api::effect_uniform_variable variable, int32_t *values, size_t count, size_t array_index) const final;
		void get_uniform_data(api::effect_uniform_variable variable, uint32_t *values, size_t count, size_t array_index) const final;
		template <typename T>
		std::enable_if_t<std::is_same_v<T, bool> || std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float>>
		get_uniform_value(const uniform &variable, T *values, size_t count = 1, size_t array_index = 0) const
		{
			get_uniform_data({ reinterpret_cast<uintptr_t>(&variable) }, values, count, array_index);
		}

		void set_uniform_data(uniform &variable, const uint8_t *data, size_t size, size_t base_index);
		void set_uniform_data(api::effect_uniform_variable variable, const bool *values, size_t count, size_t array_index) final;
		void set_uniform_data(api::effect_uniform_variable variable, const float *values, size_t count, size_t array_index) final;
		void set_uniform_data(api::effect_uniform_variable variable, const int32_t *values, size_t count, size_t array_index) final;
		void set_uniform_data(api::effect_uniform_variable variable, const uint32_t *values, size_t count, size_t array_index) final;
		template <typename T>
		std::enable_if_t<std::is_same_v<T, bool> || std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float>>
		set_uniform_value(uniform &variable, T x, T y = T(0), T z = T(0), T w = T(0))
		{
			const T values[4] = { x, y, z, w };
			set_uniform_data({ reinterpret_cast<uintptr_t>(&variable) }, values, 4, 0);
		}
		template <typename T>
		std::enable_if_t<std::is_same_v<T, bool> || std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float>>
		set_uniform_value(uniform &variable, const T *values, size_t count = 1, size_t array_index = 0)
		{
			set_uniform_data({ reinterpret_cast<uintptr_t>(&variable) }, values, count, array_index);
		}

		void enumerate_texture_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, api::effect_texture_variable variable, void *user_data), void *user_data) final;

		api::effect_texture_variable get_texture_variable(const char *effect_name, const char *variable_name) const final;

		void get_texture_binding(api::effect_texture_variable variable, api::resource_view *out_srv, api::resource_view *out_srv_srgb) const final;

		void get_texture_annotation(api::effect_texture_variable variable, const char *name, bool *values, size_t count, size_t array_index = 0) const final;
		void get_texture_annotation(api::effect_texture_variable variable, const char *name, float *values, size_t count, size_t array_index = 0) const final;
		void get_texture_annotation(api::effect_texture_variable variable, const char *name, int32_t *values, size_t count, size_t array_index = 0) const final;
		void get_texture_annotation(api::effect_texture_variable variable, const char *name, uint32_t *values, size_t count, size_t array_index = 0) const final;
		const char *get_texture_name(api::effect_texture_variable variable) const final;
		const char *get_texture_annotation(api::effect_texture_variable variable, const char *name) const final;

		/// <summary>
		/// Gets the image data of the specified <paramref name="resource"/> in 32 bits-per-pixel RGBA format.
		/// </summary>
		/// <param name="resource">Texture resource to get the image data from.</param>
		/// <param name="state">Current state the <paramref name="resource"/> is in.</param>
		/// <param name="pixels">Pointer to an array of <c>width * height * 4</c> bytes the image data is written to.</param>
		bool get_texture_data(api::resource resource, api::resource_usage state, uint8_t *pixels);
		void get_texture_data(api::effect_texture_variable variable, uint32_t *out_width, uint32_t *out_height, uint8_t *pixels) final;
		void set_texture_data(api::effect_texture_variable variable, const uint32_t width, const uint32_t height, const uint8_t *pixels) final;

		void update_texture_bindings(const char *semantic, api::resource_view srv, api::resource_view srv_srgb) final;

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
		api::format  _backbuffer_format = api::format::unknown;
		bool _is_vr = false;

	private:
		static bool check_for_update(unsigned long latest_version[3]);

		void load_config();
		void save_config() const;

		void load_current_preset();
		void save_current_preset() const;

		bool switch_to_next_preset(std::filesystem::path filter_path, bool reversed = false);

		bool load_effect(const std::filesystem::path &source_file, const ini_file &preset, size_t effect_index, bool preprocess_required = false);
		bool create_effect(size_t effect_index);
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
		void render_technique(api::command_list *cmd_list, technique &technique, api::resource backbuffer);

		void save_texture(const texture &texture);

		void reset_uniform_value(uniform &variable);

		// === Status ===

		bool _is_initialized = false;
		bool _effects_enabled = true;
		bool _effects_rendered_this_frame = false;
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
		std::vector<size_t> _reload_create_queue;
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

		std::vector<api::framebuffer> _backbuffer_fbos, _effect_backbuffer_fbos;
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
		std::unordered_map<std::string, std::pair<api::resource_view, api::resource_view>> _texture_semantic_bindings;
		std::unordered_map<std::string, std::pair<api::resource_view, api::resource_view>> _backup_texture_semantic_bindings;

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
		void render_imgui_draw_data(api::command_list *cmd_list, ImDrawData *draw_data, api::render_pass pass, api::framebuffer fbo);
		void destroy_imgui_resources();

		ImGuiContext *_imgui_context = nullptr;

		api::resource _font_atlas = {};
		api::resource_view _font_atlas_srv = {};
		api::sampler _imgui_sampler_state = {};
		api::pipeline _imgui_pipeline = {};
		api::pipeline_layout _imgui_pipeline_layout = {};
		api::descriptor_set_layout _imgui_set_layouts[2] = {};
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
		std::vector<std::pair<std::string, void(runtime::*)()>> _menu_callables;
		bool _show_splash = true;
		bool _show_overlay = false;
		bool _show_fps = false;
		bool _show_clock = false;
		bool _show_frametime = false;
		bool _show_screenshot_message = true;
		bool _no_font_scaling = false;
		bool _rebuild_font_atlas = true;
		bool _gather_gpu_statistics = false;
		bool _save_imgui_window_state = false;
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
		std::string _open_addon_name;

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
			imgui::code_editor editor;
			bool selected = false;
		};

		void open_code_editor(size_t effect_index, const std::string &entry_point);
		void open_code_editor(size_t effect_index, const std::filesystem::path &path);
		void open_code_editor(editor_instance &instance);
		void draw_code_editor(editor_instance &instance);

		std::vector<editor_instance> _editors;
		uint32_t _editor_palette[imgui::code_editor::color_palette_max];
#endif
	};
}
