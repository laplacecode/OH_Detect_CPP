# 正式交付目录

本目录按原始头文件和《OH_Detect_Station_Module 算法接口说明文档》整理正式交付物。

## 目录内容

- `base.h`
  原始基础接口头文件
- `OH_Detect_Station_Module.h`
  原始模块接口头文件
- `doc/OH_Detect_Station_Module 算法接口说明文档.pdf`
  原始 PDF 说明文档
- `doc/OH_Detect_Station_Module 算法接口说明文档.txt`
  由 PDF 转出的文本版本，便于检索和核对字段顺序
- `model/best.onnx`
  当前仓库中可提供的 ONNX 模型文件

## 客户集成配置

### 1. 头文件

将 `base.h` 和 `OH_Detect_Station_Module.h` 加入客户工程的头文件搜索路径，业务代码包含模块头文件：

```cpp
#include "OH_Detect_Station_Module.h"
```

接口类为 `OH_Detect_Station_Module`，基础抽象接口为 `Base`。完整接口定义以本目录下两个头文件为准。

### 2. Release 链接库

Release 版本按交付配置链接以下 `.lib`：

```text
Hd_AI_Station_Jr_module.lib
Pack_AI_module.lib
opencv_world4100.lib
```

### 3. DLL 动态库

将 3 个 DLL 文件夹内的所有 `.dll` 拷贝到软件生成目录下，即最终 `.exe` 所在目录。运行时需要确保算法模块、打包模块和 OpenCV 相关 DLL 能被程序加载。

### 4. 模型配置

将 `model/` 文件夹整体拷贝到程序生成目录下，保持如下相对目录结构：

```text
程序生成目录/
  model/
    best.onnx
```

`initParas` 中传入的模型路径应指向程序生成目录下的 `model/best.onnx`，或使用等价的绝对路径。

### 5. 调用顺序

客户侧按以下生命周期调用：

```cpp
OH_Detect_Station_Module module;

module.initParas(initConfig);
module.reciveData(images, receiveData);
module.run();
module.sendData(resultImages, resultData);
```

`initConfig`、`receiveData` 和 `resultData` 均为 `QByteArray`，字段顺序以 `doc/` 下 PDF/TXT 文档和交付头文件为准。

### 6. 旧版客户包装器示例

如客户现场仍使用旧版 `myLibrary` 包装器，可按以下方式理解模块路径、模型路径和调用流程。`myLibrary` 不属于本交付包接口，具体函数签名以客户现有平台为准。

```cpp
QString currPath = QCoreApplication::applicationDirPath();
QString mdName = "Hd_AI_Station_Jr_module";

// 模块路径。是否需要带 .dll 后缀，以客户平台 loadModule 实现为准。
QString modulePath = QDir(currPath).filePath(mdName);

// 模型路径。当前交付包默认模型名为 best.onnx。
QString onnxPath = QDir(currPath).filePath("model/best.onnx");

QString imgPath = "D:/jier_Right.tif";
cv::Mat image = cv::imread(imgPath.toStdString(), cv::IMREAD_UNCHANGED);

if (!image.empty()) {
    myLibrary* myLib = new myLibrary();

    myLib->loadModule(modulePath, mdName);
    myLib->initModule(onnxPath, 0.0235);
    myLib->receive(image, "123");
    myLib->run();

    bool result = myLib->send();
    qDebug() << "result:" << result;

    delete myLib;
}
```

如果客户实际模型文件仍命名为 `602_JiEr_Detect_EigghtClasses_1024.onnx`，则需要将该文件放在程序生成目录的 `model/` 下，并把 `onnxPath` 改为对应文件名：

```cpp
QString onnxPath = QDir(currPath).filePath("model/602_JiEr_Detect_EigghtClasses_1024.onnx");
```

本仓库当前验证通过的正式模型文件为 `model/best.onnx`。交付时模型文件名、`initModule` 中的标定参数和客户现场包装器参数，应与客户实际加载逻辑保持一致。

## 交付边界

- 本目录只保留正式交付内容
- 不包含训练脚本、导出脚本、预览脚本、Python 验证代码或 C++ 验证脚手架
- 如需客户侧参考源码，请查看同级目录 `delivery/reference_qt_adapter/`

## 接口约束摘要

- `initParas(QByteArray&)` 使用 UTF-8 编码的固定顺序字段，不是 JSON
- `reciveData(std::vector<cv::Mat>&, QByteArray&)` 至少包含一张 `CV_16U` 灰度图
- `sendData(std::vector<cv::Mat>&, QByteArray&)` 输出 5 张约定结果图和固定顺序字段

完整字段顺序与协议说明，请查看根目录 [README_交付说明.md](/D:/code/OH_Detect_Station_Module/README_交付说明.md:1) 以及 `doc/` 下 PDF/TXT 文档。

## 模型说明

当前 `model/best.onnx` 的已验证 IO 形态为：

```text
input:
  images  tensor(float) [1, 3, 768, 768]

outputs:
  output0 tensor(float) [1, 37, 12096]
  output1 tensor(float) [1, 32, 192, 192]
```
