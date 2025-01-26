if ((Get-Command "xgettext.exe" -ErrorAction SilentlyContinue) -eq $null) 
{ 
   Write-Host "Unable to find xgettext.exe. Please download https://gnuwin32.sourceforge.net/packages/gettext.htm."
   return
}

.\create_language_template.ps1 bg-BG
.\create_language_template.ps1 de-DE
.\create_language_template.ps1 en-US
.\create_language_template.ps1 es-ES
.\create_language_template.ps1 fr-FR
.\create_language_template.ps1 ja-JP
.\create_language_template.ps1 ko-KR
.\create_language_template.ps1 ru-RU
.\create_language_template.ps1 zh-CN
