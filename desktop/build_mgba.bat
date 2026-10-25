@echo off
rem Builds the mGBA core library (static, Release) with Visual C++ for the desktop spike.
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set PATH=C:\Program Files\CMake\bin;%PATH%;C:\devkitPro\msys2\usr\bin
cmake -S D:\gs3ds\third_party\mgba -B D:\gs3ds\desktop\build\mgba -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC=ON -DBUILD_SHARED=OFF -DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_GL=OFF -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DBUILD_LIBRETRO=OFF -DBUILD_HEADLESS=OFF -DBUILD_EXAMPLE=OFF -DBUILD_TEST=OFF -DBUILD_SUITE=OFF -DM_CORE_GB=OFF -DM_CORE_GBA=ON -DENABLE_SCRIPTING=OFF -DENABLE_GDB_STUB=OFF -DENABLE_DEBUGGERS=ON -DUSE_FFMPEG=OFF -DUSE_ZLIB=OFF -DUSE_PNG=OFF -DUSE_LIBZIP=OFF -DUSE_MINIZIP=OFF -DUSE_LZMA=OFF -DUSE_SQLITE3=OFF -DUSE_ELF=OFF -DUSE_EPOXY=OFF -DUSE_EDITLINE=OFF -DUSE_JSON_C=OFF -DUSE_LUA=OFF -DUSE_DISCORD_RPC=OFF -DUSE_FREETYPE=OFF -DBUILD_LTO=OFF -DSKIP_LIBRARY=OFF -DLIBMGBA_ONLY=ON "-DCMAKE_C_FLAGS=/DCOLOR_16_BIT /DCOLOR_5_6_5" || exit /b 2
cmake --build D:\gs3ds\desktop\build\mgba --config Release || exit /b 3
