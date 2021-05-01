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
	class runtime_impl : public runtime, public device_impl
	{
	public:
		runtime_impl(HDC hdc, HGLRC hglrc);
		~runtime_impl();

		bool get_data(const uint8_t guid[16], void **ptr) const final { return device_impl::get_data(guid, ptr); }
		void set_data(const uint8_t guid[16], void *const ptr)  final { device_impl::set_data(guid, ptr); }

		uint64_t get_native_object() const final { return reinterpret_cast<uintptr_t>(*_hdcs.begin()); } // Simply return the first device context

		api::device *get_device() final { return this; }
		api::command_queue *get_command_queue() final { return this; }

		bool on_init(HWND hwnd, unsigned int width, unsigned int height);
		void on_reset();
		void on_present(bool default_fbo = true);
		bool on_layer_submit(uint32_t eye, GLuint source_object, bool is_rbo, bool is_array, const float bounds[4], GLuint *target_rbo);

		bool capture_screenshot(uint8_t *buffer) const final;

		void update_texture_bindings(const char *semantic, api::resource_view srv) final;

	private:
		bool init_effect(size_t index) final;
		void unload_effect(size_t index) final;
		void unload_effects() final;

		bool init_texture(texture &texture) final;
		void upload_texture(const texture &texture, const uint8_t *data) final;
		void destroy_texture(texture &texture) final;
		void generate_mipmaps(const struct tex_data *impl);

		void render_technique(technique &technique) final;

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
		void render_imgui_draw_data(ImDrawData *data) final;

		struct imgui_resources
		{
			GLuint program = 0;
		} _imgui;
#endif
	};
}
