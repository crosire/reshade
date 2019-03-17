/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"
#include <unordered_set>

namespace reshadefx { struct sampler_info; }

namespace reshade::opengl
{
	class runtime_opengl : public runtime
	{
	public:
		runtime_opengl();

		bool on_init(HWND hwnd, unsigned int width, unsigned int height);
		void on_reset();
		void on_present();

		void on_draw_call(unsigned int vertices);
		void on_fbo_attachment(GLenum target, GLenum attachment, GLenum objecttarget, GLuint object, GLint level);

		void capture_screenshot(uint8_t *buffer) const override;

		std::unordered_set<HDC> _hdcs;

		GLuint _current_vertex_count = 0;
		GLuint _default_backbuffer_fbo = 0, _default_backbuffer_rbo[2] = { }, _backbuffer_texture[2] = { };
		GLuint _depth_source_fbo = 0, _depth_source = 0, _depth_texture = 0, _blit_fbo = 0;
		GLuint _default_vao = 0;
		std::vector<std::pair<GLuint, GLsizeiptr>> _effect_ubos;
		std::unordered_map<size_t, GLuint> _effect_sampler_states;

	private:
		struct depth_source_info
		{
			unsigned int width, height;
			GLint level, format;
			unsigned int drawcall_count, vertices_count;
		};

		bool init_backbuffer_texture();
		bool init_default_depth_stencil();

		bool init_texture(texture &info) override;
		void update_texture(texture &texture, const uint8_t *data) override;
		bool update_texture_reference(texture &texture);

		bool compile_effect(effect_data &effect) override;
		void unload_effects() override;

		bool add_sampler(const reshadefx::sampler_info &info, struct opengl_technique_data &technique_init);
		bool init_technique(technique &info, struct opengl_technique_data &&technique_init, const std::unordered_map<std::string, GLuint> &entry_points, std::string &errors);

		void render_technique(const technique &technique) override;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;
#endif

		void detect_depth_source();
		void create_depth_texture(GLuint width, GLuint height, GLenum format);

		state_block _app_state;
		std::unordered_map<GLuint, depth_source_info> _depth_source_table;

		GLuint _imgui_shader_program = 0, _imgui_VertHandle = 0, _imgui_FragHandle = 0;
		int _imgui_attribloc_tex = 0, _imgui_attribloc_projmtx = 0;
		int _imgui_attribloc_pos = 0, _imgui_attribloc_uv = 0, _imgui_attribloc_color = 0;
		GLuint _imgui_vbo[2] = { }, _imgui_vao = 0;
	};
}
