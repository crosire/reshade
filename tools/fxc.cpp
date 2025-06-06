/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include "effect_preprocessor.hpp"
#include "version.h"
#include <cstring>
#include <fstream>
#include <iostream>

static void print_usage(const char *path)
{
	printf(R"(usage: %s [options] <filename>

Options:
  -h, --help                Print this help.
  --version                 Print ReShade version.

  -D <id>=<text>            Define a preprocessor macro.
  -I <path>                 Add directory to include search path.
  -P <path>                 Pre-process to file. If <path> is "-", then result is written to standard output instead.

  -Fo <file>                Output SPIR-V binary to the given file.
  -Fe <file>                Output warnings and errors to the given file.

  --glsl                    Print GLSL code for the previously specified entry point.
  --hlsl                    Print HLSL code for the previously specified entry point.
  --shader-model <value>    HLSL shader model version. Can be 30, 40, 41, 50, ...

  --width <value>           Value of the 'BUFFER_WIDTH' preprocessor macro.
  --height <value>          Value of the 'BUFFER_HEIGHT' preprocessor macro.
  --invert-y                Insert code to invert the Y component of the output position in vertex shaders (only applies to SPIR-V).
  --spec-constants          Convert uniform variables to specialization constants.
  --vulkan-semantics        Generate GLSL/SPIR-V code under Vulkan semantics, instead of OpenGL semantics.

  -Zi                       Enable debug information.
	)", path);
}

int main(int argc, char *argv[])
{
	const char *source_file = nullptr;
	const char *preprocess_file = nullptr;
	const char *error_file = nullptr;
	const char *object_file = nullptr;
	const char *buffer_width = "800";
	const char *buffer_height = "600";
	bool print_glsl = false;
	bool print_hlsl = false;
	bool debug_info = false;
	bool invert_y_axis = false;
	bool spec_constants = false;
	bool vulkan_semantics = false;
	unsigned int shader_model = 50;

	reshadefx::preprocessor pp;
	pp.add_macro_definition("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
	pp.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", "0");

	// Parse command-line arguments
	for (int i = 1; i < argc; ++i)
	{
		if (const char *arg = argv[i]; arg[0] == '-')
		{
			if (0 == std::strcmp(arg, "-h") || 0 == std::strcmp(arg, "--help"))
			{
				print_usage(argv[0]);
				return 0;
			}
			if (0 == std::strcmp(arg, "--version"))
			{
				std::cout << VERSION_STRING_PRODUCT << std::endl;
				return 0;
			}

			if (0 == std::strcmp(arg, "-D"))
			{
				char *name = argv[++i];
				char *value = std::strchr(name, '=');
				if (value) *value++ = '\0';
				pp.add_macro_definition(name, value ? value : "1");
				continue;
			}

			if (0 == std::strcmp(arg, "-I"))
			{
				pp.add_include_path(argv[++i]);
				continue;
			}

			if (0 == std::strcmp(arg, "-Zi"))
				debug_info = true;
			else if (0 == std::strcmp(arg, "--glsl"))
				print_glsl = true;
			else if (0 == std::strcmp(arg, "--hlsl"))
				print_hlsl = true;
			else if (0 == std::strcmp(arg, "--invert-y"))
				invert_y_axis = true;
			else if (0 == std::strcmp(arg, "--spec-constants"))
				spec_constants = true;
			else if (0 == std::strcmp(arg, "--vulkan-semantics"))
				vulkan_semantics = true;

			if (i + 1 >= argc)
				continue;
			else if (0 == std::strcmp(arg, "-P"))
				preprocess_file = argv[++i];
			else if (0 == std::strcmp(arg, "-Fe"))
				error_file = argv[++i];
			else if (0 == std::strcmp(arg, "-Fo"))
				object_file = argv[++i];
			else if (0 == std::strcmp(arg, "--shader-model"))
				shader_model = static_cast<unsigned int>(std::strtoul(argv[++i], nullptr, 10));
			else if (0 == std::strcmp(arg, "--width"))
				buffer_width = argv[++i];
			else if (0 == std::strcmp(arg, "--height"))
				buffer_height = argv[++i];
		}
		else
		{
			if (source_file != nullptr)
			{
				std::cout << "error: More than one input file specified" << std::endl;
				return 1;
			}

			source_file = arg;
		}
	}

	if (source_file == nullptr || (print_glsl && print_hlsl) || (print_glsl && object_file) || (print_hlsl && object_file))
	{
		print_usage(argv[0]);
		return 1;
	}

	pp.add_macro_definition("BUFFER_WIDTH", buffer_width);
	pp.add_macro_definition("BUFFER_HEIGHT", buffer_height);
	pp.add_macro_definition("BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)");
	pp.add_macro_definition("BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)");

	if (!pp.append_file(source_file))
	{
		if (error_file == nullptr)
			std::cout << pp.errors() << std::endl;
		else
			std::ofstream(error_file) << pp.errors();
		return 1;
	}

	if (preprocess_file != nullptr)
	{
		if (std::strcmp(preprocess_file, "-") == 0)
			std::cout << pp.output() << std::endl;
		else
			std::ofstream(preprocess_file) << pp.output();
		return 0;
	}

	std::unique_ptr<reshadefx::codegen> backend;
	if (print_glsl)
		backend.reset(reshadefx::create_codegen_glsl(vulkan_semantics, debug_info, spec_constants, invert_y_axis));
	else if (print_hlsl)
		backend.reset(reshadefx::create_codegen_hlsl(shader_model, debug_info, spec_constants));
	else
		backend.reset(reshadefx::create_codegen_spirv(vulkan_semantics, debug_info, spec_constants, invert_y_axis));

	reshadefx::parser parser;
	if (!parser.parse(pp.output(), backend.get()))
	{
		if (error_file == nullptr)
			std::cout << pp.errors() << parser.errors() << std::endl;
		else
			std::ofstream(error_file) << pp.errors() << parser.errors();
		return 1;
	}

	std::basic_string<char> code = backend->finalize_code();

	if (print_glsl || print_hlsl)
	{
		std::cout.write(code.data(), code.size()).flush();
	}
	else if (object_file != nullptr)
	{
		std::ofstream(object_file, std::ios::binary).write(code.data(), code.size());
	}

	return 0;
}
