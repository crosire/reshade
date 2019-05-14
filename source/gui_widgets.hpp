/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <imgui.h>
#include <filesystem>

bool imgui_key_input(const char *name, unsigned int key_data[4], const reshade::input &input);

bool imgui_font_select(const char *name, std::filesystem::path &path, int &size);

bool imgui_directory_dialog(const char *name, std::filesystem::path &path);

bool imgui_directory_input_box(const char *name, std::filesystem::path &path, std::filesystem::path &dialog_path);

bool imgui_path_list(const char *label, std::vector<std::filesystem::path> &paths, std::filesystem::path &dialog_path, const std::filesystem::path &default_path = std::filesystem::path());

bool imgui_popup_button(const char *label, float width = 0.0f, ImGuiWindowFlags flags = 0);

bool imgui_list_with_buttons(const char *label, const std::string_view ui_items, int &v);

bool imgui_drag_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format = nullptr);

bool imgui_slider_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format = nullptr);

bool imgui_slider_for_alpha(const char *label, float *v);

void imgui_image_with_checkerboard_background(ImTextureID user_texture_id, const ImVec2 &size);
