# OH_Detect_Station_Module 客户验证说明

## 目录内容

- `Hd_AI_Station_Jr_module.dll`：算法模块动态库
- `Hd_AI_Station_Jr_module.lib`：MinGW Release 导入库
- `verify_dll_smoke.exe`：动态加载验证程序
- `run_verify.bat`：一键验证脚本
- `model/best.onnx`：当前交付模型
- `input/`：验证输入图像
- `output/`：验证输出目录
- `include/`：接口头文件
- `doc/`：接口说明文档

## 验证方式

双击或在命令行执行：

```bat
run_verify.bat
```

验证程序会动态加载 `Hd_AI_Station_Jr_module.dll`，调用：

```text
create -> initParas -> reciveData -> run -> sendData -> destory
```

验证成功后，`output/` 下会生成协议文件和 5 张结果图：

```text
m_drawingImage_8bit.png
m_rawImage_8bit.png
m_enhancedImage_8bit.png
m_segImage.png
m_segImageTo255.png
sendData.bin
```

## 集成说明

客户工程编译时使用 `include/` 下头文件，并在 Release 配置中链接 `Hd_AI_Station_Jr_module.lib`。运行时需保证本目录下所有 `.dll` 与客户 `.exe` 位于同一目录，或位于系统 `PATH` 可搜索目录。

模型路径建议按程序目录组织：

```text
程序目录/model/best.onnx
```

如果客户现场使用其他模型文件名，需要同步调整 `initParas` 中传入的模型路径。