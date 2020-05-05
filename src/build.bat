@echo off
if not exist "build" mkdir build
cd build

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64 > NUL

REM  ------------- WARNINGS -------------
REM Enable

REM Disable
REM /wd4100 unreferenced formal parameter
REM /wd4189 local variable is initalized but not referenced
REM /wd4201 nonstandard extension used: nameless struct/union
set Warnings=/wd4100 /wd4189 /wd4201
REM  ------------------------------------

REM -Zi Generate complete debug information
REM -FC Display full path of source code passsed to cl.exe in diagnostics text
set CFlags=-nologo -Od -Zi -FC -W4 %Warnings%

set LinkerFlags=-incremental:no User32.lib Gdi32.lib

REM if exist *.dll del *.dll > NUL 2> NUL
REM if exist *.pdb del *.pdb > NUL 2> NUL
REM if exist *.ilk del *.ilk > NUL 2> NUL

set IncludeDirs=

cl %CFlags% %IncludeDirs% ../win32_kalam.c /link %LinkerFlags% 