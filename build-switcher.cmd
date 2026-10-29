@echo off
setlocal
set "AQS_VERSION=0.7.0"
set "AQS_DLL_NAME=ArrowQuickSwitch.dll"
set "AQS_ICON_OBJECTS=icon_archive.obj item_icons.obj miniz_tinfl.obj"
set "AQS_ICON_LIBS=d3d11.lib bcrypt.lib"
if /i "%~1"=="--test" set "AQS_DLL_NAME=ArrowQuickSwitch-testing.dll"
if /i "%~1"=="--test-no-icons" set "AQS_DLL_NAME=ArrowQuickSwitch-no-icons-testing.dll"
cd /d "%~dp0"
if not exist build mkdir build
if not exist dist mkdir dist
if not exist tmp mkdir tmp
set "TEMP=%CD%\tmp"
set "TMP=%CD%\tmp"
call "%~dp0tools\find_build_tools.cmd"
if errorlevel 1 exit /b 1
cd build
python ..\tools\generate_version_resource.py --filename "%AQS_DLL_NAME%"
if errorlevel 1 exit /b 1
rc /nologo /fo version.res version.rc
if errorlevel 1 exit /b 1
python ..\tools\generate_default_config.py --header default_config.hpp
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT /I. ..\tests\config_file_tests.cpp /Fe:config_file_tests.exe /link user32.lib
if errorlevel 1 exit /b 1
config_file_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT /I. ..\tests\theme_tests.cpp /Fe:theme_tests.exe /link user32.lib
if errorlevel 1 exit /b 1
theme_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\ui_input_text_tests.cpp /Fe:ui_input_text_tests.exe /link user32.lib gdi32.lib
if errorlevel 1 exit /b 1
ui_input_text_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\cycle_tests.cpp /Fe:cycle_tests.exe
if errorlevel 1 exit /b 1
cycle_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\binding_tests.cpp /Fe:binding_tests.exe
if errorlevel 1 exit /b 1
binding_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\binding_capture_tests.cpp /Fe:binding_capture_tests.exe /link user32.lib
if errorlevel 1 exit /b 1
binding_capture_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\window_input_tests.cpp /Fe:window_input_tests.exe /link user32.lib
if errorlevel 1 exit /b 1
window_input_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\controller_tests.cpp /Fe:controller_tests.exe
if errorlevel 1 exit /b 1
controller_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\wheel_tests.cpp /Fe:wheel_tests.exe
if errorlevel 1 exit /b 1
wheel_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\ammo_effects_tests.cpp /Fe:ammo_effects_tests.exe
if errorlevel 1 exit /b 1
ammo_effects_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\switching_tests.cpp /Fe:switching_tests.exe /link user32.lib
if errorlevel 1 exit /b 1
switching_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\name_tests.cpp /Fe:name_tests.exe /link user32.lib
if errorlevel 1 exit /b 1
name_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\icon_formats_tests.cpp /Fe:icon_formats_tests.exe
if errorlevel 1 exit /b 1
icon_formats_tests.exe
if errorlevel 1 exit /b 1
if /i "%~1"=="--test-no-icons" goto build_without_item_icons
cl /nologo /guard:cf /GS /MT /O2 /c ..\third_party\miniz\miniz_tinfl.c
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT /O2 /c ..\src\icon_archive.cpp ..\src\item_icons.cpp
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT /O2 ..\tests\icon_archive_tests.cpp icon_archive.obj miniz_tinfl.obj /Fe:icon_archive_tests.exe /link bcrypt.lib
if errorlevel 1 exit /b 1
icon_archive_tests.exe
if errorlevel 1 exit /b 1
goto item_icons_ready
:build_without_item_icons
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT /O2 /c ..\src\item_icons_disabled.cpp
if errorlevel 1 exit /b 1
set "AQS_ICON_OBJECTS=item_icons_disabled.obj"
set "AQS_ICON_LIBS="
:item_icons_ready
cl /nologo /guard:cf /GS /MT /O2 /I..\third_party\imgui /c ..\third_party\imgui\imgui.cpp ..\third_party\imgui\imgui_draw.cpp ..\third_party\imgui\imgui_tables.cpp ..\third_party\imgui\imgui_widgets.cpp ..\third_party\imgui\backends\imgui_impl_win32.cpp ..\third_party\imgui\backends\imgui_impl_dx12.cpp
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT /O2 /I..\third_party\imgui /c ..\src\settings_ui.cpp
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /MT /O2 /c ..\third_party\minhook\src\buffer.c ..\third_party\minhook\src\hook.c ..\third_party\minhook\src\trampoline.c ..\third_party\minhook\src\hde\hde64.c
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT /O2 ..\tests\hook_compatibility_tests.cpp buffer.obj hook.obj trampoline.obj hde64.obj /Fe:hook_compatibility_tests.exe /link /GUARD:CF /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
hook_compatibility_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT /O2 /I..\third_party\minhook\include /c ..\src\indicator.cpp
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT /O2 /LD /I. /I..\third_party\minhook\include ..\src\switcher.cpp indicator.obj settings_ui.obj imgui.obj imgui_draw.obj imgui_tables.obj imgui_widgets.obj imgui_impl_win32.obj imgui_impl_dx12.obj %AQS_ICON_OBJECTS% buffer.obj hook.obj trampoline.obj hde64.obj version.res /Fe:..\dist\%AQS_DLL_NAME% /link /GUARD:CF /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT dwmapi.lib %AQS_ICON_LIBS% user32.lib gdi32.lib ole32.lib windowscodecs.lib d3d12.lib dxgi.lib d3dcompiler.lib version.lib /IMPLIB:ArrowQuickSwitch.lib
if errorlevel 1 exit /b 1
python ..\tools\verify_binary.py "..\dist\%AQS_DLL_NAME%"
if errorlevel 1 exit /b 1
cl /nologo /guard:cf /GS /std:c++20 /EHsc /W4 /WX /MT ..\tests\indicator_preview.cpp indicator.obj settings_ui.obj imgui.obj imgui_draw.obj imgui_tables.obj imgui_widgets.obj imgui_impl_win32.obj imgui_impl_dx12.obj %AQS_ICON_OBJECTS% buffer.obj hook.obj trampoline.obj hde64.obj /Fe:indicator_preview.exe /link dwmapi.lib %AQS_ICON_LIBS% user32.lib gdi32.lib ole32.lib windowscodecs.lib d3d12.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 exit /b 1
indicator_preview.exe "%CD%\indicator-preview.png"
if errorlevel 1 exit /b 1
copy /y ..\third_party\minhook\LICENSE.txt ..\dist\MinHook-LICENSE.txt >nul
if errorlevel 1 exit /b 1
if /i "%~1"=="--test" exit /b 0
echo Build complete: dist\%AQS_DLL_NAME%
exit /b 0
