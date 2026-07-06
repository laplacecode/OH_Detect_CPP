把测试图片放到本目录。

支持格式：
- .png
- .jpg
- .jpeg
- .bmp
- .tif
- .tiff

使用方法：
1. 把一张测试图片复制到本目录。
2. 在项目根目录执行：
   python python_verify/run_pdf_protocol_verify.py --promote-8bit

说明：
- 本目录只服务于 python_verify 下的 Python 协议验证流程。
- 验证默认使用 delivery/package/model/best.onnx。
- 如果放入的是 8 位测试图，需要使用 --promote-8bit 临时提升为伪 16 位灰度图。
- 生产输入应按接口约束提供单通道 CV_16U 灰度图。
- 客户侧 Qt 参考接入请查看 delivery/reference_qt_adapter/。
- 输出结果会保存到：
  python_verify/pdf_protocol_preview/
