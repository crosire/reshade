/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <array>
#include <string>
#include <vector>
#include <unordered_map>

struct ImFont;

namespace reshade::gui
{
	/// <summary>
	/// A text editor for ImGui with syntax highlighting.
	/// </summary>
	class code_editor
	{
	public:
		/// <summary>
		/// Position of a character in the text currently being edited.
		/// </summary>
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

		code_editor();

		/// <summary>
		/// Performs all input logic and renders this text editor to the current ImGui window.
		/// </summary>
		/// <param name="title">Name of the child window the editor is rendered into.</param>
		/// <param name="border">Set to <c>true</c> to surround the child window with a border line.</param>
		/// <param name="font">The font used for rendering the text (<c>nullptr</c> to use the default).</param>
		void render(const char *title, bool border = false, ImFont *font = nullptr);

		/// <summary>
		/// Sets the selection to be between the specified <paramref name="beg"/>in and <paramref name="end"/> positions.
		/// </summary>
		/// <param name="beg">First character to be in the selection.</param>
		/// <param name="end">Last character to be in the selection.</param>
		/// <param name="mode">Changes selection behavior to snap to character, word or line boundaries.</param>
		void select(const text_pos &beg, const text_pos &end, selection_mode mode = selection_mode::normal);
		/// <summary>
		/// Sets the selection to contain the entire text.
		/// </summary>
		void select_all();
		/// <summary>
		/// Returns whether a selection is currently active.
		/// </summary>
		bool has_selection() const { return _select_end > _select_beg; }

		/// <summary>
		/// Searches the text of this text editor for the specific search <paramref name="text"/>, starting at the cursor and scrolls to its position when found.
		/// </summary>
		/// <param name="text">The text to find.</param>
		/// <param name="backwards">Set to <c>true</c> to search in reverse direction, otherwise searches forwards.</param>
		/// <param name="with_selection">Set to <c>true</c> to start search at selection boundaries, rather than the cursor position.</param>
		/// <returns><c>true</c> when the search <paramref name="text"/> was found, <c>false</c> otherwise.</returns>
		bool find_and_scroll_to_text(const std::string &text, bool backwards = false, bool with_selection = false);

		/// <summary>
		/// Replaces the text of this text editor with the specified string.
		/// </summary>
		void set_text(const std::string &text);
		/// <summary>
		/// Clears the text of this text editor to an empty string.
		/// </summary>
		void clear_text();
		/// <summary>
		/// Inserts the specified <paramref name="text"/> at the cursor position.
		/// </summary>
		void insert_text(const std::string &text);
		/// <summary>
		/// Returns the entire text of this text editor as a string.
		/// </summary>
		std::string get_text() const;
		/// <summary>
		/// Returns the text between the specified <paramref name="beg"/>in and <paramref name="end"/> positions.
		/// </summary>
		/// <param name="beg">First character to be in the return string.</param>
		/// <param name="end">Last character to be in the return string.</param>
		std::string get_text(const text_pos &beg, const text_pos &end) const;
		/// <summary>
		/// Returns the text of the currently active selection if there is one and an empty string otherwise.
		/// </summary>
		std::string get_selected_text() const;

		/// <summary>
		/// Reverts the last action(s) performed on this text editor.
		/// </summary>
		/// <param name="steps">The number of actions to undo.</param>
		void undo(unsigned int steps = 1);
		/// <summary>
		/// Returns whether any actions have been recorded which can be reverted.
		/// </summary>
		bool can_undo() const { return _undo_index > 0; }
		/// <summary>
		/// Applies any last action(s) again that were previously reverted.
		/// </summary>
		/// <param name="steps">The number of actions to redo.</param>
		void redo(unsigned int steps = 1);
		/// <summary>
		/// Returns whether any actions have been recorded which can be applied again.
		/// </summary>
		bool can_redo() const { return _undo_index < _undo.size(); }

		/// <summary>
		/// Returns whether the user has modified the text since it was last set via <see cref="set_text"/>.
		/// </summary>
		bool is_modified() const { return _modified; }

		/// <summary>
		/// Adds an error to be displayed at the specified <paramref name="line"/>.
		/// </summary>
		/// <param name="line">The line that should be highlighted and show an error message when hovered with the mouse.</param>
		/// <param name="message">The error message that should be displayed.</param>
		/// <param name="warning">Set to <c>true</c> to indicate that this is a warning instead of an error, which uses different color coding.</param>
		void add_error(size_t line, const std::string &message, bool warning = false) { _errors.emplace(line, std::make_pair(message, warning)); }
		/// <summary>
		/// Removes all displayed errors that were previously added via <see cref="add_error"/>.
		/// </summary>
		void clear_errors() { _modified = false; _errors.clear(); }

		/// <summary>
		/// Changes the read-only state of this text editor.
		/// Set to <c>true</c> to prevent user from being able to modify the text, <c>false</c> to behave like a normal editor.
		/// </summary>
		void set_readonly(bool state) { _readonly = state; }
		/// <summary>
		/// Changes the number of spaces that are inserted when the "Tabulator" key is pressed. Defaults to 4.
		/// </summary>
		void set_tab_size(unsigned short size) { _tab_size = size; }
		/// <summary>
		/// Changes the size of the margin (in pixels) between the left border of the text editor and the rendered text.
		/// </summary>
		void set_left_margin(float margin) { _left_margin = margin; }
		/// <summary>
		/// Changes the size of the empty space (in pixels) between lines of the rendered text.
		/// </summary>
		void set_line_spacing(float spacing) { _line_spacing = spacing; }

		void set_palette(const std::array<uint32_t, color_palette_max> &palette) { _palette = palette; }
		uint32_t &get_palette_index(unsigned int index) { return _palette[index]; }
		const std::array<uint32_t, color_palette_max> &get_palette() const { return _palette; }
		static const char *get_palette_color_name(unsigned int index);

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

		// Holds the entire text split up into individual character glyphs
		std::vector<std::vector<glyph>> _lines;

		bool _readonly = false;
		bool _overwrite = false;
		bool _scroll_to_cursor = false;
		float _cursor_anim = 0.0f;
		float _left_margin = 10.0f;
		float _line_spacing = 1.0f;
		unsigned short _tab_size = 4;
		double _last_click_time = -1.0;

		text_pos _cursor_pos;
		text_pos _select_beg;
		text_pos _select_end;
		text_pos _interactive_beg;
		text_pos _interactive_end;
		std::string _highlighted;
		std::string _last_copy_string;
		bool _last_copy_from_empty_selection = false;

		bool _modified = false;
		bool _in_undo_operation = false;
		size_t _undo_index = 0;
		std::vector<undo_record> _undo;

		std::unordered_map<size_t, std::pair<std::string, bool>> _errors;

		char _search_text[256] = "";
		char _replace_text[256] = "";
		bool _show_search_popup = false;
		bool _search_case_sensitive = false;
		unsigned int _search_window_open = 0;
		unsigned int _search_window_focus = 0;

		size_t _colorize_line_beg = 0;
		size_t _colorize_line_end = 0;
		std::array<uint32_t, color_palette_max> _palette;
	};
}
