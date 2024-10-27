/*
 * Copyright (C) 2021 Patrick Mours
 * Copyright (C) 2014-2024 Omar Cornut
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON

#include <new>
#include "imgui_function_table_19040.hpp"

namespace
{
	// Convert 'ImGuiIO' as well due to 'IMGUI_DISABLE_OBSOLETE_KEYIO' not being defined by default
	void convert(const ImGuiIO &new_io, imgui_io_19040 &io)
	{
		io.ConfigFlags = new_io.ConfigFlags;
		io.BackendFlags = new_io.BackendFlags;
		io.DisplaySize = new_io.DisplaySize;
		io.DeltaTime = new_io.DeltaTime;
		io.IniSavingRate = new_io.IniSavingRate;
		io.IniFilename = new_io.IniFilename;
		io.LogFilename = new_io.LogFilename;
		io.UserData = new_io.UserData;

		// It's not safe to access the internal fields of 'FontAtlas'
		io.Fonts = new_io.Fonts;
		io.FontGlobalScale = new_io.FontGlobalScale;
		io.FontAllowUserScaling = new_io.FontAllowUserScaling;
		io.FontDefault = nullptr;
		io.DisplayFramebufferScale = new_io.DisplayFramebufferScale;

		io.ConfigDockingNoSplit = new_io.ConfigDockingNoSplit;
		io.ConfigDockingWithShift = new_io.ConfigDockingWithShift;
		io.ConfigDockingAlwaysTabBar = new_io.ConfigDockingAlwaysTabBar;
		io.ConfigDockingTransparentPayload = new_io.ConfigDockingTransparentPayload;

		io.ConfigViewportsNoAutoMerge = new_io.ConfigViewportsNoAutoMerge;
		io.ConfigViewportsNoTaskBarIcon = new_io.ConfigViewportsNoTaskBarIcon;
		io.ConfigViewportsNoDecoration = new_io.ConfigViewportsNoDecoration;
		io.ConfigViewportsNoDefaultParent = new_io.ConfigViewportsNoDefaultParent;

		io.MouseDrawCursor = new_io.MouseDrawCursor;
		io.ConfigMacOSXBehaviors = new_io.ConfigMacOSXBehaviors;
		io.ConfigInputTrickleEventQueue = new_io.ConfigInputTrickleEventQueue;
		io.ConfigInputTextCursorBlink = new_io.ConfigInputTextCursorBlink;
		io.ConfigInputTextEnterKeepActive = new_io.ConfigInputTextEnterKeepActive;
		io.ConfigDragClickToInputText = new_io.ConfigDragClickToInputText;
		io.ConfigWindowsResizeFromEdges = new_io.ConfigWindowsResizeFromEdges;
		io.ConfigWindowsMoveFromTitleBarOnly = new_io.ConfigWindowsMoveFromTitleBarOnly;
		io.ConfigMemoryCompactTimer = new_io.ConfigMemoryCompactTimer;

		io.MouseDoubleClickTime = new_io.MouseDoubleClickTime;
		io.MouseDoubleClickMaxDist = new_io.MouseDoubleClickMaxDist;
		io.MouseDragThreshold = new_io.MouseDragThreshold;
		io.KeyRepeatDelay = new_io.KeyRepeatDelay;
		io.KeyRepeatRate = new_io.KeyRepeatRate;

		io.ConfigDebugIsDebuggerPresent = new_io.ConfigDebugIsDebuggerPresent;
		io.ConfigDebugBeginReturnValueOnce = new_io.ConfigDebugBeginReturnValueOnce;
		io.ConfigDebugBeginReturnValueLoop = new_io.ConfigDebugBeginReturnValueLoop;
		io.ConfigDebugIgnoreFocusLoss = new_io.ConfigDebugIgnoreFocusLoss;
		io.ConfigDebugIniSettings = new_io.ConfigDebugIniSettings;

		io.BackendPlatformName = new_io.BackendPlatformName;
		io.BackendRendererName = new_io.BackendRendererName;
		io.BackendPlatformUserData = new_io.BackendPlatformUserData;
		io.BackendRendererUserData = new_io.BackendRendererUserData;
		io.BackendLanguageUserData = new_io.BackendLanguageUserData;

		io.GetClipboardTextFn = new_io.GetClipboardTextFn;
		io.SetClipboardTextFn = new_io.SetClipboardTextFn;
		io.ClipboardUserData = new_io.ClipboardUserData;
		io.SetPlatformImeDataFn = new_io.SetPlatformImeDataFn;
		io.PlatformLocaleDecimalPoint = new_io.PlatformLocaleDecimalPoint;

		io.WantCaptureMouse = new_io.WantCaptureMouse;
		io.WantCaptureKeyboard = new_io.WantCaptureKeyboard;
		io.WantTextInput = new_io.WantTextInput;
		io.WantSetMousePos = new_io.WantSetMousePos;
		io.WantSaveIniSettings = new_io.WantSaveIniSettings;
		io.NavActive = new_io.NavActive;
		io.NavVisible = new_io.NavVisible;
		io.Framerate = new_io.Framerate;
		io.MetricsRenderVertices = new_io.MetricsRenderVertices;
		io.MetricsRenderIndices = new_io.MetricsRenderIndices;
		io.MetricsRenderWindows = new_io.MetricsRenderWindows;
		io.MetricsActiveWindows = new_io.MetricsActiveWindows;
		io.MouseDelta = new_io.MouseDelta;

		for (int i = 0; i < 666; ++i)
			io.KeyMap[i] = 0;
		for (int i = 0; i < 666; ++i)
			io.KeysDown[i] = false; // ImGui::IsKeyDown(i)
		for (int i = 0; i < 16; ++i)
			io.NavInputs[i] = 0.0f;

		io.Ctx = new_io.Ctx;

		io.MousePos = new_io.MousePos;
		for (int i = 0; i < 5; ++i)
			io.MouseDown[i] = new_io.MouseDown[i];
		io.MouseWheel = new_io.MouseWheel;
		io.MouseWheelH = new_io.MouseWheelH;
		io.MouseSource = new_io.MouseSource;
		io.MouseHoveredViewport = new_io.MouseHoveredViewport;
		io.KeyCtrl = new_io.KeyCtrl;
		io.KeyShift = new_io.KeyShift;
		io.KeyAlt = new_io.KeyAlt;
		io.KeySuper = new_io.KeySuper;
	}
}

const imgui_function_table_19040 init_imgui_function_table_19040() { return {
	[]() -> imgui_io_19040 &{
		static imgui_io_19040 io = {};
		convert(ImGui::GetIO(), io);
		return io;
	},
	ImGui::GetStyle,
	ImGui::GetVersion,
	ImGui::Begin,
	ImGui::End,
	ImGui::BeginChild,
	ImGui::BeginChild,
	ImGui::EndChild,
	ImGui::IsWindowAppearing,
	ImGui::IsWindowCollapsed,
	ImGui::IsWindowFocused,
	ImGui::IsWindowHovered,
	ImGui::GetWindowDrawList,
	ImGui::GetWindowDpiScale,
	ImGui::GetWindowPos,
	ImGui::GetWindowSize,
	ImGui::GetWindowWidth,
	ImGui::GetWindowHeight,
	ImGui::SetNextWindowPos,
	ImGui::SetNextWindowSize,
	ImGui::SetNextWindowSizeConstraints,
	ImGui::SetNextWindowContentSize,
	ImGui::SetNextWindowCollapsed,
	ImGui::SetNextWindowFocus,
	ImGui::SetNextWindowScroll,
	ImGui::SetNextWindowBgAlpha,
	ImGui::SetWindowPos,
	ImGui::SetWindowSize,
	ImGui::SetWindowCollapsed,
	ImGui::SetWindowFocus,
	ImGui::SetWindowFontScale,
	ImGui::SetWindowPos,
	ImGui::SetWindowSize,
	ImGui::SetWindowCollapsed,
	ImGui::SetWindowFocus,
	ImGui::GetContentRegionAvail,
	ImGui::GetContentRegionMax,
	ImGui::GetWindowContentRegionMin,
	ImGui::GetWindowContentRegionMax,
	ImGui::GetScrollX,
	ImGui::GetScrollY,
	ImGui::SetScrollX,
	ImGui::SetScrollY,
	ImGui::GetScrollMaxX,
	ImGui::GetScrollMaxY,
	ImGui::SetScrollHereX,
	ImGui::SetScrollHereY,
	ImGui::SetScrollFromPosX,
	ImGui::SetScrollFromPosY,
	ImGui::PushFont,
	ImGui::PopFont,
	ImGui::PushStyleColor,
	ImGui::PushStyleColor,
	ImGui::PopStyleColor,
	ImGui::PushStyleVar,
	ImGui::PushStyleVar,
	ImGui::PopStyleVar,
	ImGui::PushTabStop,
	ImGui::PopTabStop,
	ImGui::PushButtonRepeat,
	ImGui::PopButtonRepeat,
	ImGui::PushItemWidth,
	ImGui::PopItemWidth,
	ImGui::SetNextItemWidth,
	ImGui::CalcItemWidth,
	ImGui::PushTextWrapPos,
	ImGui::PopTextWrapPos,
	ImGui::GetFont,
	ImGui::GetFontSize,
	ImGui::GetFontTexUvWhitePixel,
	ImGui::GetColorU32,
	ImGui::GetColorU32,
	ImGui::GetColorU32,
	ImGui::GetStyleColorVec4,
	ImGui::GetCursorScreenPos,
	ImGui::SetCursorScreenPos,
	ImGui::GetCursorPos,
	ImGui::GetCursorPosX,
	ImGui::GetCursorPosY,
	ImGui::SetCursorPos,
	ImGui::SetCursorPosX,
	ImGui::SetCursorPosY,
	ImGui::GetCursorStartPos,
	ImGui::Separator,
	ImGui::SameLine,
	ImGui::NewLine,
	ImGui::Spacing,
	ImGui::Dummy,
	ImGui::Indent,
	ImGui::Unindent,
	ImGui::BeginGroup,
	ImGui::EndGroup,
	ImGui::AlignTextToFramePadding,
	ImGui::GetTextLineHeight,
	ImGui::GetTextLineHeightWithSpacing,
	ImGui::GetFrameHeight,
	ImGui::GetFrameHeightWithSpacing,
	ImGui::PushID,
	ImGui::PushID,
	ImGui::PushID,
	ImGui::PushID,
	ImGui::PopID,
	ImGui::GetID,
	ImGui::GetID,
	ImGui::GetID,
	ImGui::TextUnformatted,
	ImGui::TextV,
	ImGui::TextColoredV,
	ImGui::TextDisabledV,
	ImGui::TextWrappedV,
	ImGui::LabelTextV,
	ImGui::BulletTextV,
	ImGui::SeparatorText,
	ImGui::Button,
	ImGui::SmallButton,
	ImGui::InvisibleButton,
	ImGui::ArrowButton,
	ImGui::Checkbox,
	ImGui::CheckboxFlags,
	ImGui::CheckboxFlags,
	ImGui::RadioButton,
	ImGui::RadioButton,
	ImGui::ProgressBar,
	ImGui::Bullet,
	ImGui::Image,
	ImGui::ImageButton,
	ImGui::BeginCombo,
	ImGui::EndCombo,
	ImGui::Combo,
	ImGui::Combo,
	ImGui::Combo,
	ImGui::DragFloat,
	ImGui::DragFloat2,
	ImGui::DragFloat3,
	ImGui::DragFloat4,
	ImGui::DragFloatRange2,
	ImGui::DragInt,
	ImGui::DragInt2,
	ImGui::DragInt3,
	ImGui::DragInt4,
	ImGui::DragIntRange2,
	ImGui::DragScalar,
	ImGui::DragScalarN,
	ImGui::SliderFloat,
	ImGui::SliderFloat2,
	ImGui::SliderFloat3,
	ImGui::SliderFloat4,
	ImGui::SliderAngle,
	ImGui::SliderInt,
	ImGui::SliderInt2,
	ImGui::SliderInt3,
	ImGui::SliderInt4,
	ImGui::SliderScalar,
	ImGui::SliderScalarN,
	ImGui::VSliderFloat,
	ImGui::VSliderInt,
	ImGui::VSliderScalar,
	ImGui::InputText,
	ImGui::InputTextMultiline,
	ImGui::InputTextWithHint,
	ImGui::InputFloat,
	ImGui::InputFloat2,
	ImGui::InputFloat3,
	ImGui::InputFloat4,
	ImGui::InputInt,
	ImGui::InputInt2,
	ImGui::InputInt3,
	ImGui::InputInt4,
	ImGui::InputDouble,
	ImGui::InputScalar,
	ImGui::InputScalarN,
	ImGui::ColorEdit3,
	ImGui::ColorEdit4,
	ImGui::ColorPicker3,
	ImGui::ColorPicker4,
	ImGui::ColorButton,
	ImGui::SetColorEditOptions,
	ImGui::TreeNode,
	ImGui::TreeNodeV,
	ImGui::TreeNodeV,
	ImGui::TreeNodeEx,
	ImGui::TreeNodeExV,
	ImGui::TreeNodeExV,
	ImGui::TreePush,
	ImGui::TreePush,
	ImGui::TreePop,
	ImGui::GetTreeNodeToLabelSpacing,
	ImGui::CollapsingHeader,
	ImGui::CollapsingHeader,
	ImGui::SetNextItemOpen,
	ImGui::Selectable,
	ImGui::Selectable,
	ImGui::BeginListBox,
	ImGui::EndListBox,
	ImGui::ListBox,
	ImGui::ListBox,
	ImGui::PlotLines,
	ImGui::PlotLines,
	ImGui::PlotHistogram,
	ImGui::PlotHistogram,
	ImGui::Value,
	ImGui::Value,
	ImGui::Value,
	ImGui::Value,
	ImGui::BeginMenuBar,
	ImGui::EndMenuBar,
	ImGui::BeginMainMenuBar,
	ImGui::EndMainMenuBar,
	ImGui::BeginMenu,
	ImGui::EndMenu,
	ImGui::MenuItem,
	ImGui::MenuItem,
	ImGui::BeginTooltip,
	ImGui::EndTooltip,
	ImGui::SetTooltipV,
	ImGui::BeginItemTooltip,
	ImGui::SetItemTooltipV,
	ImGui::BeginPopup,
	ImGui::BeginPopupModal,
	ImGui::EndPopup,
	ImGui::OpenPopup,
	ImGui::OpenPopup,
	ImGui::OpenPopupOnItemClick,
	ImGui::CloseCurrentPopup,
	ImGui::BeginPopupContextItem,
	ImGui::BeginPopupContextWindow,
	ImGui::BeginPopupContextVoid,
	ImGui::IsPopupOpen,
	ImGui::BeginTable,
	ImGui::EndTable,
	ImGui::TableNextRow,
	ImGui::TableNextColumn,
	ImGui::TableSetColumnIndex,
	ImGui::TableSetupColumn,
	ImGui::TableSetupScrollFreeze,
	ImGui::TableHeader,
	ImGui::TableHeadersRow,
	ImGui::TableAngledHeadersRow,
	ImGui::TableGetSortSpecs,
	ImGui::TableGetColumnCount,
	ImGui::TableGetColumnIndex,
	ImGui::TableGetRowIndex,
	ImGui::TableGetColumnName,
	ImGui::TableGetColumnFlags,
	ImGui::TableSetColumnEnabled,
	ImGui::TableSetBgColor,
	ImGui::Columns,
	ImGui::NextColumn,
	ImGui::GetColumnIndex,
	ImGui::GetColumnWidth,
	ImGui::SetColumnWidth,
	ImGui::GetColumnOffset,
	ImGui::SetColumnOffset,
	ImGui::GetColumnsCount,
	ImGui::BeginTabBar,
	ImGui::EndTabBar,
	ImGui::BeginTabItem,
	ImGui::EndTabItem,
	ImGui::TabItemButton,
	ImGui::SetTabItemClosed,
	ImGui::DockSpace,
	ImGui::SetNextWindowDockID,
	ImGui::SetNextWindowClass,
	ImGui::GetWindowDockID,
	ImGui::IsWindowDocked,
	ImGui::BeginDragDropSource,
	ImGui::SetDragDropPayload,
	ImGui::EndDragDropSource,
	ImGui::BeginDragDropTarget,
	ImGui::AcceptDragDropPayload,
	ImGui::EndDragDropTarget,
	ImGui::GetDragDropPayload,
	ImGui::BeginDisabled,
	ImGui::EndDisabled,
	ImGui::PushClipRect,
	ImGui::PopClipRect,
	ImGui::SetItemDefaultFocus,
	ImGui::SetKeyboardFocusHere,
	ImGui::SetNextItemAllowOverlap,
	ImGui::IsItemHovered,
	ImGui::IsItemActive,
	ImGui::IsItemFocused,
	ImGui::IsItemClicked,
	ImGui::IsItemVisible,
	ImGui::IsItemEdited,
	ImGui::IsItemActivated,
	ImGui::IsItemDeactivated,
	ImGui::IsItemDeactivatedAfterEdit,
	ImGui::IsItemToggledOpen,
	ImGui::IsAnyItemHovered,
	ImGui::IsAnyItemActive,
	ImGui::IsAnyItemFocused,
	ImGui::GetItemID,
	ImGui::GetItemRectMin,
	ImGui::GetItemRectMax,
	ImGui::GetItemRectSize,
	ImGui::GetBackgroundDrawList,
	ImGui::GetForegroundDrawList,
	ImGui::GetBackgroundDrawList,
	ImGui::GetForegroundDrawList,
	ImGui::IsRectVisible,
	ImGui::IsRectVisible,
	ImGui::GetTime,
	ImGui::GetFrameCount,
	ImGui::GetDrawListSharedData,
	ImGui::GetStyleColorName,
	ImGui::SetStateStorage,
	ImGui::GetStateStorage,
	ImGui::CalcTextSize,
	ImGui::ColorConvertU32ToFloat4,
	ImGui::ColorConvertFloat4ToU32,
	ImGui::ColorConvertRGBtoHSV,
	ImGui::ColorConvertHSVtoRGB,
	ImGui::IsKeyDown,
	ImGui::IsKeyPressed,
	ImGui::IsKeyReleased,
	ImGui::IsKeyChordPressed,
	ImGui::GetKeyPressedAmount,
	ImGui::GetKeyName,
	ImGui::SetNextFrameWantCaptureKeyboard,
	ImGui::IsMouseDown,
	ImGui::IsMouseClicked,
	ImGui::IsMouseReleased,
	ImGui::IsMouseDoubleClicked,
	ImGui::GetMouseClickedCount,
	ImGui::IsMouseHoveringRect,
	ImGui::IsMousePosValid,
	ImGui::IsAnyMouseDown,
	ImGui::GetMousePos,
	ImGui::GetMousePosOnOpeningCurrentPopup,
	ImGui::IsMouseDragging,
	ImGui::GetMouseDragDelta,
	ImGui::ResetMouseDragDelta,
	ImGui::GetMouseCursor,
	ImGui::SetMouseCursor,
	ImGui::SetNextFrameWantCaptureMouse,
	ImGui::GetClipboardText,
	ImGui::SetClipboardText,
	ImGui::SetAllocatorFunctions,
	ImGui::GetAllocatorFunctions,
	ImGui::MemAlloc,
	ImGui::MemFree,
	[](const ImGuiStorage *_this, ImGuiID key, int default_val) -> int { return _this->GetInt(key, default_val); },
	[](ImGuiStorage *_this, ImGuiID key, int val) -> void { _this->SetInt(key, val); },
	[](const ImGuiStorage *_this, ImGuiID key, bool default_val) -> bool { return _this->GetBool(key, default_val); },
	[](ImGuiStorage *_this, ImGuiID key, bool val) -> void { _this->SetBool(key, val); },
	[](const ImGuiStorage *_this, ImGuiID key, float default_val) -> float { return _this->GetFloat(key, default_val); },
	[](ImGuiStorage *_this, ImGuiID key, float val) -> void { _this->SetFloat(key, val); },
	[](const ImGuiStorage *_this, ImGuiID key) -> void* { return _this->GetVoidPtr(key); },
	[](ImGuiStorage *_this, ImGuiID key, void* val) -> void { _this->SetVoidPtr(key, val); },
	[](ImGuiStorage *_this, ImGuiID key, int default_val) -> int* { return _this->GetIntRef(key, default_val); },
	[](ImGuiStorage *_this, ImGuiID key, bool default_val) -> bool* { return _this->GetBoolRef(key, default_val); },
	[](ImGuiStorage *_this, ImGuiID key, float default_val) -> float* { return _this->GetFloatRef(key, default_val); },
	[](ImGuiStorage *_this, ImGuiID key, void* default_val) -> void** { return _this->GetVoidPtrRef(key, default_val); },
	[](ImGuiStorage *_this) -> void { _this->BuildSortByKey(); },
	[](ImGuiStorage *_this, int val) -> void { _this->SetAllInt(val); },
	[](ImGuiListClipper *_this) -> void { new(_this) ImGuiListClipper(); },
	[](ImGuiListClipper *_this) -> void { _this->~ImGuiListClipper(); },
	[](ImGuiListClipper *_this, int items_count, float items_height) -> void { _this->Begin(items_count, items_height); },
	[](ImGuiListClipper *_this) -> void { _this->End(); },
	[](ImGuiListClipper *_this) -> bool { return _this->Step(); },
	[](ImGuiListClipper *_this, int item_begin, int item_end) -> void { _this->IncludeItemsByIndex(item_begin, item_end); },
	[](ImDrawList *_this, const ImVec2& clip_rect_min, const ImVec2& clip_rect_max, bool intersect_with_current_clip_rect) -> void { _this->PushClipRect(clip_rect_min, clip_rect_max, intersect_with_current_clip_rect); },
	[](ImDrawList *_this) -> void { _this->PushClipRectFullScreen(); },
	[](ImDrawList *_this) -> void { _this->PopClipRect(); },
	[](ImDrawList *_this, ImTextureID texture_id) -> void { _this->PushTextureID(texture_id); },
	[](ImDrawList *_this) -> void { _this->PopTextureID(); },
	[](ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness) -> void { _this->AddLine(p1, p2, col, thickness); },
	[](ImDrawList *_this, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness) -> void { _this->AddRect(p_min, p_max, col, rounding, flags, thickness); },
	[](ImDrawList *_this, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags) -> void { _this->AddRectFilled(p_min, p_max, col, rounding, flags); },
	[](ImDrawList *_this, const ImVec2& p_min, const ImVec2& p_max, ImU32 col_upr_left, ImU32 col_upr_right, ImU32 col_bot_right, ImU32 col_bot_left) -> void { _this->AddRectFilledMultiColor(p_min, p_max, col_upr_left, col_upr_right, col_bot_right, col_bot_left); },
	[](ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness) -> void { _this->AddQuad(p1, p2, p3, p4, col, thickness); },
	[](ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col) -> void { _this->AddQuadFilled(p1, p2, p3, p4, col); },
	[](ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness) -> void { _this->AddTriangle(p1, p2, p3, col, thickness); },
	[](ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col) -> void { _this->AddTriangleFilled(p1, p2, p3, col); },
	[](ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) -> void { _this->AddCircle(center, radius, col, num_segments, thickness); },
	[](ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments) -> void { _this->AddCircleFilled(center, radius, col, num_segments); },
	[](ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) -> void { _this->AddNgon(center, radius, col, num_segments, thickness); },
	[](ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments) -> void { _this->AddNgonFilled(center, radius, col, num_segments); },
	[](ImDrawList *_this, const ImVec2& center, float radius_x, float radius_y, ImU32 col, float rot, int num_segments, float thickness) -> void { _this->AddEllipse(center, radius_x, radius_y, col, rot, num_segments, thickness); },
	[](ImDrawList *_this, const ImVec2& center, float radius_x, float radius_y, ImU32 col, float rot, int num_segments) -> void { _this->AddEllipseFilled(center, radius_x, radius_y, col, rot, num_segments); },
	[](ImDrawList *_this, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end) -> void { _this->AddText(pos, col, text_begin, text_end); },
	[](ImDrawList *_this, const ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect) -> void { _this->AddText(font, font_size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip_rect); },
	[](ImDrawList *_this, const ImVec2* points, int num_points, ImU32 col, ImDrawFlags flags, float thickness) -> void { _this->AddPolyline(points, num_points, col, flags, thickness); },
	[](ImDrawList *_this, const ImVec2* points, int num_points, ImU32 col) -> void { _this->AddConvexPolyFilled(points, num_points, col); },
	[](ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness, int num_segments) -> void { _this->AddBezierCubic(p1, p2, p3, p4, col, thickness, num_segments); },
	[](ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness, int num_segments) -> void { _this->AddBezierQuadratic(p1, p2, p3, col, thickness, num_segments); },
	[](ImDrawList *_this, ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col) -> void { _this->AddImage(user_texture_id, p_min, p_max, uv_min, uv_max, col); },
	[](ImDrawList *_this, ImTextureID user_texture_id, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3, const ImVec2& uv4, ImU32 col) -> void { _this->AddImageQuad(user_texture_id, p1, p2, p3, p4, uv1, uv2, uv3, uv4, col); },
	[](ImDrawList *_this, ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col, float rounding, ImDrawFlags flags) -> void { _this->AddImageRounded(user_texture_id, p_min, p_max, uv_min, uv_max, col, rounding, flags); },
	[](ImDrawList *_this, const ImVec2& center, float radius, float a_min, float a_max, int num_segments) -> void { _this->PathArcTo(center, radius, a_min, a_max, num_segments); },
	[](ImDrawList *_this, const ImVec2& center, float radius, int a_min_of_12, int a_max_of_12) -> void { _this->PathArcToFast(center, radius, a_min_of_12, a_max_of_12); },
	[](ImDrawList *_this, const ImVec2& center, float radius_x, float radius_y, float rot, float a_min, float a_max, int num_segments) -> void { _this->PathEllipticalArcTo(center, radius_x, radius_y, rot, a_min, a_max, num_segments); },
	[](ImDrawList *_this, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, int num_segments) -> void { _this->PathBezierCubicCurveTo(p2, p3, p4, num_segments); },
	[](ImDrawList *_this, const ImVec2& p2, const ImVec2& p3, int num_segments) -> void { _this->PathBezierQuadraticCurveTo(p2, p3, num_segments); },
	[](ImDrawList *_this, const ImVec2& rect_min, const ImVec2& rect_max, float rounding, ImDrawFlags flags) -> void { _this->PathRect(rect_min, rect_max, rounding, flags); },
	[](ImDrawList *_this, ImDrawCallback callback, void* callback_data) -> void { _this->AddCallback(callback, callback_data); },
	[](ImDrawList *_this) -> void { _this->AddDrawCmd(); },
	[](const ImDrawList *_this) -> ImDrawList* { return _this->CloneOutput(); },
	[](ImDrawList *_this, int idx_count, int vtx_count) -> void { _this->PrimReserve(idx_count, vtx_count); },
	[](ImDrawList *_this, int idx_count, int vtx_count) -> void { _this->PrimUnreserve(idx_count, vtx_count); },
	[](ImDrawList *_this, const ImVec2& a, const ImVec2& b, ImU32 col) -> void { _this->PrimRect(a, b, col); },
	[](ImDrawList *_this, const ImVec2& a, const ImVec2& b, const ImVec2& uv_a, const ImVec2& uv_b, ImU32 col) -> void { _this->PrimRectUV(a, b, uv_a, uv_b, col); },
	[](ImDrawList *_this, const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d, const ImVec2& uv_a, const ImVec2& uv_b, const ImVec2& uv_c, const ImVec2& uv_d, ImU32 col) -> void { _this->PrimQuadUV(a, b, c, d, uv_a, uv_b, uv_c, uv_d, col); },
	[](ImFont *_this) -> void { new(_this) ImFont(); },
	[](ImFont *_this) -> void { _this->~ImFont(); },
	[](const ImFont *_this, ImWchar c) -> const ImFontGlyph* { return _this->FindGlyph(c); },
	[](const ImFont *_this, ImWchar c) -> const ImFontGlyph* { return _this->FindGlyphNoFallback(c); },
	[](const ImFont *_this, float size, float max_width, float wrap_width, const char* text_begin, const char* text_end, const char** remaining) -> ImVec2 { return _this->CalcTextSizeA(size, max_width, wrap_width, text_begin, text_end, remaining); },
	[](const ImFont *_this, float scale, const char* text, const char* text_end, float wrap_width) -> const char* { return _this->CalcWordWrapPositionA(scale, text, text_end, wrap_width); },
	[](const ImFont *_this, ImDrawList* draw_list, float size, const ImVec2& pos, ImU32 col, ImWchar c) -> void { _this->RenderChar(draw_list, size, pos, col, c); },
	[](const ImFont *_this, ImDrawList* draw_list, float size, const ImVec2& pos, ImU32 col, const ImVec4& clip_rect, const char* text_begin, const char* text_end, float wrap_width, bool cpu_fine_clip) -> void { _this->RenderText(draw_list, size, pos, col, clip_rect, text_begin, text_end, wrap_width, cpu_fine_clip); },

}; }

#endif
