/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "gui_widgets.hpp"
#include <imgui.h>

bool imgui_font_select(const char *name, std::filesystem::path &path, int &size)
{
	bool res = false;
	const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::PushID(name);

	char buffer[260] = "";
	if (path.empty())
		strcpy(buffer, "ProggyClean.ttf");
	else
		path.u8string().copy(buffer, sizeof(buffer) - 1);

	ImGui::PushItemWidth(ImGui::CalcItemWidth() - spacing - 80);
	if (ImGui::InputText("##font", buffer, sizeof(buffer)))
	{
		std::error_code ec;
		const std::filesystem::path new_path = buffer;

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

	ImGui::SameLine(0, spacing);
	ImGui::TextUnformatted(name);

	ImGui::PopID();

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
			if (entry.is_regular_file(ec))
				ok = true;

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
