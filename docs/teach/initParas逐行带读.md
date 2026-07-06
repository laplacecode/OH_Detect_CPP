# `initParas` 逐行带读

这份文档只讲一件事：

`当外部调用 initParas(QByteArray& config) 时，这段代码到底一行一行做了什么。`

如果你是新手，建议和这两份文档一起看：

- [C++零基础语法入门.md](/D:/code/OH_Detect_Station_Module/docs/teach/C++零基础语法入门.md:1)
- [执行流程图解.md](/D:/code/OH_Detect_Station_Module/docs/teach/执行流程图解.md:1)

---

## 1. 先看 `initParas` 的目标

`initParas` 的职责不是推理，也不是处理图像。

它只负责两件事：

1. 把外部传进来的配置字节流 `config` 解码成结构化参数
2. 用这些参数把 ONNX Runtime 初始化好

所以你可以把它理解成：

`模块开机。`

只有这一步成功了，后面的 `reciveData` 和 `run` 才有意义。

---

## 2. 先看函数本体

对应源码位置：

- [OH_Detect_Station_Module.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/OH_Detect_Station_Module.cpp:228)

核心代码是这一段：

```cpp
bool OH_Detect_Station_Module::initParas(QByteArray& config) {
    auto& state = reference_qt_adapter::state_for(this);
    QString error;
    if (!reference_qt_adapter::decode_init_paras(config, state.init_paras, &error)) {
        qWarning() << "initParas decode failed:" << error;
        return false;
    }

    QFileInfo model_info(state.init_paras.onnx_model_path);
    QString resolved_model_path = model_info.absoluteFilePath();
    if (!model_info.exists()) {
        qWarning() << "initParas model path does not exist:" << resolved_model_path;
        return false;
    }

    if (!state.runtime.init(reference_qt_adapter::to_wstring_path(resolved_model_path), &error)) {
        qWarning() << "initParas runtime init failed:" << error;
        return false;
    }

    state.has_init = true;
    state.has_receive = false;
    state.has_run = false;
    b_initFlag = true;
    this->model_path = QDir::toNativeSeparators(resolved_model_path).toStdString();
    this->saveFolder_Path = state.init_paras.image_and_csv_save_path.toStdString();
    return true;
}
```

你第一次看可能会觉得密，但其实可以拆成 5 步：

1. 取到当前对象对应的内部状态 `state`
2. 解码 `config`
3. 检查模型路径是否存在
4. 初始化 ONNX Runtime
5. 更新“已初始化”状态

下面就按这 5 步拆开讲。

---

## 3. 第一步：找到当前对象自己的状态

代码：

```cpp
auto& state = reference_qt_adapter::state_for(this);
```

这行最容易把新手劝退，但意思其实不复杂。

### 先拆开看

- `this`
  表示“当前这个对象自己”
- `state_for(this)`
  表示“去状态表里找到这个对象对应的那份状态”
- `auto& state`
  表示“把找到的那份状态起个别名，叫 `state`”

### 为什么这里需要 `state`

因为交付头文件没有给参考实现增加私有成员，所以作者没有把这些运行时数据直接写进头文件类定义里，而是在 `.cpp` 里额外维护了一张状态表。

这个状态表在前面定义：

- [OH_Detect_Station_Module.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/OH_Detect_Station_Module.cpp:35)

```cpp
std::map<const OH_Detect_Station_Module*, std::unique_ptr<AdapterState>>& state_map()
```

你可以先这样理解：

- 每创建一个 `OH_Detect_Station_Module` 对象
- 就额外配一份 `AdapterState`
- `state_for(this)` 就是把属于这个对象的那份 `AdapterState` 找出来

`AdapterState` 里装着：

- 初始化参数 `init_paras`
- 输入参数 `receive_fields`
- 输出参数 `send_fields`
- 推理器 `runtime`
- 若干中间图
- 状态标记 `has_init/has_receive/has_run`

所以这一步本质上是在做：

`拿到自己的工作台。`

---

## 4. 第二步：准备一个错误字符串

代码：

```cpp
QString error;
```

这一步很简单。

作者先准备了一个空字符串 `error`，后面如果解码失败或者 ONNX 初始化失败，就把详细原因写到这里。

为什么不直接只返回 `true/false`？

因为：

- `true/false` 只能告诉你成没成功
- 但不能告诉你为什么失败

所以这里是一个常见模式：

- `bool` 表示成功或失败
- `QString* error` 用来带回详细错误信息

---

## 5. 第三步：解码 `config`

代码：

```cpp
if (!reference_qt_adapter::decode_init_paras(config, state.init_paras, &error)) {
    qWarning() << "initParas decode failed:" << error;
    return false;
}
```

这一段的意思是：

- 调用 `decode_init_paras`
- 把 `config` 解析到 `state.init_paras`
- 如果失败，就打印日志并返回 `false`

### 这三个参数分别是什么

#### 参数 1：`config`

外部传进来的原始字节流。

当前参考实现把它当成：

`按行组织的 UTF-8 文本`

不是二进制结构体，也不是 JSON。

它更像这样：

```text
保留字段
模型路径
图片保存路径
像素到物理尺寸比例
窗宽
窗位
...
```

#### 参数 2：`state.init_paras`

这是“解析后的结果对象”。

类型是：

- [protocol_codec.h](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.h:8) 的 `InitParasData`

也就是说：

- 输入原本是一串字节
- 解码后，会变成一个字段清晰的结构体

#### 参数 3：`&error`

把错误字符串的地址传进去。

这样 `decode_init_paras` 内部如果出错，就能把原因写回来。

---

## 6. `decode_init_paras` 里面先做了什么

对应源码位置：

- [protocol_codec.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.cpp:67)

先看开头：

```cpp
bool decode_init_paras(const QByteArray& raw, InitParasData& result, QString* error) {
    const QStringList tokens = decode_lines(raw);
    if (!expect_count(tokens, 36, error)) {
        return false;
    }
```

这段代码做了两件事：

1. 把字节流拆成很多行
2. 检查行数是不是正好 36 个

---

## 7. `decode_lines(raw)` 在干什么

对应源码位置：

- [protocol_codec.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.cpp:8)

代码：

```cpp
QStringList decode_lines(const QByteArray& raw) {
    QString text = QString::fromUtf8(raw);
    QStringList lines = text.split('\n');
    for (QString& line : lines) {
        if (line.endsWith('\r')) {
            line.chop(1);
        }
    }
    return lines;
}
```

一行一行解释：

### `QString text = QString::fromUtf8(raw);`

把字节流按 UTF-8 解释成文本。

这一步非常关键，因为它等于明确声明：

`当前协议编码按 UTF-8 处理。`

### `QStringList lines = text.split('\n');`

把整段文本按换行拆成很多行。

例如原来是：

```text
abc
123
true
```

拆开后就是一个“字符串列表”：

- 第 1 行：`abc`
- 第 2 行：`123`
- 第 3 行：`true`

### `if (line.endsWith('\r')) { line.chop(1); }`

这是在兼容 Windows 换行。

有些文本文件的换行其实是：

```text
\r\n
```

如果只按 `\n` 切，会残留一个 `\r`。

所以这里顺手把行尾多出来的 `\r` 去掉。

这一步做完后，`raw` 就变成了干净的 `tokens`。

---

## 8. 为什么一定要检查 36 个字段

代码：

```cpp
if (!expect_count(tokens, 36, error)) {
    return false;
}
```

对应辅助函数：

- [protocol_codec.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.cpp:48)

```cpp
bool expect_count(const QStringList& tokens, int expected, QString* error) {
    if (tokens.size() == expected) {
        return true;
    }
    if (error != nullptr) {
        *error = QString("Expected %1 fields, got %2.").arg(expected).arg(tokens.size());
    }
    return false;
}
```

意思很直接：

- `initParas` 协议规定必须有 36 个字段
- 少一个不行，多一个也不行

为什么要这么严格？

因为这里走的是“固定顺序协议”，不是带字段名的 JSON。

既然没有字段名，程序只能靠“位置”认字段。

例如：

- 第 0 行是什么
- 第 1 行是什么
- 第 2 行是什么

如果行数一错，后面的字段就全会错位。

所以这一步是在防止：

`整条协议从根上错位。`

---

## 9. 接下来怎么把文本行写进结构体

代码：

```cpp
result.reserved = tokens.at(0);
result.onnx_model_path = tokens.at(1);
result.image_and_csv_save_path = tokens.at(2);
```

这 3 行说明：

- 第 0 个字段给 `reserved`
- 第 1 个字段给 `onnx_model_path`
- 第 2 个字段给 `image_and_csv_save_path`

也就是说，协议完全依赖“固定位置”。

后面更多数字字段不是直接赋值，而是调用 `parse_with(...)`。

例如：

```cpp
parse_with(parse_double_token, tokens, 3, result.pixel_to_physical_scale, error, "pixel_to_physical_scale")
```

这个意思是：

- 取第 3 个字段
- 按 `double` 解析
- 成功后写进 `result.pixel_to_physical_scale`
- 如果失败，就把错误信息写到 `error`

---

## 10. `parse_with` 到底在帮什么忙

对应源码位置：

- [protocol_codec.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.cpp:57)

代码：

```cpp
template <typename T>
bool parse_with(bool (*fn)(const QString&, T&), const QStringList& tokens, int index, T& target, QString* error, const char* field_name) {
    if (fn(tokens.at(index), target)) {
        return true;
    }
    if (error != nullptr) {
        *error = QString("Failed to parse field '%1' at index %2.").arg(field_name).arg(index);
    }
    return false;
}
```

你先不要管 `template` 这个语法细节，只看它的目的：

`统一处理“取某个字段 -> 按某种类型解析 -> 失败时报错”这件事。`

否则作者就得重复写很多遍：

- 取 token
- 调解析函数
- 判断成功失败
- 写错误消息

所以 `parse_with` 是一个“通用小助手”。

---

## 11. 数字和布尔值是怎么解析的

### `double`

对应：

- [protocol_codec.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.cpp:38)

```cpp
bool parse_double_token(const QString& token, double& result) {
    bool ok = false;
    const double value = token.trimmed().toDouble(&ok);
    if (ok) {
        result = value;
    }
    return ok;
}
```

意思是：

- 把字符串去掉首尾空白
- 尝试转成 `double`
- 转成功就写入结果
- 转失败就返回 `false`

### `bool`

对应：

- [protocol_codec.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.cpp:18)

```cpp
bool parse_bool_token(const QString& token, bool& result) {
    const QString lowered = token.trimmed().toLower();
    if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "y" || lowered == "on") {
        result = true;
        return true;
    }
    if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "n" || lowered == "off") {
        result = false;
        return true;
    }
    return false;
}
```

这表示当前协议对布尔值比较宽容。

下面这些都能被当成真：

- `true`
- `1`
- `yes`
- `y`
- `on`

下面这些都能被当成假：

- `false`
- `0`
- `no`
- `n`
- `off`

这让外部生成配置时更不容易因为大小写或格式差异失败。

---

## 12. 解码成功后，`initParas` 拿到了什么

当 `decode_init_paras` 成功返回后：

```cpp
state.init_paras
```

里就已经有了完整初始化参数。

其中最关键的几个字段包括：

- `onnx_model_path`
  模型路径
- `image_and_csv_save_path`
  输出保存路径
- `window_width`
  窗宽
- `window_level`
  窗位
- `pixel_to_physical_scale`
  像素转物理尺度的比例
- 各种 OH / 对齐 / 层数阈值与开关

到这一步为止，程序只是：

`把配置读懂了`

还没有真正加载模型。

---

## 13. 第四步：检查模型路径

回到 [OH_Detect_Station_Module.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/OH_Detect_Station_Module.cpp:236) 附近：

```cpp
QFileInfo model_info(state.init_paras.onnx_model_path);
QString resolved_model_path = model_info.absoluteFilePath();
if (!model_info.exists()) {
    qWarning() << "initParas model path does not exist:" << resolved_model_path;
    return false;
}
```

这里做了 3 件事：

### 1. 用 `QFileInfo` 包装路径

`QFileInfo` 是 Qt 里专门处理文件路径和文件信息的类。

它可以告诉你：

- 文件存不存在
- 绝对路径是什么
- 文件名是什么

### 2. 取绝对路径

```cpp
QString resolved_model_path = model_info.absoluteFilePath();
```

这样做的好处是：

- 即使外面传的是相对路径
- 后面也尽量用更明确的绝对路径

### 3. 检查文件是否存在

```cpp
if (!model_info.exists()) { ... }
```

如果模型文件压根不存在，那就没必要继续初始化 ONNX Runtime 了，直接失败是最合理的。

这一步是在做：

`先确认原料真的在。`

---

## 14. 第五步：初始化 ONNX Runtime

代码：

```cpp
if (!state.runtime.init(reference_qt_adapter::to_wstring_path(resolved_model_path), &error)) {
    qWarning() << "initParas runtime init failed:" << error;
    return false;
}
```

这里的核心是调用：

- [seg_runtime.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/seg_runtime.cpp:23) 的 `SegRuntime::init`

传进去的是：

- 经过路径转换后的模型路径
- 错误输出字符串

---

## 15. 为什么路径要先转成 `std::wstring`

你会看到这里没有直接把 `QString` 传进去，而是先调用：

```cpp
reference_qt_adapter::to_wstring_path(resolved_model_path)
```

对应源码位置：

- [OH_Detect_Station_Module.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/OH_Detect_Station_Module.cpp:42)

```cpp
std::wstring to_wstring_path(const QString& path) {
    return QDir::toNativeSeparators(path).toStdWString();
}
```

这一步的目的有两个：

1. 把路径分隔符转成当前系统习惯的形式
2. 把 Qt 字符串转成 ONNX Runtime 这边更容易接受的宽字符路径

在 Windows 下，宽字符路径通常更稳，尤其是路径里可能有中文时。

---

## 16. `SegRuntime::init` 里面又做了什么

对应源码位置：

- [seg_runtime.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/seg_runtime.cpp:23)

核心代码：

```cpp
bool SegRuntime::init(const std::wstring& model_path, QString* error) {
    try {
        session_options_ = Ort::SessionOptions();
        session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        session_ = std::make_unique<Ort::Session>(env_, model_path.c_str(), session_options_);

        Ort::AllocatorWithDefaultOptions allocator;
        auto input_name = session_->GetInputNameAllocated(0, allocator);
        input_name_ = input_name.get();

        output_names_.clear();
        const size_t output_count = session_->GetOutputCount();
        for (size_t index = 0; index < output_count; ++index) {
            auto output_name = session_->GetOutputNameAllocated(index, allocator);
            output_names_.push_back(output_name.get());
        }
        return true;
    } catch (const std::exception& ex) {
        if (error != nullptr) {
            *error = QString("Failed to initialize ONNX Runtime session: %1").arg(ex.what());
        }
        session_.reset();
        return false;
    }
}
```

这段可以拆成 4 步。

### 第 1 步：创建 `SessionOptions`

```cpp
session_options_ = Ort::SessionOptions();
```

这相当于先准备 ONNX Runtime 的启动参数对象。

### 第 2 步：设置图优化等级

```cpp
session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
```

意思是启用较高等级的图优化。

你现在不用深究图优化细节，只要知道：

`这是在让 ONNX Runtime 以更合适的方式加载和执行模型。`

### 第 3 步：真正创建 Session

```cpp
session_ = std::make_unique<Ort::Session>(env_, model_path.c_str(), session_options_);
```

这是最关键的一步。

它真正把模型文件加载成一个可推理的 ONNX Runtime Session。

如果这一步失败，通常说明：

- 模型文件损坏
- 路径有问题
- ONNX Runtime 依赖不完整
- 模型格式不兼容

### 第 4 步：读取输入名和输出名

```cpp
auto input_name = session_->GetInputNameAllocated(0, allocator);
input_name_ = input_name.get();
```

以及：

```cpp
const size_t output_count = session_->GetOutputCount();
for (size_t index = 0; index < output_count; ++index) {
    auto output_name = session_->GetOutputNameAllocated(index, allocator);
    output_names_.push_back(output_name.get());
}
```

意思是：

- 记住模型输入张量名
- 记住所有输出张量名

为什么要提前记住？

因为后面 `run()` 推理时要用这些名字去喂数据和取结果。

---

## 17. 为什么这里用了 `try/catch`

你会看到 `SegRuntime::init` 外层包了一层：

```cpp
try {
    ...
} catch (const std::exception& ex) {
    ...
}
```

原因是 ONNX Runtime 初始化过程中，底层库可能直接抛异常。

如果不接住，程序可能直接崩掉。

这里的处理方式更稳：

- 把异常接住
- 写入可读的错误信息
- 清理 `session_`
- 返回 `false`

这一步是在做：

`把“可能崩溃”改成“可控失败”。`

---

## 18. 回到 `initParas`，最后做了什么

如果 ONNX Runtime 初始化成功，就会执行：

```cpp
state.has_init = true;
state.has_receive = false;
state.has_run = false;
b_initFlag = true;
this->model_path = QDir::toNativeSeparators(resolved_model_path).toStdString();
this->saveFolder_Path = state.init_paras.image_and_csv_save_path.toStdString();
return true;
```

这几行可以拆成两类动作。

### 第一类：更新状态标记

- `state.has_init = true`
- `state.has_receive = false`
- `state.has_run = false`
- `b_initFlag = true`

意思是：

- 当前对象已经完成初始化
- 但还没收到本次输入
- 也还没跑推理

这其实是在重置流程阶段，告诉后面的代码：

`现在可以接收新数据了，但结果还没算。`

### 第二类：保存一些路径信息

- `this->model_path`
- `this->saveFolder_Path`

这些字段来自原交付头文件的接口形态，参考实现把已经解析好的路径同步进去，保持接口层状态尽量一致。

最后：

```cpp
return true;
```

表示整个初始化成功完成。

---

## 19. `initParas` 失败时通常会停在哪

你以后调试这段代码时，可以优先看 3 个失败点。

### 失败点 1：协议解码失败

日志：

```cpp
qWarning() << "initParas decode failed:" << error;
```

常见原因：

- 字段数量不是 36
- 数字字段里混进了非法文本
- 布尔字段写成了无法识别的值
- `config` 不是按 UTF-8 组织的行文本

### 失败点 2：模型路径不存在

日志：

```cpp
qWarning() << "initParas model path does not exist:" << resolved_model_path;
```

常见原因：

- 路径写错
- 相对路径基准目录和你想的不一样
- 模型文件根本没放到对应位置

### 失败点 3：ONNX Runtime 初始化失败

日志：

```cpp
qWarning() << "initParas runtime init failed:" << error;
```

常见原因：

- 模型文件损坏
- ONNX Runtime 或其依赖没配好
- 路径里有特殊问题
- 模型与当前运行库不兼容

---

## 20. 用最白话的话再讲一遍

你可以把 `initParas` 想成开机器前的准备动作：

```text
收到配置字节流
  ->
按 UTF-8 拆成 36 行
  ->
每一行填进 InitParasData
  ->
拿到模型路径
  ->
确认模型文件存在
  ->
创建 ONNX Runtime Session
  ->
记住输入输出名字
  ->
把状态标记成“已初始化”
```

如果这条链任何一步失败，`initParas` 都会直接返回 `false`。

---

## 21. 你现在读源码时应该重点看什么

如果你准备亲自打开源码读，建议你只抓住下面这 6 个点：

1. `config` 不是神秘二进制，而是按行的 UTF-8 文本
2. `decode_init_paras` 的核心是“固定顺序字段解析”
3. `InitParasData` 是配置的结构化结果
4. `QFileInfo` 负责检查模型文件是否存在
5. `SegRuntime::init` 负责真正加载 ONNX 模型
6. `has_init = true` 表示模块已经“开机成功”

只要这 6 个点你理解了，`initParas` 这段代码就已经算读懂大半了。

---

## 22. 下一步最适合继续读哪一段

如果你想顺着流程继续学，推荐下一份我给你讲：

`reciveData(images, data)` 逐行带读

因为下一步就会进入：

- 输入字段解码
- 16 位图检查
- 窗宽窗位映射
- CLAHE 增强

这部分会比 `initParas` 更接近“图像处理实际在做什么”。
