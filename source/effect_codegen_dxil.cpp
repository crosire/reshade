/*
 * Copyright (C) 2025 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define RESHADEFX_CODEGEN_HLSL_INLINE

#include "effect_codegen_hlsl.cpp"
#include "com_ptr.hpp"
#include <Windows.h>
#include <combaseapi.h>
#include <dxcapi.h>

#pragma comment(lib, "dxcompiler.lib")

using namespace reshadefx;

class codegen_dxil final : public codegen_hlsl
{
public:
	codegen_dxil(unsigned int shader_model, bool debug_info, bool uniforms_to_spec_constants, int optimization_level) :
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

		com_ptr<IDxcCompiler3> compiler;
		if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))))
			return false;

		std::string hlsl;
		if (!codegen_hlsl::assemble_code_for_entry_point(entry_point_name, hlsl, hlsl, errors))
			return false;

		WCHAR profile[] = L"cs_0_0";
		profile[0] = entry_point_it->second == shader_type::vertex ? L'v' : entry_point_it->second == shader_type::pixel ? L'p' : L'c';
		profile[3] = L'0' + (_shader_model / 10) % 10;
		profile[5] = L'0' + (_shader_model % 10);

		const std::wstring entry_point_name_wide(entry_point_name.begin(), entry_point_name.end());

		WCHAR optimization_level_flag[] = L"-O3";
		optimization_level_flag[2] = _optimization_level >= 0 ? L'0' + (_optimization_level % 10) : L'd';

		LPCWSTR arguments[] = {
			L"-T", profile,
			L"-E", entry_point_name_wide.c_str(),
			optimization_level_flag,
			L"-Zi",
			L"-Qembed_debug",
			_shader_model >= 62 ? L"-enable-16bit-types" : L"",
			L"-Wno-ignored-attributes",
		};

		const DxcBuffer hlsl_buffer = {
			hlsl.data(), hlsl.size(), DXC_CP_UTF8
		};

		com_ptr<IDxcResult> result;

		HRESULT hr = compiler->Compile(&hlsl_buffer, arguments, static_cast<UINT32>(std::size(arguments)), nullptr, IID_PPV_ARGS(&result));
		if (result != nullptr)
		{
			result->GetStatus(&hr);

			if (com_ptr<IDxcBlobUtf8> d3d_errors;
				SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&d3d_errors), nullptr)))
			{
				errors.append(static_cast<const char *>(d3d_errors->GetBufferPointer()), d3d_errors->GetBufferSize());
			}
		}

		if (FAILED(hr))
			return false;

		if (com_ptr<IDxcBlob> d3d_compiled;
			SUCCEEDED(hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&d3d_compiled), nullptr)))
		{
			cso.assign(static_cast<const char *>(d3d_compiled->GetBufferPointer()), d3d_compiled->GetBufferSize());

			const DxcBuffer cso_buffer = { cso.data(), cso.size(), DXC_CP_ACP };

			result.reset();

			if (com_ptr<IDxcBlob> d3d_disassembled;
				SUCCEEDED(compiler->Disassemble(&cso_buffer, IID_PPV_ARGS(&result))) &&
				result != nullptr &&
				SUCCEEDED(result->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(&d3d_disassembled), nullptr)))
			{
				assembly.assign(static_cast<const char *>(d3d_disassembled->GetBufferPointer()), d3d_disassembled->GetBufferSize());
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

#ifndef RESHADEFX_CODEGEN_DXIL_INLINE
codegen *reshadefx::create_codegen_dxil(unsigned int shader_model, bool debug_info, bool uniforms_to_spec_constants, int optimization_level)
{
	if (shader_model < 60)
		return nullptr;

	return new codegen_dxil(shader_model, debug_info, uniforms_to_spec_constants, optimization_level);
}
#endif
