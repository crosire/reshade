#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <boost/chrono.hpp>
#include <boost/filesystem/path.hpp>
#include "moving_average.hpp"
#include "critical_section.hpp"

#pragma region Forward Declarations
struct ImDrawData;

namespace reshade
{
	class input;
	struct texture;
	struct uniform;
	struct technique;

	namespace fx
	{
		class nodetree;
	}
}
#pragma endregion

namespace reshade
{
	class runtime abstract
	{
	public:
		/// <summary>
		/// Initialize ReShade. Registers hooks and starts logging.
		/// </summary>
		/// <param name="executablePath">Path to the executable ReShade was injected into.</param>
		/// <param name="injectorPath">Path to the ReShade module itself.</param>
		static void startup(const boost::filesystem::path &exe_path, const boost::filesystem::path &injector_path);
		/// <summary>
		/// Shut down ReShade. Removes all installed hooks and cleans up.
		/// </summary>
		static void shutdown();

		explicit runtime(unsigned int renderer);
		virtual ~runtime();

		/// <summary>
		/// Return the current input manager.
		/// </summary>
		const input *input() const { return _input.get(); }
		/// <summary>
		/// Return the back buffer width.
		/// </summary>
		unsigned int frame_width() const { return _width; }
		/// <summary>
		/// Return the back buffer height.
		/// </summary>
		unsigned int frame_height() const { return _height; }

		/// <summary>
		/// Create a copy of the current image on the screen.
		/// </summary>
		/// <param name="buffer">The buffer to save the copy to. It has to be the size of at least "WIDTH * HEIGHT * 4".</param>
		virtual void screenshot(unsigned char *buffer) const = 0;

		/// <summary>
		/// Add a new texture. This transfers ownership of the pointer to this class.
		/// </summary>
		/// <param name="texture">The texture to add.</param>
		void add_texture(texture *texture);
		/// <summary>
		/// Add a new uniform. This transfers ownership of the pointer to this class.
		/// </summary>
		/// <param name="uniform">The uniform to add.</param>
		void add_uniform(uniform *uniform);
		/// <summary>
		/// Add a new technique. This transfers ownership of the pointer to this class.
		/// </summary>
		/// <param name="technique">The technique to add.</param>
		void add_technique(technique *technique);
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
		/// <param name="errors">A reference to a buffer to store errors in which occur during compilation.</param>
		virtual bool update_effect(const fx::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors) = 0;
		/// <summary>
		/// Update the image data of a texture.
		/// </summary>
		/// <param name="texture">The texture to update.</param>
		/// <param name="data">The image data to update the texture to.</param>
		/// <param name="size">The size of the image in <paramref name="data"/>.</param>
		virtual bool update_texture(texture *texture, const unsigned char *data, size_t size) = 0;
		/// <summary>
		/// Return a reference to the uniform storage buffer.
		/// </summary>
		inline std::vector<unsigned char> &get_uniform_value_storage() { return _uniform_data_storage; }
		/// <summary>
		/// Get the value of a uniform variable.
		/// </summary>
		/// <param name="variable">The variable to retrieve the value from.</param>
		/// <param name="data">The buffer to store the value in.</param>
		/// <param name="size">The size of the buffer in <paramref name="data"/>.</param>
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
		/// <param name="size">The size of the value in <paramref name="data"/>.</param>
		virtual void set_uniform_value(uniform &variable, const unsigned char *data, size_t size);
		void set_uniform_value(uniform &variable, const bool *values, size_t count);
		void set_uniform_value(uniform &variable, const int *values, size_t count);
		void set_uniform_value(uniform &variable, const unsigned int *values, size_t count);
		void set_uniform_value(uniform &variable, const float *values, size_t count);

		static std::atomic<unsigned int> s_network_traffic;

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

		bool _is_initialized, _is_effect_compiled;
		unsigned int _width, _height;
		unsigned int _vendor_id, _device_id;
		uint64_t _framecount;
		unsigned int _drawcalls, _vertices;
		std::shared_ptr<class input> _input;
		std::unique_ptr<texture> _imgui_font_atlas;
		std::vector<std::unique_ptr<texture>> _textures;
		std::vector<std::unique_ptr<uniform>> _uniforms;
		std::vector<std::unique_ptr<technique>> _techniques;

	private:
		void draw_overlay(float dt);

		bool load_effect();
		bool compile_effect();
		void process_effect();

		const unsigned int _renderer_id;
		std::vector<std::string> _pragmas;
		std::vector<boost::filesystem::path> _included_files;
		boost::chrono::high_resolution_clock::time_point _start_time, _last_create, _last_present;
		boost::chrono::high_resolution_clock::duration _last_frame_duration, _last_postprocessing_duration;
		utils::moving_average<uint64_t, 128> _average_frametime;
		std::vector<unsigned char> _uniform_data_storage;
		float _date[4];
		unsigned int _compile_step, _compile_count;
		std::string _status, _errors, _message, _effect_source;
		std::string _screenshot_format;
		boost::filesystem::path _screenshot_path;
		unsigned int _screenshot_key;
		bool _show_statistics, _show_fps, _show_clock, _show_toggle_message, _show_info_messages;
		utils::critical_section _imgui_cs;
	};
}
