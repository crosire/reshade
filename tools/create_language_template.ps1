Param(
	[string]
	$locale = "en-US"
)

# This hash function has to match the one used in localization.hpp
function compute_crc16 {
	Param($data)

	$size = $data.Length
	$i = 0
	$crc = 0;
	while ($size--)
	{
		$crc = $crc -bxor $data[$i++]
		for ($k = 0; $k -lt 8; $k++) {
			$crc = if ($crc -band 1) { ($crc -shr 1) -bxor 0xa001 } else { $crc -shr 1 }
		}
	}
	return $crc
}

$strings = ""

if (Get-Command "xgettext.exe" -ErrorAction SilentlyContinue)
{
	$message = ""
	$is_inside_message = $false

	xgettext.exe --c++ --keyword=_ --omit-header --indent --no-wrap --output=- ..\source\runtime_gui.cpp ..\source\runtime_gui_vr.cpp ..\source\imgui_widgets.cpp | ForEach-Object {
		if ($_.StartsWith("#") -or $_.Length -eq 0) {
			return # Ignore comments
		}

		if ($_.StartsWith("msgid")) {
			$is_inside_message = $true
		}
		if ($_.StartsWith("msgstr")) {
			$hash = compute_crc16([System.Text.Encoding]::UTF8.GetBytes($message.Trim('"').Replace("\`"", "`"").Replace("\\", "\").Replace("\n", "`n")))

			$message = $message.Replace("\`"", "`"`"")
			$strings += "$hash $message`r`n"

			$message = ""
			$is_inside_message = $false
			return
		}

		if ($is_inside_message) {
			if ($message.Length -ne 0) {
				$message = $message.Remove($message.Length - 1) + $_.Substring($_.IndexOf('"') + 1)
			} else {
				$message += $_.Substring($_.IndexOf('"'))
			}
		}
	}
}

$ci = New-Object -TypeName System.Globalization.CultureInfo -ArgumentList $locale
$locale = $ci.Name
$locale_name = $ci.EnglishName
$locale_short = $ci.ThreeLetterWindowsLanguageName
$locale_lang_id = $ci.LCID -band 0x3FF
$locale_sublang_id = ($ci.LCID -band 0xFC00) -shr 10

@"
/////////////////////////////////////////////////////////////////////////////
// $locale_name resources

#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_$locale_short)
LANGUAGE $locale_lang_id, $locale_sublang_id

/////////////////////////////////////////////////////////////////////////////
//
// String Table
//

STRINGTABLE
BEGIN

$strings
END

#endif    // $locale_name resources
/////////////////////////////////////////////////////////////////////////////
"@ | Out-File -FilePath "..\res\lang_$locale.rc2" -Encoding ASCII
