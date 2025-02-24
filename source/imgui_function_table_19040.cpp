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
	auto convert_color(ImGuiCol old_value) -> ImGuiCol
	{
		if (old_value >= 51)
			return old_value + 3;
		if (old_value >= 38)
			return old_value + 2;
		if (old_value == 36 || old_value == 37)
			return old_value + 1;
		if (old_value <= 32 || old_value == 35)
			return old_value;
		return old_value == 33 ? 34 : 33;
	}
	auto convert_style_var(ImGuiStyleVar old_value) -> ImGuiStyleVar
	{
		if (old_value <= 22)
			return old_value;
		if (old_value == 22)
			return old_value + 1;
		return old_value + 4;
	}

	auto convert_window_flags(ImGuiWindowFlags old_flags) -> ImGuiWindowFlags
	{
		ImGuiWindowFlags new_flags = old_flags & ~(1 << 23); // ImGuiWindowFlags_NavFlattened was removed
		if (old_flags & (1 << 29)) // ImGuiWindowFlags_DockNodeHost
			new_flags |= (1 << 23);
		return new_flags;
	}
	auto convert_tab_bar_flags(ImGuiTabBarFlags old_flags) -> ImGuiTabBarFlags
	{
		ImGuiTabBarFlags new_flags = old_flags & 0x3F;
		new_flags |= (old_flags & 0xC0) << 1;
		return new_flags;
	}
	auto convert_tree_node_flags(ImGuiTreeNodeFlags old_flags) -> ImGuiTreeNodeFlags
	{
		ImGuiTreeNodeFlags new_flags = old_flags & 0x1FFF;
		if (old_flags & (1 << 13)) // ImGuiTreeNodeFlags_SpanAllColumns
			new_flags |= 1 << 14;
		if (old_flags & (1 << 14)) // ImGuiTreeNodeFlags_NavLeftJumpsBackHere
			new_flags |= 1 << 17;
		return new_flags;
	}
	auto convert_input_text_flags(ImGuiInputTextFlags old_flags) -> ImGuiInputTextFlags
	{
		ImGuiInputTextFlags new_flags = old_flags & 0x3;
		if (old_flags & (1 << 2)) // ImGuiInputTextFlags_CharsUppercase
			new_flags |= 1 << 3;
		if (old_flags & (1 << 3)) // ImGuiInputTextFlags_CharsNoBlank
			new_flags |= 1 << 4;
		if (old_flags & (1 << 4)) // ImGuiInputTextFlags_AutoSelectAll
			new_flags |= 1 << 12;
		if (old_flags & (1 << 5)) // ImGuiInputTextFlags_EnterReturnsTrue
			new_flags |= 1 << 6;
		if (old_flags & (1 << 6)) // ImGuiInputTextFlags_CallbackCompletion
			new_flags |= 1 << 18;
		if (old_flags & (1 << 7)) // ImGuiInputTextFlags_CallbackHistory
			new_flags |= 1 << 19;
		if (old_flags & (1 << 8)) // ImGuiInputTextFlags_CallbackAlways
			new_flags |= 1 << 20;
		if (old_flags & (1 << 9)) // ImGuiInputTextFlags_CallbackCharFilter
			new_flags |= 1 << 21;
		if (old_flags & (1 << 10)) // ImGuiInputTextFlags_AllowTabInput
			new_flags |= 1 << 5;
		if (old_flags & (1 << 11)) // ImGuiInputTextFlags_CtrlEnterForNewLine
			new_flags |= 1 << 8;
		if (old_flags & (1 << 12)) // ImGuiInputTextFlags_NoHorizontalScroll
			new_flags |= 1 << 15;
		if (old_flags & (1 << 13)) // ImGuiInputTextFlags_AlwaysOverwrite
			new_flags |= 1 << 11;
		if (old_flags & (1 << 14)) // ImGuiInputTextFlags_ReadOnly
			new_flags |= 1 << 9;
		if (old_flags & (1 << 15)) // ImGuiInputTextFlags_Password
			new_flags |= 1 << 10;
		if (old_flags & (1 << 16)) // ImGuiInputTextFlags_NoUndoRedo
			new_flags |= 1 << 16;
		if (old_flags & (1 << 17)) // ImGuiInputTextFlags_CharsScientific
			new_flags |= 1 << 2;
		if (old_flags & (1 << 18)) // ImGuiInputTextFlags_CallbackResize
			new_flags |= 1 << 22;
		if (old_flags & (1 << 19)) // ImGuiInputTextFlags_CallbackEdit
			new_flags |= 1 << 23;
		if (old_flags & (1 << 20)) // ImGuiInputTextFlags_EscapeClearsAll
			new_flags |= 1 << 7;
		return new_flags;
	}

	// Convert 'ImGuiIO' as well due to 'IMGUI_DISABLE_OBSOLETE_KEYIO' not being defined by default
	void convert(const imgui_io_19180 &new_io, imgui_io_19040 &old_io)
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
		old_io.ConfigMemoryCompactTimer = new_io.ConfigMemoryCompactTimer;

		old_io.MouseDoubleClickTime = new_io.MouseDoubleClickTime;
		old_io.MouseDoubleClickMaxDist = new_io.MouseDoubleClickMaxDist;
		old_io.MouseDragThreshold = new_io.MouseDragThreshold;
		old_io.KeyRepeatDelay = new_io.KeyRepeatDelay;
		old_io.KeyRepeatRate = new_io.KeyRepeatRate;

		old_io.ConfigDebugIsDebuggerPresent = new_io.ConfigDebugIsDebuggerPresent;
		old_io.ConfigDebugBeginReturnValueOnce = new_io.ConfigDebugBeginReturnValueOnce;
		old_io.ConfigDebugBeginReturnValueLoop = new_io.ConfigDebugBeginReturnValueLoop;
		old_io.ConfigDebugIgnoreFocusLoss = new_io.ConfigDebugIgnoreFocusLoss;
		old_io.ConfigDebugIniSettings = new_io.ConfigDebugIniSettings;

		old_io.BackendPlatformName = new_io.BackendPlatformName;
		old_io.BackendRendererName = new_io.BackendRendererName;
		old_io.BackendPlatformUserData = new_io.BackendPlatformUserData;
		old_io.BackendRendererUserData = new_io.BackendRendererUserData;
		old_io.BackendLanguageUserData = new_io.BackendLanguageUserData;

		old_io.GetClipboardTextFn = nullptr;
		old_io.SetClipboardTextFn = nullptr;
		old_io.ClipboardUserData = nullptr;
		old_io.SetPlatformImeDataFn = nullptr;
		old_io.PlatformLocaleDecimalPoint = L'.';

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

		for (int i = 0; i < 666; ++i)
			old_io.KeyMap[i] = 0;
		for (int i = 0; i < 666; ++i)
			old_io.KeysDown[i] = false; // ImGui::IsKeyDown(i)
		for (int i = 0; i < 16; ++i)
			old_io.NavInputs[i] = 0.0f;

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

	void convert(const imgui_style_19180 &new_style, imgui_style_19040 &old_style)
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
		old_style.TabBarBorderSize = new_style.TabBarBorderSize;
		old_style.TableAngledHeadersAngle = new_style.TableAngledHeadersAngle;
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
		for (int i = 0; i < 55; ++i)
			old_style.Colors[i] = new_style.Colors[convert_color(i)];
	}

	void convert(const imgui_font_19180 &new_font, imgui_font_19040 &old_font)
	{
		old_font.IndexAdvanceX = new_font.IndexAdvanceX;
		old_font.FallbackAdvanceX = new_font.FallbackAdvanceX;
		old_font.FontSize = new_font.FontSize;
		old_font.IndexLookup = new_font.IndexLookup;
		old_font.Glyphs = new_font.Glyphs;
		old_font.FallbackGlyph = new_font.FallbackGlyph;
		old_font.ContainerAtlas = new_font.ContainerAtlas;
		old_font.ConfigData = nullptr;
		old_font.ConfigDataCount = new_font.ConfigDataCount;
		old_font.FallbackChar = new_font.FallbackChar;
		old_font.EllipsisChar = new_font.EllipsisChar;
		old_font.EllipsisCharCount = new_font.EllipsisCharCount;
		old_font.EllipsisWidth = new_font.EllipsisWidth;
		old_font.EllipsisCharStep = new_font.EllipsisCharStep;
		old_font.DirtyLookupTables = new_font.DirtyLookupTables;
		old_font.Scale = new_font.Scale;
		old_font.Ascent = new_font.Ascent;
		old_font.Descent = new_font.Descent;
		old_font.MetricsTotalSurface = new_font.MetricsTotalSurface;
		memcpy(old_font.Used4kPagesMap, new_font.Used8kPagesMap, sizeof(old_font.Used4kPagesMap));
	}
	void convert(const imgui_font_19040 &old_font, imgui_font_19180 &new_font)
	{
		new_font.IndexAdvanceX = old_font.IndexAdvanceX;
		new_font.FallbackAdvanceX = old_font.FallbackAdvanceX;
		new_font.FontSize = old_font.FontSize;
		new_font.IndexLookup = old_font.IndexLookup;
		new_font.Glyphs = old_font.Glyphs;
		new_font.FallbackGlyph = old_font.FallbackGlyph;
		new_font.ContainerAtlas = old_font.ContainerAtlas;
		new_font.ConfigData = nullptr;
		new_font.ConfigDataCount = old_font.ConfigDataCount;
		new_font.FallbackChar = old_font.FallbackChar;
		new_font.EllipsisChar = old_font.EllipsisChar;
		new_font.EllipsisCharCount = old_font.EllipsisCharCount;
		new_font.EllipsisWidth = old_font.EllipsisWidth;
		new_font.EllipsisCharStep = old_font.EllipsisCharStep;
		new_font.DirtyLookupTables = old_font.DirtyLookupTables;
		new_font.Scale = old_font.Scale;
		new_font.Ascent = old_font.Ascent;
		new_font.Descent = old_font.Descent;
		new_font.MetricsTotalSurface = old_font.MetricsTotalSurface;
		memcpy(new_font.Used8kPagesMap, old_font.Used4kPagesMap, sizeof(old_font.Used4kPagesMap));
	}

	void convert(const imgui_list_clipper_19180 &new_clipper, imgui_list_clipper_19040 &old_clipper)
	{
		old_clipper.Ctx = new_clipper.Ctx;
		old_clipper.DisplayStart = new_clipper.DisplayStart;
		old_clipper.DisplayEnd = new_clipper.DisplayEnd;
		old_clipper.ItemsCount = new_clipper.ItemsCount;
		old_clipper.ItemsHeight = new_clipper.ItemsHeight;
		old_clipper.StartPosY = new_clipper.StartPosY;
		old_clipper.TempData = new_clipper.TempData;
	}
	void convert(const imgui_list_clipper_19040 &old_clipper, imgui_list_clipper_19180 &new_clipper)
	{
		new_clipper.Ctx = old_clipper.Ctx;
		new_clipper.DisplayStart = old_clipper.DisplayStart;
		new_clipper.DisplayEnd = old_clipper.DisplayEnd;
		new_clipper.ItemsCount = old_clipper.ItemsCount;
		new_clipper.ItemsHeight = old_clipper.ItemsHeight;
		new_clipper.StartPosY = old_clipper.StartPosY;
		new_clipper.TempData = old_clipper.TempData;
	}

	static struct imgui_draw_list_19040_internal : public imgui_draw_list_19040 { imgui_draw_list_19180 *orig = nullptr; bool reserved_prims = false; } temp_draw_list;

	imgui_draw_list_19040 *wrap(imgui_draw_list_19180 *new_draw_list)
	{
		temp_draw_list = imgui_draw_list_19040_internal {};
		temp_draw_list.orig = new_draw_list;

		return &temp_draw_list;
	}
	imgui_draw_list_19040 *wrap(imgui_draw_list_19180 *new_draw_list, imgui_draw_list_19040 *old_draw_list)
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
			old_draw_list->_TextureIdStack = new_draw_list->_TextureIdStack;
			old_draw_list->_Path = new_draw_list->_Path;
			old_draw_list->_CmdHeader = new_draw_list->_CmdHeader;
			old_draw_list->_Splitter = new_draw_list->_Splitter;
			old_draw_list->_FringeScale = new_draw_list->_FringeScale;
		}

		return old_draw_list;
	}
	imgui_draw_list_19180 *unwrap(imgui_draw_list_19040 *old_draw_list, bool reserved_prims = false)
	{
		imgui_draw_list_19180 *const new_draw_list = temp_draw_list.orig;
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
			new_draw_list->_TextureIdStack = old_draw_list->_TextureIdStack;
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

const imgui_function_table_19040 init_imgui_function_table_19040() { return {
	[]() -> imgui_io_19040 &{
		static imgui_io_19040 io = {};
		convert(g_imgui_function_table_19180.GetIO(), io);
		return io;
	},
	[]() -> imgui_style_19040 &{
		static imgui_style_19040 style = {};
		convert(g_imgui_function_table_19180.GetStyle(), style);
		return style;
	},
	g_imgui_function_table_19180.GetVersion,
	[](const char *name, bool *p_open, ImGuiWindowFlags flags) -> bool { return g_imgui_function_table_19180.Begin(name, p_open, convert_window_flags(flags)); },
	g_imgui_function_table_19180.End,
	[](const char *str_id, const ImVec2 &size, ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags) -> bool {
		if (window_flags & (1 << 23))
			child_flags |= ImGuiChildFlags_NavFlattened;
		return g_imgui_function_table_19180.BeginChild(str_id, size, child_flags, convert_window_flags(window_flags));
	},
	[](ImGuiID id, const ImVec2 &size, ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags) -> bool {
		if (window_flags & (1 << 23))
			child_flags |= ImGuiChildFlags_NavFlattened;
		return g_imgui_function_table_19180.BeginChild2(id, size, child_flags, convert_window_flags(window_flags));
	},
	g_imgui_function_table_19180.EndChild,
	g_imgui_function_table_19180.IsWindowAppearing,
	g_imgui_function_table_19180.IsWindowCollapsed,
	g_imgui_function_table_19180.IsWindowFocused,
	g_imgui_function_table_19180.IsWindowHovered,
	[]() -> imgui_draw_list_19040 * { return wrap(g_imgui_function_table_19180.GetWindowDrawList()); },
	g_imgui_function_table_19180.GetWindowDpiScale,
	g_imgui_function_table_19180.GetWindowPos,
	g_imgui_function_table_19180.GetWindowSize,
	g_imgui_function_table_19180.GetWindowWidth,
	g_imgui_function_table_19180.GetWindowHeight,
	g_imgui_function_table_19180.SetNextWindowPos,
	g_imgui_function_table_19180.SetNextWindowSize,
	g_imgui_function_table_19180.SetNextWindowSizeConstraints,
	g_imgui_function_table_19180.SetNextWindowContentSize,
	g_imgui_function_table_19180.SetNextWindowCollapsed,
	g_imgui_function_table_19180.SetNextWindowFocus,
	g_imgui_function_table_19180.SetNextWindowScroll,
	g_imgui_function_table_19180.SetNextWindowBgAlpha,
	g_imgui_function_table_19180.SetWindowPos,
	g_imgui_function_table_19180.SetWindowSize,
	g_imgui_function_table_19180.SetWindowCollapsed,
	g_imgui_function_table_19180.SetWindowFocus,
	g_imgui_function_table_19180.SetWindowFontScale,
	g_imgui_function_table_19180.SetWindowPos2,
	g_imgui_function_table_19180.SetWindowSize2,
	g_imgui_function_table_19180.SetWindowCollapsed2,
	g_imgui_function_table_19180.SetWindowFocus2,
	g_imgui_function_table_19180.GetContentRegionAvail,
	[]() -> ImVec2 { return g_imgui_function_table_19180.GetContentRegionAvail() + g_imgui_function_table_19180.GetCursorScreenPos() - g_imgui_function_table_19180.GetWindowPos(); },
	[]() -> ImVec2 { return ImVec2(0, 0); },
	[]() -> ImVec2 { return g_imgui_function_table_19180.GetContentRegionAvail(); },
	g_imgui_function_table_19180.GetScrollX,
	g_imgui_function_table_19180.GetScrollY,
	g_imgui_function_table_19180.SetScrollX,
	g_imgui_function_table_19180.SetScrollY,
	g_imgui_function_table_19180.GetScrollMaxX,
	g_imgui_function_table_19180.GetScrollMaxY,
	g_imgui_function_table_19180.SetScrollHereX,
	g_imgui_function_table_19180.SetScrollHereY,
	g_imgui_function_table_19180.SetScrollFromPosX,
	g_imgui_function_table_19180.SetScrollFromPosY,
	[](imgui_font_19040 *) {
		// Cannot make persistent 'ImFont' here easily, so just always use default
		g_imgui_function_table_19180.PushFont(nullptr);
	},
	g_imgui_function_table_19180.PopFont,
	[](ImGuiCol idx, ImU32 col) { g_imgui_function_table_19180.PushStyleColor(convert_color(idx), col); },
	[](ImGuiCol idx, const ImVec4 &col) { g_imgui_function_table_19180.PushStyleColor2(convert_color(idx), col); },
	g_imgui_function_table_19180.PopStyleColor,
	[](ImGuiStyleVar idx, float val) { g_imgui_function_table_19180.PushStyleVar(convert_style_var(idx), val); },
	[](ImGuiStyleVar idx, const ImVec2 &val) { g_imgui_function_table_19180.PushStyleVar2(convert_style_var(idx), val); },
	g_imgui_function_table_19180.PopStyleVar,
	[](bool tab_stop) { g_imgui_function_table_19180.PushItemFlag(ImGuiItemFlags_NoTabStop, !tab_stop); },
	[]() { g_imgui_function_table_19180.PopItemFlag(); },
	[](bool repeat) { g_imgui_function_table_19180.PushItemFlag(ImGuiItemFlags_ButtonRepeat, repeat); },
	[]() { g_imgui_function_table_19180.PopItemFlag(); },
	g_imgui_function_table_19180.PushItemWidth,
	g_imgui_function_table_19180.PopItemWidth,
	g_imgui_function_table_19180.SetNextItemWidth,
	g_imgui_function_table_19180.CalcItemWidth,
	g_imgui_function_table_19180.PushTextWrapPos,
	g_imgui_function_table_19180.PopTextWrapPos,
	[]() -> imgui_font_19040 *{
		static imgui_font_19040 font = {};
		convert(*g_imgui_function_table_19180.GetFont(), font);
		return &font;
	},
	g_imgui_function_table_19180.GetFontSize,
	g_imgui_function_table_19180.GetFontTexUvWhitePixel,
	[](ImGuiCol idx, float alpha_mul) -> ImU32 { return g_imgui_function_table_19180.GetColorU32(convert_color(idx), alpha_mul); },
	g_imgui_function_table_19180.GetColorU322,
	g_imgui_function_table_19180.GetColorU323,
	[](ImGuiCol idx) -> const ImVec4 & { return g_imgui_function_table_19180.GetStyleColorVec4(convert_color(idx)); },
	g_imgui_function_table_19180.GetCursorScreenPos,
	g_imgui_function_table_19180.SetCursorScreenPos,
	g_imgui_function_table_19180.GetCursorPos,
	g_imgui_function_table_19180.GetCursorPosX,
	g_imgui_function_table_19180.GetCursorPosY,
	g_imgui_function_table_19180.SetCursorPos,
	g_imgui_function_table_19180.SetCursorPosX,
	g_imgui_function_table_19180.SetCursorPosY,
	g_imgui_function_table_19180.GetCursorStartPos,
	g_imgui_function_table_19180.Separator,
	g_imgui_function_table_19180.SameLine,
	g_imgui_function_table_19180.NewLine,
	g_imgui_function_table_19180.Spacing,
	g_imgui_function_table_19180.Dummy,
	g_imgui_function_table_19180.Indent,
	g_imgui_function_table_19180.Unindent,
	g_imgui_function_table_19180.BeginGroup,
	g_imgui_function_table_19180.EndGroup,
	g_imgui_function_table_19180.AlignTextToFramePadding,
	g_imgui_function_table_19180.GetTextLineHeight,
	g_imgui_function_table_19180.GetTextLineHeightWithSpacing,
	g_imgui_function_table_19180.GetFrameHeight,
	g_imgui_function_table_19180.GetFrameHeightWithSpacing,
	g_imgui_function_table_19180.PushID,
	g_imgui_function_table_19180.PushID2,
	g_imgui_function_table_19180.PushID3,
	g_imgui_function_table_19180.PushID4,
	g_imgui_function_table_19180.PopID,
	g_imgui_function_table_19180.GetID,
	g_imgui_function_table_19180.GetID2,
	g_imgui_function_table_19180.GetID3,
	g_imgui_function_table_19180.TextUnformatted,
	g_imgui_function_table_19180.TextV,
	g_imgui_function_table_19180.TextColoredV,
	g_imgui_function_table_19180.TextDisabledV,
	g_imgui_function_table_19180.TextWrappedV,
	g_imgui_function_table_19180.LabelTextV,
	g_imgui_function_table_19180.BulletTextV,
	g_imgui_function_table_19180.SeparatorText,
	g_imgui_function_table_19180.Button,
	g_imgui_function_table_19180.SmallButton,
	g_imgui_function_table_19180.InvisibleButton,
	g_imgui_function_table_19180.ArrowButton,
	g_imgui_function_table_19180.Checkbox,
	g_imgui_function_table_19180.CheckboxFlags,
	g_imgui_function_table_19180.CheckboxFlags2,
	g_imgui_function_table_19180.RadioButton,
	g_imgui_function_table_19180.RadioButton2,
	g_imgui_function_table_19180.ProgressBar,
	g_imgui_function_table_19180.Bullet,
	g_imgui_function_table_19180.Image,
	g_imgui_function_table_19180.ImageButton,
	g_imgui_function_table_19180.BeginCombo,
	g_imgui_function_table_19180.EndCombo,
	g_imgui_function_table_19180.Combo,
	g_imgui_function_table_19180.Combo2,
	g_imgui_function_table_19180.Combo3,
	g_imgui_function_table_19180.DragFloat,
	g_imgui_function_table_19180.DragFloat2,
	g_imgui_function_table_19180.DragFloat3,
	g_imgui_function_table_19180.DragFloat4,
	g_imgui_function_table_19180.DragFloatRange2,
	g_imgui_function_table_19180.DragInt,
	g_imgui_function_table_19180.DragInt2,
	g_imgui_function_table_19180.DragInt3,
	g_imgui_function_table_19180.DragInt4,
	g_imgui_function_table_19180.DragIntRange2,
	g_imgui_function_table_19180.DragScalar,
	g_imgui_function_table_19180.DragScalarN,
	g_imgui_function_table_19180.SliderFloat,
	g_imgui_function_table_19180.SliderFloat2,
	g_imgui_function_table_19180.SliderFloat3,
	g_imgui_function_table_19180.SliderFloat4,
	g_imgui_function_table_19180.SliderAngle,
	g_imgui_function_table_19180.SliderInt,
	g_imgui_function_table_19180.SliderInt2,
	g_imgui_function_table_19180.SliderInt3,
	g_imgui_function_table_19180.SliderInt4,
	g_imgui_function_table_19180.SliderScalar,
	g_imgui_function_table_19180.SliderScalarN,
	g_imgui_function_table_19180.VSliderFloat,
	g_imgui_function_table_19180.VSliderInt,
	g_imgui_function_table_19180.VSliderScalar,
	[](const char *label, char *buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data) -> bool { return g_imgui_function_table_19180.InputText(label, buf, buf_size, convert_input_text_flags(flags), callback, user_data); },
	[](const char *label, char *buf, size_t buf_size, const ImVec2 &size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data) -> bool { return g_imgui_function_table_19180.InputTextMultiline(label, buf, buf_size, size, convert_input_text_flags(flags), callback, user_data); },
	[](const char *label, const char *hint, char *buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data) -> bool { return g_imgui_function_table_19180.InputTextWithHint(label, hint, buf, buf_size, convert_input_text_flags(flags), callback, user_data); },
	[](const char *label, float *v, float step, float step_fast, const char *format, ImGuiInputTextFlags flags) -> bool { return g_imgui_function_table_19180.InputFloat(label, v, step, step_fast, format, convert_input_text_flags(flags)); },
	[](const char *label, float v[2], const char *format, ImGuiInputTextFlags flags) -> bool { return g_imgui_function_table_19180.InputFloat2(label, v, format, convert_input_text_flags(flags)); },
	[](const char *label, float v[3], const char *format, ImGuiInputTextFlags flags) -> bool { return g_imgui_function_table_19180.InputFloat3(label, v, format, convert_input_text_flags(flags)); },
	[](const char *label, float v[4], const char *format, ImGuiInputTextFlags flags) -> bool { return g_imgui_function_table_19180.InputFloat4(label, v, format, convert_input_text_flags(flags)); },
	[](const char *label, int *v, int step, int step_fast, ImGuiInputTextFlags flags) -> bool { return g_imgui_function_table_19180.InputInt(label, v, step, step_fast, convert_input_text_flags(flags)); },
	[](const char *label, int v[2], ImGuiInputTextFlags flags) -> bool { return g_imgui_function_table_19180.InputInt2(label, v, convert_input_text_flags(flags)); },
	[](const char *label, int v[3], ImGuiInputTextFlags flags) -> bool { return g_imgui_function_table_19180.InputInt3(label, v, convert_input_text_flags(flags)); },
	[](const char *label, int v[4], ImGuiInputTextFlags flags) -> bool { return g_imgui_function_table_19180.InputInt4(label, v, convert_input_text_flags(flags)); },
	[](const char *label, double *v, double step, double step_fast, const char *format, ImGuiInputTextFlags flags) -> bool { return g_imgui_function_table_19180.InputDouble(label, v, step, step_fast, format, convert_input_text_flags(flags)); },
	[](const char *label, ImGuiDataType data_type, void *p_data, const void *p_step, const void *p_step_fast, const char *format, ImGuiInputTextFlags flags) -> bool { return g_imgui_function_table_19180.InputScalar(label, data_type, p_data, p_step, p_step_fast, format, convert_input_text_flags(flags)); },
	[](const char *label, ImGuiDataType data_type, void *p_data, int components, const void *p_step, const void *p_step_fast, const char *format, ImGuiInputTextFlags flags) -> bool { return g_imgui_function_table_19180.InputScalarN(label, data_type, p_data, components, p_step, p_step_fast, format, convert_input_text_flags(flags)); },
	g_imgui_function_table_19180.ColorEdit3,
	g_imgui_function_table_19180.ColorEdit4,
	g_imgui_function_table_19180.ColorPicker3,
	g_imgui_function_table_19180.ColorPicker4,
	g_imgui_function_table_19180.ColorButton,
	g_imgui_function_table_19180.SetColorEditOptions,
	g_imgui_function_table_19180.TreeNode,
	g_imgui_function_table_19180.TreeNodeV,
	g_imgui_function_table_19180.TreeNodeV2,
	[](const char *label, ImGuiTreeNodeFlags flags) -> bool { return g_imgui_function_table_19180.TreeNodeEx(label, convert_tree_node_flags(flags)); },
	[](const char *str_id, ImGuiTreeNodeFlags flags, const char *fmt, va_list args) -> bool { return g_imgui_function_table_19180.TreeNodeExV(str_id, convert_tree_node_flags(flags), fmt, args); },
	[](const void *ptr_id, ImGuiTreeNodeFlags flags, const char *fmt, va_list args) -> bool { return g_imgui_function_table_19180.TreeNodeExV2(ptr_id, convert_tree_node_flags(flags), fmt, args); },
	g_imgui_function_table_19180.TreePush,
	g_imgui_function_table_19180.TreePush2,
	g_imgui_function_table_19180.TreePop,
	g_imgui_function_table_19180.GetTreeNodeToLabelSpacing,
	[](const char *label, ImGuiTreeNodeFlags flags) -> bool { return g_imgui_function_table_19180.CollapsingHeader(label, convert_tree_node_flags(flags)); },
	[](const char *label, bool *p_visible, ImGuiTreeNodeFlags flags) -> bool { return g_imgui_function_table_19180.CollapsingHeader2(label, p_visible, convert_tree_node_flags(flags)); },
	g_imgui_function_table_19180.SetNextItemOpen,
	g_imgui_function_table_19180.Selectable,
	g_imgui_function_table_19180.Selectable2,
	g_imgui_function_table_19180.BeginListBox,
	g_imgui_function_table_19180.EndListBox,
	g_imgui_function_table_19180.ListBox,
	g_imgui_function_table_19180.ListBox2,
	g_imgui_function_table_19180.PlotLines,
	g_imgui_function_table_19180.PlotLines2,
	g_imgui_function_table_19180.PlotHistogram,
	g_imgui_function_table_19180.PlotHistogram2,
	g_imgui_function_table_19180.Value,
	g_imgui_function_table_19180.Value2,
	g_imgui_function_table_19180.Value3,
	g_imgui_function_table_19180.Value4,
	g_imgui_function_table_19180.BeginMenuBar,
	g_imgui_function_table_19180.EndMenuBar,
	g_imgui_function_table_19180.BeginMainMenuBar,
	g_imgui_function_table_19180.EndMainMenuBar,
	g_imgui_function_table_19180.BeginMenu,
	g_imgui_function_table_19180.EndMenu,
	g_imgui_function_table_19180.MenuItem,
	g_imgui_function_table_19180.MenuItem2,
	g_imgui_function_table_19180.BeginTooltip,
	g_imgui_function_table_19180.EndTooltip,
	g_imgui_function_table_19180.SetTooltipV,
	g_imgui_function_table_19180.BeginItemTooltip,
	g_imgui_function_table_19180.SetItemTooltipV,
	[](const char *str_id, ImGuiWindowFlags flags) -> bool { return g_imgui_function_table_19180.BeginPopup(str_id, convert_window_flags(flags)); },
	[](const char *name, bool *p_open, ImGuiWindowFlags flags) -> bool { return g_imgui_function_table_19180.BeginPopupModal(name, p_open, convert_window_flags(flags)); },
	g_imgui_function_table_19180.EndPopup,
	g_imgui_function_table_19180.OpenPopup,
	g_imgui_function_table_19180.OpenPopup2,
	g_imgui_function_table_19180.OpenPopupOnItemClick,
	g_imgui_function_table_19180.CloseCurrentPopup,
	g_imgui_function_table_19180.BeginPopupContextItem,
	g_imgui_function_table_19180.BeginPopupContextWindow,
	g_imgui_function_table_19180.BeginPopupContextVoid,
	g_imgui_function_table_19180.IsPopupOpen,
	g_imgui_function_table_19180.BeginTable,
	g_imgui_function_table_19180.EndTable,
	g_imgui_function_table_19180.TableNextRow,
	g_imgui_function_table_19180.TableNextColumn,
	g_imgui_function_table_19180.TableSetColumnIndex,
	g_imgui_function_table_19180.TableSetupColumn,
	g_imgui_function_table_19180.TableSetupScrollFreeze,
	g_imgui_function_table_19180.TableHeader,
	g_imgui_function_table_19180.TableHeadersRow,
	g_imgui_function_table_19180.TableAngledHeadersRow,
	g_imgui_function_table_19180.TableGetSortSpecs,
	g_imgui_function_table_19180.TableGetColumnCount,
	g_imgui_function_table_19180.TableGetColumnIndex,
	g_imgui_function_table_19180.TableGetRowIndex,
	g_imgui_function_table_19180.TableGetColumnName,
	g_imgui_function_table_19180.TableGetColumnFlags,
	g_imgui_function_table_19180.TableSetColumnEnabled,
	g_imgui_function_table_19180.TableSetBgColor,
	g_imgui_function_table_19180.Columns,
	g_imgui_function_table_19180.NextColumn,
	g_imgui_function_table_19180.GetColumnIndex,
	g_imgui_function_table_19180.GetColumnWidth,
	g_imgui_function_table_19180.SetColumnWidth,
	g_imgui_function_table_19180.GetColumnOffset,
	g_imgui_function_table_19180.SetColumnOffset,
	g_imgui_function_table_19180.GetColumnsCount,
	[](const char *str_id, ImGuiTabBarFlags flags) -> bool { return g_imgui_function_table_19180.BeginTabBar(str_id, convert_tab_bar_flags(flags)); },
	g_imgui_function_table_19180.EndTabBar,
	g_imgui_function_table_19180.BeginTabItem,
	g_imgui_function_table_19180.EndTabItem,
	g_imgui_function_table_19180.TabItemButton,
	g_imgui_function_table_19180.SetTabItemClosed,
	g_imgui_function_table_19180.DockSpace,
	g_imgui_function_table_19180.SetNextWindowDockID,
	g_imgui_function_table_19180.SetNextWindowClass,
	g_imgui_function_table_19180.GetWindowDockID,
	g_imgui_function_table_19180.IsWindowDocked,
	g_imgui_function_table_19180.BeginDragDropSource,
	g_imgui_function_table_19180.SetDragDropPayload,
	g_imgui_function_table_19180.EndDragDropSource,
	g_imgui_function_table_19180.BeginDragDropTarget,
	g_imgui_function_table_19180.AcceptDragDropPayload,
	g_imgui_function_table_19180.EndDragDropTarget,
	g_imgui_function_table_19180.GetDragDropPayload,
	g_imgui_function_table_19180.BeginDisabled,
	g_imgui_function_table_19180.EndDisabled,
	g_imgui_function_table_19180.PushClipRect,
	g_imgui_function_table_19180.PopClipRect,
	g_imgui_function_table_19180.SetItemDefaultFocus,
	g_imgui_function_table_19180.SetKeyboardFocusHere,
	g_imgui_function_table_19180.SetNextItemAllowOverlap,
	g_imgui_function_table_19180.IsItemHovered,
	g_imgui_function_table_19180.IsItemActive,
	g_imgui_function_table_19180.IsItemFocused,
	g_imgui_function_table_19180.IsItemClicked,
	g_imgui_function_table_19180.IsItemVisible,
	g_imgui_function_table_19180.IsItemEdited,
	g_imgui_function_table_19180.IsItemActivated,
	g_imgui_function_table_19180.IsItemDeactivated,
	g_imgui_function_table_19180.IsItemDeactivatedAfterEdit,
	g_imgui_function_table_19180.IsItemToggledOpen,
	g_imgui_function_table_19180.IsAnyItemHovered,
	g_imgui_function_table_19180.IsAnyItemActive,
	g_imgui_function_table_19180.IsAnyItemFocused,
	g_imgui_function_table_19180.GetItemID,
	g_imgui_function_table_19180.GetItemRectMin,
	g_imgui_function_table_19180.GetItemRectMax,
	g_imgui_function_table_19180.GetItemRectSize,
	[]() -> imgui_draw_list_19040 * { return wrap(g_imgui_function_table_19180.GetBackgroundDrawList(nullptr)); },
	[]() -> imgui_draw_list_19040 * { return wrap(g_imgui_function_table_19180.GetForegroundDrawList(nullptr)); },
	[](ImGuiViewport *viewport) -> imgui_draw_list_19040 * { return wrap(g_imgui_function_table_19180.GetBackgroundDrawList(viewport)); },
	[](ImGuiViewport *viewport) -> imgui_draw_list_19040 * { return wrap(g_imgui_function_table_19180.GetForegroundDrawList(viewport)); },
	g_imgui_function_table_19180.IsRectVisible,
	g_imgui_function_table_19180.IsRectVisible2,
	g_imgui_function_table_19180.GetTime,
	g_imgui_function_table_19180.GetFrameCount,
	g_imgui_function_table_19180.GetDrawListSharedData,
	[](ImGuiCol idx) -> const char * { return g_imgui_function_table_19180.GetStyleColorName(convert_color(idx)); },
	g_imgui_function_table_19180.SetStateStorage,
	g_imgui_function_table_19180.GetStateStorage,
	g_imgui_function_table_19180.CalcTextSize,
	g_imgui_function_table_19180.ColorConvertU32ToFloat4,
	g_imgui_function_table_19180.ColorConvertFloat4ToU32,
	g_imgui_function_table_19180.ColorConvertRGBtoHSV,
	g_imgui_function_table_19180.ColorConvertHSVtoRGB,
	g_imgui_function_table_19180.IsKeyDown,
	g_imgui_function_table_19180.IsKeyPressed,
	g_imgui_function_table_19180.IsKeyReleased,
	g_imgui_function_table_19180.IsKeyChordPressed,
	g_imgui_function_table_19180.GetKeyPressedAmount,
	g_imgui_function_table_19180.GetKeyName,
	g_imgui_function_table_19180.SetNextFrameWantCaptureKeyboard,
	g_imgui_function_table_19180.IsMouseDown,
	g_imgui_function_table_19180.IsMouseClicked,
	g_imgui_function_table_19180.IsMouseReleased,
	g_imgui_function_table_19180.IsMouseDoubleClicked,
	g_imgui_function_table_19180.GetMouseClickedCount,
	g_imgui_function_table_19180.IsMouseHoveringRect,
	g_imgui_function_table_19180.IsMousePosValid,
	g_imgui_function_table_19180.IsAnyMouseDown,
	g_imgui_function_table_19180.GetMousePos,
	g_imgui_function_table_19180.GetMousePosOnOpeningCurrentPopup,
	g_imgui_function_table_19180.IsMouseDragging,
	g_imgui_function_table_19180.GetMouseDragDelta,
	g_imgui_function_table_19180.ResetMouseDragDelta,
	g_imgui_function_table_19180.GetMouseCursor,
	g_imgui_function_table_19180.SetMouseCursor,
	g_imgui_function_table_19180.SetNextFrameWantCaptureMouse,
	g_imgui_function_table_19180.GetClipboardText,
	g_imgui_function_table_19180.SetClipboardText,
	g_imgui_function_table_19180.SetAllocatorFunctions,
	g_imgui_function_table_19180.GetAllocatorFunctions,
	g_imgui_function_table_19180.MemAlloc,
	g_imgui_function_table_19180.MemFree,
	g_imgui_function_table_19180.ImGuiStorage_GetInt,
	g_imgui_function_table_19180.ImGuiStorage_SetInt,
	g_imgui_function_table_19180.ImGuiStorage_GetBool,
	g_imgui_function_table_19180.ImGuiStorage_SetBool,
	g_imgui_function_table_19180.ImGuiStorage_GetFloat,
	g_imgui_function_table_19180.ImGuiStorage_SetFloat,
	g_imgui_function_table_19180.ImGuiStorage_GetVoidPtr,
	g_imgui_function_table_19180.ImGuiStorage_SetVoidPtr,
	g_imgui_function_table_19180.ImGuiStorage_GetIntRef,
	g_imgui_function_table_19180.ImGuiStorage_GetBoolRef,
	g_imgui_function_table_19180.ImGuiStorage_GetFloatRef,
	g_imgui_function_table_19180.ImGuiStorage_GetVoidPtrRef,
	g_imgui_function_table_19180.ImGuiStorage_BuildSortByKey,
	g_imgui_function_table_19180.ImGuiStorage_SetAllInt,
	[](imgui_list_clipper_19040 *_this) -> void {
		new(_this) imgui_list_clipper_19040();
		memset(_this, 0, sizeof(*_this));
	},
	[](imgui_list_clipper_19040 *_this) -> void {
		imgui_list_clipper_19180 temp;
		convert(*_this, temp);
		g_imgui_function_table_19180.ImGuiListClipper_End(&temp);
		_this->~imgui_list_clipper_19040();
	},
	[](imgui_list_clipper_19040 *_this, int items_count, float items_height) -> void {
		imgui_list_clipper_19180 temp;
		convert(*_this, temp);
		g_imgui_function_table_19180.ImGuiListClipper_Begin(&temp, items_count, items_height);
		convert(temp, *_this);
		temp.TempData = nullptr; // Prevent 'ImGuiListClipper' destructor from doing anything
	},
	[](imgui_list_clipper_19040 *_this) -> void {
		imgui_list_clipper_19180 temp;
		convert(*_this, temp);
		g_imgui_function_table_19180.ImGuiListClipper_End(&temp);
		convert(temp, *_this);
	},
	[](imgui_list_clipper_19040 *_this) -> bool {
		imgui_list_clipper_19180 temp;
		convert(*_this, temp);
		const bool result = g_imgui_function_table_19180.ImGuiListClipper_Step(&temp);
		convert(temp, *_this);
		temp.TempData = nullptr;
		return result;
	},
	[](imgui_list_clipper_19040 *_this, int item_begin, int item_end) -> void {
		imgui_list_clipper_19180 temp;
		convert(*_this, temp);
		g_imgui_function_table_19180.ImGuiListClipper_IncludeItemsByIndex(&temp, item_begin, item_end);
		convert(temp, *_this);
		temp.TempData = nullptr;
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &clip_rect_min, const ImVec2 &clip_rect_max, bool intersect_with_current_clip_rect) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PushClipRect(draw_list_19180, clip_rect_min, clip_rect_max, intersect_with_current_clip_rect);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PushClipRectFullScreen(draw_list_19180);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PopClipRect(draw_list_19180);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, ImTextureID texture_id) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PushTextureID(draw_list_19180, texture_id);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PopTextureID(draw_list_19180);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p1, const ImVec2 &p2, ImU32 col, float thickness) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddLine(draw_list_19180, p1, p2, col, thickness);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p_min, const ImVec2 &p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddRect(draw_list_19180, p_min, p_max, col, rounding, flags, thickness);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p_min, const ImVec2 &p_max, ImU32 col, float rounding, ImDrawFlags flags) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddRectFilled(draw_list_19180, p_min, p_max, col, rounding, flags);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p_min, const ImVec2 &p_max, ImU32 col_upr_left, ImU32 col_upr_right, ImU32 col_bot_right, ImU32 col_bot_left) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddRectFilledMultiColor(draw_list_19180, p_min, p_max, col_upr_left, col_upr_right, col_bot_right, col_bot_left);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col, float thickness) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddQuad(draw_list_19180, p1, p2, p3, p4, col, thickness);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddQuadFilled(draw_list_19180, p1, p2, p3, p4, col);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col, float thickness) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddTriangle(draw_list_19180, p1, p2, p3, col, thickness);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddTriangleFilled(draw_list_19180, p1, p2, p3, col);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments, float thickness) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddCircle(draw_list_19180, center, radius, col, num_segments, thickness);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddCircleFilled(draw_list_19180, center, radius, col, num_segments);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments, float thickness) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddNgon(draw_list_19180, center, radius, col, num_segments, thickness);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &center, float radius, ImU32 col, int num_segments) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddNgonFilled(draw_list_19180, center, radius, col, num_segments);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &center, float radius_x, float radius_y, ImU32 col, float rot, int num_segments, float thickness) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddEllipse(draw_list_19180, center, ImVec2(radius_x, radius_y), col, rot, num_segments, thickness);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &center, float radius_x, float radius_y, ImU32 col, float rot, int num_segments) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddEllipseFilled(draw_list_19180, center, ImVec2(radius_x, radius_y), col, rot, num_segments);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &pos, ImU32 col, const char *text_begin, const char *text_end) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddText(draw_list_19180, pos, col, text_begin, text_end);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const imgui_font_19040 *font, float font_size, const ImVec2 &pos, ImU32 col, const char *text_begin, const char *text_end, float wrap_width, const ImVec4 *cpu_fine_clip_rect) -> void {
		const auto draw_list_19180 = unwrap(_this);
		if (font != nullptr) {
			imgui_font_19180 font_19180;
			convert(*font, font_19180);
			g_imgui_function_table_19180.ImDrawList_AddText2(draw_list_19180, &font_19180, font_size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip_rect);
		}
		else {
			g_imgui_function_table_19180.ImDrawList_AddText2(draw_list_19180, nullptr, font_size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip_rect);
		}
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 *points, int num_points, ImU32 col, ImDrawFlags flags, float thickness) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddPolyline(draw_list_19180, points, num_points, col, flags, thickness);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 *points, int num_points, ImU32 col) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddConvexPolyFilled(draw_list_19180, points, num_points, col);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, ImU32 col, float thickness, int num_segments) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddBezierCubic(draw_list_19180, p1, p2, p3, p4, col, thickness, num_segments);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, ImU32 col, float thickness, int num_segments) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddBezierQuadratic(draw_list_19180, p1, p2, p3, col, thickness, num_segments);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, ImTextureID user_texture_id, const ImVec2 &p_min, const ImVec2 &p_max, const ImVec2 &uv_min, const ImVec2 &uv_max, ImU32 col) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddImage(draw_list_19180, user_texture_id, p_min, p_max, uv_min, uv_max, col);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, ImTextureID user_texture_id, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, const ImVec2 &uv1, const ImVec2 &uv2, const ImVec2 &uv3, const ImVec2 &uv4, ImU32 col) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddImageQuad(draw_list_19180, user_texture_id, p1, p2, p3, p4, uv1, uv2, uv3, uv4, col);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, ImTextureID user_texture_id, const ImVec2 &p_min, const ImVec2 &p_max, const ImVec2 &uv_min, const ImVec2 &uv_max, ImU32 col, float rounding, ImDrawFlags flags) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddImageRounded(draw_list_19180, user_texture_id, p_min, p_max, uv_min, uv_max, col, rounding, flags);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &center, float radius, float a_min, float a_max, int num_segments) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PathArcTo(draw_list_19180, center, radius, a_min, a_max, num_segments);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &center, float radius, int a_min_of_12, int a_max_of_12) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PathArcToFast(draw_list_19180, center, radius, a_min_of_12, a_max_of_12);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &center, float radius_x, float radius_y, float rot, float a_min, float a_max, int num_segments) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PathEllipticalArcTo(draw_list_19180, center, ImVec2(radius_x, radius_y), rot, a_min, a_max, num_segments);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, int num_segments) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PathBezierCubicCurveTo(draw_list_19180, p2, p3, p4, num_segments);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &p2, const ImVec2 &p3, int num_segments) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PathBezierQuadraticCurveTo(draw_list_19180, p2, p3, num_segments);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &rect_min, const ImVec2 &rect_max, float rounding, ImDrawFlags flags) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PathRect(draw_list_19180, rect_min, rect_max, rounding, flags);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, ImDrawCallback callback, void *callback_data) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddCallback(draw_list_19180, callback, callback_data, 0);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_AddDrawCmd(draw_list_19180);
		wrap(draw_list_19180, _this);
	},
	[](const imgui_draw_list_19040 *) -> imgui_draw_list_19040 * {
		// Cloning does not work with just a single global draw list for conversion
		return nullptr;
	},
	[](imgui_draw_list_19040 *_this, int idx_count, int vtx_count) -> void {
		const auto draw_list_19180 = unwrap(_this, true);
		g_imgui_function_table_19180.ImDrawList_PrimReserve(draw_list_19180, idx_count, vtx_count);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, int idx_count, int vtx_count) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PrimUnreserve(draw_list_19180, idx_count, vtx_count);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &a, const ImVec2 &b, ImU32 col) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PrimRect(draw_list_19180, a, b, col);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &a, const ImVec2 &b, const ImVec2 &uv_a, const ImVec2 &uv_b, ImU32 col) -> void {
		const auto draw_list_19180 = unwrap(_this);
		g_imgui_function_table_19180.ImDrawList_PrimRectUV(draw_list_19180, a, b, uv_a, uv_b, col);
		wrap(draw_list_19180, _this);
	},
	[](imgui_draw_list_19040 *_this, const ImVec2 &a, const ImVec2 &b, const ImVec2 &c, const ImVec2 &d, const ImVec2 &uv_a, const ImVec2 &uv_b, const ImVec2 &uv_c, const ImVec2 &uv_d, ImU32 col) -> void {
		const auto draw_list_19180 = unwrap(_this, true);
		g_imgui_function_table_19180.ImDrawList_PrimQuadUV(draw_list_19180, a, b, c, d, uv_a, uv_b, uv_c, uv_d, col);
		wrap(draw_list_19180, _this);
	},
	[](imgui_font_19040 *_this) -> void {
		new(_this) imgui_font_19040();
		_this->FontSize = 0.0f;
		_this->FallbackAdvanceX = 0.0f;
		_this->FallbackChar = 0;
		_this->EllipsisChar = 0;
		_this->EllipsisWidth = 0.0f;
		_this->EllipsisCharStep = 0.0f;
		_this->EllipsisCharCount = 0;
		_this->FallbackGlyph = nullptr;
		_this->ContainerAtlas = nullptr;
		_this->ConfigData = nullptr;
		_this->ConfigDataCount = 0;
		_this->DirtyLookupTables = false;
		_this->Scale = 1.0f;
		_this->Ascent = 0.0f;
		_this->Descent = 0.0f;
		_this->MetricsTotalSurface = 0;
		memset(_this->Used4kPagesMap, 0, sizeof(_this->Used4kPagesMap));
	},
	[](imgui_font_19040 *_this) -> void {
		_this->~imgui_font_19040();
	},
	[](const imgui_font_19040 *_this, ImWchar c) -> const ImFontGlyph * {
		imgui_font_19180 temp;
		convert(*_this, temp);
		return g_imgui_function_table_19180.ImFont_FindGlyph(&temp, c);
	},
	[](const imgui_font_19040 *_this, ImWchar c) -> const ImFontGlyph * {
		imgui_font_19180 temp;
		convert(*_this, temp);
		return g_imgui_function_table_19180.ImFont_FindGlyphNoFallback(&temp, c);
	},
	[](const imgui_font_19040 *_this, float size, float max_width, float wrap_width, const char *text_begin, const char *text_end, const char **remaining) -> ImVec2 {
		imgui_font_19180 temp;
		convert(*_this, temp);
		return g_imgui_function_table_19180.ImFont_CalcTextSizeA(&temp, size, max_width, wrap_width, text_begin, text_end, remaining);
	},
	[](const imgui_font_19040 *_this, float scale, const char *text, const char *text_end, float wrap_width) -> const char * {
		imgui_font_19180 temp;
		convert(*_this, temp);
		return g_imgui_function_table_19180.ImFont_CalcWordWrapPositionA(&temp, scale, text, text_end, wrap_width);
	},
	[](const imgui_font_19040 *_this, imgui_draw_list_19040 *draw_list, float size, const ImVec2 &pos, ImU32 col, ImWchar c) -> void {
		imgui_font_19180 temp;
		convert(*_this, temp);
		const auto draw_list_19180 = unwrap(draw_list);
		g_imgui_function_table_19180.ImFont_RenderChar(&temp, draw_list_19180, size, pos, col, c);
		wrap(draw_list_19180, draw_list);
	},
	[](const imgui_font_19040 *_this, imgui_draw_list_19040 *draw_list, float size, const ImVec2 &pos, ImU32 col, const ImVec4 &clip_rect, const char *text_begin, const char *text_end, float wrap_width, bool cpu_fine_clip) -> void {
		imgui_font_19180 temp;
		convert(*_this, temp);
		const auto draw_list_19180 = unwrap(draw_list);
		g_imgui_function_table_19180.ImFont_RenderText(&temp, draw_list_19180, size, pos, col, clip_rect, text_begin, text_end, wrap_width, cpu_fine_clip);
		wrap(draw_list_19180, draw_list);
	},

}; }

#endif
