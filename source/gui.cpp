/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "version.h"
#include "runtime.hpp"
#include "input.hpp"
#include <assert.h>
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>

namespace reshade
{
	static const char keyboard_keys[256][16] = {
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

	static inline std::vector<std::string> split(const std::string &str, char delim)
	{
		std::vector<std::string> result;

		for (size_t i = 0, len = str.size(), found; i < len; i = found + 1)
		{
			found = str.find_first_of(delim, i);

			if (found == std::string::npos)
				found = len;

			result.push_back(str.substr(i, found - i));
		}

		return result;
	}

	void runtime::draw_overlay()
	{
		const bool show_splash = (_last_present_time - _last_reload_time) < std::chrono::seconds(5);

		if (!_overlay_key_setting_active &&
			_input->is_key_pressed(_menu_key_data[0], _menu_key_data[1], _menu_key_data[2], _menu_key_data[3]))
		{
			_show_menu = !_show_menu;
		}

		_effects_expanded_state &= 2;

		// Update ImGui configuration
		ImGui::SetCurrentContext(_imgui_context);
		auto &imgui_io = _imgui_context->IO;
		imgui_io.DeltaTime = _last_frame_duration.count() * 1e-9f;
		imgui_io.MouseDrawCursor = _show_menu;

		imgui_io.KeyCtrl = _input->is_key_down(0x11); // VK_CONTROL
		imgui_io.KeyShift = _input->is_key_down(0x10); // VK_SHIFT
		imgui_io.KeyAlt = _input->is_key_down(0x12); // VK_MENU
		imgui_io.MousePos.x = static_cast<float>(_input->mouse_position_x());
		imgui_io.MousePos.y = static_cast<float>(_input->mouse_position_y());
		imgui_io.MouseWheel += _input->mouse_wheel_delta();

		for (unsigned int i = 0; i < 256; i++)
		{
			imgui_io.KeysDown[i] = _input->is_key_down(i);
		}
		for (unsigned int i = 0; i < 5; i++)
		{
			imgui_io.MouseDown[i] = _input->is_mouse_button_down(i);
		}

		for (wchar_t c : _input->text_input())
		{
			imgui_io.AddInputCharacter(c);
		}

		if (imgui_io.KeyCtrl && !_no_font_scaling)
		{
			// Change global font scale if user presses the control key and moves the mouse wheel
			imgui_io.FontGlobalScale = ImClamp(imgui_io.FontGlobalScale + imgui_io.MouseWheel * 0.10f, 0.2f, 2.50f);
		}

		ImGui::NewFrame();

		// Create ImGui widgets and windows
		if (show_splash)
		{
			ImGui::SetNextWindowPos(ImVec2(10, 10));
			ImGui::SetNextWindowSize(ImVec2(_width - 20.0f, ImGui::GetFrameHeightWithSpacing() * 3), ImGuiCond_Appearing);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(_imgui_col_background[0], _imgui_col_background[1], _imgui_col_background[2], 0.5f));
			ImGui::Begin("Splash Screen", nullptr,
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoInputs |
				ImGuiWindowFlags_NoFocusOnAppearing);

			ImGui::TextUnformatted("ReShade " VERSION_STRING_FILE " by crosire");

			if (_needs_update)
			{
				ImGui::TextColored(ImVec4(1, 1, 0, 1),
					"An update is available! Please visit https://reshade.me and install the new version (v%lu.%lu.%lu).",
					_latest_version[0], _latest_version[1], _latest_version[2]);
			}
			else
			{
				ImGui::TextUnformatted("Visit https://reshade.me for news, updates, shaders and discussion.");
			}

			ImGui::Spacing();

			if (_reload_remaining_effects != 0)
			{
				ImGui::Text(
					"Loading (%u effects remaining) ... "
					"This might take a while. The application could become unresponsive for some time.",
					static_cast<unsigned int>(_reload_remaining_effects));
			}
			else
			{
				ImGui::Text(
					"Press '%s%s%s%s' to open the configuration menu.",
					_menu_key_data[1] ? "Ctrl + " : "",
					_menu_key_data[2] ? "Shift + " : "",
					_menu_key_data[3] ? "Alt + " : "",
					keyboard_keys[_menu_key_data[0]]);
			}

			if (!_last_reload_successful)
			{
				ImGui::SetWindowSize(ImVec2(_width - 20.0f, ImGui::GetFrameHeightWithSpacing() * 4));

				ImGui::Spacing();
				ImGui::TextColored(ImVec4(1, 0, 0, 1),
					"There were errors compiling some shaders. "
					"Open the configuration menu and switch to the 'Log' tab for more details.");
			}

			ImGui::End();
			ImGui::PopStyleColor();
		}

		ImGui::SetNextWindowPos(ImVec2(_width - 200.0f, 5));
		ImGui::SetNextWindowSize(ImVec2(200.0f, 200.0f));
		ImGui::PushFont(_imgui_context->IO.Fonts->Fonts[1]);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(_imgui_col_text_fps[0], _imgui_col_text_fps[1], _imgui_col_text_fps[2], 1.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4());
		ImGui::Begin("FPS", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoFocusOnAppearing);

		char temp[512];

		if (_show_clock)
		{
			const int hour = _date[3] / 3600;
			const int minute = (_date[3] - hour * 3600) / 60;

			ImFormatString(temp, sizeof(temp), " %02u:%02u", hour, minute);
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}
		if (_show_framerate && _imgui_context->IO.Framerate < 10000)
		{
			ImFormatString(temp, sizeof(temp), "%.0f fps", _imgui_context->IO.Framerate);
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);

			ImFormatString(temp, sizeof(temp), "%*lld ms", 3, std::chrono::duration_cast<std::chrono::milliseconds>(_last_frame_duration).count());
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(temp).x);
			ImGui::TextUnformatted(temp);
		}

		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopFont();

		if (_show_menu && _reload_remaining_effects == 0)
		{
			if (_is_fast_loading)
			{
				// Need to do a full reload since some effects might be missing due to fast loading
				reload();

				assert(!_is_fast_loading);
			}
			else
			{
				ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSize(ImVec2(730.0f, _height - 40.0f), ImGuiCond_FirstUseEver);
				ImGui::Begin("ReShade " VERSION_STRING_FILE " by crosire###Main", &_show_menu,
					ImGuiWindowFlags_MenuBar |
					ImGuiWindowFlags_NoCollapse);

				// Double click the window title bar to reset position and size
				const ImRect titlebar_rect = ImGui::GetCurrentWindow()->TitleBarRect();

				if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsMouseHoveringRect(titlebar_rect.Min, titlebar_rect.Max, false))
				{
					ImGui::SetWindowPos(ImVec2(20, 20));
					ImGui::SetWindowSize(ImVec2(730.0f, _height - 40.0f));
				}

				draw_overlay_menu();

				ImGui::End();
			}
		}

		// Render ImGui widgets and windows
		ImGui::Render();

		_input->block_mouse_input(_input_processing_mode != 0 && _show_menu && (_imgui_context->IO.WantCaptureMouse || _input_processing_mode == 2));
		_input->block_keyboard_input(_input_processing_mode != 0 && _show_menu && (_imgui_context->IO.WantCaptureKeyboard || _input_processing_mode == 2));

		if (const auto draw_data = ImGui::GetDrawData(); draw_data != nullptr && draw_data->CmdListsCount != 0 && draw_data->TotalVtxCount != 0)
		{
			render_imgui_draw_data(draw_data);
		}
	}
	void runtime::draw_overlay_menu()
	{
		if (ImGui::BeginMenuBar())
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().ItemSpacing * 2);

			for (size_t i = 0; i < _menu_callables.size(); ++i)
			{
				const std::string &label = _menu_callables[i].first;

				if (ImGui::Selectable(label.c_str(), _menu_index == i, 0, ImVec2(ImGui::CalcTextSize(label.c_str()).x, 0)))
				{
					_menu_index = i;
					// Keep this 'true' for two frame to workaround 'ImGui::SetScrollHere()' not working properly the first frame
					_switched_menu = 2;
				}

				ImGui::SameLine();
			}

			ImGui::PopStyleVar();
			ImGui::EndMenuBar();
		}

		ImGui::PushID(_menu_callables[_menu_index].first.c_str());

		_menu_callables[_menu_index].second();

		ImGui::PopID();

		if (_switched_menu > 0)
			--_switched_menu;
	}
	void runtime::draw_overlay_menu_home()
	{
		if (!_effects_enabled)
		{
			ImGui::Text("Effects are disabled. Press '%s%s%s%s' to enable them again.",
				_effects_key_data[1] ? "Ctrl + " : "",
				_effects_key_data[2] ? "Shift + " : "",
				_effects_key_data[3] ? "Alt + " : "",
				keyboard_keys[_effects_key_data[0]]);
		}

		bool continue_tutorial = false;
		const char *tutorial_text =
			"Welcome! Since this is the first time you start ReShade, we'll go through a quick tutorial covering the most important features.\n\n"
			"Before we continue: If you have difficulties reading this text, press the 'Ctrl' key and adjust the text size with your mouse wheel. "
			"The window size is variable as well, just grab the bottom right corner and move it around.\n\n"
			"Click on the 'Continue' button to continue the tutorial.";

		if (_performance_mode && _tutorial_index <= 3)
		{
			_tutorial_index = 4;
		}

		if (_tutorial_index > 0)
		{
			if (_tutorial_index == 1)
			{
				tutorial_text =
					"This is the preset selection. All changes will be saved to the selected file.\n\n"
					"You can add a new one by clicking on the '+' button. Simply enter a new preset name or the full path to an existing file (*.ini) in the text box that opens.\n"
					"To delete the currently selected preset file, click on the '-' button.\n"
					"Make sure a valid file is selected here before starting to tweak any values later, or else your changes won't be saved!\n\n"
					"Add a new preset by clicking on the '+' button to continue the tutorial.";

				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.941176f, 0.392157f, 0.392190f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.941176f, 0.392157f, 0.392190f, 1.0f));
			}

			const auto get_preset_file = [](void *data, int i, const char **out) {
				*out = static_cast<runtime *>(data)->_preset_files[i].string().c_str();
				return true;
			};

			ImGui::PushItemWidth(-(30 + ImGui::GetStyle().ItemSpacing.x) * 2 - 1);

			if (ImGui::Combo("##presets", &_current_preset, get_preset_file, this, static_cast<int>(_preset_files.size())))
			{
				save_config();

				if (_performance_mode)
				{
					reload();
				}
				else
				{
					load_preset(_preset_files[_current_preset]);
				}
			}

			ImGui::PopItemWidth();

			ImGui::SameLine();

			if (ImGui::Button("+", ImVec2(30, 0)))
			{
				ImGui::OpenPopup("Add Preset");
			}

			if (ImGui::BeginPopup("Add Preset"))
			{
				char buf[260] = { };

				if (ImGui::InputText("Name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					auto path = filesystem::absolute(buf, s_reshade_dll_path.parent_path());
					path.replace_extension(".ini");

					if (filesystem::exists(path) || filesystem::exists(path.parent_path()))
					{
						_preset_files.push_back(path);

						_current_preset = static_cast<int>(_preset_files.size()) - 1;

						load_preset(path);
						save_config();

						ImGui::CloseCurrentPopup();

						if (_tutorial_index == 1)
						{
							continue_tutorial = true;
						}
					}
				}

				ImGui::EndPopup();
			}

			if (_current_preset >= 0)
			{
				ImGui::SameLine();

				if (ImGui::Button("-", ImVec2(30, 0)))
				{
					ImGui::OpenPopup("Remove Preset");
				}

				if (ImGui::BeginPopup("Remove Preset"))
				{
					ImGui::Text("Do you really want to remove this preset?");

					if (ImGui::Button("Yes", ImVec2(-1, 0)))
					{
						_preset_files.erase(_preset_files.begin() + _current_preset);

						if (_current_preset == static_cast<ptrdiff_t>(_preset_files.size()))
						{
							_current_preset -= 1;
						}
						if (_current_preset >= 0)
						{
							load_preset(_preset_files[_current_preset]);
						}

						save_config();

						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}
			}

			if (_tutorial_index == 1)
			{
				ImGui::PopStyleColor();
				ImGui::PopStyleColor();
			}
		}

		if (_tutorial_index > 1)
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::PushItemWidth(-260);

			if (ImGui::InputText("##filter", _effect_filter_buffer, sizeof(_effect_filter_buffer), ImGuiInputTextFlags_AutoSelectAll))
			{
				filter_techniques(_effect_filter_buffer);
			}
			else if (!ImGui::IsItemActive() && _effect_filter_buffer[0] == '\0')
			{
				strcpy(_effect_filter_buffer, "Search");
			}

			ImGui::PopItemWidth();

			ImGui::SameLine();

			if (ImGui::Button("Active to Top", ImVec2(130 - ImGui::GetStyle().ItemSpacing.x, 0)))
			{
				for (size_t i = 0; i < _techniques.size(); ++i)
				{
					if (!_techniques[i].enabled)
					{
						for (size_t k = i + 1; k < _techniques.size(); ++k)
						{
							if (_techniques[k].enabled)
							{
								std::swap(_techniques[i], _techniques[k]);
								break;
							}
						}
					}
				}
			}

			ImGui::SameLine();

			if (ImGui::Button(_effects_expanded_state & 2 ? "Collapse All" : "Expand All", ImVec2(130 - ImGui::GetStyle().ItemSpacing.x, 0)))
			{
				_effects_expanded_state = (~_effects_expanded_state & 2) | 1;
			}

			if (_tutorial_index == 2)
			{
				tutorial_text =
					"This is the list of techniques. It contains all techniques in the effect files (*.fx) that were found in the effect search paths as specified on the 'Settings' tab.\n\n"
					"Enter text in the box at the top to filter it and search for specific techniques.\n\n"
					"Click on a technique to enable or disable it or drag it to a new location in the list to change the order in which the effects are applied.";

				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.941176f, 0.392157f, 0.392190f, 1.0f));
			}

			ImGui::Spacing();

			const float bottom_height = _performance_mode ? ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y : _variable_editor_height;

			if (ImGui::BeginChild("##techniques", ImVec2(-1, -bottom_height), true))
			{
				draw_overlay_technique_editor();
			}

			ImGui::EndChild();

			if (_tutorial_index == 2)
			{
				ImGui::PopStyleColor();
			}
		}

		if (_tutorial_index > 2 && !_performance_mode)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			ImGui::ButtonEx("##splitter", ImVec2(-1, 5));
			ImGui::PopStyleVar();

			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			}
			if (ImGui::IsItemActive())
			{
				_variable_editor_height -= _imgui_context->IO.MouseDelta.y;
			}

			if (_tutorial_index == 3)
			{
				tutorial_text =
					"This is the list of variables. It contains all tweakable options the effects expose. All values here apply in real-time. Double click on a widget to manually edit the value.\n\n"
					"Enter text in the box at the top to filter it and search for specific variables.\n\n"
					"Once you have finished tweaking your preset, be sure to go to the 'Settings' tab and change the 'Usage Mode' to 'Performance Mode'. "
					"This will recompile all shaders into a more optimal representation that gives a significant performance boost, but will disable variable tweaking and this list.";

				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.941176f, 0.392157f, 0.392190f, 1.0f));
			}

			const float bottom_height = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y + (_tutorial_index == 3 ? 125 : 0);

			if (ImGui::BeginChild("##variables", ImVec2(-1, -bottom_height), true))
			{
				draw_overlay_variable_editor();
			}

			ImGui::EndChild();

			if (_tutorial_index == 3)
			{
				ImGui::PopStyleColor();
			}
		}

		if (_tutorial_index > 3)
		{
			ImGui::Spacing();

			if (ImGui::Button("Reload", ImVec2(-1, 0)))
			{
				reload();
			}

			ImGui::SameLine();
		}
		else
		{
			ImGui::BeginChildFrame(ImGui::GetID("Tutorial Frame"), ImVec2(-1, 125));
			ImGui::TextWrapped(tutorial_text);
			ImGui::EndChildFrame();

			if ((_tutorial_index != 1 || _current_preset != -1) &&
				ImGui::Button(_tutorial_index == 3 ? "Finish" : "Continue", ImVec2(-1, 0)) || continue_tutorial)
			{
				_tutorial_index++;

				save_config();
			}
		}
	}
	void runtime::draw_overlay_menu_settings()
	{
		char edit_buffer[2048];

		const auto update_key_data = [&](unsigned int key_data[4]) {
			const unsigned int last_key_pressed = _input->last_key_pressed();

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
					key_data[1] = _input->is_key_down(0x11);
					key_data[2] = _input->is_key_down(0x10);
					key_data[3] = _input->is_key_down(0x12);
				}

				save_config();
			}
		};

		const auto copy_key_shortcut_to_edit_buffer = [&edit_buffer](const unsigned int shortcut[4]) {
			size_t offset = 0;
			if (shortcut[1]) memcpy(edit_buffer, "Ctrl + ", 8), offset += 7;
			if (shortcut[2]) memcpy(edit_buffer + offset, "Shift + ", 9), offset += 8;
			if (shortcut[3]) memcpy(edit_buffer + offset, "Alt + ", 7), offset += 6;
			memcpy(edit_buffer + offset, keyboard_keys[shortcut[0]], sizeof(*keyboard_keys));
		};
		const auto copy_vector_to_edit_buffer = [&edit_buffer](const std::vector<std::string> &data) {
			size_t offset = 0;
			edit_buffer[0] = '\0';
			for (const auto &line : data)
			{
				memcpy(edit_buffer + offset, line.c_str(), line.size());
				offset += line.size();
				edit_buffer[offset++] = '\n';
				edit_buffer[offset] = '\0';
			}
		};
		const auto copy_search_paths_to_edit_buffer = [&edit_buffer](const std::vector<filesystem::path> &search_paths) {
			size_t offset = 0;
			edit_buffer[0] = '\0';
			for (const auto &search_path : search_paths)
			{
				memcpy(edit_buffer + offset, search_path.string().c_str(), search_path.string().length());
				offset += search_path.string().length();
				edit_buffer[offset++] = '\n';
				edit_buffer[offset] = '\0';
			}
		};

		if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
		{
			assert(_menu_key_data[0] < 256);

			copy_key_shortcut_to_edit_buffer(_menu_key_data);

			ImGui::InputText("Overlay Key", edit_buffer, sizeof(edit_buffer), ImGuiInputTextFlags_ReadOnly);

			_overlay_key_setting_active = false;

			if (ImGui::IsItemActive())
			{
				_overlay_key_setting_active = true;

				update_key_data(_menu_key_data);
			}
			else if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
			}

			assert(_reload_key_data[0] < 256);

			copy_key_shortcut_to_edit_buffer(_reload_key_data);

			ImGui::InputText("Effect Reload Key", edit_buffer, sizeof(edit_buffer), ImGuiInputTextFlags_ReadOnly);

			if (ImGui::IsItemActive())
			{
				update_key_data(_reload_key_data);
			}
			else if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
			}

			assert(_effects_key_data[0] < 256);

			copy_key_shortcut_to_edit_buffer(_effects_key_data);

			ImGui::InputText("Effect Toggle Key", edit_buffer, sizeof(edit_buffer), ImGuiInputTextFlags_ReadOnly);

			_toggle_key_setting_active = false;

			if (ImGui::IsItemActive())
			{
				_toggle_key_setting_active = true;

				update_key_data(_effects_key_data);
			}
			else if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
			}

			int usage_mode_index = _performance_mode ? 0 : 1;

			if (ImGui::Combo("Usage Mode", &usage_mode_index, "Performance Mode\0Configuration Mode\0"))
			{
				_performance_mode = usage_mode_index == 0;

				save_config();
				reload();
			}

			if (ImGui::Combo("Input Processing", &_input_processing_mode, "Pass on all input\0Block input when cursor is on overlay\0Block all input when overlay is visible\0"))
			{
				save_config();
			}

			copy_search_paths_to_edit_buffer(_effect_search_paths);

			if (ImGui::InputTextMultiline("Effect Search Paths", edit_buffer, sizeof(edit_buffer), ImVec2(0, 60)))
			{
				const auto effect_search_paths = split(edit_buffer, '\n');
				_effect_search_paths.assign(effect_search_paths.begin(), effect_search_paths.end());

				save_config();
			}

			copy_search_paths_to_edit_buffer(_texture_search_paths);

			if (ImGui::InputTextMultiline("Texture Search Paths", edit_buffer, sizeof(edit_buffer), ImVec2(0, 60)))
			{
				const auto texture_search_paths = split(edit_buffer, '\n');
				_texture_search_paths.assign(texture_search_paths.begin(), texture_search_paths.end());

				save_config();
			}

			copy_vector_to_edit_buffer(_preprocessor_definitions);

			if (ImGui::InputTextMultiline("Preprocessor Definitions", edit_buffer, sizeof(edit_buffer), ImVec2(0, 100)))
			{
				_preprocessor_definitions = split(edit_buffer, '\n');

				save_config();
			}

			if (ImGui::Button("Restart Tutorial", ImVec2(ImGui::CalcItemWidth(), 0)))
			{
				_tutorial_index = 0;
			}
		}

		if (ImGui::CollapsingHeader("Screenshots", ImGuiTreeNodeFlags_DefaultOpen))
		{
			assert(_screenshot_key_data[0] < 256);

			copy_key_shortcut_to_edit_buffer(_screenshot_key_data);

			ImGui::InputText("Screenshot Key", edit_buffer, sizeof(edit_buffer), ImGuiInputTextFlags_ReadOnly);

			_screenshot_key_setting_active = false;

			if (ImGui::IsItemActive())
			{
				_screenshot_key_setting_active = true;

				update_key_data(_screenshot_key_data);
			}
			else if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Click in the field and press any key to change the shortcut to that key.");
			}

			memcpy(edit_buffer, _screenshot_path.string().c_str(), _screenshot_path.string().length() + 1);

			if (ImGui::InputText("Screenshot Path", edit_buffer, sizeof(edit_buffer)))
			{
				_screenshot_path = edit_buffer;

				save_config();
			}

			if (ImGui::Combo("Screenshot Format", &_screenshot_format, "Bitmap (*.bmp)\0Portable Network Graphics (*.png)\0"))
			{
				save_config();
			}

			if (ImGui::Checkbox("Include Preset (*.ini)",&_screenshot_include_preset))
			{
				_screenshot_include_configuration = false;

				save_config();
			}

			if (_screenshot_include_preset)
			{
				ImGui::SameLine();

				if (ImGui::Checkbox("Include Configuration (*.ini)", &_screenshot_include_configuration))
				{
					save_config();
				}
			}
		}

		if (ImGui::CollapsingHeader("User Interface", ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool modified = false;
			modified |= ImGui::Checkbox("Show Clock", &_show_clock);
			ImGui::SameLine(0, 10);
			modified |= ImGui::Checkbox("Show FPS", &_show_framerate);

			if (ImGui::DragFloat("Alpha", &ImGui::GetStyle().Alpha, 0.05f, 0.20f, 1.0f, "%.2f"))
			{
				ImGui::GetStyle().Alpha = __max(ImGui::GetStyle().Alpha, 0.05f);
				modified = true;
			}
			modified |= ImGui::ColorEdit3("Background Color", _imgui_col_background);
			modified |= ImGui::ColorEdit3("Item Background Color", _imgui_col_item_background);
			modified |= ImGui::ColorEdit3("Active Item Color", _imgui_col_active);
			modified |= ImGui::ColorEdit3("Text Color", _imgui_col_text);
			modified |= ImGui::ColorEdit3("FPS Text Color", _imgui_col_text_fps);

			if (modified)
			{
				save_config();
				// Style is applied in "load_config()".
				load_config();
			}
		}
	}
	void runtime::draw_overlay_menu_statistics()
	{
		if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushItemWidth(-1);
			ImGui::PlotLines("##framerate",
				_imgui_context->FramerateSecPerFrame, 120,
				_imgui_context->FramerateSecPerFrameIdx,
				nullptr,
				_imgui_context->FramerateSecPerFrameAccum / 120 * 0.5f,
				_imgui_context->FramerateSecPerFrameAccum / 120 * 1.5f,
				ImVec2(0, 50));
			ImGui::PopItemWidth();

			uint64_t post_processing_time_cpu = 0;
			uint64_t post_processing_time_gpu = 0;

			for (const auto &technique : _techniques)
			{
				post_processing_time_cpu += technique.average_cpu_duration;
				post_processing_time_gpu += technique.average_gpu_duration;
			}

			ImGui::BeginGroup();
			ImGui::TextUnformatted("Application:");
			ImGui::TextUnformatted("Date:");
			ImGui::TextUnformatted("Device:");
			ImGui::TextUnformatted("FPS:");
			ImGui::TextUnformatted("Post-Processing:");
			ImGui::TextUnformatted("Draw Calls:");
			ImGui::Text("Frame %llu:", _framecount + 1);
			ImGui::TextUnformatted("Timer:");
			ImGui::TextUnformatted("Network:");
			ImGui::EndGroup();

			ImGui::SameLine(ImGui::GetWindowWidth() * 0.333f);

			ImGui::BeginGroup();
			ImGui::Text("%X", std::hash<std::string>()(s_target_executable_path.filename_without_extension().string()));
			ImGui::Text("%d-%d-%d %d", _date[0], _date[1], _date[2], _date[3]);
			ImGui::Text("%X %d", _vendor_id, _device_id);
			ImGui::Text("%.2f", _imgui_context->IO.Framerate);
			ImGui::Text("%f ms (CPU)", (post_processing_time_cpu * 1e-6f));
			ImGui::Text("%u (%u vertices)", _drawcalls, _vertices);
			ImGui::Text("%f ms", _last_frame_duration.count() * 1e-6f);
			ImGui::Text("%f ms", std::fmod(std::chrono::duration_cast<std::chrono::nanoseconds>(_last_present_time - _start_time).count() * 1e-6f, 16777216.0f));
			ImGui::Text("%u B", g_network_traffic);
			ImGui::EndGroup();

			ImGui::SameLine(ImGui::GetWindowWidth() * 0.666f);

			ImGui::BeginGroup();
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::NewLine();

			if (post_processing_time_gpu != 0)
			{
				ImGui::Text("%f ms (GPU)", (post_processing_time_gpu * 1e-6f));
			}

			ImGui::EndGroup();
		}

		if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::BeginGroup();

			for (const auto &texture : _textures)
			{
				if (texture.impl_reference != texture_reference::none)
				{
					continue;
				}

				ImGui::Text("%s", texture.name.c_str());
			}

			ImGui::EndGroup();
			ImGui::SameLine(ImGui::GetWindowWidth() * 0.333f);
			ImGui::BeginGroup();

			for (const auto& texture : _textures)
			{
				if (texture.impl_reference != texture_reference::none)
				{
					continue;
				}

				ImGui::Text("%ux%u +%u ", texture.width, texture.height, (texture.levels - 1));
			}

			ImGui::EndGroup();
		}

		if (ImGui::CollapsingHeader("Techniques", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::BeginGroup();

			for (const auto &technique : _techniques)
			{
				if (technique.enabled)
				{
					if (technique.passes.size() > 1)
					{
						ImGui::Text("%s (%u passes)", technique.name.c_str(), static_cast<unsigned int>(technique.passes.size()));
					}
					else
					{
						ImGui::Text("%s", technique.name.c_str());
					}
				}
				else
				{
					if (technique.passes.size() > 1)
					{
						ImGui::TextDisabled("%s (%u passes)", technique.name.c_str(), static_cast<unsigned int>(technique.passes.size()));
					}
					else
					{
						ImGui::TextDisabled("%s", technique.name.c_str());
					}
				}
			}

			ImGui::EndGroup();
			ImGui::SameLine(ImGui::GetWindowWidth() * 0.333f);
			ImGui::BeginGroup();

			for (const auto &technique : _techniques)
			{
				if (technique.enabled)
				{
					ImGui::Text("%f ms (CPU)", (technique.average_cpu_duration * 1e-6f));
				}
				else
				{
					ImGui::NewLine();
				}
			}

			ImGui::EndGroup();
			ImGui::SameLine(ImGui::GetWindowWidth() * 0.666f);
			ImGui::BeginGroup();

			for (const auto &technique : _techniques)
			{
				if (technique.enabled && technique.average_gpu_duration != 0)
				{
					ImGui::Text("%f ms (GPU)", (technique.average_gpu_duration * 1e-6f));
				}
				else
				{
					ImGui::NewLine();
				}
			}

			ImGui::EndGroup();
		}
	}
	void runtime::draw_overlay_menu_log()
	{
		if (ImGui::Button("Clear Log"))
		{
			reshade::log::lines.clear();
		}

		ImGui::SameLine();
		ImGui::Checkbox("Word Wrap", &_log_wordwrap);
		ImGui::SameLine();

		static ImGuiTextFilter filter; // TODO: Better make this a member of the runtime class, in case there are multiple instances.
		filter.Draw("Filter (inc, -exc)", -150);

		std::vector<std::string> lines;
		for (auto &line : reshade::log::lines)
			if (filter.PassFilter(line.c_str()))
				lines.push_back(line);

		ImGui::BeginChild("log");

		ImGuiListClipper clipper(static_cast<int>(lines.size()), ImGui::GetTextLineHeightWithSpacing());

		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
		{
			ImVec4 textcol(1, 1, 1, 1);

			if (lines[i].find("ERROR |") != std::string::npos)
				textcol = ImVec4(1, 0, 0, 1);
			else if (lines[i].find("WARN  |") != std::string::npos)
				textcol = ImVec4(1, 1, 0, 1);
			else if (lines[i].find("DEBUG |") != std::string::npos)
				textcol = ImColor(100, 100, 255);

			ImGui::PushStyleColor(ImGuiCol_Text, textcol);
			if (_log_wordwrap) ImGui::PushTextWrapPos();

			ImGui::TextUnformatted(lines[i].c_str());

			if (_log_wordwrap) ImGui::PopTextWrapPos();
			ImGui::PopStyleColor();
		}

		clipper.End();

		// Scroll to the bottom if the log tab was just opened
		if (_switched_menu)
			ImGui::SetScrollHere();

		ImGui::EndChild();
	}
	void runtime::draw_overlay_menu_about()
	{
		ImGui::PushTextWrapPos();
		ImGui::TextUnformatted(R"(Copyright (C) 2014 Patrick Mours. All rights reserved.

https://github.com/crosire/reshade

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)");

		if (ImGui::CollapsingHeader("MinHook"))
		{
			ImGui::TextUnformatted(R"(Copyright (C) 2009-2016 Tsuda Kageyu. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)");
		}
		if (ImGui::CollapsingHeader("Hacker Disassembler Engine 32/64 C"))
		{
			ImGui::TextUnformatted(R"(Copyright (C) 2008-2009 Vyacheslav Patkov. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)");
		}
		if (ImGui::CollapsingHeader("dear imgui"))
		{
			ImGui::TextUnformatted(R"(Copyright (C) 2014-2015 Omar Cornut and ImGui contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)");
		}
		if (ImGui::CollapsingHeader("gl3w"))
		{
			ImGui::TextUnformatted("Slavomir Kaslev");
		}
		if (ImGui::CollapsingHeader("stb_image, stb_image_write"))
		{
			ImGui::TextUnformatted("Sean Barrett and contributors");
		}
		if (ImGui::CollapsingHeader("DDS loading from SOIL"))
		{
			ImGui::TextUnformatted("Jonathan \"lonesock\" Dummer");
		}

		ImGui::PopTextWrapPos();
	}
	void runtime::draw_overlay_variable_editor()
	{
		ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.5f);

		bool open_tutorial_popup = false;
		bool current_tree_is_closed = true;
		bool current_category_is_closed = false;
		std::string current_filename;
		std::string current_category;

		for (int id = 0; id < static_cast<int>(_uniform_count); id++)
		{
			auto &variable = _uniforms[id];

			if (variable.hidden || variable.annotations.count("source"))
			{
				continue;
			}

			if (current_filename != variable.effect_filename)
			{
				if (!current_tree_is_closed)
					ImGui::TreePop();

				const bool is_focused = _focus_effect == variable.effect_filename;

				if (is_focused)
					ImGui::SetNextTreeNodeOpen(true);
				else if (_effects_expanded_state & 1)
					ImGui::SetNextTreeNodeOpen((_effects_expanded_state >> 1) != 0);

				current_filename = variable.effect_filename;
				current_tree_is_closed = !ImGui::TreeNodeEx(variable.effect_filename.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

				if (is_focused)
				{
					ImGui::SetScrollHere(0.0f);
					_focus_effect.clear();
				}
			}
			if (current_tree_is_closed)
			{
				continue;
			}

			bool modified = false;
			const auto ui_type = variable.annotations["ui_type"].as<std::string>();
			const auto ui_label = variable.annotations.count("ui_label") ? variable.annotations.at("ui_label").as<std::string>() : variable.name;
			const auto ui_tooltip = variable.annotations["ui_tooltip"].as<std::string>();
			const auto ui_category = variable.annotations["ui_category"].as<std::string>();

			if (current_category != ui_category)
			{
				if (!ui_category.empty())
				{
					std::string category;
					for (float x = 0, space_x = ImGui::CalcTextSize(" ").x, width = (ImGui::CalcItemWidth() - ImGui::CalcTextSize(ui_category.c_str()).x) / 2 - 42; x < width; x += space_x)
						category += ' ';
					category += ui_category;

					current_category_is_closed = !ImGui::TreeNodeEx(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen);
				}
				else
				{
					current_category_is_closed = false;
				}

				current_category = ui_category;
			}
			if (current_category_is_closed)
			{
				continue;
			}

			ImGui::PushID(id);

			switch (variable.displaytype)
			{
				case uniform_datatype::boolean:
				{
					bool data = false;
					get_uniform_value(variable, &data, 1);

					if (ui_type == "combo")
					{
						int index = data ? 1 : 0;

						modified = ImGui::Combo(ui_label.c_str(), &index, "Off\0On\0");

						data = index != 0;
					}
					else
					{
						modified = ImGui::Checkbox(ui_label.c_str(), &data);
					}

					if (modified)
					{
						set_uniform_value(variable, &data, 1);
					}
					break;
				}
				case uniform_datatype::signed_integer:
				case uniform_datatype::unsigned_integer:
				{
					int data[4] = { };
					get_uniform_value(variable, data, 4);

					if (ui_type == "drag")
					{
						const int ui_min = variable.annotations["ui_min"].as<int>();
						const int ui_max = variable.annotations["ui_max"].as<int>();
						const float ui_step = variable.annotations["ui_step"].as<float>();

						modified = ImGui::DragScalarN(ui_label.c_str(), ImGuiDataType_S32, data, variable.rows, ui_step, &ui_min, &ui_max);
					}
					else if (ui_type == "combo")
					{
						std::string items = variable.annotations["ui_items"].as<std::string>();

						// Make sure list is terminated with a zero in case user forgot so no invalid memory is read accidentally
						if (!items.empty() && items.back() != '\0')
							items.push_back('\0');

						modified = ImGui::Combo(ui_label.c_str(), data, items.c_str());
					}
					else
					{
						modified = ImGui::InputScalarN(ui_label.c_str(), ImGuiDataType_S32, data, variable.rows);
					}

					if (modified)
					{
						set_uniform_value(variable, data, 4);
					}
					break;
				}
				case uniform_datatype::floating_point:
				{
					float data[4] = { };
					get_uniform_value(variable, data, 4);

					if (ui_type == "drag")
					{
						const float ui_min = variable.annotations["ui_min"].as<float>();
						const float ui_max = variable.annotations["ui_max"].as<float>();
						const float ui_step = variable.annotations["ui_step"].as<float>();

						modified = ImGui::DragScalarN(ui_label.c_str(), ImGuiDataType_Float, data, variable.rows, ui_step, &ui_min, &ui_max, "%.3f");
					}
					else if (ui_type == "input" || (ui_type.empty() && variable.rows < 3))
					{
						modified = ImGui::InputScalarN(ui_label.c_str(), ImGuiDataType_Float, data, variable.rows);
					}
					else if (variable.rows == 3)
					{
						modified = ImGui::ColorEdit3(ui_label.c_str(), data);
					}
					else if (variable.rows == 4)
					{
						modified = ImGui::ColorEdit4(ui_label.c_str(), data);
					}

					if (modified)
					{
						set_uniform_value(variable, data, 4);
					}
					break;
				}
			}

			if (ImGui::IsItemHovered() && !ui_tooltip.empty())
			{
				ImGui::SetTooltip("%s", ui_tooltip.c_str());
			}

			ImGui::PopID();

			if (modified)
			{
				save_current_preset();

				open_tutorial_popup = _tutorial_index == 4;
			}
		}

		if (!current_tree_is_closed)
		{
			ImGui::TreePop();
		}

		ImGui::PopItemWidth();

		if (open_tutorial_popup)
		{
			ImGui::OpenPopup("Performance Mode Hint");
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

		if (_tutorial_index == 4 && ImGui::BeginPopup("Performance Mode Hint"))
		{
			ImGui::TextUnformatted(
				"Don't forget to switch to 'Performance Mode' once you are done editing (on the 'Settings' tab).\n"
				"This will drastically increase runtime performance and loading times on next launch\n"
				"(Only effect files that are active in the current preset are loaded then instead of the entire list).");

			if (ImGui::Button("Ok", ImVec2(-1, 0)))
			{
				ImGui::CloseCurrentPopup();

				_tutorial_index++;

				save_config();
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar();
	}
	void runtime::draw_overlay_technique_editor()
	{
		int hovered_technique_index = -1;
		bool current_tree_is_closed = true;
		std::string current_filename;
		char edit_buffer[2048];
		const float toggle_key_text_offset = ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize("Toggle Key").x - 201;

		_toggle_key_setting_active = false;

		for (int id = 0, i = 0; id < static_cast<int>(_technique_count); id++, i++)
		{
			auto &technique = _techniques[id];

			if (technique.hidden)
			{
				continue;
			}

			ImGui::PushID(id);

			const std::string label = technique.name + " [" + technique.effect_filename + "]";

			if (ImGui::Checkbox(label.c_str(), &technique.enabled))
			{
				save_current_preset();
			}

			if (ImGui::IsItemActive())
			{
				_selected_technique = id;
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
			{
				hovered_technique_index = id;
			}

			if (ImGui::IsItemClicked(0))
			{
				_focus_effect = technique.effect_filename;
			}

			assert(technique.toggle_key_data[0] < 256);

			size_t offset = 0;
			if (technique.toggle_key_data[1]) memcpy(edit_buffer, "Ctrl + ", 8), offset += 7;
			if (technique.toggle_key_data[2]) memcpy(edit_buffer + offset, "Shift + ", 9), offset += 8;
			if (technique.toggle_key_data[3]) memcpy(edit_buffer + offset, "Alt + ", 7), offset += 6;
			memcpy(edit_buffer + offset, keyboard_keys[technique.toggle_key_data[0]], sizeof(*keyboard_keys));

			ImGui::SameLine(toggle_key_text_offset);
			ImGui::TextUnformatted("Toggle Key");
			ImGui::SameLine();
			ImGui::InputTextEx("##ToggleKey", edit_buffer, sizeof(edit_buffer), ImVec2(200, 0), ImGuiInputTextFlags_ReadOnly);

			if (ImGui::IsItemActive())
			{
				_toggle_key_setting_active = true;

				const unsigned int last_key_pressed = _input->last_key_pressed();

				if (last_key_pressed != 0)
				{
					if (last_key_pressed == 0x08) // Backspace
					{
						technique.toggle_key_data[0] = 0;
						technique.toggle_key_data[1] = 0;
						technique.toggle_key_data[2] = 0;
						technique.toggle_key_data[3] = 0;
					}
					else if (last_key_pressed < 0x10 || last_key_pressed > 0x12)
					{
						technique.toggle_key_data[0] = last_key_pressed;
						technique.toggle_key_data[1] = _input->is_key_down(0x11);
						technique.toggle_key_data[2] = _input->is_key_down(0x10);
						technique.toggle_key_data[3] = _input->is_key_down(0x12);
					}

					save_current_preset();
				}
			}
			else if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Click in the field and press any key to change the toggle shortcut to that key.\nPress backspace to disable the shortcut.");
			}

			ImGui::PopID();
		}

		if (!current_tree_is_closed)
		{
			ImGui::TreePop();
		}

		if (ImGui::IsMouseDragging() && _selected_technique >= 0)
		{
			ImGui::SetTooltip(_techniques[_selected_technique].name.c_str());

			if (hovered_technique_index >= 0 && hovered_technique_index != _selected_technique)
			{
				std::swap(_techniques[hovered_technique_index], _techniques[_selected_technique]);
				_selected_technique = hovered_technique_index;

				save_current_preset();
			}
		}
		else
		{
			_selected_technique = -1;
		}
	}

	void runtime::filter_techniques(const std::string &filter)
	{
		if (filter.empty())
		{
			_effects_expanded_state = 1;

			for (auto &uniform : _uniforms)
			{
				if (uniform.annotations["hidden"].as<bool>())
					continue;

				uniform.hidden = false;
			}
			for (auto &technique : _techniques)
			{
				if (technique.annotations["hidden"].as<bool>())
					continue;

				technique.hidden = false;
			}
		}
		else
		{
			_effects_expanded_state = 3;

			for (auto &uniform : _uniforms)
			{
				if (uniform.annotations["hidden"].as<bool>())
					continue;

				uniform.hidden =
					std::search(uniform.name.begin(), uniform.name.end(), filter.begin(), filter.end(),
						[](auto c1, auto c2) {
					return tolower(c1) == tolower(c2);
				}) == uniform.name.end() && uniform.effect_filename.find(filter) == std::string::npos;
			}
			for (auto &technique : _techniques)
			{
				if (technique.annotations["hidden"].as<bool>())
					continue;

				technique.hidden =
					std::search(technique.name.begin(), technique.name.end(), filter.begin(), filter.end(),
						[](auto c1, auto c2) {
					return tolower(c1) == tolower(c2);
				}) == technique.name.end() && technique.effect_filename.find(filter) == std::string::npos;
			}
		}
	}
}
