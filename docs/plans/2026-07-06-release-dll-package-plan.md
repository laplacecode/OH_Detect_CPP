# Release DLL Package Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a customer-runnable Release package containing `Hd_AI_Station_Jr_module.dll`, a dynamic-load smoke verifier, `model/best.onnx`, sample input, runtime DLLs, and a one-click verification script.

**Architecture:** Keep `delivery/package/` as the formal customer API/model directory. Build the module DLL from `delivery/reference_qt_adapter/`, build a separate verifier that loads the DLL through `create/destory`, then package runtime files under `delivery/release/OH_Detect_Station_Module_Release/`.

**Tech Stack:** MinGW g++, Qt6, OpenCV, ONNX Runtime, PowerShell packaging scripts, Windows batch launcher.

---

### Task 1: Add Release Package Acceptance Check

**Files:**
- Create: `cpp_verify/scripts/test_release_package.ps1`

**Steps:**
1. Add a PowerShell smoke check that validates required package files exist.
2. Make it run `run_verify.bat` from the package directory.
3. Run it before implementation and confirm it fails because the package does not exist.

### Task 2: Build Module DLL

**Files:**
- Create: `cpp_verify/build_module_dll_mingw.ps1`
- Create: `cpp_verify/build_module_dll_mingw_qt6.cmd`

**Steps:**
1. Compile `delivery/reference_qt_adapter/*.cpp` as `Hd_AI_Station_Jr_module.dll`.
2. Export a MinGW import library named `Hd_AI_Station_Jr_module.lib`.
3. Copy `onnxruntime.dll` into `cpp_verify/out/` for local smoke execution.

### Task 3: Add Dynamic DLL Verifier

**Files:**
- Create: `cpp_verify/src/dll_smoke_main.cpp`
- Modify: `cpp_verify/build_mingw.ps1`

**Steps:**
1. Add a verifier that uses `LoadLibrary`, `create`, `destory`, and the documented module methods.
2. Save the same 5 output images and protocol files as the current direct-link verifier.
3. Extend the build script with a `DllSmoke` mode.

### Task 4: Package Customer Release

**Files:**
- Create: `cpp_verify/package_templates/run_verify.bat`
- Create: `cpp_verify/package_templates/README_客户验证说明.md`
- Create: `cpp_verify/package_release.ps1`

**Steps:**
1. Copy the module DLL, import library, verifier exe, headers, docs, model, and sample input into a release folder.
2. Copy runtime DLL dependencies from ONNX Runtime and the MinGW runtime bin directory.
3. Keep generated output under `delivery/release/OH_Detect_Station_Module_Release/`.

### Task 5: Verify End to End

**Commands:**
- `cpp_verify\build_module_dll_mingw_qt6.cmd`
- `cpp_verify\build_mingw_manual_qt6.cmd -Mode DllSmoke`
- `powershell -ExecutionPolicy Bypass -File cpp_verify\package_release.ps1`
- `powershell -ExecutionPolicy Bypass -File cpp_verify\scripts\test_release_package.ps1`

**Expected:** The final smoke check runs the packaged verifier and reports `ReleasePackageSmoke=PASS`.
