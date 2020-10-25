/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_lexer.hpp"
#include "effect_preprocessor.hpp"
#include <cassert>

#ifndef _WIN32
	// On Linux systems the native path encoding is UTF-8 already, so no conversion necessary
	#define u8path(p) path(p)
	#define u8string() string()
#endif

enum op_type
{
	op_none = -1,

	op_or,
	op_and,
	op_bitor,
	op_bitxor,
	op_bitand,
	op_not_equal,
	op_equal,
	op_less,
	op_greater,
	op_less_equal,
	op_greater_equal,
	op_leftshift,
	op_rightshift,
	op_add,
	op_subtract,
	op_modulo,
	op_divide,
	op_multiply,
	op_plus,
	op_negate,
	op_not,
	op_bitnot,
	op_parentheses
};

enum macro_replacement
{
	macro_replacement_start = '\x00',
	macro_replacement_argument = '\xFA',
	macro_replacement_concat = '\xFF',
	macro_replacement_stringize = '\xFE',
	macro_replacement_space = '\xFD',
	macro_replacement_break = '\xFC',
	macro_replacement_expand = '\xFB',
};

static const int precedence_lookup[] = {
	0, 1, 2, 3, 4, // bitwise operators
	5, 6, 7, 7, 7, 7, // logical operators
	8, 8, // left shift, right shift
	9, 9, // add, subtract
	10, 10, 10, // modulo, divide, multiply
	11, 11, 11, 11 // unary operators
};

static bool read_file(const std::filesystem::path &path, std::string &data)
{
#ifdef _WIN32
	FILE *file = nullptr;
	if (_wfopen_s(&file, path.c_str(), L"rb") != 0)
		return false;
#else
	FILE *const file = fopen(path.c_str(), "rb");
	if (file == nullptr)
		return false;
#endif

	// Read file contents into memory
	std::vector<char> file_mem(static_cast<size_t>(std::filesystem::file_size(path) + 1));
	const size_t eof = fread(file_mem.data(), 1, file_mem.size() - 1, file);

	// Append a new line feed to the end of the input string to avoid issues with parsing
	file_mem[eof] = '\n';

	// No longer need to have a handle open to the file, since all data was read, so can safely close it
	fclose(file);

	std::string_view file_data(file_mem.data(), file_mem.size());

	// Remove BOM (0xefbbbf means 0xfeff)
	if (file_data.size() >= 3 &&
		static_cast<unsigned char>(file_data[0]) == 0xef &&
		static_cast<unsigned char>(file_data[1]) == 0xbb &&
		static_cast<unsigned char>(file_data[2]) == 0xbf)
		file_data = std::string_view(file_data.data() + 3, file_data.size() - 3);

	data = file_data;
	return true;
}

static std::string escape_string(std::string s)
{
	for (size_t offset = 0; (offset = s.find('\\', offset)) != std::string::npos; offset += 2)
		s.insert(offset, "\\", 1);
	return '\"' + s + '\"';
}

reshadefx::preprocessor::preprocessor()
{
}
reshadefx::preprocessor::~preprocessor()
{
}

void reshadefx::preprocessor::add_include_path(const std::filesystem::path &path)
{
	assert(!path.empty());
	_include_paths.push_back(path);
}
bool reshadefx::preprocessor::add_macro_definition(const std::string &name, const macro &macro)
{
	assert(!name.empty());
	return _macros.emplace(name, macro).second;
}

bool reshadefx::preprocessor::append_file(const std::filesystem::path &path)
{
	std::string data;
	if (!read_file(path, data))
		return false;

	_success = true; // Clear success flag before parsing a new file

	push(std::move(data), path.u8string());
	parse();

	return _success;
}
bool reshadefx::preprocessor::append_string(const std::string &source_code)
{
	// Enforce all input strings to end with a line feed
	assert(!source_code.empty() && source_code.back() == '\n');

	_success = true; // Clear success flag before parsing a new string

	// Give this push a name, so that lexer location starts at a new line
	// This is necessary in case this string starts with a preprocessor directive, since the lexer only reports those as such if they appear at the beginning of a new line
	// But without a name, the lexer location is set to the last token location, which most likely will not be at the start of the line
	push(source_code, "unknown");
	parse();

	return _success;
}

std::vector<std::filesystem::path> reshadefx::preprocessor::included_files() const
{
	std::vector<std::filesystem::path> files;
	files.reserve(_file_cache.size());
	for (const auto &it : _file_cache)
		files.push_back(std::filesystem::u8path(it.first));
	return files;
}
std::vector<std::pair<std::string, std::string>> reshadefx::preprocessor::used_macro_definitions() const
{
	std::vector<std::pair<std::string, std::string>> defines;
	defines.reserve(_used_macros.size());
	for (const std::string &name : _used_macros)
		if (const auto it = _macros.find(name);
			// Do not include function-like macros, since they are more likely to contain a complex replacement list
			it != _macros.end() && !it->second.is_function_like)
			defines.push_back({ name, it->second.replacement_list });
	return defines;
}

void reshadefx::preprocessor::error(const location &location, const std::string &message)
{
	_errors += location.source + '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": preprocessor error: " + message + '\n';
	_success = false; // Unset success flag
}
void reshadefx::preprocessor::warning(const location &location, const std::string &message)
{
	_errors += location.source + '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": preprocessor warning: " + message + '\n';
}

void reshadefx::preprocessor::push(std::string input, const std::string &name)
{
	location start_location = !name.empty() ?
		// Start at the beginning of the file when pushing a new file
		location(name, 1) :
		// Start with last known token location when pushing an unnamed string
		_token.location;

	input_level level = { name };
	level.lexer.reset(new lexer(
		std::move(input),
		true  /* ignore_comments */,
		false /* ignore_whitespace */,
		false /* ignore_pp_directives */,
		false /* ignore_line_directives */,
		true  /* ignore_keywords */,
		false /* escape_string_literals */,
		start_location));
	level.next_token.id = tokenid::unknown;
	level.next_token.location = start_location; // This is used in 'consume' to initialize the output location

	// Inherit hidden macros from parent
	if (!_input_stack.empty())
		level.hidden_macros = _input_stack.back().hidden_macros;

	_input_stack.push_back(std::move(level));
	_next_input_index = _input_stack.size() - 1;

	// Advance into the input stack to update next token
	consume();
}

bool reshadefx::preprocessor::peek(tokenid token) const
{
	return _input_stack[_next_input_index].next_token == token;
}
bool reshadefx::preprocessor::consume()
{
	_current_input_index = _next_input_index;

	if (_input_stack.empty())
	{
		// End of input has been reached already (this can happen when the input text is not terminated with a new line)
		assert(_current_input_index == 0);
		return false;
	}

	// Clear out input stack, now that the current token is overwritten
	while (_input_stack.size() > (_current_input_index + 1))
		_input_stack.pop_back();

	// Update location information after switching input levels
	input_level &input = _input_stack[_current_input_index];
	if (!input.name.empty() && input.name != _output_location.source)
	{
		_output += "#line " + std::to_string(input.next_token.location.line) + " \"" + input.name + "\"\n";
		_output_location.line = input.next_token.location.line;
		_output_location.source = input.name;
	}

	// Set current token
	_token = std::move(input.next_token);
	_current_token_raw_data = input.lexer->input_string().substr(_token.offset, _token.length);

	// Get the next token
	input.next_token = input.lexer->lex();

	// Verify string literals (since the lexer cannot throw errors itself)
	if (_token == tokenid::string_literal && _current_token_raw_data.back() != '\"')
		error(_token.location, "unterminated string literal");

	// Pop input level if lexical analysis has reached the end of it
	// This ensures the EOF token is not consumed until the very last file
	while (peek(tokenid::end_of_file))
	{
		// Remove any unterminated blocks from the stack
		for (; !_if_stack.empty() && _if_stack.back().input_index >= _next_input_index; _if_stack.pop_back())
			error(_if_stack.back().pp_token.location, "unterminated #if");

		if (_next_input_index == 0)
		{
			// End of input has been reached, so cannot pop further and this is the last token
			_input_stack.pop_back();
			return false;
		}
		else
		{
			_next_input_index -= 1;
		}
	}

	return true;
}
void reshadefx::preprocessor::consume_until(tokenid token)
{
	while (!accept(token) && !peek(tokenid::end_of_file))
	{
		consume();
	}
}

bool reshadefx::preprocessor::accept(tokenid token)
{
	while (peek(tokenid::space))
	{
		consume();
	}

	if (peek(token))
	{
		consume();
		return true;
	}

	return false;
}
bool reshadefx::preprocessor::expect(tokenid token)
{
	if (!accept(token))
	{
		auto actual_token = _input_stack[_next_input_index].next_token;
		actual_token.location.source = _output_location.source;

		error(actual_token.location, "syntax error: unexpected token '" +
			_input_stack[_next_input_index].lexer->input_string().substr(actual_token.offset, actual_token.length) + '\'');

		return false;
	}

	return true;
}

void reshadefx::preprocessor::parse()
{
	std::string line;

	while (consume())
	{
		_recursion_count = 0;

		const bool skip = !_if_stack.empty() && _if_stack.back().skipping;

		switch (_token)
		{
		case tokenid::hash_if:
			parse_if();
			if (!expect(tokenid::end_of_line))
				consume_until(tokenid::end_of_line);
			continue;
		case tokenid::hash_ifdef:
			parse_ifdef();
			if (!expect(tokenid::end_of_line))
				consume_until(tokenid::end_of_line);
			continue;
		case tokenid::hash_ifndef:
			parse_ifndef();
			if (!expect(tokenid::end_of_line))
				consume_until(tokenid::end_of_line);
			continue;
		case tokenid::hash_else:
			parse_else();
			if (!expect(tokenid::end_of_line))
				consume_until(tokenid::end_of_line);
			continue;
		case tokenid::hash_elif:
			parse_elif();
			if (!expect(tokenid::end_of_line))
				consume_until(tokenid::end_of_line);
			continue;
		case tokenid::hash_endif:
			parse_endif();
			if (!expect(tokenid::end_of_line))
				consume_until(tokenid::end_of_line);
			continue;
		default:
			// All other tokens are handled below
			break;
		}

		if (skip)
			continue;

		switch (_token)
		{
		case tokenid::hash_def:
			parse_def();
			if (!expect(tokenid::end_of_line))
				consume_until(tokenid::end_of_line);
			continue;
		case tokenid::hash_undef:
			parse_undef();
			if (!expect(tokenid::end_of_line))
				consume_until(tokenid::end_of_line);
			continue;
		case tokenid::hash_error:
			parse_error();
			if (!expect(tokenid::end_of_line))
				consume_until(tokenid::end_of_line);
			continue;
		case tokenid::hash_warning:
			parse_warning();
			if (!expect(tokenid::end_of_line))
				consume_until(tokenid::end_of_line);
			continue;
		case tokenid::hash_pragma:
			parse_pragma();
			if (!expect(tokenid::end_of_line))
				consume_until(tokenid::end_of_line);
			continue;
		case tokenid::hash_include:
			parse_include();
			continue;
		case tokenid::hash_unknown:
			error(_token.location, "unrecognized preprocessing directive '" + _token.literal_as_string + '\'');
			consume_until(tokenid::end_of_line);
			continue;
		case tokenid::end_of_line:
			if (line.empty())
				continue;
			_output_location.line++;
			if (_output_location.line != _token.location.line)
			{
				_output += "#line " + std::to_string(_token.location.line) + '\n';
				_output_location.line  = _token.location.line;
			}
			_output += line;
			_output += '\n';
			line.clear();
			continue;
		case tokenid::identifier:
			if (evaluate_identifier_as_macro())
				continue;
			// fall through
		default:
			line += _current_token_raw_data;
			break;
		}
	}

	// Append the last line after the EOF was reached to the output
	_output += line;
	_output += '\n';
}

void reshadefx::preprocessor::parse_def()
{
	if (!expect(tokenid::identifier))
		return;
	else if (_token.literal_as_string == "defined")
		return warning(_token.location, "macro name 'defined' is reserved");

	macro m;
	const auto location = std::move(_token.location);
	const auto macro_name = std::move(_token.literal_as_string);
	const auto macro_name_end_offset = _token.offset + _token.length;

	// Check input string here directly to ensure the parenthesis follows the macro name without any whitespace between
	if (_input_stack[_current_input_index].lexer->input_string()[macro_name_end_offset] == '(')
	{
		accept(tokenid::parenthesis_open);

		m.is_function_like = true;

		while (accept(tokenid::identifier))
		{
			m.parameters.push_back(_token.literal_as_string);

			if (!accept(tokenid::comma))
				break;
		}

		if (accept(tokenid::ellipsis))
		{
			m.is_variadic = true;
			m.parameters.push_back("__VA_ARGS__");

			// TODO: Implement variadic macros
			error(_token.location, "variadic macros are not currently supported");
			return;
		}

		if (!expect(tokenid::parenthesis_close))
			return;
	}

	create_macro_replacement_list(m);

	if (!add_macro_definition(macro_name, m))
		return error(location, "redefinition of '" + macro_name + "'");
}
void reshadefx::preprocessor::parse_undef()
{
	if (!expect(tokenid::identifier))
		return;
	else if (_token.literal_as_string == "defined")
		return warning(_token.location, "macro name 'defined' is reserved");

	_macros.erase(_token.literal_as_string);
}

void reshadefx::preprocessor::parse_if()
{
	if_level level;
	level.pp_token = _token;
	level.input_index = _current_input_index;

	// Evaluate expression after updating 'pp_token', so that it points at the beginning # token
	level.value = evaluate_expression();

	const bool parent_skipping = !_if_stack.empty() && _if_stack.back().skipping;
	level.skipping = parent_skipping || !level.value;

	_if_stack.push_back(std::move(level));
}
void reshadefx::preprocessor::parse_ifdef()
{
	if_level level;
	level.pp_token = _token;
	level.input_index = _current_input_index;

	if (!expect(tokenid::identifier))
		return;

	level.value = _macros.find(_token.literal_as_string) != _macros.end() ||
		// Check built-in macros as well
		_token.literal_as_string == "__LINE__" ||
		_token.literal_as_string == "__FILE__" ||
		_token.literal_as_string == "__FILE_NAME__" ||
		_token.literal_as_string == "__FILE_STEM__";

	const bool parent_skipping = !_if_stack.empty() && _if_stack.back().skipping;
	level.skipping = parent_skipping || !level.value;

	_if_stack.push_back(std::move(level));
	if (!parent_skipping) // Only add if this #ifdef is active
		_used_macros.emplace(_token.literal_as_string);
}
void reshadefx::preprocessor::parse_ifndef()
{
	if_level level;
	level.pp_token = _token;
	level.input_index = _current_input_index;

	if (!expect(tokenid::identifier))
		return;

	level.value = _macros.find(_token.literal_as_string) == _macros.end() &&
		_token.literal_as_string != "__LINE__" &&
		_token.literal_as_string != "__FILE__" &&
		_token.literal_as_string != "__FILE_NAME__" &&
		_token.literal_as_string != "__FILE_STEM__";

	const bool parent_skipping = !_if_stack.empty() && _if_stack.back().skipping;
	level.skipping = parent_skipping || !level.value;

	_if_stack.push_back(std::move(level));
	if (!parent_skipping) // Only add if this #ifndef is active
		_used_macros.emplace(_token.literal_as_string);
}
void reshadefx::preprocessor::parse_elif()
{
	if (_if_stack.empty())
		return error(_token.location, "missing #if for #elif");

	if_level &level = _if_stack.back();
	if (level.pp_token == tokenid::hash_else)
		return error(_token.location, "#elif is not allowed after #else");

	// Update 'pp_token' before evaluating expression, so that it points at the beginning # token
	level.pp_token = _token;
	level.input_index = _current_input_index;

	const bool parent_skipping = _if_stack.size() > 1 && _if_stack[_if_stack.size() - 2].skipping;
	const bool condition_result = evaluate_expression();
	level.skipping = parent_skipping || level.value || !condition_result;

	if (!level.value) level.value = condition_result;
}
void reshadefx::preprocessor::parse_else()
{
	if (_if_stack.empty())
		return error(_token.location, "missing #if for #else");

	if_level &level = _if_stack.back();
	if (level.pp_token == tokenid::hash_else)
		return error(_token.location, "#else is not allowed after #else");

	level.pp_token = _token;
	level.input_index = _current_input_index;

	const bool parent_skipping = _if_stack.size() > 1 && _if_stack[_if_stack.size() - 2].skipping;
	level.skipping = parent_skipping || level.value;

	if (!level.value) level.value = true;
}
void reshadefx::preprocessor::parse_endif()
{
	if (_if_stack.empty())
		error(_token.location, "missing #if for #endif");
	else
		_if_stack.pop_back();
}

void reshadefx::preprocessor::parse_error()
{
	const auto keyword_location = std::move(_token.location);
	if (!expect(tokenid::string_literal))
		return;
	error(keyword_location, _token.literal_as_string);
}
void reshadefx::preprocessor::parse_warning()
{
	const auto keyword_location = std::move(_token.location);
	if (!expect(tokenid::string_literal))
		return;
	warning(keyword_location, _token.literal_as_string);
}

void reshadefx::preprocessor::parse_pragma()
{
	const auto keyword_location = std::move(_token.location);
	if (!expect(tokenid::identifier))
		return;

	std::string pragma = std::move(_token.literal_as_string);

	while (!peek(tokenid::end_of_line) && !peek(tokenid::end_of_file))
	{
		consume();

		if (_token == tokenid::identifier && evaluate_identifier_as_macro())
			continue;

		pragma += _current_token_raw_data;
	}

	if (pragma == "once")
	{
		if (const auto it = _file_cache.find(_output_location.source); it != _file_cache.end())
			it->second.clear();
		return;
	}

	warning(keyword_location, "unknown pragma ignored");
}

void reshadefx::preprocessor::parse_include()
{
	const auto keyword_location = std::move(_token.location);

	while (accept(tokenid::identifier))
	{
		if (evaluate_identifier_as_macro())
			continue;

		error(_token.location, "syntax error: unexpected identifier in #include");
		consume_until(tokenid::end_of_line);
		return;
	}

	if (!expect(tokenid::string_literal))
	{
		consume_until(tokenid::end_of_line);
		return;
	}

	std::filesystem::path file_name = std::filesystem::u8path(_token.literal_as_string);
	std::filesystem::path file_path = std::filesystem::u8path(_output_location.source);
	file_path.replace_filename(file_name);

	if (std::error_code ec; !std::filesystem::exists(file_path, ec))
		for (const std::filesystem::path &include_path : _include_paths)
			if (std::filesystem::exists(file_path = include_path / file_name, ec))
				break;

	const std::string file_path_string = file_path.u8string();

	// Detect recursive include and abort to avoid infinite loop
	if (std::find_if(_input_stack.begin(), _input_stack.end(),
		[&file_path_string](const input_level &level) { return level.name == file_path_string; }) != _input_stack.end())
	{
		error(_token.location, "recursive #include");
		return;
	}

	std::string data;
	if (auto it = _file_cache.find(file_path_string);
		it != _file_cache.end())
	{
		data = it->second;
	}
	else
	{
		if (!read_file(file_path, data))
		{
			error(keyword_location, "could not open included file '" + file_path_string + '\'');
			consume_until(tokenid::end_of_line);
			return;
		}

		_file_cache.emplace(file_path_string, data);
	}

	// Clear out input stack before pushing include so that hidden macros do not bleed into the include
	while (_input_stack.size() > (_next_input_index + 1))
		_input_stack.pop_back();
	push(std::move(data), file_path_string);
}

bool reshadefx::preprocessor::evaluate_expression()
{
	struct rpn_token
	{
		int value;
		bool is_op;
	};

	size_t rpn_index = 0;
	size_t stack_index = 0;
	const size_t STACK_SIZE = 128;
	rpn_token rpn[STACK_SIZE];
	int stack[STACK_SIZE];

	// Keep track of previous token to figure out data type of expression
	tokenid previous_token = _token;

	// Run shunting-yard algorithm
	while (!peek(tokenid::end_of_line))
	{
		if (stack_index >= STACK_SIZE || rpn_index >= STACK_SIZE)
		{
			error(_token.location, "expression evaluator ran out of stack space");
			return false;
		}

		int op = op_none;
		bool is_left_associative = true;
		bool parenthesis_matched = false;

		consume();

		switch (_token)
		{
		case tokenid::space:
			continue;
		case tokenid::backslash:
			// Skip to next line if the line ends with a backslash
			if (accept(tokenid::end_of_line))
				continue;
			else // Otherwise continue on processing the token (it is not valid here, but make that an error below)
				break;
		case tokenid::exclaim:
			op = op_not;
			is_left_associative = false;
			break;
		case tokenid::percent:
			op = op_modulo;
			break;
		case tokenid::ampersand:
			op = op_bitand;
			break;
		case tokenid::star:
			op = op_multiply;
			break;
		case tokenid::plus:
			is_left_associative =
				previous_token == tokenid::int_literal ||
				previous_token == tokenid::uint_literal ||
				previous_token == tokenid::identifier ||
				previous_token == tokenid::parenthesis_close;
			op = is_left_associative ? op_add : op_plus;
			break;
		case tokenid::minus:
			is_left_associative =
				previous_token == tokenid::int_literal ||
				previous_token == tokenid::uint_literal ||
				previous_token == tokenid::identifier ||
				previous_token == tokenid::parenthesis_close;
			op = is_left_associative ? op_subtract : op_negate;
			break;
		case tokenid::slash:
			op = op_divide;
			break;
		case tokenid::less:
			op = op_less;
			break;
		case tokenid::greater:
			op = op_greater;
			break;
		case tokenid::caret:
			op = op_bitxor;
			break;
		case tokenid::pipe:
			op = op_bitor;
			break;
		case tokenid::tilde:
			op = op_bitnot;
			is_left_associative = false;
			break;
		case tokenid::exclaim_equal:
			op = op_not_equal;
			break;
		case tokenid::ampersand_ampersand:
			op = op_and;
			break;
		case tokenid::less_less:
			op = op_leftshift;
			break;
		case tokenid::less_equal:
			op = op_less_equal;
			break;
		case tokenid::equal_equal:
			op = op_equal;
			break;
		case tokenid::greater_greater:
			op = op_rightshift;
			break;
		case tokenid::greater_equal:
			op = op_greater_equal;
			break;
		case tokenid::pipe_pipe:
			op = op_or;
			break;
		default:
			// This is not an operator token
			break;
		}

		switch (_token)
		{
		case tokenid::parenthesis_open:
			stack[stack_index++] = op_parentheses;
			break;
		case tokenid::parenthesis_close:
			parenthesis_matched = false;
			while (stack_index > 0)
			{
				const int op2 = stack[--stack_index];
				if (op2 == op_parentheses)
				{
					parenthesis_matched = true;
					break;
				}

				rpn[rpn_index++] = { op2, true };
			}

			if (!parenthesis_matched)
			{
				error(_token.location, "unmatched ')'");
				return false;
			}
			break;
		case tokenid::identifier:
			if (evaluate_identifier_as_macro())
				continue;

			if (_token.literal_as_string == "exists")
			{
				const bool has_parentheses = accept(tokenid::parenthesis_open);
				while (accept(tokenid::identifier))
				{
					if (!evaluate_identifier_as_macro())
					{
						error(_token.location, "syntax error: unexpected identifier after 'exists'");
						return false;
					}
				}
				if (!expect(tokenid::string_literal))
					return false;
				std::filesystem::path file_name = std::filesystem::u8path(_token.literal_as_string);
				if (has_parentheses && !expect(tokenid::parenthesis_close))
					return false;
				std::filesystem::path file_path = std::filesystem::u8path(_output_location.source);
				file_path.replace_filename(file_name);

				std::error_code ec;
				if (!std::filesystem::exists(file_path, ec))
					for (const std::filesystem::path &include_path : _include_paths)
						if (std::filesystem::exists(file_path = include_path / file_name, ec))
							break;

				rpn[rpn_index++] = { std::filesystem::exists(file_path, ec) ? 1 : 0, false };
				continue;
			}
			if (_token.literal_as_string == "defined")
			{
				const bool has_parentheses = accept(tokenid::parenthesis_open);
				if (!expect(tokenid::identifier))
					return false;
				const std::string macro_name = std::move(_token.literal_as_string);
				if (has_parentheses && !expect(tokenid::parenthesis_close))
					return false;

				rpn[rpn_index++] = { _macros.find(macro_name) != _macros.end() ? 1 : 0, false };
				continue;
			}

			// An identifier that cannot be replaced with a number becomes zero
			rpn[rpn_index++] = { 0, false };
			break;
		case tokenid::int_literal:
		case tokenid::uint_literal:
			rpn[rpn_index++] = { _token.literal_as_int, false };
			break;
		default:
			if (op == op_none)
			{
				error(_token.location, "invalid expression");
				return false;
			}

			while (stack_index > 0)
			{
				const int prev_op = stack[stack_index - 1];
				if (prev_op == op_parentheses)
					break;

				if (is_left_associative ?
					(precedence_lookup[op] > precedence_lookup[prev_op]) :
					(precedence_lookup[op] >= precedence_lookup[prev_op]))
					break;

				stack_index--;
				rpn[rpn_index++] = { prev_op, true };
			}

			stack[stack_index++] = op;
			break;
		}

		previous_token = _token;
	}

	while (stack_index > 0)
	{
		const int op = stack[--stack_index];
		if (op == op_parentheses)
		{
			error(_token.location, "unmatched ')'");
			return false;
		}

		rpn[rpn_index++] = { op, true };
	}

#define UNARY_OPERATION(op) { \
	if (stack_index < 1) \
		return error(_token.location, "invalid expression"), 0; \
	stack[stack_index - 1] = op stack[stack_index - 1]; \
	}
#define BINARY_OPERATION(op) { \
	if (stack_index < 2) \
		return error(_token.location, "invalid expression"), 0; \
	stack[stack_index - 2] = stack[stack_index - 2] op stack[stack_index - 1]; \
	stack_index--; \
	}

	// Evaluate reverse polish notation output
	for (rpn_token *token = rpn; rpn_index--; token++)
	{
		if (token->is_op)
		{
			switch (token->value)
			{
			case op_or:
				BINARY_OPERATION(||);
				break;
			case op_and:
				BINARY_OPERATION(&&);
				break;
			case op_bitor:
				BINARY_OPERATION(|);
				break;
			case op_bitxor:
				BINARY_OPERATION(^);
				break;
			case op_bitand:
				BINARY_OPERATION(&);
				break;
			case op_not_equal:
				BINARY_OPERATION(!=);
				break;
			case op_equal:
				BINARY_OPERATION(==);
				break;
			case op_less:
				BINARY_OPERATION(<);
				break;
			case op_greater:
				BINARY_OPERATION(>);
				break;
			case op_less_equal:
				BINARY_OPERATION(<=);
				break;
			case op_greater_equal:
				BINARY_OPERATION(>=);
				break;
			case op_leftshift:
				BINARY_OPERATION(<<);
				break;
			case op_rightshift:
				BINARY_OPERATION(>>);
				break;
			case op_add:
				BINARY_OPERATION(+);
				break;
			case op_subtract:
				BINARY_OPERATION(-);
				break;
			case op_modulo:
				BINARY_OPERATION(%);
				break;
			case op_divide:
				BINARY_OPERATION(/);
				break;
			case op_multiply:
				BINARY_OPERATION(*);
				break;
			case op_plus:
				UNARY_OPERATION(+);
				break;
			case op_negate:
				UNARY_OPERATION(-);
				break;
			case op_not:
				UNARY_OPERATION(!);
				break;
			case op_bitnot:
				UNARY_OPERATION(~);
				break;
			}
		}
		else
		{
			stack[stack_index++] = token->value;
		}
	}

	if (stack_index != 1)
	{
		error(_token.location, "invalid expression");
		return false;
	}

	return stack[0] != 0;
}

bool reshadefx::preprocessor::evaluate_identifier_as_macro()
{
	if (_token.literal_as_string == "__LINE__")
	{
		push(std::to_string(_token.location.line));
		return true;
	}
	if (_token.literal_as_string == "__FILE__")
	{
		push(escape_string(_token.location.source));
		return true;
	}
	if (_token.literal_as_string == "__FILE_STEM__")
	{
		const std::filesystem::path file_stem = std::filesystem::u8path(_token.location.source).stem();
		push(escape_string(file_stem.u8string()));
		return true;
	}
	if (_token.literal_as_string == "__FILE_NAME__")
	{
		const std::filesystem::path file_name = std::filesystem::u8path(_token.location.source).filename();
		push(escape_string(file_name.u8string()));
		return true;
	}

	const auto it = _macros.find(_token.literal_as_string);
	if (it == _macros.end())
		return false;

	const std::unordered_set<std::string> &hidden_macros = _input_stack[_current_input_index].hidden_macros;
	if (hidden_macros.find(_token.literal_as_string) != hidden_macros.end())
		return false;

	if (_recursion_count++ >= 256)
	{
		error(_token.location, "macro recursion too high");
		return false;
	}

	std::vector<std::string> arguments;
	if (it->second.is_function_like)
	{
		if (!accept(tokenid::parenthesis_open))
			return false;

		while (true)
		{
			int parentheses_level = 0;
			std::string argument;

			while (true)
			{
				// Consume all tokens here, so spaces are added to the output too
				consume();
				if (_token == tokenid::comma && parentheses_level == 0)
					break; // Comma marks end of an argument
				if (_token == tokenid::parenthesis_open)
					parentheses_level++;
				if (_token == tokenid::parenthesis_close && --parentheses_level < 0)
					break;

				// Collapse all whitespace down to a single space
				if (_token == tokenid::space)
					argument += ' ';
				else
					argument += _current_token_raw_data;
			}

			// Trim whitespace from argument
			const size_t first = argument.find_first_not_of(" \t");
			if (first == std::string::npos)
			{
				argument.clear();
			}
			else
			{
				const size_t last = argument.find_last_not_of(" \t");
				argument = argument.substr(first, last - first + 1);
			}
			arguments.push_back(std::move(argument));

			if (parentheses_level < 0)
				break;
		}
	}

	std::string input;
	expand_macro(it->first, it->second, arguments, input);

	if (!input.empty())
	{
		push(std::move(input));

		_input_stack[_current_input_index].hidden_macros.insert(it->first);
	}

	return true;
}

void reshadefx::preprocessor::expand_macro(const std::string &name, const macro &macro, const std::vector<std::string> &arguments, std::string &out)
{
	for (size_t offset = 0; offset < macro.replacement_list.size(); ++offset)
	{
		if (macro.replacement_list[offset] != macro_replacement_start)
		{
			out += macro.replacement_list[offset];
			continue;
		}

		// This is a special replacement sequence
		const auto type = macro.replacement_list[++offset];
		if (type == macro_replacement_concat)
		{
			// Remove any whitespace preceeding or following the concatenation operator (so "a ## b" becomes "ab")
			if (const size_t last = out.find_last_not_of(" \t");
				last != std::string::npos && last + 1 < out.size())
				out.erase(last + 1);
			while (offset + 1 != macro.replacement_list.size() &&
				(macro.replacement_list[offset + 1] == ' ' || macro.replacement_list[offset + 1] == '\t'))
				++offset;
			continue;
		}

		const auto index = macro.replacement_list[++offset];
		if (static_cast<size_t>(index) >= arguments.size())
		{
			warning(_token.location, "not enough arguments for function-like macro invocation '" + name + "'");
			continue;
		}

		switch (type)
		{
		case macro_replacement_stringize:
			out.reserve(out.size() + 2 + arguments[index].size());
			out += '"';
			for (const char c : arguments[index])
			{
				// Adds backslashes to escape quotes
				if (c == '"')
					out += '\\';
				out += c;
			}
			out += '"';
			break;
		case macro_replacement_argument:
			push(arguments[index] + static_cast<char>(macro_replacement_argument));
			while (true)
			{
				// Consume all tokens here, so spaces are added to the output too
				consume();
				if (_token == tokenid::unknown) // 'macro_replacement_argument' is 'tokenid::unknown'
					break;
				if (_token == tokenid::identifier && evaluate_identifier_as_macro())
					continue;
				out += _current_token_raw_data;
			}
			assert(_current_token_raw_data[0] == macro_replacement_argument);
			break;
		}
	}
}
void reshadefx::preprocessor::create_macro_replacement_list(macro &macro)
{
	// Since the number of parameters is encoded in the string, it may not exceed the available size of a char
	if (macro.parameters.size() >= std::numeric_limits<unsigned char>::max())
		return error(_token.location, "too many macro parameters");

	while (!peek(tokenid::end_of_file) && !peek(tokenid::end_of_line))
	{
		consume();
		switch (_token)
		{
		case tokenid::hash:
			if (accept(tokenid::hash))
			{
				if (peek(tokenid::end_of_line))
				{
					error(_token.location, "## cannot appear at end of macro text");
					return;
				}

				// Start a ## token concatenation operator
				macro.replacement_list += macro_replacement_start;
				macro.replacement_list += macro_replacement_concat;
				continue;
			}
			else if (macro.is_function_like)
			{
				if (!expect(tokenid::identifier))
					return;

				const auto it = std::find(macro.parameters.begin(), macro.parameters.end(), _token.literal_as_string);
				if (it == macro.parameters.end())
					return error(_token.location, "# must be followed by parameter name");

				// Start a # stringize operator
				macro.replacement_list += macro_replacement_start;
				macro.replacement_list += macro_replacement_stringize;
				macro.replacement_list += static_cast<char>(std::distance(macro.parameters.begin(), it));
				continue;
			}
			break;
		case tokenid::backslash:
			if (peek(tokenid::end_of_line))
			{
				consume();
				continue;
			}
			break;
		case tokenid::identifier:
			if (const auto it = std::find(macro.parameters.begin(), macro.parameters.end(), _token.literal_as_string);
				it != macro.parameters.end())
			{
				macro.replacement_list += macro_replacement_start;
				macro.replacement_list += macro_replacement_argument;
				macro.replacement_list += static_cast<char>(std::distance(macro.parameters.begin(), it));
				continue;
			}
			break;
		default:
			// Token needs no special handling, raw data is added to macro below
			break;
		}

		macro.replacement_list += _current_token_raw_data;
	}
}
