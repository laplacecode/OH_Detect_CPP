# OH_Detect_Station_Module 客户使用教程（旧版配置风格）

## 1. 说明

本文按客户旧版算法配置习惯整理，用于说明如何使用当前交付的 `OH_Detect_Station_Module` 算法模块。

客户旧版配置中通常包含：

```text
Release 版本链接 lib
运行目录拷贝 dll
程序生成目录拷贝 model 文件夹
代码中配置模块路径和 ONNX 模型路径
调用 loadModule / initModule / receive / run / send
```

当前项目已经提供可运行验证包，验证包路径为：

```text
delivery/release/OH_Detect_Station_Module_Release/
```

客户可先直接运行验证包，确认算法 DLL 和 ONNX 模型可以正常工作，再接入正式软件。

## 2. 当前交付包目录

客户拿到的验证包目录建议保持如下结构：

```text
OH_Detect_Station_Module_Release/
  run_verify.bat
  verify_dll_smoke.exe

  Hd_AI_Station_Jr_module.dll
  Hd_AI_Station_Jr_module.lib
  onnxruntime.dll
  Qt6Core.dll
  Qt6Gui.dll
  OpenCV 相关 dll
  MinGW 运行库 dll

  include/
    base.h
    OH_Detect_Station_Module.h

  model/
    best.onnx

  input/
    01_Pit_80.jpg

  output/

  doc/
    README_verify.md
    customer_usage_old_style.md
    interface/
      OH_Detect_Station_Module 算法接口说明文档.pdf
      OH_Detect_Station_Module 算法接口说明文档.txt
```

## 3. 客户直接验证方式

客户如果只是先验证算法是否能运行，不需要配置 C++ 编译环境。

### 3.1 操作步骤

1. 将整个 `OH_Detect_Station_Module_Release/` 文件夹拷贝到客户电脑。
2. 进入该目录。
3. 双击或在命令行执行：

```bat
run_verify.bat
```

### 3.2 验证程序执行内容

`run_verify.bat` 会调用：

```text
verify_dll_smoke.exe
```

验证程序会动态加载：

```text
Hd_AI_Station_Jr_module.dll
```

并按以下顺序调用算法接口：

```text
create -> initParas -> reciveData -> run -> sendData -> destory
```

### 3.3 成功输出

命令行中看到以下结果，表示验证通过：

```text
DllSmokeResult=PASS
VerifyResult=PASS
```

`output/` 目录下会生成：

```text
initParas.bin
reciveData.bin
sendData.bin
m_drawingImage_8bit.png
m_rawImage_8bit.png
m_enhancedImage_8bit.png
m_segImage.png
m_segImageTo255.png
```

## 4. Release 版本工程配置

如果客户需要把算法集成到自己的 C++ 工程中，按以下方式配置。

### 4.1 头文件

将以下目录加入客户工程的 Include 路径：

```text
OH_Detect_Station_Module_Release/include
```

业务代码包含：

```cpp
#include "OH_Detect_Station_Module.h"
```

### 4.2 Release 链接库

当前验证包提供：

```text
Hd_AI_Station_Jr_module.lib
```

客户 Release 工程需要链接该库。

如果客户旧工程仍要求以下库：

```text
Pack_AI_module.lib
opencv_world4100.lib
```

需要注意：

```text
Pack_AI_module.lib 不在当前验证包内，需要客户现场已有该 Pack 层，或后续单独提供。
opencv_world4100.lib 是客户旧 OpenCV 4.10 world 库要求；当前验证包使用的是 MinGW OpenCV 4.13 拆分 DLL。
```

如果客户必须使用 `opencv_world4100.lib` 或 MSVC 工程，需要按客户指定编译环境重新构建正式版本。

### 4.3 DLL 动态库

运行时需要将发布包根目录下所有 `.dll` 放到客户软件生成目录，也就是最终 `.exe` 同级目录。

最小原则：

```text
客户程序.exe
Hd_AI_Station_Jr_module.dll
onnxruntime.dll
Qt6Core.dll
Qt6Gui.dll
OpenCV 相关 dll
MinGW/MSVC 运行库 dll
model/
  best.onnx
```

如果客户使用自己的 Pack 层，还需要同时放置：

```text
Pack_AI_module.dll
```

以及 Pack 层依赖的其他 DLL。

## 5. 模型配置

将 `model/` 文件夹整体拷贝到客户软件生成目录下，保持：

```text
客户程序生成目录/
  model/
    best.onnx
```

代码中建议使用程序目录拼接模型路径：

```cpp
QString currPath = QCoreApplication::applicationDirPath();
QString onnxPath = QDir(currPath).filePath("model/best.onnx");
```

如果客户现场仍使用旧模型名，例如：

```text
602_JiEr_Detect_EigghtClasses_1024.onnx
```

则目录和代码应改为：

```text
客户程序生成目录/
  model/
    602_JiEr_Detect_EigghtClasses_1024.onnx
```

```cpp
QString onnxPath = QDir(currPath).filePath("model/602_JiEr_Detect_EigghtClasses_1024.onnx");
```

当前交付包默认模型为：

```text
model/best.onnx
```

## 6. 旧版 myLibrary 风格调用示例

如果客户软件仍使用旧版 `myLibrary` 加载器，可按以下方式理解当前模块配置。

```cpp
QString currPath = QCoreApplication::applicationDirPath();
QString mdName = "Hd_AI_Station_Jr_module";

// 模块路径。是否带 .dll 后缀，以客户 loadModule 的实现为准。
QString modulePath = QDir(currPath).filePath(mdName);

// 当前交付包默认模型路径。
QString onnxPath = QDir(currPath).filePath("model/best.onnx");

QString imgPath = "D:/jier_Right.tif";
cv::Mat image = cv::imread(imgPath.toStdString(), cv::IMREAD_UNCHANGED);

if (!image.empty()) {
    myLibrary* myLib = new myLibrary();

    // 加载模块
    myLib->loadModule(modulePath, mdName);

    // 初始化模块。0.0235 为客户旧示例中的标定参数，实际值以现场配置为准。
    myLib->initModule(onnxPath, 0.0235);

    // 接收图像和附加数据
    myLib->receive(image, "123");

    // 运行算法
    myLib->run();

    // 获取结果
    bool result = myLib->send();
    qDebug() << "result:" << result;

    delete myLib;
}
```

说明：

```text
myLibrary、loadModule、initModule、receive、send 属于客户旧 Pack/加载器层接口。
当前算法 DLL 对外导出的是 create/destory，并实现 initParas/reciveData/run/sendData。
如果客户 Pack 层按 create/destory 加载算法模块，则可以对接当前 DLL。
```

## 7. 当前算法 DLL 原生接口

当前算法模块原生接口为：

```cpp
bool initParas(QByteArray& config);
bool reciveData(std::vector<cv::Mat>& images, QByteArray& data);
bool run();
bool sendData(std::vector<cv::Mat>& images, QByteArray& data);
```

动态库导出函数为：

```cpp
extern "C" {
    Q_DECL_EXPORT OH_Detect_Station_Module* create();
    Q_DECL_EXPORT void destory(OH_Detect_Station_Module* instance);
}
```

当前验证程序 `verify_dll_smoke.exe` 就是通过 `LoadLibrary` 加载 `Hd_AI_Station_Jr_module.dll`，再调用 `create/destory` 和上述算法接口完成验证。

## 8. 推荐客户使用顺序

建议客户按以下顺序使用：

1. 先运行 `run_verify.bat`，确认发布包在客户电脑可以运行。
2. 确认 `output/` 下是否生成 5 张结果图和 `sendData.bin`。
3. 再将 `include/`、`Hd_AI_Station_Jr_module.lib`、所有 DLL、`model/` 集成到客户工程。
4. 如果客户必须通过 `Pack_AI_module` 或 `myLibrary` 接入，确认 Pack 层是否能加载当前 DLL 的 `create/destory`。
5. 如果客户要求 MSVC 或 `opencv_world4100.lib`，重新按客户环境编译正式版本。

## 9. 常见问题

### 9.1 只拷贝 best.onnx 能不能运行？

不能。ONNX 只是模型文件。完整运行需要：

```text
Hd_AI_Station_Jr_module.dll
onnxruntime.dll
OpenCV/Qt 运行 DLL
model/best.onnx
输入图像
调用程序
```

### 9.2 run_verify.bat 提示 DLL 找不到怎么办？

确认所有 `.dll` 和 `verify_dll_smoke.exe` 在同一个目录，或者 DLL 所在目录已加入系统 `PATH`。

### 9.3 客户旧代码中的模型名和 best.onnx 不一致怎么办？

两种处理方式任选一种：

```text
方式一：客户代码改成 model/best.onnx
方式二：把模型文件改名为客户旧代码中使用的文件名，并同步验证
```

### 9.4 当前 lib 能不能直接给 MSVC 使用？

当前 `Hd_AI_Station_Jr_module.lib` 是 MinGW 导入库。MSVC 工程通常不能直接使用。若客户使用 Visual Studio/MSVC，需要重新生成 MSVC 版本 DLL 和 LIB。

## 10. 一句话总结

客户直接验证时，拷贝整个 `OH_Detect_Station_Module_Release/` 并运行 `run_verify.bat`。

客户集成软件时，将头文件、`Hd_AI_Station_Jr_module.lib`、所有运行 DLL 和 `model/` 放到对应工程和程序生成目录，并按客户旧流程传入模型路径后调用算法。