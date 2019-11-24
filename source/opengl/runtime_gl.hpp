/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"
#include "buffer_detection.hpp"
#include <unordered_set>

namespace reshade { enum class texture_reference; }
namespace reshadefx { struct sampler_info; }

namespace reshade::opengl
{
	class runtime_gl : public runtime
	{
	public:
		runtime_gl();

		bool on_init(HWND hwnd, unsigned int width, unsigned int height);
		void on_reset();
		void on_present();

		bool capture_screenshot(uint8_t *buffer) const override;

		std::unordered_set<HDC> _hdcs;
		buffer_detection _buffer_detection;

	private:
		bool init_effect(size_t index) override;
		void unload_effects() override;

		bool init_texture(texture &texture) override;
		void upload_texture(texture &texture, const uint8_t *data) override;

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
			FBO_DEPTH,
			FBO_BLIT,
				NUM_FBO
		};
		enum RBO
		{
			RBO_COLOR,
			RBO_DEPTH,
				NUM_RBO
		};

		state_block _app_state;

		GLuint _buf[NUM_BUF] = {};
		GLuint _tex[NUM_TEX] = {};
		GLuint _vao[NUM_VAO] = {};
		GLuint _fbo[NUM_FBO] = {};
		GLuint _rbo[NUM_RBO] = {};
		std::vector<GLuint> _reserved_texture_names;
		std::unordered_map<size_t, GLuint> _effect_sampler_states;
		std::vector<std::pair<GLuint, GLsizeiptr>> _effect_ubos;

#if RESHADE_GUI
		void init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;

		GLuint _imgui_program = 0;
		int _imgui_uniform_tex = 0;
		int _imgui_uniform_proj = 0;
#endif

#if RESHADE_OPENGL_CAPTURE_DEPTH_BUFFERS
		void draw_depth_debug_menu();
		void update_depthstencil_texture(GLuint source, GLuint width, GLuint height, GLuint level, GLenum format);

		bool _use_aspect_ratio_heuristics = true;
		GLuint _depth_source = 0;
		GLuint _depth_source_width = 0;
		GLuint _depth_source_height = 0;
		GLenum _depth_source_format = 0;
		GLuint _depth_source_override = std::numeric_limits<GLuint>::max();
		GLenum _default_depth_format = GL_NONE;
#endif
	};
}
