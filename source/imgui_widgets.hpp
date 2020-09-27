/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>

#define ICON_OK u8"\uf00c"
#define ICON_EDIT u8"\uf040"
#define ICON_CANCEL u8"\uf00d"
#define ICON_FILE u8"\uf15b"
#define ICON_FILE_PIC u8"\uf1c5"
#define ICON_FOLDER u8"\uf07b"
#define ICON_FOLDER_OPEN u8"\uf07c"
#define ICON_LINK u8"\uf1c9"
#define ICON_RESET u8"\uf064"
#define ICON_REMOVE u8"\uf068"
#define ICON_REFRESH u8"\uf021"
#define ICON_SAVE u8"\uf0c7"
#define ICON_SEARCH u8"\uf002"

bool imgui_key_input(const char *name, unsigned int key_data[4], const reshade::input &input);

bool imgui_font_select(const char *name, std::filesystem::path &path, std::filesystem::path &dialog_path, int &size);

bool imgui_file_dialog(const char *name, std::filesystem::path &path, float width, const std::vector<std::wstring> &exts);
bool imgui_file_input_box(const char *name, std::filesystem::path &path, std::filesystem::path &dialog_path, const std::vector<std::wstring> &exts);
bool imgui_directory_input_box(const char *name, std::filesystem::path &path, std::filesystem::path &dialog_path);

bool imgui_path_list(const char *label, std::vector<std::filesystem::path> &paths, std::filesystem::path &dialog_path, const std::filesystem::path &default_path = std::filesystem::path());

bool imgui_radio_list(const char *label, const std::string_view ui_items, int &v);

bool imgui_popup_button(const char *label, float width = 0.0f, ImGuiWindowFlags flags = 0);

bool imgui_toggle_button(const char *label, bool &toggle);

bool imgui_list_with_buttons(const char *label, const std::string_view ui_items, int &v);

bool imgui_combo_with_buttons(const char *label, bool &v);
bool imgui_combo_with_buttons(const char *label, const std::string_view ui_items, int &v);

bool imgui_drag_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format = nullptr);

bool imgui_slider_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format = nullptr);

bool imgui_slider_for_alpha_value(const char *label, float *v);

void imgui_image_with_checkerboard_background(ImTextureID user_texture_id, const ImVec2 &size, ImU32 tint_col = 0xFFFFFFFF);
