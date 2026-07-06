# cpp_verify

用于仓库根目录下的 C++ 验证入口。该目录只服务于内部验证，不属于正式交付包。

## 保留文件

```text
cpp_verify/
  build_mingw_manual_qt6.cmd
  build_mingw.ps1
  README.md
  include/
    smoke_runner.h
    QTextCodec
  src/
    smoke_main.cpp
  scripts/
    common.ps1
```

## 默认依赖路径

```text
g++                 C:\msys64\ucrt64\bin\g++.exe
Qt6 include         C:\msys64\ucrt64\include\qt6
Qt6 lib             C:\msys64\ucrt64\lib
OpenCV include      C:\msys64\ucrt64\include\opencv4
OpenCV lib          C:\msys64\ucrt64\lib
ONNX Runtime root   C:\opt\onnxruntime-win-x64-1.25.0
```

## 命令

### 1. 查看编译命令

```cmd
set ORT_ROOT=C:\opt\onnxruntime-win-x64-1.25.0
cpp_verify\build_mingw_manual_qt6.cmd -DryRun
```

### 2. 编译

```cmd
set ORT_ROOT=C:\opt\onnxruntime-win-x64-1.25.0
cpp_verify\build_mingw_manual_qt6.cmd
```

### 3. 运行验证

```cmd
cpp_verify\out\cpp_verify_smoke.exe
```

验证程序默认执行 `initParas -> reciveData -> run -> sendData`，并在输出目录保存协议字节文件和 5 张结果图。

## 默认输入输出

```text
model      delivery/package/model/best.onnx
image dir  python_verify/adapter_input/
output dir cpp_verify/out/protocol_preview/
```

## 当前模型 IO

当前默认模型为固定输入尺寸的 YOLOv8-seg ONNX：

```text
input:
  images  tensor(float) [1, 3, 768, 768]

outputs:
  output0 tensor(float) [1, 37, 12096]
  output1 tensor(float) [1, 32, 192, 192]
```

如果更换模型后 IO 结构变化，需要同步调整 `delivery/reference_qt_adapter/seg_runtime.*`。

## 边界说明

- 本目录不保存第三方 SDK 二进制。
- 构建依赖 Qt、OpenCV、ONNX Runtime 和 MinGW 环境。
- 验证输出属于临时产物，不应放入 `delivery/package/`。
