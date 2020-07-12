/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <array>
#include <string>
#include <vector>
#include <unordered_map>

class imgui_code_editor
{
public:
	struct text_pos
	{
		size_t line, column;

		text_pos() : line(0), column(0) {}
		text_pos(size_t line, size_t column = 0) : line(line), column(column) {}

		bool operator==(const text_pos &o) const { return line == o.line && column == o.column; }
		bool operator!=(const text_pos &o) const { return line != o.line || column != o.column; }
		bool operator< (const text_pos &o) const { return line != o.line ? line < o.line : column <  o.column; }
		bool operator<=(const text_pos &o) const { return line != o.line ? line < o.line : column <= o.column; }
		bool operator> (const text_pos &o) const { return line != o.line ? line > o.line : column >  o.column; }
		bool operator>=(const text_pos &o) const { return line != o.line ? line > o.line : column >= o.column; }
	};

	enum color
	{
		color_default,
		color_keyword,
		color_number_literal,
		color_string_literal,
		color_punctuation,
		color_preprocessor,
		color_identifier,
		color_known_identifier,
		color_preprocessor_identifier,
		color_comment,
		color_multiline_comment,
		color_background,
		color_cursor,
		color_selection,
		color_error_marker,
		color_warning_marker,
		color_line_number,
		color_current_line_fill,
		color_current_line_fill_inactive,
		color_current_line_edge,

		color_palette_max
	};

	enum class selection_mode
	{
		normal,
		word,
		line
	};

	imgui_code_editor();

	void render(const char *title, bool border = false);

	void select(const text_pos &beg, const text_pos &end, selection_mode mode = selection_mode::normal);
	void select_all();
	bool has_selection() const { return _select_end > _select_beg; }

	void set_text(const std::string &text);
	void clear_text();
	void insert_text(const std::string &text);
	std::string get_text() const;
	std::string get_text(const text_pos &beg, const text_pos &end) const;
	std::string get_selected_text() const;

	void undo(unsigned int steps = 1);
	bool can_undo() const { return _undo_index > 0; }
	void redo(unsigned int steps = 1);
	bool can_redo() const { return _undo_index < _undo.size(); }

	void add_error(size_t line, const std::string &message, bool warning = false) { _errors.insert({ line, { message, warning } }); }
	void clear_errors() { _errors.clear(); }

	void set_readonly(bool state) { _readonly = state; }
	void set_tab_size(unsigned short size) { _tab_size = size; }
	void set_left_margin(float margin) { _left_margin = margin; }
	void set_line_spacing(float spacing) { _line_spacing = spacing; }

	void set_palette(const std::array<uint32_t, color_palette_max> &palette) { _palette = palette; }
	uint32_t &get_palette_index(unsigned int index) { return _palette[index]; }
	const std::array<uint32_t, color_palette_max> &get_palette() const { return _palette; }
	static const char *get_palette_color_name(unsigned int index);

	bool find_and_scroll_to_text(const std::string &text, bool backwards = false, bool with_selection = false);

private:
	struct glyph
	{
		char c = '\0';
		color col = color_default;
	};

	struct undo_record
	{
		text_pos added_beg;
		text_pos added_end;
		std::string added;
		text_pos removed_beg;
		text_pos removed_end;
		std::string removed;
	};

	void record_undo(undo_record &&record);

	void insert_character(char c, bool auto_indent);

	void delete_next();
	void delete_previous();
	void delete_selection();
	void delete_lines(size_t first_line, size_t last_line);

	void clipboard_copy();
	void clipboard_cut();
	void clipboard_paste();

	void move_up(size_t amount = 1, bool select = false);
	void move_down(size_t amount = 1, bool select = false);
	void move_left(size_t amount = 1, bool select = false, bool word_mode = false);
	void move_right(size_t amount = 1, bool select = false, bool word_mode = false);
	void move_top(bool select = false);
	void move_bottom(bool select = false);
	void move_home(bool select = false);
	void move_end(bool select = false);
	void move_lines_up();
	void move_lines_down();

	void colorize();

	float _left_margin = 10.0f;
	float _line_spacing = 1.0f;
	unsigned short _tab_size = 4;
	bool _readonly = false;
	bool _overwrite = false;
	bool _scroll_to_cursor = false;
	float _cursor_anim = 0.0f;
	double _last_click_time = -1.0;

	std::vector<std::vector<glyph>> _lines;
	std::unordered_map<size_t, std::pair<std::string, bool>> _errors;

	text_pos _cursor_pos;
	text_pos _select_beg;
	text_pos _select_end;
	text_pos _interactive_beg;
	text_pos _interactive_end;
	std::string _highlighted;

	char _search_text[256] = "";
	char _replace_text[256] = "";
	bool _show_search_popup = false;
	bool _search_case_sensitive = false;
	unsigned int _search_window_open = 0;
	unsigned int _search_window_focus = 0;

	size_t _undo_index = 0;
	std::vector<undo_record> _undo;
	bool _in_undo_operation = false;

	size_t _colorize_line_beg = 0;
	size_t _colorize_line_end = 0;
	std::array<uint32_t, color_palette_max> _palette;
};
