$function_list = @{}

$function_table = ""
$function_table_init = ""
$function_definitions = ""

$is_inside_namespace = 0

Get-Content ..\deps\imgui\imgui.h | ForEach-Object {
	if ($is_inside_namespace -eq 0 -and $_.Contains("namespace ImGui")) {
		$is_inside_namespace = 1
	}
	if ($is_inside_namespace -eq 1 -and $_.Contains("} // namespace ImGui")) {
		$is_inside_namespace = 2
	}

	if ($is_inside_namespace -eq 1 -and $_ -match 'IMGUI_API\s([\s\w\&\*]+)\s+(\w+)\((.*)\);(?:(?:\s*\/\/)|$)') {
		$type = $matches[1].TrimEnd()
		$name = $matches[2]
		$args = $matches[3]

		# Filter out various functions
		if ($name.StartsWith("Log") -or $name.StartsWith("Show") -or $name.StartsWith("StyleColors") -or $name.EndsWith("Context") -or $name.Contains("IniSettings") -or $name.Contains("Viewport") -or $name.Contains("Platform") -or $name -eq "NewFrame" -or $name -eq "EndFrame" -or $name -eq "Render" -or $name -eq "GetDrawData") {
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

			if ($_.Groups[2].Value) {
				$arg_type = $_.Groups[1].Value
				$arg_name = $last_arg_name = $_.Groups[2].Value
				$arg_args = $_.Groups[3].Value

				$args_decl += $arg_type + "(*" + $arg_name + ")(" + $arg_args + ")"
				$args_decl_with_defaults += $arg_type + "(*" + $arg_name + ")(" + $arg_args + ")"
				$args_call += $arg_name
			}
			else {
				$arg_type = $_.Groups[1].Value
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
	}
}


$function_table = @"
struct imgui_function_table
{
$function_table
};
"@

$function_table_init = @"
#include <imgui.h>
#include "reshade_overlay.hpp"

imgui_function_table g_imgui_function_table = {
$function_table_init
};
"@

#$function_table = "`t" + (($function_table -split "`r`n") -join "`r`n`t")

$function_definitions = @"
#pragma once

#if defined(IMGUI_VERSION_NUM)

#ifndef ImTextureID
#define ImTextureID reshade::api::resource_view
#endif

$function_table

inline const imgui_function_table *&imgui_function_table_instance()
{
	static const imgui_function_table *instance = nullptr;
	return instance;
}

#ifndef RESHADE_ADDON

namespace ImGui
{
$function_definitions
}

#endif

#endif
"@

$function_table_init | Out-File -FilePath "..\source\imgui_function_table.cpp" -Encoding ASCII
$function_definitions | Out-File -FilePath "..\include\reshade_overlay.hpp" -Encoding ASCII
