/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>

// See 'reshade::runtime::build_font_atlas'
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

namespace reshade::gui::widgets
{
	/// <summary>
	/// Adds a widget to manage a list of directory paths.
	/// </summary>
	bool path_list(const char *label, std::vector<std::filesystem::path> &paths, std::filesystem::path &dialog_path, const std::filesystem::path &default_path = std::filesystem::path());

	/// <summary>
	/// Adds a file or directory selection popup window.
	/// </summary>
	/// <param name="name">The name of the popup window.</param>
	/// <param name="path">A reference that should initially be set to path to start the selection at and is then set to the chosen file or directory path by this widget.</param>
	/// <param name="width">The with of the popup window (in pixels).</param>
	/// <param name="exts">A list of file extensions that are valid for selection, or an empty list to make this a directory selection.</param>
	bool file_dialog(const char *name, std::filesystem::path &path, float width, const std::vector<std::wstring> &exts);

	/// <summary>
	/// Adds a keyboard shortcut widget.
	/// </summary>
	/// <param name="label">The label text describing this widget.</param>
	/// <param name="key">The shortcut, consisting of the [virtual key code, Ctrl, Shift, Alt].</param>
	/// <param name="input">A reference to the input state.</param>
	bool key_input_box(const char *label, unsigned int key[4], const reshade::input &input);

	/// <summary>
	/// Adds a TTF font file selection widget.
	/// </summary>
	bool font_input_box(const char *label, std::filesystem::path &path, std::filesystem::path &dialog_path, int &size);

	/// <summary>
	/// Adds a file selection widget which has both a text input box for the path and a button to open a file selection dialog.
	/// </summary>
	bool file_input_box(const char *label, std::filesystem::path &path, std::filesystem::path &dialog_path, const std::vector<std::wstring> &exts);
	/// <summary>
	/// Adds a direction selection widget which has both a text input box for the path and a button to open a direction selection dialog.
	/// </summary>
	bool directory_input_box(const char *label, std::filesystem::path &path, std::filesystem::path &dialog_path);

	/// <summary>
	/// Adds a widget which shows a vertical list of radio buttons plus a label to the right.
	/// </summary>
	/// <param name="label">The label text describing this widget.</param>
	/// <param name="ui_items">A list of labels for the items, separated with '\0' characters.</param>
	/// <param name="v">The index of the active item in the <paramref name="ui_items"/> list.</param>
	bool radio_list(const char *label, const std::string_view ui_items, int &v);

	/// <summary>
	/// Convenience function which adds a button that when pressed opens a popup window.
	/// Begins the popup window and returns <c>true</c> if it is open. Don't forget to call 'ImGui::EndPopup'.
	/// </summary>
	bool popup_button(const char *label, float width = 0.0f, ImGuiWindowFlags flags = 0);

	/// <summary>
	/// Adds a button with a visible toggle state. Clicking it toggles that state.
	/// </summary>
	bool toggle_button(const char *label, bool &v);

	/// <summary>
	/// Adds an ImGui drag widget but with additional "&lt;" and "&gt;" buttons to decrease/increase the value.
	/// </summary>
	bool drag_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format = nullptr);
	/// <summary>
	/// Adds an ImGui list widget but with additional "&lt;" and "&gt;" buttons to decrease/increase the value.
	/// </summary>
	bool list_with_buttons(const char *label, const std::string_view ui_items, int &v);
	/// <summary>
	/// Adds a combo widget for a boolean value, with the items "On" and "Off".
	/// </summary>
	bool combo_with_buttons(const char *label, bool &v);
	/// <summary>
	/// Adds an ImGui combo widget but with additional "&lt;" and "&gt;" buttons to decrease/increase the value.
	/// </summary>
	bool combo_with_buttons(const char *label, const std::string_view ui_items, int &v);
	/// <summary>
	/// Adds an ImGui slider widget but with additional "&lt;" and "&gt;" buttons to decrease/increase the value.
	/// </summary>
	bool slider_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format = nullptr);

	/// <summary>
	/// Adds an ImGui slider widget but with an additional color preview and picker for alpha values.
	/// </summary>
	bool slider_for_alpha_value(const char *label, float *v);

	/// <summary>
	/// Adds an image widget which has a checkerboard background for transparent images.
	/// </summary>
	/// <param name="user_texture_id">The texture to be rendered as the image.</param>
	/// <param name="size">The size of the widget.</param>
	/// <param name="tint_col">Optional tint color mulitplied with each pixel of the image during rendering.</param>
	void image_with_checkerboard_background(ImTextureID user_texture_id, const ImVec2 &size, ImU32 tint_col = 0xFFFFFFFF);
}
