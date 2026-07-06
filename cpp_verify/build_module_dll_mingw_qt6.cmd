@echo off
setlocal

if "%ORT_ROOT%"=="" set "ORT_ROOT=C:\opt\onnxruntime-win-x64-1.25.0"

set "SCRIPT_DIR=%~dp0"
set "QT_INCLUDE_DIRS=C:\msys64\ucrt64\include\qt6;C:\msys64\ucrt64\include\qt6\QtCore;C:\msys64\ucrt64\include\qt6\QtGui;C:\msys64\ucrt64\share\qt6\mkspecs\win32-g++"
set "QT_LIB_DIR=C:\msys64\ucrt64\lib"
set "QT_LIBS=Qt6Core;Qt6Gui"
set "OPENCV_INCLUDE_DIRS=C:\msys64\ucrt64\include\opencv4"
set "OPENCV_LIB_DIR=C:\msys64\ucrt64\lib"
set "OPENCV_LIBS=opencv_core;opencv_imgproc;opencv_dnn;opencv_imgcodecs;opencv_highgui"

powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build_module_dll_mingw.ps1" ^
  %*

endlocal