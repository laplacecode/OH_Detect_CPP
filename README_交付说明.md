# OH_Detect_Station_Module 交付说明

## 0. 当前状态

本仓库当前定位为 OH 检测分割算法的交付与验证仓库，正式交付内容、客户侧参考源码和内部验证工具已经分层放置。

当前默认正式模型为：

```text
delivery/package/model/best.onnx
```

最近一次 ONNX IO 检查结果：

```text
input:
  images  tensor(float) [1, 3, 768, 768]

outputs:
  output0 tensor(float) [1, 37, 12096]
  output1 tensor(float) [1, 32, 192, 192]
```

该模型符合标准 YOLOv8-seg 双输出形态。`best.onnx` 需要配套预处理、后处理和协议组包逻辑，不能单独等同于完整交付模块。

## 1. 判定依据

本仓库当前交付边界以以下文件为准：

- [base.h](/D:/code/OH_Detect_Station_Module/base.h:1)
- [OH_Detect_Station_Module.h](/D:/code/OH_Detect_Station_Module/OH_Detect_Station_Module.h:1)
- [OH_Detect_Station_Module 算法接口说明文档.txt](/D:/code/OH_Detect_Station_Module/OH_Detect_Station_Module%20算法接口说明文档.txt:1)
- `delivery/package/doc/` 下的 PDF/TXT 文档

## 2. 当前目录约定

```text
delivery/
  package/
  reference_qt_adapter/

python_verify/
cpp_verify/
runs/
```

- `delivery/package/`：正式交付物
- `delivery/reference_qt_adapter/`：客户侧 Qt 参考适配源码
- `python_verify/`：内部 Python 训练、导出和 PDF 协议验证
- `cpp_verify/`：内部 C++ 适配验证脚手架
- `runs/`：训练和导出过程产生的输出目录

其中：

- `delivery/package/` 是正式交付核心内容
- `delivery/reference_qt_adapter/` 是参考源码附件，不替代正式交付包
- `python_verify/`、`cpp_verify/` 和 `runs/` 不属于正式交付物

## 3. 正式交付物

```text
delivery/package/
  base.h
  OH_Detect_Station_Module.h
  doc/
  model/
    best.onnx
  README.md
```

说明：

- 交付包保留原始接口头文件
- 当前可随包提供的 ONNX 文件是 `best.onnx`
- Python 和 C++ 验证目录不打包给客户

### 客户 Release 集成要点

- 头文件：将 `base.h` 和 `OH_Detect_Station_Module.h` 加入客户工程头文件搜索路径，业务代码包含 `OH_Detect_Station_Module.h`
- Release 链接库：`Hd_AI_Station_Jr_module.lib`、`Pack_AI_module.lib`、`opencv_world4100.lib`
- DLL 动态库：将 3 个 DLL 文件夹内的所有 `.dll` 拷贝到软件生成目录下，即最终 `.exe` 所在目录
- 模型配置：将 `model/` 文件夹整体拷贝到程序生成目录下，保持 `model/best.onnx` 路径结构
- 调用顺序：`initParas -> reciveData -> run -> sendData`
- 旧版客户 `myLibrary` 包装器示例已整理到 `delivery/package/README.md`，其中 `onnxPath` 对应程序生成目录下的 `model/<模型文件名>.onnx`

更完整的客户接入步骤请查看 `delivery/package/README.md`。

## 4. Python 验证流程

默认内部验证使用：

- 验证入口：`python python_verify/run_pdf_protocol_verify.py --promote-8bit`
- 验证模型：`delivery/package/model/best.onnx`
- 训练数据：`python_verify/test_Pit/data.yaml`
- 输入图目录：`python_verify/adapter_input/`
- 输出目录：`python_verify/pdf_protocol_preview/`

完整流程如下：

1. 使用 `python_verify/test_Pit/data.yaml` 训练，得到 `runs/segment/test_Pit/weights/best.pt`
2. 导出 `runs/segment/test_Pit/weights/best.onnx`
3. 用 `python_verify/check_onnx_io.py` 检查 ONNX 输入输出
4. 将验证通过的 `best.onnx` 覆盖到 `delivery/package/model/best.onnx`
5. 用 `python_verify/run_pdf_protocol_verify.py` 验证 `initParas -> reciveData -> run -> sendData`

注意：当前 `python_verify/test_Pit/data.yaml` 中的 `path` 仍可能保留历史路径。重新训练前应确认数据集根目录与当前仓库实际目录一致。

## 5. C++ 验证流程

默认 C++ 验证使用：

- 构建入口：`cpp_verify/build_mingw_manual_qt6.cmd`
- 验证入口：`cpp_verify/out/cpp_verify_smoke.exe`
- 输入图目录：`python_verify/adapter_input/`
- 输出目录：`cpp_verify/out/protocol_preview/`

在 `cmd` 中执行：

```cmd
set ORT_ROOT=C:\opt\onnxruntime-win-x64-1.25.0
cpp_verify\build_mingw_manual_qt6.cmd
cpp_verify\out\cpp_verify_smoke.exe
```

## 6. 接口约束摘要

### `initParas`

- 形式：`bool initParas(QByteArray& byte)`
- `byte` 为 UTF-8 字节数组
- 参数按固定顺序编码
- 不是 JSON 配置接口

### `reciveData`

- 输入图像至少包含一张 `CV_16U` 16 位灰度图
- `data` 也是固定顺序字段的 `QByteArray`

### `sendData`

- 输出图像共 5 张：
  - `m_drawingImage_8bit`
  - `m_rawImage_8bit`
  - `m_enhancedImage_8bit`
  - `m_segImage`
  - `m_segImageTo255`
- 输出 `data` 为固定顺序结果字段，不是 JSON

完整字段顺序请以 `delivery/package/doc/` 下 PDF/TXT 文档和原始头文件为准。

当前 Python 和 C++ 参考验证链路按以下字段数量组织：

- `initParas`：36 个字段
- `reciveData`：6 个字段
- `sendData`：23 个字段

## 7. 当前结论

- 正式交付应以 [delivery/package](/D:/code/OH_Detect_Station_Module/delivery/package) 为准
- [delivery/reference_qt_adapter](/D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter) 是客户侧参考源码附件
- [python_verify](/D:/code/OH_Detect_Station_Module/python_verify) 与 [cpp_verify](/D:/code/OH_Detect_Station_Module/cpp_verify) 仅供内部验证使用
- 当前 `best.onnx` 需要配套预处理、后处理和协议组包逻辑，不能单独等同于完整交付模块
- `delivery/reference_qt_adapter` 的测量逻辑用于复现当前验证链路中的代理结果，不应宣称等价于原始生产 OH 算法
