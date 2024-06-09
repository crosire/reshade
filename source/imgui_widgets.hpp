/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>

namespace reshade
{
	class input;
}

namespace reshade::imgui
{
	/// <summary>
	/// Adds a widget to manage a list of directory paths.
	/// </summary>
	bool path_list(const char *label, std::vector<std::filesystem::path> &paths, std::filesystem::path &dialog_path, const std::filesystem::path &default_path = std::filesystem::path());

	/// <summary>
	/// Adds a file or directory selection popup window.
	/// </summary>
	/// <param name="name">Name of the popup window.</param>
	/// <param name="path">Reference that should initially be set to path to start the selection at and is then set to the chosen file or directory path by this widget.</param>
	/// <param name="width">Width of the popup window (in pixels).</param>
	/// <param name="extensions">List of file extensions that are valid for selection, or an empty list to make this a directory selection.</param>
	/// <param name="hidden_paths">Optional list of file paths that are hidden.</param>
	bool file_dialog(const char *name, std::filesystem::path &path, float width, const std::vector<std::wstring> &extensions, const std::vector<std::filesystem::path> &hidden_paths = {});

	/// <summary>
	/// Adds a keyboard shortcut widget.
	/// </summary>
	/// <param name="label">Label text describing this widget.</param>
	/// <param name="key">Shortcut, consisting of the [virtual key code, Ctrl, Shift, Alt].</param>
	/// <param name="input">Reference to the input state.</param>
	bool key_input_box(const char *label, unsigned int key[4], const reshade::input &input);

	/// <summary>
	/// Adds a TTF font file selection widget.
	/// </summary>
	bool font_input_box(const char *label, const char *hint, std::filesystem::path &path, std::filesystem::path &dialog_path, int &size);

	/// <summary>
	/// Adds a search text box widget.
	/// </summary>
	bool search_input_box(char *filter, int filter_size, float width = 0.0f);

	/// <summary>
	/// Adds a file selection widget which has both a text input box for the path and a button to open a file selection dialog.
	/// </summary>
	bool file_input_box(const char *label, const char *hint, std::filesystem::path &path, std::filesystem::path &dialog_path, const std::vector<std::wstring> &exts);
	/// <summary>
	/// Adds a direction selection widget which has both a text input box for the path and a button to open a direction selection dialog.
	/// </summary>
	bool directory_input_box(const char *label, std::filesystem::path &path, std::filesystem::path &dialog_path);

	/// <summary>
	/// Adds a widget which shows a vertical list of radio buttons plus a label to the right.
	/// </summary>
	/// <param name="label">Label text describing this widget.</param>
	/// <param name="ui_items">List of labels for the items, separated with '\0' characters.</param>
	/// <param name="v">Index of the active item in the <paramref name="ui_items"/> list.</param>
	bool radio_list(const char *label, const std::string_view ui_items, int *v);

	/// <summary>
	/// Adds a widget which shows a vertical list of check boxes plus a label to the right.
	/// </summary>
	/// <param name="label">Label text describing this widget.</param>
	/// <param name="ui_items">List of labels for the items, separated with '\0' characters.</param>
	/// <param name="v">Item values.</param>
	bool checkbox_list(const char *label, const std::string_view ui_items, unsigned int *v, int components);
	/// <summary>
	/// Adds a checkbox with three states (checkmark, filled out, empty) instead of just two.
	/// </summary>
	bool checkbox_tristate(const char *label, unsigned int *v);

	/// <summary>
	/// Convenience function which adds a button that when pressed opens a popup window.
	/// Begins the popup window and returns <see langword="true"/> if it is open. Don't forget to call 'ImGui::EndPopup'.
	/// </summary>
	bool popup_button(const char *label, float width = 0.0f, ImGuiWindowFlags flags = 0);

	/// <summary>
	/// Adds a button with a visible toggle state. Clicking it toggles that state.
	/// </summary>
	bool toggle_button(const char *label, bool &v, float width = 0.0f, ImGuiButtonFlags flags = 0);

	/// <summary>
	/// Adds a button that asks for confirmation when pressed. Only returns <see langword="true"/> once that is acknowledged.
	/// </summary>
	bool confirm_button(const char *label, float width, const char *message, ...);

	/// <summary>
	/// Adds an ImGui drag widget but with additional "&lt;" and "&gt;" buttons to decrease/increase the value.
	/// </summary>
	bool drag_with_buttons(const char *label, ImGuiDataType data_type, void *v, int components, const void *v_speed, const void *v_min, const void *v_max, const char *format = nullptr);
	/// <summary>
	/// Adds an ImGui list widget but with additional "&lt;" and "&gt;" buttons to decrease/increase the value.
	/// </summary>
	bool list_with_buttons(const char *label, const std::string_view ui_items, int *v);
	/// <summary>
	/// Adds a combo widget for a boolean value, with the items "On" and "Off".
	/// </summary>
	bool combo_with_buttons(const char *label, bool *v);
	/// <summary>
	/// Adds an ImGui combo widget but with additional "&lt;" and "&gt;" buttons to decrease/increase the value.
	/// </summary>
	bool combo_with_buttons(const char *label, const std::string_view ui_items, int *v);
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
	/// <param name="user_texture_id">Texture handle to be rendered as the image.</param>
	/// <param name="size">Size of the widget.</param>
	/// <param name="tint_col">Optional tint color mulitplied with each pixel of the image during rendering.</param>
	void image_with_checkerboard_background(ImTextureID user_texture_id, const ImVec2 &size, ImU32 tint_col = 0xFFFFFFFF);
}
