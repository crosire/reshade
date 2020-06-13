$version = 0,0,0,0

# Get version from existing file
if (Test-Path $args[0]) {
	Get-Content $args[0] |
	Where { $_ -match "VERSION_FULL (\d+).(\d+).(\d+).(\d+)" } |
	ForEach { $version = [int]::Parse($matches[1]), [int]::Parse($matches[2]), [int]::Parse($matches[3]), [int]::Parse($matches[4]) }
}

# Increment build version for Release builds
if ($args[1] -eq "Release") {
	$version[3] += 1
	"Updating version to $([string]::Join('.', $version)) ..."
}
else {
	return
}

# Update version file with the new version information
@"
#pragma once

#define VERSION_DATE "$([DateTimeOffset]::Now.ToString('yyyy-MM-dd'))"
#define VERSION_TIME "$([DateTimeOffset]::Now.ToString('HH:mm:ss'))"

#define VERSION_FULL $([string]::Join('.', $version))
#define VERSION_MAJOR $($version[0])
#define VERSION_MINOR $($version[1])
#define VERSION_REVISION $($version[2])
#define VERSION_BUILD $($version[3])

#define VERSION_STRING_FILE "$([string]::Join('.', $version))"
#define VERSION_STRING_PRODUCT "$($version[0]).$($version[1]).$($version[2])"
"@ | Out-File -FilePath $args[0]
