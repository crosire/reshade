/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "input.hpp"
#include "gui_widgets.hpp"
#include <imgui.h>
#include <assert.h>

const char *g_keyboard_keys[256] = {
	"", "", "", "Cancel", "", "", "", "", "Backspace", "Tab", "", "", "Clear", "Enter", "", "",
	"Shift", "Control", "Alt", "Pause", "Caps Lock", "", "", "", "", "", "", "Escape", "", "", "", "",
	"Space", "Page Up", "Page Down", "End", "Home", "Left Arrow", "Up Arrow", "Right Arrow", "Down Arrow", "Select", "", "", "Print Screen", "Insert", "Delete", "Help",
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "", "", "", "", "", "",
	"", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
	"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Left Windows", "Right Windows", "", "", "Sleep",
	"Numpad 0", "Numpad 1", "Numpad 2", "Numpad 3", "Numpad 4", "Numpad 5", "Numpad 6", "Numpad 7", "Numpad 8", "Numpad 9", "Numpad *", "Numpad +", "", "Numpad -", "Numpad Decimal", "Numpad /",
	"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16",
	"F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "", "", "", "", "", "", "", "",
	"Num Lock", "Scroll Lock",
};

bool imgui_key_input(const char *name, unsigned int key_data[4], const reshade::input &input)
{
	bool res = false;

	assert(key_data[0] < 256);

	char buf[38];
	size_t offset = 0;
	if (key_data[1]) memcpy(buf, "Ctrl + ", 8), offset += 7;
	if (key_data[2]) memcpy(buf + offset, "Shift + ", 9), offset += 8;
	if (key_data[3]) memcpy(buf + offset, "Alt + ", 7), offset += 6;
	strcpy(buf + offset, g_keyboard_keys[key_data[0]]);

	ImGui::InputText(name, buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);

	if (ImGui::IsItemActive())
	{
		const unsigned int last_key_pressed = input.last_key_pressed();

		if (last_key_pressed != 0)
		{
			if (last_key_pressed == 0x08) // Backspace
			{
				key_data[0] = 0;
				key_data[1] = 0;
				key_data[2] = 0;
				key_data[3] = 0;
			}
			else if (last_key_pressed < 0x10 || last_key_pressed > 0x12)
			{
				key_data[0] = last_key_pressed;
				key_data[1] = input.is_key_down(0x11);
				key_data[2] = input.is_key_down(0x10);
				key_data[3] = input.is_key_down(0x12);
			}

			res = true;
		}
	}
	else if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
	}

	return res;
}

bool imgui_font_select(const char *name, std::filesystem::path &path, int &size)
{
	bool res = false;
	const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(name);

	char buf[260] = "";
	if (path.empty())
		strcpy(buf, "ProggyClean.ttf");
	else
		path.u8string().copy(buf, sizeof(buf) - 1);

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - spacing - 80);
	if (ImGui::InputText("##font", buf, sizeof(buf)))
	{
		std::error_code ec;
		const std::filesystem::path new_path = buf;

		if ((new_path.empty() || new_path == "ProggyClean.ttf") || (std::filesystem::is_regular_file(new_path, ec) && new_path.extension() == ".ttf"))
		{
			res = true;
			path = new_path;
		}
	}
	ImGui::PopItemWidth();

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

bool imgui_directory_dialog(const char *name, std::filesystem::path &path)
{
	bool ok = false, cancel = false;

	if (!ImGui::BeginPopup(name))
		return false;

	char buf[260] = "";
	path.u8string().copy(buf, sizeof(buf) - 1);

	ImGui::PushItemWidth(400);
	if (ImGui::InputText("##path", buf, sizeof(buf)))
		path = buf;
	ImGui::PopItemWidth();

	ImGui::SameLine();
	if (ImGui::Button("Ok", ImVec2(100, 0)))
		ok = true;
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(100, 0)))
		cancel = true;

	ImGui::BeginChild("##files", ImVec2(0, 300), true);

	std::error_code ec;

	if (path.is_relative())
		path = std::filesystem::absolute(path);
	if (!std::filesystem::is_directory(path, ec))
		path.remove_filename();
	else if (!path.has_filename())
		path = path.parent_path();

	if (ImGui::Selectable("."))
		ok = true;
	if (path.has_parent_path() && ImGui::Selectable(".."))
		path = path.parent_path();

	for (const auto &entry : std::filesystem::directory_iterator(path, ec))
	{
		// Only show directories
		if (!entry.is_directory(ec))
			continue;

		if (ImGui::Selectable(entry.path().filename().u8string().c_str(), entry.path() == path))
		{
			path = entry.path();
			break;
		}
	}

	ImGui::EndChild();

	if (ok || cancel)
		ImGui::CloseCurrentPopup();

	ImGui::EndPopup();

	return ok;
}

bool imgui_directory_input_box(const char *name, std::filesystem::path &path, std::filesystem::path &dialog_path)
{
	bool res = false;
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::PushID(name);
	ImGui::BeginGroup();

	char buf[260] = "";
	path.u8string().copy(buf, sizeof(buf) - 1);

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - (button_spacing + button_size));
	if (ImGui::InputText("##path", buf, sizeof(buf)))
		path = buf, res = true;
	ImGui::PopItemWidth();

	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button("..", ImVec2(button_size, 0)))
		dialog_path = path, ImGui::OpenPopup("##select");

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(name);
	ImGui::EndGroup();

	if (imgui_directory_dialog("##select", dialog_path))
		path = dialog_path, res = true;

	ImGui::PopID();

	return res;
}

bool imgui_path_list(const char *label, std::vector<std::filesystem::path> &paths, std::filesystem::path &dialog_path, const std::filesystem::path &default_path)
{
	bool res = false;
	const float item_width = ImGui::CalcItemWidth();
	const float item_height = ImGui::GetFrameHeightWithSpacing();
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(label);

	char buf[260];

	if (ImGui::BeginChild("##paths", ImVec2(item_width, (paths.size() + 1) * item_height), false, ImGuiWindowFlags_NoScrollWithMouse))
	{
		for (size_t i = 0; i < paths.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));

			memset(buf, 0, sizeof(buf));
			paths[i].u8string().copy(buf, sizeof(buf) - 1);

			ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - (button_spacing + button_size));
			if (ImGui::InputText("##path", buf, sizeof(buf)))
			{
				res = true;
				paths[i] = buf;
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
			dialog_path = default_path;
			ImGui::OpenPopup("##select");
		}

		if (imgui_directory_dialog("##select", dialog_path))
		{
			res = true;
			paths.push_back(dialog_path);
		}
	} ImGui::EndChild();

	ImGui::PopID();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return res;
}

bool imgui_popup_button(const char *label, float width, ImGuiWindowFlags flags)
{
	if (ImGui::Button(label, ImVec2(width, 0)))
		ImGui::OpenPopup(label); // Popups can have the same ID as other items without conflict
	return ImGui::BeginPopup(label, flags);
}

template <typename T, ImGuiDataType data_type>
bool imgui_slider_with_buttons(const char *label, T *v, int components, T v_speed, T v_min, T v_max, const char *format)
{
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(label);

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - (button_spacing * 2 + button_size * 2));
	bool value_changed = ImGui::SliderScalarN("##v", data_type, v, components, &v_min, &v_max, format);
	ImGui::PopItemWidth();

	ImGui::SameLine(0, 0);
	if (ImGui::Button("<", ImVec2(button_size, 0)) && v[0] > v_min)
	{
		for (int c = 0; c < components; ++c)
			v[c] -= v_speed;
		value_changed = true;
	}
	ImGui::SameLine(0, button_spacing);
	if (ImGui::Button(">", ImVec2(button_size, 0)) && v[0] < v_max)
	{
		for (int c = 0; c < components; ++c)
			v[c] += v_speed;
		value_changed = true;
	}

	ImGui::PopID();

	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);

	ImGui::EndGroup();

	return value_changed;

}
bool imgui_slider_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format)
{
	switch (data_type)
	{
	default:
		return false;
	case ImGuiDataType_S32:
		return imgui_slider_with_buttons<ImS32, ImGuiDataType_S32>(label, static_cast<ImS32 *>(v), components, *static_cast<const ImS32 *>(v_speed), *static_cast<const ImS32 *>(v_min), *static_cast<const ImS32 *>(v_max), format);
	case ImGuiDataType_U32:
		return imgui_slider_with_buttons<ImU32, ImGuiDataType_U32>(label, static_cast<ImU32 *>(v), components, *static_cast<const ImU32 *>(v_speed), *static_cast<const ImU32 *>(v_min), *static_cast<const ImU32 *>(v_max), format);
	case ImGuiDataType_S64:
		return imgui_slider_with_buttons<ImS64, ImGuiDataType_S64>(label, static_cast<ImS64 *>(v), components, *static_cast<const ImS64 *>(v_speed), *static_cast<const ImS64 *>(v_min), *static_cast<const ImS64 *>(v_max), format);
	case ImGuiDataType_U64:
		return imgui_slider_with_buttons<ImU64, ImGuiDataType_U64>(label, static_cast<ImU64 *>(v), components, *static_cast<const ImU64 *>(v_speed), *static_cast<const ImU64 *>(v_min), *static_cast<const ImU64 *>(v_max), format);
	case ImGuiDataType_Float:
		return imgui_slider_with_buttons<float, ImGuiDataType_Float>(label, static_cast<float *>(v), components, *static_cast<const float *>(v_speed), *static_cast<const float *>(v_min), *static_cast<const float *>(v_max), format);
	case ImGuiDataType_Double:
		return imgui_slider_with_buttons<double, ImGuiDataType_Double>(label, static_cast<double *>(v), components, *static_cast<const double *>(v_speed), *static_cast<const double *>(v_min), *static_cast<const double *>(v_max), format);
	}
}
