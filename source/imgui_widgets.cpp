/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "imgui_widgets.hpp"
#include "input.hpp" // input::key_name
#include "localization.hpp"
#include "fonts/forkawesome.h"
#include <cassert>
#include <cwctype> // std::towlower
#include <algorithm> // std::find, std::max, std::min, std::replace std::transform

extern std::filesystem::path g_reshade_base_path;

static bool is_activate_key_pressed()
{
	return ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGui::GetIO().ConfigNavSwapGamepadButtons ? ImGuiKey_GamepadFaceRight : ImGuiKey_GamepadFaceDown); // See 'ImGuiKey_NavGamepadActivate'
}

bool reshade::imgui::path_list(const char *label, std::vector<std::filesystem::path> &paths, std::filesystem::path &dialog_path, const std::filesystem::path &default_path)
{
	bool res = false;

	const auto button_size = ImGui::GetFrameHeight();
	const auto button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	const auto item_width = ImGui::CalcItemWidth();
	const auto item_height = ImGui::GetFrameHeightWithSpacing();

	ImGui::BeginGroup();
	ImGui::PushID(label);

	if (ImGui::BeginChild("##paths", ImVec2(item_width, (paths.size() + 1) * item_height), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse))
	{
		for (int i = 0; i < static_cast<int>(paths.size()); ++i)
		{
			ImGui::PushID(i);

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
		path = g_reshade_base_path / path;
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

	ImGui::BeginChild("##files", ImVec2(width, 200), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);

	if (parent_path.has_parent_path() && parent_path != parent_path.root_path())
	{
		if (ImGui::Selectable(ICON_FK_FOLDER " ..", false, ImGuiSelectableFlags_AllowDoubleClick) && is_activate_key_pressed())
		{
			path = parent_path.parent_path();
			if (path.has_stem())
				path += std::filesystem::path::preferred_separator;
		}
	}

	std::vector<std::filesystem::path> file_entries;
	for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(parent_path, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		if (entry.path().has_filename() && entry.path().filename().native().front() == L'.')
			continue; // Skip "hidden" files and directories

		if (entry.is_directory())
		{
			const bool selected = (entry == path);

			const std::string label = ICON_FK_FOLDER " " + entry.path().filename().u8string();
			if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
			{
				path = entry;

				// Navigate into directory when double clicking one
				if (is_activate_key_pressed())
					path += std::filesystem::path::preferred_separator;
			}

			if (selected && ImGui::IsWindowAppearing())
				ImGui::SetScrollHereY();
			continue;
		}

		// Convert entry extension to lowercase before parsing
		std::wstring entry_ext = entry.path().extension().wstring();
		std::transform(entry_ext.begin(), entry_ext.end(), entry_ext.begin(), std::towlower);
		
		if (std::find(exts.cbegin(), exts.cend(), entry_ext) != exts.cend() &&
			std::find(hidden_paths.cbegin(), hidden_paths.cend(), entry.path()) == hidden_paths.cend())
			file_entries.push_back(entry);
	}

	// Always show file entries after all directory entries
	bool has_double_clicked_file = false;
	for (std::filesystem::path &file_path : file_entries)
	{
		const bool selected = (file_path == path);
		// Convert entry extension to lowercase before parsing
		std::wstring file_path_ext = file_path.extension().wstring();
		std::transform(file_path_ext.begin(), file_path_ext.end(), file_path_ext.begin(), std::towlower);

		std::string label = ICON_FK_FILE " ";
		if (file_path_ext == L".fx" || file_path_ext == L".fxh")
			label = ICON_FK_FILE_CODE " " + label;
		else if (file_path_ext == L".bmp" || file_path_ext == L".png" || file_path_ext == L".jpg" || file_path_ext == L".jpeg" || file_path_ext == L".dds")
			label = ICON_FK_FILE_IMAGE " " + label;
		label += file_path.filename().u8string();

		if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
		{
			path = std::move(file_path);

			// Double clicking a file on the other hand acts as if pressing the ok button
			if (is_activate_key_pressed())
				has_double_clicked_file = true;
		}

		if (selected && ImGui::IsWindowAppearing())
			ImGui::SetScrollHereY();
	}

	ImGui::EndChild();

	std::filesystem::path path_name = path.has_filename() || !exts.empty() ? path.filename() : path.parent_path().filename();

	const auto button_size = 8.0f * ImGui::GetFontSize();
	const auto button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	{	char buf[4096];
		const size_t buf_len = path_name.u8string().copy(buf, sizeof(buf) - 1);
		buf[buf_len] = '\0';

		ImGui::SetNextItemWidth(std::max(0.0f, width - (2 * (button_spacing + button_size))));
		if (ImGui::InputText("##name", buf, sizeof(buf)))
			path = path.parent_path() / buf;
	}

	ImGui::SameLine(0, button_spacing);
	const bool select = ImGui::Button(ICON_FK_OK " " + _("Select"), ImVec2(button_size, 0));
	ImGui::SameLine(0, button_spacing);
	const bool cancel = ImGui::Button(ICON_FK_CANCEL " " + _("Cancel"), ImVec2(button_size, 0));

	// Navigate into directory when clicking select button
	if (select && path.has_stem() && std::filesystem::is_directory(path, ec))
		path += std::filesystem::path::preferred_separator;
	
	// Convert entry extension to lowercase before parsing
	std::wstring path_ext = path.extension().wstring();
	std::transform(path_ext.begin(), path_ext.end(), path_ext.begin(), std::towlower);

	const bool result = (select || ImGui::IsKeyPressed(ImGuiKey_Enter) || has_double_clicked_file) && (exts.empty() || std::find(exts.cbegin(), exts.cend(), path_ext) != exts.cend());
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

	ImGui::InputTextWithHint(name, _("Click to set keyboard shortcut"), buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_NoHorizontalScroll);

	if (ImGui::IsItemActive())
	{
		const unsigned int last_key_pressed = input.last_key_pressed();
		if (last_key_pressed != 0)
		{
			if (last_key_pressed == 0x08) // Backspace
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
	else if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
	{
		ImGui::SetTooltip(_("Click in the field and press any key to change the shortcut to that key or press backspace to remove the shortcut."));
	}

	ImGui::EndDisabled();

	return res;
}

bool reshade::imgui::font_input_box(const char *name, const char *hint, std::filesystem::path &path, std::filesystem::path &dialog_path, int &size)
{
	bool res = false;

	const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(name);

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - spacing - 80);
	if (file_input_box("##font", hint, path, dialog_path, { L".ttf", L".ttc" }))
		res = true;

	ImGui::SameLine(0, spacing);
	ImGui::SetNextItemWidth(80);
	ImGui::SliderInt("##size", &size, 8, 32, "%d", ImGuiSliderFlags_AlwaysClamp);
	if (ImGui::IsItemDeactivatedAfterEdit())
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

	if (0.0f == width)
		width = ImGui::GetContentRegionAvail().x;

	ImGui::SetNextItemWidth(width - (show_clear_button ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x : 0.0f));
	if (ImGui::InputTextWithHint("##filter", _("Search") + " " ICON_FK_SEARCH, filter, filter_size, ImGuiInputTextFlags_AutoSelectAll))
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

	const auto button_size = ImGui::GetFrameHeight();
	const auto button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::PushID(name);
	ImGui::BeginGroup();

	char buf[4096];
	const size_t buf_len = path.u8string().copy(buf, sizeof(buf) - 1);
	buf[buf_len] = '\0'; // Null-terminate string

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - (button_spacing + button_size));
	if (ImGui::InputTextWithHint("##path", hint, buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		dialog_path = std::filesystem::u8path(buf);
		// Convert path extension to lowercase before parsing
		std::wstring dialog_path_ext = dialog_path.extension().wstring();
		std::transform(dialog_path_ext.begin(), dialog_path_ext.end(), dialog_path_ext.begin(), std::towlower);
		// Succeed only if extension matches
		if (std::find(exts.cbegin(), exts.cend(), dialog_path_ext) != exts.cend() || dialog_path.empty())
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

	const auto button_size = ImGui::GetFrameHeight();
	const auto button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

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

bool reshade::imgui::radio_list(const char *label, const std::string_view ui_items, int *v)
{
	bool res = false;

	const auto item_width = ImGui::CalcItemWidth();

	// Group all radio buttons together into a list
	ImGui::BeginGroup();

	for (size_t offset = 0, next, i = 0; (next = ui_items.find('\0', offset)) != std::string_view::npos; offset = next + 1, ++i)
		res |= ImGui::RadioButton(ui_items.data() + offset, v, static_cast<int>(i));

	ImGui::SameLine(item_width, ImGui::GetStyle().ItemInnerSpacing.x);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return res;
}

bool reshade::imgui::checkbox_list(const char *label, const std::string_view ui_items, unsigned int *v, int components)
{
	if (ui_items.empty())
		return ImGui::Checkbox(label, reinterpret_cast<bool *>(v));

	bool res = false;

	const auto item_width = ImGui::CalcItemWidth();

	ImGui::BeginGroup();

	for (size_t offset = 0, next, i = 0; (next = ui_items.find('\0', offset)) != std::string_view::npos && i < static_cast<size_t>(components); offset = next + 1, ++i)
		res |= ImGui::Checkbox(ui_items.data() + offset, reinterpret_cast<bool *>(&v[i]));

	ImGui::SameLine(item_width, ImGui::GetStyle().ItemInnerSpacing.x);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return res;
}
bool reshade::imgui::checkbox_tristate(const char *label, unsigned int *v)
{
	const bool mixed = *v > 1;
	bool value = *v != 0;

	ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixed);
	const bool res = ImGui::Checkbox(label, &value);
	if (res)
		*v = value ? 1 : mixed ? 0 : 2;
	ImGui::PopItemFlag();

	return res;
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
	const bool res = ImGui::ButtonEx(label, ImVec2(width, 0), flags);
	if (v)
		ImGui::PopStyleColor();

	if (res)
		v = !v;

	return res;
}

bool reshade::imgui::confirm_button(const char *label, float width, const char *message, ...)
{
	bool res = false;

	if (popup_button(label, width))
	{
		va_list args;
		va_start(args, message);
		ImGui::TextV(message, args);
		va_end(args);

		const float button_width = (ImGui::GetContentRegionAvail().x / 2) - ImGui::GetStyle().ItemInnerSpacing.x;

		if (ImGui::Button(ICON_FK_OK " " + _("Yes"), ImVec2(button_width, 0)))
		{
			ImGui::CloseCurrentPopup();
			res = true;
		}

		ImGui::SameLine();

		if (ImGui::Button(ICON_FK_CANCEL " " + _("No"), ImVec2(button_width, 0)))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	return res;
}

bool reshade::imgui::list_with_buttons(const char *label, const std::string_view ui_items, int *v)
{
	bool res = false;

	const auto button_size = ImGui::GetFrameHeight();
	const auto button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	const auto item_width = ImGui::CalcItemWidth() - (2 * (button_spacing + button_size));

	std::vector<std::string_view> items;
	for (size_t offset = 0, next; (next = ui_items.find('\0', offset)) != std::string_view::npos; offset = next + 1)
		items.push_back(ui_items.substr(offset, next + 1 - offset));

	ImGui::BeginGroup();

	ImGui::BeginGroup();
	ImGui::PushID(label);

	ImVec2 hover_pos = ImGui::GetCursorScreenPos();
	hover_pos.y += button_size;

	ImGui::SetNextItemWidth(item_width);

	if (ImGui::BeginCombo("", *v >= 0 && static_cast<size_t>(*v) < items.size() ? items[*v].data() : nullptr, ImGuiComboFlags_NoArrowButton))
	{
		for (int i = 0; i < static_cast<int>(items.size()); ++i)
		{
			ImGui::PushID(i);

			bool selected = (*v == i);
			if (ImGui::Selectable(items[i].data(), &selected))
			{
				*v = i;
				res = true;
			}

			if (selected)
				ImGui::SetItemDefaultFocus();

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	ImGui::BeginDisabled(items.empty());

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button("<", ImVec2(button_size, 0)))
	{
		res = true;
		*v = (*v == 0) ? static_cast<int>(items.size() - 1) : *v - 1;
	}

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button(">", ImVec2(button_size, 0)))
	{
		res = true;
		*v = (*v == static_cast<int>(items.size() - 1)) ? 0 : *v + 1;
	}

	ImGui::EndDisabled();

	ImGui::PopID();
	ImGui::EndGroup();
	const bool is_hovered = ImGui::IsItemHovered();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	if (is_hovered)
	{
		const ImGuiStyle &style = ImGui::GetStyle();

		ImGui::SetNextWindowPos(hover_pos);
		ImGui::SetNextWindowSizeConstraints(ImVec2(item_width, 0.0f), ImVec2(FLT_MAX, (ImGui::GetFontSize() + style.ItemSpacing.y) * 8 - style.ItemSpacing.y + (style.WindowPadding.y * 2))); // 8 by ImGuiComboFlags_HeightRegular
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.FramePadding.x, style.WindowPadding.y));
		ImGui::Begin("##spinner_items", NULL, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);
		ImGui::PopStyleVar();

		for (int i = 0; i < static_cast<int>(items.size()); ++i)
		{
			const bool selected = (*v == i);
			ImGui::Selectable(items[i].data(), selected);
			if (selected)
				ImGui::SetScrollHereY();
		}

		ImGui::End();
	}

	return res;
}

bool reshade::imgui::combo_with_buttons(const char *label, bool *v)
{
	std::string items = _("Off\nOn\n");
	std::replace(items.begin(), items.end(), '\n', '\0');

	int current_item = *v ? 1 : 0;
	const bool res = combo_with_buttons(label, items, &current_item);
	*v = current_item != 0;
	return res;
}
bool reshade::imgui::combo_with_buttons(const char *label, const std::string_view ui_items, int *v)
{
	bool res = false;

	const auto button_size = ImGui::GetFrameHeight();
	const auto button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	const auto item_width = ImGui::CalcItemWidth() - (2 * (button_spacing + button_size));

	std::vector<std::string_view> items;
	for (size_t offset = 0, next; (next = ui_items.find('\0', offset)) != std::string_view::npos; offset = next + 1)
		items.push_back(ui_items.substr(offset, next + 1 - offset));

	ImGui::BeginGroup();
	ImGui::PushID(label);

	ImGui::SetNextItemWidth(item_width);

	if (ImGui::BeginCombo("", *v >= 0 && static_cast<size_t>(*v) < items.size() ? items[*v].data() : nullptr, ImGuiComboFlags_None))
	{
		for (int i = 0; i < static_cast<int>(items.size()); ++i)
		{
			ImGui::PushID(i);

			bool selected = (*v == i);
			if (ImGui::Selectable(items[i].data(), &selected))
			{
				*v = i;
				res = true;
			}

			if (selected)
				ImGui::SetItemDefaultFocus();

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	ImGui::BeginDisabled(items.empty());

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button("<", ImVec2(button_size, 0)))
	{
		res = true;
		*v = (*v == 0) ? static_cast<int>(items.size() - 1) : *v - 1;
	}

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button(">", ImVec2(button_size, 0)))
	{
		res = true;
		*v = (*v == static_cast<int>(items.size() - 1)) ? 0 : *v + 1;
	}

	ImGui::EndDisabled();

	ImGui::PopID();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return res;
}

template <typename T, ImGuiDataType data_type>
static bool drag_with_buttons(const char *label, T *v, int components, T v_speed, T v_min, T v_max, const char *format)
{
	bool res = false;

	const auto button_size = ImGui::GetFrameHeight();
	const auto button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	const auto item_width = ImGui::CalcItemWidth();
	const auto item_width_with_buttons = item_width - (components * 2 * (button_spacing + button_size));

	const bool with_buttons = item_width_with_buttons > 50 * components;
	const bool ignore_limits = ImGui::GetIO().KeyCtrl;

	ImGui::BeginGroup();
	ImGui::PushID(label);

	ImGui::PushMultiItemsWidths(components, with_buttons ? item_width_with_buttons : item_width - (2 * (button_spacing + button_size)));

	for (int c = 0; c < components; ++c)
	{
		ImGui::PushID(c);
		if (c > 0)
			ImGui::SameLine(0, button_spacing);
		res |= ImGui::DragScalar("", data_type, v + c, static_cast<float>(v_speed), &v_min, &v_max, format);

		ImGui::PopItemWidth();

		if (with_buttons)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
			ImGui::SameLine(0, button_spacing);
			if (ImGui::ButtonEx("<", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick) && (ignore_limits || v[c] > v_min))
			{
				v[c] -= v_speed;
				if (!ignore_limits)
					v[c] = std::max(v[c], v_min);
				res = true;
			}
			ImGui::SameLine(0, button_spacing);
			if (ImGui::ButtonEx(">", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick) && (ignore_limits || v[c] < v_max))
			{
				v[c] += v_speed;
				if (!ignore_limits)
					v[c] = std::min(v[c], v_max);
				res = true;
			}
			ImGui::PopItemFlag();
		}

		ImGui::PopID();
	}

	if (!with_buttons)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
		ImGui::SameLine(0, button_spacing);
		if (ImGui::ButtonEx("<", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick) && (ignore_limits || v[0] > v_min))
		{
			for (int c = 0; c < components; ++c)
			{
				v[c] -= v_speed;
				if (!ignore_limits)
					v[c] = std::max(v[c], v_min);
			}
			res = true;
		}
		ImGui::SameLine(0, button_spacing);
		if (ImGui::ButtonEx(">", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick) && (ignore_limits || v[0] < v_max))
		{
			for (int c = 0; c < components; ++c)
			{
				v[c] += v_speed;
				if (!ignore_limits)
					v[c] = std::min(v[c], v_max);
			}
			res = true;
		}
		ImGui::PopItemFlag();
	}

	ImGui::PopID();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return res;

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
	bool res = false;

	const auto button_size = ImGui::GetFrameHeight();
	const auto button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	const auto item_width = ImGui::CalcItemWidth();
	const auto item_width_with_buttons = item_width - (components * 2 * (button_spacing + button_size));

	const bool with_buttons = item_width_with_buttons > 50 * components;
	const bool ignore_limits = ImGui::GetIO().KeyCtrl;

	ImGui::BeginGroup();
	ImGui::PushID(label);

	ImGui::PushMultiItemsWidths(components, with_buttons ? item_width_with_buttons : item_width - (2 * (button_spacing + button_size)));

	for (int c = 0; c < components; ++c)
	{
		ImGui::PushID(c);
		if (c > 0)
			ImGui::SameLine(0, button_spacing);
		res |= ImGui::SliderScalar("", data_type, v + c, &v_min, &v_max, format);

		ImGui::PopItemWidth();

		if (with_buttons)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
			ImGui::SameLine(0, button_spacing);
			if (ImGui::ButtonEx("<", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick) && (ignore_limits || v[c] > v_min))
			{
				v[c] -= v_speed;
				if (!ignore_limits)
					v[c] = std::max(v[c], v_min);
				res = true;
			}
			ImGui::SameLine(0, button_spacing);
			if (ImGui::ButtonEx(">", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick) && (ignore_limits || v[c] < v_max))
			{
				v[c] += v_speed;
				if (!ignore_limits)
					v[c] = std::min(v[c], v_max);
				res = true;
			}
			ImGui::PopItemFlag();
		}

		ImGui::PopID();
	}

	if (!with_buttons)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
		ImGui::SameLine(0, button_spacing);
		if (ImGui::ButtonEx("<", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick) && (ignore_limits || v[0] > v_min))
		{
			for (int c = 0; c < components; ++c)
			{
				v[c] -= v_speed;
				if (!ignore_limits)
					v[c] = std::max(v[c], v_min);
			}
			res = true;
		}
		ImGui::SameLine(0, button_spacing);
		if (ImGui::ButtonEx(">", ImVec2(button_size, 0), ImGuiButtonFlags_PressedOnClick) && (ignore_limits || v[0] < v_max))
		{
			for (int c = 0; c < components; ++c)
			{
				v[c] += v_speed;
				if (!ignore_limits)
					v[c] = std::min(v[c], v_max);
			}
			res = true;
		}
		ImGui::PopItemFlag();
	}

	ImGui::PopID();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return res;

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
	const auto button_size = ImGui::GetFrameHeight();
	const auto button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(label);

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - (button_spacing + button_size));
	const bool res = ImGui::SliderFloat("", v, 0.0f, 1.0f);

	ImGui::SameLine(0, button_spacing);
	ImGui::ColorButton("##preview", ImVec4(1.0f, 1.0f, 1.0f, *v), ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoPicker);

	ImGui::PopID();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return res;
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
	ImGui::ImageWithBg(user_texture_id, size, ImVec2(0, 0), ImVec2(1, 1), ImColor(), ImColor(tint_col));
}

void reshade::imgui::spinner(float value, float radius, float thickness)
{
	ImDrawList *const draw_list = ImGui::GetWindowDrawList();

	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const ImVec2 radius_with_padding = ImVec2(radius + thickness * 0.5f, radius + thickness * 0.5f);
	const ImVec2 center = pos + radius_with_padding;

	if (value < 0.0f)
		value = ImAbs(ImSin(static_cast<float>(ImGui::GetTime())));

	const ImU32 colors[6 + 1] = {
		0xFF00FF00,
		0xFFFFFF00,
		0xFFFF0000,
		0xFFFF00FF,
		0xFF0001FF,
		0xFF00FFFF,
		0xFF00FF00
	};

	const float epsilon = 0.5f / radius;
	const float rotation = 0.15f * IM_PI;

	draw_list->PathClear();

	for (int n = 0; n < 6; ++n)
	{
		if (n > value * 6)
			break;

		const float a_min = 2.0f * IM_PI * (n / 6.0f) - rotation - epsilon;
		const float a_max = 2.0f * IM_PI * ImClamp(value, n / 6.0f, (n + 1) / 6.0f) - rotation + epsilon;

		const int vert_beg_idx = draw_list->VtxBuffer.Size;
		draw_list->PathArcTo(center, radius, a_min, a_max);
		draw_list->PathStroke(colors[n], 0, thickness);
		const int vert_end_idx = draw_list->VtxBuffer.Size;

		const ImVec2 gradient_p0(center.x + ImCos(a_min) * radius, center.y + ImSin(a_min) * radius);
		const ImVec2 gradient_p1(center.x + ImCos(a_max) * radius, center.y + ImSin(a_max) * radius);
		ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw_list, vert_beg_idx, vert_end_idx, gradient_p0, gradient_p1, colors[n], colors[n == 2 || n == 5 ? n : n + 1]);
	}

	ImGui::Dummy(radius_with_padding * 2);
}
