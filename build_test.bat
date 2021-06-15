@REM Build for Visual Studio compiler. Run your copy of amd64/vcvars32.bat to setup 64-bit command-line compiler.

@set INCLUDES=/I includes
@set SOURCES=test.cpp includes/seriallib.cpp
@set LIBS=/LIBPATH:.  opengl32.lib gdi32.lib shell32.lib

@set OUT_DIR=Debug_test
@set OUT_EXE=serial_test
if not exist %OUT_DIR% mkdir %OUT_DIR%
cl /nologo /Zi /MD /Ox /Oi /EHsc /std:c++17 %INCLUDES% %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS%
%~dp0/%OUT_DIR%/%OUT_EXE%.exe