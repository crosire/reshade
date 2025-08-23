@echo off
echo ReShade License File Generator
echo =============================
echo.
echo This will create a license file with authorized HWIDs.
echo File location: %%APPDATA%%\ReShade\license.txt
echo.

if not exist "%APPDATA%\ReShade" mkdir "%APPDATA%\ReShade"

echo # ReShade License File > "%APPDATA%\ReShade\license.txt"
echo # Add authorized Hardware IDs below (one per line) >> "%APPDATA%\ReShade\license.txt"
echo # Lines starting with # are comments >> "%APPDATA%\ReShade\license.txt"
echo. >> "%APPDATA%\ReShade\license.txt"
echo # Example HWIDs: >> "%APPDATA%\ReShade\license.txt"
echo 12345678_abcdef00 >> "%APPDATA%\ReShade\license.txt"
echo 87654321_00fedcba >> "%APPDATA%\ReShade\license.txt"
echo. >> "%APPDATA%\ReShade\license.txt"
echo # Add customer HWIDs below: >> "%APPDATA%\ReShade\license.txt"

echo License file created at: %APPDATA%\ReShade\license.txt
echo.
echo To add a customer:
echo 1. Get their HWID from ReShade log
echo 2. Add the HWID to this file
echo 3. Send them the updated ReShade DLL
echo.
echo Opening the license file...
notepad "%APPDATA%\ReShade\license.txt"
pause