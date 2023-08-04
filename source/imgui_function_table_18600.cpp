/*
 * Copyright (C) 2021 Patrick Mours
 * Copyright (C) 2014-2022 Omar Cornut
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON

#include <new>
#include <imgui.h>
#include "imgui_function_table_18600.hpp"

void imgui_font_18600::convert(const ImFont &new_font, imgui_font_18600 &font)
{
	font.IndexAdvanceX = new_font.IndexAdvanceX;
	font.FallbackAdvanceX = new_font.FallbackAdvanceX;
	font.FontSize = new_font.FontSize;
	font.IndexLookup = new_font.IndexLookup;
	font.Glyphs = new_font.Glyphs;
	font.FallbackGlyph = new_font.FallbackGlyph;
	font.ContainerAtlas = new_font.ContainerAtlas;
	font.ConfigData = new_font.ConfigData;
	font.ConfigDataCount = new_font.ConfigDataCount;
	font.FallbackChar = new_font.FallbackChar;
	font.EllipsisChar = new_font.EllipsisChar;
	font.DirtyLookupTables = new_font.DirtyLookupTables;
	font.Scale = new_font.Scale;
	font.Ascent = new_font.Ascent;
	font.Descent = new_font.Descent;
	font.MetricsTotalSurface = new_font.MetricsTotalSurface;
	memcpy(font.Used4kPagesMap, new_font.Used4kPagesMap, sizeof(font.Used4kPagesMap));
}
void imgui_font_18600::convert(const imgui_font_18600 &font, ImFont &new_font)
{
	new_font.IndexAdvanceX = font.IndexAdvanceX;
	new_font.FallbackAdvanceX = font.FallbackAdvanceX;
	new_font.FontSize = font.FontSize;
	new_font.IndexLookup = font.IndexLookup;
	new_font.Glyphs = font.Glyphs;
	new_font.FallbackGlyph = font.FallbackGlyph;
	new_font.ContainerAtlas = font.ContainerAtlas;
	new_font.ConfigData = font.ConfigData;
	new_font.ConfigDataCount = font.ConfigDataCount;
	new_font.FallbackChar = font.FallbackChar;
	new_font.EllipsisChar = font.EllipsisChar;
	new_font.EllipsisCharCount = 1;
	new_font.EllipsisWidth = 4;
	new_font.EllipsisCharStep = 4;
	new_font.DirtyLookupTables = font.DirtyLookupTables;
	new_font.Scale = font.Scale;
	new_font.Ascent = font.Ascent;
	new_font.Descent = font.Descent;
	new_font.MetricsTotalSurface = font.MetricsTotalSurface;
	memcpy(new_font.Used4kPagesMap, font.Used4kPagesMap, sizeof(new_font.Used4kPagesMap));
}

void imgui_io_18600::convert(const ImGuiIO &new_io, imgui_io_18600 &io)
{
	io.ConfigFlags = new_io.ConfigFlags;
	io.BackendFlags = new_io.BackendFlags;
	io.DisplaySize = new_io.DisplaySize;
	io.DeltaTime = new_io.DeltaTime;
	io.IniSavingRate = new_io.IniSavingRate;
	io.IniFilename = new_io.IniFilename;
	io.LogFilename = new_io.LogFilename;
	io.MouseDoubleClickTime = new_io.MouseDoubleClickTime;
	io.MouseDoubleClickMaxDist = new_io.MouseDoubleClickMaxDist;
	io.MouseDragThreshold = new_io.MouseDragThreshold;
	for (int i = 0; i < 22; ++i)
		io.KeyMap[i] = new_io.KeyMap[i];
	io.KeyRepeatDelay = new_io.KeyRepeatDelay;
	io.KeyRepeatRate = new_io.KeyRepeatRate;
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
	io.ConfigInputTextCursorBlink = new_io.ConfigInputTextCursorBlink;
	io.ConfigDragClickToInputText = new_io.ConfigDragClickToInputText;
	io.ConfigWindowsResizeFromEdges = new_io.ConfigWindowsResizeFromEdges;
	io.ConfigWindowsMoveFromTitleBarOnly = new_io.ConfigWindowsMoveFromTitleBarOnly;
	io.ConfigMemoryCompactTimer = new_io.ConfigMemoryCompactTimer;

	io.BackendPlatformName = new_io.BackendPlatformName;
	io.BackendRendererName = new_io.BackendRendererName;
	io.BackendPlatformUserData = new_io.BackendPlatformUserData;
	io.BackendRendererUserData = new_io.BackendRendererUserData;
	io.BackendLanguageUserData = new_io.BackendLanguageUserData;

	io.GetClipboardTextFn = new_io.GetClipboardTextFn;
	io.SetClipboardTextFn = new_io.SetClipboardTextFn;
	io.ClipboardUserData = new_io.ClipboardUserData;

	io.MousePos = new_io.MousePos;
	for (int i = 0; i < 5; ++i)
		io.MouseDown[i] = new_io.MouseDown[i];
	io.MouseWheel = new_io.MouseWheel;
	io.MouseWheelH = new_io.MouseWheelH;
	io.MouseHoveredViewport = new_io.MouseHoveredViewport;
	io.KeyCtrl = new_io.KeyCtrl;
	io.KeyShift = new_io.KeyShift;
	io.KeyAlt = new_io.KeyAlt;
	io.KeySuper = new_io.KeySuper;
	for (int i = 0; i < 512; ++i)
		io.KeysDown[i] = new_io.KeysDown[i];
	for (int i = 0; i < (20 < ImGuiNavInput_COUNT ? 20 : ImGuiNavInput_COUNT); ++i)
		io.NavInputs[i] = new_io.NavInputs[i];

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
	io.MetricsActiveAllocations = new_io.MetricsActiveAllocations;
	io.MouseDelta = new_io.MouseDelta;
}

void imgui_style_18600::convert(const ImGuiStyle &new_style, imgui_style_18600 &style)
{
	style.Alpha = new_style.Alpha;
	style.DisabledAlpha = new_style.DisabledAlpha;
	style.WindowPadding = new_style.WindowPadding;
	style.WindowRounding = new_style.WindowRounding;
	style.WindowBorderSize = new_style.WindowBorderSize;
	style.WindowMinSize = new_style.WindowMinSize;
	style.WindowTitleAlign = new_style.WindowTitleAlign;
	style.WindowMenuButtonPosition = new_style.WindowMenuButtonPosition;
	style.ChildRounding = new_style.ChildRounding;
	style.ChildBorderSize = new_style.ChildBorderSize;
	style.PopupRounding = new_style.PopupRounding;
	style.PopupBorderSize = new_style.PopupBorderSize;
	style.FramePadding = new_style.FramePadding;
	style.FrameRounding = new_style.FrameRounding;
	style.FrameBorderSize = new_style.FrameBorderSize;
	style.ItemSpacing = new_style.ItemSpacing;
	style.ItemInnerSpacing = new_style.ItemInnerSpacing;
	style.CellPadding = new_style.CellPadding;
	style.TouchExtraPadding = new_style.TouchExtraPadding;
	style.IndentSpacing = new_style.IndentSpacing;
	style.ColumnsMinSpacing = new_style.ColumnsMinSpacing;
	style.ScrollbarSize = new_style.ScrollbarSize;
	style.ScrollbarRounding = new_style.ScrollbarRounding;
	style.GrabMinSize = new_style.GrabMinSize;
	style.GrabRounding = new_style.GrabRounding;
	style.LogSliderDeadzone = new_style.LogSliderDeadzone;
	style.TabRounding = new_style.TabRounding;
	style.TabBorderSize = new_style.TabBorderSize;
	style.TabMinWidthForCloseButton = new_style.TabMinWidthForCloseButton;
	style.ColorButtonPosition = new_style.ColorButtonPosition;
	style.ButtonTextAlign = new_style.ButtonTextAlign;
	style.SelectableTextAlign = new_style.SelectableTextAlign;
	style.DisplayWindowPadding = new_style.DisplayWindowPadding;
	style.DisplaySafeAreaPadding = new_style.DisplaySafeAreaPadding;
	style.MouseCursorScale = new_style.MouseCursorScale;
	style.AntiAliasedLines = new_style.AntiAliasedLines;
	style.AntiAliasedLinesUseTex = new_style.AntiAliasedLinesUseTex;
	style.AntiAliasedFill = new_style.AntiAliasedFill;
	style.CurveTessellationTol = new_style.CurveTessellationTol;
	style.CircleTessellationMaxError = new_style.CircleTessellationMaxError;
	for (int i = 0; i < 55; ++i)
		style.Colors[i] = new_style.Colors[i];
}

void imgui_list_clipper_18600::convert(const ImGuiListClipper &new_clipper, imgui_list_clipper_18600 &clipper)
{
	clipper.DisplayStart = new_clipper.DisplayStart;
	clipper.DisplayEnd = new_clipper.DisplayEnd;
	clipper.ItemsCount = new_clipper.ItemsCount;
	clipper.ItemsHeight = new_clipper.ItemsHeight;
	clipper.StartPosY = new_clipper.StartPosY;
	clipper.TempData = new_clipper.TempData;
}
void imgui_list_clipper_18600::convert(const imgui_list_clipper_18600 &clipper, ImGuiListClipper &new_clipper)
{
	new_clipper.Ctx = ImGui::GetCurrentContext();
	new_clipper.DisplayStart = clipper.DisplayStart;
	new_clipper.DisplayEnd = clipper.DisplayEnd;
	new_clipper.ItemsCount = clipper.ItemsCount;
	new_clipper.ItemsHeight = clipper.ItemsHeight;
	new_clipper.StartPosY = clipper.StartPosY;
	new_clipper.TempData = clipper.TempData;
}

imgui_function_table_18600 g_imgui_function_table_18600 = {
	[]() -> imgui_io_18600 & {
		static imgui_io_18600 io = {};
		imgui_io_18600::convert(ImGui::GetIO(), io);
		return io;
	},
	[]() -> imgui_style_18600 & {
		static imgui_style_18600 style = {};
		imgui_style_18600::convert(ImGui::GetStyle(), style);
		return style;
	},
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
	[](imgui_font_18600 *) {
		// Cannot make persistent 'ImFont' here easily, so just always use default
		ImGui::PushFont(nullptr);
	},
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
	[]() -> imgui_font_18600 * {
		static imgui_font_18600 font = {};
		imgui_font_18600::convert(*ImGui::GetFont(), font);
		return &font;
	},
	ImGui::GetFontSize,
	ImGui::GetFontTexUvWhitePixel,
	ImGui::GetColorU32,
	ImGui::GetColorU32,
	ImGui::GetColorU32,
	ImGui::GetStyleColorVec4,
	ImGui::Separator,
	ImGui::SameLine,
	ImGui::NewLine,
	ImGui::Spacing,
	ImGui::Dummy,
	ImGui::Indent,
	ImGui::Unindent,
	ImGui::BeginGroup,
	ImGui::EndGroup,
	ImGui::GetCursorPos,
	ImGui::GetCursorPosX,
	ImGui::GetCursorPosY,
	ImGui::SetCursorPos,
	ImGui::SetCursorPosX,
	ImGui::SetCursorPosY,
	ImGui::GetCursorStartPos,
	ImGui::GetCursorScreenPos,
	ImGui::SetCursorScreenPos,
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
	ImGui::Button,
	ImGui::SmallButton,
	ImGui::InvisibleButton,
	ImGui::ArrowButton,
	ImGui::Image,
	[](ImTextureID user_texture_id, const ImVec2 &size, const ImVec2 &uv0, const ImVec2 &uv1, int frame_padding, const ImVec4 &bg_col, const ImVec4 &tint_col) -> bool
	{
		ImGui::PushID(reinterpret_cast<void *>(user_texture_id));
		if (frame_padding >= 0)
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2((float)frame_padding, (float)frame_padding));
		bool ret = ImGui::ImageButton("#image", user_texture_id, size, uv0, uv1, bg_col, tint_col);
		if (frame_padding >= 0)
			ImGui::PopStyleVar();
		ImGui::PopID();
		return ret;
	},
	ImGui::Checkbox,
	ImGui::CheckboxFlags,
	ImGui::CheckboxFlags,
	ImGui::RadioButton,
	ImGui::RadioButton,
	ImGui::ProgressBar,
	ImGui::Bullet,
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
	[](const char *desc_id, const ImVec4 &col, ImGuiColorEditFlags flags, ImVec2 size) -> bool { return ImGui::ColorButton(desc_id, col, flags, size); },
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
	[]() { ImGui::BeginTooltip(); },
	ImGui::EndTooltip,
	ImGui::SetTooltipV,
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
	ImGui::TableHeadersRow,
	ImGui::TableHeader,
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
	ImGui::GetItemRectMin,
	ImGui::GetItemRectMax,
	ImGui::GetItemRectSize,
	[]() { /* ImGui::SetItemAllowOverlap(); */ },
	ImGui::IsRectVisible,
	ImGui::IsRectVisible,
	ImGui::GetTime,
	ImGui::GetFrameCount,
	ImGui::GetBackgroundDrawList,
	ImGui::GetForegroundDrawList,
	ImGui::GetBackgroundDrawList,
	ImGui::GetForegroundDrawList,
	ImGui::GetDrawListSharedData,
	ImGui::GetStyleColorName,
	ImGui::SetStateStorage,
	ImGui::GetStateStorage,
	ImGui::BeginChildFrame,
	ImGui::EndChildFrame,
	ImGui::CalcTextSize,
	ImGui::ColorConvertU32ToFloat4,
	ImGui::ColorConvertFloat4ToU32,
	ImGui::ColorConvertRGBtoHSV,
	ImGui::ColorConvertHSVtoRGB,
	[](ImGuiKey imgui_key) -> int { return static_cast<int>(ImGui::GetKeyIndex(imgui_key)); },
	[](int user_key_index) -> bool { return ImGui::IsKeyDown(static_cast<ImGuiKey>(user_key_index)); },
	[](int user_key_index, bool repeat) -> bool { return ImGui::IsKeyPressed(static_cast<ImGuiKey>(user_key_index), repeat); },
	[](int user_key_index) -> bool { return ImGui::IsKeyReleased(static_cast<ImGuiKey>(user_key_index)); },
	[](int user_key_index, float repeat_delay, float rate) -> int { return ImGui::GetKeyPressedAmount(static_cast<ImGuiKey>(user_key_index), repeat_delay, rate); },
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
	ImGui::DebugCheckVersionAndDataLayout,
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
	[](const ImGuiStorage *_this, ImGuiID key) -> void * { return _this->GetVoidPtr(key); },
	[](ImGuiStorage *_this, ImGuiID key, void *val) -> void { _this->SetVoidPtr(key, val); },
	[](ImGuiStorage *_this, ImGuiID key, int default_val) -> int * { return _this->GetIntRef(key, default_val); },
	[](ImGuiStorage *_this, ImGuiID key, bool default_val) -> bool * { return _this->GetBoolRef(key, default_val); },
	[](ImGuiStorage *_this, ImGuiID key, float default_val) -> float * { return _this->GetFloatRef(key, default_val); },
	[](ImGuiStorage *_this, ImGuiID key, void *default_val) -> void ** { return _this->GetVoidPtrRef(key, default_val); },
	[](ImGuiStorage *_this, int val) -> void { _this->SetAllInt(val); },
	[](ImGuiStorage *_this) -> void { _this->BuildSortByKey(); },
	[](imgui_list_clipper_18600 *_this) -> void {
		new(_this) imgui_list_clipper_18600();
		memset(_this, 0, sizeof(*_this));
		_this->ItemsCount = -1;
	},
	[](imgui_list_clipper_18600 *_this) -> void {
		ImGuiListClipper temp;
		imgui_list_clipper_18600::convert(*_this, temp);
		temp.End();
		_this->~imgui_list_clipper_18600();
	},
	[](imgui_list_clipper_18600 *_this, int items_count, float items_height) -> void {
		ImGuiListClipper temp;
		imgui_list_clipper_18600::convert(*_this, temp);
		temp.Begin(items_count, items_height);
		imgui_list_clipper_18600::convert(temp, *_this);
		temp.TempData = nullptr; // Prevent 'ImGuiListClipper' destructor from doing anything
	},
	[](imgui_list_clipper_18600 *_this) -> void {
		ImGuiListClipper temp;
		imgui_list_clipper_18600::convert(*_this, temp);
		temp.End();
		imgui_list_clipper_18600::convert(temp, *_this);
	},
	[](imgui_list_clipper_18600 *_this) -> bool {
		ImGuiListClipper temp;
		imgui_list_clipper_18600::convert(*_this, temp);
		const bool result = temp.Step();
		imgui_list_clipper_18600::convert(temp, *_this);
		temp.TempData = nullptr;
		return result;
	},
	[](imgui_list_clipper_18600 *_this, int item_min, int item_max) -> void {
		ImGuiListClipper temp;
		imgui_list_clipper_18600::convert(*_this, temp);
		temp.IncludeRangeByIndices(item_min, item_max);
		imgui_list_clipper_18600::convert(temp, *_this);
		temp.TempData = nullptr;
	},
	[](ImDrawList *_this, ImVec2 clip_rect_min, ImVec2 clip_rect_max, bool intersect_with_current_clip_rect) -> void { _this->PushClipRect(clip_rect_min, clip_rect_max, intersect_with_current_clip_rect); },
	[](ImDrawList *_this) -> void { _this->PushClipRectFullScreen(); },
	[](ImDrawList *_this) -> void { _this->PopClipRect(); },
	[](ImDrawList *_this, ImTextureID texture_id) -> void { _this->PushTextureID(texture_id); },
	[](ImDrawList *_this) -> void { _this->PopTextureID(); },
	[](ImDrawList *_this, const ImVec2 &p1, const ImVec2 &p2, ImU32 col, float thickness) -> void { _this->AddLine(p1, p2, col, thickness); },
	[](ImDrawList *_this, const ImVec2 &p_min, const ImVec2 &p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness) -> void { _this->AddRect(p_min, p_max, col, rounding, flags, thickness); },
	[](ImDrawList *_this, const ImVec2 &p_min, const ImVec2 &p_max, ImU32 col, float rounding, ImDrawFlags flags) -> void { _this->AddRectFilled(p_min, p_max, col, rounding, flags); },
	[](ImDrawList *_this, const ImVec2 &p_min, const ImVec2 &p_max, ImU32 col_upr_left, ImU32 col_upr_right, ImU32 col_bot_right, ImU32 col_bot_left) -> void { _this->AddRectFilledMultiColor(p_min, p_max, col_upr_left, col_upr_right, col_bot_right, col_bot_left); },
	[](ImDrawList *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col, float thickness) -> void { _this->AddQuad(p1, p2, p3, p4, col, thickness); },
	[](ImDrawList *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col) -> void { _this->AddQuadFilled(p1, p2, p3, p4, col); },
	[](ImDrawList *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col, float thickness) -> void { _this->AddTriangle(p1, p2, p3, col, thickness); },
	[](ImDrawList *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col) -> void { _this->AddTriangleFilled(p1, p2, p3, col); },
	[](ImDrawList *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments, float thickness) -> void { _this->AddCircle(center, radius, col, num_segments, thickness); },
	[](ImDrawList *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments) -> void { _this->AddCircleFilled(center, radius, col, num_segments); },
	[](ImDrawList *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments, float thickness) -> void { _this->AddNgon(center, radius, col, num_segments, thickness); },
	[](ImDrawList *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments) -> void { _this->AddNgonFilled(center, radius, col, num_segments); },
	[](ImDrawList *_this, const ImVec2 &pos, ImU32 col, const char *text_begin, const char *text_end) -> void { _this->AddText(pos, col, text_begin, text_end); },
	[](ImDrawList *_this, const imgui_font_18600 *font, float font_size, const ImVec2 &pos, ImU32 col, const char *text_begin, const char *text_end, float wrap_width, const ImVec4 *cpu_fine_clip_rect) -> void {
		if (font != nullptr) {
			ImFont temp;
			imgui_font_18600::convert(*font, temp);
			_this->AddText(&temp, font_size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip_rect);
		}
		else {
			_this->AddText(nullptr, font_size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip_rect);
		}
	},
	[](ImDrawList *_this, const ImVec2 *points, int num_points, ImU32 col, ImDrawFlags flags, float thickness) -> void { _this->AddPolyline(points, num_points, col, flags, thickness); },
	[](ImDrawList *_this, const ImVec2 *points, int num_points, ImU32 col) -> void { _this->AddConvexPolyFilled(points, num_points, col); },
	[](ImDrawList *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col, float thickness, int num_segments) -> void { _this->AddBezierCubic(p1, p2, p3, p4, col, thickness, num_segments); },
	[](ImDrawList *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col, float thickness, int num_segments) -> void { _this->AddBezierQuadratic(p1, p2, p3, col, thickness, num_segments); },
	[](ImDrawList *_this, ImTextureID user_texture_id, const ImVec2 &p_min, const ImVec2 &p_max, const ImVec2 &uv_min, const ImVec2 &uv_max, ImU32 col) -> void { _this->AddImage(user_texture_id, p_min, p_max, uv_min, uv_max, col); },
	[](ImDrawList *_this, ImTextureID user_texture_id, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, const ImVec2 &uv1, const ImVec2 &uv2, const ImVec2 &uv3, const ImVec2 &uv4, ImU32 col) -> void { _this->AddImageQuad(user_texture_id, p1, p2, p3, p4, uv1, uv2, uv3, uv4, col); },
	[](ImDrawList *_this, ImTextureID user_texture_id, const ImVec2 &p_min, const ImVec2 &p_max, const ImVec2 &uv_min, const ImVec2 &uv_max, ImU32 col, float rounding, ImDrawFlags flags) -> void { _this->AddImageRounded(user_texture_id, p_min, p_max, uv_min, uv_max, col, rounding, flags); },
	[](ImDrawList *_this, const ImVec2 &center, float radius, float a_min, float a_max, int num_segments) -> void { _this->PathArcTo(center, radius, a_min, a_max, num_segments); },
	[](ImDrawList *_this, const ImVec2 &center, float radius, int a_min_of_12, int a_max_of_12) -> void { _this->PathArcToFast(center, radius, a_min_of_12, a_max_of_12); },
	[](ImDrawList *_this, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, int num_segments) -> void { _this->PathBezierCubicCurveTo(p2, p3, p4, num_segments); },
	[](ImDrawList *_this, const ImVec2 &p2, const ImVec2 &p3, int num_segments) -> void { _this->PathBezierQuadraticCurveTo(p2, p3, num_segments); },
	[](ImDrawList *_this, const ImVec2 &rect_min, const ImVec2 &rect_max, float rounding, ImDrawFlags flags) -> void { _this->PathRect(rect_min, rect_max, rounding, flags); },
	[](ImDrawList *_this, ImDrawCallback callback, void *callback_data) -> void { _this->AddCallback(callback, callback_data); },
	[](ImDrawList *_this) -> void { _this->AddDrawCmd(); },
	[](const ImDrawList *_this) -> ImDrawList * { return _this->CloneOutput(); },
	[](ImDrawList *_this, int idx_count, int vtx_count) -> void { _this->PrimReserve(idx_count, vtx_count); },
	[](ImDrawList *_this, int idx_count, int vtx_count) -> void { _this->PrimUnreserve(idx_count, vtx_count); },
	[](ImDrawList *_this, const ImVec2 &a, const ImVec2 &b, ImU32 col) -> void { _this->PrimRect(a, b, col); },
	[](ImDrawList *_this, const ImVec2 &a, const ImVec2 &b, const ImVec2 &uv_a, const ImVec2 &uv_b, ImU32 col) -> void { _this->PrimRectUV(a, b, uv_a, uv_b, col); },
	[](ImDrawList *_this, const ImVec2 &a, const ImVec2 &b, const ImVec2 &c, const ImVec2 &d, const ImVec2 &uv_a, const ImVec2 &uv_b, const ImVec2 &uv_c, const ImVec2 &uv_d, ImU32 col) -> void { _this->PrimQuadUV(a, b, c, d, uv_a, uv_b, uv_c, uv_d, col); },
	[](imgui_font_18600 *_this) -> void {
		new(_this) imgui_font_18600();
		_this->FontSize = 0.0f;
		_this->FallbackAdvanceX = 0.0f;
		_this->FallbackChar = (ImWchar)-1;
		_this->EllipsisChar = (ImWchar)-1;
		_this->DotChar = (ImWchar)-1;
		_this->FallbackGlyph = NULL;
		_this->ContainerAtlas = NULL;
		_this->ConfigData = NULL;
		_this->ConfigDataCount = 0;
		_this->DirtyLookupTables = false;
		_this->Scale = 1.0f;
		_this->Ascent = 0.0f;
		_this->Descent = 0.0f;
		_this->MetricsTotalSurface = 0;
		memset(_this->Used4kPagesMap, 0, sizeof(_this->Used4kPagesMap));
	},
	[](imgui_font_18600 *_this) -> void {
		_this->~imgui_font_18600();
	},
	[](const imgui_font_18600 *_this, ImWchar c) -> const ImFontGlyph * {
		ImFont temp;
		imgui_font_18600::convert(*_this, temp);
		return temp.FindGlyph(c);
	},
	[](const imgui_font_18600 *_this, ImWchar c) -> const ImFontGlyph * {
		ImFont temp;
		imgui_font_18600::convert(*_this, temp);
		return temp.FindGlyphNoFallback(c);
	},
	[](const imgui_font_18600 *_this, float size, float max_width, float wrap_width, const char *text_begin, const char *text_end, const char **remaining) -> ImVec2 {
		ImFont temp;
		imgui_font_18600::convert(*_this, temp);
		return temp.CalcTextSizeA(size, max_width, wrap_width, text_begin, text_end, remaining);
	},
	[](const imgui_font_18600 *_this, float scale, const char *text, const char *text_end, float wrap_width) -> const char * {
		ImFont temp;
		imgui_font_18600::convert(*_this, temp);
		return temp.CalcWordWrapPositionA(scale, text, text_end, wrap_width);
	},
	[](const imgui_font_18600 *_this, ImDrawList *draw_list, float size, ImVec2 pos, ImU32 col, ImWchar c) -> void {
		ImFont temp;
		imgui_font_18600::convert(*_this, temp);
		temp.RenderChar(draw_list, size, pos, col, c);
	},
	[](const imgui_font_18600 *_this, ImDrawList *draw_list, float size, ImVec2 pos, ImU32 col, const ImVec4 &clip_rect, const char *text_begin, const char *text_end, float wrap_width, bool cpu_fine_clip) -> void {
		ImFont temp;
		imgui_font_18600::convert(*_this, temp);
		temp.RenderText(draw_list, size, pos, col, clip_rect, text_begin, text_end, wrap_width, cpu_fine_clip);
	},

};

#endif
