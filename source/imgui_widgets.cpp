/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "input.hpp" // input::key_name
#include "imgui_widgets.hpp"
#include <cassert>

bool reshade::gui::widgets::path_list(const char *label, std::vector<std::filesystem::path> &paths, std::filesystem::path &dialog_path, const std::filesystem::path &default_path)
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

			char buf[260];
			const size_t buf_len = paths[i].u8string().copy(buf, sizeof(buf) - 1);
			buf[buf_len] = '\0';

			ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - (button_spacing + button_size));
			if (ImGui::InputText("##path", buf, sizeof(buf)))
			{
				res = true;
				paths[i] = std::filesystem::u8path(buf);
			}
			ImGui::PopItemWidth();

			ImGui::SameLine(0, button_spacing);
			if (ImGui::Button("-", ImVec2(button_size, 0)))
			{
				res = true;
				paths.erase(paths.begin() + i--);
			}

			ImGui::PopID();
		}

		ImGui::Dummy(ImVec2(0, 0));
		ImGui::SameLine(0, ImGui::GetWindowContentRegionWidth() - button_size);
		if (ImGui::Button("+", ImVec2(button_size, 0)))
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

bool reshade::gui::widgets::file_dialog(const char *name, std::filesystem::path &path, float width, const std::vector<std::wstring> &exts)
{
	if (!ImGui::BeginPopup(name))
		return false;

	std::error_code ec;
	if (path.is_relative())
		path = std::filesystem::absolute(path);
	std::filesystem::path parent_path = path.parent_path();

	{	char buf[260];
		const size_t buf_len = parent_path.u8string().copy(buf, sizeof(buf) - 1);
		buf[buf_len] = '\0';

		ImGui::PushItemWidth(width);
		if (ImGui::InputText("##path", buf, sizeof(buf)))
		{
			path = std::filesystem::u8path(buf);
			if (path.has_stem() && std::filesystem::is_directory(path, ec))
				path += std::filesystem::path::preferred_separator;
		}
		ImGui::PopItemWidth();

		if (ImGui::IsItemActivated())
			ImGui::GetCurrentContext()->InputTextState.ClearSelection();
		if (ImGui::IsWindowAppearing())
			ImGui::SetKeyboardFocusHere(1);
	}

	ImGui::BeginChild("##files", ImVec2(width, 200), false, ImGuiWindowFlags_NavFlattened);

	if (parent_path.has_parent_path())
	{
		if (ImGui::Selectable(ICON_FOLDER " ..", false, ImGuiSelectableFlags_AllowDoubleClick) &&
			ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			path = parent_path.parent_path();
			if (path.has_stem())
				path += std::filesystem::path::preferred_separator;
		}
	}

	std::vector<std::filesystem::path> file_entries;
	for (const auto &entry : std::filesystem::directory_iterator(parent_path, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		if (entry.is_directory())
		{
			const bool is_selected = entry == path;
			const std::string label = ICON_FOLDER " " + entry.path().filename().u8string();
			if (ImGui::Selectable(label.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick))
			{
				path = entry;

				// Navigate into directory when double clicking one
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					path += std::filesystem::path::preferred_separator;
			}

			if (is_selected && ImGui::IsWindowAppearing())
				ImGui::SetScrollHereY();
		}
		else if (std::find(exts.begin(), exts.end(), entry.path().extension()) != exts.end())
		{
			file_entries.push_back(entry);
		}
	}

	// Always show file entries after all directory entries
	bool has_double_clicked_file = false;
	for (std::filesystem::path &file_path : file_entries)
	{
		const bool is_selected = file_path == path;
		const std::string label = ICON_FILE " " + file_path.filename().u8string();
		if (ImGui::Selectable(label.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick))
		{
			path = std::move(file_path);

			// Double clicking a file on the other hand acts as if pressing the ok button
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				has_double_clicked_file = true;
		}

		if (is_selected && ImGui::IsWindowAppearing())
			ImGui::SetScrollHereY();
	}

	ImGui::EndChild();

	std::filesystem::path path_name = path.has_filename() || !exts.empty() ? path.filename() : path.parent_path().filename();

	{	char buf[260];
		const size_t buf_len = path_name.u8string().copy(buf, sizeof(buf) - 1);
		buf[buf_len] = '\0';

		ImGui::PushItemWidth(width - (2 * (80 + ImGui::GetStyle().ItemSpacing.x)));
		if (ImGui::InputText("##name", buf, sizeof(buf)))
			path = path.parent_path() / buf;
		ImGui::PopItemWidth();
	}

	ImGui::SameLine();
	const bool select = ImGui::Button(ICON_OK " Select", ImVec2(80, 0));
	ImGui::SameLine();
	const bool cancel = ImGui::Button(ICON_CANCEL " Cancel", ImVec2(80, 0));

	// Navigate into directory when clicking select button
	if (select && path.has_stem() && std::filesystem::is_directory(path, ec))
		path += std::filesystem::path::preferred_separator;

	const bool result = (select || ImGui::IsKeyPressedMap(ImGuiKey_Enter) || has_double_clicked_file) && (exts.empty() || std::find(exts.begin(), exts.end(), path.extension()) != exts.end());
	if (result || cancel)
		ImGui::CloseCurrentPopup();

	ImGui::EndPopup();

	return result;
}

bool reshade::gui::widgets::key_input_box(const char *name, unsigned int key[4], const reshade::input &input)
{
	char buf[48] = "Click to set key shortcut";
	if (key[0] || key[1] || key[2] || key[3])
		buf[reshade::input::key_name(key).copy(buf, sizeof(buf) - 1)] = '\0';

	ImGui::InputText(name, buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_NoHorizontalScroll);

	if (ImGui::IsItemActive())
	{
		const unsigned int last_key_pressed = input.last_key_pressed();
		if (last_key_pressed != 0)
		{
			if (last_key_pressed == ImGui::GetKeyIndex(ImGuiKey_Backspace))
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

			return true;
		}
	}
	else if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
	}

	return false;
}

bool reshade::gui::widgets::font_input_box(const char *name, std::filesystem::path &path, std::filesystem::path &dialog_path, int &size)
{
	bool res = false;
	const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(name);

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - spacing - 80);
	if (file_input_box("##font", path, dialog_path, { L".ttf" }))
		res = true;
	ImGui::PopItemWidth();

	// Reset to the default font name if path is empty
	if (path.empty())
		path = L"ProggyClean.ttf";

	ImGui::SameLine(0, spacing);
	ImGui::PushItemWidth(80);
	if (ImGui::SliderInt("##size", &size, 8, 32))
		res = true;
	ImGui::PopItemWidth();

	ImGui::PopID();

	ImGui::SameLine(0, spacing);
	ImGui::TextUnformatted(name);

	ImGui::EndGroup();

	return res;
}

bool reshade::gui::widgets::file_input_box(const char *name, std::filesystem::path &path, std::filesystem::path &dialog_path, const std::vector<std::wstring> &exts)
{
	bool res = false;
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::PushID(name);
	ImGui::BeginGroup();

	char buf[260];
	const size_t buf_len = path.u8string().copy(buf, sizeof(buf) - 1);
	buf[buf_len] = '\0'; // Null-terminate string

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - (button_spacing + button_size));
	if (ImGui::InputText("##path", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		dialog_path = std::filesystem::u8path(buf);
		// Succeed only if extension matches
		if (std::find(exts.begin(), exts.end(), dialog_path.extension()) != exts.end() || dialog_path.empty())
			path = dialog_path, res = true;
	}
	ImGui::PopItemWidth();

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button(ICON_FOLDER_OPEN, ImVec2(button_size, 0)))
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
bool reshade::gui::widgets::directory_input_box(const char *name, std::filesystem::path &path, std::filesystem::path &dialog_path)
{
	bool res = false;
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::PushID(name);
	ImGui::BeginGroup();

	char buf[260];
	const size_t buf_len = path.u8string().copy(buf, sizeof(buf) - 1);
	buf[buf_len] = '\0'; // Null-terminate string

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - (button_spacing + button_size));
	if (ImGui::InputText("##path", buf, sizeof(buf)))
		path = std::filesystem::u8path(buf), res = true;
	ImGui::PopItemWidth();

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button(ICON_FOLDER_OPEN, ImVec2(button_size, 0)))
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

bool reshade::gui::widgets::radio_list(const char *label, const std::string_view ui_items, int &v)
{
	bool modified = false;
	const float item_width = ImGui::CalcItemWidth();

	// Group all radio buttons together into a list
	ImGui::BeginGroup();

	for (size_t offset = 0, next, i = 0; (next = ui_items.find('\0', offset)) != std::string::npos; offset = next + 1, ++i)
		modified |= ImGui::RadioButton(ui_items.data() + offset, &v, static_cast<int>(i));

	ImGui::EndGroup();

	ImGui::SameLine(ImGui::GetCursorPosX() + item_width, ImGui::GetStyle().ItemInnerSpacing.x);
	ImGui::TextUnformatted(label);

	return modified;
}

bool reshade::gui::widgets::popup_button(const char *label, float width, ImGuiWindowFlags flags)
{
	if (ImGui::Button(label, ImVec2(width, 0)))
		ImGui::OpenPopup(label); // Popups can have the same ID as other items without conflict
	return ImGui::BeginPopup(label, flags);
}

bool reshade::gui::widgets::toggle_button(const char *label, bool &v)
{
	if (v)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
	const bool modified = ImGui::SmallButton(label);
	if (v)
		ImGui::PopStyleColor();

	if (modified)
		v = !v;

	return modified;
}

bool reshade::gui::widgets::list_with_buttons(const char *label, const std::string_view ui_items, int &v)
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
	const ImVec2 hover_pos = ImGui::GetCursorScreenPos() + ImVec2(0, button_size);

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - (button_spacing * 2 + button_size * 2));

	if (ImGui::BeginCombo("##v", items.size() > static_cast<size_t>(v) && v >= 0 ? items[v].data() : nullptr, ImGuiComboFlags_NoArrowButton))
	{
		auto it = items.begin();
		for (size_t i = 0; it != items.end(); ++it, ++i)
		{
			bool selected = v == static_cast<int>(i);
			if (ImGui::Selectable(it->data(), &selected))
				v = static_cast<int>(i), modified = true;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}

	ImGui::PopItemWidth();

	ImGui::SameLine(0, button_spacing);
	if (ImGui::ButtonEx("<", ImVec2(button_size, 0), items.size() > 0 ? 0 : ImGuiButtonFlags_Disabled))
	{
		modified = true;
		v = (v == 0) ? static_cast<int>(items.size() - 1) : v - 1;
	}

	ImGui::SameLine(0, button_spacing);
	if (ImGui::ButtonEx(">", ImVec2(button_size, 0), items.size() > 0 ? 0 : ImGuiButtonFlags_Disabled))
	{
		modified = true;
		v = (v == static_cast<int>(items.size() - 1)) ? 0 : v + 1;
	}

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

		auto it = items.begin();
		for (size_t i = 0; it != items.end(); ++it, ++i)
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

bool reshade::gui::widgets::combo_with_buttons(const char *label, bool &v)
{
	int current_item = v ? 1 : 0;
	bool modified = combo_with_buttons(label, "Off\0On\0", current_item);
	v = current_item != 0;
	return modified;
}
bool reshade::gui::widgets::combo_with_buttons(const char *label, const std::string_view ui_items, int &v)
{
	size_t num_items = 0;
	std::string items;
	items.reserve(ui_items.size());
	for (size_t offset = 0, next; (next = ui_items.find('\0', offset)) != std::string::npos; offset = next + 1, ++num_items)
	{
		items += ui_items.data() + offset;
		items += '\0'; // Terminate item in the combo list
	}

	ImGui::BeginGroup();

	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - (button_spacing * 2 + button_size * 2));

	bool modified = ImGui::Combo("##v", &v, items.c_str());

	ImGui::PopItemWidth();

	ImGui::SameLine(0, button_spacing);
	if (ImGui::ButtonEx("<", ImVec2(button_size, 0), num_items > 0 ? 0 : ImGuiButtonFlags_Disabled))
	{
		modified = true;
		v = (v == 0) ? static_cast<int>(num_items - 1) : v - 1;
	}

	ImGui::SameLine(0, button_spacing);
	if (ImGui::ButtonEx(">", ImVec2(button_size, 0), num_items > 0 ? 0 : ImGuiButtonFlags_Disabled))
	{
		modified = true;
		v = (v == static_cast<int>(num_items - 1)) ? 0 : v + 1;
	}

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

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - (button_spacing * 2 + button_size * 2));
	bool value_changed = ImGui::DragScalarN("##v", data_type, v, components, static_cast<float>(v_speed), &v_min, &v_max, format);
	ImGui::PopItemWidth();

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
bool reshade::gui::widgets::drag_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format)
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

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - (button_spacing * 2 + button_size * 2));
	bool value_changed = ImGui::SliderScalarN("##v", data_type, v, components, &v_min, &v_max, format);
	ImGui::PopItemWidth();

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
bool reshade::gui::widgets::slider_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format)
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

bool reshade::gui::widgets::slider_for_alpha_value(const char *label, float *v)
{
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(label);

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - button_spacing - button_size);
	const bool modified = ImGui::SliderFloat("##v", v, 0.0f, 1.0f);
	ImGui::PopItemWidth();

	ImGui::SameLine(0, button_spacing);
	ImGui::ColorButton("##preview", ImVec4(1.0f, 1.0f, 1.0f, *v), ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoPicker);

	ImGui::PopID();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return modified;

}

void reshade::gui::widgets::image_with_checkerboard_background(ImTextureID user_texture_id, const ImVec2 &size, ImU32 tint_col)
{
	const auto draw_list = ImGui::GetWindowDrawList();

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
