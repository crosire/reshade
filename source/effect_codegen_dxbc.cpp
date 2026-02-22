/*
 * Copyright (C) 2025 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define RESHADEFX_CODEGEN_HLSL_INLINE

#include "effect_codegen_hlsl.cpp"
#include "com_ptr.hpp"
#include <d3dcompiler.h>

using namespace reshadefx;

class codegen_dxbc final : public codegen_hlsl
{
public:
	codegen_dxbc(unsigned int shader_model, bool debug_info, bool uniforms_to_spec_constants, int optimization_level) :
		codegen_hlsl(shader_model, debug_info, uniforms_to_spec_constants),
		_optimization_level(optimization_level)
	{
	}

	bool assemble_code_for_entry_point(const std::string &entry_point_name, std::string &cso, std::string &assembly, std::string &errors) const override
	{
		const auto entry_point_it = std::find_if(_module.entry_points.begin(), _module.entry_points.end(),
			[&entry_point_name](const std::pair<std::string, shader_type> &entry_point) {
				return entry_point.first == entry_point_name;
			});
		if (entry_point_it == _module.entry_points.end())
			return false;

		std::string hlsl;
		if (!codegen_hlsl::assemble_code_for_entry_point(entry_point_name, hlsl, hlsl, errors))
			return false;

		CHAR profile[] = "cs_0_0";
		profile[0] = entry_point_it->second == shader_type::vertex ? 'v' : entry_point_it->second == shader_type::pixel ? 'p' : 'c';
		profile[3] = '0' + (_shader_model / 10) % 10;
		profile[5] = '0' + (_shader_model % 10);

		UINT compile_flags = 0;
		if (_optimization_level < 0)
			compile_flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		else if (_optimization_level >= 3)
			compile_flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
		else if (_optimization_level == 2)
			compile_flags |= D3DCOMPILE_OPTIMIZATION_LEVEL2;
		else if (_optimization_level == 1)
			compile_flags |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
		else if (_optimization_level == 0)
			compile_flags |= D3DCOMPILE_OPTIMIZATION_LEVEL0;

		if (_shader_model >= 40)
			compile_flags |= D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
		compile_flags |= D3DCOMPILE_DEBUG;
#endif

		com_ptr<ID3DBlob> d3d_errors;
		com_ptr<ID3DBlob> d3d_compiled;
		com_ptr<ID3DBlob> d3d_disassembled;

		const HRESULT hr = D3DCompile(hlsl.data(), hlsl.size(), nullptr, nullptr, nullptr, entry_point_name.c_str(), profile, compile_flags, 0, &d3d_compiled, &d3d_errors);

		if (d3d_errors != nullptr) // Append warnings to the output error string as well
		{
			std::string d3d_errors_string(
				static_cast<const char *>(d3d_errors->GetBufferPointer()),
				d3d_errors->GetBufferSize() - 1); // Subtracting one to not append the null-terminator as well
			d3d_errors.reset();

			// De-duplicate error lines (D3DCompiler sometimes repeats the same error multiple times)
			for (size_t line_offset = 0, next_line_offset; (next_line_offset = d3d_errors_string.find('\n', line_offset)) != std::string::npos; line_offset = next_line_offset + 1)
			{
				const std::string_view cur_line(d3d_errors_string.data() + line_offset, next_line_offset - line_offset);

				if (const size_t end_offset = d3d_errors_string.find('\n', next_line_offset + 1);
					end_offset != std::string::npos)
				{
					const std::string_view next_line(d3d_errors_string.data() + next_line_offset + 1, end_offset - next_line_offset - 1);
					if (cur_line == next_line)
					{
						d3d_errors_string.erase(next_line_offset, end_offset - next_line_offset);
						next_line_offset = line_offset - 1;
					}
				}

				// Also remove D3DCompiler warnings about 'groupshared' specifier used in VS/PS modules
				if (cur_line.find("X3579") != std::string_view::npos)
				{
					d3d_errors_string.erase(line_offset, next_line_offset + 1 - line_offset);
					next_line_offset = line_offset - 1;
				}
			}

			errors += d3d_errors_string;
		}

		if (d3d_compiled != nullptr)
		{
			cso.assign(static_cast<const char *>(d3d_compiled->GetBufferPointer()), d3d_compiled->GetBufferSize());
			d3d_compiled.reset();

			if (SUCCEEDED(D3DDisassemble(cso.data(), cso.size(), 0, nullptr, &d3d_disassembled)))
			{
				assembly.assign(static_cast<const char *>(d3d_disassembled->GetBufferPointer()), d3d_disassembled->GetBufferSize() - 1);
				d3d_disassembled.reset();
			}
		}

		return SUCCEEDED(hr);
	}

	void emit_pragma(const std::string &pragma) override
	{
		if (pragma == "reshade skipoptimization" || pragma == "reshade nooptimization")
			_optimization_level = -1;

		codegen_hlsl::emit_pragma(pragma);
	}

private:
	int _optimization_level;
};

#ifndef RESHADEFX_CODEGEN_DXBC_INLINE
codegen *reshadefx::create_codegen_dxbc(unsigned int shader_model, bool debug_info, bool uniforms_to_spec_constants, int optimization_level)
{
	return new codegen_dxbc(shader_model, debug_info, uniforms_to_spec_constants, optimization_level);
}
#endif
