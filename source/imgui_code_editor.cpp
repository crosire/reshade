/*
 * Copyright (C) 2017 BalazsJako
 * Copyright (C) 2018 Patrick Mours
 * SPDX-License-Identifier: MIT
 *
 * Heavily modified fork of https://github.com/BalazsJako/ImGuiColorTextEdit
 */

#include "imgui_code_editor.hpp"
#include <cmath> // std::abs, std::floor, std::fmod
#include <cctype> // std::isblank, std::tolower
#include <cstdio> // std::snprintf
#include <algorithm> // std::max, std::min
#include <utf8/unchecked.h>
#include <imgui.h>

static int get_parenthesis_type(utf8::utfchar32_t c)
{
	switch (c)
	{
	default:
		return  0;
	case '(':
		return -1;
	case ')':
		return +1;
	case '<':
		return -2;
	case '>':
		return +2;
	case '[':
		return -3;
	case ']':
		return +3;
	case '{':
		return -4;
	case '}':
		return +4;
	}
}

const char *reshade::imgui::code_editor::get_palette_color_name(unsigned int col)
{
	// Use similar naming convention as 'ImGui::GetStyleColorName'
	switch (col)
	{
	case color_default:
		return "Default";
	case color_keyword:
		return "Keyword";
	case color_number_literal:
		return "NumberLiteral";
	case color_string_literal:
		return "StringLiteral";
	case color_punctuation:
		return "Punctuation";
	case color_preprocessor:
		return "Preprocessor";
	case color_identifier:
		return "Identifier";
	case color_known_identifier:
		return "KnownIdentifier";
	case color_preprocessor_identifier:
		return "PreprocessorIdentifier";
	case color_comment:
		return "Comment";
	case color_multiline_comment:
		return "MultilineComment";
	case color_background:
		return "Background";
	case color_cursor:
		return "Cursor";
	case color_selection:
		return "Selection";
	case color_error_marker:
		return "ErrorMarker";
	case color_warning_marker:
		return "WarningMarker";
	case color_line_number:
		return "LineNumber";
	case color_current_line_fill:
		return "LineNumberFill";
	case color_current_line_fill_inactive:
		return "LineNumberFillInactive";
	case color_current_line_edge:
		return "CurrentLineEdge";
	}
	return nullptr;
}

// 'ImGui::CalcTextSize' cancels out character spacing at the end, but we do not want that, hence the custom function
static ImVec2 calc_text_size(const char *text, const char *text_end = nullptr)
{
	return ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, text, text_end, nullptr);
}

reshade::imgui::code_editor::code_editor()
{
	_lines.emplace_back();
}

void reshade::imgui::code_editor::render(const char *title, const uint32_t palette[color_palette_max], bool border, ImFont *font, float font_size)
{
	// There should always at least be a single line with a new line character
	assert(!_lines.empty());

	// Get all the style values here, before they are overwritten via 'ImGui::PushStyleVar'
	const float button_size = ImGui::GetFrameHeight();
	const float bottom_height = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::PushFont(font, font_size);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(palette[color_background]));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

	ImGuiIO &io = ImGui::GetIO();
	const bool ctrl = io.KeyCtrl, shift = io.KeyShift, alt = io.KeyAlt;

	const bool open_search_window = ctrl && !shift && !alt && (ImGui::IsKeyPressed(ImGuiKey_F) || ImGui::IsKeyPressed(ImGuiKey_H));
	if (open_search_window)
	{
		// Copy currently selected text into search box
		if (_select_beg != _select_end)
			_search_text[get_selected_text().copy(_search_text, sizeof(_search_text) - 1)] = '\0';

		_search_window_open = ImGui::IsKeyPressed(ImGuiKey_H) && !_readonly ? 2 /* search + replace */ : 1 /* search */;
	}
	else if (_search_window_open < 0)
	{
		_search_window_open = 0;
		ImGui::SetNextWindowFocus();
	}

	ImGui::BeginChild(title, ImVec2(0, _search_window_open * -bottom_height), border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);

	char buf[128] = "", *buf_end = buf;

	// Deduce text start offset by evaluating maximum number of lines plus two spaces as text width
	std::snprintf(buf, 16, " %zu ", _lines.size());
	const float text_start = calc_text_size(buf).x + _left_margin;
	const float space_size = calc_text_size(" ").x;
	// The following holds the approximate width and height of a default character for offset calculation
	const ImVec2 char_advance = ImVec2(space_size, ImGui::GetTextLineHeightWithSpacing() * _line_spacing);

	_cursor_anim += io.DeltaTime;

	// Handle keyboard input
	if (ImGui::IsWindowFocused())
	{
		if (ImGui::IsWindowHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);

		io.WantTextInput = true;
		io.WantCaptureKeyboard = true;

		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			ImGui::SetWindowFocus(nullptr); // Reset window focus
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Z))
			undo();
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Y))
			redo();
		else if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_UpArrow))
			if (alt && !shift && !_readonly) // Alt + Up moves the current line one up
				move_lines_up();
			else
				move_up(1, shift);
		else if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_DownArrow))
			if (alt && !shift && !_readonly) // Alt + Down moves the current line one down
				move_lines_down();
			else
				move_down(1, shift);
		else if (!alt && ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
			move_left(1, shift, ctrl);
		else if (!alt && ImGui::IsKeyPressed(ImGuiKey_RightArrow))
			move_right(1, shift, ctrl);
		else if (!alt && ImGui::IsKeyPressed(ImGuiKey_PageUp))
			move_up(static_cast<size_t>(floor((ImGui::GetWindowHeight() - 20.0f) / char_advance.y) - 4.0f), shift);
		else if (!alt && ImGui::IsKeyPressed(ImGuiKey_PageDown))
			move_down(static_cast<size_t>(floor((ImGui::GetWindowHeight() - 20.0f) / char_advance.y) - 4.0f), shift);
		else if (!alt && ImGui::IsKeyPressed(ImGuiKey_Home))
			if (ctrl)
				move_top(shift);
			else
				move_home(shift);
		else if (!alt && ImGui::IsKeyPressed(ImGuiKey_End))
			if (ctrl)
				move_bottom(shift);
			else
				move_end(shift);
		else if (!ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Delete) && !_readonly)
			delete_next();
		else if (!ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Backspace) && !_readonly)
			delete_previous();
		else if (!alt && ImGui::IsKeyPressed(ImGuiKey_Insert))
			if (ctrl)
				clipboard_copy();
			else if (shift)
				clipboard_paste();
			else
				_overwrite ^= true;
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_C))
			clipboard_copy();
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_V))
			clipboard_paste();
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_X))
			clipboard_cut();
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_A))
			select_all();
		else if (!_readonly)
			if (!ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Enter))
				insert_character('\n', true);
			else
				for (ImWchar c : io.InputQueueCharacters)
					if (c != 0 && (isprint(c) || isspace(c)))
						insert_character(static_cast<char>(c), true);
	}

	// Handle mouse input
	if (ImGui::IsWindowHovered() && !alt)
	{
		const auto mouse_to_text_pos = [this, text_start, space_size, &char_advance]() {
			const ImVec2 pos(ImGui::GetMousePos().x - ImGui::GetCursorScreenPos().x, ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y);

			text_pos res;
			res.line = std::max(static_cast<size_t>(0), static_cast<size_t>(std::floor(pos.y / char_advance.y)));
			res.line = std::min(res.line, _lines.size() - 1);

			float column_width = 0.0f;
			float cumulated_string_width[2] = { 0.0f, 0.0f }; // [0] is the latest, [1] is the previous. I use that trick to check where cursor is exactly (important for tabs).

			const std::vector<glyph> &line = _lines[res.line];

			// First we find the hovered column
			while (text_start + cumulated_string_width[0] < pos.x && res.column < line.size())
			{
				cumulated_string_width[1] = cumulated_string_width[0];

				if (line[res.column].c == '\t')
				{
					cumulated_string_width[0] += _tab_size * space_size;
				}
				else
				{
					char text[4], *const text_end = utf8::unchecked::append(line[res.column].c, text);
					cumulated_string_width[0] += calc_text_size(text, text_end).x;
				}

				column_width = (cumulated_string_width[0] - cumulated_string_width[1]);
				res.column++;
			}

			// Then we reduce by 1 column if cursor is on the left side of the hovered column
			if (text_start + cumulated_string_width[0] - column_width / 2.0f > pos.x && res.column > 0)
				res.column--;

			return res;
		};

		const bool is_clicked = ImGui::IsMouseClicked(0);
		const bool is_double_click = !shift && ImGui::IsMouseDoubleClicked(0);
		const bool is_triple_click = !shift && is_clicked && !is_double_click && (ImGui::GetTime() - _last_click_time) < io.MouseDoubleClickTime;

		if (is_triple_click)
		{
			if (!ctrl)
			{
				_cursor_pos = mouse_to_text_pos();
				_interactive_beg = _cursor_pos;
				_interactive_end = _cursor_pos;

				select(_interactive_beg, _interactive_end, selection_mode::line);
			}

			_last_click_time = -1.0;
		}
		else if (is_double_click)
		{
			if (!ctrl)
			{
				_cursor_pos = mouse_to_text_pos();
				_interactive_beg = _cursor_pos;
				_interactive_end = _cursor_pos;

				select(_interactive_beg, _interactive_end, selection_mode::word);
			}

			_last_click_time = ImGui::GetTime();
		}
		else if (is_clicked)
		{
			const bool flip_selection = _cursor_pos > _select_beg;

			_cursor_pos = mouse_to_text_pos();

			if (!ctrl)
			{
				_interactive_beg = _cursor_pos;
				_interactive_end = _cursor_pos;

				if (shift)
					if (flip_selection)
						_interactive_beg = _select_beg;
					else
						_interactive_end = _select_end;

				select(_interactive_beg, _interactive_end, selection_mode::normal);
			}
			else
			{
				select(_cursor_pos, _cursor_pos, selection_mode::word);
				const bool search_whole_word = _search_whole_word;
				_search_whole_word = true;
				if (const std::string highlighted = get_selected_text(); !find_and_scroll_to_text(highlighted))
				{
					// Wrap around to the beginning
					select(text_pos(), text_pos());
					find_and_scroll_to_text(highlighted);
				}
				_search_whole_word = search_whole_word;
			}

			_last_click_time = ImGui::GetTime();
		}
		else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) // Update selection while left mouse is dragging
		{
			io.WantCaptureMouse = true;

			_cursor_pos = mouse_to_text_pos();
			_interactive_end = _cursor_pos;

			select(_interactive_beg, _interactive_end, selection_mode::normal);
		}
	}

	// If cursor is on a parenthesis or bracket, find the matching sibling to highlight
	text_pos parenthesis_pos[2];
	{
		utf8::utfchar32_t parenthesis_c = 0;
		text_pos search_pos = _cursor_pos;

		// Check if character to the right or left of the cursor is a parenthesis or bracket
		if (search_pos.column < _lines[search_pos.line].size())
			parenthesis_c = _lines[search_pos.line][search_pos.column].c;
		if (search_pos.column > 0 && !get_parenthesis_type(parenthesis_c))
			parenthesis_c = _lines[search_pos.line][--search_pos.column].c;

		if (const int parenthesis_type = get_parenthesis_type(parenthesis_c))
		{
			parenthesis_pos[0] = search_pos;

			const bool backwards = parenthesis_type > 0;
			int parentheses_level = 1;
			// These wrap around at zero to 'std::numeric_limits<size_t>::max()' which fail the loop condition and therefore work
			search_pos.column += (backwards ? -1 : 1);
			for (; search_pos.line <= (_lines.size() - 1) && parentheses_level != 0; backwards ? --search_pos.line : ++search_pos.line)
			{
				for (; search_pos.column < _lines[search_pos.line].size(); backwards ? --search_pos.column : ++search_pos.column)
				{
					const int next_parenthesis_type = get_parenthesis_type(_lines[search_pos.line][search_pos.column].c);
					if (std::abs(parenthesis_type) != std::abs(next_parenthesis_type))
						continue;

					if ((next_parenthesis_type < 0 && (backwards ? --parentheses_level : ++parentheses_level) == 0) ||
						(next_parenthesis_type > 0 && (backwards ? ++parentheses_level : --parentheses_level) == 0))
					{
						parenthesis_pos[1] = text_pos(search_pos.line, search_pos.column);
						break;
					}
				}

				// Reset search column on new line
				search_pos.column = backwards && search_pos.line > 0 && !_lines[search_pos.line - 1].empty() ? _lines[search_pos.line - 1].size() - 1 : 0;
			}
		}
	}

	// Update glyph colors
	colorize();

	ImDrawList *const draw_list = ImGui::GetWindowDrawList();

	float longest_line = 0.0f;

	size_t line_no = static_cast<size_t>(std::floor(ImGui::GetScrollY() / char_advance.y));
	size_t line_max = std::max(static_cast<size_t>(0), std::min(_lines.size() - 1, line_no + static_cast<size_t>(std::floor((ImGui::GetScrollY() + ImGui::GetContentRegionAvail().y) / char_advance.y))));

	const auto calc_text_distance_to_line_begin = [this, space_size](const text_pos &from) {
		float distance = 0.0f;
		const std::vector<glyph> &line = _lines[from.line];
		for (size_t i = 0u; i < line.size() && i < from.column; ++i)
		{
			if (line[i].c == '\t')
			{
				distance += _tab_size * space_size;
			}
			else
			{
				char text[4], *const text_end = utf8::unchecked::append(line[i].c, text);
				distance += calc_text_size(text, text_end).x;
			}
		}
		return distance;
	};

	for (; line_no <= line_max; ++line_no, buf_end = buf)
	{
		const std::vector<glyph> &line = _lines[line_no];

		// Position of the line number
		const ImVec2 line_screen_pos = ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + line_no * char_advance.y);
		// Position of the text inside the editor
		const ImVec2 text_screen_pos = ImVec2(line_screen_pos.x + text_start, line_screen_pos.y);

		const text_pos line_beg_pos(line_no, 0);
		const text_pos line_end_pos(line_no, line.size());
		longest_line = std::max(calc_text_distance_to_line_begin(line_end_pos), longest_line);

		// Calculate selection rectangle
		float selection_beg = -1.0f;
		if (_select_beg <= line_end_pos)
			selection_beg = _select_beg > line_beg_pos ? calc_text_distance_to_line_begin(_select_beg) : 0.0f;
		float selection_end = -1.0f;
		if (_select_end >  line_beg_pos)
			selection_end = calc_text_distance_to_line_begin(_select_end < line_end_pos ? _select_end : line_end_pos);

		// Add small overhead rectangle at the end of selected lines
		if (_select_end.line > line_no)
			selection_end += char_advance.x;

		// Draw selected area
		if (selection_beg != -1 && selection_end != -1 && selection_beg < selection_end)
		{
			const ImVec2 beg = ImVec2(text_screen_pos.x + selection_beg, text_screen_pos.y);
			const ImVec2 end = ImVec2(text_screen_pos.x + selection_end, text_screen_pos.y + char_advance.y);

			draw_list->AddRectFilled(beg, end, palette[color_selection]);
		}

		// Highlight matching parentheses
		if (parenthesis_pos[0] != text_pos() && parenthesis_pos[1] != text_pos())
		{
			for (int i = 0; i < 2; ++i)
			{
				if (parenthesis_pos[i].line == line_no)
				{
					const ImVec2 beg = ImVec2(text_screen_pos.x + calc_text_distance_to_line_begin(parenthesis_pos[i]), text_screen_pos.y);
					const ImVec2 end = ImVec2(text_screen_pos.x + calc_text_distance_to_line_begin(text_pos(line_no, parenthesis_pos[i].column + 1)), text_screen_pos.y + char_advance.y);

					draw_list->AddRectFilled(beg, end, palette[color_selection]);
				}
			}
		}

		// Find any highlighted words and draw a selection rectangle below them
		if (!_highlighted.empty())
		{
			size_t begin_column = 0;
			size_t highlight_index = 0;

			for (size_t i = 0; i < line.size(); ++i)
			{
				if (line[i].c == _highlighted[highlight_index] && line[i].col == color_identifier)
				{
					if (highlight_index == 0)
						begin_column = i;

					if (++highlight_index == _highlighted.size())
					{
						if ((begin_column == 0 || line[begin_column - 1].col != color_identifier) && (i + 1 == line.size() || line[i + 1].col != color_identifier)) // Make sure this is a whole word and not just part of one
						{
							// We found a matching text block
							const ImVec2 beg = ImVec2(text_screen_pos.x + calc_text_distance_to_line_begin(text_pos(line_no, begin_column)), text_screen_pos.y);
							const ImVec2 end = ImVec2(text_screen_pos.x + calc_text_distance_to_line_begin(text_pos(line_no, i + 1)), text_screen_pos.y + char_advance.y);

							draw_list->AddRectFilled(beg, end, palette[color_selection]);
						}
					}
					else
					{
						// Continue search
						continue;
					}
				}

				// Current text no longer matches, reset search
				highlight_index = 0;
			}
		}

		// Draw error markers
		if (auto it = _errors.find(line_no + 1); it != _errors.end())
		{
			const ImVec2 beg = ImVec2(line_screen_pos.x + ImGui::GetScrollX(), line_screen_pos.y);
			const ImVec2 end = ImVec2(line_screen_pos.x + ImGui::GetContentRegionAvail().x + 2.0f * ImGui::GetScrollX(), line_screen_pos.y + char_advance.y);

			draw_list->AddRectFilled(beg, end, palette[it->second.second ? color_warning_marker : color_error_marker]);

			if (ImGui::IsMouseHoveringRect(beg, end))
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(it->second.first.c_str());
				ImGui::EndTooltip();
			}
		}

		// Highlight the current line (where the cursor is)
		if (_cursor_pos.line == line_no)
		{
			const bool is_focused = ImGui::IsWindowFocused();

			if (!has_selection())
			{
				const ImVec2 beg = ImVec2(line_screen_pos.x + ImGui::GetScrollX(), line_screen_pos.y);
				const ImVec2 end = ImVec2(line_screen_pos.x + ImGui::GetContentRegionAvail().x + 2.0f * ImGui::GetScrollX(), line_screen_pos.y + char_advance.y);

				draw_list->AddRectFilled(beg, end, palette[is_focused ? color_current_line_fill : color_current_line_fill_inactive]);
				draw_list->AddRect(beg, end, palette[color_current_line_edge], 1.0f);
			}

			// Draw the cursor animation
			if (is_focused && io.ConfigInputTextCursorBlink && std::fmod(_cursor_anim, 1.0f) <= 0.5f)
			{
				const float cx = calc_text_distance_to_line_begin(_cursor_pos);

				const ImVec2 beg = ImVec2(text_screen_pos.x + cx, line_screen_pos.y);
				const ImVec2 end = ImVec2(text_screen_pos.x + cx + (_overwrite ? char_advance.x : 1.0f), line_screen_pos.y + char_advance.y); // Larger cursor while overwriting

				draw_list->AddRectFilled(beg, end, palette[color_cursor]);
			}
		}

		// Draw line number (right aligned)
		std::snprintf(buf, 16, "%zu  ", line_no + 1);

		draw_list->AddText(ImVec2(text_screen_pos.x - calc_text_size(buf).x, line_screen_pos.y), palette[color_line_number], buf);

		// Nothing to draw if the line is empty, so continue on
		if (line.empty())
			continue;

		// Draw colorized line text
		float text_offset = 0.0f;
		color current_color = line[0].col;

		// Fill temporary buffer with glyph characters and commit it every time the color changes or a tab character is encountered
		for (const glyph &glyph : line)
		{
			if (buf != buf_end && (glyph.col != current_color || glyph.c == '\t' || buf_end - buf >= sizeof(buf) - 4)) // Up to 4 bytes per unicode code point
			{
				draw_list->AddText(ImVec2(text_screen_pos.x + text_offset, text_screen_pos.y), palette[current_color], buf, buf_end);

				text_offset += calc_text_size(buf, buf_end).x; buf_end = buf; // Reset temporary buffer
			}

			if (glyph.c != '\t')
				buf_end = utf8::unchecked::append(glyph.c, buf_end);
			else
				text_offset += _tab_size * space_size;

			current_color = glyph.col;
		}

		// Draw any text still in the temporary buffer that was not yet committed
		if (buf != buf_end)
			draw_list->AddText(ImVec2(text_screen_pos.x + text_offset, text_screen_pos.y), palette[current_color], buf, buf_end);
	}

	// Create dummy widget so a horizontal scrollbar appears
	ImGui::Dummy(ImVec2(text_start + longest_line, _lines.size() * char_advance.y));

	if (_scroll_to_cursor)
	{
		const float len = calc_text_distance_to_line_begin(_cursor_pos);
		const float extra_space = 8.0f;

		const float max_scroll_width = ImGui::GetWindowWidth() - 16.0f;
		const float max_scroll_height = ImGui::GetWindowHeight() - 32.0f;

		if (_cursor_pos.line < (ImGui::GetScrollY()) / char_advance.y) // No additional space when scrolling up
			ImGui::SetScrollY(std::max(0.0f, _cursor_pos.line * char_advance.y));
		if (_cursor_pos.line > (ImGui::GetScrollY() + max_scroll_height - extra_space) / char_advance.y)
			ImGui::SetScrollY(std::max(0.0f, _cursor_pos.line * char_advance.y + extra_space - max_scroll_height));

		if (len + text_start < (ImGui::GetScrollX() + extra_space))
			ImGui::SetScrollX(std::max(0.0f, len + text_start - extra_space));
		if (len + text_start > (ImGui::GetScrollX() + max_scroll_width - extra_space))
			ImGui::SetScrollX(std::max(0.0f, len + text_start + extra_space - max_scroll_width));

		if (_search_window_open == 0) // Focus on search text box instead of the editor window when it is open
			ImGui::SetWindowFocus();

		_scroll_to_cursor = false;
	}

	ImGui::EndChild();

	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	ImGui::PopFont();

	if (_search_window_open > 0)
	{
		ImGui::Dummy(ImVec2(0, ImGui::GetStyle().ItemSpacing.y));
		ImGui::BeginChild("##search", ImVec2(0, 0));

		if (open_search_window)
			ImGui::SetKeyboardFocusHere();

		const float input_width = ImGui::GetContentRegionAvail().x - ((5 * button_spacing) + (2 * (button_size + 5)) + (3 * button_size));
		ImGui::PushItemWidth(input_width);
		if (ImGui::InputText("##search", _search_text, sizeof(_search_text), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AllowTabInput))
		{
			ImGui::SetKeyboardFocusHere(-1); // Focus text box again after entering a value

			find_and_scroll_to_text(_search_text);
		}
		ImGui::PopItemWidth();

		ImGui::SameLine(0.0f, button_spacing);
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[_search_case_sensitive ? ImGuiCol_ButtonActive : ImGuiCol_Button]);
		if (ImGui::Button("Aa", ImVec2(button_size + 5, 0)) || (!ctrl && !shift && alt && ImGui::IsKeyPressed(ImGuiKey_C)))
			_search_case_sensitive = !_search_case_sensitive;
		ImGui::PopStyleColor();
		ImGui::SetItemTooltip("Match case (Alt + C)");

		ImGui::SameLine(0.0f, button_spacing);
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[_search_whole_word ? ImGuiCol_ButtonActive : ImGuiCol_Button]);
		if (ImGui::Button("ab", ImVec2(button_size + 5, 0)) || (!ctrl && !shift && alt && ImGui::IsKeyPressed(ImGuiKey_W)))
			_search_whole_word = !_search_whole_word;
		ImGui::PopStyleColor();
		ImGui::SetItemTooltip("Match whole word (Alt + W)");

		ImGui::SameLine(0.0f, button_spacing);
		if (ImGui::Button("<", ImVec2(button_size, 0)) || (shift && ImGui::IsKeyPressed(ImGuiKey_F3)))
			find_and_scroll_to_text(_search_text, true);
		ImGui::SetItemTooltip("Find previous (Shift + F3)");

		ImGui::SameLine(0.0f, button_spacing);
		if (ImGui::Button(">", ImVec2(button_size, 0)) || (!shift && ImGui::IsKeyPressed(ImGuiKey_F3)))
			find_and_scroll_to_text(_search_text, false);
		ImGui::SetItemTooltip("Find next (F3)");

		ImGui::SameLine(0.0f, button_spacing);
		if (ImGui::Button("X", ImVec2(button_size, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			// Move focus back to text editor again next frame
			_search_window_open = -1;
		}
		ImGui::SetItemTooltip("Close (Escape)");

		if (_search_window_open > +1)
		{
			ImGui::PushItemWidth(input_width);
			if (ImGui::InputText("##replace", _replace_text, sizeof(_replace_text), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AllowTabInput))
			{
				ImGui::SetKeyboardFocusHere(-1); // Focus replace text box again after entering a value

				if (find_and_scroll_to_text(_search_text, false, true))
				{
					delete_selection();
					insert_text(_replace_text);
				}
			}
			ImGui::PopItemWidth();

			ImGui::SameLine(0.0f, (3 * button_spacing) + (2 * (button_size + 5)));
			if (ImGui::Button("Repl", ImVec2(2 * button_size + button_spacing, 0)) || (!ctrl && !shift && alt && ImGui::IsKeyPressed(ImGuiKey_R)))
			{
				if (find_and_scroll_to_text(_search_text, false, true))
				{
					delete_selection();
					insert_text(_replace_text);
				}
			}
			ImGui::SetItemTooltip("Replace next (Alt + R)");

			ImGui::SameLine(0.0f, button_spacing);
			if (ImGui::Button("A", ImVec2(button_size, 0)) || (!ctrl && !shift && alt && ImGui::IsKeyPressed(ImGuiKey_A)))
			{
				// Reset select position so that replace stats at document begin
				_select_beg = text_pos();
				while (find_and_scroll_to_text(_search_text, false, true))
				{
					delete_selection();
					insert_text(_replace_text);
				}
			}
			ImGui::SetItemTooltip("Replace all (Alt + A)");
		}

		ImGui::EndChild();
	}
}

void reshade::imgui::code_editor::select(const text_pos &beg, const text_pos &end, selection_mode mode)
{
	assert(beg.line < _lines.size());
	assert(end.line < _lines.size());
	assert(beg.column <= _lines[beg.line].size()); // The last column is after the last character in the line
	assert(end.column <= _lines[end.line].size());

	if (end > beg)
		_select_beg = beg,
		_select_end = end;
	else
		_select_end = beg,
		_select_beg = end;

	const auto select_word = [this](text_pos &beg, text_pos &end) {
		const std::vector<glyph> &beg_line = _lines[beg.line];
		const std::vector<glyph> &end_line = _lines[end.line];
		// Empty lines cannot have any words, so abort
		if (beg_line.empty() || end_line.empty())
			return;
		// Whitespace has a special meaning in that if we select the space next to a word, then that word is precedence over the whitespace
		if (beg.column == beg_line.size() || (beg.column > 0 && beg_line[beg.column].col == color_default))
			beg.column--;
		if (end.column == end_line.size() || (end.column > 0 && end_line[end.column].col == color_default))
			end.column--;
		// Search from the first position backwards until a character with a different color is found
		for (color word_color = beg_line[beg.column].col;
			beg.column > 0 && beg_line[beg.column - 1].col == word_color;
			--beg.column)
			continue;
		// Search from the selection end position forwards until a character with a different color is found
		for (color word_color = end_line[end.column].col;
			end.column < end_line.size() && end_line[end.column].col == word_color;
			++end.column)
			continue;
	};

	// Reset cursor animation (so it is always visible when clicking something)
	_cursor_anim = 0;

	// Find the identifier under the cursor position
	_highlighted.clear();
	text_pos highlight_beg = _select_beg;
	text_pos highlight_end = _select_end;
	select_word(highlight_beg, highlight_end);
	if (_lines[highlight_beg.line].size() > highlight_beg.column && _lines[highlight_beg.line][highlight_beg.column].col == color_identifier)
	{
		for (text_pos it = highlight_beg; it < highlight_end;)
		{
			const std::vector<glyph> &line = _lines[it.line];

			if (it.column < line.size())
			{
				_highlighted.push_back(line[it.column].c);

				it.column++;
			}
			else
			{
				it.line++;
				it.column = 0;

				if (it.line < _lines.size())
					// Reached end of line, so append a new line feed
					_highlighted.push_back('\n');
			}
		}

	}

	switch (mode)
	{
	case selection_mode::word:
		select_word(_select_beg, _select_end);
		break;
	case selection_mode::line:
		_select_beg.column = 0;
		_select_end.column = _lines[end.line].size();
		break;
	}
}
void reshade::imgui::code_editor::select_all()
{
	if (_lines.empty())
		return; // Cannot select anything if no text is set

	// Move cursor to end of text
	_cursor_pos = text_pos(_lines.size() - 1, _lines.back().size());

	// Update selection to contain everything
	_interactive_beg = text_pos(0, 0);
	_interactive_end = _cursor_pos;

	select(_interactive_beg, _interactive_end);
}

void reshade::imgui::code_editor::set_text(const std::string_view text)
{
	_lines.clear();
	_lines.emplace_back();

	_undo.clear();
	_undo_index = 0;
	_undo_base_index = 0;
	_errors.clear();

	std::string_view::const_iterator it = text.begin();
	if (utf8::starts_with_bom(it, text.end()))
		it += std::size(utf8::bom);
	for (; it < text.end();)
	{
		const utf8::utfchar32_t c = utf8::unchecked::next(it);
		if (c == '\r')
			continue; // Ignore the carriage return character
		else if (c == '\n')
			_lines.emplace_back();
		else
			_lines.back().push_back({ c, color_default });
	}

	// Restrict cursor position to new text bounds
	_select_beg = _select_end = text_pos();
	_interactive_beg = _interactive_end = text_pos();
	_cursor_pos.line = std::min(_cursor_pos.line, _lines.size() - 1);
	_cursor_pos.column = std::min(_cursor_pos.column, _lines[_cursor_pos.line].size());

	_colorize_line_beg = 0;
	_colorize_line_end = _lines.size();
}
void reshade::imgui::code_editor::clear_text()
{
	set_text(std::string_view());
}
void reshade::imgui::code_editor::append_text(const std::string_view text)
{
	text_pos cursor_pos = _cursor_pos;
	const bool reset_cursor_pos = cursor_pos != get_text_end();
	_cursor_pos = get_text_end();
	insert_text(text);
	// Keep the cursor position at the new end when the cursor was at the end before
	// This way the editor scrolls with text being appended when the cursor is at the end, but does not scroll when the cursor is somewhere else
	if (reset_cursor_pos)
		_cursor_pos = cursor_pos;
}
void reshade::imgui::code_editor::insert_text(const std::string_view text)
{
	undo_record u;
	if (!_readonly)
	{
		u.added = text;
		u.added_beg = _cursor_pos;
	}

	_colorize_line_beg = std::min(_colorize_line_beg, _cursor_pos.line);

	// Insert all characters of the text
	for (auto it = text.begin(); it < text.end();)
	{
		const uint32_t c = utf8::unchecked::next(it);

		// New line feed requires insertion of a new line
		if (c == '\n')
		{
			// Move all error markers after the new line one up
			std::unordered_map<size_t, std::pair<std::string, bool>> errors;
			errors.reserve(_errors.size());
			for (std::pair<const size_t, std::pair<std::string, bool>> &i : _errors)
				errors.insert({ i.first >= _cursor_pos.line + 1 ? i.first + 1 : i.first, std::move(i.second) });
			_errors = std::move(errors);

			std::vector<glyph> &new_line = *_lines.emplace(_lines.begin() + _cursor_pos.line + 1);
			std::vector<glyph> &line = _lines[_cursor_pos.line];

			new_line.insert(new_line.end(), line.begin() + _cursor_pos.column, line.end());
			line.erase(line.begin() + _cursor_pos.column, line.begin() + line.size());

			_cursor_pos.line++;
			_cursor_pos.column = 0;
		}
		else if (c != '\r') // Ignore carriage return
		{
			std::vector<glyph> &line = _lines[_cursor_pos.line];

			if (_overwrite && _cursor_pos.column < line.size())
				line[_cursor_pos.column] = { c, color_default };
			else
				line.insert(line.begin() + _cursor_pos.column, { c, color_default });

			_cursor_pos.column++;
		}
	}

	if (!_readonly)
	{
		u.added_end = _cursor_pos;
		record_undo(std::move(u));
	}

	_scroll_to_cursor = true;

	_colorize_line_end = std::max(_colorize_line_end, _cursor_pos.line + 1);

	// Move cursor to end of inserted text
	select(_cursor_pos, _cursor_pos);
}
void reshade::imgui::code_editor::insert_character(uint32_t c, bool auto_indent)
{
	undo_record u;

	if (has_selection())
	{
		if (c == '\t' && auto_indent) // Pressing tab with a selection indents the entire selection
		{
			text_pos &beg = _select_beg;
			text_pos &end = _select_end;

			_colorize_line_beg = std::min(_colorize_line_beg, beg.line - std::min(beg.line, static_cast<size_t>(10)));
			_colorize_line_end = std::max(_colorize_line_end, end.line + 10 + 1);

			beg.column = 0;
			if (end.column == 0 && end.line > 0)
				end.column = _lines[--end.line].size();

			u.removed = get_text(beg, end);
			u.removed_beg = beg;
			u.removed_end = end;

			for (size_t i = beg.line; i <= end.line; i++)
			{
				std::vector<glyph> &line = _lines[i];

				if (ImGui::GetIO().KeyShift)
				{
					if (line.empty())
						continue; // Line is already empty, so there is no indentation to remove

					if (line[0].c == '\t')
					{
						line.erase(line.begin());
						if (i == end.line && end.column > 0)
							end.column--;
						if (i == _cursor_pos.line && _cursor_pos.column > 0)
							_cursor_pos.column--;
					}
					else for (size_t j = 0; j < _tab_size && !line.empty() && line[0].c == ' '; j++) // Do the same for spaces
					{
						line.erase(line.begin());
						if (i == end.line && end.column > 0)
							end.column--;
						if (i == _cursor_pos.line && _cursor_pos.column > 0)
							_cursor_pos.column--;
					}
				}
				else
				{
					line.insert(line.begin(), { '\t', color_background });
					if (i == end.line)
						end.column++;
					if (i == _cursor_pos.line)
						_cursor_pos.column++;
				}
			}

			u.added = get_text(beg, end);
			u.added_beg = beg;
			u.added_end = end;
			record_undo(std::move(u));
			return;
		}

		// Otherwise overwrite the selection
		delete_selection();
	}
	else
	{
		if (c == '\t' && auto_indent && ImGui::GetIO().KeyShift)
		{
			std::vector<glyph> &line = _lines[_cursor_pos.line];

			if (line.empty())
				return; // Line is already empty, so there is no indentation to remove

			u.removed_beg = text_pos(_cursor_pos.line, 0);
			u.removed_end = u.removed_beg;

			if (line[0].c == '\t')
			{
				utf8::unchecked::append(line[0].c, std::back_inserter(u.removed));
				u.removed_end.column++;
				line.erase(line.begin());
				if (_cursor_pos.column > 0)
					_cursor_pos.column--;
			}
			else for (size_t j = 0; j < _tab_size && !line.empty() && line[0].c == ' '; j++) // Do the same for spaces
			{
				utf8::unchecked::append(line[0].c, std::back_inserter(u.removed));
				u.removed_end.column++;
				line.erase(line.begin());
				if (_cursor_pos.column > 0)
					_cursor_pos.column--;
			}

			record_undo(std::move(u));
			return;
		}
	}

	assert(!_lines.empty());

	u.added.clear();
	if (!_readonly)
	{
		utf8::unchecked::append(c, std::back_inserter(u.added));
		u.added_beg = _cursor_pos;
	}

	// Colorize additional 10 lines above and below to better catch multi-line constructs
	_colorize_line_beg = std::min(_colorize_line_beg, _cursor_pos.line - std::min(_cursor_pos.line, static_cast<size_t>(10)));

	// New line feed requires insertion of a new line
	if (c == '\n')
	{
		// Move all error markers after the new line one up
		std::unordered_map<size_t, std::pair<std::string, bool>> errors;
		errors.reserve(_errors.size());
		for (std::pair<const size_t, std::pair<std::string, bool>> &i : _errors)
			errors.insert({ i.first >= _cursor_pos.line + 1 ? i.first + 1 : i.first, std::move(i.second) });
		_errors = std::move(errors);

		std::vector<glyph> &new_line = *_lines.emplace(_lines.begin() + _cursor_pos.line + 1);
		std::vector<glyph> &line = _lines[_cursor_pos.line];

		// Auto indentation
		if (auto_indent && _cursor_pos.column == line.size())
		{
			for (size_t i = 0; i < line.size() && std::isblank(line[i].c); ++i)
			{
				new_line.push_back(line[i]);
				if (!_readonly)
					utf8::unchecked::append(line[i].c, std::back_inserter(u.added));
			}
		}
		const size_t indentation = new_line.size();

		new_line.insert(new_line.end(), line.begin() + _cursor_pos.column, line.end());
		line.erase(line.begin() + _cursor_pos.column, line.begin() + line.size());

		_cursor_pos.line++;
		_cursor_pos.column = indentation;
	}
	else if (c != '\r') // Ignore carriage return
	{
		std::vector<glyph> &line = _lines[_cursor_pos.line];

		if (_overwrite && _cursor_pos.column < line.size())
			line[_cursor_pos.column] = { c, color_default };
		else
			line.insert(line.begin() + _cursor_pos.column, { c, color_default });

		_cursor_pos.column++;
	}

	if (!_readonly)
	{
		u.added_end = _cursor_pos;
		record_undo(std::move(u));
	}

	// Reset cursor animation
	_cursor_anim = 0;

	_scroll_to_cursor = true;

	_colorize_line_end = std::max(_colorize_line_end, _cursor_pos.line + 10 + 1);
}

std::string reshade::imgui::code_editor::get_text() const
{
	return get_text(0, _lines.size());
}
std::string reshade::imgui::code_editor::get_text(const text_pos &beg, const text_pos &end) const
{
	// Calculate length of text to pre-allocate memory before building the string
	size_t length = 0;
	for (text_pos it = beg; it < end; it.line++)
		length += _lines[it.line].size() + 1;

	std::string result;
	result.reserve(length);

	for (text_pos it = beg; it < end;)
	{
		const std::vector<glyph> &line = _lines[it.line];

		if (it.column < line.size())
		{
			utf8::unchecked::append(line[it.column].c, std::back_inserter(result));

			it.column++;
		}
		else
		{
			it.line++;
			it.column = 0;

			if (it.line < _lines.size())
				// Reached end of line, so append a new line feed
				result.push_back('\n');
		}
	}

	return result;
}
std::string reshade::imgui::code_editor::get_selected_text() const
{
	assert(has_selection());

	return get_text(_select_beg, _select_end);
}

void reshade::imgui::code_editor::undo(unsigned int steps)
{
	// Reset selection
	_select_beg = _cursor_pos;
	_select_end = _cursor_pos;
	_interactive_beg = _cursor_pos;
	_interactive_end = _cursor_pos;

	assert(!_undo_operation_active);
	_undo_operation_active = true;

	while (can_undo() && steps-- > 0)
	{
		const undo_record &record = _undo[--_undo_index];

		if (!record.added.empty())
		{
			_cursor_pos = record.added_beg;
			select(record.added_beg, record.added_end);
			delete_selection();
		}
		if (!record.removed.empty())
		{
			_cursor_pos = record.removed_beg;
			insert_text(record.removed);
			select(record.removed_beg, record.removed_end);
		}
	}

	_scroll_to_cursor = true;
	_undo_operation_active = false;
}
void reshade::imgui::code_editor::redo(unsigned int steps)
{
	// Reset selection
	_select_beg = _cursor_pos;
	_select_end = _cursor_pos;
	_interactive_beg = _cursor_pos;
	_interactive_end = _cursor_pos;

	assert(!_undo_operation_active);
	_undo_operation_active = true;

	while (can_redo() && steps-- > 0)
	{
		const undo_record &record = _undo[_undo_index++];

		if (!record.removed.empty())
		{
			_cursor_pos = record.removed_beg;
			select(record.removed_beg, record.removed_end);
			delete_selection();
		}
		if (!record.added.empty())
		{
			_cursor_pos = record.added_beg;
			insert_text(record.added);
			select(record.added_beg, record.added_end);
		}
	}

	_scroll_to_cursor = true;
	_undo_operation_active = false;
}

void reshade::imgui::code_editor::record_undo(undo_record &&record)
{
	if (_undo_operation_active || _readonly)
		return;

	_undo.resize(_undo_index); // Remove all undo records after the current one
	_undo.push_back(std::move(record)); // Append new record to the list
	_undo_index++;
}

void reshade::imgui::code_editor::delete_next()
{
	assert(!_readonly);

	if (has_selection())
	{
		delete_selection();
		return;
	}

	assert(!_lines.empty());

	std::vector<glyph> &line = _lines[_cursor_pos.line];

	undo_record u;
	u.removed_beg = _cursor_pos;
	u.removed_end = _cursor_pos;

	// If at end of line, move next line into the current one
	if (_cursor_pos.column == line.size())
	{
		if (_cursor_pos.line == _lines.size() - 1)
			return; // This already is the last line

		const std::vector<glyph> &next_line = _lines[_cursor_pos.line + 1];

		u.removed = '\n';
		u.removed_end.line++;
		u.removed_end.column = 0;

		// Copy next line into current line
		line.insert(line.end(), next_line.begin(), next_line.end());

		// Remove the line
		delete_lines(_cursor_pos.line + 1, _cursor_pos.line + 1);
	}
	else
	{
		u.removed.clear();
		utf8::unchecked::append(line[_cursor_pos.column].c, std::back_inserter(u.removed));
		u.removed_end.column++;

		// Otherwise just remove the character at the cursor position
		line.erase(line.begin() + _cursor_pos.column);
	}

	record_undo(std::move(u));

	_colorize_line_beg = std::min(_colorize_line_beg, _cursor_pos.line - std::min(_cursor_pos.line, static_cast<size_t>(10)));
	_colorize_line_end = std::max(_colorize_line_end, _cursor_pos.line + 10 + 1);
}
void reshade::imgui::code_editor::delete_previous()
{
	assert(!_readonly);

	if (has_selection())
	{
		delete_selection();
		return;
	}

	assert(!_lines.empty());

	std::vector<glyph> &line = _lines[_cursor_pos.line];

	undo_record u;
	u.removed_end = _cursor_pos;

	// If at beginning of line, move previous line into the current one
	if (_cursor_pos.column == 0)
	{
		if (_cursor_pos.line == 0)
			return; // This already is the first line

		std::vector<glyph> &prev_line = _lines[_cursor_pos.line - 1];
		_cursor_pos.line--;
		_cursor_pos.column = prev_line.size();

		u.removed = '\n';

		// Copy current line into previous line
		prev_line.insert(prev_line.end(), line.begin(), line.end());

		// Remove the line
		delete_lines(_cursor_pos.line + 1, _cursor_pos.line + 1);
	}
	else
	{
		_cursor_pos.column--;

		u.removed.clear();
		utf8::unchecked::append(line[_cursor_pos.column].c, std::back_inserter(u.removed));

		// Otherwise remove the character next to the cursor position
		line.erase(line.begin() + _cursor_pos.column);
	}

	u.removed_beg = _cursor_pos;
	record_undo(std::move(u));

	_scroll_to_cursor = true;

	_colorize_line_beg = std::min(_colorize_line_beg, _cursor_pos.line - std::min(_cursor_pos.line, static_cast<size_t>(10)));
	_colorize_line_end = std::max(_colorize_line_end, _cursor_pos.line + 10 + 1);
}
void reshade::imgui::code_editor::delete_selection()
{
	assert(!_readonly);

	if (!has_selection())
		return;

	assert(!_lines.empty());

	undo_record u;
	u.removed = get_selected_text();
	u.removed_beg = _select_beg;
	u.removed_end = _select_end;
	record_undo(std::move(u));

	if (_select_beg.line == _select_end.line)
	{
		std::vector<glyph> &line = _lines[_select_beg.line];

		if (_select_end.column >= line.size())
			line.erase(line.begin() + _select_beg.column, line.end());
		else
			line.erase(line.begin() + _select_beg.column, line.begin() + _select_end.column);
	}
	else
	{
		std::vector<glyph> &beg_line = _lines[_select_beg.line];
		std::vector<glyph> &end_line = _lines[_select_end.line];

		beg_line.erase(beg_line.begin() + _select_beg.column, beg_line.end());
		end_line.erase(end_line.begin(), end_line.begin() + _select_end.column);

		if (_select_beg.line < _select_end.line)
		{
			beg_line.insert(beg_line.end(), end_line.begin(), end_line.end());

			delete_lines(_select_beg.line + 1, _select_end.line);
		}

		assert(!_lines.empty());
	}

	_colorize_line_beg = std::min(_colorize_line_beg, _select_beg.line - std::min(_cursor_pos.line, static_cast<size_t>(10)));
	_colorize_line_end = std::max(_colorize_line_end, _select_end.line + 10 + 1);

	// Reset selection
	_cursor_pos = _select_beg;
	_interactive_beg = _cursor_pos;
	_interactive_end = _cursor_pos;

	select(_cursor_pos, _cursor_pos);
}
void reshade::imgui::code_editor::delete_lines(size_t first_line, size_t last_line)
{
	// Move all error markers after the deleted lines down
	std::unordered_map<size_t, std::pair<std::string, bool>> errors;
	errors.reserve(_errors.size());
	for (std::pair<const size_t, std::pair<std::string, bool>> &i : _errors)
		if (i.first < first_line || i.first > last_line)
			errors.insert({ i.first > last_line ? i.first - (last_line - first_line) : i.first, std::move(i.second) });
	_errors = std::move(errors);

	_lines.erase(_lines.begin() + first_line, _lines.begin() + last_line + 1);
}

void reshade::imgui::code_editor::clipboard_copy()
{
	_last_copy_string.clear();
	_last_copy_from_empty_selection = false;

	if (has_selection())
	{
		ImGui::SetClipboardText(get_selected_text().c_str());
	}
	else if (!_lines.empty()) // Copy current line if there is no selection
	{
		std::string line_text;
		line_text.reserve(_lines[_cursor_pos.line].size() + 1);
		for (const glyph &glyph : _lines[_cursor_pos.line])
			utf8::unchecked::append(glyph.c, std::back_inserter(line_text));
		// Include new line character
		line_text += '\n';

		ImGui::SetClipboardText(line_text.c_str());

		_last_copy_string = std::move(line_text);
		_last_copy_from_empty_selection = true;
	}
}
void reshade::imgui::code_editor::clipboard_cut()
{
	if (!has_selection())
		return;

	clipboard_copy();

	if (_readonly)
		return;

	delete_selection();
}
void reshade::imgui::code_editor::clipboard_paste()
{
	if (_readonly)
		return;

	const char *const text = ImGui::GetClipboardText();

	if (text == nullptr || *text == '\0')
		return;

	undo_record u;

	_undo_operation_active = true;

	bool reset_cursor_pos = false;
	const size_t cursor_column = _cursor_pos.column;
	if (has_selection())
	{
		u.removed = get_selected_text();
		u.removed_beg = _select_beg;
		u.removed_end = _select_end;

		delete_selection();
	}
	else if (_last_copy_from_empty_selection && _last_copy_string == text)
	{
		reset_cursor_pos = true;
		// Paste line above current line if there is no selection and entire line was copied to clipboard without a selection too
		_cursor_pos.column = 0;
	}

	u.added = text;
	u.added_beg = _cursor_pos;

	insert_text(u.added);

	u.added_end = _cursor_pos;

	_undo_operation_active = false;

	record_undo(std::move(u));

	// Reset cursor position after pasting entire line above
	if (reset_cursor_pos)
		_cursor_pos.column = cursor_column;
}

void reshade::imgui::code_editor::move_up(size_t amount, bool selection)
{
	assert(!_lines.empty());

	const text_pos prev_pos = _cursor_pos;
	_cursor_pos.line = std::max(static_cast<size_t>(0), _cursor_pos.line - amount);

	// The line before could be shorter, so adjust column
	_cursor_pos.column = std::min(_cursor_pos.column, _lines[_cursor_pos.line].size());

	if (prev_pos == _cursor_pos)
		return;

	if (selection)
	{
		if (prev_pos == _interactive_beg)
			_interactive_beg = _cursor_pos;
		else if (prev_pos == _interactive_end)
			_interactive_end = _cursor_pos;
		else
			_interactive_beg = _cursor_pos,
			_interactive_end = prev_pos;
	}
	else
	{
		_interactive_beg = _cursor_pos;
		_interactive_end = _cursor_pos;
	}

	select(_interactive_beg, _interactive_end);
	_scroll_to_cursor = true;
}
void reshade::imgui::code_editor::move_down(size_t amount, bool selection)
{
	assert(!_lines.empty());

	const text_pos prev_pos = _cursor_pos;
	_cursor_pos.line = std::min(_cursor_pos.line + amount, _lines.size() - 1);

	// The line after could be shorter, so adjust column
	_cursor_pos.column = std::min(_cursor_pos.column, _lines[_cursor_pos.line].size());

	if (prev_pos == _cursor_pos)
		return;

	if (selection)
	{
		if (prev_pos == _interactive_beg)
			_interactive_beg = _cursor_pos;
		else if (prev_pos == _interactive_end)
			_interactive_end = _cursor_pos;
		else
			_interactive_beg = prev_pos,
			_interactive_end = _cursor_pos;
	}
	else
	{
		_interactive_beg = _cursor_pos;
		_interactive_end = _cursor_pos;
	}

	select(_interactive_beg, _interactive_end);
	_scroll_to_cursor = true;
}
void reshade::imgui::code_editor::move_left(size_t amount, bool selection, bool word_mode)
{
	assert(!_lines.empty());

	const text_pos prev_pos = _cursor_pos;

	// Move cursor to selection start when moving left and no longer selecting
	if (!selection && _interactive_beg != _interactive_end)
	{
		_cursor_pos = _interactive_beg;
		_interactive_end = _cursor_pos;
	}
	else
	{
		while (amount-- > 0)
		{
			if (_cursor_pos.column == 0) // At the beginning of the current line, so move on to previous
			{
				if (_cursor_pos.line == 0)
					break;

				_cursor_pos.line--;
				_cursor_pos.column = _lines[_cursor_pos.line].size();
			}
			else if (word_mode)
			{
				for (const color word_color = _lines[_cursor_pos.line][_cursor_pos.column - 1].col; _cursor_pos.column > 0; --_cursor_pos.column)
					if (_lines[_cursor_pos.line][_cursor_pos.column - 1].col != word_color)
						break;
			}
			else
			{
				_cursor_pos.column--;
			}
		}

		if (selection)
		{
			if (prev_pos == _interactive_beg)
				_interactive_beg = _cursor_pos;
			else if (prev_pos == _interactive_end)
				_interactive_end = _cursor_pos;
			else
				_interactive_beg = _cursor_pos,
				_interactive_end = prev_pos;
		}
		else
		{
			_interactive_beg = _cursor_pos;
			_interactive_end = _cursor_pos;
		}
	}

	select(_interactive_beg, _interactive_end);
	_scroll_to_cursor = true;
}
void reshade::imgui::code_editor::move_right(size_t amount, bool selection, bool word_mode)
{
	assert(!_lines.empty());

	const text_pos prev_pos = _cursor_pos;

	while (amount-- > 0)
	{
		const std::vector<glyph> &line = _lines[_cursor_pos.line];

		if (_cursor_pos.column >= line.size()) // At the end of the current line, so move on to next
		{
			if (_cursor_pos.line >= _lines.size() - 1)
				break; // Reached end of input

			_cursor_pos.line++;
			_cursor_pos.column = 0;
		}
		else if (word_mode)
		{
			for (const color word_color = _lines[_cursor_pos.line][_cursor_pos.column].col; _cursor_pos.column < _lines[_cursor_pos.line].size(); ++_cursor_pos.column)
				if (_lines[_cursor_pos.line][_cursor_pos.column].col != word_color)
					break;
		}
		else
		{
			_cursor_pos.column++;
		}
	}

	if (selection)
	{
		if (prev_pos == _interactive_end)
			_interactive_end = _cursor_pos;
		else if (prev_pos == _interactive_beg)
			_interactive_beg = _cursor_pos;
		else
			_interactive_beg = prev_pos,
			_interactive_end = _cursor_pos;
	}
	else
	{
		_interactive_beg = _interactive_end = _cursor_pos;
	}

	select(_interactive_beg, _interactive_end);
	_scroll_to_cursor = true;
}
void reshade::imgui::code_editor::move_top(bool selection)
{
	assert(!_lines.empty());

	const text_pos prev_pos = _cursor_pos;
	_cursor_pos = text_pos(0, 0);

	if (prev_pos == _cursor_pos)
		return;

	if (selection)
	{
		// This logic ensures the selection is updated depending on which direction it was created in (mimics behavior of popular text editors)
		if (_interactive_beg > _interactive_end)
			std::swap(_interactive_beg, _interactive_end);
		if (prev_pos != _interactive_beg)
			_interactive_end = _interactive_beg;

		_interactive_beg = _cursor_pos;
	}
	else
	{
		_interactive_beg = _cursor_pos;
		_interactive_end = _cursor_pos;
	}

	select(_interactive_beg, _interactive_end);
	_scroll_to_cursor = true;
}
void reshade::imgui::code_editor::move_bottom(bool selection)
{
	assert(!_lines.empty());

	const text_pos prev_pos = _cursor_pos;
	_cursor_pos = text_pos(_lines.size() - 1, 0);

	if (prev_pos == _cursor_pos)
		return;

	if (selection)
	{
		if (_interactive_beg > _interactive_end)
			std::swap(_interactive_beg, _interactive_end);
		if (prev_pos != _interactive_end)
			_interactive_beg = _interactive_end;

		_interactive_end = _cursor_pos;
	}
	else
	{
		_interactive_beg = _cursor_pos;
		_interactive_end = _cursor_pos;
	}

	select(_interactive_beg, _interactive_end);
	_scroll_to_cursor = true;
}
void reshade::imgui::code_editor::move_home(bool selection)
{
	assert(!_lines.empty());

	const text_pos prev_pos = _cursor_pos;
	_cursor_pos.column = 0;

	if (prev_pos == _cursor_pos &&
		_interactive_beg == _interactive_end) // This ensures that deselection works even when cursor is already at begin
		return;

	if (selection)
	{
		if (prev_pos == _interactive_beg)
			_interactive_beg = _cursor_pos;
		else if (prev_pos == _interactive_end)
			_interactive_end = _cursor_pos;
		else
			_interactive_beg = _cursor_pos,
			_interactive_end = prev_pos;
	}
	else
	{
		_interactive_beg = _interactive_end = _cursor_pos;
	}

	select(_interactive_beg, _interactive_end);
	_scroll_to_cursor = true;
}
void reshade::imgui::code_editor::move_end(bool selection)
{
	assert(!_lines.empty());

	const text_pos prev_pos = _cursor_pos;
	_cursor_pos.column = _lines[_cursor_pos.line].size();

	if (prev_pos == _cursor_pos &&
		_interactive_beg == _interactive_end) // This ensures that deselection works even when cursor is already at end
		return;

	if (selection)
	{
		if (prev_pos == _interactive_end)
			_interactive_end = _cursor_pos;
		else if (prev_pos == _interactive_beg)
			_interactive_beg = _cursor_pos;
		else
			_interactive_beg = prev_pos,
			_interactive_end = _cursor_pos;
	}
	else
	{
		_interactive_beg = _interactive_end = _cursor_pos;
	}

	select(_interactive_beg, _interactive_end);
	_scroll_to_cursor = true;
}
void reshade::imgui::code_editor::move_lines_up()
{
	assert(!_readonly);

	if (_select_beg.line == 0)
		return;

	for (size_t line = _select_beg.line; line <= _select_end.line; ++line)
		std::swap(_lines[line], _lines[line - 1]);

	_select_beg.line--;
	_select_end.line--;
	_cursor_pos.line--;
}
void reshade::imgui::code_editor::move_lines_down()
{
	assert(!_readonly);

	if (_select_end.line + 1 >= _lines.size())
		return;

	for (size_t line = _select_end.line; line >= _select_beg.line && line < _lines.size(); --line)
		std::swap(_lines[line], _lines[line + 1]);

	_select_beg.line++;
	_select_end.line++;
	_cursor_pos.line++;
}

bool reshade::imgui::code_editor::find_and_scroll_to_text(const std::string_view text, bool backwards, bool with_selection)
{
	if (text.empty())
		return false; // Cannot search for empty text

	const auto compare_c = [this](utf8::utfchar32_t clhs, utf8::utfchar32_t crhs) {
		return _search_case_sensitive ? clhs == crhs : std::tolower(clhs) == std::tolower(crhs);
	};

	// Start search at the cursor position
	text_pos match_pos_beg, search_pos = backwards != with_selection ? _select_beg : _select_end;

	const utf8::unchecked::iterator<std::string_view::iterator> text_begin(text.begin());
	const utf8::unchecked::iterator<std::string_view::iterator> text_end(text.end());

	if (backwards)
	{
		// Start searching at the character before the current cursor position
		if (search_pos.column != 0)
		{
			search_pos.column -= 1;
		}
		else if (search_pos.line != 0)
		{
			search_pos.line -= 1;
			search_pos.column = _lines[search_pos.line].size();
		}

		const utf8::unchecked::iterator<std::string_view::iterator> match_last = std::prev(text_end);
		utf8::unchecked::iterator<std::string_view::iterator> match_offset = match_last;

		while (true)
		{
			if (!_lines[search_pos.line].empty())
			{
				// Trim column index to the last character in the line (rather than the actual end)
				search_pos.column = std::min(search_pos.column, _lines[search_pos.line].size() - 1);

				while (true)
				{
					if (compare_c(_lines[search_pos.line][search_pos.column].c, *match_offset))
					{
						if (match_offset == match_last) // Keep track of end of the match
							match_pos_beg = search_pos;

						// All characters matching means the text was found, so select it and return
						if (match_offset == text_begin)
						{
							if (!_search_whole_word || (match_pos_beg.column + 1 >= _lines[match_pos_beg.line].size() || _lines[match_pos_beg.line][match_pos_beg.column + 1].col != _lines[match_pos_beg.line][match_pos_beg.column].col))
							{
								_select_beg = search_pos;
								_select_end = text_pos(match_pos_beg.line, match_pos_beg.column + 1);
								_cursor_pos = _select_beg;
								_scroll_to_cursor = true;
								return true;
							}
							else
							{
								match_offset = match_last;
							}
						}
						else
						{
							--match_offset;
						}
					}
					else
					{
						// A character mismatched, so start from the end again
						match_offset = match_last;
					}

					if (search_pos.column == 0)
						break; // Reached the first column and cannot go further back, so break and go up the next line
					else
						search_pos.column -= 1;
				}
			}

			if (search_pos.line == 0)
				break; // Reached the first line and cannot go further back, so abort
			else
				search_pos.line -= 1;

			if (match_last != match_offset && *match_offset-- != '\n')
				match_offset = match_last; // Check for line feed in search text between lines

			search_pos.column = _lines[search_pos.line].size(); // Continue at end of previous line
		}
	}
	else
	{
		utf8::unchecked::iterator<std::string_view::iterator> match_offset = text_begin;

		while (search_pos.line < _lines.size())
		{
			if (text_begin != match_offset && *match_offset++ != '\n')
				match_offset = text_begin; // Check for line feed in search text between lines

			while (search_pos.column < _lines[search_pos.line].size())
			{
				if (compare_c(_lines[search_pos.line][search_pos.column].c, *match_offset))
				{
					if (match_offset == text_begin) // Keep track of beginning of the match
						match_pos_beg = search_pos;

					// All characters matching means the text was found, so select it and return
					if ((++match_offset) == text_end)
					{
						if (!_search_whole_word || (search_pos.column + 1 >= _lines[search_pos.line].size() || _lines[search_pos.line][search_pos.column + 1].col != _lines[search_pos.line][search_pos.column].col))
						{
							_select_beg = match_pos_beg;
							_select_end = text_pos(search_pos.line, search_pos.column + 1);
							_cursor_pos = _select_end;
							_scroll_to_cursor = true;
							return true;
						}
						else
						{
							match_offset = text_begin;
						}
					}
				}
				else
				{
					// A character mismatched, so start from the beginning again
					match_offset = text_begin;
				}

				search_pos.column += 1;
			}

			search_pos.line += 1;
			search_pos.column = 0;
		}
	}

	return false; // No match found
}

#include "effect_lexer.hpp"

void reshade::imgui::code_editor::colorize()
{
	if (_colorize_line_beg >= _colorize_line_end)
		return;

	// Step through code incrementally rather than coloring everything at once
	const size_t from = _colorize_line_beg, to = std::min(from + 1000, _colorize_line_end);
	_colorize_line_beg = to;

	// Reset coloring range if we have finished coloring it after this iteration
	if (_colorize_line_end > _lines.size())
		_colorize_line_end = _lines.size();
	if (_colorize_line_beg == _colorize_line_end)
	{
		_colorize_line_beg = std::numeric_limits<size_t>::max();
		_colorize_line_end = 0;
	}

	// Copy lines into string for consumption by the lexer (needs to use the same offsets as the indices in '_lines', so strip any unicode characters which are multi-byte)
	std::string input_string;
	for (size_t l = from; l < to && l < _lines.size(); ++l, input_string.push_back('\n'))
		for (size_t k = 0; k < _lines[l].size(); ++k)
			input_string += _lines[l][k].c < 0x80 ? static_cast<char>(_lines[l][k].c) : '?';

	reshadefx::lexer lexer(
		std::move(input_string),
		false /* ignore_comments */,
		true  /* ignore_whitespace */,
		false /* ignore_pp_directives */,
		true  /* ignore_line_directives */,
		false /* ignore_keywords */,
		false /* escape_string_literals */);

	for (reshadefx::token tok; (tok = lexer.lex()).id != reshadefx::tokenid::end_of_file;)
	{
		color col = color_default;

		switch (tok.id)
		{
		case reshadefx::tokenid::exclaim:
		case reshadefx::tokenid::percent:
		case reshadefx::tokenid::ampersand:
		case reshadefx::tokenid::parenthesis_open:
		case reshadefx::tokenid::parenthesis_close:
		case reshadefx::tokenid::star:
		case reshadefx::tokenid::plus:
		case reshadefx::tokenid::comma:
		case reshadefx::tokenid::minus:
		case reshadefx::tokenid::dot:
		case reshadefx::tokenid::slash:
		case reshadefx::tokenid::colon:
		case reshadefx::tokenid::semicolon:
		case reshadefx::tokenid::less:
		case reshadefx::tokenid::equal:
		case reshadefx::tokenid::greater:
		case reshadefx::tokenid::question:
		case reshadefx::tokenid::bracket_open:
		case reshadefx::tokenid::backslash:
		case reshadefx::tokenid::bracket_close:
		case reshadefx::tokenid::caret:
		case reshadefx::tokenid::brace_open:
		case reshadefx::tokenid::pipe:
		case reshadefx::tokenid::brace_close:
		case reshadefx::tokenid::tilde:
		case reshadefx::tokenid::exclaim_equal:
		case reshadefx::tokenid::percent_equal:
		case reshadefx::tokenid::ampersand_ampersand:
		case reshadefx::tokenid::ampersand_equal:
		case reshadefx::tokenid::star_equal:
		case reshadefx::tokenid::plus_plus:
		case reshadefx::tokenid::plus_equal:
		case reshadefx::tokenid::minus_minus:
		case reshadefx::tokenid::minus_equal:
		case reshadefx::tokenid::arrow:
		case reshadefx::tokenid::ellipsis:
		case reshadefx::tokenid::slash_equal:
		case reshadefx::tokenid::colon_colon:
		case reshadefx::tokenid::less_less_equal:
		case reshadefx::tokenid::less_less:
		case reshadefx::tokenid::less_equal:
		case reshadefx::tokenid::equal_equal:
		case reshadefx::tokenid::greater_greater_equal:
		case reshadefx::tokenid::greater_greater:
		case reshadefx::tokenid::greater_equal:
		case reshadefx::tokenid::caret_equal:
		case reshadefx::tokenid::pipe_equal:
		case reshadefx::tokenid::pipe_pipe:
			col = color_punctuation;
			break;
		case reshadefx::tokenid::identifier:
			col = color_identifier;
			break;
		case reshadefx::tokenid::int_literal:
		case reshadefx::tokenid::uint_literal:
		case reshadefx::tokenid::float_literal:
		case reshadefx::tokenid::double_literal:
			col = color_number_literal;
			break;
		case reshadefx::tokenid::string_literal:
			col = color_string_literal;
			break;
		case reshadefx::tokenid::true_literal:
		case reshadefx::tokenid::false_literal:
		case reshadefx::tokenid::namespace_:
		case reshadefx::tokenid::struct_:
		case reshadefx::tokenid::technique:
		case reshadefx::tokenid::pass:
		case reshadefx::tokenid::for_:
		case reshadefx::tokenid::while_:
		case reshadefx::tokenid::do_:
		case reshadefx::tokenid::if_:
		case reshadefx::tokenid::else_:
		case reshadefx::tokenid::switch_:
		case reshadefx::tokenid::case_:
		case reshadefx::tokenid::default_:
		case reshadefx::tokenid::break_:
		case reshadefx::tokenid::continue_:
		case reshadefx::tokenid::return_:
		case reshadefx::tokenid::discard_:
		case reshadefx::tokenid::extern_:
		case reshadefx::tokenid::static_:
		case reshadefx::tokenid::uniform_:
		case reshadefx::tokenid::volatile_:
		case reshadefx::tokenid::precise:
		case reshadefx::tokenid::groupshared:
		case reshadefx::tokenid::in:
		case reshadefx::tokenid::out:
		case reshadefx::tokenid::inout:
		case reshadefx::tokenid::const_:
		case reshadefx::tokenid::linear:
		case reshadefx::tokenid::noperspective:
		case reshadefx::tokenid::centroid:
		case reshadefx::tokenid::nointerpolation:
		case reshadefx::tokenid::void_:
		case reshadefx::tokenid::bool_:
		case reshadefx::tokenid::bool2:
		case reshadefx::tokenid::bool3:
		case reshadefx::tokenid::bool4:
		case reshadefx::tokenid::bool2x2:
		case reshadefx::tokenid::bool2x3:
		case reshadefx::tokenid::bool2x4:
		case reshadefx::tokenid::bool3x2:
		case reshadefx::tokenid::bool3x3:
		case reshadefx::tokenid::bool3x4:
		case reshadefx::tokenid::bool4x2:
		case reshadefx::tokenid::bool4x3:
		case reshadefx::tokenid::bool4x4:
		case reshadefx::tokenid::int_:
		case reshadefx::tokenid::int2:
		case reshadefx::tokenid::int3:
		case reshadefx::tokenid::int4:
		case reshadefx::tokenid::int2x2:
		case reshadefx::tokenid::int2x3:
		case reshadefx::tokenid::int2x4:
		case reshadefx::tokenid::int3x2:
		case reshadefx::tokenid::int3x3:
		case reshadefx::tokenid::int3x4:
		case reshadefx::tokenid::int4x2:
		case reshadefx::tokenid::int4x3:
		case reshadefx::tokenid::int4x4:
		case reshadefx::tokenid::min16int:
		case reshadefx::tokenid::min16int2:
		case reshadefx::tokenid::min16int3:
		case reshadefx::tokenid::min16int4:
		case reshadefx::tokenid::min16int2x2:
		case reshadefx::tokenid::min16int2x3:
		case reshadefx::tokenid::min16int2x4:
		case reshadefx::tokenid::min16int3x2:
		case reshadefx::tokenid::min16int3x3:
		case reshadefx::tokenid::min16int3x4:
		case reshadefx::tokenid::min16int4x2:
		case reshadefx::tokenid::min16int4x3:
		case reshadefx::tokenid::min16int4x4:
		case reshadefx::tokenid::uint_:
		case reshadefx::tokenid::uint2:
		case reshadefx::tokenid::uint3:
		case reshadefx::tokenid::uint4:
		case reshadefx::tokenid::uint2x2:
		case reshadefx::tokenid::uint2x3:
		case reshadefx::tokenid::uint2x4:
		case reshadefx::tokenid::uint3x2:
		case reshadefx::tokenid::uint3x3:
		case reshadefx::tokenid::uint3x4:
		case reshadefx::tokenid::uint4x2:
		case reshadefx::tokenid::uint4x3:
		case reshadefx::tokenid::uint4x4:
		case reshadefx::tokenid::min16uint:
		case reshadefx::tokenid::min16uint2:
		case reshadefx::tokenid::min16uint3:
		case reshadefx::tokenid::min16uint4:
		case reshadefx::tokenid::min16uint2x2:
		case reshadefx::tokenid::min16uint2x3:
		case reshadefx::tokenid::min16uint2x4:
		case reshadefx::tokenid::min16uint3x2:
		case reshadefx::tokenid::min16uint3x3:
		case reshadefx::tokenid::min16uint3x4:
		case reshadefx::tokenid::min16uint4x2:
		case reshadefx::tokenid::min16uint4x3:
		case reshadefx::tokenid::min16uint4x4:
		case reshadefx::tokenid::float_:
		case reshadefx::tokenid::float2:
		case reshadefx::tokenid::float3:
		case reshadefx::tokenid::float4:
		case reshadefx::tokenid::float2x2:
		case reshadefx::tokenid::float2x3:
		case reshadefx::tokenid::float2x4:
		case reshadefx::tokenid::float3x2:
		case reshadefx::tokenid::float3x3:
		case reshadefx::tokenid::float3x4:
		case reshadefx::tokenid::float4x2:
		case reshadefx::tokenid::float4x3:
		case reshadefx::tokenid::float4x4:
		case reshadefx::tokenid::min16float:
		case reshadefx::tokenid::min16float2:
		case reshadefx::tokenid::min16float3:
		case reshadefx::tokenid::min16float4:
		case reshadefx::tokenid::min16float2x2:
		case reshadefx::tokenid::min16float2x3:
		case reshadefx::tokenid::min16float2x4:
		case reshadefx::tokenid::min16float3x2:
		case reshadefx::tokenid::min16float3x3:
		case reshadefx::tokenid::min16float3x4:
		case reshadefx::tokenid::min16float4x2:
		case reshadefx::tokenid::min16float4x3:
		case reshadefx::tokenid::min16float4x4:
		case reshadefx::tokenid::vector:
		case reshadefx::tokenid::matrix:
		case reshadefx::tokenid::string_:
		case reshadefx::tokenid::texture1d:
		case reshadefx::tokenid::texture2d:
		case reshadefx::tokenid::texture3d:
		case reshadefx::tokenid::sampler1d:
		case reshadefx::tokenid::sampler2d:
		case reshadefx::tokenid::sampler3d:
		case reshadefx::tokenid::storage1d:
		case reshadefx::tokenid::storage2d:
		case reshadefx::tokenid::storage3d:
			col = color_keyword;
			break;
		case reshadefx::tokenid::hash_def:
		case reshadefx::tokenid::hash_undef:
		case reshadefx::tokenid::hash_if:
		case reshadefx::tokenid::hash_ifdef:
		case reshadefx::tokenid::hash_ifndef:
		case reshadefx::tokenid::hash_else:
		case reshadefx::tokenid::hash_elif:
		case reshadefx::tokenid::hash_endif:
		case reshadefx::tokenid::hash_error:
		case reshadefx::tokenid::hash_warning:
		case reshadefx::tokenid::hash_pragma:
		case reshadefx::tokenid::hash_include:
		case reshadefx::tokenid::hash_unknown:
			col = color_preprocessor;
			// Find matching starting hash
			assert(tok.offset > 0);
			do
			{
				tok.offset--;
				tok.length++;
			} while (tok.offset > 0 && lexer.input_string()[tok.offset] != '#');
			break;
		case reshadefx::tokenid::single_line_comment:
			col = color_comment;
			break;
		case reshadefx::tokenid::multi_line_comment:
			col = color_multiline_comment;
			break;
		}

		// Update character range matching the current the token
		size_t line = from + tok.location.line - 1;
		size_t column = tok.location.column - 1;

		for (size_t k = 0; k < tok.length; ++k)
		{
			if (column >= _lines[line].size())
			{
				line++;
				column = 0;
				continue;
			}

			_lines[line][column++].col = col;
		}
	}
}

void reshade::imgui::code_editor::colorize(const text_pos &beg, const text_pos &end, color col)
{
	for (size_t l = beg.line; l <= end.line && l < _lines.size(); ++l)
		for (size_t k = (l == beg.line ? beg.column : 0); k < _lines[l].size() && (l != end.line || k < end.column); ++k)
			_lines[l][k].col = col;

	_colorize_line_beg = std::numeric_limits<size_t>::max();
	_colorize_line_end = 0;
}
