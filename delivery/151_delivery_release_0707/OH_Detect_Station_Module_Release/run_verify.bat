@echo off
setlocal

set "ROOT=%~dp0"
set "PATH=%ROOT%;%PATH%"

"%ROOT%verify_dll_smoke.exe" ^
  --module "%ROOT%Hd_AI_Station_Jr_module.dll" ^
  --model "%ROOT%model\best.onnx" ^
  --input-dir "%ROOT%input" ^
  --output-dir "%ROOT%output"

if errorlevel 1 (
  echo VerifyResult=FAIL
  exit /b 1
)

echo VerifyResult=PASS
exit /b 0