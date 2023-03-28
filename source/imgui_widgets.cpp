/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "input.hpp" // input::key_name
#include "imgui_widgets.hpp"
#include "fonts/forkawesome.h"
#include <cassert>

bool reshade::imgui::path_list(const char *label, std::vector<std::filesystem::path> &paths, std::filesystem::path &dialog_path, const std::filesystem::path &default_path)
{
	bool res = false;
	const float item_width = ImGui::CalcItemWidth();
	const float item_height = ImGui::GetFrameHeightWithSpacing();
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(label);

	if (ImGui::BeginChild("##paths", ImVec2(item_width, (paths.size() + 1) * item_height), false, ImGuiWindowFlags_NoScrollWithMouse))
	{
		for (size_t i = 0; i < paths.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));

			char buf[4096];
			const size_t buf_len = paths[i].u8string().copy(buf, sizeof(buf) - 1);
			buf[buf_len] = '\0';

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - (button_spacing + button_size));
			if (ImGui::InputText("##path", buf, sizeof(buf)))
			{
				res = true;
				paths[i] = std::filesystem::u8path(buf);
			}

			ImGui::SameLine(0, button_spacing);
			if (ImGui::Button(ICON_FK_MINUS, ImVec2(button_size, 0)))
			{
				res = true;
				paths.erase(paths.begin() + i--);
			}

			ImGui::PopID();
		}

		ImGui::Dummy(ImVec2(0, 0));
		ImGui::SameLine(0, ImGui::GetContentRegionAvail().x - button_size);
		if (ImGui::Button(ICON_FK_PLUS, ImVec2(button_size, 0)))
		{
			// Do not show directory dialog when Alt key is pressed
			if (ImGui::GetIO().KeyAlt)
			{
				res = true;

				// Take last the last path of the list if possible.
				paths.push_back(paths.empty() ? default_path : paths.back());
			}
			else
			{
				dialog_path = default_path;
				if (dialog_path.has_stem())
					dialog_path += std::filesystem::path::preferred_separator;
				ImGui::OpenPopup("##select");
			}
		}

		// Show directory dialog
		if (file_dialog("##select", dialog_path, 500, {}))
		{
			res = true;
			paths.push_back(dialog_path);
		}
	}

	ImGui::EndChild();

	ImGui::PopID();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return res;
}

bool reshade::imgui::file_dialog(const char *name, std::filesystem::path &path, float width, const std::vector<std::wstring> &exts, const std::vector<std::filesystem::path> &hidden_paths)
{
	if (!ImGui::BeginPopup(name))
		return false;

	std::error_code ec;
	if (path.empty())
		path = L".\\";
	if (path.is_relative())
		path = std::filesystem::absolute(path, ec);
	std::filesystem::path parent_path = path.parent_path();

	{	char buf[4096];
		const size_t buf_len = parent_path.u8string().copy(buf, sizeof(buf) - 1);
		buf[buf_len] = '\0';

		ImGui::SetNextItemWidth(width);
		if (ImGui::InputText("##path", buf, sizeof(buf)))
		{
			path = std::filesystem::u8path(buf);
			if ((path.has_stem() && std::filesystem::is_directory(path, ec)) || (path.has_root_name() && path == path.root_name()))
				path += std::filesystem::path::preferred_separator;
			parent_path = path.parent_path();
		}

		if (ImGui::IsItemActivated())
			ImGui::GetCurrentContext()->InputTextState.ClearSelection();
		if (ImGui::IsWindowAppearing())
			ImGui::SetKeyboardFocusHere(1);
	}

	ImGui::BeginChild("##files", ImVec2(width, 200), true, ImGuiWindowFlags_NavFlattened);

	if (parent_path.has_parent_path() && parent_path != parent_path.root_path())
	{
		if (ImGui::Selectable(ICON_FK_FOLDER " ..", false, ImGuiSelectableFlags_AllowDoubleClick) && (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || ImGui::IsNavInputTest(ImGuiNavInput_Activate, ImGuiInputReadMode_Pressed)))
		{
			path = parent_path.parent_path();
			if (path.has_stem())
				path += std::filesystem::path::preferred_separator;
		}
	}

	std::vector<std::filesystem::path> file_entries;
	for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(parent_path, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		if (entry.is_directory())
		{
			const bool is_selected = entry == path;

			const std::string label = ICON_FK_FOLDER " " + entry.path().filename().u8string();
			if (ImGui::Selectable(label.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick))
			{
				path = entry;

				// Navigate into directory when double clicking one
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || ImGui::IsNavInputTest(ImGuiNavInput_Activate, ImGuiInputReadMode_Pressed))
					path += std::filesystem::path::preferred_separator;
			}

			if (is_selected && ImGui::IsWindowAppearing())
				ImGui::SetScrollHereY();
			continue;
		}

		if (std::find(exts.cbegin(), exts.cend(), entry.path().extension()) != exts.cend() &&
			std::find(hidden_paths.cbegin(), hidden_paths.cend(), entry.path()) == hidden_paths.cend())
			file_entries.push_back(entry);
	}

	// Always show file entries after all directory entries
	bool has_double_clicked_file = false;
	for (std::filesystem::path &file_path : file_entries)
	{
		const bool is_selected = file_path == path;

		std::string label = ICON_FK_FILE " ";
		if (const std::filesystem::path ext = file_path.extension();
			ext == L".fx" || ext == L".fxh")
			label = ICON_FK_FILE_CODE " " + label;
		else if (ext == L".bmp" || ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".dds")
			label = ICON_FK_FILE_IMAGE " " + label;
		label += file_path.filename().u8string();

		if (ImGui::Selectable(label.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick))
		{
			path = std::move(file_path);

			// Double clicking a file on the other hand acts as if pressing the ok button
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || ImGui::IsNavInputTest(ImGuiNavInput_Activate, ImGuiInputReadMode_Pressed))
				has_double_clicked_file = true;
		}

		if (is_selected && ImGui::IsWindowAppearing())
			ImGui::SetScrollHereY();
	}

	ImGui::EndChild();

	std::filesystem::path path_name = path.has_filename() || !exts.empty() ? path.filename() : path.parent_path().filename();
	const float button_size = 6.0f * ImGui::GetFontSize();

	{	char buf[4096];
		const size_t buf_len = path_name.u8string().copy(buf, sizeof(buf) - 1);
		buf[buf_len] = '\0';

		ImGui::SetNextItemWidth(std::max(0.0f, width - (2 * (button_size + ImGui::GetStyle().ItemSpacing.x))));
		if (ImGui::InputText("##name", buf, sizeof(buf)))
			path = path.parent_path() / buf;
	}

	ImGui::SameLine();
	const bool select = ImGui::Button(ICON_FK_OK " Select", ImVec2(button_size, 0));
	ImGui::SameLine();
	const bool cancel = ImGui::Button(ICON_FK_CANCEL " Cancel", ImVec2(button_size, 0));

	// Navigate into directory when clicking select button
	if (select && path.has_stem() && std::filesystem::is_directory(path, ec))
		path += std::filesystem::path::preferred_separator;

	const bool result = (select || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) || has_double_clicked_file) && (exts.empty() || std::find(exts.cbegin(), exts.cend(), path.extension()) != exts.cend());
	if (result || cancel)
		ImGui::CloseCurrentPopup();

	ImGui::EndPopup();

	return result;
}

bool reshade::imgui::key_input_box(const char *name, unsigned int key[4], const reshade::input &input)
{
	bool res = false;
	char buf[48]; buf[0] = '\0';
	if (key[0] || key[1] || key[2] || key[3])
		buf[input::key_name(key).copy(buf, sizeof(buf) - 1)] = '\0';

	ImGui::BeginDisabled(ImGui::GetCurrentContext()->NavInputSource == ImGuiInputSource_Gamepad);

	ImGui::InputTextWithHint(name, "Click to set keyboard shortcut", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_NoHorizontalScroll);

	if (ImGui::IsItemActive())
	{
		const unsigned int last_key_pressed = input.last_key_pressed();
		if (last_key_pressed != 0)
		{
			if (last_key_pressed == static_cast<unsigned int>(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
			{
				key[0] = 0;
				key[1] = 0;
				key[2] = 0;
				key[3] = 0;
			}
			else if (last_key_pressed < 0x10 || last_key_pressed > 0x12) // Exclude modifier keys
			{
				key[0] = last_key_pressed;
				key[1] = input.is_key_down(0x11); // Ctrl
				key[2] = input.is_key_down(0x10); // Shift
				key[3] = input.is_key_down(0x12); // Alt
			}

			res = true;
		}
	}
	else if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
	}

	ImGui::EndDisabled();

	return res;
}

bool reshade::imgui::font_input_box(const char *name, std::filesystem::path &path, std::filesystem::path &dialog_path, int &size)
{
	bool res = false;
	const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(name);

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - spacing - 80);
	if (file_input_box("##font", nullptr, path, dialog_path, { L".ttf" }))
		res = true;

	// Reset to the default font name if path is empty
	if (path.empty())
		path = L"ProggyClean.ttf";

	ImGui::SameLine(0, spacing);
	ImGui::SetNextItemWidth(80);
	if (ImGui::SliderInt("##size", &size, 8, 32))
		res = true;

	ImGui::PopID();

	ImGui::SameLine(0, spacing);
	ImGui::TextUnformatted(name);

	ImGui::EndGroup();

	return res;
}

bool reshade::imgui::search_input_box(char *filter, int filter_size, float width)
{
	bool res = false;
	const bool show_clear_button = filter[0] != '\0';

	if (ImGui::InputTextEx("##filter", "Search " ICON_FK_SEARCH, filter, filter_size,
			ImVec2(width - (show_clear_button ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x : 0.0001f), 0), ImGuiInputTextFlags_AutoSelectAll))
		res = true;

	if (show_clear_button)
	{
		ImGui::SameLine();

		if (ImGui::Button(ICON_FK_CANCEL, ImVec2(ImGui::GetFrameHeight(), 0)))
			res = true, filter[0] = '\0';
	}

	return res;
}

bool reshade::imgui::file_input_box(const char *name, const char *hint, std::filesystem::path &path, std::filesystem::path &dialog_path, const std::vector<std::wstring> &exts)
{
	bool res = false;
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::PushID(name);
	ImGui::BeginGroup();

	char buf[4096];
	const size_t buf_len = path.u8string().copy(buf, sizeof(buf) - 1);
	buf[buf_len] = '\0'; // Null-terminate string

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - (button_spacing + button_size));
	if (ImGui::InputTextWithHint("##path", hint, buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		dialog_path = std::filesystem::u8path(buf);
		// Succeed only if extension matches
		if (std::find(exts.cbegin(), exts.cend(), dialog_path.extension()) != exts.cend() || dialog_path.empty())
			path = dialog_path, res = true;
	}

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button(ICON_FK_FOLDER_OPEN, ImVec2(button_size, 0)))
	{
		dialog_path = path;
		ImGui::OpenPopup("##select");
	}

	if (name[0] != '#')
	{
		ImGui::SameLine(0, button_spacing);
		ImGui::TextUnformatted(name);
	}

	ImGui::EndGroup();

	// Show file selection dialog
	if (file_dialog("##select", dialog_path, 500, exts))
		path = dialog_path, res = true;

	ImGui::PopID();

	return res;
}
bool reshade::imgui::directory_input_box(const char *name, std::filesystem::path &path, std::filesystem::path &dialog_path)
{
	bool res = false;
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::PushID(name);
	ImGui::BeginGroup();

	char buf[4096];
	const size_t buf_len = path.u8string().copy(buf, sizeof(buf) - 1);
	buf[buf_len] = '\0'; // Null-terminate string

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - (button_spacing + button_size));
	if (ImGui::InputText("##path", buf, sizeof(buf)))
		path = std::filesystem::u8path(buf), res = true;

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button(ICON_FK_FOLDER_OPEN, ImVec2(button_size, 0)))
	{
		dialog_path = path;
		// Add separator at end so that file dialog navigates into this directory
		if (dialog_path.has_stem())
			dialog_path += std::filesystem::path::preferred_separator;
		ImGui::OpenPopup("##select");
	}

	if (name[0] != '#')
	{
		ImGui::SameLine(0, button_spacing);
		ImGui::TextUnformatted(name);
	}

	ImGui::EndGroup();

	// Show directory dialog
	if (file_dialog("##select", dialog_path, 500, {}))
		path = dialog_path, res = true;

	ImGui::PopID();

	return res;
}

bool reshade::imgui::radio_list(const char *label, const std::string_view ui_items, int &v)
{
	bool modified = false;
	const float item_width = ImGui::CalcItemWidth();

	// Group all radio buttons together into a list
	ImGui::BeginGroup();

	for (size_t offset = 0, next, i = 0; (next = ui_items.find('\0', offset)) != std::string_view::npos; offset = next + 1, ++i)
		modified |= ImGui::RadioButton(ui_items.data() + offset, &v, static_cast<int>(i));

	ImGui::EndGroup();

	ImGui::SameLine(ImGui::GetCursorPosX() + item_width, ImGui::GetStyle().ItemInnerSpacing.x);
	ImGui::TextUnformatted(label);

	return modified;
}

bool reshade::imgui::popup_button(const char *label, float width, ImGuiWindowFlags flags)
{
	if (ImGui::Button(label, ImVec2(width, 0)))
		ImGui::OpenPopup(label); // Popups can have the same ID as other items without conflict
	return ImGui::BeginPopup(label, flags);
}

bool reshade::imgui::toggle_button(const char *label, bool &v, float width, ImGuiButtonFlags flags)
{
	if (v)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
	const bool modified = ImGui::ButtonEx(label, ImVec2(width, 0), flags);
	if (v)
		ImGui::PopStyleColor();

	if (modified)
		v = !v;

	return modified;
}

bool reshade::imgui::confirm_button(const char *label, float width, const char *message, ...)
{
	bool modified = false;

	if (popup_button(label, width))
	{
		va_list args;
		va_start(args, message);
		ImGui::TextV(message, args);
		va_end(args);

		const float button_width = (ImGui::GetContentRegionAvail().x / 2) - ImGui::GetStyle().ItemInnerSpacing.x;

		if (ImGui::Button(ICON_FK_OK " Yes", ImVec2(button_width, 0)))
		{
			ImGui::CloseCurrentPopup();
			modified = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FK_CANCEL " No", ImVec2(button_width, 0)))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	return modified;
}

bool reshade::imgui::list_with_buttons(const char *label, const std::string_view ui_items, int &v)
{
	bool modified = false;

	std::vector<std::string_view> items;
	if (!ui_items.empty())
		for (std::string_view item = ui_items.data(); ui_items.data() + ui_items.size() > item.data();
			item = item.data() + item.size() + 1)
			items.push_back(item);

	ImGui::BeginGroup();

	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImVec2 hover_pos = ImGui::GetCursorScreenPos();
	hover_pos.y += button_size;

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - (button_spacing * 2 + button_size * 2));
	if (ImGui::BeginCombo("##v", items.size() > static_cast<size_t>(v) && v >= 0 ? items[v].data() : nullptr, ImGuiComboFlags_NoArrowButton))
	{
		auto it = items.cbegin();
		for (size_t i = 0; it != items.cend(); ++it, ++i)
		{
			bool selected = v == static_cast<int>(i);
			if (ImGui::Selectable(it->data(), &selected))
				v = static_cast<int>(i), modified = true;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}

	ImGui::BeginDisabled(items.empty());

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button("<", ImVec2(button_size, 0)))
	{
		modified = true;
		v = (v == 0) ? static_cast<int>(items.size() - 1) : v - 1;
	}

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button(">", ImVec2(button_size, 0)))
	{
		modified = true;
		v = (v == static_cast<int>(items.size() - 1)) ? 0 : v + 1;
	}

	ImGui::EndDisabled();

	ImGui::EndGroup();
	const bool is_hovered = ImGui::IsItemHovered();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	if (is_hovered)
	{
		const ImGuiStyle &style = ImGui::GetStyle();

		ImGui::SetNextWindowPos(hover_pos);
		ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::CalcItemWidth(), 0.0f), ImVec2(FLT_MAX, (ImGui::GetFontSize() + style.ItemSpacing.y) * 8 - style.ItemSpacing.y + (style.WindowPadding.y * 2))); // 8 by ImGuiComboFlags_HeightRegular
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.FramePadding.x, style.WindowPadding.y));
		ImGui::Begin("##spinner_items", NULL, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);
		ImGui::PopStyleVar();

		auto it = items.cbegin();
		for (size_t i = 0; it != items.cend(); ++it, ++i)
		{
			bool selected = v == static_cast<int>(i);
			ImGui::Selectable(it->data(), &selected);
			if (selected)
				ImGui::SetScrollHereY();
		}

		ImGui::End();
	}

	return modified;
}

bool reshade::imgui::combo_with_buttons(const char *label, bool &v)
{
	int current_item = v ? 1 : 0;
	bool modified = combo_with_buttons(label, "Off\0On\0", current_item);
	v = current_item != 0;
	return modified;
}
bool reshade::imgui::combo_with_buttons(const char *label, const std::string_view ui_items, int &v)
{
	size_t num_items = 0;
	std::string items;
	items.reserve(ui_items.size());
	for (size_t offset = 0, next; (next = ui_items.find('\0', offset)) != std::string_view::npos; offset = next + 1, ++num_items)
	{
		items += ui_items.data() + offset;
		items += '\0'; // Terminate item in the combo list
	}

	ImGui::BeginGroup();

	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - (button_spacing * 2 + button_size * 2));
	bool modified = ImGui::Combo("##v", &v, items.c_str());

	ImGui::BeginDisabled(num_items == 0);

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button("<", ImVec2(button_size, 0)))
	{
		modified = true;
		v = (v == 0) ? static_cast<int>(num_items - 1) : v - 1;
	}

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button(">", ImVec2(button_size, 0)))
	{
		modified = true;
		v = (v == static_cast<int>(num_items - 1)) ? 0 : v + 1;
	}

	ImGui::EndDisabled();

	ImGui::EndGroup();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	return modified;
}

template <typename T, ImGuiDataType data_type>
static bool drag_with_buttons(const char *label, T *v, int components, T v_speed, T v_min, T v_max, const char *format)
{
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(label);

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - (button_spacing * 2 + button_size * 2));
	bool value_changed = (components < 2) ?
		ImGui::DragScalar("##v", data_type, v, static_cast<float>(v_speed), &v_min, &v_max, format) :
		ImGui::DragScalarN("##v", data_type, v, components, static_cast<float>(v_speed), &v_min, &v_max, format);

	const bool ignore_limits = ImGui::GetIO().KeyCtrl;

	ImGui::SameLine(0, button_spacing);
	if (ImGui::ButtonEx("<", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_Repeat) && (ignore_limits || v[0] > v_min))
	{
		for (int c = 0; c < components; ++c)
		{
			v[c] -= v_speed;
			if (!ignore_limits)
				v[c] = std::max(v[c], v_min);
		}
		value_changed = true;
	}
	ImGui::SameLine(0, button_spacing);
	if (ImGui::ButtonEx(">", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_Repeat) && (ignore_limits || v[0] < v_max))
	{
		for (int c = 0; c < components; ++c)
		{
			v[c] += v_speed;
			if (!ignore_limits)
				v[c] = std::min(v[c], v_max);
		}
		value_changed = true;
	}

	ImGui::PopID();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return value_changed;

}
bool reshade::imgui::drag_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format)
{
	switch (data_type)
	{
	default:
		assert(false); // Not implemented
		return false;
	case ImGuiDataType_S32:
		return ::drag_with_buttons<ImS32, ImGuiDataType_S32>(label, static_cast<ImS32 *>(v), components, *static_cast<const ImS32 *>(v_speed), *static_cast<const ImS32 *>(v_min), *static_cast<const ImS32 *>(v_max), format);
	case ImGuiDataType_U32:
		return ::drag_with_buttons<ImU32, ImGuiDataType_U32>(label, static_cast<ImU32 *>(v), components, *static_cast<const ImU32 *>(v_speed), *static_cast<const ImU32 *>(v_min), *static_cast<const ImU32 *>(v_max), format);
	case ImGuiDataType_Float:
		return ::drag_with_buttons<float, ImGuiDataType_Float>(label, static_cast<float *>(v), components, *static_cast<const float *>(v_speed), *static_cast<const float *>(v_min), *static_cast<const float *>(v_max), format);
#if 0
	case ImGuiDataType_S64:
		return ::drag_with_buttons<ImS64, ImGuiDataType_S64>(label, static_cast<ImS64 *>(v), components, *static_cast<const ImS64 *>(v_speed), *static_cast<const ImS64 *>(v_min), *static_cast<const ImS64 *>(v_max), format);
	case ImGuiDataType_U64:
		return ::drag_with_buttons<ImU64, ImGuiDataType_U64>(label, static_cast<ImU64 *>(v), components, *static_cast<const ImU64 *>(v_speed), *static_cast<const ImU64 *>(v_min), *static_cast<const ImU64 *>(v_max), format);
	case ImGuiDataType_Double:
		return ::drag_with_buttons<double, ImGuiDataType_Double>(label, static_cast<double *>(v), components, *static_cast<const double *>(v_speed), *static_cast<const double *>(v_min), *static_cast<const double *>(v_max), format);
#endif
	}
}

template <typename T, ImGuiDataType data_type>
static bool slider_with_buttons(const char *label, T *v, int components, T v_speed, T v_min, T v_max, const char *format)
{
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(label);

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - (button_spacing * 2 + button_size * 2));
	bool value_changed = (components < 2) ?
		ImGui::SliderScalar("##v", data_type, v, &v_min, &v_max, format) :
		ImGui::SliderScalarN("##v", data_type, v, components, &v_min, &v_max, format);

	ImGui::SameLine(0, button_spacing);
	if (ImGui::ButtonEx("<", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_Repeat) && v[0] > v_min)
	{
		for (int c = 0; c < components; ++c)
			v[c] = std::max(v[c] - v_speed, v_min);
		value_changed = true;
	}
	ImGui::SameLine(0, button_spacing);
	if (ImGui::ButtonEx(">", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_Repeat) && v[0] < v_max)
	{
		for (int c = 0; c < components; ++c)
			v[c] = std::min(v[c] + v_speed, v_max);
		value_changed = true;
	}

	ImGui::PopID();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return value_changed;

}
bool reshade::imgui::slider_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format)
{
	switch (data_type)
	{
	default:
		assert(false); // Not implemented
		return false;
	case ImGuiDataType_S32:
		return ::slider_with_buttons<ImS32, ImGuiDataType_S32>(label, static_cast<ImS32 *>(v), components, *static_cast<const ImS32 *>(v_speed), *static_cast<const ImS32 *>(v_min), *static_cast<const ImS32 *>(v_max), format);
	case ImGuiDataType_U32:
		return ::slider_with_buttons<ImU32, ImGuiDataType_U32>(label, static_cast<ImU32 *>(v), components, *static_cast<const ImU32 *>(v_speed), *static_cast<const ImU32 *>(v_min), *static_cast<const ImU32 *>(v_max), format);
	case ImGuiDataType_Float:
		return ::slider_with_buttons<float, ImGuiDataType_Float>(label, static_cast<float *>(v), components, *static_cast<const float *>(v_speed), *static_cast<const float *>(v_min), *static_cast<const float *>(v_max), format);
#if 0
	case ImGuiDataType_S64:
		return ::slider_with_buttons<ImS64, ImGuiDataType_S64>(label, static_cast<ImS64 *>(v), components, *static_cast<const ImS64 *>(v_speed), *static_cast<const ImS64 *>(v_min), *static_cast<const ImS64 *>(v_max), format);
	case ImGuiDataType_U64:
		return ::slider_with_buttons<ImU64, ImGuiDataType_U64>(label, static_cast<ImU64 *>(v), components, *static_cast<const ImU64 *>(v_speed), *static_cast<const ImU64 *>(v_min), *static_cast<const ImU64 *>(v_max), format);
	case ImGuiDataType_Double:
		return ::slider_with_buttons<double, ImGuiDataType_Double>(label, static_cast<double *>(v), components, *static_cast<const double *>(v_speed), *static_cast<const double *>(v_min), *static_cast<const double *>(v_max), format);
#endif
	}
}

bool reshade::imgui::slider_for_alpha_value(const char *label, float *v)
{
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(label);

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - button_spacing - button_size);
	const bool modified = ImGui::SliderFloat("##v", v, 0.0f, 1.0f);

	ImGui::SameLine(0, button_spacing);
	ImGui::ColorButton("##preview", ImVec4(1.0f, 1.0f, 1.0f, *v), ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoPicker);

	ImGui::PopID();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return modified;
}

bool reshade::imgui::checkbox_tristate(const char *label, unsigned int *v)
{
	const bool mixed = *v > 1;
	bool value = *v != 0;

	ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixed);
	const bool modified = ImGui::Checkbox(label, &value);
	if (modified)
		*v = value ? 1 : mixed ? 0 : 2;
	ImGui::PopItemFlag();

	return modified;
}

void reshade::imgui::image_with_checkerboard_background(ImTextureID user_texture_id, const ImVec2 &size, ImU32 tint_col)
{
	ImDrawList *const draw_list = ImGui::GetWindowDrawList();

	// Render background checkerboard pattern
	const ImVec2 pos_min = ImGui::GetCursorScreenPos();
	const ImVec2 pos_max = ImVec2(pos_min.x + size.x, pos_min.y + size.y); int yi = 0;

	for (float y = pos_min.y, grid_size = 25.0f; y < pos_max.y; y += grid_size, yi++)
	{
		const float y1 = std::min(y, pos_max.y), y2 = std::min(y + grid_size, pos_max.y);
		for (float x = pos_min.x + (yi & 1) * grid_size; x < pos_max.x; x += grid_size * 2.0f)
		{
			const float x1 = std::min(x, pos_max.x), x2 = std::min(x + grid_size, pos_max.x);
			draw_list->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(128, 128, 128, 255));
		}
	}

	// Add image on top
	ImGui::Image(user_texture_id, size, ImVec2(0, 0), ImVec2(1, 1), ImColor(tint_col));
}
