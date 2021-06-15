@REM Build for Visual Studio compiler. Run your copy of amd64/vcvars32.bat to setup 64-bit command-line compiler.

@set INCLUDES=/I includes\imgui /I includes\implot /I includes\backends /I includes /I %VULKAN_SDK%\include
@set SOURCES=main.cpp includes\backends\imgui_impl_vulkan.cpp includes\backends\imgui_impl_glfw.cpp includes\imgui\imgui*.cpp includes\implot\implot*.cpp includes/seriallib.cpp
@set LIBS=/LIBPATH:libs /libpath:%VULKAN_SDK%\lib glfw3.lib opengl32.lib gdi32.lib shell32.lib vulkan-1.lib

@set OUT_DIR=Debug
@set OUT_EXE=rsoc_test
if not exist %OUT_DIR% mkdir %OUT_DIR%
cl /nologo /Zi /MD /Ox /Oi /EHsc /std:c++17 %INCLUDES% %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS%
%~dp0/%OUT_DIR%/%OUT_EXE%.exe