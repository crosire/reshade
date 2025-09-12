/*
 * Copyright (C) 2021 Patrick Mours
 * Copyright (C) 2014-2025 Omar Cornut
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON && RESHADE_GUI

#include <new>
#include "imgui_function_table_19191.hpp"

namespace
{
	auto convert_style_col(ImGuiCol old_value) -> ImGuiCol
	{
		if (old_value <= 33) // ImGuiCol_ResizeGripActive
			return old_value;
		else if (old_value <= 52) // ImGuiCol_TextSelectedBg
			return old_value + 1;
		else
			return old_value + 2;
	}
	auto convert_style_var(ImGuiStyleVar old_value) -> ImGuiStyleVar
	{
		if (old_value <= 24) // ImGuiStyleVar_TabBorderSize
			return old_value;
		else if (old_value <= 28) // ImGuiStyleVar_TableAngledHeadersTextAlign
			return old_value + 2;
		else
			return old_value + 4;
	}

	auto convert_tab_bar_flags(ImGuiTabBarFlags old_flags) -> ImGuiTabBarFlags
	{
		ImGuiTabBarFlags new_flags = old_flags & 0x7F;
		if (old_flags & (1 << 7)) // ImGuiTabBarFlags_FittingPolicyResizeDown
			new_flags |= 1 << 8;
		if (old_flags & (1 << 8)) // ImGuiTabBarFlags_FittingPolicyScroll
			new_flags |= 1 << 9;
		return new_flags;
	}

	void convert(const imgui_io_19222 &new_io, imgui_io_19191 &old_io)
	{
		old_io.ConfigFlags = new_io.ConfigFlags;
		old_io.BackendFlags = new_io.BackendFlags;
		old_io.DisplaySize = new_io.DisplaySize;
		old_io.DeltaTime = new_io.DeltaTime;
		old_io.IniSavingRate = new_io.IniSavingRate;
		old_io.IniFilename = new_io.IniFilename;
		old_io.LogFilename = new_io.LogFilename;
		old_io.UserData = new_io.UserData;

		// It's not safe to access the internal fields of 'FontAtlas'
		old_io.Fonts = new_io.Fonts;
		old_io.FontGlobalScale = 1.0f;
		old_io.FontAllowUserScaling = new_io.FontAllowUserScaling;
		old_io.FontDefault = nullptr;
		old_io.DisplayFramebufferScale = new_io.DisplayFramebufferScale;

		old_io.ConfigNavSwapGamepadButtons = new_io.ConfigNavSwapGamepadButtons;
		old_io.ConfigNavMoveSetMousePos = new_io.ConfigNavMoveSetMousePos;
		old_io.ConfigNavCaptureKeyboard = new_io.ConfigNavCaptureKeyboard;
		old_io.ConfigNavEscapeClearFocusItem = new_io.ConfigNavEscapeClearFocusItem;
		old_io.ConfigNavEscapeClearFocusWindow = new_io.ConfigNavEscapeClearFocusWindow;
		old_io.ConfigNavCursorVisibleAuto = new_io.ConfigNavCursorVisibleAuto;
		old_io.ConfigNavCursorVisibleAlways = new_io.ConfigNavCursorVisibleAlways;

		old_io.ConfigDockingNoSplit = new_io.ConfigDockingNoSplit;
		old_io.ConfigDockingWithShift = new_io.ConfigDockingWithShift;
		old_io.ConfigDockingAlwaysTabBar = new_io.ConfigDockingAlwaysTabBar;
		old_io.ConfigDockingTransparentPayload = new_io.ConfigDockingTransparentPayload;

		old_io.ConfigViewportsNoAutoMerge = new_io.ConfigViewportsNoAutoMerge;
		old_io.ConfigViewportsNoTaskBarIcon = new_io.ConfigViewportsNoTaskBarIcon;
		old_io.ConfigViewportsNoDecoration = new_io.ConfigViewportsNoDecoration;
		old_io.ConfigViewportsNoDefaultParent = new_io.ConfigViewportsNoDefaultParent;

		old_io.MouseDrawCursor = new_io.MouseDrawCursor;
		old_io.ConfigMacOSXBehaviors = new_io.ConfigMacOSXBehaviors;
		old_io.ConfigInputTrickleEventQueue = new_io.ConfigInputTrickleEventQueue;
		old_io.ConfigInputTextCursorBlink = new_io.ConfigInputTextCursorBlink;
		old_io.ConfigInputTextEnterKeepActive = new_io.ConfigInputTextEnterKeepActive;
		old_io.ConfigDragClickToInputText = new_io.ConfigDragClickToInputText;
		old_io.ConfigWindowsResizeFromEdges = new_io.ConfigWindowsResizeFromEdges;
		old_io.ConfigWindowsMoveFromTitleBarOnly = new_io.ConfigWindowsMoveFromTitleBarOnly;
		old_io.ConfigWindowsCopyContentsWithCtrlC = new_io.ConfigWindowsCopyContentsWithCtrlC;
		old_io.ConfigScrollbarScrollByPage = new_io.ConfigScrollbarScrollByPage;
		old_io.ConfigMemoryCompactTimer = new_io.ConfigMemoryCompactTimer;

		old_io.MouseDoubleClickTime = new_io.MouseDoubleClickTime;
		old_io.MouseDoubleClickMaxDist = new_io.MouseDoubleClickMaxDist;
		old_io.MouseDragThreshold = new_io.MouseDragThreshold;
		old_io.KeyRepeatDelay = new_io.KeyRepeatDelay;
		old_io.KeyRepeatRate = new_io.KeyRepeatRate;

		old_io.ConfigErrorRecovery = new_io.ConfigErrorRecovery;
		old_io.ConfigErrorRecoveryEnableAssert = new_io.ConfigErrorRecoveryEnableAssert;
		old_io.ConfigErrorRecoveryEnableDebugLog = new_io.ConfigErrorRecoveryEnableDebugLog;
		old_io.ConfigErrorRecoveryEnableTooltip = new_io.ConfigErrorRecoveryEnableTooltip;
		old_io.ConfigDebugIsDebuggerPresent = new_io.ConfigDebugIsDebuggerPresent;
		old_io.ConfigDebugHighlightIdConflicts = new_io.ConfigDebugHighlightIdConflicts;
		old_io.ConfigDebugHighlightIdConflictsShowItemPicker = new_io.ConfigDebugHighlightIdConflictsShowItemPicker;
		old_io.ConfigDebugBeginReturnValueOnce = new_io.ConfigDebugBeginReturnValueOnce;
		old_io.ConfigDebugBeginReturnValueLoop = new_io.ConfigDebugBeginReturnValueLoop;
		old_io.ConfigDebugIgnoreFocusLoss = new_io.ConfigDebugIgnoreFocusLoss;
		old_io.ConfigDebugIniSettings = new_io.ConfigDebugIniSettings;

		old_io.BackendPlatformName = new_io.BackendPlatformName;
		old_io.BackendRendererName = new_io.BackendRendererName;
		old_io.BackendPlatformUserData = new_io.BackendPlatformUserData;
		old_io.BackendRendererUserData = new_io.BackendRendererUserData;
		old_io.BackendLanguageUserData = new_io.BackendLanguageUserData;

		old_io.WantCaptureMouse = new_io.WantCaptureMouse;
		old_io.WantCaptureKeyboard = new_io.WantCaptureKeyboard;
		old_io.WantTextInput = new_io.WantTextInput;
		old_io.WantSetMousePos = new_io.WantSetMousePos;
		old_io.WantSaveIniSettings = new_io.WantSaveIniSettings;
		old_io.NavActive = new_io.NavActive;
		old_io.NavVisible = new_io.NavVisible;
		old_io.Framerate = new_io.Framerate;
		old_io.MetricsRenderVertices = new_io.MetricsRenderVertices;
		old_io.MetricsRenderIndices = new_io.MetricsRenderIndices;
		old_io.MetricsRenderWindows = new_io.MetricsRenderWindows;
		old_io.MetricsActiveWindows = new_io.MetricsActiveWindows;
		old_io.MouseDelta = new_io.MouseDelta;

		old_io.Ctx = new_io.Ctx;

		old_io.MousePos = new_io.MousePos;
		for (int i = 0; i < 5; ++i)
			old_io.MouseDown[i] = new_io.MouseDown[i];
		old_io.MouseWheel = new_io.MouseWheel;
		old_io.MouseWheelH = new_io.MouseWheelH;
		old_io.MouseSource = new_io.MouseSource;
		old_io.MouseHoveredViewport = new_io.MouseHoveredViewport;
		old_io.KeyCtrl = new_io.KeyCtrl;
		old_io.KeyShift = new_io.KeyShift;
		old_io.KeyAlt = new_io.KeyAlt;
		old_io.KeySuper = new_io.KeySuper;
	}

	void convert(const imgui_style_19222 &new_style, imgui_style_19191 &old_style)
	{
		old_style.Alpha = new_style.Alpha;
		old_style.DisabledAlpha = new_style.DisabledAlpha;
		old_style.WindowPadding = new_style.WindowPadding;
		old_style.WindowRounding = new_style.WindowRounding;
		old_style.WindowBorderSize = new_style.WindowBorderSize;
		old_style.WindowMinSize = new_style.WindowMinSize;
		old_style.WindowTitleAlign = new_style.WindowTitleAlign;
		old_style.WindowMenuButtonPosition = new_style.WindowMenuButtonPosition;
		old_style.ChildRounding = new_style.ChildRounding;
		old_style.ChildBorderSize = new_style.ChildBorderSize;
		old_style.PopupRounding = new_style.PopupRounding;
		old_style.PopupBorderSize = new_style.PopupBorderSize;
		old_style.FramePadding = new_style.FramePadding;
		old_style.FrameRounding = new_style.FrameRounding;
		old_style.FrameBorderSize = new_style.FrameBorderSize;
		old_style.ItemSpacing = new_style.ItemSpacing;
		old_style.ItemInnerSpacing = new_style.ItemInnerSpacing;
		old_style.CellPadding = new_style.CellPadding;
		old_style.TouchExtraPadding = new_style.TouchExtraPadding;
		old_style.IndentSpacing = new_style.IndentSpacing;
		old_style.ColumnsMinSpacing = new_style.ColumnsMinSpacing;
		old_style.ScrollbarSize = new_style.ScrollbarSize;
		old_style.ScrollbarRounding = new_style.ScrollbarRounding;
		old_style.GrabMinSize = new_style.GrabMinSize;
		old_style.GrabRounding = new_style.GrabRounding;
		old_style.LogSliderDeadzone = new_style.LogSliderDeadzone;
		old_style.ImageBorderSize = new_style.ImageBorderSize;
		old_style.TabRounding = new_style.TabRounding;
		old_style.TabBorderSize = new_style.TabBorderSize;
		old_style.TabCloseButtonMinWidthSelected = new_style.TabCloseButtonMinWidthSelected;
		old_style.TabCloseButtonMinWidthUnselected = new_style.TabCloseButtonMinWidthUnselected;
		old_style.TabBarBorderSize = new_style.TabBarBorderSize;
		old_style.TabBarOverlineSize = new_style.TabBarOverlineSize;
		old_style.TableAngledHeadersAngle = new_style.TableAngledHeadersAngle;
		old_style.TableAngledHeadersTextAlign = new_style.TableAngledHeadersTextAlign;
		old_style.ColorButtonPosition = new_style.ColorButtonPosition;
		old_style.ButtonTextAlign = new_style.ButtonTextAlign;
		old_style.SelectableTextAlign = new_style.SelectableTextAlign;
		old_style.SeparatorTextBorderSize = new_style.SeparatorTextBorderSize;
		old_style.SeparatorTextAlign = new_style.SeparatorTextAlign;
		old_style.SeparatorTextPadding = new_style.SeparatorTextPadding;
		old_style.DisplayWindowPadding = new_style.DisplayWindowPadding;
		old_style.DisplaySafeAreaPadding = new_style.DisplaySafeAreaPadding;
		old_style.DockingSeparatorSize = new_style.DockingSeparatorSize;
		old_style.MouseCursorScale = new_style.MouseCursorScale;
		old_style.AntiAliasedLines = new_style.AntiAliasedLines;
		old_style.AntiAliasedLinesUseTex = new_style.AntiAliasedLinesUseTex;
		old_style.AntiAliasedFill = new_style.AntiAliasedFill;
		old_style.CurveTessellationTol = new_style.CurveTessellationTol;
		old_style.CircleTessellationMaxError = new_style.CircleTessellationMaxError;
		for (ImGuiCol i = 0; i < 57; ++i)
			old_style.Colors[i] = new_style.Colors[convert_style_col(i)];
		old_style.HoverStationaryDelay = new_style.HoverStationaryDelay;
		old_style.HoverDelayShort = new_style.HoverDelayShort;
		old_style.HoverDelayNormal = new_style.HoverDelayNormal;
		old_style.HoverFlagsForTooltipMouse = new_style.HoverFlagsForTooltipMouse;
		old_style.HoverFlagsForTooltipNav = new_style.HoverFlagsForTooltipNav;
	}

	void convert(const imgui_list_clipper_19222 &new_clipper, imgui_list_clipper_19191 &old_clipper)
	{
		old_clipper.Ctx = new_clipper.Ctx;
		old_clipper.DisplayStart = new_clipper.DisplayStart;
		old_clipper.DisplayEnd = new_clipper.DisplayEnd;
		old_clipper.ItemsCount = new_clipper.ItemsCount;
		old_clipper.ItemsHeight = new_clipper.ItemsHeight;
		old_clipper.StartPosY = static_cast<float>(new_clipper.StartPosY);
		old_clipper.StartSeekOffsetY = new_clipper.StartSeekOffsetY;
		old_clipper.TempData = new_clipper.TempData;
	}
	void convert(const imgui_list_clipper_19191 &old_clipper, imgui_list_clipper_19222 &new_clipper)
	{
		new_clipper.Ctx = old_clipper.Ctx;
		new_clipper.DisplayStart = old_clipper.DisplayStart;
		new_clipper.DisplayEnd = old_clipper.DisplayEnd;
		new_clipper.ItemsCount = old_clipper.ItemsCount;
		new_clipper.ItemsHeight = old_clipper.ItemsHeight;
		new_clipper.StartPosY = static_cast<double>(old_clipper.StartPosY);
		new_clipper.StartSeekOffsetY = old_clipper.StartSeekOffsetY;
		new_clipper.TempData = old_clipper.TempData;
	}

	static struct imgui_draw_list_19191_internal : public imgui_draw_list_19191 { imgui_draw_list_19222 *orig = nullptr; bool reserved_prims = false; } temp_draw_list;

	imgui_draw_list_19191 *wrap(imgui_draw_list_19222 *new_draw_list)
	{
		temp_draw_list = imgui_draw_list_19191_internal {};
		temp_draw_list.orig = new_draw_list;

		return &temp_draw_list;
	}
	imgui_draw_list_19191 *wrap(imgui_draw_list_19222 *new_draw_list, imgui_draw_list_19191 *old_draw_list)
	{
		if (temp_draw_list.reserved_prims && temp_draw_list.orig == new_draw_list)
		{
			old_draw_list->CmdBuffer = new_draw_list->CmdBuffer;
			old_draw_list->IdxBuffer = new_draw_list->IdxBuffer;
			old_draw_list->VtxBuffer = new_draw_list->VtxBuffer;
			old_draw_list->Flags = new_draw_list->Flags;

			old_draw_list->_VtxCurrentIdx = new_draw_list->_VtxCurrentIdx;
			old_draw_list->_Data = new_draw_list->_Data;
			old_draw_list->_OwnerName = new_draw_list->_OwnerName;
			old_draw_list->_VtxWritePtr = old_draw_list->VtxBuffer.Data + (new_draw_list->_VtxWritePtr - new_draw_list->VtxBuffer.Data);
			old_draw_list->_IdxWritePtr = old_draw_list->IdxBuffer.Data + (new_draw_list->_IdxWritePtr - new_draw_list->IdxBuffer.Data);
			old_draw_list->_ClipRectStack = new_draw_list->_ClipRectStack;
			old_draw_list->_Path = new_draw_list->_Path;
			old_draw_list->_CmdHeader = new_draw_list->_CmdHeader;
			old_draw_list->_Splitter = new_draw_list->_Splitter;
			old_draw_list->_FringeScale = new_draw_list->_FringeScale;
		}

		return old_draw_list;
	}
	imgui_draw_list_19222 *unwrap(imgui_draw_list_19191 *old_draw_list, bool reserved_prims = false)
	{
		imgui_draw_list_19222 *const new_draw_list = temp_draw_list.orig;
		assert(old_draw_list == &temp_draw_list && new_draw_list != nullptr);

		if (temp_draw_list.reserved_prims && temp_draw_list.orig == new_draw_list)
		{
			new_draw_list->CmdBuffer = old_draw_list->CmdBuffer;
			new_draw_list->IdxBuffer = old_draw_list->IdxBuffer;
			new_draw_list->VtxBuffer = old_draw_list->VtxBuffer;
			new_draw_list->Flags = old_draw_list->Flags;

			new_draw_list->_VtxCurrentIdx = old_draw_list->_VtxCurrentIdx;
			new_draw_list->_Data = old_draw_list->_Data;
			new_draw_list->_VtxWritePtr = new_draw_list->VtxBuffer.Data + (old_draw_list->_VtxWritePtr - old_draw_list->VtxBuffer.Data);
			new_draw_list->_IdxWritePtr = new_draw_list->IdxBuffer.Data + (old_draw_list->_IdxWritePtr - old_draw_list->IdxBuffer.Data);
			new_draw_list->_ClipRectStack = old_draw_list->_ClipRectStack;
			new_draw_list->_Path = old_draw_list->_Path;
			new_draw_list->_CmdHeader = old_draw_list->_CmdHeader;
			new_draw_list->_Splitter = old_draw_list->_Splitter;
			new_draw_list->_FringeScale = old_draw_list->_FringeScale;
			new_draw_list->_OwnerName = old_draw_list->_OwnerName;
		}

		temp_draw_list.reserved_prims = reserved_prims;

		return new_draw_list;
	}
}

const imgui_function_table_19191 init_imgui_function_table_19191() { return {
	[]() -> imgui_io_19191 & {
		static imgui_io_19191 io = {};
		convert(g_imgui_function_table_19222.GetIO(), io);
		return io;
	},
	[]() -> imgui_style_19191 & {
		static imgui_style_19191 style = {};
		convert(g_imgui_function_table_19222.GetStyle(), style);
		return style;
	},
	g_imgui_function_table_19222.GetVersion,
	g_imgui_function_table_19222.Begin,
	g_imgui_function_table_19222.End,
	g_imgui_function_table_19222.BeginChild,
	g_imgui_function_table_19222.BeginChild2,
	g_imgui_function_table_19222.EndChild,
	g_imgui_function_table_19222.IsWindowAppearing,
	g_imgui_function_table_19222.IsWindowCollapsed,
	g_imgui_function_table_19222.IsWindowFocused,
	g_imgui_function_table_19222.IsWindowHovered,
	[]() -> imgui_draw_list_19191 * { return wrap(g_imgui_function_table_19222.GetWindowDrawList()); },
	g_imgui_function_table_19222.GetWindowDpiScale,
	g_imgui_function_table_19222.GetWindowPos,
	g_imgui_function_table_19222.GetWindowSize,
	g_imgui_function_table_19222.GetWindowWidth,
	g_imgui_function_table_19222.GetWindowHeight,
	g_imgui_function_table_19222.SetNextWindowPos,
	g_imgui_function_table_19222.SetNextWindowSize,
	g_imgui_function_table_19222.SetNextWindowSizeConstraints,
	g_imgui_function_table_19222.SetNextWindowContentSize,
	g_imgui_function_table_19222.SetNextWindowCollapsed,
	g_imgui_function_table_19222.SetNextWindowFocus,
	g_imgui_function_table_19222.SetNextWindowScroll,
	g_imgui_function_table_19222.SetNextWindowBgAlpha,
	g_imgui_function_table_19222.SetWindowPos,
	g_imgui_function_table_19222.SetWindowSize,
	g_imgui_function_table_19222.SetWindowCollapsed,
	g_imgui_function_table_19222.SetWindowFocus,
	[](float /* scale */) -> void { /* No good implementation for ImGui::SetWindowFontScale */ },
	g_imgui_function_table_19222.SetWindowPos2,
	g_imgui_function_table_19222.SetWindowSize2,
	g_imgui_function_table_19222.SetWindowCollapsed2,
	g_imgui_function_table_19222.SetWindowFocus2,
	g_imgui_function_table_19222.GetScrollX,
	g_imgui_function_table_19222.GetScrollY,
	g_imgui_function_table_19222.SetScrollX,
	g_imgui_function_table_19222.SetScrollY,
	g_imgui_function_table_19222.GetScrollMaxX,
	g_imgui_function_table_19222.GetScrollMaxY,
	g_imgui_function_table_19222.SetScrollHereX,
	g_imgui_function_table_19222.SetScrollHereY,
	g_imgui_function_table_19222.SetScrollFromPosX,
	g_imgui_function_table_19222.SetScrollFromPosY,
	[](imgui_font_19191 *) {
		// Cannot make persistent 'ImFont' here easily, so just always use default
		g_imgui_function_table_19222.PushFont(nullptr, 0.0f);
	},
	g_imgui_function_table_19222.PopFont,
	[](ImGuiCol idx, ImU32 col) -> void { g_imgui_function_table_19222.PushStyleColor(convert_style_col(idx), col); },
	[](ImGuiCol idx, const ImVec4 &col) -> void { g_imgui_function_table_19222.PushStyleColor2(convert_style_col(idx), col); },
	g_imgui_function_table_19222.PopStyleColor,
	[](ImGuiStyleVar idx, float val) -> void { g_imgui_function_table_19222.PushStyleVar(convert_style_var(idx), val); },
	[](ImGuiStyleVar idx, const ImVec2 &val) -> void { g_imgui_function_table_19222.PushStyleVar2(convert_style_var(idx), val); },
	[](ImGuiStyleVar idx, float val) -> void { g_imgui_function_table_19222.PushStyleVarX(convert_style_var(idx), val); },
	[](ImGuiStyleVar idx, float val) -> void { g_imgui_function_table_19222.PushStyleVarY(convert_style_var(idx), val); },
	g_imgui_function_table_19222.PopStyleVar,
	g_imgui_function_table_19222.PushItemFlag,
	g_imgui_function_table_19222.PopItemFlag,
	g_imgui_function_table_19222.PushItemWidth,
	g_imgui_function_table_19222.PopItemWidth,
	g_imgui_function_table_19222.SetNextItemWidth,
	g_imgui_function_table_19222.CalcItemWidth,
	g_imgui_function_table_19222.PushTextWrapPos,
	g_imgui_function_table_19222.PopTextWrapPos,
	[]() -> imgui_font_19191 * {
		static imgui_font_19191 font = {};
		return &font;
	},
	g_imgui_function_table_19222.GetFontSize,
	g_imgui_function_table_19222.GetFontTexUvWhitePixel,
	[](ImGuiCol idx, float alpha_mul) -> ImU32 { return g_imgui_function_table_19222.GetColorU32(convert_style_col(idx), alpha_mul); },
	g_imgui_function_table_19222.GetColorU322,
	g_imgui_function_table_19222.GetColorU323,
	[](ImGuiCol idx) -> const ImVec4 & { return g_imgui_function_table_19222.GetStyleColorVec4(convert_style_col(idx)); },
	g_imgui_function_table_19222.GetCursorScreenPos,
	g_imgui_function_table_19222.SetCursorScreenPos,
	g_imgui_function_table_19222.GetContentRegionAvail,
	g_imgui_function_table_19222.GetCursorPos,
	g_imgui_function_table_19222.GetCursorPosX,
	g_imgui_function_table_19222.GetCursorPosY,
	g_imgui_function_table_19222.SetCursorPos,
	g_imgui_function_table_19222.SetCursorPosX,
	g_imgui_function_table_19222.SetCursorPosY,
	g_imgui_function_table_19222.GetCursorStartPos,
	g_imgui_function_table_19222.Separator,
	g_imgui_function_table_19222.SameLine,
	g_imgui_function_table_19222.NewLine,
	g_imgui_function_table_19222.Spacing,
	g_imgui_function_table_19222.Dummy,
	g_imgui_function_table_19222.Indent,
	g_imgui_function_table_19222.Unindent,
	g_imgui_function_table_19222.BeginGroup,
	g_imgui_function_table_19222.EndGroup,
	g_imgui_function_table_19222.AlignTextToFramePadding,
	g_imgui_function_table_19222.GetTextLineHeight,
	g_imgui_function_table_19222.GetTextLineHeightWithSpacing,
	g_imgui_function_table_19222.GetFrameHeight,
	g_imgui_function_table_19222.GetFrameHeightWithSpacing,
	g_imgui_function_table_19222.PushID,
	g_imgui_function_table_19222.PushID2,
	g_imgui_function_table_19222.PushID3,
	g_imgui_function_table_19222.PushID4,
	g_imgui_function_table_19222.PopID,
	g_imgui_function_table_19222.GetID,
	g_imgui_function_table_19222.GetID2,
	g_imgui_function_table_19222.GetID3,
	g_imgui_function_table_19222.GetID4,
	g_imgui_function_table_19222.TextUnformatted,
	g_imgui_function_table_19222.TextV,
	g_imgui_function_table_19222.TextColoredV,
	g_imgui_function_table_19222.TextDisabledV,
	g_imgui_function_table_19222.TextWrappedV,
	g_imgui_function_table_19222.LabelTextV,
	g_imgui_function_table_19222.BulletTextV,
	g_imgui_function_table_19222.SeparatorText,
	g_imgui_function_table_19222.Button,
	g_imgui_function_table_19222.SmallButton,
	g_imgui_function_table_19222.InvisibleButton,
	g_imgui_function_table_19222.ArrowButton,
	g_imgui_function_table_19222.Checkbox,
	g_imgui_function_table_19222.CheckboxFlags,
	g_imgui_function_table_19222.CheckboxFlags2,
	g_imgui_function_table_19222.RadioButton,
	g_imgui_function_table_19222.RadioButton2,
	g_imgui_function_table_19222.ProgressBar,
	g_imgui_function_table_19222.Bullet,
	g_imgui_function_table_19222.TextLink,
	[](const char *label, const char *url) -> void { ImGui::TextLinkOpenURL(label, url); },
	[](ImTextureID user_texture_id, const ImVec2 &image_size, const ImVec2 &uv0, const ImVec2 &uv1) -> void {
		g_imgui_function_table_19222.Image(ImTextureRef(user_texture_id), image_size, uv0, uv1);
	},
	[](ImTextureID user_texture_id, const ImVec2 &image_size, const ImVec2 &uv0, const ImVec2 &uv1, const ImVec4 &bg_col, const ImVec4 &tint_col) -> void {
		g_imgui_function_table_19222.ImageWithBg(ImTextureRef(user_texture_id), image_size, uv0, uv1, bg_col, tint_col);
	},
	[](const char *str_id, ImTextureID user_texture_id, const ImVec2 &image_size, const ImVec2 &uv0, const ImVec2 &uv1, const ImVec4 &bg_col, const ImVec4 &tint_col) -> bool {
		return g_imgui_function_table_19222.ImageButton(str_id, ImTextureRef(user_texture_id), image_size, uv0, uv1, bg_col, tint_col);
	},
	g_imgui_function_table_19222.BeginCombo,
	g_imgui_function_table_19222.EndCombo,
	g_imgui_function_table_19222.Combo,
	g_imgui_function_table_19222.Combo2,
	g_imgui_function_table_19222.Combo3,
	g_imgui_function_table_19222.DragFloat,
	g_imgui_function_table_19222.DragFloat2,
	g_imgui_function_table_19222.DragFloat3,
	g_imgui_function_table_19222.DragFloat4,
	g_imgui_function_table_19222.DragFloatRange2,
	g_imgui_function_table_19222.DragInt,
	g_imgui_function_table_19222.DragInt2,
	g_imgui_function_table_19222.DragInt3,
	g_imgui_function_table_19222.DragInt4,
	g_imgui_function_table_19222.DragIntRange2,
	g_imgui_function_table_19222.DragScalar,
	g_imgui_function_table_19222.DragScalarN,
	g_imgui_function_table_19222.SliderFloat,
	g_imgui_function_table_19222.SliderFloat2,
	g_imgui_function_table_19222.SliderFloat3,
	g_imgui_function_table_19222.SliderFloat4,
	g_imgui_function_table_19222.SliderAngle,
	g_imgui_function_table_19222.SliderInt,
	g_imgui_function_table_19222.SliderInt2,
	g_imgui_function_table_19222.SliderInt3,
	g_imgui_function_table_19222.SliderInt4,
	g_imgui_function_table_19222.SliderScalar,
	g_imgui_function_table_19222.SliderScalarN,
	g_imgui_function_table_19222.VSliderFloat,
	g_imgui_function_table_19222.VSliderInt,
	g_imgui_function_table_19222.VSliderScalar,
	g_imgui_function_table_19222.InputText,
	g_imgui_function_table_19222.InputTextMultiline,
	g_imgui_function_table_19222.InputTextWithHint,
	g_imgui_function_table_19222.InputFloat,
	g_imgui_function_table_19222.InputFloat2,
	g_imgui_function_table_19222.InputFloat3,
	g_imgui_function_table_19222.InputFloat4,
	g_imgui_function_table_19222.InputInt,
	g_imgui_function_table_19222.InputInt2,
	g_imgui_function_table_19222.InputInt3,
	g_imgui_function_table_19222.InputInt4,
	g_imgui_function_table_19222.InputDouble,
	g_imgui_function_table_19222.InputScalar,
	g_imgui_function_table_19222.InputScalarN,
	g_imgui_function_table_19222.ColorEdit3,
	g_imgui_function_table_19222.ColorEdit4,
	g_imgui_function_table_19222.ColorPicker3,
	g_imgui_function_table_19222.ColorPicker4,
	g_imgui_function_table_19222.ColorButton,
	g_imgui_function_table_19222.SetColorEditOptions,
	g_imgui_function_table_19222.TreeNode,
	g_imgui_function_table_19222.TreeNodeV,
	g_imgui_function_table_19222.TreeNodeV2,
	g_imgui_function_table_19222.TreeNodeEx,
	g_imgui_function_table_19222.TreeNodeExV,
	g_imgui_function_table_19222.TreeNodeExV2,
	g_imgui_function_table_19222.TreePush,
	g_imgui_function_table_19222.TreePush2,
	g_imgui_function_table_19222.TreePop,
	g_imgui_function_table_19222.GetTreeNodeToLabelSpacing,
	g_imgui_function_table_19222.CollapsingHeader,
	g_imgui_function_table_19222.CollapsingHeader2,
	g_imgui_function_table_19222.SetNextItemOpen,
	g_imgui_function_table_19222.SetNextItemStorageID,
	g_imgui_function_table_19222.Selectable,
	g_imgui_function_table_19222.Selectable2,
	g_imgui_function_table_19222.BeginMultiSelect,
	g_imgui_function_table_19222.EndMultiSelect,
	g_imgui_function_table_19222.SetNextItemSelectionUserData,
	g_imgui_function_table_19222.IsItemToggledSelection,
	g_imgui_function_table_19222.BeginListBox,
	g_imgui_function_table_19222.EndListBox,
	g_imgui_function_table_19222.ListBox,
	g_imgui_function_table_19222.ListBox2,
	g_imgui_function_table_19222.PlotLines,
	g_imgui_function_table_19222.PlotLines2,
	g_imgui_function_table_19222.PlotHistogram,
	g_imgui_function_table_19222.PlotHistogram2,
	g_imgui_function_table_19222.Value,
	g_imgui_function_table_19222.Value2,
	g_imgui_function_table_19222.Value3,
	g_imgui_function_table_19222.Value4,
	g_imgui_function_table_19222.BeginMenuBar,
	g_imgui_function_table_19222.EndMenuBar,
	g_imgui_function_table_19222.BeginMainMenuBar,
	g_imgui_function_table_19222.EndMainMenuBar,
	g_imgui_function_table_19222.BeginMenu,
	g_imgui_function_table_19222.EndMenu,
	g_imgui_function_table_19222.MenuItem,
	g_imgui_function_table_19222.MenuItem2,
	g_imgui_function_table_19222.BeginTooltip,
	g_imgui_function_table_19222.EndTooltip,
	g_imgui_function_table_19222.SetTooltipV,
	g_imgui_function_table_19222.BeginItemTooltip,
	g_imgui_function_table_19222.SetItemTooltipV,
	g_imgui_function_table_19222.BeginPopup,
	g_imgui_function_table_19222.BeginPopupModal,
	g_imgui_function_table_19222.EndPopup,
	g_imgui_function_table_19222.OpenPopup,
	g_imgui_function_table_19222.OpenPopup2,
	g_imgui_function_table_19222.OpenPopupOnItemClick,
	g_imgui_function_table_19222.CloseCurrentPopup,
	g_imgui_function_table_19222.BeginPopupContextItem,
	g_imgui_function_table_19222.BeginPopupContextWindow,
	g_imgui_function_table_19222.BeginPopupContextVoid,
	g_imgui_function_table_19222.IsPopupOpen,
	g_imgui_function_table_19222.BeginTable,
	g_imgui_function_table_19222.EndTable,
	g_imgui_function_table_19222.TableNextRow,
	g_imgui_function_table_19222.TableNextColumn,
	g_imgui_function_table_19222.TableSetColumnIndex,
	g_imgui_function_table_19222.TableSetupColumn,
	g_imgui_function_table_19222.TableSetupScrollFreeze,
	g_imgui_function_table_19222.TableHeader,
	g_imgui_function_table_19222.TableHeadersRow,
	g_imgui_function_table_19222.TableAngledHeadersRow,
	g_imgui_function_table_19222.TableGetSortSpecs,
	g_imgui_function_table_19222.TableGetColumnCount,
	g_imgui_function_table_19222.TableGetColumnIndex,
	g_imgui_function_table_19222.TableGetRowIndex,
	g_imgui_function_table_19222.TableGetColumnName,
	g_imgui_function_table_19222.TableGetColumnFlags,
	g_imgui_function_table_19222.TableSetColumnEnabled,
	g_imgui_function_table_19222.TableGetHoveredColumn,
	g_imgui_function_table_19222.TableSetBgColor,
	g_imgui_function_table_19222.Columns,
	g_imgui_function_table_19222.NextColumn,
	g_imgui_function_table_19222.GetColumnIndex,
	g_imgui_function_table_19222.GetColumnWidth,
	g_imgui_function_table_19222.SetColumnWidth,
	g_imgui_function_table_19222.GetColumnOffset,
	g_imgui_function_table_19222.SetColumnOffset,
	g_imgui_function_table_19222.GetColumnsCount,
	[](const char *str_id, ImGuiTabBarFlags flags) -> bool { return g_imgui_function_table_19222.BeginTabBar(str_id, convert_tab_bar_flags(flags)); },
	g_imgui_function_table_19222.EndTabBar,
	g_imgui_function_table_19222.BeginTabItem,
	g_imgui_function_table_19222.EndTabItem,
	g_imgui_function_table_19222.TabItemButton,
	g_imgui_function_table_19222.SetTabItemClosed,
	g_imgui_function_table_19222.DockSpace,
	g_imgui_function_table_19222.SetNextWindowDockID,
	g_imgui_function_table_19222.SetNextWindowClass,
	g_imgui_function_table_19222.GetWindowDockID,
	g_imgui_function_table_19222.IsWindowDocked,
	g_imgui_function_table_19222.BeginDragDropSource,
	g_imgui_function_table_19222.SetDragDropPayload,
	g_imgui_function_table_19222.EndDragDropSource,
	g_imgui_function_table_19222.BeginDragDropTarget,
	g_imgui_function_table_19222.AcceptDragDropPayload,
	g_imgui_function_table_19222.EndDragDropTarget,
	g_imgui_function_table_19222.GetDragDropPayload,
	g_imgui_function_table_19222.BeginDisabled,
	g_imgui_function_table_19222.EndDisabled,
	g_imgui_function_table_19222.PushClipRect,
	g_imgui_function_table_19222.PopClipRect,
	g_imgui_function_table_19222.SetItemDefaultFocus,
	g_imgui_function_table_19222.SetKeyboardFocusHere,
	g_imgui_function_table_19222.SetNavCursorVisible,
	g_imgui_function_table_19222.SetNextItemAllowOverlap,
	g_imgui_function_table_19222.IsItemHovered,
	g_imgui_function_table_19222.IsItemActive,
	g_imgui_function_table_19222.IsItemFocused,
	g_imgui_function_table_19222.IsItemClicked,
	g_imgui_function_table_19222.IsItemVisible,
	g_imgui_function_table_19222.IsItemEdited,
	g_imgui_function_table_19222.IsItemActivated,
	g_imgui_function_table_19222.IsItemDeactivated,
	g_imgui_function_table_19222.IsItemDeactivatedAfterEdit,
	g_imgui_function_table_19222.IsItemToggledOpen,
	g_imgui_function_table_19222.IsAnyItemHovered,
	g_imgui_function_table_19222.IsAnyItemActive,
	g_imgui_function_table_19222.IsAnyItemFocused,
	g_imgui_function_table_19222.GetItemID,
	g_imgui_function_table_19222.GetItemRectMin,
	g_imgui_function_table_19222.GetItemRectMax,
	g_imgui_function_table_19222.GetItemRectSize,
	[](ImGuiViewport *viewport) -> imgui_draw_list_19191 * { return wrap(g_imgui_function_table_19222.GetBackgroundDrawList(viewport)); },
	[](ImGuiViewport *viewport) -> imgui_draw_list_19191 * { return wrap(g_imgui_function_table_19222.GetForegroundDrawList(viewport)); },
	g_imgui_function_table_19222.IsRectVisible,
	g_imgui_function_table_19222.IsRectVisible2,
	g_imgui_function_table_19222.GetTime,
	g_imgui_function_table_19222.GetFrameCount,
	g_imgui_function_table_19222.GetDrawListSharedData,
	[](ImGuiCol idx) -> const char * { return g_imgui_function_table_19222.GetStyleColorName(convert_style_col(idx)); },
	g_imgui_function_table_19222.SetStateStorage,
	g_imgui_function_table_19222.GetStateStorage,
	g_imgui_function_table_19222.CalcTextSize,
	g_imgui_function_table_19222.ColorConvertU32ToFloat4,
	g_imgui_function_table_19222.ColorConvertFloat4ToU32,
	g_imgui_function_table_19222.ColorConvertRGBtoHSV,
	g_imgui_function_table_19222.ColorConvertHSVtoRGB,
	g_imgui_function_table_19222.IsKeyDown,
	g_imgui_function_table_19222.IsKeyPressed,
	g_imgui_function_table_19222.IsKeyReleased,
	g_imgui_function_table_19222.IsKeyChordPressed,
	g_imgui_function_table_19222.GetKeyPressedAmount,
	g_imgui_function_table_19222.GetKeyName,
	g_imgui_function_table_19222.SetNextFrameWantCaptureKeyboard,
	g_imgui_function_table_19222.Shortcut,
	g_imgui_function_table_19222.SetNextItemShortcut,
	g_imgui_function_table_19222.SetItemKeyOwner,
	g_imgui_function_table_19222.IsMouseDown,
	g_imgui_function_table_19222.IsMouseClicked,
	g_imgui_function_table_19222.IsMouseReleased,
	g_imgui_function_table_19222.IsMouseDoubleClicked,
	g_imgui_function_table_19222.IsMouseReleasedWithDelay,
	g_imgui_function_table_19222.GetMouseClickedCount,
	g_imgui_function_table_19222.IsMouseHoveringRect,
	g_imgui_function_table_19222.IsMousePosValid,
	g_imgui_function_table_19222.IsAnyMouseDown,
	g_imgui_function_table_19222.GetMousePos,
	g_imgui_function_table_19222.GetMousePosOnOpeningCurrentPopup,
	g_imgui_function_table_19222.IsMouseDragging,
	g_imgui_function_table_19222.GetMouseDragDelta,
	g_imgui_function_table_19222.ResetMouseDragDelta,
	g_imgui_function_table_19222.GetMouseCursor,
	g_imgui_function_table_19222.SetMouseCursor,
	g_imgui_function_table_19222.SetNextFrameWantCaptureMouse,
	g_imgui_function_table_19222.GetClipboardText,
	g_imgui_function_table_19222.SetClipboardText,
	g_imgui_function_table_19222.SetAllocatorFunctions,
	g_imgui_function_table_19222.GetAllocatorFunctions,
	g_imgui_function_table_19222.MemAlloc,
	g_imgui_function_table_19222.MemFree,
	[](const imgui_storage_19191 *_this, ImGuiID key, int default_val) -> int { return _this->GetInt(key, default_val); },
	[](imgui_storage_19191 *_this, ImGuiID key, int val) -> void { _this->SetInt(key, val); },
	[](const imgui_storage_19191 *_this, ImGuiID key, bool default_val) -> bool { return _this->GetBool(key, default_val); },
	[](imgui_storage_19191 *_this, ImGuiID key, bool val) -> void { _this->SetBool(key, val); },
	[](const imgui_storage_19191 *_this, ImGuiID key, float default_val) -> float { return _this->GetFloat(key, default_val); },
	[](imgui_storage_19191 *_this, ImGuiID key, float val) -> void { _this->SetFloat(key, val); },
	[](const imgui_storage_19191 *_this, ImGuiID key) -> void * { return _this->GetVoidPtr(key); },
	[](imgui_storage_19191 *_this, ImGuiID key, void *val) -> void { _this->SetVoidPtr(key, val); },
	[](imgui_storage_19191 *_this, ImGuiID key, int default_val) -> int * { return _this->GetIntRef(key, default_val); },
	[](imgui_storage_19191 *_this, ImGuiID key, bool default_val) -> bool * { return _this->GetBoolRef(key, default_val); },
	[](imgui_storage_19191 *_this, ImGuiID key, float default_val) -> float * { return _this->GetFloatRef(key, default_val); },
	[](imgui_storage_19191 *_this, ImGuiID key, void *default_val) -> void ** { return _this->GetVoidPtrRef(key, default_val); },
	[](imgui_storage_19191 *_this) -> void { _this->BuildSortByKey(); },
	[](imgui_storage_19191 *_this, int val) -> void { _this->SetAllInt(val); },
	[](imgui_list_clipper_19191 *_this) -> void {
		new(_this) imgui_list_clipper_19191();
		memset(_this, 0, sizeof(*_this));
	},
	[](imgui_list_clipper_19191 *_this) -> void {
		imgui_list_clipper_19222 temp;
		convert(*_this, temp);
		g_imgui_function_table_19222.ImGuiListClipper_End(&temp);
		_this->~imgui_list_clipper_19191();
	},
	[](imgui_list_clipper_19191 *_this, int items_count, float items_height) -> void {
		imgui_list_clipper_19222 temp;
		convert(*_this, temp);
		g_imgui_function_table_19222.ImGuiListClipper_Begin(&temp, items_count, items_height);
		convert(temp, *_this);
		temp.TempData = nullptr; // Prevent 'ImGuiListClipper' destructor from doing anything
	},
	[](imgui_list_clipper_19191 *_this) -> void {
		imgui_list_clipper_19222 temp;
		convert(*_this, temp);
		g_imgui_function_table_19222.ImGuiListClipper_End(&temp);
		convert(temp, *_this);
	},
	[](imgui_list_clipper_19191 *_this) -> bool {
		imgui_list_clipper_19222 temp;
		convert(*_this, temp);
		const bool result = g_imgui_function_table_19222.ImGuiListClipper_Step(&temp);
		convert(temp, *_this);
		temp.TempData = nullptr;
		return result;
	},
	[](imgui_list_clipper_19191 *_this, int item_begin, int item_end) -> void {
		imgui_list_clipper_19222 temp;
		convert(*_this, temp);
		g_imgui_function_table_19222.ImGuiListClipper_IncludeItemsByIndex(&temp, item_begin, item_end);
		convert(temp, *_this);
		temp.TempData = nullptr;
	},
	[](imgui_list_clipper_19191 *_this, int item_index) -> void {
		imgui_list_clipper_19222 temp;
		convert(*_this, temp);
		g_imgui_function_table_19222.ImGuiListClipper_SeekCursorForItem(&temp, item_index);
		convert(temp, *_this);
		temp.TempData = nullptr;
	},
	[](imgui_draw_list_19191 *_this, ImDrawListSharedData *shared_data) -> void {
		new(_this) imgui_draw_list_19191();
		_this->_Data = shared_data;
	},
	[](imgui_draw_list_19191 *_this) -> void {
		_this->~imgui_draw_list_19191();
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &clip_rect_min, const ImVec2 &clip_rect_max, bool intersect_with_current_clip_rect) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PushClipRect(draw_list_19222, clip_rect_min, clip_rect_max, intersect_with_current_clip_rect);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PushClipRectFullScreen(draw_list_19222);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PopClipRect(draw_list_19222);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, ImTextureID texture_id) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PushTexture(draw_list_19222, ImTextureRef(texture_id));
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PopTexture(draw_list_19222);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p1, const ImVec2 &p2, ImU32 col, float thickness) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddLine(draw_list_19222, p1, p2, col, thickness);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p_min, const ImVec2 &p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddRect(draw_list_19222, p_min, p_max, col, rounding, flags, thickness);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p_min, const ImVec2 &p_max, ImU32 col, float rounding, ImDrawFlags flags) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddRectFilled(draw_list_19222, p_min, p_max, col, rounding, flags);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p_min, const ImVec2 &p_max, ImU32 col_upr_left, ImU32 col_upr_right, ImU32 col_bot_right, ImU32 col_bot_left) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddRectFilledMultiColor(draw_list_19222, p_min, p_max, col_upr_left, col_upr_right, col_bot_right, col_bot_left);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col, float thickness) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddQuad(draw_list_19222, p1, p2, p3, p4, col, thickness);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddQuadFilled(draw_list_19222, p1, p2, p3, p4, col);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col, float thickness) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddTriangle(draw_list_19222, p1, p2, p3, col, thickness);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddTriangleFilled(draw_list_19222, p1, p2, p3, col);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments, float thickness) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddCircle(draw_list_19222, center, radius, col, num_segments, thickness);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddCircleFilled(draw_list_19222, center, radius, col, num_segments);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments, float thickness) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddNgon(draw_list_19222, center, radius, col, num_segments, thickness);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddNgonFilled(draw_list_19222, center, radius, col, num_segments);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &center, const ImVec2 &radius, ImU32 col, float rot, int num_segments, float thickness) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddEllipse(draw_list_19222, center, radius, col, rot, num_segments, thickness);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &center, const ImVec2 &radius, ImU32 col, float rot, int num_segments) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddEllipseFilled(draw_list_19222, center, radius, col, rot, num_segments);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &pos, ImU32 col, const char *text_begin, const char *text_end) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddText(draw_list_19222, pos, col, text_begin, text_end);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, imgui_font_19191 *, float font_size, const ImVec2 &pos, ImU32 col, const char *text_begin, const char *text_end, float wrap_width, const ImVec4 *cpu_fine_clip_rect) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddText2(draw_list_19222, nullptr, font_size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip_rect);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col, float thickness, int num_segments) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddBezierCubic(draw_list_19222, p1, p2, p3, p4, col, thickness, num_segments);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col, float thickness, int num_segments) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddBezierQuadratic(draw_list_19222, p1, p2, p3, col, thickness, num_segments);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 *points, int num_points, ImU32 col, ImDrawFlags flags, float thickness) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddPolyline(draw_list_19222, points, num_points, col, flags, thickness);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 *points, int num_points, ImU32 col) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddConvexPolyFilled(draw_list_19222, points, num_points, col);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 *points, int num_points, ImU32 col) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddConcavePolyFilled(draw_list_19222, points, num_points, col);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, ImTextureID user_texture_id, const ImVec2 &p_min, const ImVec2 &p_max, const ImVec2 &uv_min, const ImVec2 &uv_max, ImU32 col) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddImage(draw_list_19222, user_texture_id, p_min, p_max, uv_min, uv_max, col);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, ImTextureID user_texture_id, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, const ImVec2 &uv1, const ImVec2 &uv2, const ImVec2 &uv3, const ImVec2 &uv4, ImU32 col) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddImageQuad(draw_list_19222, user_texture_id, p1, p2, p3, p4, uv1, uv2, uv3, uv4, col);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, ImTextureID user_texture_id, const ImVec2 &p_min, const ImVec2 &p_max, const ImVec2 &uv_min, const ImVec2 &uv_max, ImU32 col, float rounding, ImDrawFlags flags) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddImageRounded(draw_list_19222, user_texture_id, p_min, p_max, uv_min, uv_max, col, rounding, flags);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &center, float radius, float a_min, float a_max, int num_segments) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PathArcTo(draw_list_19222, center, radius, a_min, a_max, num_segments);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &center, float radius, int a_min_of_12, int a_max_of_12) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PathArcToFast(draw_list_19222, center, radius, a_min_of_12, a_max_of_12);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &center, const ImVec2 &radius, float rot, float a_min, float a_max, int num_segments) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PathEllipticalArcTo(draw_list_19222, center, radius, rot, a_min, a_max, num_segments);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, int num_segments) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PathBezierCubicCurveTo(draw_list_19222, p2, p3, p4, num_segments);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &p2, const ImVec2 &p3, int num_segments) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PathBezierQuadraticCurveTo(draw_list_19222, p2, p3, num_segments);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &rect_min, const ImVec2 &rect_max, float rounding, ImDrawFlags flags) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PathRect(draw_list_19222, rect_min, rect_max, rounding, flags);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, ImDrawCallback callback, void *userdata, size_t userdata_size) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddCallback(draw_list_19222, callback, userdata, userdata_size);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_AddDrawCmd(draw_list_19222);
		wrap(draw_list_19222, _this);
	},
	[](const imgui_draw_list_19191 *) -> imgui_draw_list_19191 * {
		// Cloning does not work with just a single global draw list for conversion
		return nullptr;
	},
	[](imgui_draw_list_19191 *_this, int idx_count, int vtx_count) -> void {
		const auto draw_list_19222 = unwrap(_this, true);
		g_imgui_function_table_19222.ImDrawList_PrimReserve(draw_list_19222, idx_count, vtx_count);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, int idx_count, int vtx_count) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PrimUnreserve(draw_list_19222, idx_count, vtx_count);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &a, const ImVec2 &b, ImU32 col) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PrimRect(draw_list_19222, a, b, col);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &a, const ImVec2 &b, const ImVec2 &uv_a, const ImVec2 &uv_b, ImU32 col) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PrimRectUV(draw_list_19222, a, b, uv_a, uv_b, col);
		wrap(draw_list_19222, _this);
	},
	[](imgui_draw_list_19191 *_this, const ImVec2 &a, const ImVec2 &b, const ImVec2 &c, const ImVec2 &d, const ImVec2 &uv_a, const ImVec2 &uv_b, const ImVec2 &uv_c, const ImVec2 &uv_d, ImU32 col) -> void {
		const auto draw_list_19222 = unwrap(_this);
		g_imgui_function_table_19222.ImDrawList_PrimQuadUV(draw_list_19222, a, b, c, d, uv_a, uv_b, uv_c, uv_d, col);
		wrap(draw_list_19222, _this);
	},
	[](imgui_font_19191 *_this) -> void {
		new(_this) imgui_font_19191();
		_this->FallbackAdvanceX = 0.0f;
		_this->FontSize = 0.0f;
		_this->FallbackAdvanceX = 0.0f;
		_this->FallbackGlyph = nullptr;
		_this->ContainerAtlas = nullptr;
		_this->Sources = nullptr;
		_this->SourcesCount = 0;
		_this->EllipsisCharCount = 0;
		_this->EllipsisChar = 0;
		_this->FallbackChar = 0;
		_this->EllipsisWidth = 0.0f;
		_this->EllipsisCharStep = 0.0f;
		_this->Scale = 1.0f;
		_this->Ascent = 0.0f;
		_this->Descent = 0.0f;
		_this->MetricsTotalSurface = 0;
		_this->DirtyLookupTables = false;
		memset(_this->Used8kPagesMap, 0, sizeof(_this->Used8kPagesMap));
	},
	[](imgui_font_19191 *_this) -> void {
		_this->~imgui_font_19191();
	},
	[](imgui_font_19191 *, ImWchar) -> ImFontGlyph * {
		return nullptr;
	},
	[](imgui_font_19191 *, ImWchar) -> ImFontGlyph * {
		return nullptr;
	},
	[](imgui_font_19191 *, float, float, float, const char *, const char *, const char **) -> ImVec2 {
		return ImVec2();
	},
	[](imgui_font_19191 *, float, const char *, const char *, float) -> const char * {
		return nullptr;
	},
	[](imgui_font_19191 *, imgui_draw_list_19191 *draw_list, float size, const ImVec2 &pos, ImU32 col, ImWchar c) -> void {
		const auto draw_list_19222 = unwrap(draw_list);
		g_imgui_function_table_19222.ImDrawList_AddText2(draw_list_19222, nullptr, size, pos, col, reinterpret_cast<const char *>(&c), reinterpret_cast<const char *>(&c) + 1, 0.0f, nullptr);
		wrap(draw_list_19222, draw_list);
	},
	[](imgui_font_19191 *, imgui_draw_list_19191 *draw_list, float size, const ImVec2 &pos, ImU32 col, const ImVec4 &clip_rect, const char *text_begin, const char *text_end, float wrap_width, bool cpu_fine_clip) -> void {
		const auto draw_list_19222 = unwrap(draw_list);
		g_imgui_function_table_19222.ImDrawList_AddText2(draw_list_19222, nullptr, size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip ? &clip_rect : nullptr);
		wrap(draw_list_19222, draw_list);
	},

}; }

#endif
