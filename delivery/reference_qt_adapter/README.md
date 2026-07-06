# Qt 参考适配器源码

本目录提供一个面向客户集成的 Qt 参考适配器源码，实现形式保持与正式交付头文件一致的 `Qt + QByteArray` 接口形态。

## 目录定位

- 本目录是参考源码，不替代 `delivery/package/` 正式交付包。
- 正式交付接口定义仍以 `delivery/package/base.h` 和 `delivery/package/OH_Detect_Station_Module.h` 为准。
- 本目录的目标是复现当前仓库已经验证的协议流程和 ONNX 适配逻辑，便于客户侧理解与接入。
- 本目录对应的是客户侧 Qt 集成参考，不是内部 Python 验证目录。

## 依赖

- Qt
- OpenCV
- ONNX Runtime

## 生命周期

参考实现遵循如下调用顺序：

1. `initParas(QByteArray& config)`
2. `reciveData(std::vector<cv::Mat>& images, QByteArray& data)`
3. `run()`
4. `sendData(std::vector<cv::Mat>& images, QByteArray& data)`

## 协议字段数量

当前参考实现与 Python 验证链路保持一致，使用逐行 UTF-8 文本字段作为 `QByteArray` 的内部验证约定：

```text
initParas   36 个字段
reciveData   6 个字段
sendData    23 个字段
```

字段顺序不能重排。完整语义以 `delivery/package/doc/` 下原始接口说明文档和交付头文件为准。

## 仓库内验证入口

- Python 协议验证入口：`python python_verify/run_pdf_protocol_verify.py --promote-8bit`
- C++ 适配验证构建入口：`cpp_verify\build_mingw_manual_qt6.cmd`
- C++ 验证产物：`cpp_verify\out\cpp_verify_smoke.exe`
- 当前默认验证输入图目录：`python_verify/adapter_input/`
- 当前默认 C++ 验证输出目录：`cpp_verify/out/protocol_preview/`

## 源文件说明

- `OH_Detect_Station_Module.cpp`
  接口实现与状态管理。
- `protocol_codec.*`
  固定顺序 `QByteArray` 编解码。
- `image_pipeline.*`
  16 位图预处理、8 位增强、裁切与结果图恢复。
- `seg_runtime.*`
  ONNX Runtime 推理、标准 YOLOv8-seg 双输出解析与分割图重建。

## 模型约束

当前默认交付模型为 `delivery/package/model/best.onnx`。已检查的 ONNX IO 形态为：

```text
input:
  images  tensor(float) [1, 3, 768, 768]

outputs:
  output0 tensor(float) [1, 37, 12096]
  output1 tensor(float) [1, 32, 192, 192]
```

如果模型输入尺寸、输出数量或输出张量结构变化，需要同步调整 `seg_runtime.*` 和 Python 验证链路。

## 边界说明

- 当前参考实现依赖 `delivery/package/model/best.onnx` 这类标准 YOLOv8-seg 双输出模型。
- 当前参考实现说明了：`best.onnx` 需要配套的预处理、后处理和协议组包逻辑，不能单独等同于完整交付模块。
- 当前参考实现用于复现本仓库已验证的协议行为。
- 当前参考实现不等同于原始 OH 生产算法，只能视为客户可集成的参考源码。
- 当前参考实现中的 OH、对齐度和层数结果是验证链路中的代理几何统计，不应作为生产算法等价声明。
