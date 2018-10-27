/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_parser.hpp"

namespace reshade::opengl
{
	class opengl_effect_compiler
	{
	public:
		opengl_effect_compiler(class opengl_runtime *runtime, const reshadefx::module &module, std::string &errors);

		bool run();

	private:
		void error(const std::string &message);
		void warning( const std::string &message);

		void visit_texture(const reshadefx::texture_info &texture_info);
		void visit_sampler(const reshadefx::sampler_info &sampler_info);
		void visit_uniform(const reshadefx::uniform_info &uniform_info);
		void visit_technique(const reshadefx::technique_info &technique_info);

		void compile_entry_point(const std::string &name, bool is_ps);

		opengl_runtime *_runtime;
		const reshadefx::module *_module;
		bool _success;
		std::string &_errors;
		GLintptr _uniform_storage_offset = 0, _uniform_buffer_size = 0;
		std::vector<opengl_sampler_data> _sampler_bindings;
		std::unordered_map<std::string, GLuint> vs_entry_points;
		std::unordered_map<std::string, GLuint> fs_entry_points;
	};
}
