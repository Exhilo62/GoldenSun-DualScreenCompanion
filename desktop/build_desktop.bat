@echo off
set ROOT=%~dp0..
rem Builds the desktop spike (gs_desktop.exe) with Visual C++ against SDL2 and the mGBA library from build_mgba.bat.
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set SDL=%ROOT%\desktop\deps\sdl2\SDL2-2.30.9
set OUT=%ROOT%\desktop\build\app
if not exist %OUT% mkdir %OUT%
cl /nologo /O2 /std:c11 /experimental:c11atomics /MD /D_CRT_SECURE_NO_WARNINGS /DBUILD_STATIC /DM_CORE_GBA /DENABLE_DEBUGGERS /DENABLE_VFS /DENABLE_VFS_FD /DENABLE_DIRECTORIES /DHAVE_SETLOCALE /DHAVE_STRDUP /DNOMINMAX /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DWIN32 /FI%ROOT%\desktop\msvc_compat.h /wd4244 /wd4267 /wd4018 /wd4146 /wd4090 /wd4996 ^
 /I%ROOT%\third_party\mgba\include /I%ROOT%\desktop\build\mgba\include /I%SDL%\include ^
 /Fo%OUT%\ /Fe%OUT%\gs_desktop.exe ^
 %ROOT%\desktop\main_sdl.c %ROOT%\src\gs_state.c %ROOT%\src\ui.c %ROOT%\src\gfx.c %ROOT%\src\font_data.c %ROOT%\src\art.c %ROOT%\src\art_data.c %ROOT%\src\logo_data.c %ROOT%\src\icon_data.c %ROOT%\src\mapicon_data.c ^
 %ROOT%\src\mapview.c %ROOT%\src\menunav.c %ROOT%\src\gs_areas.c %ROOT%\src\gs_items.c %ROOT%\src\gs_abilities.c %ROOT%\src\bestiary.c %ROOT%\src\gs_djinn.c %ROOT%\src\dialoglog.c %ROOT%\src\gs_field.c %ROOT%\src\gs_classes.c %ROOT%\src\gs_itemdesc.c %ROOT%\src\mapdecode.c ^
 /link /LIBPATH:%SDL%\lib\x64 /LIBPATH:%ROOT%\desktop\build\mgba %ROOT%\desktop\build\mgba\mgba.lib SDL2.lib SDL2main.lib shell32.lib ole32.lib ws2_32.lib winmm.lib shlwapi.lib /SUBSYSTEM:CONSOLE || exit /b 2
copy /Y %SDL%\lib\x64\SDL2.dll %OUT%\ >nul
