/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"
#include "state_tracking.hpp"
#include <unordered_set>

namespace reshade::opengl
{
	class runtime_gl : public runtime
	{
	public:
		runtime_gl();
		~runtime_gl();

		bool on_init(HWND hwnd, unsigned int width, unsigned int height);
		void on_reset();
		void on_present();

		bool capture_screenshot(uint8_t *buffer) const override;

		state_tracking _state_tracking;
		bool _compatibility_context = false;
		std::unordered_set<HDC> _hdcs;

	private:
		bool init_effect(size_t index) override;
		void unload_effect(size_t index) override;
		void unload_effects() override;

		bool init_texture(texture &texture) override;
		void upload_texture(const texture &texture, const uint8_t *data) override;
		void destroy_texture(texture &texture) override;
		void generate_mipmaps(const struct tex_data *impl);

		void render_technique(technique &technique) override;

		enum BUF
		{
#if RESHADE_GUI
			VBO_IMGUI,
			IBO_IMGUI,
#else
			BUF_DUMMY,
#endif
				NUM_BUF
		};
		enum TEX
		{
			TEX_BACK,
			TEX_BACK_SRGB,
			TEX_DEPTH,
				NUM_TEX
		};
		enum VAO
		{
			VAO_FX,
#if RESHADE_GUI
			VAO_IMGUI,
#endif
				NUM_VAO
		};
		enum FBO
		{
			FBO_BACK,
			FBO_BLIT,
			FBO_CLEAR,
			FBO_DEPTH_SRC,
			FBO_DEPTH_DEST,
				NUM_FBO
		};
		enum RBO
		{
			RBO_COLOR,
			RBO_STENCIL,
				NUM_RBO
		};

		state_block _app_state;

		GLuint _buf[NUM_BUF] = {};
		GLuint _tex[NUM_TEX] = {};
		GLuint _vao[NUM_VAO] = {};
		GLuint _fbo[NUM_FBO] = {}, _current_fbo = 0;
		GLuint _rbo[NUM_RBO] = {};
		GLuint _mipmap_program = 0;
		GLenum _default_depth_format = GL_NONE;
		std::vector<GLuint> _effect_ubos;
		std::vector<GLuint> _reserved_texture_names;
		std::unordered_map<size_t, GLuint> _effect_sampler_states;

#if RESHADE_GUI
		void init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;

		struct imgui_resources
		{
			GLuint program = 0;
		} _imgui;
#endif

#if RESHADE_DEPTH
		void draw_depth_debug_menu();
		void update_depth_texture_bindings(state_tracking::depthstencil_info info);

		bool _copy_depth_source = true;
		GLuint _depth_source = 0;
		GLuint _depth_source_width = 0, _depth_source_height = 0;
		GLenum _depth_source_format = 0;
		GLuint _depth_source_override = std::numeric_limits<GLuint>::max();
#endif
	};
}
