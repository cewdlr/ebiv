@echo off
rem Commands to compile the pyEBIV interface using only (free) the Visual Studio commandline tools (VS2022)
rem SWIG needs to be available (see http://www.swig.org/)
rem 
rem NOTE: BAT files may be antiquated but it's easy to follow the compilation steps and has
rem       little overhead. 
rem       Feel free to port this to CMAKE or whatever...
rem
rem First, some necessary paths for the current Python installation...
rem Modify variable PYTHON_DIR as necessary, other should be fine
rem This one is for Python v3.9 or v3.10 on Windows using the WinPython distribution!
SET arg=v%1
IF "%arg%" == "v" goto :USAGE

rem Set version and destination directory
set DESTDIR=..\python_%arg%

goto :%arg%

:v3.9
set PYTHON_DIR=C:\Python\WPy64-39100\python-3.9.10.amd64
set PYTHON_LIB=%PYTHON_DIR%\libs\python39.lib
goto :BUILD

:v3.10
set PYTHON_DIR=C:\Python\WPy64-310111\python-3.10.11.amd64
set PYTHON_LIB=%PYTHON_DIR%\libs\python310.lib
goto :BUILD

:v3.11
set PYTHON_DIR=C:\Python\WPy64-31180\python-3.11.8.amd64
set PYTHON_LIB=%PYTHON_DIR%\libs\python311.lib
goto :BUILD

:BUILD
set PYTHON_INCLUDE=%PYTHON_DIR%\include
set NUMPY_INCL=%PYTHON_DIR%\Lib\site-packages\numpy\core\include
rem place to put the final pyEBIV interface/DLL

echo In order to function correctly, please ensure the PYTHON environment variables are correctly.
echo Current configuration is as follows:
echo    PYTHON_DIR: %PYTHON_DIR% 
echo    PYTHON_INCLUDE: %PYTHON_INCLUDE% 
echo    PYTHON_LIB: %PYTHON_LIB% 
echo    NUMPY_INCL: %NUMPY_INCL% 

if not exist %PYTHON_DIR% (
	echo PYTHON_DIR: %PYTHON_DIR% not found
	goto :END
)
if not exist %NUMPY_INCL% (
	echo NUMPY_INCL: %NUMPY_INCL% not found
	goto :END
)

set LIBSRC=..\src
rem create C++ wrapper file using SWIG
swig.exe -c++ -python -o pyebiv_wrap.cpp pyebiv.i

rem Compiler options
set CXX=cl
set CXXFLAGS=-nologo -Zc:wchar_t -FS -Zc:rvalueCast -Zc:inline -Zc:strictStrings -Zc:throwingNew -Zc:referenceBinding -Zc:__cplusplus -O2 -MD -W3 -w34100 -w34189 -w44996 -w44456 -w44457 -w44458 -wd4577 -wd4467 -EHsc 
set INCPATH= -I. -I..\include -I%PYTHON_INCLUDE% -I%NUMPY_INCL%
set DEFINES=-DNDEBUG -DPYIMX_EXPORTS -D_WINDOWS -D_USRDLL
set LINKER=link
set LFLAGS= /NOLOGO /DYNAMICBASE /NXCOMPAT /NODEFAULTLIB:libcmt.lib /INCREMENTAL:NO /DLL /SUBSYSTEM:CONSOLE
set LIBS=/LIBPATH:.\lib user32.lib %PYTHON_LIB%
set OUTDIR=.\x64\obj
set OUTDLL=.\x64\pyebiv.dll


rem Place to put intermediate object/build files
if not exist %OUTDIR% md .\x64\obj

rem Compile C++ files
FOR %%F IN (pyebiv_wrap pyebiv) do (
   %CXX% -c %CXXFLAGS% %DEFINES% %INCPATH% -Fo%OUTDIR%\%%F.obj %%F.cpp
)
FOR %%F IN (ebi_events ebi_image ebi_utils) do (
   %CXX% -c %CXXFLAGS% %DEFINES% %INCPATH% -Fo%OUTDIR%\%%F.obj %LIBSRC%\%%F.cpp
)

rem call Linker
set OBJECTS=.\x64\obj\pyebiv.obj .\x64\obj\pyebiv_wrap.obj .\x64\obj\ebi_events.obj .\x64\obj\ebi_image.obj .\x64\obj\ebi_utils.obj
%LINKER% %LFLAGS% /MANIFEST:embed /OUT:%OUTDLL% %OBJECTS% %LIBS%
 
rem convert/copy to python lib
copy /y %OUTDLL% _pyebiv.pyd

echo Final pyEBIV module files are: "pyebiv.py" and "_pyebiv.pyd" located in %DESTDIR%
echo Copy them to your own python project/environment
if not exist %DESTDIR% md %DESTDIR%
FOR %%F in (pyebiv.py _pyebiv.pyd) DO copy /y %%F %DESTDIR%\.
goto :END

:USAGE
echo Usage: %0 [python_version]
echo    where [python_version] is one of the following
echo    '3.9'      for Python v.3.9 
echo    '3.10'     for Python v.3.10 
echo    '3.11'     for Python v.3.11 

:END
