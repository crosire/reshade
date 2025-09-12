$version = 0

$function_list = @{}

$function_table = ""
$function_table_init = ""
$function_definitions = ""
$function_definitions_struct = ""

$is_inside_struct = ""
$is_inside_namespace = 0

Get-Content ..\deps\imgui\imgui.h | ForEach-Object {
	if ($_.StartsWith("#define IMGUI_VERSION_NUM")) {
		$version = [int]$_.Substring(26)
		return
	}

	if ($is_inside_namespace -eq 0 -and $_.StartsWith("namespace ImGui")) {
		$is_inside_namespace = 1
		return
	}
	if ($is_inside_namespace -eq 1 -and $_.StartsWith("} // namespace ImGui")) {
		$is_inside_namespace = 2
		return
	}

	if ($is_inside_namespace -eq 1 -and $_ -match 'IMGUI_API\s([\s\w\&\*]+)\s+(\w+)\((.*)\);(?:(?:\s*\/\/)|$)') {
		$type = $matches[1].TrimEnd()
		$name = $matches[2]
		$args = $matches[3]

		# Filter out various functions
		if ($name.StartsWith("Debug") -or $name.StartsWith("Log") -or $name.StartsWith("Show") -or $name.StartsWith("StyleColors") -or $name.EndsWith("Context") -or $name.Contains("IniSettings") -or $name.Contains("Viewport") -or $name.Contains("Platform") -or $name -eq "NewFrame" -or $name -eq "EndFrame" -or $name -eq "Render" -or $name -eq "GetDrawData") {
			return;
		}

		$internal_idx = 1
		$internal_name = $name
		if ($function_list.ContainsKey($name)) {
			$internal_idx = $function_list.Get_Item($name) + 1
			$function_list.Set_Item($name, $internal_idx)
		}
		else {
			$function_list.Add($name, $internal_idx)
		}

		$has_return = $type -ne "void"
		$has_ellipsis = ($args -is [string]) -and $args.Contains("...")
		if ($args -is [string] -and $args.Contains("IM_FMT")) {
			$args = $args.Remove($args.IndexOf("IM_FMT"))
			$args = $args.Remove($args.LastIndexOf(')'))
		}

		$args_decl = ""
		$args_decl_with_defaults = ""
		$args_call = ""
		$last_arg_name = ""

		$args | Select-String -Pattern '((?:const\s)?(?:unsigned\s)?\w+\*{0,2}&?(?:\sconst)?)(?:(?:\s?\(\*(\w+)\)\(([^()]*)\))|\s(\w+)(\[\d*\])?)(?:\s=\s((?:[^,]+\([\+\-0-9\.,\s]*\))|(?:[\+\-0-9\._a-zA-Z]+)|(?:".*")))?' -AllMatches | ForEach-Object { $_.Matches } | ForEach-Object {
			if ($args_decl) {
				$args_decl += ", "
				$args_decl_with_defaults += ", "
				$args_call += ", "
			}

			$arg_type = $_.Groups[1].Value

			if ($_.Groups[2].Value) {
				$arg_name = $last_arg_name = $_.Groups[2].Value
				$arg_args = $_.Groups[3].Value

				$args_decl += $arg_type + "(*" + $arg_name + ")(" + $arg_args + ")"
				$args_decl_with_defaults += $arg_type + "(*" + $arg_name + ")(" + $arg_args + ")"
				$args_call += $arg_name
			}
			else {
				$arg_name = $last_arg_name = $_.Groups[4].Value
				$arg_default = $_.Groups[6].Value

				$args_decl += $arg_type + " " + $arg_name + $_.Groups[5].Value
				$args_decl_with_defaults += $arg_type + " " + $arg_name + $_.Groups[5].Value
				if ($_.Groups[6].Value) { $args_decl_with_defaults += " = " + $arg_default }
				$args_call += $arg_name
			}
		}

		if ($has_ellipsis) {
			$internal_name += "V" # Call the variant which accepts a "va_list"
			if ($internal_idx -gt 2) { $internal_name += ($internal_idx - 1) }

			$function_definitions += "`tinline " + $type + " " + $name + "(" + $args_decl + ", ...) { va_list args; va_start(args, " + $last_arg_name + "); "
			if ($has_return) { $function_definitions += "return " }
			$function_definitions += "imgui_function_table_instance()->" + $internal_name + "(" + $args_call + ", args); "
			$function_definitions += "va_end(args); }`r`n"
		}
		else {
			if ($internal_idx -gt 1) { $internal_name += $internal_idx }

			$function_table += "`t" + $type + "(*" + $internal_name + ")(" + $args_decl + ");`r`n"
			$function_table_init += "`tImGui::" + $name + ",`r`n"

			$function_definitions += "`tinline " + $type + " " + $name + "(" + $args_decl + ") { ";
			if ($has_return) { $function_definitions += "return " }
			$function_definitions += "imgui_function_table_instance()->" + $internal_name + "(" + $args_call + "); "
			$function_definitions += "}`r`n"
		}
		return
	}

	if ($is_inside_struct.Length -eq 0 -and $_ -match "^struct\s(Im\w+)" -and -not $_.Contains(";")) {
		$is_inside_struct = $matches[1]
		return
	}
	if ($is_inside_struct.Length -ne 0 -and ($_.StartsWith("};") -or $_.Contains("Don't use"))) {
		$is_inside_struct = ""
		return
	}

	if ($is_inside_struct.Length -ne 0 -and ($is_inside_struct -eq "ImFont" -or $is_inside_struct -eq "ImDrawList" -or $is_inside_struct -eq "ImGuiStorage" -or $is_inside_struct -eq "ImGuiListClipper") -and $_ -match 'IMGUI_API\s(?:([\s\w\&\*]+)(\s+|\*))?([\w~]+)\((.*)\)\s*(const)?;(?:(?:\s*\/\/)|$)') {
		$type = $(if ($matches[1]) { $matches[1].TrimEnd() + $matches[2].TrimEnd() })
		$name = $matches[3]
		$args = $matches[4]
		$is_const = $matches[5]
		if ($is_const) {
			$is_const += " "
		}

		if ($name.StartsWith("_")) {
			return
		}

		$internal_idx = 1
		$internal_name = $is_inside_struct + "_" + $name
		if ($function_list.ContainsKey($internal_name)) {
			$internal_idx = $function_list.Get_Item($internal_name) + 1
			$function_list.Set_Item($internal_name, $internal_idx)
			$internal_name += $internal_idx
		}
		else {
			$function_list.Add($internal_name, $internal_idx)
		}

		$has_return = $type -and $type -ne "void"
		if (-not $type) {
			if ($name.StartsWith("~")) {
				$internal_name = "Destruct" + $name.Substring(1)
			}
			else {
				$internal_name = "Construct" + $name
			}
		}

		$args_decl = ""
		$args_decl_with_defaults = ""
		$args_call = ""
		$last_arg_name = ""

		$args | Select-String -Pattern '((?:const\s)?(?:unsigned\s)?\w+\*{0,2}&?(?:\sconst)?)(?:(?:\s?\(\*(\w+)\)\(([^()]*)\))|\s(\w+)(\[\d*\])?)(?:\s=\s((?:[^,]+\([\+\-0-9\.,\s]*\))|(?:[\+\-0-9\._a-zA-Z]+)|(?:".*")))?' -AllMatches | ForEach-Object { $_.Matches } | ForEach-Object {
			if ($args_decl) {
				$args_decl += ", "
				$args_decl_with_defaults += ", "
				$args_call += ", "
			}

			$arg_type = $_.Groups[1].Value

			if ($_.Groups[2].Value) {
				$arg_name = $last_arg_name = $_.Groups[2].Value
				$arg_args = $_.Groups[3].Value

				$args_decl += $arg_type + "(*" + $arg_name + ")(" + $arg_args + ")"
				$args_decl_with_defaults += $arg_type + "(*" + $arg_name + ")(" + $arg_args + ")"
				$args_call += $arg_name
			}
			else {
				$arg_name = $last_arg_name = $_.Groups[4].Value
				$arg_default = $_.Groups[6].Value

				$args_decl += $arg_type + " " + $arg_name + $_.Groups[5].Value
				$args_decl_with_defaults += $arg_type + " " + $arg_name + $_.Groups[5].Value
				if ($_.Groups[6].Value) { $args_decl_with_defaults += " = " + $arg_default }
				$args_call += $arg_name
			}
		}

		if (-not $type) {
			$function_table += "`tvoid(*" + $internal_name + ")(" + $is_inside_struct + " *_this);`r`n"
			$function_table_init += "`t[](" + $is_inside_struct + " *_this) -> void { " + $(if ($name.StartsWith("~")) { "_this->" } else { "new(_this) " }) + $name + "(); },`r`n"

			$function_definitions_struct += "inline " + $is_inside_struct + "::" + $name + "(" + $args_decl + ") { "
			$function_definitions_struct += "imgui_function_table_instance()->" + $internal_name + "(this); "
			$function_definitions_struct += "}`r`n"
		}
		else {
			$function_table += "`t" + $type + "(*" + $internal_name + ")(" + $is_const + $is_inside_struct + " *_this" + $(if ($args_decl) { ", " }) + $args_decl + ");`r`n"
			$function_table_init += "`t[](" + $is_const + $is_inside_struct + " *_this" + $(if ($args_decl) { ", " }) + $args_decl + ") -> " + $type + " { "
			if ($has_return) { $function_table_init += "return " }
			$function_table_init += "_this->" + $name + "(" + $args_call + "); },`r`n"

			$function_definitions_struct += "inline " + $type + " " + $is_inside_struct + "::" + $name + "(" + $args_decl + ") " + $is_const + "{ ";
			if ($has_return) { $function_definitions_struct += "return " }
			$function_definitions_struct += "imgui_function_table_instance()->" + $internal_name + "(this" + $(if ($args_call) { ", " }) + $args_call + "); "
			$function_definitions_struct += "}`r`n"
		}
		return
	}
}


$year = Get-Date -Format "yyyy"

$function_table = @"
struct imgui_function_table_$version
{
$function_table
};
"@

$function_table_init = @"
/*
 * Copyright (C) 2021 Patrick Mours
 * Copyright (C) 2014-$year Omar Cornut
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON && RESHADE_GUI

#include <new>
#include "imgui_function_table_$version.hpp"

const imgui_function_table_$version init_imgui_function_table_$version() { return {
$function_table_init
}; }

#endif
"@

#$function_table = "`t" + (($function_table -split "`r`n") -join "`r`n`t")

$function_definitions = @"
/*
 * Copyright (C) 2021 Patrick Mours
 * Copyright (C) 2014-$year Omar Cornut
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if defined(IMGUI_VERSION_NUM)

#if IMGUI_VERSION_NUM != $version
#error Unexpected ImGui version, please update the "imgui.h" header to version $version!
#endif

// Check that the 'ImTextureID' type has the same size as 'reshade::api::resource_view'
static_assert(sizeof(ImTextureID) == 8, "missing \"#define ImTextureID ImU64\" before \"#include <imgui.h>\"");

$function_table

using imgui_function_table = imgui_function_table_$version;

inline const imgui_function_table *&imgui_function_table_instance()
{
	static const imgui_function_table *instance = nullptr;
	return instance;
}

#ifndef RESHADE_API_LIBRARY_EXPORT

namespace ImGui
{
$function_definitions
}

$function_definitions_struct

#endif

#endif
"@

$function_table = @"
/*
 * Copyright (C) 2021 Patrick Mours
 * Copyright (C) 2014-$year Omar Cornut
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <imgui.h>

$function_table

extern const imgui_function_table_$version g_imgui_function_table_$version;
"@

$function_table | Out-File -FilePath "..\source\imgui_function_table_$version.hpp" -Encoding ASCII
$function_table_init | Out-File -FilePath "..\source\imgui_function_table_$version.cpp" -Encoding ASCII
$function_definitions | Out-File -FilePath "..\include\reshade_overlay.hpp" -Encoding ASCII
