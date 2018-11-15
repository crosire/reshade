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
		size_t line, column;

		text_pos() : line(0), column(0) {}
		text_pos(size_t line, size_t column = 0) : line(line), column(column) {}

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

	void undo(unsigned int steps = 1);
	bool can_undo() const { return _undo_index > 0; }
	void redo(unsigned int steps = 1);
	bool can_redo() const { return _undo_index < _undo.size(); }

	void add_error(size_t line, const std::string &message, bool warning = false) { _errors.insert({ line, { message, warning } }); }
	void clear_errors() { _errors.clear(); }

	void set_tab_size(unsigned int size) { _tab_size = size; }
	void set_left_margin(float margin) { _left_margin = margin; }
	void set_line_spacing(float spacing) { _line_spacing = spacing; }
	void set_palette(const std::array<uint32_t, 20> &colors) { _palette = colors; }

private:
	struct glyph
	{
		char c = '\0';
		uint8_t col = 0;
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

	void colorize();

	float _left_margin = 10.0f;
	float _line_spacing = 1.0f;
	unsigned int _tab_size = 4;
	bool _overwrite = false;
	bool _scroll_to_cursor = false;
	float _cursor_anim = 0.0f;
	double _last_click_time = -1.0;

	std::array<uint32_t, 20> _palette;

	std::vector<std::vector<glyph>> _lines;

	text_pos _cursor_pos;
	text_pos _select_beg;
	text_pos _select_end;
	text_pos _interactive_beg;
	text_pos _interactive_end;
	std::string _highlighted;

	size_t _undo_index = 0;
	std::vector<undo_record> _undo;
	bool _in_undo_operation = false;

	size_t _colorize_line_beg = 0;
	size_t _colorize_line_end = 0;

	std::unordered_map<size_t, std::pair<std::string, bool>> _errors;
};
