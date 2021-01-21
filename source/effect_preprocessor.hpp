/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_token.hpp"
#include <memory> // std::unique_ptr
#include <filesystem>
#include <unordered_set>
#include <unordered_map>

namespace reshadefx
{
	/// <summary>
	/// A C-style preprocessor implementation.
	/// </summary>
	class preprocessor
	{
	public:
		struct macro
		{
			std::string replacement_list;
			std::vector<std::string> parameters;
			bool is_variadic = false;
			bool is_function_like = false;
		};

		// Define constructor explicitly because lexer class is not included here
		preprocessor();
		~preprocessor();

		/// <summary>
		/// Add an include directory to the list of search paths used when resolving #include directives.
		/// </summary>
		/// <param name="path">The path to the directory to add.</param>
		void add_include_path(const std::filesystem::path &path);

		/// <summary>
		/// Add a new macro definition. This is equal to appending '#define name macro' to this preprocessor instance.
		/// </summary>
		/// <param name="name">The name of the macro to define.</param>
		/// <param name="macro">The definition of the macro function or value.</param>
		/// <returns></returns>
		bool add_macro_definition(const std::string &name, const macro &macro);
		/// <summary>
		/// Add a new macro value definition. This is equal to appending '#define name macro' to this preprocessor instance.
		/// </summary>
		/// <param name="name">The name of the macro to define.</param>
		/// <param name="value">The value to define that macro to.</param>
		/// <returns></returns>
		bool add_macro_definition(const std::string &name, std::string value = "1")
		{
			return add_macro_definition(name, macro { std::move(value), {} });
		}

		/// <summary>
		/// Open the specified file, parse its contents and append them to the output.
		/// </summary>
		/// <param name="path">The path to the file to parse.</param>
		/// <returns>A boolean value indicating whether parsing was successful or not.</returns>
		bool append_file(const std::filesystem::path &path);
		/// <summary>
		/// Parse the specified string and append it to the output.
		/// </summary>
		/// <param name="source_code">The string to parse.</param>
		/// <returns>A boolean value indicating whether parsing was successful or not.</returns>
		bool append_string(const std::string &source_code);

		/// <summary>
		/// Get the list of error messages.
		/// </summary>
		std::string &errors() { return _errors; }
		const std::string &errors() const { return _errors; }
		/// <summary>
		/// Get the current pre-processed output string.
		/// </summary>
		std::string &output() { return _output; }
		const std::string &output() const { return _output; }

		/// <summary>
		/// Get a list of all included files.
		/// </summary>
		std::vector<std::filesystem::path> included_files() const;

		/// <summary>
		/// Get a list of all defines that were used in #ifdef and #ifndef lines
		/// </summary>
		/// <returns></returns>
		std::vector<std::pair<std::string, std::string>> used_macro_definitions() const;

	private:
		struct if_level
		{
			bool value;
			bool skipping;
			token pp_token;
			size_t input_index;
		};
		struct input_level
		{
			std::string name;
			std::unique_ptr<class lexer> lexer;
			token next_token;
			std::unordered_set<std::string> hidden_macros;
		};

		void error(const location &location, const std::string &message);
		void warning(const location &location, const std::string &message);

		void push(std::string input, const std::string &name = std::string());

		bool peek(tokenid token) const;
		bool consume();
		void consume_until(tokenid token);
		bool accept(tokenid token);
		bool expect(tokenid token);

		void parse();
		void parse_def();
		void parse_undef();
		void parse_if();
		void parse_ifdef();
		void parse_ifndef();
		void parse_elif();
		void parse_else();
		void parse_endif();
		void parse_error();
		void parse_warning();
		void parse_pragma();
		void parse_include();

		bool evaluate_expression();
		bool evaluate_identifier_as_macro();

		void expand_macro(const std::string &name, const macro &macro, const std::vector<std::string> &arguments, std::string &out);
		void create_macro_replacement_list(macro &macro);

		bool _success = true;
		std::string _output, _errors;
		std::string _current_token_raw_data;
		reshadefx::token _token;
		std::vector<if_level> _if_stack;
		std::vector<input_level> _input_stack;
		size_t _next_input_index = 0;
		size_t _current_input_index = 0;
		unsigned short _recursion_count = 0;
		location _output_location;
		std::unordered_set<std::string> _used_macros;
		std::unordered_map<std::string, macro> _macros;
		std::vector<std::filesystem::path> _include_paths;
		std::unordered_map<std::string, std::string> _file_cache;
	};
}
