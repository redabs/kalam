@echo off
if not exist "build" mkdir build
cd build

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64 > NUL

REM  ------------- WARNINGS -------------
REM Enable

REM Disable
REM /wd4100 unreferenced formal parameter

set Warnings=/wd4100
REM  ------------------------------------

REM -Zi Generate complete debug information
REM -FC Display full path of source code passsed to cl.exe in diagnostics text
set CFlags=-nologo -Od -Zi -FC -W4 %Warnings%

set LinkerFlags=-incremental:no User32.lib Gdi32.lib

REM if exist *.dll del *.dll > NUL 2> NUL
REM if exist *.pdb del *.pdb > NUL 2> NUL
REM if exist *.ilk del *.ilk > NUL 2> NUL

set IncludeDirs=

cl %CFlags% %IncludeDirs% ../win32_kalam.cpp /link %LinkerFlags% 