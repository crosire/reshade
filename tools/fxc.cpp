/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_parser.hpp"
#include "effect_preprocessor.hpp"
#include "version.h"
#include <vector>
#include <fstream>
#include <iostream>

void print_usage(const char *path)
{
	printf(R"(usage: %s [options] <filename>

Options:
  -h, --help                Print this help.
  --version                 Print ReShade version.

  -D <value>                Define a pre-processor macro.
  -I <value>                Add directory to include search path.
  -P <value>                Pre-process to file. If <value> is "-", then result is written to standard output instead.

  -Fo <file>                Output SPIR-V binary to the given file.
  -Fe <file>                Output warnings and errors to the given file.

  -E, --entry <value>       Entry point name.
  --glsl                    Print GLSL code for the previously specified entry point.
  --hlsl                    Print HLSL code for the previously specified entry point.
  --shader-model <value>    HLSL shader model version. Can be 30, 40, 41, 50, ...

  -Zi                       Enable debug information.
	)", path);
}

int main(int argc, char *argv[])
{
	const char *filename = nullptr;
	const char *preprocess = nullptr;
	const char *errorfile = nullptr;
	const char *objectfile = nullptr;
	const char *entrypoint = nullptr;
	bool print_glsl = false;
	bool print_hlsl = false;
	bool debug_info = false;
	unsigned int shader_model = 50;

	reshadefx::preprocessor pp;
	pp.add_macro_definition("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
	pp.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", "0");
	pp.add_macro_definition("BUFFER_WIDTH", "800");
	pp.add_macro_definition("BUFFER_HEIGHT", "600");
	pp.add_macro_definition("BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)");
	pp.add_macro_definition("BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)");

	// Parse command-line arguments
	for (int i = 1; i < argc; ++i)
	{
		if (const char *arg = argv[i]; arg[0] == '-')
		{
			if (0 == strcmp(arg, "-h") || 0 == strcmp(arg, "--help"))
			{
				print_usage(argv[0]);
				return 0;
			}
			else if (0 == strcmp(arg, "--version"))
			{
				printf("%s\n", VERSION_STRING_PRODUCT);
				return 0;
			}
			else if (0 == strcmp(arg, "-D"))
			{
				char *macro = argv[++i];
				char *value = strchr(macro, '=');
				if (value) *value++ = '\0';
				pp.add_macro_definition(macro, value ? value : "1");
			}
			else if (0 == strcmp(arg, "-I"))
			{
				pp.add_include_path(argv[++i]);
			}
			else if (0 == strcmp(arg, "-P"))
			{
				preprocess = argv[++i];
			}
			else if (0 == strcmp(arg, "-Fe"))
			{
				errorfile = argv[++i];
			}
			else if (0 == strcmp(arg, "-Fo"))
			{
				objectfile = argv[++i];
			}
			else if (0 == strcmp(arg, "-E") || 0 == strcmp(arg, "--entry"))
			{
				entrypoint = argv[++i];
			}
			else if (0 == strcmp(arg, "--glsl"))
			{
				//if (entrypoint == nullptr)
				//{
				//	std::cout << "error: No entry point specified" << std::endl;
				//	return 1;
				//}

				print_glsl = true;
			}
			else if (0 == strcmp(arg, "--hlsl"))
			{
				//if (entrypoint == nullptr)
				//{
				//	std::cout << "error: No entry point specified" << std::endl;
				//	return 1;
				//}

				print_hlsl = true;
			}
			else if (0 == strcmp(arg, "--shader-model"))
			{
				shader_model = std::strtol(argv[++i], nullptr, 10);
			}
			else if (0 == strcmp(arg, "-Zi"))
			{
				debug_info = true;
			}
		}
		else
		{
			if (filename != nullptr)
			{
				std::cout << "error: More than one input file specified" << std::endl;
				return 1;
			}

			filename = arg;
		}
	}

	if (filename == nullptr)
	{
		print_usage(argv[0]);
		return 1;
	}

	if (!pp.run(filename))
	{
		if (errorfile == nullptr)
			std::cout << pp.errors() << std::endl;
		else
			std::ofstream(errorfile) << pp.errors();
		return 1;
	}

	if (preprocess != nullptr)
	{
		if (strcmp(preprocess, "-") == 0)
			std::cout << pp.current_output() << std::endl;
		else
			std::ofstream(preprocess) << pp.current_output();
		return 0;
	}

	reshadefx::codegen::backend backend = reshadefx::codegen::backend::spirv;

	if (print_glsl)
	{
		backend = reshadefx::codegen::backend::glsl;
	}
	if (print_hlsl)
	{
		backend = reshadefx::codegen::backend::hlsl;
	}

	reshadefx::parser parser(backend, shader_model, debug_info);

	if (!parser.parse(pp.current_output()))
	{
		if (errorfile == nullptr)
			std::cout << parser.errors() << std::endl;
		else
			std::ofstream(errorfile) << parser.errors();
		return 1;
	}

	reshadefx::module module;
	parser.write_result(module);

	if (print_glsl || print_hlsl)
	{
		std::cout << module.hlsl << std::endl;
	}
	else if (objectfile != nullptr)
	{
		std::ofstream(objectfile, std::ios::binary).write(
			reinterpret_cast<const char *>(module.spirv.data()), module.spirv.size() * sizeof(uint32_t));
	}

	return 0;
}
