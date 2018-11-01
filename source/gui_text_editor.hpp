/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <array>
#include <string>
#include <vector>
#include <unordered_map>

class code_editor_widget
{
public:
	struct text_pos
	{
		int line, column;

		text_pos() : line(0), column(0) {}
		text_pos(int line, int column = 0) : line(line), column(column) {}

		bool operator ==(const text_pos& o) const { return line == o.line && column == o.column; }
		bool operator !=(const text_pos& o) const { return line != o.line || column != o.column; }
		bool operator < (const text_pos& o) const { return line != o.line ? line < o.line : column <  o.column; }
		bool operator <=(const text_pos& o) const { return line != o.line ? line < o.line : column <= o.column; }
		bool operator > (const text_pos& o) const { return line != o.line ? line > o.line : column >  o.column; }
		bool operator >=(const text_pos& o) const { return line != o.line ? line > o.line : column >= o.column; }
	};

	enum class selection_mode
	{
		normal,
		word,
		line
	};

	code_editor_widget();

	void render(const char *title, bool border = false);

	void select(const text_pos &beg, const text_pos &end, selection_mode mode = selection_mode::normal);
	void select_all();
	bool has_selection() const { return _select_end > _select_beg; }

	void set_text(const std::string &text);
	void insert_text(const std::string &text);
	std::string get_text() const;
	std::string get_text(const text_pos &beg, const text_pos &end) const;
	std::string get_selected_text() const;
	std::string get_current_line_text() const;

	void add_error(int line, const std::string &message)
	{
		_errors.insert({ line, message });
	}

	void set_tab_size(int size) { _tab_size = size; }
	void set_left_margin(float margin) { _left_margin = margin; }
	void set_line_spacing(float spacing) { _line_spacing = spacing; }

private:
	struct glyph
	{
		char c = '\0';
		uint8_t col = 0;
	};

	void insert_character(char c, bool shift);
	std::vector<glyph> &insert_line(int line_pos);

	void delete_next();
	void delete_previous();
	void delete_selection();
	void delete_range(const text_pos &beg, const text_pos &end);
	void delete_lines(int first_line, int last_line);

	void clipboard_copy();
	void clipboard_cut();
	void clipboard_paste();

	void move_up(unsigned int amount = 1, bool select = false);
	void move_down(unsigned int amount = 1, bool select = false);
	void move_left(unsigned int amount = 1, bool select = false, bool word_mode = false);
	void move_right(unsigned int amount = 1, bool select = false, bool word_mode = false);
	void move_top(bool select = false);
	void move_bottom(bool select = false);
	void move_home(bool select = false);
	void move_end(bool select = false);

	void colorize();

	std::vector<std::vector<glyph>> _lines;
	std::array<uint32_t, 20> _palette;
	std::unordered_map<int, std::string> _errors;
	int _tab_size = 4;
	float _left_margin = 10.0f;
	float _line_spacing = 1.0f;
	bool _overwrite = false;
	bool _scroll_to_cursor = false;
	float _cursor_anim = 0.0f;
	float _last_click_time = -1.0f;
	text_pos _cursor_pos;
	text_pos _select_beg;
	text_pos _select_end;
	text_pos _interactive_beg;
	text_pos _interactive_end;
	int _colorize_line_beg = 0;
	int _colorize_line_end = 0;
};
