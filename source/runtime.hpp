#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>

#pragma region Forward Declarations
struct ImDrawData;

namespace reshade
{
	class input;
	struct texture;
	struct uniform;
	struct technique;

	namespace filesystem
	{
		class path;
	}
}
namespace reshadefx
{
	class syntax_tree;
}

extern volatile long g_network_traffic;
#pragma endregion

namespace reshade
{
	class runtime abstract
	{
	public:
		/// <summary>
		/// Initialize ReShade. Registers hooks and starts logging.
		/// </summary>
		/// <param name="exe_path">Path to the executable ReShade was injected into.</param>
		/// <param name="injector_path">Path to the ReShade module.</param>
		static void startup(const filesystem::path &exe_path, const filesystem::path &injector_path);
		/// <summary>
		/// Shut down ReShade. Removes all installed hooks and cleans up.
		/// </summary>
		static void shutdown();

		/// <summary>
		/// Construct a new runtime instance.
		/// </summary>
		/// <param name="renderer">A unique number identifying the renderer implementation.</param>
		explicit runtime(uint32_t renderer);
		virtual ~runtime();

		/// <summary>
		/// Create a copy of the current frame.
		/// </summary>
		/// <param name="buffer">The buffer to save the copy to. It has to be the size of at least "frame_width() * frame_height() * 4".</param>
		virtual void screenshot(uint8_t *buffer) const = 0;
		/// <summary>
		/// Returns the frame width in pixels.
		/// </summary>
		unsigned int frame_width() const { return _width; }
		/// <summary>
		/// Returns the frame height in pixels.
		/// </summary>
		unsigned int frame_height() const { return _height; }

		/// <summary>
		/// Add a new texture. This transfers ownership of the pointer to this class.
		/// </summary>
		/// <param name="texture">The texture to add.</param>
		void add_texture(texture *texture);
		/// <summary>
		/// Add a new uniform.
		/// </summary>
		/// <param name="uniform">The uniform to add.</param>
		void add_uniform(uniform &&uniform);
		/// <summary>
		/// Add a new technique.
		/// </summary>
		/// <param name="technique">The technique to add.</param>
		void add_technique(technique &&technique);
		/// <summary>
		/// Find the texture with the specified name.
		/// </summary>
		/// <param name="name">The name of the texture.</param>
		texture *find_texture(const std::string &name);

		/// <summary>
		/// Compile effect from the specified abstract syntax tree and initialize textures, constants and techniques.
		/// </summary>
		/// <param name="ast">The abstract syntax tree of the effect to compile.</param>
		/// <param name="pragmas">A list of additional commands to the compiler.</param>
		/// <param name="errors">A reference to a buffer to store errors which occur during compilation.</param>
		virtual bool update_effect(const reshadefx::syntax_tree &ast, std::string &errors) = 0;
		/// <summary>
		/// Update the image data of a texture.
		/// </summary>
		/// <param name="texture">The texture to update.</param>
		/// <param name="data">The RGBA image data to update the texture to.</param>
		virtual bool update_texture(texture &texture, const uint8_t *data) = 0;
		/// <summary>
		/// Return a reference to the uniform storage buffer.
		/// </summary>
		inline std::vector<unsigned char> &get_uniform_value_storage() { return _uniform_data_storage; }
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

	protected:
		/// <summary>
		/// Callback function called when the runtime is initialized.
		/// </summary>
		/// <returns>Returns if the initialization succeeded.</returns>
		virtual bool on_init();
		/// <summary>
		/// Callback function called when the runtime is uninitialized.
		/// </summary>
		virtual void on_reset();
		/// <summary>
		/// Callback function called when the post-processing effects are uninitialized.
		/// </summary>
		virtual void on_reset_effect();
		/// <summary>
		/// Callback function called at the end of each frame.
		/// </summary>
		virtual void on_present();
		/// <summary>
		/// Callback function called at every draw call.
		/// </summary>
		/// <param name="vertices">The number of vertices this draw call generates.</param>
		virtual void on_draw_call(unsigned int vertices);
		/// <summary>
		/// Callback function called to apply the post-processing effects to the screen.
		/// </summary>
		virtual void on_apply_effect();
		/// <summary>
		/// Callback function called to render the specified effect technique.
		/// </summary>
		/// <param name="technique">The technique to render.</param>
		virtual void on_apply_effect_technique(const technique &technique);

		/// <summary>
		/// Render ImGui draw lists.
		/// </summary>
		/// <param name="data">The draw data to render.</param>
		virtual void render_draw_lists(ImDrawData *draw_data) = 0;

		bool _is_initialized = false;
		unsigned int _width = 0, _height = 0;
		unsigned int _vendor_id = 0, _device_id = 0;
		uint64_t _framecount = 0;
		unsigned int _drawcalls = 0, _vertices = 0;
		std::shared_ptr<input> _input;
		std::unique_ptr<texture> _imgui_font_atlas;
		std::vector<std::unique_ptr<texture>> _textures;
		std::vector<uniform> _uniforms;
		std::vector<technique> _techniques;

	private:
		struct key_shortcut { int keycode; bool ctrl, shift; };

		void reload();
		void screenshot();
		bool load_effect(const filesystem::path &path, reshadefx::syntax_tree &ast);
		void load_textures();
		void load_configuration();
		void save_configuration() const;
		void load_preset(const filesystem::path &path);
		void save_preset(const filesystem::path &path) const;

		void draw_overlay();
		void draw_overlay_menu();
		void draw_overlay_menu_home();
		void draw_overlay_menu_settings();
		void draw_overlay_menu_statistics();
		void draw_overlay_menu_about();
		void draw_overlay_shader_editor();
		void draw_overlay_variable_editor();

		const unsigned int _renderer_id;
		std::vector<std::string> _effect_files, _preset_files, _effect_search_paths, _texture_search_paths;
		std::vector<filesystem::path> _included_files;
		std::chrono::high_resolution_clock::time_point _start_time, _last_create, _last_present;
		std::chrono::high_resolution_clock::duration _last_frame_duration;
		std::vector<unsigned char> _uniform_data_storage;
		int _date[4] = { };
		std::string _errors, _message, _effect_source;
		int _menu_index = 0, _screenshot_format = 0, _current_preset = -1, _current_effect_file = -1;
		key_shortcut _menu_key = { }, _screenshot_key = { };
		std::string _screenshot_path;
		bool _developer_mode = false, _show_menu = false, _show_shader_editor = false, _show_variable_editor = false;
		int _selected_technique = -1, _hovered_technique = -1;
		std::vector<char> _shader_edit_buffer;
	};
}
