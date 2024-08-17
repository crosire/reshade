/*
 * Copyright (C) 2021 Patrick Mours
 * Copyright (C) 2014-2022 Omar Cornut
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON

#include <new>
#include "imgui_function_table_18600.hpp"

namespace
{
	void convert(const imgui_io_18971 &new_io, imgui_io_18600 &old_io)
	{
		old_io.ConfigFlags = new_io.ConfigFlags;
		old_io.BackendFlags = new_io.BackendFlags;
		old_io.DisplaySize = new_io.DisplaySize;
		old_io.DeltaTime = new_io.DeltaTime;
		old_io.IniSavingRate = new_io.IniSavingRate;
		old_io.IniFilename = new_io.IniFilename;
		old_io.LogFilename = new_io.LogFilename;
		old_io.MouseDoubleClickTime = new_io.MouseDoubleClickTime;
		old_io.MouseDoubleClickMaxDist = new_io.MouseDoubleClickMaxDist;
		old_io.MouseDragThreshold = new_io.MouseDragThreshold;
		for (int i = 0; i < 22; ++i)
			old_io.KeyMap[i] = new_io.KeyMap[i];
		old_io.KeyRepeatDelay = new_io.KeyRepeatDelay;
		old_io.KeyRepeatRate = new_io.KeyRepeatRate;
		old_io.UserData = new_io.UserData;

		// It's not safe to access the internal fields of 'FontAtlas'
		old_io.Fonts = new_io.Fonts;
		old_io.FontGlobalScale = new_io.FontGlobalScale;
		old_io.FontAllowUserScaling = new_io.FontAllowUserScaling;
		old_io.FontDefault = nullptr;
		old_io.DisplayFramebufferScale = new_io.DisplayFramebufferScale;

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
		old_io.ConfigInputTextCursorBlink = new_io.ConfigInputTextCursorBlink;
		old_io.ConfigDragClickToInputText = new_io.ConfigDragClickToInputText;
		old_io.ConfigWindowsResizeFromEdges = new_io.ConfigWindowsResizeFromEdges;
		old_io.ConfigWindowsMoveFromTitleBarOnly = new_io.ConfigWindowsMoveFromTitleBarOnly;
		old_io.ConfigMemoryCompactTimer = new_io.ConfigMemoryCompactTimer;

		old_io.BackendPlatformName = new_io.BackendPlatformName;
		old_io.BackendRendererName = new_io.BackendRendererName;
		old_io.BackendPlatformUserData = new_io.BackendPlatformUserData;
		old_io.BackendRendererUserData = new_io.BackendRendererUserData;
		old_io.BackendLanguageUserData = new_io.BackendLanguageUserData;

		old_io.GetClipboardTextFn = new_io.GetClipboardTextFn;
		old_io.SetClipboardTextFn = new_io.SetClipboardTextFn;
		old_io.ClipboardUserData = new_io.ClipboardUserData;

		old_io.MousePos = new_io.MousePos;
		for (int i = 0; i < 5; ++i)
			old_io.MouseDown[i] = new_io.MouseDown[i];
		old_io.MouseWheel = new_io.MouseWheel;
		old_io.MouseWheelH = new_io.MouseWheelH;
		old_io.MouseHoveredViewport = new_io.MouseHoveredViewport;
		old_io.KeyCtrl = new_io.KeyCtrl;
		old_io.KeyShift = new_io.KeyShift;
		old_io.KeyAlt = new_io.KeyAlt;
		old_io.KeySuper = new_io.KeySuper;
		for (int i = 0; i < 512; ++i)
			old_io.KeysDown[i] = new_io.KeysDown[i];
		for (int i = 0; i < 16; ++i)
			old_io.NavInputs[i] = new_io.NavInputs[i];

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
		old_io.MetricsActiveAllocations = new_io.MetricsActiveAllocations;
		old_io.MouseDelta = new_io.MouseDelta;
	}

	void convert(const imgui_style_18971 &new_style, imgui_style_18600 &old_style)
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
		old_style.TabMinWidthForCloseButton = new_style.TabMinWidthForCloseButton;
		old_style.ColorButtonPosition = new_style.ColorButtonPosition;
		old_style.ButtonTextAlign = new_style.ButtonTextAlign;
		old_style.SelectableTextAlign = new_style.SelectableTextAlign;
		old_style.DisplayWindowPadding = new_style.DisplayWindowPadding;
		old_style.DisplaySafeAreaPadding = new_style.DisplaySafeAreaPadding;
		old_style.MouseCursorScale = new_style.MouseCursorScale;
		old_style.AntiAliasedLines = new_style.AntiAliasedLines;
		old_style.AntiAliasedLinesUseTex = new_style.AntiAliasedLinesUseTex;
		old_style.AntiAliasedFill = new_style.AntiAliasedFill;
		old_style.CurveTessellationTol = new_style.CurveTessellationTol;
		old_style.CircleTessellationMaxError = new_style.CircleTessellationMaxError;
		for (int i = 0; i < 55; ++i)
			old_style.Colors[i] = new_style.Colors[i];
	}

	void convert(const imgui_font_18971 &new_font, imgui_font_18600 &old_font)
	{
		old_font.IndexAdvanceX = new_font.IndexAdvanceX;
		old_font.FallbackAdvanceX = new_font.FallbackAdvanceX;
		old_font.FontSize = new_font.FontSize;
		old_font.IndexLookup = new_font.IndexLookup;
		old_font.Glyphs = new_font.Glyphs;
		old_font.FallbackGlyph = new_font.FallbackGlyph;
		old_font.ContainerAtlas = new_font.ContainerAtlas;
		old_font.ConfigData = new_font.ConfigData;
		old_font.ConfigDataCount = new_font.ConfigDataCount;
		old_font.FallbackChar = new_font.FallbackChar;
		old_font.EllipsisChar = new_font.EllipsisChar;
		old_font.DirtyLookupTables = new_font.DirtyLookupTables;
		old_font.Scale = new_font.Scale;
		old_font.Ascent = new_font.Ascent;
		old_font.Descent = new_font.Descent;
		old_font.MetricsTotalSurface = new_font.MetricsTotalSurface;
		memcpy(old_font.Used4kPagesMap, new_font.Used4kPagesMap, sizeof(old_font.Used4kPagesMap));
	}
	void convert(const imgui_font_18600 &old_font, imgui_font_18971 &new_font)
	{
		new_font.IndexAdvanceX = old_font.IndexAdvanceX;
		new_font.FallbackAdvanceX = old_font.FallbackAdvanceX;
		new_font.FontSize = old_font.FontSize;
		new_font.IndexLookup = old_font.IndexLookup;
		new_font.Glyphs = old_font.Glyphs;
		new_font.FallbackGlyph = old_font.FallbackGlyph;
		new_font.ContainerAtlas = old_font.ContainerAtlas;
		new_font.ConfigData = old_font.ConfigData;
		new_font.ConfigDataCount = old_font.ConfigDataCount;
		new_font.FallbackChar = old_font.FallbackChar;
		new_font.EllipsisChar = old_font.EllipsisChar;
		new_font.EllipsisCharCount = 1;
		new_font.EllipsisWidth = 4;
		new_font.EllipsisCharStep = 4;
		new_font.DirtyLookupTables = old_font.DirtyLookupTables;
		new_font.Scale = old_font.Scale;
		new_font.Ascent = old_font.Ascent;
		new_font.Descent = old_font.Descent;
		new_font.MetricsTotalSurface = old_font.MetricsTotalSurface;
		memcpy(new_font.Used4kPagesMap, old_font.Used4kPagesMap, sizeof(new_font.Used4kPagesMap));
	}

	void convert(const imgui_list_clipper_18971 &new_clipper, imgui_list_clipper_18600 &old_clipper)
	{
		old_clipper.DisplayStart = new_clipper.DisplayStart;
		old_clipper.DisplayEnd = new_clipper.DisplayEnd;
		old_clipper.ItemsCount = new_clipper.ItemsCount;
		old_clipper.ItemsHeight = new_clipper.ItemsHeight;
		old_clipper.StartPosY = new_clipper.StartPosY;
		old_clipper.TempData = new_clipper.TempData;
	}
	void convert(const imgui_list_clipper_18600 &old_clipper, imgui_list_clipper_18971 &new_clipper)
	{
		new_clipper.Ctx = ImGui::GetCurrentContext();
		new_clipper.DisplayStart = old_clipper.DisplayStart;
		new_clipper.DisplayEnd = old_clipper.DisplayEnd;
		new_clipper.ItemsCount = old_clipper.ItemsCount;
		new_clipper.ItemsHeight = old_clipper.ItemsHeight;
		new_clipper.StartPosY = old_clipper.StartPosY;
		new_clipper.TempData = old_clipper.TempData;
	}
}

const imgui_function_table_18600 init_imgui_function_table_18600() { return {
	[]() -> imgui_io_18600 & {
		static imgui_io_18600 io = {};
		convert(g_imgui_function_table_18971.GetIO(), io);
		return io;
	},
	[]() -> imgui_style_18600 & {
		static imgui_style_18600 style = {};
		convert(g_imgui_function_table_18971.GetStyle(), style);
		return style;
	},
	g_imgui_function_table_18971.GetVersion,
	g_imgui_function_table_18971.Begin,
	g_imgui_function_table_18971.End,
	g_imgui_function_table_18971.BeginChild,
	g_imgui_function_table_18971.BeginChild2,
	g_imgui_function_table_18971.EndChild,
	g_imgui_function_table_18971.IsWindowAppearing,
	g_imgui_function_table_18971.IsWindowCollapsed,
	g_imgui_function_table_18971.IsWindowFocused,
	g_imgui_function_table_18971.IsWindowHovered,
	g_imgui_function_table_18971.GetWindowDrawList,
	g_imgui_function_table_18971.GetWindowDpiScale,
	g_imgui_function_table_18971.GetWindowPos,
	g_imgui_function_table_18971.GetWindowSize,
	g_imgui_function_table_18971.GetWindowWidth,
	g_imgui_function_table_18971.GetWindowHeight,
	g_imgui_function_table_18971.SetNextWindowPos,
	g_imgui_function_table_18971.SetNextWindowSize,
	g_imgui_function_table_18971.SetNextWindowSizeConstraints,
	g_imgui_function_table_18971.SetNextWindowContentSize,
	g_imgui_function_table_18971.SetNextWindowCollapsed,
	g_imgui_function_table_18971.SetNextWindowFocus,
	g_imgui_function_table_18971.SetNextWindowBgAlpha,
	g_imgui_function_table_18971.SetWindowPos,
	g_imgui_function_table_18971.SetWindowSize,
	g_imgui_function_table_18971.SetWindowCollapsed,
	g_imgui_function_table_18971.SetWindowFocus,
	g_imgui_function_table_18971.SetWindowFontScale,
	g_imgui_function_table_18971.SetWindowPos2,
	g_imgui_function_table_18971.SetWindowSize2,
	g_imgui_function_table_18971.SetWindowCollapsed2,
	g_imgui_function_table_18971.SetWindowFocus2,
	g_imgui_function_table_18971.GetContentRegionAvail,
	g_imgui_function_table_18971.GetContentRegionMax,
	g_imgui_function_table_18971.GetWindowContentRegionMin,
	g_imgui_function_table_18971.GetWindowContentRegionMax,
	g_imgui_function_table_18971.GetScrollX,
	g_imgui_function_table_18971.GetScrollY,
	g_imgui_function_table_18971.SetScrollX,
	g_imgui_function_table_18971.SetScrollY,
	g_imgui_function_table_18971.GetScrollMaxX,
	g_imgui_function_table_18971.GetScrollMaxY,
	g_imgui_function_table_18971.SetScrollHereX,
	g_imgui_function_table_18971.SetScrollHereY,
	g_imgui_function_table_18971.SetScrollFromPosX,
	g_imgui_function_table_18971.SetScrollFromPosY,
	[](imgui_font_18600 *) {
		// Cannot make persistent 'ImFont' here easily, so just always use default
		g_imgui_function_table_18971.PushFont(nullptr);
	},
	g_imgui_function_table_18971.PopFont,
	g_imgui_function_table_18971.PushStyleColor,
	g_imgui_function_table_18971.PushStyleColor2,
	g_imgui_function_table_18971.PopStyleColor,
	g_imgui_function_table_18971.PushStyleVar,
	g_imgui_function_table_18971.PushStyleVar2,
	g_imgui_function_table_18971.PopStyleVar,
	g_imgui_function_table_18971.PushTabStop,
	g_imgui_function_table_18971.PopTabStop,
	g_imgui_function_table_18971.PushButtonRepeat,
	g_imgui_function_table_18971.PopButtonRepeat,
	g_imgui_function_table_18971.PushItemWidth,
	g_imgui_function_table_18971.PopItemWidth,
	g_imgui_function_table_18971.SetNextItemWidth,
	g_imgui_function_table_18971.CalcItemWidth,
	g_imgui_function_table_18971.PushTextWrapPos,
	g_imgui_function_table_18971.PopTextWrapPos,
	[]() -> imgui_font_18600 * {
		static imgui_font_18600 font = {};
		convert(*g_imgui_function_table_18971.GetFont(), font);
		return &font;
	},
	g_imgui_function_table_18971.GetFontSize,
	g_imgui_function_table_18971.GetFontTexUvWhitePixel,
	g_imgui_function_table_18971.GetColorU32,
	g_imgui_function_table_18971.GetColorU322,
	g_imgui_function_table_18971.GetColorU323,
	g_imgui_function_table_18971.GetStyleColorVec4,
	g_imgui_function_table_18971.Separator,
	g_imgui_function_table_18971.SameLine,
	g_imgui_function_table_18971.NewLine,
	g_imgui_function_table_18971.Spacing,
	g_imgui_function_table_18971.Dummy,
	g_imgui_function_table_18971.Indent,
	g_imgui_function_table_18971.Unindent,
	g_imgui_function_table_18971.BeginGroup,
	g_imgui_function_table_18971.EndGroup,
	g_imgui_function_table_18971.GetCursorPos,
	g_imgui_function_table_18971.GetCursorPosX,
	g_imgui_function_table_18971.GetCursorPosY,
	g_imgui_function_table_18971.SetCursorPos,
	g_imgui_function_table_18971.SetCursorPosX,
	g_imgui_function_table_18971.SetCursorPosY,
	g_imgui_function_table_18971.GetCursorStartPos,
	g_imgui_function_table_18971.GetCursorScreenPos,
	g_imgui_function_table_18971.SetCursorScreenPos,
	g_imgui_function_table_18971.AlignTextToFramePadding,
	g_imgui_function_table_18971.GetTextLineHeight,
	g_imgui_function_table_18971.GetTextLineHeightWithSpacing,
	g_imgui_function_table_18971.GetFrameHeight,
	g_imgui_function_table_18971.GetFrameHeightWithSpacing,
	g_imgui_function_table_18971.PushID,
	g_imgui_function_table_18971.PushID2,
	g_imgui_function_table_18971.PushID3,
	g_imgui_function_table_18971.PushID4,
	g_imgui_function_table_18971.PopID,
	g_imgui_function_table_18971.GetID,
	g_imgui_function_table_18971.GetID2,
	g_imgui_function_table_18971.GetID3,
	g_imgui_function_table_18971.TextUnformatted,
	g_imgui_function_table_18971.TextV,
	g_imgui_function_table_18971.TextColoredV,
	g_imgui_function_table_18971.TextDisabledV,
	g_imgui_function_table_18971.TextWrappedV,
	g_imgui_function_table_18971.LabelTextV,
	g_imgui_function_table_18971.BulletTextV,
	g_imgui_function_table_18971.Button,
	g_imgui_function_table_18971.SmallButton,
	g_imgui_function_table_18971.InvisibleButton,
	g_imgui_function_table_18971.ArrowButton,
	g_imgui_function_table_18971.Image,
	[](ImTextureID user_texture_id, const ImVec2 &size, const ImVec2 &uv0, const ImVec2 &uv1, int frame_padding, const ImVec4 &bg_col, const ImVec4 &tint_col) -> bool
	{
		g_imgui_function_table_18971.PushID3(reinterpret_cast<void *>(user_texture_id));
		if (frame_padding >= 0)
			g_imgui_function_table_18971.PushStyleVar2(ImGuiStyleVar_FramePadding, ImVec2((float)frame_padding, (float)frame_padding));
		bool ret = g_imgui_function_table_18971.ImageButton("#image", user_texture_id, size, uv0, uv1, bg_col, tint_col);
		if (frame_padding >= 0)
			g_imgui_function_table_18971.PopStyleVar(1);
		g_imgui_function_table_18971.PopID();
		return ret;
	},
	g_imgui_function_table_18971.Checkbox,
	g_imgui_function_table_18971.CheckboxFlags,
	g_imgui_function_table_18971.CheckboxFlags2,
	g_imgui_function_table_18971.RadioButton,
	g_imgui_function_table_18971.RadioButton2,
	g_imgui_function_table_18971.ProgressBar,
	g_imgui_function_table_18971.Bullet,
	g_imgui_function_table_18971.BeginCombo,
	g_imgui_function_table_18971.EndCombo,
	g_imgui_function_table_18971.Combo,
	g_imgui_function_table_18971.Combo2,
	g_imgui_function_table_18971.Combo3,
	g_imgui_function_table_18971.DragFloat,
	g_imgui_function_table_18971.DragFloat2,
	g_imgui_function_table_18971.DragFloat3,
	g_imgui_function_table_18971.DragFloat4,
	g_imgui_function_table_18971.DragFloatRange2,
	g_imgui_function_table_18971.DragInt,
	g_imgui_function_table_18971.DragInt2,
	g_imgui_function_table_18971.DragInt3,
	g_imgui_function_table_18971.DragInt4,
	g_imgui_function_table_18971.DragIntRange2,
	g_imgui_function_table_18971.DragScalar,
	g_imgui_function_table_18971.DragScalarN,
	g_imgui_function_table_18971.SliderFloat,
	g_imgui_function_table_18971.SliderFloat2,
	g_imgui_function_table_18971.SliderFloat3,
	g_imgui_function_table_18971.SliderFloat4,
	g_imgui_function_table_18971.SliderAngle,
	g_imgui_function_table_18971.SliderInt,
	g_imgui_function_table_18971.SliderInt2,
	g_imgui_function_table_18971.SliderInt3,
	g_imgui_function_table_18971.SliderInt4,
	g_imgui_function_table_18971.SliderScalar,
	g_imgui_function_table_18971.SliderScalarN,
	g_imgui_function_table_18971.VSliderFloat,
	g_imgui_function_table_18971.VSliderInt,
	g_imgui_function_table_18971.VSliderScalar,
	g_imgui_function_table_18971.InputText,
	g_imgui_function_table_18971.InputTextMultiline,
	g_imgui_function_table_18971.InputTextWithHint,
	g_imgui_function_table_18971.InputFloat,
	g_imgui_function_table_18971.InputFloat2,
	g_imgui_function_table_18971.InputFloat3,
	g_imgui_function_table_18971.InputFloat4,
	g_imgui_function_table_18971.InputInt,
	g_imgui_function_table_18971.InputInt2,
	g_imgui_function_table_18971.InputInt3,
	g_imgui_function_table_18971.InputInt4,
	g_imgui_function_table_18971.InputDouble,
	g_imgui_function_table_18971.InputScalar,
	g_imgui_function_table_18971.InputScalarN,
	g_imgui_function_table_18971.ColorEdit3,
	g_imgui_function_table_18971.ColorEdit4,
	g_imgui_function_table_18971.ColorPicker3,
	g_imgui_function_table_18971.ColorPicker4,
	[](const char *desc_id, const ImVec4 &col, ImGuiColorEditFlags flags, ImVec2 size) -> bool { return g_imgui_function_table_18971.ColorButton(desc_id, col, flags, size); },
	g_imgui_function_table_18971.SetColorEditOptions,
	g_imgui_function_table_18971.TreeNode,
	g_imgui_function_table_18971.TreeNodeV,
	g_imgui_function_table_18971.TreeNodeV2,
	g_imgui_function_table_18971.TreeNodeEx,
	g_imgui_function_table_18971.TreeNodeExV,
	g_imgui_function_table_18971.TreeNodeExV2,
	g_imgui_function_table_18971.TreePush,
	g_imgui_function_table_18971.TreePush2,
	g_imgui_function_table_18971.TreePop,
	g_imgui_function_table_18971.GetTreeNodeToLabelSpacing,
	g_imgui_function_table_18971.CollapsingHeader,
	g_imgui_function_table_18971.CollapsingHeader2,
	g_imgui_function_table_18971.SetNextItemOpen,
	g_imgui_function_table_18971.Selectable,
	g_imgui_function_table_18971.Selectable2,
	g_imgui_function_table_18971.BeginListBox,
	g_imgui_function_table_18971.EndListBox,
	g_imgui_function_table_18971.ListBox,
	g_imgui_function_table_18971.ListBox2,
	g_imgui_function_table_18971.PlotLines,
	g_imgui_function_table_18971.PlotLines2,
	g_imgui_function_table_18971.PlotHistogram,
	g_imgui_function_table_18971.PlotHistogram2,
	g_imgui_function_table_18971.Value,
	g_imgui_function_table_18971.Value2,
	g_imgui_function_table_18971.Value3,
	g_imgui_function_table_18971.Value4,
	g_imgui_function_table_18971.BeginMenuBar,
	g_imgui_function_table_18971.EndMenuBar,
	g_imgui_function_table_18971.BeginMainMenuBar,
	g_imgui_function_table_18971.EndMainMenuBar,
	g_imgui_function_table_18971.BeginMenu,
	g_imgui_function_table_18971.EndMenu,
	g_imgui_function_table_18971.MenuItem,
	g_imgui_function_table_18971.MenuItem2,
	[]() { g_imgui_function_table_18971.BeginTooltip(); },
	g_imgui_function_table_18971.EndTooltip,
	g_imgui_function_table_18971.SetTooltipV,
	g_imgui_function_table_18971.BeginPopup,
	g_imgui_function_table_18971.BeginPopupModal,
	g_imgui_function_table_18971.EndPopup,
	g_imgui_function_table_18971.OpenPopup,
	g_imgui_function_table_18971.OpenPopup2,
	g_imgui_function_table_18971.OpenPopupOnItemClick,
	g_imgui_function_table_18971.CloseCurrentPopup,
	g_imgui_function_table_18971.BeginPopupContextItem,
	g_imgui_function_table_18971.BeginPopupContextWindow,
	g_imgui_function_table_18971.BeginPopupContextVoid,
	g_imgui_function_table_18971.IsPopupOpen,
	g_imgui_function_table_18971.BeginTable,
	g_imgui_function_table_18971.EndTable,
	g_imgui_function_table_18971.TableNextRow,
	g_imgui_function_table_18971.TableNextColumn,
	g_imgui_function_table_18971.TableSetColumnIndex,
	g_imgui_function_table_18971.TableSetupColumn,
	g_imgui_function_table_18971.TableSetupScrollFreeze,
	g_imgui_function_table_18971.TableHeadersRow,
	g_imgui_function_table_18971.TableHeader,
	g_imgui_function_table_18971.TableGetSortSpecs,
	g_imgui_function_table_18971.TableGetColumnCount,
	g_imgui_function_table_18971.TableGetColumnIndex,
	g_imgui_function_table_18971.TableGetRowIndex,
	g_imgui_function_table_18971.TableGetColumnName,
	g_imgui_function_table_18971.TableGetColumnFlags,
	g_imgui_function_table_18971.TableSetColumnEnabled,
	g_imgui_function_table_18971.TableSetBgColor,
	g_imgui_function_table_18971.Columns,
	g_imgui_function_table_18971.NextColumn,
	g_imgui_function_table_18971.GetColumnIndex,
	g_imgui_function_table_18971.GetColumnWidth,
	g_imgui_function_table_18971.SetColumnWidth,
	g_imgui_function_table_18971.GetColumnOffset,
	g_imgui_function_table_18971.SetColumnOffset,
	g_imgui_function_table_18971.GetColumnsCount,
	g_imgui_function_table_18971.BeginTabBar,
	g_imgui_function_table_18971.EndTabBar,
	g_imgui_function_table_18971.BeginTabItem,
	g_imgui_function_table_18971.EndTabItem,
	g_imgui_function_table_18971.TabItemButton,
	g_imgui_function_table_18971.SetTabItemClosed,
	g_imgui_function_table_18971.DockSpace,
	g_imgui_function_table_18971.SetNextWindowDockID,
	g_imgui_function_table_18971.SetNextWindowClass,
	g_imgui_function_table_18971.GetWindowDockID,
	g_imgui_function_table_18971.IsWindowDocked,
	g_imgui_function_table_18971.BeginDragDropSource,
	g_imgui_function_table_18971.SetDragDropPayload,
	g_imgui_function_table_18971.EndDragDropSource,
	g_imgui_function_table_18971.BeginDragDropTarget,
	g_imgui_function_table_18971.AcceptDragDropPayload,
	g_imgui_function_table_18971.EndDragDropTarget,
	g_imgui_function_table_18971.GetDragDropPayload,
	g_imgui_function_table_18971.BeginDisabled,
	g_imgui_function_table_18971.EndDisabled,
	g_imgui_function_table_18971.PushClipRect,
	g_imgui_function_table_18971.PopClipRect,
	g_imgui_function_table_18971.SetItemDefaultFocus,
	g_imgui_function_table_18971.SetKeyboardFocusHere,
	g_imgui_function_table_18971.IsItemHovered,
	g_imgui_function_table_18971.IsItemActive,
	g_imgui_function_table_18971.IsItemFocused,
	g_imgui_function_table_18971.IsItemClicked,
	g_imgui_function_table_18971.IsItemVisible,
	g_imgui_function_table_18971.IsItemEdited,
	g_imgui_function_table_18971.IsItemActivated,
	g_imgui_function_table_18971.IsItemDeactivated,
	g_imgui_function_table_18971.IsItemDeactivatedAfterEdit,
	g_imgui_function_table_18971.IsItemToggledOpen,
	g_imgui_function_table_18971.IsAnyItemHovered,
	g_imgui_function_table_18971.IsAnyItemActive,
	g_imgui_function_table_18971.IsAnyItemFocused,
	g_imgui_function_table_18971.GetItemRectMin,
	g_imgui_function_table_18971.GetItemRectMax,
	g_imgui_function_table_18971.GetItemRectSize,
	[]() { /* ImGui::SetItemAllowOverlap(); */ },
	g_imgui_function_table_18971.IsRectVisible,
	g_imgui_function_table_18971.IsRectVisible2,
	g_imgui_function_table_18971.GetTime,
	g_imgui_function_table_18971.GetFrameCount,
	g_imgui_function_table_18971.GetBackgroundDrawList,
	g_imgui_function_table_18971.GetForegroundDrawList,
	g_imgui_function_table_18971.GetBackgroundDrawList2,
	g_imgui_function_table_18971.GetForegroundDrawList2,
	g_imgui_function_table_18971.GetDrawListSharedData,
	g_imgui_function_table_18971.GetStyleColorName,
	g_imgui_function_table_18971.SetStateStorage,
	g_imgui_function_table_18971.GetStateStorage,
	g_imgui_function_table_18971.BeginChildFrame,
	g_imgui_function_table_18971.EndChildFrame,
	g_imgui_function_table_18971.CalcTextSize,
	g_imgui_function_table_18971.ColorConvertU32ToFloat4,
	g_imgui_function_table_18971.ColorConvertFloat4ToU32,
	g_imgui_function_table_18971.ColorConvertRGBtoHSV,
	g_imgui_function_table_18971.ColorConvertHSVtoRGB,
	[](ImGuiKey imgui_key) -> int { return static_cast<int>(ImGui::GetKeyIndex(imgui_key)); },
	[](int user_key_index) -> bool { return g_imgui_function_table_18971.IsKeyDown(static_cast<ImGuiKey>(user_key_index)); },
	[](int user_key_index, bool repeat) -> bool { return g_imgui_function_table_18971.IsKeyPressed(static_cast<ImGuiKey>(user_key_index), repeat); },
	[](int user_key_index) -> bool { return g_imgui_function_table_18971.IsKeyReleased(static_cast<ImGuiKey>(user_key_index)); },
	[](int user_key_index, float repeat_delay, float rate) -> int { return g_imgui_function_table_18971.GetKeyPressedAmount(static_cast<ImGuiKey>(user_key_index), repeat_delay, rate); },
	g_imgui_function_table_18971.SetNextFrameWantCaptureKeyboard,
	g_imgui_function_table_18971.IsMouseDown,
	g_imgui_function_table_18971.IsMouseClicked,
	g_imgui_function_table_18971.IsMouseReleased,
	g_imgui_function_table_18971.IsMouseDoubleClicked,
	g_imgui_function_table_18971.GetMouseClickedCount,
	g_imgui_function_table_18971.IsMouseHoveringRect,
	g_imgui_function_table_18971.IsMousePosValid,
	g_imgui_function_table_18971.IsAnyMouseDown,
	g_imgui_function_table_18971.GetMousePos,
	g_imgui_function_table_18971.GetMousePosOnOpeningCurrentPopup,
	g_imgui_function_table_18971.IsMouseDragging,
	g_imgui_function_table_18971.GetMouseDragDelta,
	g_imgui_function_table_18971.ResetMouseDragDelta,
	g_imgui_function_table_18971.GetMouseCursor,
	g_imgui_function_table_18971.SetMouseCursor,
	g_imgui_function_table_18971.SetNextFrameWantCaptureMouse,
	g_imgui_function_table_18971.GetClipboardText,
	g_imgui_function_table_18971.SetClipboardText,
	[](const char *version, size_t sz_io, size_t sz_style, size_t sz_vec2, size_t sz_vec4, size_t sz_vert, size_t sz_idx) -> bool {
		return strcmp(version, "1.86.0") == 0 && sz_io == sizeof(imgui_io_18600) && sz_style == sizeof(imgui_style_18600) && sz_vec2 == sizeof(ImVec2) && sz_vec4 == sizeof(ImVec4) && sz_vert == sizeof(ImDrawVert) && sz_idx == sizeof(ImDrawIdx);
	},
	g_imgui_function_table_18971.SetAllocatorFunctions,
	g_imgui_function_table_18971.GetAllocatorFunctions,
	g_imgui_function_table_18971.MemAlloc,
	g_imgui_function_table_18971.MemFree,
	g_imgui_function_table_18971.ImGuiStorage_GetInt,
	g_imgui_function_table_18971.ImGuiStorage_SetInt,
	g_imgui_function_table_18971.ImGuiStorage_GetBool,
	g_imgui_function_table_18971.ImGuiStorage_SetBool,
	g_imgui_function_table_18971.ImGuiStorage_GetFloat,
	g_imgui_function_table_18971.ImGuiStorage_SetFloat,
	g_imgui_function_table_18971.ImGuiStorage_GetVoidPtr,
	g_imgui_function_table_18971.ImGuiStorage_SetVoidPtr,
	g_imgui_function_table_18971.ImGuiStorage_GetIntRef,
	g_imgui_function_table_18971.ImGuiStorage_GetBoolRef,
	g_imgui_function_table_18971.ImGuiStorage_GetFloatRef,
	g_imgui_function_table_18971.ImGuiStorage_GetVoidPtrRef,
	g_imgui_function_table_18971.ImGuiStorage_SetAllInt,
	g_imgui_function_table_18971.ImGuiStorage_BuildSortByKey,
	[](imgui_list_clipper_18600 *_this) -> void {
		new(_this) imgui_list_clipper_18600();
		memset(_this, 0, sizeof(*_this));
		_this->ItemsCount = -1;
	},
	[](imgui_list_clipper_18600 *_this) -> void {
		imgui_list_clipper_18971 temp;
		convert(*_this, temp);
		g_imgui_function_table_18971.ImGuiListClipper_End(&temp);
		_this->~imgui_list_clipper_18600();
	},
	[](imgui_list_clipper_18600 *_this, int items_count, float items_height) -> void {
		imgui_list_clipper_18971 temp;
		convert(*_this, temp);
		g_imgui_function_table_18971.ImGuiListClipper_Begin(&temp, items_count, items_height);
		convert(temp, *_this);
		temp.TempData = nullptr; // Prevent 'ImGuiListClipper' destructor from doing anything
	},
	[](imgui_list_clipper_18600 *_this) -> void {
		imgui_list_clipper_18971 temp;
		convert(*_this, temp);
		g_imgui_function_table_18971.ImGuiListClipper_End(&temp);
		convert(temp, *_this);
	},
	[](imgui_list_clipper_18600 *_this) -> bool {
		imgui_list_clipper_18971 temp;
		convert(*_this, temp);
		const bool result = g_imgui_function_table_18971.ImGuiListClipper_Step(&temp);
		convert(temp, *_this);
		temp.TempData = nullptr;
		return result;
	},
	[](imgui_list_clipper_18600 *_this, int item_min, int item_max) -> void {
		imgui_list_clipper_18971 temp;
		convert(*_this, temp);
		g_imgui_function_table_18971.ImGuiListClipper_IncludeRangeByIndices(&temp, item_min, item_max);
		convert(temp, *_this);
		temp.TempData = nullptr;
	},
	[](imgui_draw_list_18600 *_this, ImVec2 clip_rect_min, ImVec2 clip_rect_max, bool intersect_with_current_clip_rect) -> void { g_imgui_function_table_18971.ImDrawList_PushClipRect(_this, clip_rect_min, clip_rect_max, intersect_with_current_clip_rect); },
	g_imgui_function_table_18971.ImDrawList_PushClipRectFullScreen,
	g_imgui_function_table_18971.ImDrawList_PopClipRect,
	g_imgui_function_table_18971.ImDrawList_PushTextureID,
	g_imgui_function_table_18971.ImDrawList_PopTextureID,
	g_imgui_function_table_18971.ImDrawList_AddLine,
	g_imgui_function_table_18971.ImDrawList_AddRect,
	g_imgui_function_table_18971.ImDrawList_AddRectFilled,
	g_imgui_function_table_18971.ImDrawList_AddRectFilledMultiColor,
	g_imgui_function_table_18971.ImDrawList_AddQuad,
	g_imgui_function_table_18971.ImDrawList_AddQuadFilled,
	g_imgui_function_table_18971.ImDrawList_AddTriangle,
	g_imgui_function_table_18971.ImDrawList_AddTriangleFilled,
	g_imgui_function_table_18971.ImDrawList_AddCircle,
	g_imgui_function_table_18971.ImDrawList_AddCircleFilled,
	g_imgui_function_table_18971.ImDrawList_AddNgon,
	g_imgui_function_table_18971.ImDrawList_AddNgonFilled,
	g_imgui_function_table_18971.ImDrawList_AddText,
	[](imgui_draw_list_18600 *_this, const imgui_font_18600 *font, float font_size, const ImVec2 &pos, ImU32 col, const char *text_begin, const char *text_end, float wrap_width, const ImVec4 *cpu_fine_clip_rect) -> void {
		if (font != nullptr) {
			imgui_font_18971 temp;
			convert(*font, temp);
			g_imgui_function_table_18971.ImDrawList_AddText2(_this, &temp, font_size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip_rect);
		}
		else {
			g_imgui_function_table_18971.ImDrawList_AddText2(_this, nullptr, font_size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip_rect);
		}
	},
	g_imgui_function_table_18971.ImDrawList_AddPolyline,
	g_imgui_function_table_18971.ImDrawList_AddConvexPolyFilled,
	g_imgui_function_table_18971.ImDrawList_AddBezierCubic,
	g_imgui_function_table_18971.ImDrawList_AddBezierQuadratic,
	g_imgui_function_table_18971.ImDrawList_AddImage,
	g_imgui_function_table_18971.ImDrawList_AddImageQuad,
	g_imgui_function_table_18971.ImDrawList_AddImageRounded,
	g_imgui_function_table_18971.ImDrawList_PathArcTo,
	g_imgui_function_table_18971.ImDrawList_PathArcToFast,
	g_imgui_function_table_18971.ImDrawList_PathBezierCubicCurveTo,
	g_imgui_function_table_18971.ImDrawList_PathBezierQuadraticCurveTo,
	g_imgui_function_table_18971.ImDrawList_PathRect,
	g_imgui_function_table_18971.ImDrawList_AddCallback,
	g_imgui_function_table_18971.ImDrawList_AddDrawCmd,
	g_imgui_function_table_18971.ImDrawList_CloneOutput,
	g_imgui_function_table_18971.ImDrawList_PrimReserve,
	g_imgui_function_table_18971.ImDrawList_PrimUnreserve,
	g_imgui_function_table_18971.ImDrawList_PrimRect,
	g_imgui_function_table_18971.ImDrawList_PrimRectUV,
	g_imgui_function_table_18971.ImDrawList_PrimQuadUV,
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
		imgui_font_18971 temp;
		convert(*_this, temp);
		return g_imgui_function_table_18971.ImFont_FindGlyph(&temp, c);
	},
	[](const imgui_font_18600 *_this, ImWchar c) -> const ImFontGlyph * {
		imgui_font_18971 temp;
		convert(*_this, temp);
		return g_imgui_function_table_18971.ImFont_FindGlyphNoFallback(&temp, c);
	},
	[](const imgui_font_18600 *_this, float size, float max_width, float wrap_width, const char *text_begin, const char *text_end, const char **remaining) -> ImVec2 {
		imgui_font_18971 temp;
		convert(*_this, temp);
		return g_imgui_function_table_18971.ImFont_CalcTextSizeA(&temp, size, max_width, wrap_width, text_begin, text_end, remaining);
	},
	[](const imgui_font_18600 *_this, float scale, const char *text, const char *text_end, float wrap_width) -> const char * {
		imgui_font_18971 temp;
		convert(*_this, temp);
		return g_imgui_function_table_18971.ImFont_CalcWordWrapPositionA(&temp, scale, text, text_end, wrap_width);
	},
	[](const imgui_font_18600 *_this, imgui_draw_list_18600 *draw_list, float size, ImVec2 pos, ImU32 col, ImWchar c) -> void {
		imgui_font_18971 temp;
		convert(*_this, temp);
		return g_imgui_function_table_18971.ImFont_RenderChar(&temp, draw_list, size, pos, col, c);
	},
	[](const imgui_font_18600 *_this, imgui_draw_list_18600 *draw_list, float size, ImVec2 pos, ImU32 col, const ImVec4 &clip_rect, const char *text_begin, const char *text_end, float wrap_width, bool cpu_fine_clip) -> void {
		imgui_font_18971 temp;
		convert(*_this, temp);
		return g_imgui_function_table_18971.ImFont_RenderText(&temp, draw_list, size, pos, col, clip_rect, text_begin, text_end, wrap_width, cpu_fine_clip);
	},

}; }

#endif
