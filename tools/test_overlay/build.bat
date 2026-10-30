@echo off
rem Compila la prova dell'overlay (Win32) con lo stesso Overlay.cpp della mod. Uso: build.bat <cartella di uscita>
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cl /nologo /std:c++17 /EHsc /O2 /MT /W3 /DUNICODE /D_UNICODE /Fe"%~1\test_overlay.exe" /Fo"%~1\\" "%~dp0test_overlay.cpp" "%~dp0..\..\ImperiumPad\Overlay.cpp" "%~dp0..\..\ImperiumPad\Lang.cpp" "%~dp0..\..\ImperiumPad\Glyphs.cpp" "%~dp0..\..\ImperiumPad\Radial.cpp" "%~dp0..\..\ImperiumPad\Cursor.cpp" user32.lib gdi32.lib
