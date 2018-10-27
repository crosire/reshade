/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_parser.hpp"

namespace reshade::d3d10
{
	class d3d10_effect_compiler
	{
	public:
		d3d10_effect_compiler(class d3d10_runtime *runtime, const reshadefx::module &module, std::string &errors);

		bool run();

	private:
		void error(const std::string &message);
		void warning(const std::string &message);

		void visit_texture(const reshadefx::texture_info &texture_info);
		void visit_sampler(const reshadefx::sampler_info &sampler_info);
		void visit_uniform(const reshadefx::uniform_info &uniform_info);
		void visit_technique(const reshadefx::technique_info &technique_info);

		void compile_entry_point(const std::string &entry_point, bool is_ps);

		d3d10_runtime *_runtime;
		const reshadefx::module *_module;
		bool _success = true;
		std::string &_errors;
		size_t _uniform_storage_offset = 0, _constant_buffer_size = 0;
		HMODULE _d3dcompiler_module = nullptr;
		std::unordered_map<std::string, com_ptr<ID3D10VertexShader>> vs_entry_points;
		std::unordered_map<std::string, com_ptr< ID3D10PixelShader>> ps_entry_points;
		std::vector<com_ptr<ID3D10SamplerState>> _sampler_bindings;
		std::vector<com_ptr<ID3D10ShaderResourceView>> _texture_bindings;
	};
}
