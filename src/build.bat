@echo off
if not exist "build" mkdir build
cd build

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64 > NUL

REM  ------------- WARNINGS -------------
REM Enable
REM /we4013 treat undefined symbol as error instead of assume extern returning int
REM /we4296 An unsigned variable was used in a comparison operation with zero.

REM Disable
REM /wd4100 unreferenced formal parameter
REM /wd4189 local variable is initalized but not referenced
REM /wd4201 nonstandard extension used: nameless struct/union
REM /wd4204 nonstandard extension used: non-constant aggregate initializer
REM /wd4200 nonstandard extension used: zero-sized array in struct/union

set Warnings=/we4013 /we4296 /wd4100 /wd4189 /wd4201 /wd4204 /wd4200
REM  ------------------------------------

REM -Zi Generate complete debug information
REM -FC Display full path of source code passsed to cl.exe in diagnostics text
set CFlags=-nologo -Od -Zi -FC -W4 %Warnings% /D UNICODE

set LinkerFlags=-incremental:no User32.lib Gdi32.lib Pathcch.lib

REM if exist *.dll del *.dll > NUL 2> NUL
REM if exist *.pdb del *.pdb > NUL 2> NUL
REM if exist *.ilk del *.ilk > NUL 2> NUL

set IncludeDirs=

set SourceFiles=../k_win32.cpp ../kalam.cpp ../stb_truetype.cpp

cl %CFlags% %IncludeDirs% %SourceFiles% /link %LinkerFlags% /OUT:kalam.exe

REM This is just to verify that the test case for the syntax highlighter and declaration parser is valid code
REM cl %CFlags% ../test/test.c /link /OUT:test_binary_do_not_run.exe
