/*
 * Copyright (C) 2021 Patrick Mours
 * Copyright (C) 2014-2022 Omar Cornut
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#if defined(IMGUI_VERSION_NUM)

// Check that the 'ImTextureID' type has the same size as 'reshade::api::resource_view'
static_assert(sizeof(ImTextureID) == 8, "missing \"#define ImTextureID unsigned long long\" before \"#include <imgui.h>\"");

struct imgui_function_table
{
	ImGuiIO&(*GetIO)();
	ImGuiStyle&(*GetStyle)();
	const char*(*GetVersion)();
	bool(*Begin)(const char* name, bool* p_open, ImGuiWindowFlags flags);
	void(*End)();
	bool(*BeginChild)(const char* str_id, const ImVec2& size, bool border, ImGuiWindowFlags flags);
	bool(*BeginChild2)(ImGuiID id, const ImVec2& size, bool border, ImGuiWindowFlags flags);
	void(*EndChild)();
	bool(*IsWindowAppearing)();
	bool(*IsWindowCollapsed)();
	bool(*IsWindowFocused)(ImGuiFocusedFlags flags);
	bool(*IsWindowHovered)(ImGuiHoveredFlags flags);
	ImDrawList*(*GetWindowDrawList)();
	float(*GetWindowDpiScale)();
	ImVec2(*GetWindowPos)();
	ImVec2(*GetWindowSize)();
	float(*GetWindowWidth)();
	float(*GetWindowHeight)();
	void(*SetNextWindowPos)(const ImVec2& pos, ImGuiCond cond, const ImVec2& pivot);
	void(*SetNextWindowSize)(const ImVec2& size, ImGuiCond cond);
	void(*SetNextWindowSizeConstraints)(const ImVec2& size_min, const ImVec2& size_max, ImGuiSizeCallback custom_callback, void* custom_callback_data);
	void(*SetNextWindowContentSize)(const ImVec2& size);
	void(*SetNextWindowCollapsed)(bool collapsed, ImGuiCond cond);
	void(*SetNextWindowFocus)();
	void(*SetNextWindowBgAlpha)(float alpha);
	void(*SetWindowPos)(const ImVec2& pos, ImGuiCond cond);
	void(*SetWindowSize)(const ImVec2& size, ImGuiCond cond);
	void(*SetWindowCollapsed)(bool collapsed, ImGuiCond cond);
	void(*SetWindowFocus)();
	void(*SetWindowFontScale)(float scale);
	void(*SetWindowPos2)(const char* name, const ImVec2& pos, ImGuiCond cond);
	void(*SetWindowSize2)(const char* name, const ImVec2& size, ImGuiCond cond);
	void(*SetWindowCollapsed2)(const char* name, bool collapsed, ImGuiCond cond);
	void(*SetWindowFocus2)(const char* name);
	ImVec2(*GetContentRegionAvail)();
	ImVec2(*GetContentRegionMax)();
	ImVec2(*GetWindowContentRegionMin)();
	ImVec2(*GetWindowContentRegionMax)();
	float(*GetScrollX)();
	float(*GetScrollY)();
	void(*SetScrollX)(float scroll_x);
	void(*SetScrollY)(float scroll_y);
	float(*GetScrollMaxX)();
	float(*GetScrollMaxY)();
	void(*SetScrollHereX)(float center_x_ratio);
	void(*SetScrollHereY)(float center_y_ratio);
	void(*SetScrollFromPosX)(float local_x, float center_x_ratio);
	void(*SetScrollFromPosY)(float local_y, float center_y_ratio);
	void(*PushFont)(ImFont* font);
	void(*PopFont)();
	void(*PushStyleColor)(ImGuiCol idx, ImU32 col);
	void(*PushStyleColor2)(ImGuiCol idx, const ImVec4& col);
	void(*PopStyleColor)(int count);
	void(*PushStyleVar)(ImGuiStyleVar idx, float val);
	void(*PushStyleVar2)(ImGuiStyleVar idx, const ImVec2& val);
	void(*PopStyleVar)(int count);
	void(*PushAllowKeyboardFocus)(bool allow_keyboard_focus);
	void(*PopAllowKeyboardFocus)();
	void(*PushButtonRepeat)(bool repeat);
	void(*PopButtonRepeat)();
	void(*PushItemWidth)(float item_width);
	void(*PopItemWidth)();
	void(*SetNextItemWidth)(float item_width);
	float(*CalcItemWidth)();
	void(*PushTextWrapPos)(float wrap_local_pos_x);
	void(*PopTextWrapPos)();
	ImFont*(*GetFont)();
	float(*GetFontSize)();
	ImVec2(*GetFontTexUvWhitePixel)();
	ImU32(*GetColorU32)(ImGuiCol idx, float alpha_mul);
	ImU32(*GetColorU322)(const ImVec4& col);
	ImU32(*GetColorU323)(ImU32 col);
	const ImVec4&(*GetStyleColorVec4)(ImGuiCol idx);
	void(*Separator)();
	void(*SameLine)(float offset_from_start_x, float spacing);
	void(*NewLine)();
	void(*Spacing)();
	void(*Dummy)(const ImVec2& size);
	void(*Indent)(float indent_w);
	void(*Unindent)(float indent_w);
	void(*BeginGroup)();
	void(*EndGroup)();
	ImVec2(*GetCursorPos)();
	float(*GetCursorPosX)();
	float(*GetCursorPosY)();
	void(*SetCursorPos)(const ImVec2& local_pos);
	void(*SetCursorPosX)(float local_x);
	void(*SetCursorPosY)(float local_y);
	ImVec2(*GetCursorStartPos)();
	ImVec2(*GetCursorScreenPos)();
	void(*SetCursorScreenPos)(const ImVec2& pos);
	void(*AlignTextToFramePadding)();
	float(*GetTextLineHeight)();
	float(*GetTextLineHeightWithSpacing)();
	float(*GetFrameHeight)();
	float(*GetFrameHeightWithSpacing)();
	void(*PushID)(const char* str_id);
	void(*PushID2)(const char* str_id_begin, const char* str_id_end);
	void(*PushID3)(const void* ptr_id);
	void(*PushID4)(int int_id);
	void(*PopID)();
	ImGuiID(*GetID)(const char* str_id);
	ImGuiID(*GetID2)(const char* str_id_begin, const char* str_id_end);
	ImGuiID(*GetID3)(const void* ptr_id);
	void(*TextUnformatted)(const char* text, const char* text_end);
	void(*TextV)(const char* fmt, va_list args);
	void(*TextColoredV)(const ImVec4& col, const char* fmt, va_list args);
	void(*TextDisabledV)(const char* fmt, va_list args);
	void(*TextWrappedV)(const char* fmt, va_list args);
	void(*LabelTextV)(const char* label, const char* fmt, va_list args);
	void(*BulletTextV)(const char* fmt, va_list args);
	bool(*Button)(const char* label, const ImVec2& size);
	bool(*SmallButton)(const char* label);
	bool(*InvisibleButton)(const char* str_id, const ImVec2& size, ImGuiButtonFlags flags);
	bool(*ArrowButton)(const char* str_id, ImGuiDir dir);
	void(*Image)(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col);
	bool(*ImageButton)(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col);
	bool(*Checkbox)(const char* label, bool* v);
	bool(*CheckboxFlags)(const char* label, int* flags, int flags_value);
	bool(*CheckboxFlags2)(const char* label, unsigned int* flags, unsigned int flags_value);
	bool(*RadioButton)(const char* label, bool active);
	bool(*RadioButton2)(const char* label, int* v, int v_button);
	void(*ProgressBar)(float fraction, const ImVec2& size_arg, const char* overlay);
	void(*Bullet)();
	bool(*BeginCombo)(const char* label, const char* preview_value, ImGuiComboFlags flags);
	void(*EndCombo)();
	bool(*Combo)(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items);
	bool(*Combo2)(const char* label, int* current_item, const char* items_separated_by_zeros, int popup_max_height_in_items);
	bool(*Combo3)(const char* label, int* current_item, bool(*items_getter)(void* data, int idx, const char** out_text), void* data, int items_count, int popup_max_height_in_items);
	bool(*DragFloat)(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags);
	bool(*DragFloat2)(const char* label, float v[2], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags);
	bool(*DragFloat3)(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags);
	bool(*DragFloat4)(const char* label, float v[4], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags);
	bool(*DragFloatRange2)(const char* label, float* v_current_min, float* v_current_max, float v_speed, float v_min, float v_max, const char* format, const char* format_max, ImGuiSliderFlags flags);
	bool(*DragInt)(const char* label, int* v, float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags);
	bool(*DragInt2)(const char* label, int v[2], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags);
	bool(*DragInt3)(const char* label, int v[3], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags);
	bool(*DragInt4)(const char* label, int v[4], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags);
	bool(*DragIntRange2)(const char* label, int* v_current_min, int* v_current_max, float v_speed, int v_min, int v_max, const char* format, const char* format_max, ImGuiSliderFlags flags);
	bool(*DragScalar)(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags);
	bool(*DragScalarN)(const char* label, ImGuiDataType data_type, void* p_data, int components, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags);
	bool(*SliderFloat)(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags);
	bool(*SliderFloat2)(const char* label, float v[2], float v_min, float v_max, const char* format, ImGuiSliderFlags flags);
	bool(*SliderFloat3)(const char* label, float v[3], float v_min, float v_max, const char* format, ImGuiSliderFlags flags);
	bool(*SliderFloat4)(const char* label, float v[4], float v_min, float v_max, const char* format, ImGuiSliderFlags flags);
	bool(*SliderAngle)(const char* label, float* v_rad, float v_degrees_min, float v_degrees_max, const char* format, ImGuiSliderFlags flags);
	bool(*SliderInt)(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags);
	bool(*SliderInt2)(const char* label, int v[2], int v_min, int v_max, const char* format, ImGuiSliderFlags flags);
	bool(*SliderInt3)(const char* label, int v[3], int v_min, int v_max, const char* format, ImGuiSliderFlags flags);
	bool(*SliderInt4)(const char* label, int v[4], int v_min, int v_max, const char* format, ImGuiSliderFlags flags);
	bool(*SliderScalar)(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags);
	bool(*SliderScalarN)(const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags);
	bool(*VSliderFloat)(const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags);
	bool(*VSliderInt)(const char* label, const ImVec2& size, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags);
	bool(*VSliderScalar)(const char* label, const ImVec2& size, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags);
	bool(*InputText)(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);
	bool(*InputTextMultiline)(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);
	bool(*InputTextWithHint)(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);
	bool(*InputFloat)(const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags);
	bool(*InputFloat2)(const char* label, float v[2], const char* format, ImGuiInputTextFlags flags);
	bool(*InputFloat3)(const char* label, float v[3], const char* format, ImGuiInputTextFlags flags);
	bool(*InputFloat4)(const char* label, float v[4], const char* format, ImGuiInputTextFlags flags);
	bool(*InputInt)(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags);
	bool(*InputInt2)(const char* label, int v[2], ImGuiInputTextFlags flags);
	bool(*InputInt3)(const char* label, int v[3], ImGuiInputTextFlags flags);
	bool(*InputInt4)(const char* label, int v[4], ImGuiInputTextFlags flags);
	bool(*InputDouble)(const char* label, double* v, double step, double step_fast, const char* format, ImGuiInputTextFlags flags);
	bool(*InputScalar)(const char* label, ImGuiDataType data_type, void* p_data, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags);
	bool(*InputScalarN)(const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags);
	bool(*ColorEdit3)(const char* label, float col[3], ImGuiColorEditFlags flags);
	bool(*ColorEdit4)(const char* label, float col[4], ImGuiColorEditFlags flags);
	bool(*ColorPicker3)(const char* label, float col[3], ImGuiColorEditFlags flags);
	bool(*ColorPicker4)(const char* label, float col[4], ImGuiColorEditFlags flags, const float* ref_col);
	bool(*ColorButton)(const char* desc_id, const ImVec4& col, ImGuiColorEditFlags flags, ImVec2 size);
	void(*SetColorEditOptions)(ImGuiColorEditFlags flags);
	bool(*TreeNode)(const char* label);
	bool(*TreeNodeV)(const char* str_id, const char* fmt, va_list args);
	bool(*TreeNodeV2)(const void* ptr_id, const char* fmt, va_list args);
	bool(*TreeNodeEx)(const char* label, ImGuiTreeNodeFlags flags);
	bool(*TreeNodeExV)(const char* str_id, ImGuiTreeNodeFlags flags, const char* fmt, va_list args);
	bool(*TreeNodeExV2)(const void* ptr_id, ImGuiTreeNodeFlags flags, const char* fmt, va_list args);
	void(*TreePush)(const char* str_id);
	void(*TreePush2)(const void* ptr_id);
	void(*TreePop)();
	float(*GetTreeNodeToLabelSpacing)();
	bool(*CollapsingHeader)(const char* label, ImGuiTreeNodeFlags flags);
	bool(*CollapsingHeader2)(const char* label, bool* p_visible, ImGuiTreeNodeFlags flags);
	void(*SetNextItemOpen)(bool is_open, ImGuiCond cond);
	bool(*Selectable)(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size);
	bool(*Selectable2)(const char* label, bool* p_selected, ImGuiSelectableFlags flags, const ImVec2& size);
	bool(*BeginListBox)(const char* label, const ImVec2& size);
	void(*EndListBox)();
	bool(*ListBox)(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items);
	bool(*ListBox2)(const char* label, int* current_item, bool(*items_getter)(void* data, int idx, const char** out_text), void* data, int items_count, int height_in_items);
	void(*PlotLines)(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride);
	void(*PlotLines2)(const char* label, float(*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size);
	void(*PlotHistogram)(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride);
	void(*PlotHistogram2)(const char* label, float(*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size);
	void(*Value)(const char* prefix, bool b);
	void(*Value2)(const char* prefix, int v);
	void(*Value3)(const char* prefix, unsigned int v);
	void(*Value4)(const char* prefix, float v, const char* float_format);
	bool(*BeginMenuBar)();
	void(*EndMenuBar)();
	bool(*BeginMainMenuBar)();
	void(*EndMainMenuBar)();
	bool(*BeginMenu)(const char* label, bool enabled);
	void(*EndMenu)();
	bool(*MenuItem)(const char* label, const char* shortcut, bool selected, bool enabled);
	bool(*MenuItem2)(const char* label, const char* shortcut, bool* p_selected, bool enabled);
	void(*BeginTooltip)();
	void(*EndTooltip)();
	void(*SetTooltipV)(const char* fmt, va_list args);
	bool(*BeginPopup)(const char* str_id, ImGuiWindowFlags flags);
	bool(*BeginPopupModal)(const char* name, bool* p_open, ImGuiWindowFlags flags);
	void(*EndPopup)();
	void(*OpenPopup)(const char* str_id, ImGuiPopupFlags popup_flags);
	void(*OpenPopup2)(ImGuiID id, ImGuiPopupFlags popup_flags);
	void(*OpenPopupOnItemClick)(const char* str_id, ImGuiPopupFlags popup_flags);
	void(*CloseCurrentPopup)();
	bool(*BeginPopupContextItem)(const char* str_id, ImGuiPopupFlags popup_flags);
	bool(*BeginPopupContextWindow)(const char* str_id, ImGuiPopupFlags popup_flags);
	bool(*BeginPopupContextVoid)(const char* str_id, ImGuiPopupFlags popup_flags);
	bool(*IsPopupOpen)(const char* str_id, ImGuiPopupFlags flags);
	bool(*BeginTable)(const char* str_id, int column, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width);
	void(*EndTable)();
	void(*TableNextRow)(ImGuiTableRowFlags row_flags, float min_row_height);
	bool(*TableNextColumn)();
	bool(*TableSetColumnIndex)(int column_n);
	void(*TableSetupColumn)(const char* label, ImGuiTableColumnFlags flags, float init_width_or_weight, ImGuiID user_id);
	void(*TableSetupScrollFreeze)(int cols, int rows);
	void(*TableHeadersRow)();
	void(*TableHeader)(const char* label);
	ImGuiTableSortSpecs*(*TableGetSortSpecs)();
	int(*TableGetColumnCount)();
	int(*TableGetColumnIndex)();
	int(*TableGetRowIndex)();
	const char*(*TableGetColumnName)(int column_n);
	ImGuiTableColumnFlags(*TableGetColumnFlags)(int column_n);
	void(*TableSetColumnEnabled)(int column_n, bool v);
	void(*TableSetBgColor)(ImGuiTableBgTarget target, ImU32 color, int column_n);
	void(*Columns)(int count, const char* id, bool border);
	void(*NextColumn)();
	int(*GetColumnIndex)();
	float(*GetColumnWidth)(int column_index);
	void(*SetColumnWidth)(int column_index, float width);
	float(*GetColumnOffset)(int column_index);
	void(*SetColumnOffset)(int column_index, float offset_x);
	int(*GetColumnsCount)();
	bool(*BeginTabBar)(const char* str_id, ImGuiTabBarFlags flags);
	void(*EndTabBar)();
	bool(*BeginTabItem)(const char* label, bool* p_open, ImGuiTabItemFlags flags);
	void(*EndTabItem)();
	bool(*TabItemButton)(const char* label, ImGuiTabItemFlags flags);
	void(*SetTabItemClosed)(const char* tab_or_docked_window_label);
	ImGuiID(*DockSpace)(ImGuiID id, const ImVec2& size, int flags, const struct ImGuiWindowClass* window_class);
	void(*SetNextWindowDockID)(ImGuiID dock_id, ImGuiCond cond);
	void(*SetNextWindowClass)(const struct ImGuiWindowClass* window_class);
	ImGuiID(*GetWindowDockID)();
	bool(*IsWindowDocked)();
	bool(*BeginDragDropSource)(ImGuiDragDropFlags flags);
	bool(*SetDragDropPayload)(const char* type, const void* data, size_t sz, ImGuiCond cond);
	void(*EndDragDropSource)();
	bool(*BeginDragDropTarget)();
	const ImGuiPayload*(*AcceptDragDropPayload)(const char* type, ImGuiDragDropFlags flags);
	void(*EndDragDropTarget)();
	const ImGuiPayload*(*GetDragDropPayload)();
	void(*BeginDisabled)(bool disabled);
	void(*EndDisabled)();
	void(*PushClipRect)(const ImVec2& clip_rect_min, const ImVec2& clip_rect_max, bool intersect_with_current_clip_rect);
	void(*PopClipRect)();
	void(*SetItemDefaultFocus)();
	void(*SetKeyboardFocusHere)(int offset);
	bool(*IsItemHovered)(ImGuiHoveredFlags flags);
	bool(*IsItemActive)();
	bool(*IsItemFocused)();
	bool(*IsItemClicked)(ImGuiMouseButton mouse_button);
	bool(*IsItemVisible)();
	bool(*IsItemEdited)();
	bool(*IsItemActivated)();
	bool(*IsItemDeactivated)();
	bool(*IsItemDeactivatedAfterEdit)();
	bool(*IsItemToggledOpen)();
	bool(*IsAnyItemHovered)();
	bool(*IsAnyItemActive)();
	bool(*IsAnyItemFocused)();
	ImVec2(*GetItemRectMin)();
	ImVec2(*GetItemRectMax)();
	ImVec2(*GetItemRectSize)();
	void(*SetItemAllowOverlap)();
	bool(*IsRectVisible)(const ImVec2& size);
	bool(*IsRectVisible2)(const ImVec2& rect_min, const ImVec2& rect_max);
	double(*GetTime)();
	int(*GetFrameCount)();
	ImDrawList*(*GetBackgroundDrawList)();
	ImDrawList*(*GetForegroundDrawList)();
	ImDrawList*(*GetBackgroundDrawList2)(ImGuiViewport* viewport);
	ImDrawList*(*GetForegroundDrawList2)(ImGuiViewport* viewport);
	ImDrawListSharedData*(*GetDrawListSharedData)();
	const char*(*GetStyleColorName)(ImGuiCol idx);
	void(*SetStateStorage)(ImGuiStorage* storage);
	ImGuiStorage*(*GetStateStorage)();
	bool(*BeginChildFrame)(ImGuiID id, const ImVec2& size, ImGuiWindowFlags flags);
	void(*EndChildFrame)();
	ImVec2(*CalcTextSize)(const char* text, const char* text_end, bool hide_text_after_double_hash, float wrap_width);
	ImVec4(*ColorConvertU32ToFloat4)(ImU32 in);
	ImU32(*ColorConvertFloat4ToU32)(const ImVec4& in);
	void(*ColorConvertRGBtoHSV)(float r, float g, float b, float& out_h, float& out_s, float& out_v);
	void(*ColorConvertHSVtoRGB)(float h, float s, float v, float& out_r, float& out_g, float& out_b);
	int(*GetKeyIndex)(ImGuiKey imgui_key);
	bool(*IsKeyDown)(int user_key_index);
	bool(*IsKeyPressed)(int user_key_index, bool repeat);
	bool(*IsKeyReleased)(int user_key_index);
	int(*GetKeyPressedAmount)(int key_index, float repeat_delay, float rate);
	void(*CaptureKeyboardFromApp)(bool want_capture_keyboard_value);
	bool(*IsMouseDown)(ImGuiMouseButton button);
	bool(*IsMouseClicked)(ImGuiMouseButton button, bool repeat);
	bool(*IsMouseReleased)(ImGuiMouseButton button);
	bool(*IsMouseDoubleClicked)(ImGuiMouseButton button);
	int(*GetMouseClickedCount)(ImGuiMouseButton button);
	bool(*IsMouseHoveringRect)(const ImVec2& r_min, const ImVec2& r_max, bool clip);
	bool(*IsMousePosValid)(const ImVec2* mouse_pos);
	bool(*IsAnyMouseDown)();
	ImVec2(*GetMousePos)();
	ImVec2(*GetMousePosOnOpeningCurrentPopup)();
	bool(*IsMouseDragging)(ImGuiMouseButton button, float lock_threshold);
	ImVec2(*GetMouseDragDelta)(ImGuiMouseButton button, float lock_threshold);
	void(*ResetMouseDragDelta)(ImGuiMouseButton button);
	ImGuiMouseCursor(*GetMouseCursor)();
	void(*SetMouseCursor)(ImGuiMouseCursor cursor_type);
	void(*CaptureMouseFromApp)(bool want_capture_mouse_value);
	const char*(*GetClipboardText)();
	void(*SetClipboardText)(const char* text);
	bool(*DebugCheckVersionAndDataLayout)(const char* version_str, size_t sz_io, size_t sz_style, size_t sz_vec2, size_t sz_vec4, size_t sz_drawvert, size_t sz_drawidx);
	void(*SetAllocatorFunctions)(ImGuiMemAllocFunc alloc_func, ImGuiMemFreeFunc free_func, void* user_data);
	void(*GetAllocatorFunctions)(ImGuiMemAllocFunc* p_alloc_func, ImGuiMemFreeFunc* p_free_func, void** p_user_data);
	void*(*MemAlloc)(size_t size);
	void(*MemFree)(void* ptr);
	int(*ImGuiStorage_GetInt)(const ImGuiStorage *_this, ImGuiID key, int default_val);
	void(*ImGuiStorage_SetInt)(ImGuiStorage *_this, ImGuiID key, int val);
	bool(*ImGuiStorage_GetBool)(const ImGuiStorage *_this, ImGuiID key, bool default_val);
	void(*ImGuiStorage_SetBool)(ImGuiStorage *_this, ImGuiID key, bool val);
	float(*ImGuiStorage_GetFloat)(const ImGuiStorage *_this, ImGuiID key, float default_val);
	void(*ImGuiStorage_SetFloat)(ImGuiStorage *_this, ImGuiID key, float val);
	void*(*ImGuiStorage_GetVoidPtr)(const ImGuiStorage *_this, ImGuiID key);
	void(*ImGuiStorage_SetVoidPtr)(ImGuiStorage *_this, ImGuiID key, void* val);
	int*(*ImGuiStorage_GetIntRef)(ImGuiStorage *_this, ImGuiID key, int default_val);
	bool*(*ImGuiStorage_GetBoolRef)(ImGuiStorage *_this, ImGuiID key, bool default_val);
	float*(*ImGuiStorage_GetFloatRef)(ImGuiStorage *_this, ImGuiID key, float default_val);
	void**(*ImGuiStorage_GetVoidPtrRef)(ImGuiStorage *_this, ImGuiID key, void* default_val);
	void(*ImGuiStorage_SetAllInt)(ImGuiStorage *_this, int val);
	void(*ImGuiStorage_BuildSortByKey)(ImGuiStorage *_this);
	void(*ConstructImGuiListClipper)(ImGuiListClipper *_this);
	void(*DestructImGuiListClipper)(ImGuiListClipper *_this);
	void(*ImGuiListClipper_Begin)(ImGuiListClipper *_this, int items_count, float items_height);
	void(*ImGuiListClipper_End)(ImGuiListClipper *_this);
	bool(*ImGuiListClipper_Step)(ImGuiListClipper *_this);
	void(*ImGuiListClipper_ForceDisplayRangeByIndices)(ImGuiListClipper *_this, int item_min, int item_max);
	void(*ImDrawList_PushClipRect)(ImDrawList *_this, ImVec2 clip_rect_min, ImVec2 clip_rect_max, bool intersect_with_current_clip_rect);
	void(*ImDrawList_PushClipRectFullScreen)(ImDrawList *_this);
	void(*ImDrawList_PopClipRect)(ImDrawList *_this);
	void(*ImDrawList_PushTextureID)(ImDrawList *_this, ImTextureID texture_id);
	void(*ImDrawList_PopTextureID)(ImDrawList *_this);
	void(*ImDrawList_AddLine)(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness);
	void(*ImDrawList_AddRect)(ImDrawList *_this, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness);
	void(*ImDrawList_AddRectFilled)(ImDrawList *_this, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags);
	void(*ImDrawList_AddRectFilledMultiColor)(ImDrawList *_this, const ImVec2& p_min, const ImVec2& p_max, ImU32 col_upr_left, ImU32 col_upr_right, ImU32 col_bot_right, ImU32 col_bot_left);
	void(*ImDrawList_AddQuad)(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness);
	void(*ImDrawList_AddQuadFilled)(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col);
	void(*ImDrawList_AddTriangle)(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness);
	void(*ImDrawList_AddTriangleFilled)(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col);
	void(*ImDrawList_AddCircle)(ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness);
	void(*ImDrawList_AddCircleFilled)(ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments);
	void(*ImDrawList_AddNgon)(ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness);
	void(*ImDrawList_AddNgonFilled)(ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments);
	void(*ImDrawList_AddText)(ImDrawList *_this, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end);
	void(*ImDrawList_AddText2)(ImDrawList *_this, const ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect);
	void(*ImDrawList_AddPolyline)(ImDrawList *_this, const ImVec2* points, int num_points, ImU32 col, ImDrawFlags flags, float thickness);
	void(*ImDrawList_AddConvexPolyFilled)(ImDrawList *_this, const ImVec2* points, int num_points, ImU32 col);
	void(*ImDrawList_AddBezierCubic)(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness, int num_segments);
	void(*ImDrawList_AddBezierQuadratic)(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness, int num_segments);
	void(*ImDrawList_AddImage)(ImDrawList *_this, ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col);
	void(*ImDrawList_AddImageQuad)(ImDrawList *_this, ImTextureID user_texture_id, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3, const ImVec2& uv4, ImU32 col);
	void(*ImDrawList_AddImageRounded)(ImDrawList *_this, ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col, float rounding, ImDrawFlags flags);
	void(*ImDrawList_PathArcTo)(ImDrawList *_this, const ImVec2& center, float radius, float a_min, float a_max, int num_segments);
	void(*ImDrawList_PathArcToFast)(ImDrawList *_this, const ImVec2& center, float radius, int a_min_of_12, int a_max_of_12);
	void(*ImDrawList_PathBezierCubicCurveTo)(ImDrawList *_this, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, int num_segments);
	void(*ImDrawList_PathBezierQuadraticCurveTo)(ImDrawList *_this, const ImVec2& p2, const ImVec2& p3, int num_segments);
	void(*ImDrawList_PathRect)(ImDrawList *_this, const ImVec2& rect_min, const ImVec2& rect_max, float rounding, ImDrawFlags flags);
	void(*ImDrawList_AddCallback)(ImDrawList *_this, ImDrawCallback callback, void* callback_data);
	void(*ImDrawList_AddDrawCmd)(ImDrawList *_this);
	ImDrawList*(*ImDrawList_CloneOutput)(const ImDrawList *_this);
	void(*ImDrawList_PrimReserve)(ImDrawList *_this, int idx_count, int vtx_count);
	void(*ImDrawList_PrimUnreserve)(ImDrawList *_this, int idx_count, int vtx_count);
	void(*ImDrawList_PrimRect)(ImDrawList *_this, const ImVec2& a, const ImVec2& b, ImU32 col);
	void(*ImDrawList_PrimRectUV)(ImDrawList *_this, const ImVec2& a, const ImVec2& b, const ImVec2& uv_a, const ImVec2& uv_b, ImU32 col);
	void(*ImDrawList_PrimQuadUV)(ImDrawList *_this, const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d, const ImVec2& uv_a, const ImVec2& uv_b, const ImVec2& uv_c, const ImVec2& uv_d, ImU32 col);
	void(*ConstructImFont)(ImFont *_this);
	void(*DestructImFont)(ImFont *_this);
	const ImFontGlyph*(*ImFont_FindGlyph)(const ImFont *_this, ImWchar c);
	const ImFontGlyph*(*ImFont_FindGlyphNoFallback)(const ImFont *_this, ImWchar c);
	ImVec2(*ImFont_CalcTextSizeA)(const ImFont *_this, float size, float max_width, float wrap_width, const char* text_begin, const char* text_end, const char** remaining);
	const char*(*ImFont_CalcWordWrapPositionA)(const ImFont *_this, float scale, const char* text, const char* text_end, float wrap_width);
	void(*ImFont_RenderChar)(const ImFont *_this, ImDrawList* draw_list, float size, ImVec2 pos, ImU32 col, ImWchar c);
	void(*ImFont_RenderText)(const ImFont *_this, ImDrawList* draw_list, float size, ImVec2 pos, ImU32 col, const ImVec4& clip_rect, const char* text_begin, const char* text_end, float wrap_width, bool cpu_fine_clip);

};

inline const imgui_function_table *&imgui_function_table_instance()
{
	static const imgui_function_table *instance = nullptr;
	return instance;
}

#ifndef RESHADE_ADDON

namespace ImGui
{
	inline ImGuiIO& GetIO() { return imgui_function_table_instance()->GetIO(); }
	inline ImGuiStyle& GetStyle() { return imgui_function_table_instance()->GetStyle(); }
	inline const char* GetVersion() { return imgui_function_table_instance()->GetVersion(); }
	inline bool Begin(const char* name, bool* p_open, ImGuiWindowFlags flags) { return imgui_function_table_instance()->Begin(name, p_open, flags); }
	inline void End() { imgui_function_table_instance()->End(); }
	inline bool BeginChild(const char* str_id, const ImVec2& size, bool border, ImGuiWindowFlags flags) { return imgui_function_table_instance()->BeginChild(str_id, size, border, flags); }
	inline bool BeginChild(ImGuiID id, const ImVec2& size, bool border, ImGuiWindowFlags flags) { return imgui_function_table_instance()->BeginChild2(id, size, border, flags); }
	inline void EndChild() { imgui_function_table_instance()->EndChild(); }
	inline bool IsWindowAppearing() { return imgui_function_table_instance()->IsWindowAppearing(); }
	inline bool IsWindowCollapsed() { return imgui_function_table_instance()->IsWindowCollapsed(); }
	inline bool IsWindowFocused(ImGuiFocusedFlags flags) { return imgui_function_table_instance()->IsWindowFocused(flags); }
	inline bool IsWindowHovered(ImGuiHoveredFlags flags) { return imgui_function_table_instance()->IsWindowHovered(flags); }
	inline ImDrawList* GetWindowDrawList() { return imgui_function_table_instance()->GetWindowDrawList(); }
	inline float GetWindowDpiScale() { return imgui_function_table_instance()->GetWindowDpiScale(); }
	inline ImVec2 GetWindowPos() { return imgui_function_table_instance()->GetWindowPos(); }
	inline ImVec2 GetWindowSize() { return imgui_function_table_instance()->GetWindowSize(); }
	inline float GetWindowWidth() { return imgui_function_table_instance()->GetWindowWidth(); }
	inline float GetWindowHeight() { return imgui_function_table_instance()->GetWindowHeight(); }
	inline void SetNextWindowPos(const ImVec2& pos, ImGuiCond cond, const ImVec2& pivot) { imgui_function_table_instance()->SetNextWindowPos(pos, cond, pivot); }
	inline void SetNextWindowSize(const ImVec2& size, ImGuiCond cond) { imgui_function_table_instance()->SetNextWindowSize(size, cond); }
	inline void SetNextWindowSizeConstraints(const ImVec2& size_min, const ImVec2& size_max, ImGuiSizeCallback custom_callback, void* custom_callback_data) { imgui_function_table_instance()->SetNextWindowSizeConstraints(size_min, size_max, custom_callback, custom_callback_data); }
	inline void SetNextWindowContentSize(const ImVec2& size) { imgui_function_table_instance()->SetNextWindowContentSize(size); }
	inline void SetNextWindowCollapsed(bool collapsed, ImGuiCond cond) { imgui_function_table_instance()->SetNextWindowCollapsed(collapsed, cond); }
	inline void SetNextWindowFocus() { imgui_function_table_instance()->SetNextWindowFocus(); }
	inline void SetNextWindowBgAlpha(float alpha) { imgui_function_table_instance()->SetNextWindowBgAlpha(alpha); }
	inline void SetWindowPos(const ImVec2& pos, ImGuiCond cond) { imgui_function_table_instance()->SetWindowPos(pos, cond); }
	inline void SetWindowSize(const ImVec2& size, ImGuiCond cond) { imgui_function_table_instance()->SetWindowSize(size, cond); }
	inline void SetWindowCollapsed(bool collapsed, ImGuiCond cond) { imgui_function_table_instance()->SetWindowCollapsed(collapsed, cond); }
	inline void SetWindowFocus() { imgui_function_table_instance()->SetWindowFocus(); }
	inline void SetWindowFontScale(float scale) { imgui_function_table_instance()->SetWindowFontScale(scale); }
	inline void SetWindowPos(const char* name, const ImVec2& pos, ImGuiCond cond) { imgui_function_table_instance()->SetWindowPos2(name, pos, cond); }
	inline void SetWindowSize(const char* name, const ImVec2& size, ImGuiCond cond) { imgui_function_table_instance()->SetWindowSize2(name, size, cond); }
	inline void SetWindowCollapsed(const char* name, bool collapsed, ImGuiCond cond) { imgui_function_table_instance()->SetWindowCollapsed2(name, collapsed, cond); }
	inline void SetWindowFocus(const char* name) { imgui_function_table_instance()->SetWindowFocus2(name); }
	inline ImVec2 GetContentRegionAvail() { return imgui_function_table_instance()->GetContentRegionAvail(); }
	inline ImVec2 GetContentRegionMax() { return imgui_function_table_instance()->GetContentRegionMax(); }
	inline ImVec2 GetWindowContentRegionMin() { return imgui_function_table_instance()->GetWindowContentRegionMin(); }
	inline ImVec2 GetWindowContentRegionMax() { return imgui_function_table_instance()->GetWindowContentRegionMax(); }
	inline float GetScrollX() { return imgui_function_table_instance()->GetScrollX(); }
	inline float GetScrollY() { return imgui_function_table_instance()->GetScrollY(); }
	inline void SetScrollX(float scroll_x) { imgui_function_table_instance()->SetScrollX(scroll_x); }
	inline void SetScrollY(float scroll_y) { imgui_function_table_instance()->SetScrollY(scroll_y); }
	inline float GetScrollMaxX() { return imgui_function_table_instance()->GetScrollMaxX(); }
	inline float GetScrollMaxY() { return imgui_function_table_instance()->GetScrollMaxY(); }
	inline void SetScrollHereX(float center_x_ratio) { imgui_function_table_instance()->SetScrollHereX(center_x_ratio); }
	inline void SetScrollHereY(float center_y_ratio) { imgui_function_table_instance()->SetScrollHereY(center_y_ratio); }
	inline void SetScrollFromPosX(float local_x, float center_x_ratio) { imgui_function_table_instance()->SetScrollFromPosX(local_x, center_x_ratio); }
	inline void SetScrollFromPosY(float local_y, float center_y_ratio) { imgui_function_table_instance()->SetScrollFromPosY(local_y, center_y_ratio); }
	inline void PushFont(ImFont* font) { imgui_function_table_instance()->PushFont(font); }
	inline void PopFont() { imgui_function_table_instance()->PopFont(); }
	inline void PushStyleColor(ImGuiCol idx, ImU32 col) { imgui_function_table_instance()->PushStyleColor(idx, col); }
	inline void PushStyleColor(ImGuiCol idx, const ImVec4& col) { imgui_function_table_instance()->PushStyleColor2(idx, col); }
	inline void PopStyleColor(int count) { imgui_function_table_instance()->PopStyleColor(count); }
	inline void PushStyleVar(ImGuiStyleVar idx, float val) { imgui_function_table_instance()->PushStyleVar(idx, val); }
	inline void PushStyleVar(ImGuiStyleVar idx, const ImVec2& val) { imgui_function_table_instance()->PushStyleVar2(idx, val); }
	inline void PopStyleVar(int count) { imgui_function_table_instance()->PopStyleVar(count); }
	inline void PushAllowKeyboardFocus(bool allow_keyboard_focus) { imgui_function_table_instance()->PushAllowKeyboardFocus(allow_keyboard_focus); }
	inline void PopAllowKeyboardFocus() { imgui_function_table_instance()->PopAllowKeyboardFocus(); }
	inline void PushButtonRepeat(bool repeat) { imgui_function_table_instance()->PushButtonRepeat(repeat); }
	inline void PopButtonRepeat() { imgui_function_table_instance()->PopButtonRepeat(); }
	inline void PushItemWidth(float item_width) { imgui_function_table_instance()->PushItemWidth(item_width); }
	inline void PopItemWidth() { imgui_function_table_instance()->PopItemWidth(); }
	inline void SetNextItemWidth(float item_width) { imgui_function_table_instance()->SetNextItemWidth(item_width); }
	inline float CalcItemWidth() { return imgui_function_table_instance()->CalcItemWidth(); }
	inline void PushTextWrapPos(float wrap_local_pos_x) { imgui_function_table_instance()->PushTextWrapPos(wrap_local_pos_x); }
	inline void PopTextWrapPos() { imgui_function_table_instance()->PopTextWrapPos(); }
	inline ImFont* GetFont() { return imgui_function_table_instance()->GetFont(); }
	inline float GetFontSize() { return imgui_function_table_instance()->GetFontSize(); }
	inline ImVec2 GetFontTexUvWhitePixel() { return imgui_function_table_instance()->GetFontTexUvWhitePixel(); }
	inline ImU32 GetColorU32(ImGuiCol idx, float alpha_mul) { return imgui_function_table_instance()->GetColorU32(idx, alpha_mul); }
	inline ImU32 GetColorU32(const ImVec4& col) { return imgui_function_table_instance()->GetColorU322(col); }
	inline ImU32 GetColorU32(ImU32 col) { return imgui_function_table_instance()->GetColorU323(col); }
	inline const ImVec4& GetStyleColorVec4(ImGuiCol idx) { return imgui_function_table_instance()->GetStyleColorVec4(idx); }
	inline void Separator() { imgui_function_table_instance()->Separator(); }
	inline void SameLine(float offset_from_start_x, float spacing) { imgui_function_table_instance()->SameLine(offset_from_start_x, spacing); }
	inline void NewLine() { imgui_function_table_instance()->NewLine(); }
	inline void Spacing() { imgui_function_table_instance()->Spacing(); }
	inline void Dummy(const ImVec2& size) { imgui_function_table_instance()->Dummy(size); }
	inline void Indent(float indent_w) { imgui_function_table_instance()->Indent(indent_w); }
	inline void Unindent(float indent_w) { imgui_function_table_instance()->Unindent(indent_w); }
	inline void BeginGroup() { imgui_function_table_instance()->BeginGroup(); }
	inline void EndGroup() { imgui_function_table_instance()->EndGroup(); }
	inline ImVec2 GetCursorPos() { return imgui_function_table_instance()->GetCursorPos(); }
	inline float GetCursorPosX() { return imgui_function_table_instance()->GetCursorPosX(); }
	inline float GetCursorPosY() { return imgui_function_table_instance()->GetCursorPosY(); }
	inline void SetCursorPos(const ImVec2& local_pos) { imgui_function_table_instance()->SetCursorPos(local_pos); }
	inline void SetCursorPosX(float local_x) { imgui_function_table_instance()->SetCursorPosX(local_x); }
	inline void SetCursorPosY(float local_y) { imgui_function_table_instance()->SetCursorPosY(local_y); }
	inline ImVec2 GetCursorStartPos() { return imgui_function_table_instance()->GetCursorStartPos(); }
	inline ImVec2 GetCursorScreenPos() { return imgui_function_table_instance()->GetCursorScreenPos(); }
	inline void SetCursorScreenPos(const ImVec2& pos) { imgui_function_table_instance()->SetCursorScreenPos(pos); }
	inline void AlignTextToFramePadding() { imgui_function_table_instance()->AlignTextToFramePadding(); }
	inline float GetTextLineHeight() { return imgui_function_table_instance()->GetTextLineHeight(); }
	inline float GetTextLineHeightWithSpacing() { return imgui_function_table_instance()->GetTextLineHeightWithSpacing(); }
	inline float GetFrameHeight() { return imgui_function_table_instance()->GetFrameHeight(); }
	inline float GetFrameHeightWithSpacing() { return imgui_function_table_instance()->GetFrameHeightWithSpacing(); }
	inline void PushID(const char* str_id) { imgui_function_table_instance()->PushID(str_id); }
	inline void PushID(const char* str_id_begin, const char* str_id_end) { imgui_function_table_instance()->PushID2(str_id_begin, str_id_end); }
	inline void PushID(const void* ptr_id) { imgui_function_table_instance()->PushID3(ptr_id); }
	inline void PushID(int int_id) { imgui_function_table_instance()->PushID4(int_id); }
	inline void PopID() { imgui_function_table_instance()->PopID(); }
	inline ImGuiID GetID(const char* str_id) { return imgui_function_table_instance()->GetID(str_id); }
	inline ImGuiID GetID(const char* str_id_begin, const char* str_id_end) { return imgui_function_table_instance()->GetID2(str_id_begin, str_id_end); }
	inline ImGuiID GetID(const void* ptr_id) { return imgui_function_table_instance()->GetID3(ptr_id); }
	inline void TextUnformatted(const char* text, const char* text_end) { imgui_function_table_instance()->TextUnformatted(text, text_end); }
	inline void Text(const char* fmt, ...) { va_list args; va_start(args, fmt); imgui_function_table_instance()->TextV(fmt, args); va_end(args); }
	inline void TextV(const char* fmt, va_list args) { imgui_function_table_instance()->TextV(fmt, args); }
	inline void TextColored(const ImVec4& col, const char* fmt, ...) { va_list args; va_start(args, fmt); imgui_function_table_instance()->TextColoredV(col, fmt, args); va_end(args); }
	inline void TextColoredV(const ImVec4& col, const char* fmt, va_list args) { imgui_function_table_instance()->TextColoredV(col, fmt, args); }
	inline void TextDisabled(const char* fmt, ...) { va_list args; va_start(args, fmt); imgui_function_table_instance()->TextDisabledV(fmt, args); va_end(args); }
	inline void TextDisabledV(const char* fmt, va_list args) { imgui_function_table_instance()->TextDisabledV(fmt, args); }
	inline void TextWrapped(const char* fmt, ...) { va_list args; va_start(args, fmt); imgui_function_table_instance()->TextWrappedV(fmt, args); va_end(args); }
	inline void TextWrappedV(const char* fmt, va_list args) { imgui_function_table_instance()->TextWrappedV(fmt, args); }
	inline void LabelText(const char* label, const char* fmt, ...) { va_list args; va_start(args, fmt); imgui_function_table_instance()->LabelTextV(label, fmt, args); va_end(args); }
	inline void LabelTextV(const char* label, const char* fmt, va_list args) { imgui_function_table_instance()->LabelTextV(label, fmt, args); }
	inline void BulletText(const char* fmt, ...) { va_list args; va_start(args, fmt); imgui_function_table_instance()->BulletTextV(fmt, args); va_end(args); }
	inline void BulletTextV(const char* fmt, va_list args) { imgui_function_table_instance()->BulletTextV(fmt, args); }
	inline bool Button(const char* label, const ImVec2& size) { return imgui_function_table_instance()->Button(label, size); }
	inline bool SmallButton(const char* label) { return imgui_function_table_instance()->SmallButton(label); }
	inline bool InvisibleButton(const char* str_id, const ImVec2& size, ImGuiButtonFlags flags) { return imgui_function_table_instance()->InvisibleButton(str_id, size, flags); }
	inline bool ArrowButton(const char* str_id, ImGuiDir dir) { return imgui_function_table_instance()->ArrowButton(str_id, dir); }
	inline void Image(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col) { imgui_function_table_instance()->Image(user_texture_id, size, uv0, uv1, tint_col, border_col); }
	inline bool ImageButton(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col) { return imgui_function_table_instance()->ImageButton(user_texture_id, size, uv0, uv1, frame_padding, bg_col, tint_col); }
	inline bool Checkbox(const char* label, bool* v) { return imgui_function_table_instance()->Checkbox(label, v); }
	inline bool CheckboxFlags(const char* label, int* flags, int flags_value) { return imgui_function_table_instance()->CheckboxFlags(label, flags, flags_value); }
	inline bool CheckboxFlags(const char* label, unsigned int* flags, unsigned int flags_value) { return imgui_function_table_instance()->CheckboxFlags2(label, flags, flags_value); }
	inline bool RadioButton(const char* label, bool active) { return imgui_function_table_instance()->RadioButton(label, active); }
	inline bool RadioButton(const char* label, int* v, int v_button) { return imgui_function_table_instance()->RadioButton2(label, v, v_button); }
	inline void ProgressBar(float fraction, const ImVec2& size_arg, const char* overlay) { imgui_function_table_instance()->ProgressBar(fraction, size_arg, overlay); }
	inline void Bullet() { imgui_function_table_instance()->Bullet(); }
	inline bool BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags) { return imgui_function_table_instance()->BeginCombo(label, preview_value, flags); }
	inline void EndCombo() { imgui_function_table_instance()->EndCombo(); }
	inline bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items) { return imgui_function_table_instance()->Combo(label, current_item, items, items_count, popup_max_height_in_items); }
	inline bool Combo(const char* label, int* current_item, const char* items_separated_by_zeros, int popup_max_height_in_items) { return imgui_function_table_instance()->Combo2(label, current_item, items_separated_by_zeros, popup_max_height_in_items); }
	inline bool Combo(const char* label, int* current_item, bool(*items_getter)(void* data, int idx, const char** out_text), void* data, int items_count, int popup_max_height_in_items) { return imgui_function_table_instance()->Combo3(label, current_item, items_getter, data, items_count, popup_max_height_in_items); }
	inline bool DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragFloat(label, v, v_speed, v_min, v_max, format, flags); }
	inline bool DragFloat2(const char* label, float v[2], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragFloat2(label, v, v_speed, v_min, v_max, format, flags); }
	inline bool DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragFloat3(label, v, v_speed, v_min, v_max, format, flags); }
	inline bool DragFloat4(const char* label, float v[4], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragFloat4(label, v, v_speed, v_min, v_max, format, flags); }
	inline bool DragFloatRange2(const char* label, float* v_current_min, float* v_current_max, float v_speed, float v_min, float v_max, const char* format, const char* format_max, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragFloatRange2(label, v_current_min, v_current_max, v_speed, v_min, v_max, format, format_max, flags); }
	inline bool DragInt(const char* label, int* v, float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragInt(label, v, v_speed, v_min, v_max, format, flags); }
	inline bool DragInt2(const char* label, int v[2], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragInt2(label, v, v_speed, v_min, v_max, format, flags); }
	inline bool DragInt3(const char* label, int v[3], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragInt3(label, v, v_speed, v_min, v_max, format, flags); }
	inline bool DragInt4(const char* label, int v[4], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragInt4(label, v, v_speed, v_min, v_max, format, flags); }
	inline bool DragIntRange2(const char* label, int* v_current_min, int* v_current_max, float v_speed, int v_min, int v_max, const char* format, const char* format_max, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragIntRange2(label, v_current_min, v_current_max, v_speed, v_min, v_max, format, format_max, flags); }
	inline bool DragScalar(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragScalar(label, data_type, p_data, v_speed, p_min, p_max, format, flags); }
	inline bool DragScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->DragScalarN(label, data_type, p_data, components, v_speed, p_min, p_max, format, flags); }
	inline bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->SliderFloat(label, v, v_min, v_max, format, flags); }
	inline bool SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->SliderFloat2(label, v, v_min, v_max, format, flags); }
	inline bool SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->SliderFloat3(label, v, v_min, v_max, format, flags); }
	inline bool SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->SliderFloat4(label, v, v_min, v_max, format, flags); }
	inline bool SliderAngle(const char* label, float* v_rad, float v_degrees_min, float v_degrees_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->SliderAngle(label, v_rad, v_degrees_min, v_degrees_max, format, flags); }
	inline bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->SliderInt(label, v, v_min, v_max, format, flags); }
	inline bool SliderInt2(const char* label, int v[2], int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->SliderInt2(label, v, v_min, v_max, format, flags); }
	inline bool SliderInt3(const char* label, int v[3], int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->SliderInt3(label, v, v_min, v_max, format, flags); }
	inline bool SliderInt4(const char* label, int v[4], int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->SliderInt4(label, v, v_min, v_max, format, flags); }
	inline bool SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->SliderScalar(label, data_type, p_data, p_min, p_max, format, flags); }
	inline bool SliderScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->SliderScalarN(label, data_type, p_data, components, p_min, p_max, format, flags); }
	inline bool VSliderFloat(const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->VSliderFloat(label, size, v, v_min, v_max, format, flags); }
	inline bool VSliderInt(const char* label, const ImVec2& size, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->VSliderInt(label, size, v, v_min, v_max, format, flags); }
	inline bool VSliderScalar(const char* label, const ImVec2& size, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) { return imgui_function_table_instance()->VSliderScalar(label, size, data_type, p_data, p_min, p_max, format, flags); }
	inline bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) { return imgui_function_table_instance()->InputText(label, buf, buf_size, flags, callback, user_data); }
	inline bool InputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) { return imgui_function_table_instance()->InputTextMultiline(label, buf, buf_size, size, flags, callback, user_data); }
	inline bool InputTextWithHint(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) { return imgui_function_table_instance()->InputTextWithHint(label, hint, buf, buf_size, flags, callback, user_data); }
	inline bool InputFloat(const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags) { return imgui_function_table_instance()->InputFloat(label, v, step, step_fast, format, flags); }
	inline bool InputFloat2(const char* label, float v[2], const char* format, ImGuiInputTextFlags flags) { return imgui_function_table_instance()->InputFloat2(label, v, format, flags); }
	inline bool InputFloat3(const char* label, float v[3], const char* format, ImGuiInputTextFlags flags) { return imgui_function_table_instance()->InputFloat3(label, v, format, flags); }
	inline bool InputFloat4(const char* label, float v[4], const char* format, ImGuiInputTextFlags flags) { return imgui_function_table_instance()->InputFloat4(label, v, format, flags); }
	inline bool InputInt(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags) { return imgui_function_table_instance()->InputInt(label, v, step, step_fast, flags); }
	inline bool InputInt2(const char* label, int v[2], ImGuiInputTextFlags flags) { return imgui_function_table_instance()->InputInt2(label, v, flags); }
	inline bool InputInt3(const char* label, int v[3], ImGuiInputTextFlags flags) { return imgui_function_table_instance()->InputInt3(label, v, flags); }
	inline bool InputInt4(const char* label, int v[4], ImGuiInputTextFlags flags) { return imgui_function_table_instance()->InputInt4(label, v, flags); }
	inline bool InputDouble(const char* label, double* v, double step, double step_fast, const char* format, ImGuiInputTextFlags flags) { return imgui_function_table_instance()->InputDouble(label, v, step, step_fast, format, flags); }
	inline bool InputScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags) { return imgui_function_table_instance()->InputScalar(label, data_type, p_data, p_step, p_step_fast, format, flags); }
	inline bool InputScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags) { return imgui_function_table_instance()->InputScalarN(label, data_type, p_data, components, p_step, p_step_fast, format, flags); }
	inline bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags) { return imgui_function_table_instance()->ColorEdit3(label, col, flags); }
	inline bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags) { return imgui_function_table_instance()->ColorEdit4(label, col, flags); }
	inline bool ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags) { return imgui_function_table_instance()->ColorPicker3(label, col, flags); }
	inline bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags, const float* ref_col) { return imgui_function_table_instance()->ColorPicker4(label, col, flags, ref_col); }
	inline bool ColorButton(const char* desc_id, const ImVec4& col, ImGuiColorEditFlags flags, ImVec2 size) { return imgui_function_table_instance()->ColorButton(desc_id, col, flags, size); }
	inline void SetColorEditOptions(ImGuiColorEditFlags flags) { imgui_function_table_instance()->SetColorEditOptions(flags); }
	inline bool TreeNode(const char* label) { return imgui_function_table_instance()->TreeNode(label); }
	inline bool TreeNode(const char* str_id, const char* fmt, ...) { va_list args; va_start(args, fmt); return imgui_function_table_instance()->TreeNodeV(str_id, fmt, args); va_end(args); }
	inline bool TreeNode(const void* ptr_id, const char* fmt, ...) { va_list args; va_start(args, fmt); return imgui_function_table_instance()->TreeNodeV2(ptr_id, fmt, args); va_end(args); }
	inline bool TreeNodeV(const char* str_id, const char* fmt, va_list args) { return imgui_function_table_instance()->TreeNodeV(str_id, fmt, args); }
	inline bool TreeNodeV(const void* ptr_id, const char* fmt, va_list args) { return imgui_function_table_instance()->TreeNodeV2(ptr_id, fmt, args); }
	inline bool TreeNodeEx(const char* label, ImGuiTreeNodeFlags flags) { return imgui_function_table_instance()->TreeNodeEx(label, flags); }
	inline bool TreeNodeEx(const char* str_id, ImGuiTreeNodeFlags flags, const char* fmt, ...) { va_list args; va_start(args, fmt); return imgui_function_table_instance()->TreeNodeExV(str_id, flags, fmt, args); va_end(args); }
	inline bool TreeNodeEx(const void* ptr_id, ImGuiTreeNodeFlags flags, const char* fmt, ...) { va_list args; va_start(args, fmt); return imgui_function_table_instance()->TreeNodeExV2(ptr_id, flags, fmt, args); va_end(args); }
	inline bool TreeNodeExV(const char* str_id, ImGuiTreeNodeFlags flags, const char* fmt, va_list args) { return imgui_function_table_instance()->TreeNodeExV(str_id, flags, fmt, args); }
	inline bool TreeNodeExV(const void* ptr_id, ImGuiTreeNodeFlags flags, const char* fmt, va_list args) { return imgui_function_table_instance()->TreeNodeExV2(ptr_id, flags, fmt, args); }
	inline void TreePush(const char* str_id) { imgui_function_table_instance()->TreePush(str_id); }
	inline void TreePush(const void* ptr_id) { imgui_function_table_instance()->TreePush2(ptr_id); }
	inline void TreePop() { imgui_function_table_instance()->TreePop(); }
	inline float GetTreeNodeToLabelSpacing() { return imgui_function_table_instance()->GetTreeNodeToLabelSpacing(); }
	inline bool CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags) { return imgui_function_table_instance()->CollapsingHeader(label, flags); }
	inline bool CollapsingHeader(const char* label, bool* p_visible, ImGuiTreeNodeFlags flags) { return imgui_function_table_instance()->CollapsingHeader2(label, p_visible, flags); }
	inline void SetNextItemOpen(bool is_open, ImGuiCond cond) { imgui_function_table_instance()->SetNextItemOpen(is_open, cond); }
	inline bool Selectable(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size) { return imgui_function_table_instance()->Selectable(label, selected, flags, size); }
	inline bool Selectable(const char* label, bool* p_selected, ImGuiSelectableFlags flags, const ImVec2& size) { return imgui_function_table_instance()->Selectable2(label, p_selected, flags, size); }
	inline bool BeginListBox(const char* label, const ImVec2& size) { return imgui_function_table_instance()->BeginListBox(label, size); }
	inline void EndListBox() { imgui_function_table_instance()->EndListBox(); }
	inline bool ListBox(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items) { return imgui_function_table_instance()->ListBox(label, current_item, items, items_count, height_in_items); }
	inline bool ListBox(const char* label, int* current_item, bool(*items_getter)(void* data, int idx, const char** out_text), void* data, int items_count, int height_in_items) { return imgui_function_table_instance()->ListBox2(label, current_item, items_getter, data, items_count, height_in_items); }
	inline void PlotLines(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride) { imgui_function_table_instance()->PlotLines(label, values, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size, stride); }
	inline void PlotLines(const char* label, float(*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size) { imgui_function_table_instance()->PlotLines2(label, values_getter, data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size); }
	inline void PlotHistogram(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride) { imgui_function_table_instance()->PlotHistogram(label, values, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size, stride); }
	inline void PlotHistogram(const char* label, float(*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size) { imgui_function_table_instance()->PlotHistogram2(label, values_getter, data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size); }
	inline void Value(const char* prefix, bool b) { imgui_function_table_instance()->Value(prefix, b); }
	inline void Value(const char* prefix, int v) { imgui_function_table_instance()->Value2(prefix, v); }
	inline void Value(const char* prefix, unsigned int v) { imgui_function_table_instance()->Value3(prefix, v); }
	inline void Value(const char* prefix, float v, const char* float_format) { imgui_function_table_instance()->Value4(prefix, v, float_format); }
	inline bool BeginMenuBar() { return imgui_function_table_instance()->BeginMenuBar(); }
	inline void EndMenuBar() { imgui_function_table_instance()->EndMenuBar(); }
	inline bool BeginMainMenuBar() { return imgui_function_table_instance()->BeginMainMenuBar(); }
	inline void EndMainMenuBar() { imgui_function_table_instance()->EndMainMenuBar(); }
	inline bool BeginMenu(const char* label, bool enabled) { return imgui_function_table_instance()->BeginMenu(label, enabled); }
	inline void EndMenu() { imgui_function_table_instance()->EndMenu(); }
	inline bool MenuItem(const char* label, const char* shortcut, bool selected, bool enabled) { return imgui_function_table_instance()->MenuItem(label, shortcut, selected, enabled); }
	inline bool MenuItem(const char* label, const char* shortcut, bool* p_selected, bool enabled) { return imgui_function_table_instance()->MenuItem2(label, shortcut, p_selected, enabled); }
	inline void BeginTooltip() { imgui_function_table_instance()->BeginTooltip(); }
	inline void EndTooltip() { imgui_function_table_instance()->EndTooltip(); }
	inline void SetTooltip(const char* fmt, ...) { va_list args; va_start(args, fmt); imgui_function_table_instance()->SetTooltipV(fmt, args); va_end(args); }
	inline void SetTooltipV(const char* fmt, va_list args) { imgui_function_table_instance()->SetTooltipV(fmt, args); }
	inline bool BeginPopup(const char* str_id, ImGuiWindowFlags flags) { return imgui_function_table_instance()->BeginPopup(str_id, flags); }
	inline bool BeginPopupModal(const char* name, bool* p_open, ImGuiWindowFlags flags) { return imgui_function_table_instance()->BeginPopupModal(name, p_open, flags); }
	inline void EndPopup() { imgui_function_table_instance()->EndPopup(); }
	inline void OpenPopup(const char* str_id, ImGuiPopupFlags popup_flags) { imgui_function_table_instance()->OpenPopup(str_id, popup_flags); }
	inline void OpenPopup(ImGuiID id, ImGuiPopupFlags popup_flags) { imgui_function_table_instance()->OpenPopup2(id, popup_flags); }
	inline void OpenPopupOnItemClick(const char* str_id, ImGuiPopupFlags popup_flags) { imgui_function_table_instance()->OpenPopupOnItemClick(str_id, popup_flags); }
	inline void CloseCurrentPopup() { imgui_function_table_instance()->CloseCurrentPopup(); }
	inline bool BeginPopupContextItem(const char* str_id, ImGuiPopupFlags popup_flags) { return imgui_function_table_instance()->BeginPopupContextItem(str_id, popup_flags); }
	inline bool BeginPopupContextWindow(const char* str_id, ImGuiPopupFlags popup_flags) { return imgui_function_table_instance()->BeginPopupContextWindow(str_id, popup_flags); }
	inline bool BeginPopupContextVoid(const char* str_id, ImGuiPopupFlags popup_flags) { return imgui_function_table_instance()->BeginPopupContextVoid(str_id, popup_flags); }
	inline bool IsPopupOpen(const char* str_id, ImGuiPopupFlags flags) { return imgui_function_table_instance()->IsPopupOpen(str_id, flags); }
	inline bool BeginTable(const char* str_id, int column, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width) { return imgui_function_table_instance()->BeginTable(str_id, column, flags, outer_size, inner_width); }
	inline void EndTable() { imgui_function_table_instance()->EndTable(); }
	inline void TableNextRow(ImGuiTableRowFlags row_flags, float min_row_height) { imgui_function_table_instance()->TableNextRow(row_flags, min_row_height); }
	inline bool TableNextColumn() { return imgui_function_table_instance()->TableNextColumn(); }
	inline bool TableSetColumnIndex(int column_n) { return imgui_function_table_instance()->TableSetColumnIndex(column_n); }
	inline void TableSetupColumn(const char* label, ImGuiTableColumnFlags flags, float init_width_or_weight, ImGuiID user_id) { imgui_function_table_instance()->TableSetupColumn(label, flags, init_width_or_weight, user_id); }
	inline void TableSetupScrollFreeze(int cols, int rows) { imgui_function_table_instance()->TableSetupScrollFreeze(cols, rows); }
	inline void TableHeadersRow() { imgui_function_table_instance()->TableHeadersRow(); }
	inline void TableHeader(const char* label) { imgui_function_table_instance()->TableHeader(label); }
	inline ImGuiTableSortSpecs* TableGetSortSpecs() { return imgui_function_table_instance()->TableGetSortSpecs(); }
	inline int TableGetColumnCount() { return imgui_function_table_instance()->TableGetColumnCount(); }
	inline int TableGetColumnIndex() { return imgui_function_table_instance()->TableGetColumnIndex(); }
	inline int TableGetRowIndex() { return imgui_function_table_instance()->TableGetRowIndex(); }
	inline const char* TableGetColumnName(int column_n) { return imgui_function_table_instance()->TableGetColumnName(column_n); }
	inline ImGuiTableColumnFlags TableGetColumnFlags(int column_n) { return imgui_function_table_instance()->TableGetColumnFlags(column_n); }
	inline void TableSetColumnEnabled(int column_n, bool v) { imgui_function_table_instance()->TableSetColumnEnabled(column_n, v); }
	inline void TableSetBgColor(ImGuiTableBgTarget target, ImU32 color, int column_n) { imgui_function_table_instance()->TableSetBgColor(target, color, column_n); }
	inline void Columns(int count, const char* id, bool border) { imgui_function_table_instance()->Columns(count, id, border); }
	inline void NextColumn() { imgui_function_table_instance()->NextColumn(); }
	inline int GetColumnIndex() { return imgui_function_table_instance()->GetColumnIndex(); }
	inline float GetColumnWidth(int column_index) { return imgui_function_table_instance()->GetColumnWidth(column_index); }
	inline void SetColumnWidth(int column_index, float width) { imgui_function_table_instance()->SetColumnWidth(column_index, width); }
	inline float GetColumnOffset(int column_index) { return imgui_function_table_instance()->GetColumnOffset(column_index); }
	inline void SetColumnOffset(int column_index, float offset_x) { imgui_function_table_instance()->SetColumnOffset(column_index, offset_x); }
	inline int GetColumnsCount() { return imgui_function_table_instance()->GetColumnsCount(); }
	inline bool BeginTabBar(const char* str_id, ImGuiTabBarFlags flags) { return imgui_function_table_instance()->BeginTabBar(str_id, flags); }
	inline void EndTabBar() { imgui_function_table_instance()->EndTabBar(); }
	inline bool BeginTabItem(const char* label, bool* p_open, ImGuiTabItemFlags flags) { return imgui_function_table_instance()->BeginTabItem(label, p_open, flags); }
	inline void EndTabItem() { imgui_function_table_instance()->EndTabItem(); }
	inline bool TabItemButton(const char* label, ImGuiTabItemFlags flags) { return imgui_function_table_instance()->TabItemButton(label, flags); }
	inline void SetTabItemClosed(const char* tab_or_docked_window_label) { imgui_function_table_instance()->SetTabItemClosed(tab_or_docked_window_label); }
	inline ImGuiID DockSpace(ImGuiID id, const ImVec2& size, int flags, const struct ImGuiWindowClass* window_class) { return imgui_function_table_instance()->DockSpace(id, size, flags, window_class); }
	inline void SetNextWindowDockID(ImGuiID dock_id, ImGuiCond cond) { imgui_function_table_instance()->SetNextWindowDockID(dock_id, cond); }
	inline void SetNextWindowClass(const struct ImGuiWindowClass* window_class) { imgui_function_table_instance()->SetNextWindowClass(window_class); }
	inline ImGuiID GetWindowDockID() { return imgui_function_table_instance()->GetWindowDockID(); }
	inline bool IsWindowDocked() { return imgui_function_table_instance()->IsWindowDocked(); }
	inline bool BeginDragDropSource(ImGuiDragDropFlags flags) { return imgui_function_table_instance()->BeginDragDropSource(flags); }
	inline bool SetDragDropPayload(const char* type, const void* data, size_t sz, ImGuiCond cond) { return imgui_function_table_instance()->SetDragDropPayload(type, data, sz, cond); }
	inline void EndDragDropSource() { imgui_function_table_instance()->EndDragDropSource(); }
	inline bool BeginDragDropTarget() { return imgui_function_table_instance()->BeginDragDropTarget(); }
	inline const ImGuiPayload* AcceptDragDropPayload(const char* type, ImGuiDragDropFlags flags) { return imgui_function_table_instance()->AcceptDragDropPayload(type, flags); }
	inline void EndDragDropTarget() { imgui_function_table_instance()->EndDragDropTarget(); }
	inline const ImGuiPayload* GetDragDropPayload() { return imgui_function_table_instance()->GetDragDropPayload(); }
	inline void BeginDisabled(bool disabled) { imgui_function_table_instance()->BeginDisabled(disabled); }
	inline void EndDisabled() { imgui_function_table_instance()->EndDisabled(); }
	inline void PushClipRect(const ImVec2& clip_rect_min, const ImVec2& clip_rect_max, bool intersect_with_current_clip_rect) { imgui_function_table_instance()->PushClipRect(clip_rect_min, clip_rect_max, intersect_with_current_clip_rect); }
	inline void PopClipRect() { imgui_function_table_instance()->PopClipRect(); }
	inline void SetItemDefaultFocus() { imgui_function_table_instance()->SetItemDefaultFocus(); }
	inline void SetKeyboardFocusHere(int offset) { imgui_function_table_instance()->SetKeyboardFocusHere(offset); }
	inline bool IsItemHovered(ImGuiHoveredFlags flags) { return imgui_function_table_instance()->IsItemHovered(flags); }
	inline bool IsItemActive() { return imgui_function_table_instance()->IsItemActive(); }
	inline bool IsItemFocused() { return imgui_function_table_instance()->IsItemFocused(); }
	inline bool IsItemClicked(ImGuiMouseButton mouse_button) { return imgui_function_table_instance()->IsItemClicked(mouse_button); }
	inline bool IsItemVisible() { return imgui_function_table_instance()->IsItemVisible(); }
	inline bool IsItemEdited() { return imgui_function_table_instance()->IsItemEdited(); }
	inline bool IsItemActivated() { return imgui_function_table_instance()->IsItemActivated(); }
	inline bool IsItemDeactivated() { return imgui_function_table_instance()->IsItemDeactivated(); }
	inline bool IsItemDeactivatedAfterEdit() { return imgui_function_table_instance()->IsItemDeactivatedAfterEdit(); }
	inline bool IsItemToggledOpen() { return imgui_function_table_instance()->IsItemToggledOpen(); }
	inline bool IsAnyItemHovered() { return imgui_function_table_instance()->IsAnyItemHovered(); }
	inline bool IsAnyItemActive() { return imgui_function_table_instance()->IsAnyItemActive(); }
	inline bool IsAnyItemFocused() { return imgui_function_table_instance()->IsAnyItemFocused(); }
	inline ImVec2 GetItemRectMin() { return imgui_function_table_instance()->GetItemRectMin(); }
	inline ImVec2 GetItemRectMax() { return imgui_function_table_instance()->GetItemRectMax(); }
	inline ImVec2 GetItemRectSize() { return imgui_function_table_instance()->GetItemRectSize(); }
	inline void SetItemAllowOverlap() { imgui_function_table_instance()->SetItemAllowOverlap(); }
	inline bool IsRectVisible(const ImVec2& size) { return imgui_function_table_instance()->IsRectVisible(size); }
	inline bool IsRectVisible(const ImVec2& rect_min, const ImVec2& rect_max) { return imgui_function_table_instance()->IsRectVisible2(rect_min, rect_max); }
	inline double GetTime() { return imgui_function_table_instance()->GetTime(); }
	inline int GetFrameCount() { return imgui_function_table_instance()->GetFrameCount(); }
	inline ImDrawList* GetBackgroundDrawList() { return imgui_function_table_instance()->GetBackgroundDrawList(); }
	inline ImDrawList* GetForegroundDrawList() { return imgui_function_table_instance()->GetForegroundDrawList(); }
	inline ImDrawList* GetBackgroundDrawList(ImGuiViewport* viewport) { return imgui_function_table_instance()->GetBackgroundDrawList2(viewport); }
	inline ImDrawList* GetForegroundDrawList(ImGuiViewport* viewport) { return imgui_function_table_instance()->GetForegroundDrawList2(viewport); }
	inline ImDrawListSharedData* GetDrawListSharedData() { return imgui_function_table_instance()->GetDrawListSharedData(); }
	inline const char* GetStyleColorName(ImGuiCol idx) { return imgui_function_table_instance()->GetStyleColorName(idx); }
	inline void SetStateStorage(ImGuiStorage* storage) { imgui_function_table_instance()->SetStateStorage(storage); }
	inline ImGuiStorage* GetStateStorage() { return imgui_function_table_instance()->GetStateStorage(); }
	inline bool BeginChildFrame(ImGuiID id, const ImVec2& size, ImGuiWindowFlags flags) { return imgui_function_table_instance()->BeginChildFrame(id, size, flags); }
	inline void EndChildFrame() { imgui_function_table_instance()->EndChildFrame(); }
	inline ImVec2 CalcTextSize(const char* text, const char* text_end, bool hide_text_after_double_hash, float wrap_width) { return imgui_function_table_instance()->CalcTextSize(text, text_end, hide_text_after_double_hash, wrap_width); }
	inline ImVec4 ColorConvertU32ToFloat4(ImU32 in) { return imgui_function_table_instance()->ColorConvertU32ToFloat4(in); }
	inline ImU32 ColorConvertFloat4ToU32(const ImVec4& in) { return imgui_function_table_instance()->ColorConvertFloat4ToU32(in); }
	inline void ColorConvertRGBtoHSV(float r, float g, float b, float& out_h, float& out_s, float& out_v) { imgui_function_table_instance()->ColorConvertRGBtoHSV(r, g, b, out_h, out_s, out_v); }
	inline void ColorConvertHSVtoRGB(float h, float s, float v, float& out_r, float& out_g, float& out_b) { imgui_function_table_instance()->ColorConvertHSVtoRGB(h, s, v, out_r, out_g, out_b); }
	inline int GetKeyIndex(ImGuiKey imgui_key) { return imgui_function_table_instance()->GetKeyIndex(imgui_key); }
	inline bool IsKeyDown(int user_key_index) { return imgui_function_table_instance()->IsKeyDown(user_key_index); }
	inline bool IsKeyPressed(int user_key_index, bool repeat) { return imgui_function_table_instance()->IsKeyPressed(user_key_index, repeat); }
	inline bool IsKeyReleased(int user_key_index) { return imgui_function_table_instance()->IsKeyReleased(user_key_index); }
	inline int GetKeyPressedAmount(int key_index, float repeat_delay, float rate) { return imgui_function_table_instance()->GetKeyPressedAmount(key_index, repeat_delay, rate); }
	inline void CaptureKeyboardFromApp(bool want_capture_keyboard_value) { imgui_function_table_instance()->CaptureKeyboardFromApp(want_capture_keyboard_value); }
	inline bool IsMouseDown(ImGuiMouseButton button) { return imgui_function_table_instance()->IsMouseDown(button); }
	inline bool IsMouseClicked(ImGuiMouseButton button, bool repeat) { return imgui_function_table_instance()->IsMouseClicked(button, repeat); }
	inline bool IsMouseReleased(ImGuiMouseButton button) { return imgui_function_table_instance()->IsMouseReleased(button); }
	inline bool IsMouseDoubleClicked(ImGuiMouseButton button) { return imgui_function_table_instance()->IsMouseDoubleClicked(button); }
	inline int GetMouseClickedCount(ImGuiMouseButton button) { return imgui_function_table_instance()->GetMouseClickedCount(button); }
	inline bool IsMouseHoveringRect(const ImVec2& r_min, const ImVec2& r_max, bool clip) { return imgui_function_table_instance()->IsMouseHoveringRect(r_min, r_max, clip); }
	inline bool IsMousePosValid(const ImVec2* mouse_pos) { return imgui_function_table_instance()->IsMousePosValid(mouse_pos); }
	inline bool IsAnyMouseDown() { return imgui_function_table_instance()->IsAnyMouseDown(); }
	inline ImVec2 GetMousePos() { return imgui_function_table_instance()->GetMousePos(); }
	inline ImVec2 GetMousePosOnOpeningCurrentPopup() { return imgui_function_table_instance()->GetMousePosOnOpeningCurrentPopup(); }
	inline bool IsMouseDragging(ImGuiMouseButton button, float lock_threshold) { return imgui_function_table_instance()->IsMouseDragging(button, lock_threshold); }
	inline ImVec2 GetMouseDragDelta(ImGuiMouseButton button, float lock_threshold) { return imgui_function_table_instance()->GetMouseDragDelta(button, lock_threshold); }
	inline void ResetMouseDragDelta(ImGuiMouseButton button) { imgui_function_table_instance()->ResetMouseDragDelta(button); }
	inline ImGuiMouseCursor GetMouseCursor() { return imgui_function_table_instance()->GetMouseCursor(); }
	inline void SetMouseCursor(ImGuiMouseCursor cursor_type) { imgui_function_table_instance()->SetMouseCursor(cursor_type); }
	inline void CaptureMouseFromApp(bool want_capture_mouse_value) { imgui_function_table_instance()->CaptureMouseFromApp(want_capture_mouse_value); }
	inline const char* GetClipboardText() { return imgui_function_table_instance()->GetClipboardText(); }
	inline void SetClipboardText(const char* text) { imgui_function_table_instance()->SetClipboardText(text); }
	inline bool DebugCheckVersionAndDataLayout(const char* version_str, size_t sz_io, size_t sz_style, size_t sz_vec2, size_t sz_vec4, size_t sz_drawvert, size_t sz_drawidx) { return imgui_function_table_instance()->DebugCheckVersionAndDataLayout(version_str, sz_io, sz_style, sz_vec2, sz_vec4, sz_drawvert, sz_drawidx); }
	inline void SetAllocatorFunctions(ImGuiMemAllocFunc alloc_func, ImGuiMemFreeFunc free_func, void* user_data) { imgui_function_table_instance()->SetAllocatorFunctions(alloc_func, free_func, user_data); }
	inline void GetAllocatorFunctions(ImGuiMemAllocFunc* p_alloc_func, ImGuiMemFreeFunc* p_free_func, void** p_user_data) { imgui_function_table_instance()->GetAllocatorFunctions(p_alloc_func, p_free_func, p_user_data); }
	inline void* MemAlloc(size_t size) { return imgui_function_table_instance()->MemAlloc(size); }
	inline void MemFree(void* ptr) { imgui_function_table_instance()->MemFree(ptr); }

}

inline int ImGuiStorage::GetInt(ImGuiID key, int default_val) const { return imgui_function_table_instance()->ImGuiStorage_GetInt(this, key, default_val); }
inline void ImGuiStorage::SetInt(ImGuiID key, int val) { imgui_function_table_instance()->ImGuiStorage_SetInt(this, key, val); }
inline bool ImGuiStorage::GetBool(ImGuiID key, bool default_val) const { return imgui_function_table_instance()->ImGuiStorage_GetBool(this, key, default_val); }
inline void ImGuiStorage::SetBool(ImGuiID key, bool val) { imgui_function_table_instance()->ImGuiStorage_SetBool(this, key, val); }
inline float ImGuiStorage::GetFloat(ImGuiID key, float default_val) const { return imgui_function_table_instance()->ImGuiStorage_GetFloat(this, key, default_val); }
inline void ImGuiStorage::SetFloat(ImGuiID key, float val) { imgui_function_table_instance()->ImGuiStorage_SetFloat(this, key, val); }
inline void* ImGuiStorage::GetVoidPtr(ImGuiID key) const { return imgui_function_table_instance()->ImGuiStorage_GetVoidPtr(this, key); }
inline void ImGuiStorage::SetVoidPtr(ImGuiID key, void* val) { imgui_function_table_instance()->ImGuiStorage_SetVoidPtr(this, key, val); }
inline int* ImGuiStorage::GetIntRef(ImGuiID key, int default_val) { return imgui_function_table_instance()->ImGuiStorage_GetIntRef(this, key, default_val); }
inline bool* ImGuiStorage::GetBoolRef(ImGuiID key, bool default_val) { return imgui_function_table_instance()->ImGuiStorage_GetBoolRef(this, key, default_val); }
inline float* ImGuiStorage::GetFloatRef(ImGuiID key, float default_val) { return imgui_function_table_instance()->ImGuiStorage_GetFloatRef(this, key, default_val); }
inline void** ImGuiStorage::GetVoidPtrRef(ImGuiID key, void* default_val) { return imgui_function_table_instance()->ImGuiStorage_GetVoidPtrRef(this, key, default_val); }
inline void ImGuiStorage::SetAllInt(int val) { imgui_function_table_instance()->ImGuiStorage_SetAllInt(this, val); }
inline void ImGuiStorage::BuildSortByKey() { imgui_function_table_instance()->ImGuiStorage_BuildSortByKey(this); }
inline ImGuiListClipper::ImGuiListClipper() { imgui_function_table_instance()->ConstructImGuiListClipper(this); }
inline ImGuiListClipper::~ImGuiListClipper() { imgui_function_table_instance()->DestructImGuiListClipper(this); }
inline void ImGuiListClipper::Begin(int items_count, float items_height) { imgui_function_table_instance()->ImGuiListClipper_Begin(this, items_count, items_height); }
inline void ImGuiListClipper::End() { imgui_function_table_instance()->ImGuiListClipper_End(this); }
inline bool ImGuiListClipper::Step() { return imgui_function_table_instance()->ImGuiListClipper_Step(this); }
inline void ImGuiListClipper::ForceDisplayRangeByIndices(int item_min, int item_max) { imgui_function_table_instance()->ImGuiListClipper_ForceDisplayRangeByIndices(this, item_min, item_max); }
inline void ImDrawList::PushClipRect(ImVec2 clip_rect_min, ImVec2 clip_rect_max, bool intersect_with_current_clip_rect) { imgui_function_table_instance()->ImDrawList_PushClipRect(this, clip_rect_min, clip_rect_max, intersect_with_current_clip_rect); }
inline void ImDrawList::PushClipRectFullScreen() { imgui_function_table_instance()->ImDrawList_PushClipRectFullScreen(this); }
inline void ImDrawList::PopClipRect() { imgui_function_table_instance()->ImDrawList_PopClipRect(this); }
inline void ImDrawList::PushTextureID(ImTextureID texture_id) { imgui_function_table_instance()->ImDrawList_PushTextureID(this, texture_id); }
inline void ImDrawList::PopTextureID() { imgui_function_table_instance()->ImDrawList_PopTextureID(this); }
inline void ImDrawList::AddLine(const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness) { imgui_function_table_instance()->ImDrawList_AddLine(this, p1, p2, col, thickness); }
inline void ImDrawList::AddRect(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness) { imgui_function_table_instance()->ImDrawList_AddRect(this, p_min, p_max, col, rounding, flags, thickness); }
inline void ImDrawList::AddRectFilled(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags) { imgui_function_table_instance()->ImDrawList_AddRectFilled(this, p_min, p_max, col, rounding, flags); }
inline void ImDrawList::AddRectFilledMultiColor(const ImVec2& p_min, const ImVec2& p_max, ImU32 col_upr_left, ImU32 col_upr_right, ImU32 col_bot_right, ImU32 col_bot_left) { imgui_function_table_instance()->ImDrawList_AddRectFilledMultiColor(this, p_min, p_max, col_upr_left, col_upr_right, col_bot_right, col_bot_left); }
inline void ImDrawList::AddQuad(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness) { imgui_function_table_instance()->ImDrawList_AddQuad(this, p1, p2, p3, p4, col, thickness); }
inline void ImDrawList::AddQuadFilled(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col) { imgui_function_table_instance()->ImDrawList_AddQuadFilled(this, p1, p2, p3, p4, col); }
inline void ImDrawList::AddTriangle(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness) { imgui_function_table_instance()->ImDrawList_AddTriangle(this, p1, p2, p3, col, thickness); }
inline void ImDrawList::AddTriangleFilled(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col) { imgui_function_table_instance()->ImDrawList_AddTriangleFilled(this, p1, p2, p3, col); }
inline void ImDrawList::AddCircle(const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) { imgui_function_table_instance()->ImDrawList_AddCircle(this, center, radius, col, num_segments, thickness); }
inline void ImDrawList::AddCircleFilled(const ImVec2& center, float radius, ImU32 col, int num_segments) { imgui_function_table_instance()->ImDrawList_AddCircleFilled(this, center, radius, col, num_segments); }
inline void ImDrawList::AddNgon(const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) { imgui_function_table_instance()->ImDrawList_AddNgon(this, center, radius, col, num_segments, thickness); }
inline void ImDrawList::AddNgonFilled(const ImVec2& center, float radius, ImU32 col, int num_segments) { imgui_function_table_instance()->ImDrawList_AddNgonFilled(this, center, radius, col, num_segments); }
inline void ImDrawList::AddText(const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end) { imgui_function_table_instance()->ImDrawList_AddText(this, pos, col, text_begin, text_end); }
inline void ImDrawList::AddText(const ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect) { imgui_function_table_instance()->ImDrawList_AddText2(this, font, font_size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip_rect); }
inline void ImDrawList::AddPolyline(const ImVec2* points, int num_points, ImU32 col, ImDrawFlags flags, float thickness) { imgui_function_table_instance()->ImDrawList_AddPolyline(this, points, num_points, col, flags, thickness); }
inline void ImDrawList::AddConvexPolyFilled(const ImVec2* points, int num_points, ImU32 col) { imgui_function_table_instance()->ImDrawList_AddConvexPolyFilled(this, points, num_points, col); }
inline void ImDrawList::AddBezierCubic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness, int num_segments) { imgui_function_table_instance()->ImDrawList_AddBezierCubic(this, p1, p2, p3, p4, col, thickness, num_segments); }
inline void ImDrawList::AddBezierQuadratic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness, int num_segments) { imgui_function_table_instance()->ImDrawList_AddBezierQuadratic(this, p1, p2, p3, col, thickness, num_segments); }
inline void ImDrawList::AddImage(ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col) { imgui_function_table_instance()->ImDrawList_AddImage(this, user_texture_id, p_min, p_max, uv_min, uv_max, col); }
inline void ImDrawList::AddImageQuad(ImTextureID user_texture_id, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3, const ImVec2& uv4, ImU32 col) { imgui_function_table_instance()->ImDrawList_AddImageQuad(this, user_texture_id, p1, p2, p3, p4, uv1, uv2, uv3, uv4, col); }
inline void ImDrawList::AddImageRounded(ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col, float rounding, ImDrawFlags flags) { imgui_function_table_instance()->ImDrawList_AddImageRounded(this, user_texture_id, p_min, p_max, uv_min, uv_max, col, rounding, flags); }
inline void ImDrawList::PathArcTo(const ImVec2& center, float radius, float a_min, float a_max, int num_segments) { imgui_function_table_instance()->ImDrawList_PathArcTo(this, center, radius, a_min, a_max, num_segments); }
inline void ImDrawList::PathArcToFast(const ImVec2& center, float radius, int a_min_of_12, int a_max_of_12) { imgui_function_table_instance()->ImDrawList_PathArcToFast(this, center, radius, a_min_of_12, a_max_of_12); }
inline void ImDrawList::PathBezierCubicCurveTo(const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, int num_segments) { imgui_function_table_instance()->ImDrawList_PathBezierCubicCurveTo(this, p2, p3, p4, num_segments); }
inline void ImDrawList::PathBezierQuadraticCurveTo(const ImVec2& p2, const ImVec2& p3, int num_segments) { imgui_function_table_instance()->ImDrawList_PathBezierQuadraticCurveTo(this, p2, p3, num_segments); }
inline void ImDrawList::PathRect(const ImVec2& rect_min, const ImVec2& rect_max, float rounding, ImDrawFlags flags) { imgui_function_table_instance()->ImDrawList_PathRect(this, rect_min, rect_max, rounding, flags); }
inline void ImDrawList::AddCallback(ImDrawCallback callback, void* callback_data) { imgui_function_table_instance()->ImDrawList_AddCallback(this, callback, callback_data); }
inline void ImDrawList::AddDrawCmd() { imgui_function_table_instance()->ImDrawList_AddDrawCmd(this); }
inline ImDrawList* ImDrawList::CloneOutput() const { return imgui_function_table_instance()->ImDrawList_CloneOutput(this); }
inline void ImDrawList::PrimReserve(int idx_count, int vtx_count) { imgui_function_table_instance()->ImDrawList_PrimReserve(this, idx_count, vtx_count); }
inline void ImDrawList::PrimUnreserve(int idx_count, int vtx_count) { imgui_function_table_instance()->ImDrawList_PrimUnreserve(this, idx_count, vtx_count); }
inline void ImDrawList::PrimRect(const ImVec2& a, const ImVec2& b, ImU32 col) { imgui_function_table_instance()->ImDrawList_PrimRect(this, a, b, col); }
inline void ImDrawList::PrimRectUV(const ImVec2& a, const ImVec2& b, const ImVec2& uv_a, const ImVec2& uv_b, ImU32 col) { imgui_function_table_instance()->ImDrawList_PrimRectUV(this, a, b, uv_a, uv_b, col); }
inline void ImDrawList::PrimQuadUV(const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d, const ImVec2& uv_a, const ImVec2& uv_b, const ImVec2& uv_c, const ImVec2& uv_d, ImU32 col) { imgui_function_table_instance()->ImDrawList_PrimQuadUV(this, a, b, c, d, uv_a, uv_b, uv_c, uv_d, col); }
inline ImFont::ImFont() { imgui_function_table_instance()->ConstructImFont(this); }
inline ImFont::~ImFont() { imgui_function_table_instance()->DestructImFont(this); }
inline const ImFontGlyph* ImFont::FindGlyph(ImWchar c) const { return imgui_function_table_instance()->ImFont_FindGlyph(this, c); }
inline const ImFontGlyph* ImFont::FindGlyphNoFallback(ImWchar c) const { return imgui_function_table_instance()->ImFont_FindGlyphNoFallback(this, c); }
inline ImVec2 ImFont::CalcTextSizeA(float size, float max_width, float wrap_width, const char* text_begin, const char* text_end, const char** remaining) const { return imgui_function_table_instance()->ImFont_CalcTextSizeA(this, size, max_width, wrap_width, text_begin, text_end, remaining); }
inline const char* ImFont::CalcWordWrapPositionA(float scale, const char* text, const char* text_end, float wrap_width) const { return imgui_function_table_instance()->ImFont_CalcWordWrapPositionA(this, scale, text, text_end, wrap_width); }
inline void ImFont::RenderChar(ImDrawList* draw_list, float size, ImVec2 pos, ImU32 col, ImWchar c) const { imgui_function_table_instance()->ImFont_RenderChar(this, draw_list, size, pos, col, c); }
inline void ImFont::RenderText(ImDrawList* draw_list, float size, ImVec2 pos, ImU32 col, const ImVec4& clip_rect, const char* text_begin, const char* text_end, float wrap_width, bool cpu_fine_clip) const { imgui_function_table_instance()->ImFont_RenderText(this, draw_list, size, pos, col, clip_rect, text_begin, text_end, wrap_width, cpu_fine_clip); }


#endif

#endif
