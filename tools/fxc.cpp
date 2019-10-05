/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include "effect_preprocessor.hpp"
#include "version.h"
#include <vector>
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

  --width                   Value of the 'BUFFER_WIDTH' preprocessor macro.
  --height                  Value of the 'BUFFER_HEIGHT' preprocessor macro.

  -Zi                       Enable debug information.
	)", path);
}

int main(int argc, char *argv[])
{
	const char *filename = nullptr;
	const char *preprocess = nullptr;
	const char *errorfile = nullptr;
	const char *objectfile = nullptr;
	const char *buffer_width = "800";
	const char *buffer_height = "600";
	bool print_glsl = false;
	bool print_hlsl = false;
	bool debug_info = false;
	unsigned int shader_model = 50;

	reshadefx::parser parser;
	reshadefx::preprocessor pp;
	pp.add_macro_definition("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
	pp.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", "0");

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
			if (0 == strcmp(arg, "--version"))
			{
				printf("%s\n", VERSION_STRING_PRODUCT);
				return 0;
			}

			if (0 == strcmp(arg, "-D"))
			{
				char *macro = argv[++i];
				char *value = strchr(macro, '=');
				if (value) *value++ = '\0';
				pp.add_macro_definition(macro, value ? value : "1");
				continue;
			}

			if (0 == strcmp(arg, "-I"))
			{
				pp.add_include_path(argv[++i]);
				continue;
			}

			if (0 == strcmp(arg, "-Zi"))
				debug_info = true;
			else if (0 == strcmp(arg, "--glsl"))
				print_glsl = true;
			else if (0 == strcmp(arg, "--hlsl"))
				print_hlsl = true;

			if (i + 1 >= argc)
				break;
			else if (0 == strcmp(arg, "-P"))
				preprocess = argv[++i];
			else if (0 == strcmp(arg, "-Fe"))
				errorfile = argv[++i];
			else if (0 == strcmp(arg, "-Fo"))
				objectfile = argv[++i];
			else if (0 == strcmp(arg, "--shader-model"))
				shader_model = std::strtol(argv[++i], nullptr, 10);
			else if (0 == strcmp(arg, "--width"))
				buffer_width = argv[++i];
			else if (0 == strcmp(arg, "--height"))
				buffer_height = argv[++i];
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

	pp.add_macro_definition("BUFFER_WIDTH", buffer_width);
	pp.add_macro_definition("BUFFER_HEIGHT", buffer_height);
	pp.add_macro_definition("BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)");
	pp.add_macro_definition("BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)");

	if (!pp.append_file(filename))
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
			std::cout << pp.output() << std::endl;
		else
			std::ofstream(preprocess) << pp.output();
		return 0;
	}

	std::unique_ptr<reshadefx::codegen> backend;
	if (print_glsl)
		backend.reset(reshadefx::create_codegen_glsl(debug_info));
	else if (print_hlsl)
		backend.reset(reshadefx::create_codegen_hlsl(shader_model, debug_info));
	else
		backend.reset(reshadefx::create_codegen_spirv(true, debug_info));

	if (!parser.parse(pp.output(), backend.get()))
	{
		if (errorfile == nullptr)
			std::cout << pp.errors() << parser.errors() << std::endl;
		else
			std::ofstream(errorfile) << pp.errors() << parser.errors();
		return 1;
	}

	reshadefx::module module;
	backend->write_result(module);

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
