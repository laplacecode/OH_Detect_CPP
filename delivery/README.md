# delivery 目录说明

本目录用于整理正式交付内容和客户参考源码附件。

## 当前结构

```text
delivery/
  package/
  reference_qt_adapter/
```

## 使用方式

- `package/`
  正式交付包，只包含接口头文件、模型、正式说明文档和交付 README。
- `reference_qt_adapter/`
  客户可集成的 Qt 参考适配源码，用于说明如何把 `best.onnx` 接到现有 `Qt + QByteArray` 接口形态。

## 交付边界

- 对外交付核心内容：`package/`
- 可随交付提供的参考源码附件：`reference_qt_adapter/`
- 根目录 `python_verify/`、`cpp_verify/` 和 `runs/` 为内部验证内容，不属于正式交付物

## 当前模型约束

当前默认交付模型为：

```text
package/model/best.onnx
```

该模型是固定输入尺寸的 YOLOv8-seg ONNX 模型：

```text
input   images  [1, 3, 768, 768]
output  output0 [1, 37, 12096]
output  output1 [1, 32, 192, 192]
```

`best.onnx` 不能单独代表完整模块，必须配套参考适配器中的图像预处理、模型后处理和 `QByteArray` 协议组包逻辑。
