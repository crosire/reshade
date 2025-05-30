/*
 * Copyright (C) 2021 Patrick Mours
 * Copyright (C) 2014-2025 Omar Cornut
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON && RESHADE_GUI

#include <new>
#include "imgui_function_table_19180.hpp"

namespace
{
	void convert(const imgui_io_19191 &new_io, imgui_io_19180 &old_io)
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
		old_io.FontGlobalScale = new_io.FontGlobalScale;
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

	void convert(const imgui_style_19191 &new_style, imgui_style_19180 &old_style)
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
		old_style.TabRounding = new_style.TabRounding;
		old_style.TabBorderSize = new_style.TabBorderSize;
		old_style.TabMinWidthForCloseButton = new_style.TabCloseButtonMinWidthUnselected;
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
		for (int i = 0; i < 58; ++i)
			old_style.Colors[i] = new_style.Colors[i];
		old_style.HoverStationaryDelay = new_style.HoverStationaryDelay;
		old_style.HoverDelayShort = new_style.HoverDelayShort;
		old_style.HoverDelayNormal = new_style.HoverDelayNormal;
		old_style.HoverFlagsForTooltipMouse = new_style.HoverFlagsForTooltipMouse;
		old_style.HoverFlagsForTooltipNav = new_style.HoverFlagsForTooltipNav;
	}
}

const imgui_function_table_19180 init_imgui_function_table_19180() { return {
	[]() -> imgui_io_19180 &{
		static imgui_io_19180 io = {};
		convert(g_imgui_function_table_19191.GetIO(), io);
		return io;
	},
	[]() -> imgui_style_19180 &{
		static imgui_style_19180 style = {};
		convert(g_imgui_function_table_19191.GetStyle(), style);
		return style;
	},
	g_imgui_function_table_19191.GetVersion,
	g_imgui_function_table_19191.Begin,
	g_imgui_function_table_19191.End,
	g_imgui_function_table_19191.BeginChild,
	g_imgui_function_table_19191.BeginChild2,
	g_imgui_function_table_19191.EndChild,
	g_imgui_function_table_19191.IsWindowAppearing,
	g_imgui_function_table_19191.IsWindowCollapsed,
	g_imgui_function_table_19191.IsWindowFocused,
	g_imgui_function_table_19191.IsWindowHovered,
	g_imgui_function_table_19191.GetWindowDrawList,
	g_imgui_function_table_19191.GetWindowDpiScale,
	g_imgui_function_table_19191.GetWindowPos,
	g_imgui_function_table_19191.GetWindowSize,
	g_imgui_function_table_19191.GetWindowWidth,
	g_imgui_function_table_19191.GetWindowHeight,
	g_imgui_function_table_19191.SetNextWindowPos,
	g_imgui_function_table_19191.SetNextWindowSize,
	g_imgui_function_table_19191.SetNextWindowSizeConstraints,
	g_imgui_function_table_19191.SetNextWindowContentSize,
	g_imgui_function_table_19191.SetNextWindowCollapsed,
	g_imgui_function_table_19191.SetNextWindowFocus,
	g_imgui_function_table_19191.SetNextWindowScroll,
	g_imgui_function_table_19191.SetNextWindowBgAlpha,
	g_imgui_function_table_19191.SetWindowPos,
	g_imgui_function_table_19191.SetWindowSize,
	g_imgui_function_table_19191.SetWindowCollapsed,
	g_imgui_function_table_19191.SetWindowFocus,
	g_imgui_function_table_19191.SetWindowFontScale,
	g_imgui_function_table_19191.SetWindowPos2,
	g_imgui_function_table_19191.SetWindowSize2,
	g_imgui_function_table_19191.SetWindowCollapsed2,
	g_imgui_function_table_19191.SetWindowFocus2,
	g_imgui_function_table_19191.GetScrollX,
	g_imgui_function_table_19191.GetScrollY,
	g_imgui_function_table_19191.SetScrollX,
	g_imgui_function_table_19191.SetScrollY,
	g_imgui_function_table_19191.GetScrollMaxX,
	g_imgui_function_table_19191.GetScrollMaxY,
	g_imgui_function_table_19191.SetScrollHereX,
	g_imgui_function_table_19191.SetScrollHereY,
	g_imgui_function_table_19191.SetScrollFromPosX,
	g_imgui_function_table_19191.SetScrollFromPosY,
	g_imgui_function_table_19191.PushFont,
	g_imgui_function_table_19191.PopFont,
	g_imgui_function_table_19191.PushStyleColor,
	g_imgui_function_table_19191.PushStyleColor2,
	g_imgui_function_table_19191.PopStyleColor,
	g_imgui_function_table_19191.PushStyleVar,
	g_imgui_function_table_19191.PushStyleVar2,
	g_imgui_function_table_19191.PushStyleVarX,
	g_imgui_function_table_19191.PushStyleVarY,
	g_imgui_function_table_19191.PopStyleVar,
	g_imgui_function_table_19191.PushItemFlag,
	g_imgui_function_table_19191.PopItemFlag,
	g_imgui_function_table_19191.PushItemWidth,
	g_imgui_function_table_19191.PopItemWidth,
	g_imgui_function_table_19191.SetNextItemWidth,
	g_imgui_function_table_19191.CalcItemWidth,
	g_imgui_function_table_19191.PushTextWrapPos,
	g_imgui_function_table_19191.PopTextWrapPos,
	g_imgui_function_table_19191.GetFont,
	g_imgui_function_table_19191.GetFontSize,
	g_imgui_function_table_19191.GetFontTexUvWhitePixel,
	g_imgui_function_table_19191.GetColorU32,
	g_imgui_function_table_19191.GetColorU322,
	g_imgui_function_table_19191.GetColorU323,
	g_imgui_function_table_19191.GetStyleColorVec4,
	g_imgui_function_table_19191.GetCursorScreenPos,
	g_imgui_function_table_19191.SetCursorScreenPos,
	g_imgui_function_table_19191.GetContentRegionAvail,
	g_imgui_function_table_19191.GetCursorPos,
	g_imgui_function_table_19191.GetCursorPosX,
	g_imgui_function_table_19191.GetCursorPosY,
	g_imgui_function_table_19191.SetCursorPos,
	g_imgui_function_table_19191.SetCursorPosX,
	g_imgui_function_table_19191.SetCursorPosY,
	g_imgui_function_table_19191.GetCursorStartPos,
	g_imgui_function_table_19191.Separator,
	g_imgui_function_table_19191.SameLine,
	g_imgui_function_table_19191.NewLine,
	g_imgui_function_table_19191.Spacing,
	g_imgui_function_table_19191.Dummy,
	g_imgui_function_table_19191.Indent,
	g_imgui_function_table_19191.Unindent,
	g_imgui_function_table_19191.BeginGroup,
	g_imgui_function_table_19191.EndGroup,
	g_imgui_function_table_19191.AlignTextToFramePadding,
	g_imgui_function_table_19191.GetTextLineHeight,
	g_imgui_function_table_19191.GetTextLineHeightWithSpacing,
	g_imgui_function_table_19191.GetFrameHeight,
	g_imgui_function_table_19191.GetFrameHeightWithSpacing,
	g_imgui_function_table_19191.PushID,
	g_imgui_function_table_19191.PushID2,
	g_imgui_function_table_19191.PushID3,
	g_imgui_function_table_19191.PushID4,
	g_imgui_function_table_19191.PopID,
	g_imgui_function_table_19191.GetID,
	g_imgui_function_table_19191.GetID2,
	g_imgui_function_table_19191.GetID3,
	g_imgui_function_table_19191.GetID4,
	g_imgui_function_table_19191.TextUnformatted,
	g_imgui_function_table_19191.TextV,
	g_imgui_function_table_19191.TextColoredV,
	g_imgui_function_table_19191.TextDisabledV,
	g_imgui_function_table_19191.TextWrappedV,
	g_imgui_function_table_19191.LabelTextV,
	g_imgui_function_table_19191.BulletTextV,
	g_imgui_function_table_19191.SeparatorText,
	g_imgui_function_table_19191.Button,
	g_imgui_function_table_19191.SmallButton,
	g_imgui_function_table_19191.InvisibleButton,
	g_imgui_function_table_19191.ArrowButton,
	g_imgui_function_table_19191.Checkbox,
	g_imgui_function_table_19191.CheckboxFlags,
	g_imgui_function_table_19191.CheckboxFlags2,
	g_imgui_function_table_19191.RadioButton,
	g_imgui_function_table_19191.RadioButton2,
	g_imgui_function_table_19191.ProgressBar,
	g_imgui_function_table_19191.Bullet,
	g_imgui_function_table_19191.TextLink,
	g_imgui_function_table_19191.TextLinkOpenURL,
	[](ImTextureID user_texture_id, const ImVec2 &image_size, const ImVec2 &uv0, const ImVec2 &uv1, const ImVec4 &tint_col, const ImVec4 &border_col) {
		const float image_border_size = g_imgui_function_table_19191.GetStyle().ImageBorderSize;
		g_imgui_function_table_19191.PushStyleVar(ImGuiStyleVar_ImageBorderSize, (border_col.w > 0.0f) ? (image_border_size > 1.0f ? image_border_size : 1.0f) : 0.0f);
		g_imgui_function_table_19191.PushStyleColor2(ImGuiCol_Border, border_col);
		g_imgui_function_table_19191.ImageWithBg(user_texture_id, image_size, uv0, uv1, ImVec4(0, 0, 0, 0), tint_col);
		g_imgui_function_table_19191.PopStyleColor(1);
		g_imgui_function_table_19191.PopStyleVar(1);
	},
	g_imgui_function_table_19191.ImageButton,
	g_imgui_function_table_19191.BeginCombo,
	g_imgui_function_table_19191.EndCombo,
	g_imgui_function_table_19191.Combo,
	g_imgui_function_table_19191.Combo2,
	g_imgui_function_table_19191.Combo3,
	g_imgui_function_table_19191.DragFloat,
	g_imgui_function_table_19191.DragFloat2,
	g_imgui_function_table_19191.DragFloat3,
	g_imgui_function_table_19191.DragFloat4,
	g_imgui_function_table_19191.DragFloatRange2,
	g_imgui_function_table_19191.DragInt,
	g_imgui_function_table_19191.DragInt2,
	g_imgui_function_table_19191.DragInt3,
	g_imgui_function_table_19191.DragInt4,
	g_imgui_function_table_19191.DragIntRange2,
	g_imgui_function_table_19191.DragScalar,
	g_imgui_function_table_19191.DragScalarN,
	g_imgui_function_table_19191.SliderFloat,
	g_imgui_function_table_19191.SliderFloat2,
	g_imgui_function_table_19191.SliderFloat3,
	g_imgui_function_table_19191.SliderFloat4,
	g_imgui_function_table_19191.SliderAngle,
	g_imgui_function_table_19191.SliderInt,
	g_imgui_function_table_19191.SliderInt2,
	g_imgui_function_table_19191.SliderInt3,
	g_imgui_function_table_19191.SliderInt4,
	g_imgui_function_table_19191.SliderScalar,
	g_imgui_function_table_19191.SliderScalarN,
	g_imgui_function_table_19191.VSliderFloat,
	g_imgui_function_table_19191.VSliderInt,
	g_imgui_function_table_19191.VSliderScalar,
	g_imgui_function_table_19191.InputText,
	g_imgui_function_table_19191.InputTextMultiline,
	g_imgui_function_table_19191.InputTextWithHint,
	g_imgui_function_table_19191.InputFloat,
	g_imgui_function_table_19191.InputFloat2,
	g_imgui_function_table_19191.InputFloat3,
	g_imgui_function_table_19191.InputFloat4,
	g_imgui_function_table_19191.InputInt,
	g_imgui_function_table_19191.InputInt2,
	g_imgui_function_table_19191.InputInt3,
	g_imgui_function_table_19191.InputInt4,
	g_imgui_function_table_19191.InputDouble,
	g_imgui_function_table_19191.InputScalar,
	g_imgui_function_table_19191.InputScalarN,
	g_imgui_function_table_19191.ColorEdit3,
	g_imgui_function_table_19191.ColorEdit4,
	g_imgui_function_table_19191.ColorPicker3,
	g_imgui_function_table_19191.ColorPicker4,
	g_imgui_function_table_19191.ColorButton,
	g_imgui_function_table_19191.SetColorEditOptions,
	g_imgui_function_table_19191.TreeNode,
	g_imgui_function_table_19191.TreeNodeV,
	g_imgui_function_table_19191.TreeNodeV2,
	g_imgui_function_table_19191.TreeNodeEx,
	g_imgui_function_table_19191.TreeNodeExV,
	g_imgui_function_table_19191.TreeNodeExV2,
	g_imgui_function_table_19191.TreePush,
	g_imgui_function_table_19191.TreePush2,
	g_imgui_function_table_19191.TreePop,
	g_imgui_function_table_19191.GetTreeNodeToLabelSpacing,
	g_imgui_function_table_19191.CollapsingHeader,
	g_imgui_function_table_19191.CollapsingHeader2,
	g_imgui_function_table_19191.SetNextItemOpen,
	g_imgui_function_table_19191.SetNextItemStorageID,
	g_imgui_function_table_19191.Selectable,
	g_imgui_function_table_19191.Selectable2,
	g_imgui_function_table_19191.BeginMultiSelect,
	g_imgui_function_table_19191.EndMultiSelect,
	g_imgui_function_table_19191.SetNextItemSelectionUserData,
	g_imgui_function_table_19191.IsItemToggledSelection,
	g_imgui_function_table_19191.BeginListBox,
	g_imgui_function_table_19191.EndListBox,
	g_imgui_function_table_19191.ListBox,
	g_imgui_function_table_19191.ListBox2,
	g_imgui_function_table_19191.PlotLines,
	g_imgui_function_table_19191.PlotLines2,
	g_imgui_function_table_19191.PlotHistogram,
	g_imgui_function_table_19191.PlotHistogram2,
	g_imgui_function_table_19191.Value,
	g_imgui_function_table_19191.Value2,
	g_imgui_function_table_19191.Value3,
	g_imgui_function_table_19191.Value4,
	g_imgui_function_table_19191.BeginMenuBar,
	g_imgui_function_table_19191.EndMenuBar,
	g_imgui_function_table_19191.BeginMainMenuBar,
	g_imgui_function_table_19191.EndMainMenuBar,
	g_imgui_function_table_19191.BeginMenu,
	g_imgui_function_table_19191.EndMenu,
	g_imgui_function_table_19191.MenuItem,
	g_imgui_function_table_19191.MenuItem2,
	g_imgui_function_table_19191.BeginTooltip,
	g_imgui_function_table_19191.EndTooltip,
	g_imgui_function_table_19191.SetTooltipV,
	g_imgui_function_table_19191.BeginItemTooltip,
	g_imgui_function_table_19191.SetItemTooltipV,
	g_imgui_function_table_19191.BeginPopup,
	g_imgui_function_table_19191.BeginPopupModal,
	g_imgui_function_table_19191.EndPopup,
	g_imgui_function_table_19191.OpenPopup,
	g_imgui_function_table_19191.OpenPopup2,
	g_imgui_function_table_19191.OpenPopupOnItemClick,
	g_imgui_function_table_19191.CloseCurrentPopup,
	g_imgui_function_table_19191.BeginPopupContextItem,
	g_imgui_function_table_19191.BeginPopupContextWindow,
	g_imgui_function_table_19191.BeginPopupContextVoid,
	g_imgui_function_table_19191.IsPopupOpen,
	g_imgui_function_table_19191.BeginTable,
	g_imgui_function_table_19191.EndTable,
	g_imgui_function_table_19191.TableNextRow,
	g_imgui_function_table_19191.TableNextColumn,
	g_imgui_function_table_19191.TableSetColumnIndex,
	g_imgui_function_table_19191.TableSetupColumn,
	g_imgui_function_table_19191.TableSetupScrollFreeze,
	g_imgui_function_table_19191.TableHeader,
	g_imgui_function_table_19191.TableHeadersRow,
	g_imgui_function_table_19191.TableAngledHeadersRow,
	g_imgui_function_table_19191.TableGetSortSpecs,
	g_imgui_function_table_19191.TableGetColumnCount,
	g_imgui_function_table_19191.TableGetColumnIndex,
	g_imgui_function_table_19191.TableGetRowIndex,
	g_imgui_function_table_19191.TableGetColumnName,
	g_imgui_function_table_19191.TableGetColumnFlags,
	g_imgui_function_table_19191.TableSetColumnEnabled,
	g_imgui_function_table_19191.TableGetHoveredColumn,
	g_imgui_function_table_19191.TableSetBgColor,
	g_imgui_function_table_19191.Columns,
	g_imgui_function_table_19191.NextColumn,
	g_imgui_function_table_19191.GetColumnIndex,
	g_imgui_function_table_19191.GetColumnWidth,
	g_imgui_function_table_19191.SetColumnWidth,
	g_imgui_function_table_19191.GetColumnOffset,
	g_imgui_function_table_19191.SetColumnOffset,
	g_imgui_function_table_19191.GetColumnsCount,
	g_imgui_function_table_19191.BeginTabBar,
	g_imgui_function_table_19191.EndTabBar,
	g_imgui_function_table_19191.BeginTabItem,
	g_imgui_function_table_19191.EndTabItem,
	g_imgui_function_table_19191.TabItemButton,
	g_imgui_function_table_19191.SetTabItemClosed,
	g_imgui_function_table_19191.DockSpace,
	g_imgui_function_table_19191.SetNextWindowDockID,
	g_imgui_function_table_19191.SetNextWindowClass,
	g_imgui_function_table_19191.GetWindowDockID,
	g_imgui_function_table_19191.IsWindowDocked,
	g_imgui_function_table_19191.BeginDragDropSource,
	g_imgui_function_table_19191.SetDragDropPayload,
	g_imgui_function_table_19191.EndDragDropSource,
	g_imgui_function_table_19191.BeginDragDropTarget,
	g_imgui_function_table_19191.AcceptDragDropPayload,
	g_imgui_function_table_19191.EndDragDropTarget,
	g_imgui_function_table_19191.GetDragDropPayload,
	g_imgui_function_table_19191.BeginDisabled,
	g_imgui_function_table_19191.EndDisabled,
	g_imgui_function_table_19191.PushClipRect,
	g_imgui_function_table_19191.PopClipRect,
	g_imgui_function_table_19191.SetItemDefaultFocus,
	g_imgui_function_table_19191.SetKeyboardFocusHere,
	g_imgui_function_table_19191.SetNavCursorVisible,
	g_imgui_function_table_19191.SetNextItemAllowOverlap,
	g_imgui_function_table_19191.IsItemHovered,
	g_imgui_function_table_19191.IsItemActive,
	g_imgui_function_table_19191.IsItemFocused,
	g_imgui_function_table_19191.IsItemClicked,
	g_imgui_function_table_19191.IsItemVisible,
	g_imgui_function_table_19191.IsItemEdited,
	g_imgui_function_table_19191.IsItemActivated,
	g_imgui_function_table_19191.IsItemDeactivated,
	g_imgui_function_table_19191.IsItemDeactivatedAfterEdit,
	g_imgui_function_table_19191.IsItemToggledOpen,
	g_imgui_function_table_19191.IsAnyItemHovered,
	g_imgui_function_table_19191.IsAnyItemActive,
	g_imgui_function_table_19191.IsAnyItemFocused,
	g_imgui_function_table_19191.GetItemID,
	g_imgui_function_table_19191.GetItemRectMin,
	g_imgui_function_table_19191.GetItemRectMax,
	g_imgui_function_table_19191.GetItemRectSize,
	g_imgui_function_table_19191.GetBackgroundDrawList,
	g_imgui_function_table_19191.GetForegroundDrawList,
	g_imgui_function_table_19191.IsRectVisible,
	g_imgui_function_table_19191.IsRectVisible2,
	g_imgui_function_table_19191.GetTime,
	g_imgui_function_table_19191.GetFrameCount,
	g_imgui_function_table_19191.GetDrawListSharedData,
	g_imgui_function_table_19191.GetStyleColorName,
	g_imgui_function_table_19191.SetStateStorage,
	g_imgui_function_table_19191.GetStateStorage,
	g_imgui_function_table_19191.CalcTextSize,
	g_imgui_function_table_19191.ColorConvertU32ToFloat4,
	g_imgui_function_table_19191.ColorConvertFloat4ToU32,
	g_imgui_function_table_19191.ColorConvertRGBtoHSV,
	g_imgui_function_table_19191.ColorConvertHSVtoRGB,
	g_imgui_function_table_19191.IsKeyDown,
	g_imgui_function_table_19191.IsKeyPressed,
	g_imgui_function_table_19191.IsKeyReleased,
	g_imgui_function_table_19191.IsKeyChordPressed,
	g_imgui_function_table_19191.GetKeyPressedAmount,
	g_imgui_function_table_19191.GetKeyName,
	g_imgui_function_table_19191.SetNextFrameWantCaptureKeyboard,
	g_imgui_function_table_19191.Shortcut,
	g_imgui_function_table_19191.SetNextItemShortcut,
	g_imgui_function_table_19191.SetItemKeyOwner,
	g_imgui_function_table_19191.IsMouseDown,
	g_imgui_function_table_19191.IsMouseClicked,
	g_imgui_function_table_19191.IsMouseReleased,
	g_imgui_function_table_19191.IsMouseDoubleClicked,
	g_imgui_function_table_19191.IsMouseReleasedWithDelay,
	g_imgui_function_table_19191.GetMouseClickedCount,
	g_imgui_function_table_19191.IsMouseHoveringRect,
	g_imgui_function_table_19191.IsMousePosValid,
	g_imgui_function_table_19191.IsAnyMouseDown,
	g_imgui_function_table_19191.GetMousePos,
	g_imgui_function_table_19191.GetMousePosOnOpeningCurrentPopup,
	g_imgui_function_table_19191.IsMouseDragging,
	g_imgui_function_table_19191.GetMouseDragDelta,
	g_imgui_function_table_19191.ResetMouseDragDelta,
	g_imgui_function_table_19191.GetMouseCursor,
	g_imgui_function_table_19191.SetMouseCursor,
	g_imgui_function_table_19191.SetNextFrameWantCaptureMouse,
	g_imgui_function_table_19191.GetClipboardText,
	g_imgui_function_table_19191.SetClipboardText,
	g_imgui_function_table_19191.SetAllocatorFunctions,
	g_imgui_function_table_19191.GetAllocatorFunctions,
	g_imgui_function_table_19191.MemAlloc,
	g_imgui_function_table_19191.MemFree,
	g_imgui_function_table_19191.ImGuiStorage_GetInt,
	g_imgui_function_table_19191.ImGuiStorage_SetInt,
	g_imgui_function_table_19191.ImGuiStorage_GetBool,
	g_imgui_function_table_19191.ImGuiStorage_SetBool,
	g_imgui_function_table_19191.ImGuiStorage_GetFloat,
	g_imgui_function_table_19191.ImGuiStorage_SetFloat,
	g_imgui_function_table_19191.ImGuiStorage_GetVoidPtr,
	g_imgui_function_table_19191.ImGuiStorage_SetVoidPtr,
	g_imgui_function_table_19191.ImGuiStorage_GetIntRef,
	g_imgui_function_table_19191.ImGuiStorage_GetBoolRef,
	g_imgui_function_table_19191.ImGuiStorage_GetFloatRef,
	g_imgui_function_table_19191.ImGuiStorage_GetVoidPtrRef,
	g_imgui_function_table_19191.ImGuiStorage_BuildSortByKey,
	g_imgui_function_table_19191.ImGuiStorage_SetAllInt,
	g_imgui_function_table_19191.ConstructImGuiListClipper,
	g_imgui_function_table_19191.DestructImGuiListClipper,
	g_imgui_function_table_19191.ImGuiListClipper_Begin,
	g_imgui_function_table_19191.ImGuiListClipper_End,
	g_imgui_function_table_19191.ImGuiListClipper_Step,
	g_imgui_function_table_19191.ImGuiListClipper_IncludeItemsByIndex,
	g_imgui_function_table_19191.ImGuiListClipper_SeekCursorForItem,
	g_imgui_function_table_19191.ConstructImDrawList,
	g_imgui_function_table_19191.DestructImDrawList,
	g_imgui_function_table_19191.ImDrawList_PushClipRect,
	g_imgui_function_table_19191.ImDrawList_PushClipRectFullScreen,
	g_imgui_function_table_19191.ImDrawList_PopClipRect,
	g_imgui_function_table_19191.ImDrawList_PushTextureID,
	g_imgui_function_table_19191.ImDrawList_PopTextureID,
	g_imgui_function_table_19191.ImDrawList_AddLine,
	g_imgui_function_table_19191.ImDrawList_AddRect,
	g_imgui_function_table_19191.ImDrawList_AddRectFilled,
	g_imgui_function_table_19191.ImDrawList_AddRectFilledMultiColor,
	g_imgui_function_table_19191.ImDrawList_AddQuad,
	g_imgui_function_table_19191.ImDrawList_AddQuadFilled,
	g_imgui_function_table_19191.ImDrawList_AddTriangle,
	g_imgui_function_table_19191.ImDrawList_AddTriangleFilled,
	g_imgui_function_table_19191.ImDrawList_AddCircle,
	g_imgui_function_table_19191.ImDrawList_AddCircleFilled,
	g_imgui_function_table_19191.ImDrawList_AddNgon,
	g_imgui_function_table_19191.ImDrawList_AddNgonFilled,
	g_imgui_function_table_19191.ImDrawList_AddEllipse,
	g_imgui_function_table_19191.ImDrawList_AddEllipseFilled,
	g_imgui_function_table_19191.ImDrawList_AddText,
	g_imgui_function_table_19191.ImDrawList_AddText2,
	g_imgui_function_table_19191.ImDrawList_AddBezierCubic,
	g_imgui_function_table_19191.ImDrawList_AddBezierQuadratic,
	g_imgui_function_table_19191.ImDrawList_AddPolyline,
	g_imgui_function_table_19191.ImDrawList_AddConvexPolyFilled,
	g_imgui_function_table_19191.ImDrawList_AddConcavePolyFilled,
	g_imgui_function_table_19191.ImDrawList_AddImage,
	g_imgui_function_table_19191.ImDrawList_AddImageQuad,
	g_imgui_function_table_19191.ImDrawList_AddImageRounded,
	g_imgui_function_table_19191.ImDrawList_PathArcTo,
	g_imgui_function_table_19191.ImDrawList_PathArcToFast,
	g_imgui_function_table_19191.ImDrawList_PathEllipticalArcTo,
	g_imgui_function_table_19191.ImDrawList_PathBezierCubicCurveTo,
	g_imgui_function_table_19191.ImDrawList_PathBezierQuadraticCurveTo,
	g_imgui_function_table_19191.ImDrawList_PathRect,
	g_imgui_function_table_19191.ImDrawList_AddCallback,
	g_imgui_function_table_19191.ImDrawList_AddDrawCmd,
	g_imgui_function_table_19191.ImDrawList_CloneOutput,
	g_imgui_function_table_19191.ImDrawList_PrimReserve,
	g_imgui_function_table_19191.ImDrawList_PrimUnreserve,
	g_imgui_function_table_19191.ImDrawList_PrimRect,
	g_imgui_function_table_19191.ImDrawList_PrimRectUV,
	g_imgui_function_table_19191.ImDrawList_PrimQuadUV,
	g_imgui_function_table_19191.ConstructImFont,
	g_imgui_function_table_19191.DestructImFont,
	[](imgui_font_19180 *_this, ImWchar c) -> const ImFontGlyph *{ return g_imgui_function_table_19191.ImFont_FindGlyph(_this, c); },
	[](imgui_font_19180 *_this, ImWchar c) -> const ImFontGlyph *{ return g_imgui_function_table_19191.ImFont_FindGlyphNoFallback(_this, c); },
	g_imgui_function_table_19191.ImFont_CalcTextSizeA,
	g_imgui_function_table_19191.ImFont_CalcWordWrapPositionA,
	g_imgui_function_table_19191.ImFont_RenderChar,
	g_imgui_function_table_19191.ImFont_RenderText,

}; }

#endif
