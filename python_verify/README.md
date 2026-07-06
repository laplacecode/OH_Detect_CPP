# python_verify 目录说明

本目录用于内部训练、导出、ONNX 检查和 PDF 协议验证，不属于正式交付物。

如需客户侧 C++/Qt 参考接入源码，请查看 `delivery/reference_qt_adapter/`。

## 快速执行

在项目根目录按顺序执行：

```bash
python python_verify/train_yolov8_seg.py --data python_verify/test_Pit/data.yaml --device cpu --epochs 200 --batch 4
python python_verify/export_yolov8_seg_onnx.py --weights runs/segment/test_Pit/weights/best.pt
python python_verify/check_onnx_io.py --model delivery/package/model/best.onnx
python python_verify/run_pdf_protocol_verify.py --promote-8bit
```

如果 ONNX 检查和 PDF 协议验证都通过，再把验证通过的模型覆盖到正式交付目录：

```powershell
Copy-Item runs\segment\test_Pit\weights\best.onnx delivery\package\model\best.onnx -Force
```

## 默认目录

- 训练集：`python_verify/test_Pit/`
- 测试图：`python_verify/adapter_input/`
- 验证输出：`python_verify/pdf_protocol_preview/`
- 预训练权重：仓库根目录 `yolov8n-seg.pt`

## 当前模型 IO

`delivery/package/model/best.onnx` 当前已检查的输入输出为：

```text
input:
  images  tensor(float) [1, 3, 768, 768]

outputs:
  output0 tensor(float) [1, 37, 12096]
  output1 tensor(float) [1, 32, 192, 192]
```

这是标准 YOLOv8-seg 双输出形态。若重新导出模型后 IO 发生变化，需要同步检查 `pdf_protocol_adapter.py`、`seg_runtime_utils.py` 和 C++ 参考适配器。

## 数据集提醒

当前 `test_Pit` 数据集位于 `python_verify/test_Pit/`。重新训练前应检查 `python_verify/test_Pit/data.yaml` 中的 `path` 是否仍指向旧目录；如果路径不一致，Ultralytics 训练会找不到图片或读取到错误数据。

## 当前文件

- `train_yolov8_seg.py`：训练脚本
- `export_yolov8_seg_onnx.py`：ONNX 导出脚本
- `check_onnx_io.py`：ONNX 输入输出检查脚本
- `pdf_protocol_codec.py`：固定顺序 `QByteArray` 字段编解码
- `seg_runtime_utils.py`：分割推理与分割图重建工具
- `pdf_protocol_adapter.py`：PDF 生命周期验证适配实现
- `run_pdf_protocol_verify.py`：PDF 协议验证主入口
- `test_Pit/`：默认训练数据集
- `adapter_input/`：协议验证输入图目录
- `pdf_protocol_preview/`：协议验证输出目录
- `ideal_example/`：示意结果，不属于主验证流程
