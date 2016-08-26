#pragma once

#include <stack>
#include <vector>
#include <unordered_map>
#include <memory>
#include "lexer.hpp"
#include "filesystem.hpp"

namespace reshadefx
{
	class preprocessor
	{
	public:
		struct macro
		{
			std::string replacement_list;
			bool is_function_like = false, is_variadic = false;
			std::vector<std::string> parameters;
		};

		void add_include_path(const reshade::filesystem::path &path);
		bool add_macro_definition(const std::string &name, const macro &macro);
		bool add_macro_definition(const std::string &name, const std::string &value = "1");

		const std::string &current_output() const { return _output; }
		const std::string &current_errors() const { return _errors; }
		const std::vector<std::string> &current_pragmas() const { return _pragmas; }

		bool run(const reshade::filesystem::path &file_path, std::vector<reshade::filesystem::path> &included_files);

	private:
		struct if_level
		{
			lexer::token token;
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
				_next_token.id = lexer::tokenid::unknown;
				_next_token.offset = _next_token.length = 0;
			}

			std::string _name;
			std::unique_ptr<lexer> _lexer;
			lexer::token _next_token;
			size_t _offset;
			std::stack<if_level> _if_stack;
			input_level *_parent;
		};

		void error(const location &location, const std::string &message);
		void warning(const location &location, const std::string &message);

		lexer &current_lexer();
		inline lexer::token current_token() const { return _token; }
		std::stack<if_level> &current_if_stack();
		if_level &current_if_level();
		void push(const std::string &input, const std::string &name = std::string());
		bool peek(lexer::tokenid token) const;
		void consume();
		void consume_until(lexer::tokenid token);
		bool accept(lexer::tokenid token);
		bool expect(lexer::tokenid token);

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
		lexer::token _token;
		std::stack<input_level> _input_stack;
		location _output_location;
		std::string _output, _errors, _current_token_raw_data;
		int _recursion_count = 0;
		std::unordered_map<std::string, macro> _macros;
		std::vector<std::string> _pragmas;
		std::vector<reshade::filesystem::path> _include_paths;
		std::unordered_map<std::string, std::string> _filecache;
	};
}
