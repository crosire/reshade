/*
 * Copyright (C) 2021 Patrick Mours
 * Copyright (C) 2014-2023 Omar Cornut
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON

#include <new>
#include "imgui_function_table_18971.hpp"

namespace
{
	void convert(const imgui_io_19000 &new_io, imgui_io_18971 &io)
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
		io.ImeWindowHandle = nullptr;

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
		io.MetricsActiveAllocations = 0;
		io.MouseDelta = new_io.MouseDelta;

		for (int i = 0; i < 652; ++i)
			io.KeyMap[i] = 0;
		for (int i = 0; i < 652; ++i)
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

	void convert(const imgui_style_19000 &new_style, imgui_style_18971 &style)
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
		style.SeparatorTextBorderSize = new_style.SeparatorTextBorderSize;
		style.SeparatorTextAlign = new_style.SeparatorTextAlign;
		style.SeparatorTextPadding = new_style.SeparatorTextPadding;
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
		style.HoverStationaryDelay = new_style.HoverStationaryDelay;
		style.HoverDelayShort = new_style.HoverDelayShort;
		style.HoverDelayNormal = new_style.HoverDelayNormal;
		style.HoverFlagsForTooltipMouse = new_style.HoverFlagsForTooltipMouse;
		style.HoverFlagsForTooltipNav = new_style.HoverFlagsForTooltipNav;
	}

	auto convert_window_flags(ImGuiWindowFlags old_flags) -> ImGuiWindowFlags
	{
		int new_flags = 0;
		if (old_flags & (1 << 18))
			new_flags |= ImGuiWindowFlags_NoNavInputs;
		if (old_flags & (1 << 19))
			new_flags |= ImGuiWindowFlags_NoNavFocus;
		if (old_flags & (1 << 20))
			new_flags |= ImGuiWindowFlags_UnsavedDocument;
		if (old_flags & (1 << 21))
			new_flags |= ImGuiWindowFlags_NoDocking;

		return (old_flags & 0xFFC0FFFF) | new_flags;
	}
	auto convert_hovered_flags(ImGuiHoveredFlags old_flags) -> ImGuiHoveredFlags
	{
		int new_flags = 0;
		if (old_flags & (1 << 11))
			new_flags |= ImGuiHoveredFlags_ForTooltip;
		if (old_flags & (1 << 12))
			new_flags |= ImGuiHoveredFlags_Stationary;
		if (old_flags & (1 << 13))
			new_flags |= ImGuiHoveredFlags_DelayNone;
		if (old_flags & (1 << 14))
			new_flags |= ImGuiHoveredFlags_DelayShort;
		if (old_flags & (1 << 15))
			new_flags |= ImGuiHoveredFlags_DelayNormal;
		if (old_flags & (1 << 16))
			new_flags |= ImGuiHoveredFlags_NoSharedDelay;

		return (old_flags & 0x00FFFFFF) | new_flags;
	}
	auto convert_tree_node_flags(ImGuiTreeNodeFlags old_flags) -> ImGuiTreeNodeFlags
	{
		int new_flags = 0;
		if (old_flags & (1 << 13))
			new_flags |= ImGuiTreeNodeFlags_NavLeftJumpsBackHere;

		return (old_flags & 0xFFFF9FFF) | new_flags;
	}

	auto convert_key(ImGuiKey old_value) -> ImGuiKey
	{
		return old_value <= ImGuiKey_F12 ? old_value : old_value <= (ImGuiKey_KeypadEqual - (ImGuiKey_F24 - ImGuiKey_F12)) ? static_cast<ImGuiKey>(old_value - (ImGuiKey_F24 - ImGuiKey_F12)) : static_cast<ImGuiKey>(old_value - (ImGuiKey_F24 - ImGuiKey_F12) - (ImGuiKey_AppForward - ImGuiKey_KeypadEqual));
	}
	auto convert_style_var(ImGuiStyleVar old_value) -> ImGuiStyleVar
	{
		return old_value <= ImGuiStyleVar_TabRounding ? old_value : old_value + 1;
	}
}

const imgui_function_table_18971 init_imgui_function_table_18971() { return {
	[]() -> imgui_io_18971 & {
		static imgui_io_18971 io = {};
		convert(g_imgui_function_table_19000.GetIO(), io);
		return io;
	},
	[]() -> imgui_style_18971 & {
		static imgui_style_18971 style = {};
		convert(g_imgui_function_table_19000.GetStyle(), style);
		return style;
	},
	g_imgui_function_table_19000.GetVersion,
	[](const char *name, bool *p_open, ImGuiWindowFlags flags) -> bool { return g_imgui_function_table_19000.Begin(name, p_open, convert_window_flags(flags)); },
	g_imgui_function_table_19000.End,
	[](const char *str_id, const ImVec2 &size, bool border, ImGuiWindowFlags flags) -> bool {
		ImGuiChildFlags child_flags = border ? ImGuiChildFlags_Border : ImGuiChildFlags_None;
		if (flags & (1 << 16))
			child_flags |= ImGuiChildFlags_AlwaysUseWindowPadding;
		ImVec2 full_size = size;
		if (flags & ImGuiWindowFlags_AlwaysAutoResize)
		{
			flags ^= ImGuiWindowFlags_AlwaysAutoResize;
			child_flags |= ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize;
			if (size.x == 0.0f)
				full_size.x = g_imgui_function_table_19000.GetContentRegionAvail().x;
			if (size.y == 0.0f)
				full_size.y = g_imgui_function_table_19000.GetContentRegionAvail().y;
		}
		return g_imgui_function_table_19000.BeginChild(str_id, full_size, child_flags, convert_window_flags(flags));
	},
	[](ImGuiID id, const ImVec2 &size, bool border, ImGuiWindowFlags flags) -> bool {
		ImGuiChildFlags child_flags = border ? ImGuiChildFlags_Border : ImGuiChildFlags_None;
		if (flags & (1 << 16))
			child_flags |= ImGuiChildFlags_AlwaysUseWindowPadding;
		ImVec2 full_size = size;
		if (flags & ImGuiWindowFlags_AlwaysAutoResize)
		{
			flags ^= ImGuiWindowFlags_AlwaysAutoResize;
			child_flags |= ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize;
			if (size.x == 0.0f)
				full_size.x = g_imgui_function_table_19000.GetContentRegionAvail().x;
			if (size.y == 0.0f)
				full_size.y = g_imgui_function_table_19000.GetContentRegionAvail().y;
		}
		return g_imgui_function_table_19000.BeginChild2(id, full_size, child_flags, convert_window_flags(flags));
	},
	g_imgui_function_table_19000.EndChild,
	g_imgui_function_table_19000.IsWindowAppearing,
	g_imgui_function_table_19000.IsWindowCollapsed,
	g_imgui_function_table_19000.IsWindowFocused,
	[](ImGuiHoveredFlags flags) -> bool { return g_imgui_function_table_19000.IsWindowHovered(convert_hovered_flags(flags)); },
	g_imgui_function_table_19000.GetWindowDrawList,
	g_imgui_function_table_19000.GetWindowDpiScale,
	g_imgui_function_table_19000.GetWindowPos,
	g_imgui_function_table_19000.GetWindowSize,
	g_imgui_function_table_19000.GetWindowWidth,
	g_imgui_function_table_19000.GetWindowHeight,
	g_imgui_function_table_19000.SetNextWindowPos,
	g_imgui_function_table_19000.SetNextWindowSize,
	g_imgui_function_table_19000.SetNextWindowSizeConstraints,
	g_imgui_function_table_19000.SetNextWindowContentSize,
	g_imgui_function_table_19000.SetNextWindowCollapsed,
	g_imgui_function_table_19000.SetNextWindowFocus,
	g_imgui_function_table_19000.SetNextWindowScroll,
	g_imgui_function_table_19000.SetNextWindowBgAlpha,
	g_imgui_function_table_19000.SetWindowPos,
	g_imgui_function_table_19000.SetWindowSize,
	g_imgui_function_table_19000.SetWindowCollapsed,
	g_imgui_function_table_19000.SetWindowFocus,
	g_imgui_function_table_19000.SetWindowFontScale,
	g_imgui_function_table_19000.SetWindowPos2,
	g_imgui_function_table_19000.SetWindowSize2,
	g_imgui_function_table_19000.SetWindowCollapsed2,
	g_imgui_function_table_19000.SetWindowFocus2,
	g_imgui_function_table_19000.GetContentRegionAvail,
	g_imgui_function_table_19000.GetContentRegionMax,
	g_imgui_function_table_19000.GetWindowContentRegionMin,
	g_imgui_function_table_19000.GetWindowContentRegionMax,
	g_imgui_function_table_19000.GetScrollX,
	g_imgui_function_table_19000.GetScrollY,
	g_imgui_function_table_19000.SetScrollX,
	g_imgui_function_table_19000.SetScrollY,
	g_imgui_function_table_19000.GetScrollMaxX,
	g_imgui_function_table_19000.GetScrollMaxY,
	g_imgui_function_table_19000.SetScrollHereX,
	g_imgui_function_table_19000.SetScrollHereY,
	g_imgui_function_table_19000.SetScrollFromPosX,
	g_imgui_function_table_19000.SetScrollFromPosY,
	g_imgui_function_table_19000.PushFont,
	g_imgui_function_table_19000.PopFont,
	g_imgui_function_table_19000.PushStyleColor,
	g_imgui_function_table_19000.PushStyleColor2,
	g_imgui_function_table_19000.PopStyleColor,
	[](ImGuiStyleVar idx, float val) { g_imgui_function_table_19000.PushStyleVar(convert_style_var(idx), val); },
	[](ImGuiStyleVar idx, const ImVec2 &val) { g_imgui_function_table_19000.PushStyleVar2(convert_style_var(idx), val); },
	g_imgui_function_table_19000.PopStyleVar,
	g_imgui_function_table_19000.PushTabStop,
	g_imgui_function_table_19000.PopTabStop,
	g_imgui_function_table_19000.PushButtonRepeat,
	g_imgui_function_table_19000.PopButtonRepeat,
	g_imgui_function_table_19000.PushItemWidth,
	g_imgui_function_table_19000.PopItemWidth,
	g_imgui_function_table_19000.SetNextItemWidth,
	g_imgui_function_table_19000.CalcItemWidth,
	g_imgui_function_table_19000.PushTextWrapPos,
	g_imgui_function_table_19000.PopTextWrapPos,
	g_imgui_function_table_19000.GetFont,
	g_imgui_function_table_19000.GetFontSize,
	g_imgui_function_table_19000.GetFontTexUvWhitePixel,
	g_imgui_function_table_19000.GetColorU32,
	g_imgui_function_table_19000.GetColorU322,
	g_imgui_function_table_19000.GetColorU323,
	g_imgui_function_table_19000.GetStyleColorVec4,
	g_imgui_function_table_19000.Separator,
	g_imgui_function_table_19000.SameLine,
	g_imgui_function_table_19000.NewLine,
	g_imgui_function_table_19000.Spacing,
	g_imgui_function_table_19000.Dummy,
	g_imgui_function_table_19000.Indent,
	g_imgui_function_table_19000.Unindent,
	g_imgui_function_table_19000.BeginGroup,
	g_imgui_function_table_19000.EndGroup,
	g_imgui_function_table_19000.GetCursorPos,
	g_imgui_function_table_19000.GetCursorPosX,
	g_imgui_function_table_19000.GetCursorPosY,
	g_imgui_function_table_19000.SetCursorPos,
	g_imgui_function_table_19000.SetCursorPosX,
	g_imgui_function_table_19000.SetCursorPosY,
	g_imgui_function_table_19000.GetCursorStartPos,
	g_imgui_function_table_19000.GetCursorScreenPos,
	g_imgui_function_table_19000.SetCursorScreenPos,
	g_imgui_function_table_19000.AlignTextToFramePadding,
	g_imgui_function_table_19000.GetTextLineHeight,
	g_imgui_function_table_19000.GetTextLineHeightWithSpacing,
	g_imgui_function_table_19000.GetFrameHeight,
	g_imgui_function_table_19000.GetFrameHeightWithSpacing,
	g_imgui_function_table_19000.PushID,
	g_imgui_function_table_19000.PushID2,
	g_imgui_function_table_19000.PushID3,
	g_imgui_function_table_19000.PushID4,
	g_imgui_function_table_19000.PopID,
	g_imgui_function_table_19000.GetID,
	g_imgui_function_table_19000.GetID2,
	g_imgui_function_table_19000.GetID3,
	g_imgui_function_table_19000.TextUnformatted,
	g_imgui_function_table_19000.TextV,
	g_imgui_function_table_19000.TextColoredV,
	g_imgui_function_table_19000.TextDisabledV,
	g_imgui_function_table_19000.TextWrappedV,
	g_imgui_function_table_19000.LabelTextV,
	g_imgui_function_table_19000.BulletTextV,
	g_imgui_function_table_19000.SeparatorText,
	g_imgui_function_table_19000.Button,
	g_imgui_function_table_19000.SmallButton,
	g_imgui_function_table_19000.InvisibleButton,
	g_imgui_function_table_19000.ArrowButton,
	g_imgui_function_table_19000.Checkbox,
	g_imgui_function_table_19000.CheckboxFlags,
	g_imgui_function_table_19000.CheckboxFlags2,
	g_imgui_function_table_19000.RadioButton,
	g_imgui_function_table_19000.RadioButton2,
	g_imgui_function_table_19000.ProgressBar,
	g_imgui_function_table_19000.Bullet,
	g_imgui_function_table_19000.Image,
	g_imgui_function_table_19000.ImageButton,
	g_imgui_function_table_19000.BeginCombo,
	g_imgui_function_table_19000.EndCombo,
	g_imgui_function_table_19000.Combo,
	g_imgui_function_table_19000.Combo2,
	[](const char *label, int *current_item, bool (*callback)(void *user_data, int idx, const char **out_text), void *user_data, int items_count, int popup_max_height_in_items) -> bool {
		struct old_to_new_data_type { void *user_data; bool (*old_callback)(void *, int, const char **); } old_to_new_data = { user_data, callback };
		return g_imgui_function_table_19000.Combo3(
			label,
			current_item,
			[](void *user_data, int idx) -> const char *{
				const char *element = nullptr;
				static_cast<old_to_new_data_type *>(user_data)->old_callback(static_cast<old_to_new_data_type *>(user_data)->user_data, idx, &element);
				return element;
			},
			&old_to_new_data,
			items_count,
			popup_max_height_in_items);
	},
	g_imgui_function_table_19000.DragFloat,
	g_imgui_function_table_19000.DragFloat2,
	g_imgui_function_table_19000.DragFloat3,
	g_imgui_function_table_19000.DragFloat4,
	g_imgui_function_table_19000.DragFloatRange2,
	g_imgui_function_table_19000.DragInt,
	g_imgui_function_table_19000.DragInt2,
	g_imgui_function_table_19000.DragInt3,
	g_imgui_function_table_19000.DragInt4,
	g_imgui_function_table_19000.DragIntRange2,
	g_imgui_function_table_19000.DragScalar,
	g_imgui_function_table_19000.DragScalarN,
	g_imgui_function_table_19000.SliderFloat,
	g_imgui_function_table_19000.SliderFloat2,
	g_imgui_function_table_19000.SliderFloat3,
	g_imgui_function_table_19000.SliderFloat4,
	g_imgui_function_table_19000.SliderAngle,
	g_imgui_function_table_19000.SliderInt,
	g_imgui_function_table_19000.SliderInt2,
	g_imgui_function_table_19000.SliderInt3,
	g_imgui_function_table_19000.SliderInt4,
	g_imgui_function_table_19000.SliderScalar,
	g_imgui_function_table_19000.SliderScalarN,
	g_imgui_function_table_19000.VSliderFloat,
	g_imgui_function_table_19000.VSliderInt,
	g_imgui_function_table_19000.VSliderScalar,
	g_imgui_function_table_19000.InputText,
	g_imgui_function_table_19000.InputTextMultiline,
	g_imgui_function_table_19000.InputTextWithHint,
	g_imgui_function_table_19000.InputFloat,
	g_imgui_function_table_19000.InputFloat2,
	g_imgui_function_table_19000.InputFloat3,
	g_imgui_function_table_19000.InputFloat4,
	g_imgui_function_table_19000.InputInt,
	g_imgui_function_table_19000.InputInt2,
	g_imgui_function_table_19000.InputInt3,
	g_imgui_function_table_19000.InputInt4,
	g_imgui_function_table_19000.InputDouble,
	g_imgui_function_table_19000.InputScalar,
	g_imgui_function_table_19000.InputScalarN,
	g_imgui_function_table_19000.ColorEdit3,
	g_imgui_function_table_19000.ColorEdit4,
	g_imgui_function_table_19000.ColorPicker3,
	g_imgui_function_table_19000.ColorPicker4,
	g_imgui_function_table_19000.ColorButton,
	g_imgui_function_table_19000.SetColorEditOptions,
	g_imgui_function_table_19000.TreeNode,
	g_imgui_function_table_19000.TreeNodeV,
	g_imgui_function_table_19000.TreeNodeV2,
	[](const char *label, ImGuiTreeNodeFlags flags) -> bool { return g_imgui_function_table_19000.TreeNodeEx(label, convert_tree_node_flags(flags)); },
	[](const char *str_id, ImGuiTreeNodeFlags flags, const char *fmt, va_list args) -> bool { return g_imgui_function_table_19000.TreeNodeExV(str_id, convert_tree_node_flags(flags), fmt, args); },
	[](const void *ptr_id, ImGuiTreeNodeFlags flags, const char *fmt, va_list args) -> bool { return g_imgui_function_table_19000.TreeNodeExV2(ptr_id, convert_tree_node_flags(flags), fmt, args); },
	g_imgui_function_table_19000.TreePush,
	g_imgui_function_table_19000.TreePush2,
	g_imgui_function_table_19000.TreePop,
	g_imgui_function_table_19000.GetTreeNodeToLabelSpacing,
	[](const char *label, ImGuiTreeNodeFlags flags) -> bool { return g_imgui_function_table_19000.CollapsingHeader(label, convert_tree_node_flags(flags)); },
	[](const char *label, bool *p_visible, ImGuiTreeNodeFlags flags) -> bool { return g_imgui_function_table_19000.CollapsingHeader2(label, p_visible, convert_tree_node_flags(flags)); },
	g_imgui_function_table_19000.SetNextItemOpen,
	g_imgui_function_table_19000.Selectable,
	g_imgui_function_table_19000.Selectable2,
	g_imgui_function_table_19000.BeginListBox,
	g_imgui_function_table_19000.EndListBox,
	g_imgui_function_table_19000.ListBox,
	[](const char *label, int *current_item, bool (*callback)(void *user_data, int idx, const char **out_text), void *user_data, int items_count, int height_in_items) -> bool {

		struct old_to_new_data_type { void *user_data; bool (*old_callback)(void *, int, const char **); } old_to_new_data = { user_data, callback };
		return g_imgui_function_table_19000.ListBox2(
			label,
			current_item,
			[](void *user_data, int idx) -> const char *{
				const char *element = nullptr;
				static_cast<old_to_new_data_type *>(user_data)->old_callback(static_cast<old_to_new_data_type *>(user_data)->user_data, idx, &element);
				return element;
			},
			&old_to_new_data,
			items_count,
			height_in_items);
	},
	g_imgui_function_table_19000.PlotLines,
	g_imgui_function_table_19000.PlotLines2,
	g_imgui_function_table_19000.PlotHistogram,
	g_imgui_function_table_19000.PlotHistogram2,
	g_imgui_function_table_19000.Value,
	g_imgui_function_table_19000.Value2,
	g_imgui_function_table_19000.Value3,
	g_imgui_function_table_19000.Value4,
	g_imgui_function_table_19000.BeginMenuBar,
	g_imgui_function_table_19000.EndMenuBar,
	g_imgui_function_table_19000.BeginMainMenuBar,
	g_imgui_function_table_19000.EndMainMenuBar,
	g_imgui_function_table_19000.BeginMenu,
	g_imgui_function_table_19000.EndMenu,
	g_imgui_function_table_19000.MenuItem,
	g_imgui_function_table_19000.MenuItem2,
	g_imgui_function_table_19000.BeginTooltip,
	g_imgui_function_table_19000.EndTooltip,
	g_imgui_function_table_19000.SetTooltipV,
	g_imgui_function_table_19000.BeginItemTooltip,
	g_imgui_function_table_19000.SetItemTooltipV,
	[](const char *str_id, ImGuiWindowFlags flags) -> bool { return g_imgui_function_table_19000.BeginPopup(str_id, convert_window_flags(flags)); },
	[](const char *name, bool *p_open, ImGuiWindowFlags flags) -> bool { return g_imgui_function_table_19000.BeginPopupModal(name, p_open, convert_window_flags(flags)); },
	g_imgui_function_table_19000.EndPopup,
	g_imgui_function_table_19000.OpenPopup,
	g_imgui_function_table_19000.OpenPopup2,
	g_imgui_function_table_19000.OpenPopupOnItemClick,
	g_imgui_function_table_19000.CloseCurrentPopup,
	g_imgui_function_table_19000.BeginPopupContextItem,
	g_imgui_function_table_19000.BeginPopupContextWindow,
	g_imgui_function_table_19000.BeginPopupContextVoid,
	g_imgui_function_table_19000.IsPopupOpen,
	g_imgui_function_table_19000.BeginTable,
	g_imgui_function_table_19000.EndTable,
	g_imgui_function_table_19000.TableNextRow,
	g_imgui_function_table_19000.TableNextColumn,
	g_imgui_function_table_19000.TableSetColumnIndex,
	g_imgui_function_table_19000.TableSetupColumn,
	g_imgui_function_table_19000.TableSetupScrollFreeze,
	g_imgui_function_table_19000.TableHeadersRow,
	g_imgui_function_table_19000.TableHeader,
	g_imgui_function_table_19000.TableGetSortSpecs,
	g_imgui_function_table_19000.TableGetColumnCount,
	g_imgui_function_table_19000.TableGetColumnIndex,
	g_imgui_function_table_19000.TableGetRowIndex,
	g_imgui_function_table_19000.TableGetColumnName,
	g_imgui_function_table_19000.TableGetColumnFlags,
	g_imgui_function_table_19000.TableSetColumnEnabled,
	g_imgui_function_table_19000.TableSetBgColor,
	g_imgui_function_table_19000.Columns,
	g_imgui_function_table_19000.NextColumn,
	g_imgui_function_table_19000.GetColumnIndex,
	g_imgui_function_table_19000.GetColumnWidth,
	g_imgui_function_table_19000.SetColumnWidth,
	g_imgui_function_table_19000.GetColumnOffset,
	g_imgui_function_table_19000.SetColumnOffset,
	g_imgui_function_table_19000.GetColumnsCount,
	g_imgui_function_table_19000.BeginTabBar,
	g_imgui_function_table_19000.EndTabBar,
	g_imgui_function_table_19000.BeginTabItem,
	g_imgui_function_table_19000.EndTabItem,
	g_imgui_function_table_19000.TabItemButton,
	g_imgui_function_table_19000.SetTabItemClosed,
	g_imgui_function_table_19000.DockSpace,
	g_imgui_function_table_19000.SetNextWindowDockID,
	g_imgui_function_table_19000.SetNextWindowClass,
	g_imgui_function_table_19000.GetWindowDockID,
	g_imgui_function_table_19000.IsWindowDocked,
	g_imgui_function_table_19000.BeginDragDropSource,
	g_imgui_function_table_19000.SetDragDropPayload,
	g_imgui_function_table_19000.EndDragDropSource,
	g_imgui_function_table_19000.BeginDragDropTarget,
	g_imgui_function_table_19000.AcceptDragDropPayload,
	g_imgui_function_table_19000.EndDragDropTarget,
	g_imgui_function_table_19000.GetDragDropPayload,
	g_imgui_function_table_19000.BeginDisabled,
	g_imgui_function_table_19000.EndDisabled,
	g_imgui_function_table_19000.PushClipRect,
	g_imgui_function_table_19000.PopClipRect,
	g_imgui_function_table_19000.SetItemDefaultFocus,
	g_imgui_function_table_19000.SetKeyboardFocusHere,
	g_imgui_function_table_19000.SetNextItemAllowOverlap,
	[](ImGuiHoveredFlags flags) -> bool { return g_imgui_function_table_19000.IsItemHovered(convert_hovered_flags(flags)); },
	g_imgui_function_table_19000.IsItemActive,
	g_imgui_function_table_19000.IsItemFocused,
	g_imgui_function_table_19000.IsItemClicked,
	g_imgui_function_table_19000.IsItemVisible,
	g_imgui_function_table_19000.IsItemEdited,
	g_imgui_function_table_19000.IsItemActivated,
	g_imgui_function_table_19000.IsItemDeactivated,
	g_imgui_function_table_19000.IsItemDeactivatedAfterEdit,
	g_imgui_function_table_19000.IsItemToggledOpen,
	g_imgui_function_table_19000.IsAnyItemHovered,
	g_imgui_function_table_19000.IsAnyItemActive,
	g_imgui_function_table_19000.IsAnyItemFocused,
	g_imgui_function_table_19000.GetItemID,
	g_imgui_function_table_19000.GetItemRectMin,
	g_imgui_function_table_19000.GetItemRectMax,
	g_imgui_function_table_19000.GetItemRectSize,
	g_imgui_function_table_19000.GetBackgroundDrawList,
	g_imgui_function_table_19000.GetForegroundDrawList,
	g_imgui_function_table_19000.GetBackgroundDrawList2,
	g_imgui_function_table_19000.GetForegroundDrawList2,
	g_imgui_function_table_19000.IsRectVisible,
	g_imgui_function_table_19000.IsRectVisible2,
	g_imgui_function_table_19000.GetTime,
	g_imgui_function_table_19000.GetFrameCount,
	g_imgui_function_table_19000.GetDrawListSharedData,
	g_imgui_function_table_19000.GetStyleColorName,
	g_imgui_function_table_19000.SetStateStorage,
	g_imgui_function_table_19000.GetStateStorage,
	[](ImGuiID id, const ImVec2 &size, ImGuiWindowFlags flags) -> bool {
		ImGuiChildFlags child_flags = ImGuiChildFlags_FrameStyle;
		if (flags & (1 << 16))
			child_flags |= ImGuiChildFlags_AlwaysUseWindowPadding;
		ImVec2 full_size = size;
		if (flags & ImGuiWindowFlags_AlwaysAutoResize)
		{
			flags ^= ImGuiWindowFlags_AlwaysAutoResize;
			child_flags |= ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize;
			if (size.x == 0.0f)
				full_size.x = g_imgui_function_table_19000.GetContentRegionAvail().x;
			if (size.y == 0.0f)
				full_size.y = g_imgui_function_table_19000.GetContentRegionAvail().y;
		}
		return g_imgui_function_table_19000.BeginChild2(id, full_size, child_flags, convert_window_flags(flags));
	},
	[]() { g_imgui_function_table_19000.EndChild(); },
	g_imgui_function_table_19000.CalcTextSize,
	g_imgui_function_table_19000.ColorConvertU32ToFloat4,
	g_imgui_function_table_19000.ColorConvertFloat4ToU32,
	g_imgui_function_table_19000.ColorConvertRGBtoHSV,
	g_imgui_function_table_19000.ColorConvertHSVtoRGB,
	[](ImGuiKey key) -> bool { return g_imgui_function_table_19000.IsKeyDown(convert_key(key)); },
	[](ImGuiKey key, bool repeat) -> bool { return g_imgui_function_table_19000.IsKeyPressed(convert_key(key), repeat); },
	[](ImGuiKey key) -> bool { return g_imgui_function_table_19000.IsKeyReleased(convert_key(key)); },
	[](ImGuiKey key, float repeat_delay, float rate) -> int { return g_imgui_function_table_19000.GetKeyPressedAmount(convert_key(key), repeat_delay, rate); },
	[](ImGuiKey key) -> const char * { return g_imgui_function_table_19000.GetKeyName(convert_key(key)); },
	g_imgui_function_table_19000.SetNextFrameWantCaptureKeyboard,
	g_imgui_function_table_19000.IsMouseDown,
	g_imgui_function_table_19000.IsMouseClicked,
	g_imgui_function_table_19000.IsMouseReleased,
	g_imgui_function_table_19000.IsMouseDoubleClicked,
	g_imgui_function_table_19000.GetMouseClickedCount,
	g_imgui_function_table_19000.IsMouseHoveringRect,
	g_imgui_function_table_19000.IsMousePosValid,
	g_imgui_function_table_19000.IsAnyMouseDown,
	g_imgui_function_table_19000.GetMousePos,
	g_imgui_function_table_19000.GetMousePosOnOpeningCurrentPopup,
	g_imgui_function_table_19000.IsMouseDragging,
	g_imgui_function_table_19000.GetMouseDragDelta,
	g_imgui_function_table_19000.ResetMouseDragDelta,
	g_imgui_function_table_19000.GetMouseCursor,
	g_imgui_function_table_19000.SetMouseCursor,
	g_imgui_function_table_19000.SetNextFrameWantCaptureMouse,
	g_imgui_function_table_19000.GetClipboardText,
	g_imgui_function_table_19000.SetClipboardText,
	g_imgui_function_table_19000.SetAllocatorFunctions,
	g_imgui_function_table_19000.GetAllocatorFunctions,
	g_imgui_function_table_19000.MemAlloc,
	g_imgui_function_table_19000.MemFree,
	g_imgui_function_table_19000.ImGuiStorage_GetInt,
	g_imgui_function_table_19000.ImGuiStorage_SetInt,
	g_imgui_function_table_19000.ImGuiStorage_GetBool,
	g_imgui_function_table_19000.ImGuiStorage_SetBool,
	g_imgui_function_table_19000.ImGuiStorage_GetFloat,
	g_imgui_function_table_19000.ImGuiStorage_SetFloat,
	g_imgui_function_table_19000.ImGuiStorage_GetVoidPtr,
	g_imgui_function_table_19000.ImGuiStorage_SetVoidPtr,
	g_imgui_function_table_19000.ImGuiStorage_GetIntRef,
	g_imgui_function_table_19000.ImGuiStorage_GetBoolRef,
	g_imgui_function_table_19000.ImGuiStorage_GetFloatRef,
	g_imgui_function_table_19000.ImGuiStorage_GetVoidPtrRef,
	g_imgui_function_table_19000.ImGuiStorage_SetAllInt,
	g_imgui_function_table_19000.ImGuiStorage_BuildSortByKey,
	g_imgui_function_table_19000.ConstructImGuiListClipper,
	g_imgui_function_table_19000.DestructImGuiListClipper,
	g_imgui_function_table_19000.ImGuiListClipper_Begin,
	g_imgui_function_table_19000.ImGuiListClipper_End,
	g_imgui_function_table_19000.ImGuiListClipper_Step,
	g_imgui_function_table_19000.ImGuiListClipper_IncludeItemsByIndex,
	g_imgui_function_table_19000.ImDrawList_PushClipRect,
	g_imgui_function_table_19000.ImDrawList_PushClipRectFullScreen,
	g_imgui_function_table_19000.ImDrawList_PopClipRect,
	g_imgui_function_table_19000.ImDrawList_PushTextureID,
	g_imgui_function_table_19000.ImDrawList_PopTextureID,
	g_imgui_function_table_19000.ImDrawList_AddLine,
	g_imgui_function_table_19000.ImDrawList_AddRect,
	g_imgui_function_table_19000.ImDrawList_AddRectFilled,
	g_imgui_function_table_19000.ImDrawList_AddRectFilledMultiColor,
	g_imgui_function_table_19000.ImDrawList_AddQuad,
	g_imgui_function_table_19000.ImDrawList_AddQuadFilled,
	g_imgui_function_table_19000.ImDrawList_AddTriangle,
	g_imgui_function_table_19000.ImDrawList_AddTriangleFilled,
	g_imgui_function_table_19000.ImDrawList_AddCircle,
	g_imgui_function_table_19000.ImDrawList_AddCircleFilled,
	g_imgui_function_table_19000.ImDrawList_AddNgon,
	g_imgui_function_table_19000.ImDrawList_AddNgonFilled,
	g_imgui_function_table_19000.ImDrawList_AddText,
	g_imgui_function_table_19000.ImDrawList_AddText2,
	g_imgui_function_table_19000.ImDrawList_AddPolyline,
	g_imgui_function_table_19000.ImDrawList_AddConvexPolyFilled,
	g_imgui_function_table_19000.ImDrawList_AddBezierCubic,
	g_imgui_function_table_19000.ImDrawList_AddBezierQuadratic,
	g_imgui_function_table_19000.ImDrawList_AddImage,
	g_imgui_function_table_19000.ImDrawList_AddImageQuad,
	g_imgui_function_table_19000.ImDrawList_AddImageRounded,
	g_imgui_function_table_19000.ImDrawList_PathArcTo,
	g_imgui_function_table_19000.ImDrawList_PathArcToFast,
	g_imgui_function_table_19000.ImDrawList_PathBezierCubicCurveTo,
	g_imgui_function_table_19000.ImDrawList_PathBezierQuadraticCurveTo,
	g_imgui_function_table_19000.ImDrawList_PathRect,
	g_imgui_function_table_19000.ImDrawList_AddCallback,
	g_imgui_function_table_19000.ImDrawList_AddDrawCmd,
	g_imgui_function_table_19000.ImDrawList_CloneOutput,
	g_imgui_function_table_19000.ImDrawList_PrimReserve,
	g_imgui_function_table_19000.ImDrawList_PrimUnreserve,
	g_imgui_function_table_19000.ImDrawList_PrimRect,
	g_imgui_function_table_19000.ImDrawList_PrimRectUV,
	g_imgui_function_table_19000.ImDrawList_PrimQuadUV,
	g_imgui_function_table_19000.ConstructImFont,
	g_imgui_function_table_19000.DestructImFont,
	g_imgui_function_table_19000.ImFont_FindGlyph,
	g_imgui_function_table_19000.ImFont_FindGlyphNoFallback,
	g_imgui_function_table_19000.ImFont_CalcTextSizeA,
	g_imgui_function_table_19000.ImFont_CalcWordWrapPositionA,
	g_imgui_function_table_19000.ImFont_RenderChar,
	g_imgui_function_table_19000.ImFont_RenderText,

}; }

#endif
