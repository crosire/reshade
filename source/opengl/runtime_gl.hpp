/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "render_gl.hpp"
#include "state_block_gl.hpp"

namespace reshade::opengl
{
	class runtime_gl : public runtime, public device_impl
	{
	public:
		runtime_gl(HDC hdc, HGLRC hglrc);
		~runtime_gl();

		bool get_data(const uint8_t guid[16], void **ptr) const override { return device_impl::get_data(guid, ptr); }
		void set_data(const uint8_t guid[16], void *const ptr)  override { device_impl::set_data(guid, ptr); }

		uint64_t get_native_object() const override { return reinterpret_cast<uintptr_t>(*_hdcs.begin()); } // Simply return the first device context

		api::device *get_device() override { return this; }
		api::command_queue *get_command_queue() override { return this; }

		bool on_init(HWND hwnd, unsigned int width, unsigned int height);
		void on_reset();
		void on_present();

		bool capture_screenshot(uint8_t *buffer) const override;

		void update_texture_bindings(const char *semantic, api::resource_view_handle srv) override;

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
		std::vector<GLuint> _effect_ubos;
		std::vector<GLuint> _reserved_texture_names;
		std::unordered_map<size_t, GLuint> _effect_sampler_states;
		std::unordered_map<std::string, GLuint> _texture_semantic_bindings;

#if RESHADE_GUI
		void init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;

		struct imgui_resources
		{
			GLuint program = 0;
		} _imgui;
#endif
	};
}
