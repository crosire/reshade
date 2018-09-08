/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "spirv_module.hpp"
#include <sstream>
#include <unordered_set>
#include <spirv_hlsl.hpp>

namespace reshade::d3d10
{
	#pragma region Forward Declarations
	struct d3d10_pass_data;
	class d3d10_runtime;
	#pragma endregion

	class d3d10_effect_compiler
	{
	public:
		d3d10_effect_compiler(d3d10_runtime *runtime, const reshadefx::spirv_module &module, std::string &errors, bool skipoptimization = false);

		bool run();

	private:
		void error(const std::string &message);
		void warning(const std::string &message);

		void visit_texture(const reshadefx::spirv_texture_info &texture_info);
		void visit_sampler(const reshadefx::spirv_sampler_info &sampler_info);
		void visit_uniform(const spirv_cross::CompilerHLSL &cross, const reshadefx::spirv_uniform_info &uniform_info);
		void visit_technique(const reshadefx::spirv_technique_info &technique_info);

		void compile_entry_point(spirv_cross::CompilerHLSL &cross, const spirv_cross::EntryPoint &entry, unsigned int shader_model_version);

		d3d10_runtime *_runtime;
		const reshadefx::spirv_module *_module;
		bool _success = true;
		std::string &_errors;
		std::stringstream _global_code, _global_uniforms;
		bool _skip_shader_optimization, _is_in_parameter_block = false, _is_in_function_block = false;
		size_t _uniform_storage_offset = 0, _constant_buffer_size = 0;
		HMODULE _d3dcompiler_module = nullptr;
		std::unordered_map<std::string, com_ptr<ID3D10VertexShader>> vs_entry_points;
		std::unordered_map<std::string, com_ptr< ID3D10PixelShader>> ps_entry_points;
		std::vector<com_ptr<ID3D10SamplerState>> _sampler_bindings;
		std::vector<com_ptr<ID3D10ShaderResourceView>> _texture_bindings;
	};
}
