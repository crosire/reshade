/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <stack>
#include <vector>
#include <unordered_map>
#include <memory>
#include "effect_lexer.hpp"
#include "filesystem.hpp"

namespace reshadefx
{
	/// <summary>
	/// A C-style pre-processor implementation.
	/// </summary>
	class preprocessor
	{
	public:
		struct macro
		{
			std::string replacement_list;
			bool is_function_like = false, is_variadic = false;
			std::vector<std::string> parameters;
		};

		/// <summary>
		/// Add an include directory to the list of search paths used when resolving #include directives.
		/// </summary>
		/// <param name="path">The path to the directory to add.</param>
		void add_include_path(const reshade::filesystem::path &path);

		/// <summary>
		/// Add a new macro definition. This is equal to appending '#define name macro' to this pre-processor instance.
		/// </summary>
		/// <param name="name">The name of the macro to define.</param>
		/// <param name="macro">The definition of the macro function or value.</param>
		/// <returns></returns>
		bool add_macro_definition(const std::string &name, const macro &macro);
		/// <summary>
		/// Add a new macro value definition. This is equal to appending '#define name macro' to this pre-processor instance.
		/// </summary>
		/// <param name="name">The name of the macro to define.</param>
		/// <param name="value">The value to define that macro to.</param>
		/// <returns></returns>
		bool add_macro_definition(const std::string &name, const std::string &value = "1");

		/// <summary>
		/// Open the specified file, parse its contents and append them to the output.
		/// </summary>
		/// <param name="path">The path to the file to parse.</param>
		/// <returns>A boolean value indicating whether parsing was successful or not.</returns>
		bool append_file(const reshade::filesystem::path &path);
		/// <summary>
		/// Parse the specified string and append it to the output.
		/// </summary>
		/// <param name="source_code">The string to parse.</param>
		/// <returns>A boolean value indicating whether parsing was successful or not.</returns>
		bool append_string(const std::string &source_code);

		std::string &errors() { return _errors; }
		const std::string &errors() const { return _errors; }
		std::string &output() { return _output; }
		const std::string &output() const { return _output; }

	private:
		struct if_level
		{
			token token;
			bool value, skipping;
			if_level *parent;
		};
		struct input_level
		{
			input_level(const std::string &name, const std::string &text, input_level *parent) :
				_name(name),
				_lexer(new lexer(text, false, false, true, false)),
				_parent(parent)
			{
				_next_token.id = tokenid::unknown;
				_next_token.offset = _next_token.length = 0;
			}

			std::string _name;
			std::unique_ptr<lexer> _lexer;
			token _next_token;
			size_t _offset;
			std::stack<if_level> _if_stack;
			input_level *_parent;
		};

		void error(const location &location, const std::string &message);
		void warning(const location &location, const std::string &message);

		lexer &current_lexer();
		inline token current_token() const { return _token; }
		std::stack<if_level> &current_if_stack();
		if_level &current_if_level();

		void push(const std::string &input, const std::string &name = std::string());

		bool peek(tokenid token) const;
		void consume();
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

		void expand_macro(const macro &macro, const std::vector<std::string> &arguments, std::string &out);
		void create_macro_replacement_list(macro &macro);

		bool _success = true;
		token _token;
		std::stack<input_level> _input_stack;
		location _output_location;
		std::string _output, _errors, _current_token_raw_data;
		int _recursion_count = 0;
		std::unordered_map<std::string, macro> _macros;
		std::vector<reshade::filesystem::path> _include_paths;
		std::unordered_map<std::string, std::string> _filecache;
	};
}
