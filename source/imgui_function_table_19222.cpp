/*
 * Copyright (C) 2021 Patrick Mours
 * Copyright (C) 2014-2025 Omar Cornut
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON && RESHADE_GUI

#include <new>
#include "imgui_function_table_19222.hpp"

namespace
{
	auto convert_style_col(ImGuiCol old_value) -> ImGuiCol
	{
		if (old_value <= 55) // ImGuiCol_DragDropTarget
			return old_value;
		else
			return old_value + 2;
	}
	auto convert_style_var(ImGuiStyleVar old_value) -> ImGuiStyleVar
	{
		if (old_value <= 19) // ImGuiStyleVar_ScrollbarRounding
			return old_value;
		else
			return old_value + 1;
	}

	void convert(const imgui_io_19250 &new_io, imgui_io_19222 &old_io)
	{
		old_io.ConfigFlags = new_io.ConfigFlags;
		old_io.BackendFlags = new_io.BackendFlags;
		old_io.DisplaySize = new_io.DisplaySize;
		old_io.DisplayFramebufferScale = new_io.DisplayFramebufferScale;
		old_io.DeltaTime = new_io.DeltaTime;
		old_io.IniSavingRate = new_io.IniSavingRate;
		old_io.IniFilename = new_io.IniFilename;
		old_io.LogFilename = new_io.LogFilename;
		old_io.UserData = new_io.UserData;

		// It's not safe to access the internal fields of 'FontAtlas'
		old_io.Fonts = new_io.Fonts;
		old_io.FontDefault = nullptr;
		old_io.FontAllowUserScaling = new_io.FontAllowUserScaling;

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
		old_io.ConfigViewportsPlatformFocusSetsImGuiFocus = new_io.ConfigViewportsPlatformFocusSetsImGuiFocus;

		old_io.ConfigDpiScaleFonts = new_io.ConfigDpiScaleFonts;
		old_io.ConfigDpiScaleViewports = new_io.ConfigDpiScaleViewports;

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

	void convert(const imgui_style_19250 &new_style, imgui_style_19222 &old_style)
	{
		old_style.FontSizeBase = new_style.FontSizeBase;
		old_style.FontScaleMain = new_style.FontScaleMain;
		old_style.FontScaleDpi = new_style.FontScaleDpi;

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
		old_style.TabMinWidthBase = new_style.TabMinWidthBase;
		old_style.TabMinWidthShrink = new_style.TabMinWidthShrink;
		old_style.TabCloseButtonMinWidthSelected = new_style.TabCloseButtonMinWidthSelected;
		old_style.TabCloseButtonMinWidthUnselected = new_style.TabCloseButtonMinWidthUnselected;
		old_style.TabBarBorderSize = new_style.TabBarBorderSize;
		old_style.TabBarOverlineSize = new_style.TabBarOverlineSize;
		old_style.TableAngledHeadersAngle = new_style.TableAngledHeadersAngle;
		old_style.TableAngledHeadersTextAlign = new_style.TableAngledHeadersTextAlign;
		old_style.TreeLinesFlags = new_style.TreeLinesFlags;
		old_style.TreeLinesSize = new_style.TreeLinesSize;
		old_style.TreeLinesRounding = new_style.TreeLinesRounding;
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
		for (ImGuiCol i = 0; i < 60; ++i)
			old_style.Colors[i] = new_style.Colors[convert_style_col(i)];
		old_style.HoverStationaryDelay = new_style.HoverStationaryDelay;
		old_style.HoverDelayShort = new_style.HoverDelayShort;
		old_style.HoverDelayNormal = new_style.HoverDelayNormal;
		old_style.HoverFlagsForTooltipMouse = new_style.HoverFlagsForTooltipMouse;
		old_style.HoverFlagsForTooltipNav = new_style.HoverFlagsForTooltipNav;
	}

	void convert(const imgui_list_clipper_19250 &new_clipper, imgui_list_clipper_19222 &old_clipper)
	{
		old_clipper.Ctx = new_clipper.Ctx;
		old_clipper.DisplayStart = new_clipper.DisplayStart;
		old_clipper.DisplayEnd = new_clipper.DisplayEnd;
		old_clipper.ItemsCount = new_clipper.ItemsCount;
		old_clipper.ItemsHeight = new_clipper.ItemsHeight;
		old_clipper.StartPosY = new_clipper.StartPosY;
		old_clipper.StartSeekOffsetY = new_clipper.StartSeekOffsetY;
		old_clipper.TempData = new_clipper.TempData;
	}
	void convert(const imgui_list_clipper_19222 &old_clipper, imgui_list_clipper_19250 &new_clipper)
	{
		new_clipper.Ctx = old_clipper.Ctx;
		new_clipper.DisplayStart = old_clipper.DisplayStart;
		new_clipper.DisplayEnd = old_clipper.DisplayEnd;
		new_clipper.ItemsCount = old_clipper.ItemsCount;
		new_clipper.ItemsHeight = old_clipper.ItemsHeight;
		new_clipper.StartPosY = old_clipper.StartPosY;
		new_clipper.StartSeekOffsetY = old_clipper.StartSeekOffsetY;
		new_clipper.TempData = old_clipper.TempData;
		new_clipper.Flags = ImGuiListClipperFlags_None;
	}
}

const imgui_function_table_19222 init_imgui_function_table_19222() { return {
	[]() -> imgui_io_19222 & {
		static imgui_io_19222 io = {};
		convert(g_imgui_function_table_19250.GetIO(), io);
		return io;
	},
	[]() -> imgui_style_19222 & {
		static imgui_style_19222 style = {};
		convert(g_imgui_function_table_19250.GetStyle(), style);
		return style;
	},
	g_imgui_function_table_19250.GetVersion,
	g_imgui_function_table_19250.Begin,
	g_imgui_function_table_19250.End,
	g_imgui_function_table_19250.BeginChild,
	g_imgui_function_table_19250.BeginChild2,
	g_imgui_function_table_19250.EndChild,
	g_imgui_function_table_19250.IsWindowAppearing,
	g_imgui_function_table_19250.IsWindowCollapsed,
	g_imgui_function_table_19250.IsWindowFocused,
	g_imgui_function_table_19250.IsWindowHovered,
	g_imgui_function_table_19250.GetWindowDrawList,
	g_imgui_function_table_19250.GetWindowDpiScale,
	g_imgui_function_table_19250.GetWindowPos,
	g_imgui_function_table_19250.GetWindowSize,
	g_imgui_function_table_19250.GetWindowWidth,
	g_imgui_function_table_19250.GetWindowHeight,
	g_imgui_function_table_19250.SetNextWindowPos,
	g_imgui_function_table_19250.SetNextWindowSize,
	g_imgui_function_table_19250.SetNextWindowSizeConstraints,
	g_imgui_function_table_19250.SetNextWindowContentSize,
	g_imgui_function_table_19250.SetNextWindowCollapsed,
	g_imgui_function_table_19250.SetNextWindowFocus,
	g_imgui_function_table_19250.SetNextWindowScroll,
	g_imgui_function_table_19250.SetNextWindowBgAlpha,
	g_imgui_function_table_19250.SetWindowPos,
	g_imgui_function_table_19250.SetWindowSize,
	g_imgui_function_table_19250.SetWindowCollapsed,
	g_imgui_function_table_19250.SetWindowFocus,
	g_imgui_function_table_19250.SetWindowPos2,
	g_imgui_function_table_19250.SetWindowSize2,
	g_imgui_function_table_19250.SetWindowCollapsed2,
	g_imgui_function_table_19250.SetWindowFocus2,
	g_imgui_function_table_19250.GetScrollX,
	g_imgui_function_table_19250.GetScrollY,
	g_imgui_function_table_19250.SetScrollX,
	g_imgui_function_table_19250.SetScrollY,
	g_imgui_function_table_19250.GetScrollMaxX,
	g_imgui_function_table_19250.GetScrollMaxY,
	g_imgui_function_table_19250.SetScrollHereX,
	g_imgui_function_table_19250.SetScrollHereY,
	g_imgui_function_table_19250.SetScrollFromPosX,
	g_imgui_function_table_19250.SetScrollFromPosY,
	g_imgui_function_table_19250.PushFont,
	g_imgui_function_table_19250.PopFont,
	g_imgui_function_table_19250.GetFont,
	g_imgui_function_table_19250.GetFontSize,
	g_imgui_function_table_19250.GetFontBaked,
	[](ImGuiCol idx, ImU32 col) -> void { g_imgui_function_table_19250.PushStyleColor(convert_style_col(idx), col); },
	[](ImGuiCol idx, const ImVec4 &col) -> void { g_imgui_function_table_19250.PushStyleColor2(convert_style_col(idx), col); },
	g_imgui_function_table_19250.PopStyleColor,
	[](ImGuiStyleVar idx, float val) -> void { g_imgui_function_table_19250.PushStyleVar(convert_style_var(idx), val); },
	[](ImGuiStyleVar idx, const ImVec2 &val) -> void { g_imgui_function_table_19250.PushStyleVar2(convert_style_var(idx), val); },
	[](ImGuiStyleVar idx, float val) -> void { g_imgui_function_table_19250.PushStyleVarX(convert_style_var(idx), val); },
	[](ImGuiStyleVar idx, float val) -> void { g_imgui_function_table_19250.PushStyleVarY(convert_style_var(idx), val); },
	g_imgui_function_table_19250.PopStyleVar,
	g_imgui_function_table_19250.PushItemFlag,
	g_imgui_function_table_19250.PopItemFlag,
	g_imgui_function_table_19250.PushItemWidth,
	g_imgui_function_table_19250.PopItemWidth,
	g_imgui_function_table_19250.SetNextItemWidth,
	g_imgui_function_table_19250.CalcItemWidth,
	g_imgui_function_table_19250.PushTextWrapPos,
	g_imgui_function_table_19250.PopTextWrapPos,
	g_imgui_function_table_19250.GetFontTexUvWhitePixel,
	[](ImGuiCol idx, float alpha_mul) -> ImU32 { return g_imgui_function_table_19250.GetColorU32(convert_style_col(idx), alpha_mul); },
	g_imgui_function_table_19250.GetColorU322,
	g_imgui_function_table_19250.GetColorU323,
	[](ImGuiCol idx) -> const ImVec4 &{ return g_imgui_function_table_19250.GetStyleColorVec4(convert_style_col(idx)); },
	g_imgui_function_table_19250.GetCursorScreenPos,
	g_imgui_function_table_19250.SetCursorScreenPos,
	g_imgui_function_table_19250.GetContentRegionAvail,
	g_imgui_function_table_19250.GetCursorPos,
	g_imgui_function_table_19250.GetCursorPosX,
	g_imgui_function_table_19250.GetCursorPosY,
	g_imgui_function_table_19250.SetCursorPos,
	g_imgui_function_table_19250.SetCursorPosX,
	g_imgui_function_table_19250.SetCursorPosY,
	g_imgui_function_table_19250.GetCursorStartPos,
	g_imgui_function_table_19250.Separator,
	g_imgui_function_table_19250.SameLine,
	g_imgui_function_table_19250.NewLine,
	g_imgui_function_table_19250.Spacing,
	g_imgui_function_table_19250.Dummy,
	g_imgui_function_table_19250.Indent,
	g_imgui_function_table_19250.Unindent,
	g_imgui_function_table_19250.BeginGroup,
	g_imgui_function_table_19250.EndGroup,
	g_imgui_function_table_19250.AlignTextToFramePadding,
	g_imgui_function_table_19250.GetTextLineHeight,
	g_imgui_function_table_19250.GetTextLineHeightWithSpacing,
	g_imgui_function_table_19250.GetFrameHeight,
	g_imgui_function_table_19250.GetFrameHeightWithSpacing,
	g_imgui_function_table_19250.PushID,
	g_imgui_function_table_19250.PushID2,
	g_imgui_function_table_19250.PushID3,
	g_imgui_function_table_19250.PushID4,
	g_imgui_function_table_19250.PopID,
	g_imgui_function_table_19250.GetID,
	g_imgui_function_table_19250.GetID2,
	g_imgui_function_table_19250.GetID3,
	g_imgui_function_table_19250.GetID4,
	g_imgui_function_table_19250.TextUnformatted,
	g_imgui_function_table_19250.TextV,
	g_imgui_function_table_19250.TextColoredV,
	g_imgui_function_table_19250.TextDisabledV,
	g_imgui_function_table_19250.TextWrappedV,
	g_imgui_function_table_19250.LabelTextV,
	g_imgui_function_table_19250.BulletTextV,
	g_imgui_function_table_19250.SeparatorText,
	g_imgui_function_table_19250.Button,
	g_imgui_function_table_19250.SmallButton,
	g_imgui_function_table_19250.InvisibleButton,
	g_imgui_function_table_19250.ArrowButton,
	g_imgui_function_table_19250.Checkbox,
	g_imgui_function_table_19250.CheckboxFlags,
	g_imgui_function_table_19250.CheckboxFlags2,
	g_imgui_function_table_19250.RadioButton,
	g_imgui_function_table_19250.RadioButton2,
	g_imgui_function_table_19250.ProgressBar,
	g_imgui_function_table_19250.Bullet,
	g_imgui_function_table_19250.TextLink,
	g_imgui_function_table_19250.TextLinkOpenURL,
	g_imgui_function_table_19250.Image,
	g_imgui_function_table_19250.ImageWithBg,
	g_imgui_function_table_19250.ImageButton,
	g_imgui_function_table_19250.BeginCombo,
	g_imgui_function_table_19250.EndCombo,
	g_imgui_function_table_19250.Combo,
	g_imgui_function_table_19250.Combo2,
	g_imgui_function_table_19250.Combo3,
	g_imgui_function_table_19250.DragFloat,
	g_imgui_function_table_19250.DragFloat2,
	g_imgui_function_table_19250.DragFloat3,
	g_imgui_function_table_19250.DragFloat4,
	g_imgui_function_table_19250.DragFloatRange2,
	g_imgui_function_table_19250.DragInt,
	g_imgui_function_table_19250.DragInt2,
	g_imgui_function_table_19250.DragInt3,
	g_imgui_function_table_19250.DragInt4,
	g_imgui_function_table_19250.DragIntRange2,
	g_imgui_function_table_19250.DragScalar,
	g_imgui_function_table_19250.DragScalarN,
	g_imgui_function_table_19250.SliderFloat,
	g_imgui_function_table_19250.SliderFloat2,
	g_imgui_function_table_19250.SliderFloat3,
	g_imgui_function_table_19250.SliderFloat4,
	g_imgui_function_table_19250.SliderAngle,
	g_imgui_function_table_19250.SliderInt,
	g_imgui_function_table_19250.SliderInt2,
	g_imgui_function_table_19250.SliderInt3,
	g_imgui_function_table_19250.SliderInt4,
	g_imgui_function_table_19250.SliderScalar,
	g_imgui_function_table_19250.SliderScalarN,
	g_imgui_function_table_19250.VSliderFloat,
	g_imgui_function_table_19250.VSliderInt,
	g_imgui_function_table_19250.VSliderScalar,
	g_imgui_function_table_19250.InputText,
	g_imgui_function_table_19250.InputTextMultiline,
	g_imgui_function_table_19250.InputTextWithHint,
	g_imgui_function_table_19250.InputFloat,
	g_imgui_function_table_19250.InputFloat2,
	g_imgui_function_table_19250.InputFloat3,
	g_imgui_function_table_19250.InputFloat4,
	g_imgui_function_table_19250.InputInt,
	g_imgui_function_table_19250.InputInt2,
	g_imgui_function_table_19250.InputInt3,
	g_imgui_function_table_19250.InputInt4,
	g_imgui_function_table_19250.InputDouble,
	g_imgui_function_table_19250.InputScalar,
	g_imgui_function_table_19250.InputScalarN,
	g_imgui_function_table_19250.ColorEdit3,
	g_imgui_function_table_19250.ColorEdit4,
	g_imgui_function_table_19250.ColorPicker3,
	g_imgui_function_table_19250.ColorPicker4,
	g_imgui_function_table_19250.ColorButton,
	g_imgui_function_table_19250.SetColorEditOptions,
	g_imgui_function_table_19250.TreeNode,
	g_imgui_function_table_19250.TreeNodeV,
	g_imgui_function_table_19250.TreeNodeV2,
	g_imgui_function_table_19250.TreeNodeEx,
	g_imgui_function_table_19250.TreeNodeExV,
	g_imgui_function_table_19250.TreeNodeExV2,
	g_imgui_function_table_19250.TreePush,
	g_imgui_function_table_19250.TreePush2,
	g_imgui_function_table_19250.TreePop,
	g_imgui_function_table_19250.GetTreeNodeToLabelSpacing,
	g_imgui_function_table_19250.CollapsingHeader,
	g_imgui_function_table_19250.CollapsingHeader2,
	g_imgui_function_table_19250.SetNextItemOpen,
	g_imgui_function_table_19250.SetNextItemStorageID,
	g_imgui_function_table_19250.Selectable,
	g_imgui_function_table_19250.Selectable2,
	g_imgui_function_table_19250.BeginMultiSelect,
	g_imgui_function_table_19250.EndMultiSelect,
	g_imgui_function_table_19250.SetNextItemSelectionUserData,
	g_imgui_function_table_19250.IsItemToggledSelection,
	g_imgui_function_table_19250.BeginListBox,
	g_imgui_function_table_19250.EndListBox,
	g_imgui_function_table_19250.ListBox,
	g_imgui_function_table_19250.ListBox2,
	g_imgui_function_table_19250.PlotLines,
	g_imgui_function_table_19250.PlotLines2,
	g_imgui_function_table_19250.PlotHistogram,
	g_imgui_function_table_19250.PlotHistogram2,
	g_imgui_function_table_19250.Value,
	g_imgui_function_table_19250.Value2,
	g_imgui_function_table_19250.Value3,
	g_imgui_function_table_19250.Value4,
	g_imgui_function_table_19250.BeginMenuBar,
	g_imgui_function_table_19250.EndMenuBar,
	g_imgui_function_table_19250.BeginMainMenuBar,
	g_imgui_function_table_19250.EndMainMenuBar,
	g_imgui_function_table_19250.BeginMenu,
	g_imgui_function_table_19250.EndMenu,
	g_imgui_function_table_19250.MenuItem,
	g_imgui_function_table_19250.MenuItem2,
	g_imgui_function_table_19250.BeginTooltip,
	g_imgui_function_table_19250.EndTooltip,
	g_imgui_function_table_19250.SetTooltipV,
	g_imgui_function_table_19250.BeginItemTooltip,
	g_imgui_function_table_19250.SetItemTooltipV,
	g_imgui_function_table_19250.BeginPopup,
	g_imgui_function_table_19250.BeginPopupModal,
	g_imgui_function_table_19250.EndPopup,
	g_imgui_function_table_19250.OpenPopup,
	g_imgui_function_table_19250.OpenPopup2,
	g_imgui_function_table_19250.OpenPopupOnItemClick,
	g_imgui_function_table_19250.CloseCurrentPopup,
	g_imgui_function_table_19250.BeginPopupContextItem,
	g_imgui_function_table_19250.BeginPopupContextWindow,
	g_imgui_function_table_19250.BeginPopupContextVoid,
	g_imgui_function_table_19250.IsPopupOpen,
	g_imgui_function_table_19250.BeginTable,
	g_imgui_function_table_19250.EndTable,
	g_imgui_function_table_19250.TableNextRow,
	g_imgui_function_table_19250.TableNextColumn,
	g_imgui_function_table_19250.TableSetColumnIndex,
	g_imgui_function_table_19250.TableSetupColumn,
	g_imgui_function_table_19250.TableSetupScrollFreeze,
	g_imgui_function_table_19250.TableHeader,
	g_imgui_function_table_19250.TableHeadersRow,
	g_imgui_function_table_19250.TableAngledHeadersRow,
	g_imgui_function_table_19250.TableGetSortSpecs,
	g_imgui_function_table_19250.TableGetColumnCount,
	g_imgui_function_table_19250.TableGetColumnIndex,
	g_imgui_function_table_19250.TableGetRowIndex,
	g_imgui_function_table_19250.TableGetColumnName,
	g_imgui_function_table_19250.TableGetColumnFlags,
	g_imgui_function_table_19250.TableSetColumnEnabled,
	g_imgui_function_table_19250.TableGetHoveredColumn,
	g_imgui_function_table_19250.TableSetBgColor,
	g_imgui_function_table_19250.Columns,
	g_imgui_function_table_19250.NextColumn,
	g_imgui_function_table_19250.GetColumnIndex,
	g_imgui_function_table_19250.GetColumnWidth,
	g_imgui_function_table_19250.SetColumnWidth,
	g_imgui_function_table_19250.GetColumnOffset,
	g_imgui_function_table_19250.SetColumnOffset,
	g_imgui_function_table_19250.GetColumnsCount,
	g_imgui_function_table_19250.BeginTabBar,
	g_imgui_function_table_19250.EndTabBar,
	g_imgui_function_table_19250.BeginTabItem,
	g_imgui_function_table_19250.EndTabItem,
	g_imgui_function_table_19250.TabItemButton,
	g_imgui_function_table_19250.SetTabItemClosed,
	g_imgui_function_table_19250.DockSpace,
	g_imgui_function_table_19250.SetNextWindowDockID,
	g_imgui_function_table_19250.SetNextWindowClass,
	g_imgui_function_table_19250.GetWindowDockID,
	g_imgui_function_table_19250.IsWindowDocked,
	g_imgui_function_table_19250.BeginDragDropSource,
	g_imgui_function_table_19250.SetDragDropPayload,
	g_imgui_function_table_19250.EndDragDropSource,
	g_imgui_function_table_19250.BeginDragDropTarget,
	g_imgui_function_table_19250.AcceptDragDropPayload,
	g_imgui_function_table_19250.EndDragDropTarget,
	g_imgui_function_table_19250.GetDragDropPayload,
	g_imgui_function_table_19250.BeginDisabled,
	g_imgui_function_table_19250.EndDisabled,
	g_imgui_function_table_19250.PushClipRect,
	g_imgui_function_table_19250.PopClipRect,
	g_imgui_function_table_19250.SetItemDefaultFocus,
	g_imgui_function_table_19250.SetKeyboardFocusHere,
	g_imgui_function_table_19250.SetNavCursorVisible,
	g_imgui_function_table_19250.SetNextItemAllowOverlap,
	g_imgui_function_table_19250.IsItemHovered,
	g_imgui_function_table_19250.IsItemActive,
	g_imgui_function_table_19250.IsItemFocused,
	g_imgui_function_table_19250.IsItemClicked,
	g_imgui_function_table_19250.IsItemVisible,
	g_imgui_function_table_19250.IsItemEdited,
	g_imgui_function_table_19250.IsItemActivated,
	g_imgui_function_table_19250.IsItemDeactivated,
	g_imgui_function_table_19250.IsItemDeactivatedAfterEdit,
	g_imgui_function_table_19250.IsItemToggledOpen,
	g_imgui_function_table_19250.IsAnyItemHovered,
	g_imgui_function_table_19250.IsAnyItemActive,
	g_imgui_function_table_19250.IsAnyItemFocused,
	g_imgui_function_table_19250.GetItemID,
	g_imgui_function_table_19250.GetItemRectMin,
	g_imgui_function_table_19250.GetItemRectMax,
	g_imgui_function_table_19250.GetItemRectSize,
	g_imgui_function_table_19250.GetBackgroundDrawList,
	g_imgui_function_table_19250.GetForegroundDrawList,
	g_imgui_function_table_19250.IsRectVisible,
	g_imgui_function_table_19250.IsRectVisible2,
	g_imgui_function_table_19250.GetTime,
	g_imgui_function_table_19250.GetFrameCount,
	g_imgui_function_table_19250.GetDrawListSharedData,
	[](ImGuiCol idx) -> const char * { return g_imgui_function_table_19250.GetStyleColorName(convert_style_col(idx)); },
	g_imgui_function_table_19250.SetStateStorage,
	g_imgui_function_table_19250.GetStateStorage,
	g_imgui_function_table_19250.CalcTextSize,
	g_imgui_function_table_19250.ColorConvertU32ToFloat4,
	g_imgui_function_table_19250.ColorConvertFloat4ToU32,
	g_imgui_function_table_19250.ColorConvertRGBtoHSV,
	g_imgui_function_table_19250.ColorConvertHSVtoRGB,
	g_imgui_function_table_19250.IsKeyDown,
	g_imgui_function_table_19250.IsKeyPressed,
	g_imgui_function_table_19250.IsKeyReleased,
	g_imgui_function_table_19250.IsKeyChordPressed,
	g_imgui_function_table_19250.GetKeyPressedAmount,
	g_imgui_function_table_19250.GetKeyName,
	g_imgui_function_table_19250.SetNextFrameWantCaptureKeyboard,
	g_imgui_function_table_19250.Shortcut,
	g_imgui_function_table_19250.SetNextItemShortcut,
	g_imgui_function_table_19250.SetItemKeyOwner,
	g_imgui_function_table_19250.IsMouseDown,
	g_imgui_function_table_19250.IsMouseClicked,
	g_imgui_function_table_19250.IsMouseReleased,
	g_imgui_function_table_19250.IsMouseDoubleClicked,
	g_imgui_function_table_19250.IsMouseReleasedWithDelay,
	g_imgui_function_table_19250.GetMouseClickedCount,
	g_imgui_function_table_19250.IsMouseHoveringRect,
	g_imgui_function_table_19250.IsMousePosValid,
	g_imgui_function_table_19250.IsAnyMouseDown,
	g_imgui_function_table_19250.GetMousePos,
	g_imgui_function_table_19250.GetMousePosOnOpeningCurrentPopup,
	g_imgui_function_table_19250.IsMouseDragging,
	g_imgui_function_table_19250.GetMouseDragDelta,
	g_imgui_function_table_19250.ResetMouseDragDelta,
	g_imgui_function_table_19250.GetMouseCursor,
	g_imgui_function_table_19250.SetMouseCursor,
	g_imgui_function_table_19250.SetNextFrameWantCaptureMouse,
	g_imgui_function_table_19250.GetClipboardText,
	g_imgui_function_table_19250.SetClipboardText,
	g_imgui_function_table_19250.SetAllocatorFunctions,
	g_imgui_function_table_19250.GetAllocatorFunctions,
	g_imgui_function_table_19250.MemAlloc,
	g_imgui_function_table_19250.MemFree,
	g_imgui_function_table_19250.ImGuiStorage_GetInt,
	g_imgui_function_table_19250.ImGuiStorage_SetInt,
	g_imgui_function_table_19250.ImGuiStorage_GetBool,
	g_imgui_function_table_19250.ImGuiStorage_SetBool,
	g_imgui_function_table_19250.ImGuiStorage_GetFloat,
	g_imgui_function_table_19250.ImGuiStorage_SetFloat,
	g_imgui_function_table_19250.ImGuiStorage_GetVoidPtr,
	g_imgui_function_table_19250.ImGuiStorage_SetVoidPtr,
	g_imgui_function_table_19250.ImGuiStorage_GetIntRef,
	g_imgui_function_table_19250.ImGuiStorage_GetBoolRef,
	g_imgui_function_table_19250.ImGuiStorage_GetFloatRef,
	g_imgui_function_table_19250.ImGuiStorage_GetVoidPtrRef,
	g_imgui_function_table_19250.ImGuiStorage_BuildSortByKey,
	g_imgui_function_table_19250.ImGuiStorage_SetAllInt,
	[](imgui_list_clipper_19222 *_this) -> void {
		new(_this) imgui_list_clipper_19222();
		memset(_this, 0, sizeof(*_this));
	},
	[](imgui_list_clipper_19222 *_this) -> void {
		imgui_list_clipper_19250 temp;
		convert(*_this, temp);
		g_imgui_function_table_19250.ImGuiListClipper_End(&temp);
		_this->~imgui_list_clipper_19222();
	},
	[](imgui_list_clipper_19222 *_this, int items_count, float items_height) -> void {
		imgui_list_clipper_19250 temp;
		convert(*_this, temp);
		g_imgui_function_table_19250.ImGuiListClipper_Begin(&temp, items_count, items_height);
		convert(temp, *_this);
		temp.TempData = nullptr; // Prevent 'ImGuiListClipper' destructor from doing anything
	},
	[](imgui_list_clipper_19222 *_this) -> void {
		imgui_list_clipper_19250 temp;
		convert(*_this, temp);
		g_imgui_function_table_19250.ImGuiListClipper_End(&temp);
		convert(temp, *_this);
	},
	[](imgui_list_clipper_19222 *_this) -> bool {
		imgui_list_clipper_19250 temp;
		convert(*_this, temp);
		const bool result = g_imgui_function_table_19250.ImGuiListClipper_Step(&temp);
		convert(temp, *_this);
		temp.TempData = nullptr;
		return result;
	},
	[](imgui_list_clipper_19222 *_this, int item_begin, int item_end) -> void {
		imgui_list_clipper_19250 temp;
		convert(*_this, temp);
		g_imgui_function_table_19250.ImGuiListClipper_IncludeItemsByIndex(&temp, item_begin, item_end);
		convert(temp, *_this);
		temp.TempData = nullptr;
	},
	[](imgui_list_clipper_19222 *_this, int item_index) -> void {
		imgui_list_clipper_19250 temp;
		convert(*_this, temp);
		g_imgui_function_table_19250.ImGuiListClipper_SeekCursorForItem(&temp, item_index);
		convert(temp, *_this);
		temp.TempData = nullptr;
	},
	g_imgui_function_table_19250.ConstructImDrawList,
	g_imgui_function_table_19250.DestructImDrawList,
	g_imgui_function_table_19250.ImDrawList_PushClipRect,
	g_imgui_function_table_19250.ImDrawList_PushClipRectFullScreen,
	g_imgui_function_table_19250.ImDrawList_PopClipRect,
	g_imgui_function_table_19250.ImDrawList_PushTexture,
	g_imgui_function_table_19250.ImDrawList_PopTexture,
	g_imgui_function_table_19250.ImDrawList_AddLine,
	g_imgui_function_table_19250.ImDrawList_AddRect,
	g_imgui_function_table_19250.ImDrawList_AddRectFilled,
	g_imgui_function_table_19250.ImDrawList_AddRectFilledMultiColor,
	g_imgui_function_table_19250.ImDrawList_AddQuad,
	g_imgui_function_table_19250.ImDrawList_AddQuadFilled,
	g_imgui_function_table_19250.ImDrawList_AddTriangle,
	g_imgui_function_table_19250.ImDrawList_AddTriangleFilled,
	g_imgui_function_table_19250.ImDrawList_AddCircle,
	g_imgui_function_table_19250.ImDrawList_AddCircleFilled,
	g_imgui_function_table_19250.ImDrawList_AddNgon,
	g_imgui_function_table_19250.ImDrawList_AddNgonFilled,
	g_imgui_function_table_19250.ImDrawList_AddEllipse,
	g_imgui_function_table_19250.ImDrawList_AddEllipseFilled,
	g_imgui_function_table_19250.ImDrawList_AddText,
	g_imgui_function_table_19250.ImDrawList_AddText2,
	g_imgui_function_table_19250.ImDrawList_AddBezierCubic,
	g_imgui_function_table_19250.ImDrawList_AddBezierQuadratic,
	g_imgui_function_table_19250.ImDrawList_AddPolyline,
	g_imgui_function_table_19250.ImDrawList_AddConvexPolyFilled,
	g_imgui_function_table_19250.ImDrawList_AddConcavePolyFilled,
	g_imgui_function_table_19250.ImDrawList_AddImage,
	g_imgui_function_table_19250.ImDrawList_AddImageQuad,
	g_imgui_function_table_19250.ImDrawList_AddImageRounded,
	g_imgui_function_table_19250.ImDrawList_PathArcTo,
	g_imgui_function_table_19250.ImDrawList_PathArcToFast,
	g_imgui_function_table_19250.ImDrawList_PathEllipticalArcTo,
	g_imgui_function_table_19250.ImDrawList_PathBezierCubicCurveTo,
	g_imgui_function_table_19250.ImDrawList_PathBezierQuadraticCurveTo,
	g_imgui_function_table_19250.ImDrawList_PathRect,
	g_imgui_function_table_19250.ImDrawList_AddCallback,
	g_imgui_function_table_19250.ImDrawList_AddDrawCmd,
	g_imgui_function_table_19250.ImDrawList_CloneOutput,
	g_imgui_function_table_19250.ImDrawList_PrimReserve,
	g_imgui_function_table_19250.ImDrawList_PrimUnreserve,
	g_imgui_function_table_19250.ImDrawList_PrimRect,
	g_imgui_function_table_19250.ImDrawList_PrimRectUV,
	g_imgui_function_table_19250.ImDrawList_PrimQuadUV,
	g_imgui_function_table_19250.ConstructImFont,
	g_imgui_function_table_19250.DestructImFont,
	g_imgui_function_table_19250.ImFont_IsGlyphInFont,

}; }

#endif
