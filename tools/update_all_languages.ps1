if ((Get-Command "xgettext.exe" -ErrorAction SilentlyContinue) -eq $null) 
{ 
	Write-Host "Unable to find xgettext.exe. Please download https://gnuwin32.sourceforge.net/packages/gettext.htm."
	return
}

$languages = @(
	"bg-BG",
	"de-DE",
	"en-US",
	"es-ES",
	"fr-FR",
	"ja-JP",
	"ko-KR",
	"pt-BR",
	"ru-RU",
	"sl-SI",
	"th-TH",
	"tr-TR",
	"zh-CN",
	"zh-HK"
)

foreach ($language in $languages) {
	.\create_language_template.ps1 -locale $language
}
