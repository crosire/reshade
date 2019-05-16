/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_preprocessor.hpp"
#include <assert.h>

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
bool reshadefx::preprocessor::add_macro_definition(const std::string &name, std::string value)
{
	macro macro;
	macro.replacement_list = std::move(value);

	return add_macro_definition(name, macro);
}

bool reshadefx::preprocessor::append_file(const std::filesystem::path &path)
{
	struct _stat64 st {};

	FILE *file = nullptr;
	if (_wstati64(path.wstring().c_str(), &st) != 0 || _wfopen_s(&file, path.wstring().c_str(), L"rb") != 0)
		return false;

	std::vector<char> filebuffer(static_cast<size_t>(st.st_size) + 1);

	// Read file contents into a string
	size_t eof = fread_s(filebuffer.data(), filebuffer.size(), 1, static_cast<size_t>(st.st_size), file);

	// Append a new line feed to the end of the input string to avoid issues with parsing
	filebuffer[eof] = '\n';

	// No longer need to have a handle open to the file, since all data was read to a string, so can safely close it
	fclose(file);

	std::string_view filedata(filebuffer.data(), filebuffer.size());

	// Remove BOM (0xefbbbf means 0xfeff)
	if (filedata.size() >= 3 &&
		static_cast<unsigned char>(filedata[0]) == 0xef &&
		static_cast<unsigned char>(filedata[1]) == 0xbb &&
		static_cast<unsigned char>(filedata[2]) == 0xbf)
		filedata = std::string_view(filedata.data() + 3, filedata.size() - 3);

	_success = true;
	push(std::string(filedata), path.u8string());
	parse();

	return _success;
}
bool reshadefx::preprocessor::append_string(const std::string &source_code)
{
	// Enforce all input strings to end with a line feed
	assert(!source_code.empty() && source_code.back() == '\n');

	_success = true;
	push(source_code);
	parse();

	return _success;
}

void reshadefx::preprocessor::error(const location &location, const std::string &message)
{
	_errors += location.source + '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": preprocessor error: " + message + '\n';
	_success = false;
}
void reshadefx::preprocessor::warning(const location &location, const std::string &message)
{
	_errors += location.source + '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": preprocessor warning: " + message + '\n';
}

reshadefx::lexer &reshadefx::preprocessor::current_lexer()
{
	assert(!_input_stack.empty());

	return *_input_stack.top().lexer;
}
std::stack<reshadefx::preprocessor::if_level> &reshadefx::preprocessor::current_if_stack()
{
	assert(!_input_stack.empty());

	return _input_stack.top().if_stack;
}

void reshadefx::preprocessor::push(std::string input, const std::string &name)
{
	input_level level = {};
	level.name = name;
	level.lexer.reset(new lexer(std::move(input), true, false, false, false, true, false));
	level.parent = _input_stack.empty() ? nullptr : &_input_stack.top();
	level.next_token.id = tokenid::unknown;

	if (name.empty())
	{
		if (level.parent != nullptr)
			level.name = level.parent->name;
	}
	else
	{
		_output_location.source = name;

		_output += "#line 1 \"" + name + "\"\n";
	}

	_input_stack.push(std::move(level));

	consume();
}

bool reshadefx::preprocessor::peek(tokenid token) const
{
	assert(!_input_stack.empty());

	return _input_stack.top().next_token == token;
}
void reshadefx::preprocessor::consume()
{
	assert(!_input_stack.empty());

	auto &input_level = _input_stack.top();
	const auto &input_string = input_level.lexer->input_string();

	_token = std::move(input_level.next_token);
	_token.location.source = _output_location.source;
	_current_token_raw_data = input_string.substr(_token.offset, _token.length);

	// Get the next token
	input_level.next_token =
		input_level.lexer->lex();

	// Pop input level if lexical analysis has reached the end of it
	while (_input_stack.top().next_token == tokenid::end_of_file)
	{
		if (!current_if_stack().empty())
			error(current_if_stack().top().token.location, "unterminated #if");

		_input_stack.pop();

		if (_input_stack.empty())
			break;

		const auto &top = _input_stack.top();

		if (top.name != _output_location.source)
		{
			_output_location.line = 1;
			_output_location.source = top.name;

			_output += "#line 1 \"" + top.name + "\"\n";
		}
	}
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
		assert(!_input_stack.empty());

		auto actual_token = _input_stack.top().next_token;
		actual_token.location.source = _output_location.source;

		error(actual_token.location, "syntax error: unexpected token '" + current_lexer().input_string().substr(actual_token.offset, actual_token.length) + "'");

		return false;
	}

	return true;
}

void reshadefx::preprocessor::parse()
{
	std::string line;

	while (!_input_stack.empty())
	{
		_recursion_count = 0;

		const bool skip = !current_if_stack().empty() && current_if_stack().top().skipping;

		consume();

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
			error(_token.location, "unrecognized preprocessing directive '" + _token.literal_as_string + "'");
			consume_until(tokenid::end_of_line);
			continue;
		case tokenid::end_of_line:
			if (line.empty())
				continue;
			if (++_output_location.line != _token.location.line)
				_output += "#line " + std::to_string(_output_location.line = _token.location.line) + '\n';
			_output += line;
			_output += '\n';
			line.clear();
			continue;
		case tokenid::identifier:
			if (evaluate_identifier_as_macro())
				continue;
		default:
			line += _current_token_raw_data;
			break;
		}
	}

	_output += line;
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

	if (current_lexer().input_string()[macro_name_end_offset] == '(')
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
	const bool condition_result = evaluate_expression();

	if_level level;
	level.token = _token;
	level.value = condition_result;
	level.parent = current_if_stack().empty() ? nullptr : &current_if_stack().top();
	level.skipping = (level.parent != nullptr && level.parent->skipping) || !level.value;

	current_if_stack().push(level);
}
void reshadefx::preprocessor::parse_ifdef()
{
	if_level level;
	level.token = _token;

	if (!expect(tokenid::identifier))
		return;

	level.value = _macros.find(_token.literal_as_string) != _macros.end();
	level.parent = current_if_stack().empty() ? nullptr : &current_if_stack().top();
	level.skipping = (level.parent != nullptr && level.parent->skipping) || !level.value;

	current_if_stack().push(level);
}
void reshadefx::preprocessor::parse_ifndef()
{
	if_level level;
	level.token = _token;

	if (!expect(tokenid::identifier))
		return;

	level.value = _macros.find(_token.literal_as_string) == _macros.end();
	level.parent = current_if_stack().empty() ? nullptr : &current_if_stack().top();
	level.skipping = (level.parent != nullptr && level.parent->skipping) || !level.value;

	current_if_stack().push(level);
}
void reshadefx::preprocessor::parse_elif()
{
	if (current_if_stack().empty())
		return error(_token.location, "missing #if for #elif");

	if_level &level = current_if_stack().top();

	if (level.token == tokenid::hash_else)
		return error(_token.location, "#elif is not allowed after #else");

	const bool condition_result = evaluate_expression();

	level.token = _token;
	level.skipping = (level.parent != nullptr && level.parent->skipping) || level.value || !condition_result;

	if (!level.value) level.value = condition_result;
}
void reshadefx::preprocessor::parse_else()
{
	if (current_if_stack().empty())
		return error(_token.location, "missing #if for #else");

	if_level &level = current_if_stack().top();

	if (level.token == tokenid::hash_else)
		return error(_token.location, "#else is not allowed after #else");

	level.token = _token;
	level.skipping = (level.parent != nullptr && level.parent->skipping) || level.value;

	if (!level.value) level.value = true;
}
void reshadefx::preprocessor::parse_endif()
{
	if (current_if_stack().empty())
		return error(_token.location, "missing #if for #endif");

	current_if_stack().pop();
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
		if (const auto it = _filecache.find(_output_location.source); it != _filecache.end())
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

	const std::filesystem::path filename = std::move(_token.literal_as_string);

	std::filesystem::path filepath = _output_location.source;
	filepath.replace_filename(filename);

	struct _stat64 st {};
	if (_wstati64(filepath.wstring().c_str(), &st) != 0)
		for (const auto &include_path : _include_paths)
			if (filepath = include_path / filename, _wstati64(filepath.wstring().c_str(), &st) == 0)
				break;

	auto it = _filecache.find(filepath.u8string());

	if (it == _filecache.end())
	{
		FILE *file = nullptr;
		if (_wfopen_s(&file, filepath.wstring().c_str(), L"rb") != 0)
		{
			error(keyword_location, "could not open included file '" + filepath.u8string() + "'");
			consume_until(tokenid::end_of_line);
			return;
		}

		std::vector<char> filebuffer(static_cast<size_t>(st.st_size) + 1);

		// Read file contents into a string
		size_t eof = fread_s(filebuffer.data(), filebuffer.size(), 1, static_cast<size_t>(st.st_size), file);

		// Append a new line feed to the end of the input string to avoid issues with parsing
		filebuffer[eof] = '\n';

		// No longer need to have a handle open to the file, since all data was read to a string, so can safely close it
		fclose(file);

		std::string_view filedata(filebuffer.data(), filebuffer.size());

		// Remove BOM (0xefbbbf means 0xfeff)
		if (filedata.size() >= 3 &&
			static_cast<unsigned char>(filedata[0]) == 0xef &&
			static_cast<unsigned char>(filedata[1]) == 0xbb &&
			static_cast<unsigned char>(filedata[2]) == 0xbf)
			filedata = std::string_view(filedata.data() + 3, filedata.size() - 3);

		it = _filecache.emplace(filepath.u8string(), std::string(filedata)).first;
	}

	push(it->second, filepath.u8string());
}

bool reshadefx::preprocessor::evaluate_expression()
{
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
	struct rpn_token
	{
		bool is_op;
		int value;
	};

	int stack[128];
	rpn_token rpn[128];
	size_t stack_count = 0, rpn_count = 0;
	tokenid previous_token = _token;
	const int precedence[] = { 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 8, 8, 9, 9, 10, 10, 10, 11, 11, 11, 11 };

	// Run shunting-yard algorithm
	while (!peek(tokenid::end_of_line))
	{
		if (stack_count >= _countof(stack) || rpn_count >= _countof(rpn))
		{
			error(_token.location, "expression evaluator ran out of stack space");
			return false;
		}
		int op = op_none;
		bool is_left_associative = true;

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
		}

		switch (_token)
		{
			case tokenid::parenthesis_open:
				stack[stack_count++] = op_parentheses;
				break;
			case tokenid::parenthesis_close:
			{
				bool matched = false;

				while (stack_count > 0)
				{
					const int op2 = stack[--stack_count];

					if (op2 == op_parentheses)
					{
						matched = true;
						break;
					}

					rpn[rpn_count].is_op = true;
					rpn[rpn_count++].value = op2;
				}

				if (!matched)
				{
					error(_token.location, "unmatched ')'");
					return false;
				}
				break;
			}
			case tokenid::identifier:
			{
				if (evaluate_identifier_as_macro())
				{
					continue;
				}
				else if (_token.literal_as_string == "exists")
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

					const std::filesystem::path filename = std::move(_token.literal_as_string);

					if (has_parentheses && !expect(tokenid::parenthesis_close))
						return false;

					std::error_code ec;
					std::filesystem::path filepath = _output_location.source;
					filepath.replace_filename(filename);

					if (!std::filesystem::exists(filepath, ec))
						for (const auto &include_path : _include_paths)
							if (std::filesystem::exists(filepath = include_path / filename, ec))
								break;

					rpn[rpn_count].is_op = false;
					rpn[rpn_count++].value = std::filesystem::exists(filepath, ec);
					continue;
				}
				else if (_token.literal_as_string == "defined")
				{
					const bool has_parentheses = accept(tokenid::parenthesis_open);

					if (!expect(tokenid::identifier))
						return false;

					const bool is_macro_defined = _macros.find(_token.literal_as_string) != _macros.end();

					if (has_parentheses && !expect(tokenid::parenthesis_close))
						return false;

					rpn[rpn_count].is_op = false;
					rpn[rpn_count++].value = is_macro_defined;
					continue;
				}

				// An identifier that cannot be replaced with a number becomes zero
				rpn[rpn_count].is_op = false;
				rpn[rpn_count++].value = 0;
				break;
			}
			case tokenid::int_literal:
			case tokenid::uint_literal:
			{
				rpn[rpn_count].is_op = false;
				rpn[rpn_count++].value = _token.literal_as_int;
				break;
			}
			default:
			{
				if (op == op_none)
				{
					error(_token.location, "invalid expression");
					return false;
				}

				const int precedence1 = precedence[op];

				while (stack_count > 0)
				{
					const int op2 = stack[stack_count - 1];

					if (op2 == op_parentheses)
						break;

					const int precedence2 = precedence[op2];

					if ((is_left_associative && (precedence1 <= precedence2)) || (!is_left_associative && (precedence1 < precedence2)))
					{
						stack_count--;
						rpn[rpn_count].is_op = true;
						rpn[rpn_count++].value = op2;
					}
					else
					{
						break;
					}
				}

				stack[stack_count++] = op;
				break;
			}
		}

		previous_token = _token;
	}

	while (stack_count > 0)
	{
		const int op = stack[--stack_count];

		if (op == op_parentheses)
		{
			error(_token.location, "unmatched ')'");
			return false;
		}

		rpn[rpn_count].is_op = true;
		rpn[rpn_count++].value = static_cast<int>(op);
	}

	// Evaluate reverse polish notation output
	for (rpn_token *token = rpn; rpn_count-- != 0; token++)
	{
		if (token->is_op)
		{
#define UNARY_OPERATION(op) { \
	if (stack_count < 1) \
		return error(_token.location, "invalid expression"), 0; \
	stack[stack_count - 1] = op stack[stack_count - 1]; \
	}
#define BINARY_OPERATION(op) { \
	if (stack_count < 2) \
		return error(_token.location, "invalid expression"), 0; \
	stack[stack_count - 2] = stack[stack_count - 2] op stack[stack_count - 1]; \
	stack_count--; \
	}
			switch (token->value)
			{
				case op_or: BINARY_OPERATION(||); break;
				case op_and: BINARY_OPERATION(&&); break;
				case op_bitor: BINARY_OPERATION(|); break;
				case op_bitxor: BINARY_OPERATION(^); break;
				case op_bitand: BINARY_OPERATION(&); break;
				case op_not_equal: BINARY_OPERATION(!=); break;
				case op_equal: BINARY_OPERATION(==); break;
				case op_less: BINARY_OPERATION(<); break;
				case op_greater: BINARY_OPERATION(>); break;
				case op_less_equal: BINARY_OPERATION(<=); break;
				case op_greater_equal: BINARY_OPERATION(>=); break;
				case op_leftshift: BINARY_OPERATION(<<); break;
				case op_rightshift: BINARY_OPERATION(>>); break;
				case op_add: BINARY_OPERATION(+); break;
				case op_subtract: BINARY_OPERATION(-); break;
				case op_modulo: BINARY_OPERATION(%); break;
				case op_divide: BINARY_OPERATION(/); break;
				case op_multiply: BINARY_OPERATION(*); break;
				case op_plus: UNARY_OPERATION(+); break;
				case op_negate: UNARY_OPERATION(-); break;
				case op_not: UNARY_OPERATION(!); break;
				case op_bitnot: UNARY_OPERATION(~); break;
			}
		}
		else
		{
			stack[stack_count++] = token->value;
		}
	}

	if (stack_count != 1)
	{
		error(_token.location, "invalid expression");
		return false;
	}

	return stack[0] != 0;
}
bool reshadefx::preprocessor::evaluate_identifier_as_macro()
{
	if (_recursion_count++ >= 256)
	{
		error(_token.location, "macro recursion too high");
		return false;
	}

	const auto it = _macros.find(_token.literal_as_string);

	if (it == _macros.end())
		return false;

	const auto &macro = it->second;
	std::vector<std::string> arguments;

	if (macro.is_function_like)
	{
		if (!accept(tokenid::parenthesis_open))
			return false;

		while (true)
		{
			int parentheses_level = 0;
			std::string argument;

			while (true)
			{
				consume();

				if (_token == tokenid::parenthesis_open)
					parentheses_level++;
				else if (
					(_token == tokenid::parenthesis_close && --parentheses_level < 0) ||
					(_token == tokenid::comma && parentheses_level == 0))
					break;

				argument += _current_token_raw_data;
			}

			if (!argument.empty() && argument.back() == ' ')
				argument.pop_back();
			if (!argument.empty() && argument.front() == ' ')
				argument.erase(0, 1);

			arguments.push_back(argument);

			if (parentheses_level < 0)
				break;
		}
	}

	std::string input;
	expand_macro(it->second, arguments, input);

	push(input);

	return true;
}

void reshadefx::preprocessor::expand_macro(const macro &macro, const std::vector<std::string> &arguments, std::string &out)
{
	for (auto it = macro.replacement_list.begin(); it != macro.replacement_list.end(); ++it)
	{
		if (*it != macro_replacement_start)
		{
			out += *it;
			continue;
		}

		switch (*++it)
		{
		case macro_replacement_concat:
			continue;
		case macro_replacement_stringize:
			out += '"' + arguments.at(*++it) + '"';
			break;
		case macro_replacement_argument:
			push(arguments.at(*++it) + static_cast<char>(macro_replacement_argument));
			while (!accept(tokenid::unknown))
			{
				consume();
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
			{
				if (accept(tokenid::hash))
				{
					if (peek(tokenid::end_of_line))
					{
						error(_token.location, "## cannot appear at end of macro text");
						return;
					}

					// the ## token concatenation operator
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

					// the # stringize operator
					macro.replacement_list += macro_replacement_start;
					macro.replacement_list += macro_replacement_stringize;
					macro.replacement_list += static_cast<char>(std::distance(macro.parameters.begin(), it));
					continue;
				}
				break;
			}
			case tokenid::backslash:
			{
				if (peek(tokenid::end_of_line))
				{
					consume();
					continue;
				}
				break;
			}
			case tokenid::identifier:
			{
				const auto it = std::find(macro.parameters.begin(), macro.parameters.end(), _token.literal_as_string);

				if (it != macro.parameters.end())
				{
					macro.replacement_list += macro_replacement_start;
					macro.replacement_list += macro_replacement_argument;
					macro.replacement_list += static_cast<char>(std::distance(macro.parameters.begin(), it));
					continue;
				}
				break;
			}
		}

		macro.replacement_list += _current_token_raw_data;
	}
}
