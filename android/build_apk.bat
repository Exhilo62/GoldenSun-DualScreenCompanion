@echo off
rem Builds the debug APK: android\app\build\outputs\apk\debug\app-debug.apk
rem Needs a JDK 17+, the Android SDK (platform 34, build-tools 34.0.0, NDK 26.3.11579264) and the SDL2 source tree; override the locations below with environment variables.
if "%JAVA_HOME%"=="" set JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot
if "%ANDROID_HOME%"=="" set ANDROID_HOME=D:\android-sdk\sdk
if "%SDL2_DIR%"=="" set SDL2_DIR=D:/android-sdk/SDL2-2.30.9
set PATH=%JAVA_HOME%\bin;%PATH%
cd /d %~dp0
if not exist gradlew.bat (
  if exist D:\android-sdk\gradle-8.7\bin\gradle.bat call D:\android-sdk\gradle-8.7\bin\gradle.bat wrapper --gradle-version 8.7 || exit /b 1
)
call "%~dp0gradlew.bat" --no-daemon %* assembleDebug
