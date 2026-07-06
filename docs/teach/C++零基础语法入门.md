# `reference_qt_adapter` 用到的 C++/Qt 语法入门

这不是一本完整的 C++ 教材。

目标只有一个：

`让你能读懂 delivery/reference_qt_adapter/ 这套代码里真正出现过的语法。`

如果你把本文档和 [执行流程图解.md](/D:/code/OH_Detect_Station_Module/docs/teach/执行流程图解.md:1) 一起看，会更容易把“语法”和“程序在做什么”连起来。

---

## 1. 先记住一句最重要的话

你现在不需要先学会完整的 C++。

你只需要先学会这套代码里最常见的这些东西：

- `.h` 和 `.cpp`
- `#include`
- `struct`
- `class`
- `namespace`
- `const`
- `&` 引用
- `*` 指针
- `std::vector`
- `std::map`
- `std::unique_ptr`
- `QString`
- `QByteArray`
- `cv::Mat`

先把这些弄明白，就已经足够开始读这套参考适配器代码了。

---

## 2. `.h` 和 `.cpp` 是什么

### `.h`

头文件，主要放“声明”。

你可以把它理解成：

`先告诉编译器：我这里有哪些结构体、类、函数。`

例如 [protocol_codec.h](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.h:1) 里主要声明了：

- `InitParasData`
- `ReceiveDataData`
- `SendDataData`
- `decode_init_paras`
- `decode_receive_data`
- `encode_send_data`

### `.cpp`

实现文件，主要放“这些函数到底怎么做”。

例如 [protocol_codec.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.cpp:1) 里就真正写了：

- 如何把 `QByteArray` 拆成多行
- 如何把字符串转成 `bool/int/double`
- 如何把最终结果重新编码成 `QByteArray`

你可以先把它理解成：

- `.h` 是“目录”
- `.cpp` 是“正文”

---

## 3. `#include` 是什么

例如：

```cpp
#include "protocol_codec.h"
#include <QString>
#include <vector>
```

含义是：把别处已经定义好的内容引进来用。

你可以粗略类比成 Python 里的 `import`。

区别是：

- `#include "xxx.h"` 常用于你自己项目里的头文件
- `#include <xxx>` 常用于库文件或系统头文件

---

## 4. `namespace` 是什么

例如：

```cpp
namespace reference_qt_adapter {
```

它的作用是“给名字加一层归属范围”，避免重名。

比如：

```cpp
reference_qt_adapter::SegRuntime
```

表示这个 `SegRuntime` 是 `reference_qt_adapter` 这个命名空间里的。

你可以先把 `namespace` 理解成“分组标签”。

---

## 5. `struct` 是什么

例如在 [protocol_codec.h](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.h:8) 里：

```cpp
struct InitParasData {
    QString reserved;
    QString onnx_model_path;
    double pixel_to_physical_scale = 1.0;
};
```

这表示一个“打包好的数据结构”。

它里面放很多字段，方便把一组相关数据装在一起。

你可以把它先类比成 Python 的：

- `dataclass`
- 或者“有固定字段的对象”

在这套代码里，三个最重要的结构体是：

- `InitParasData`
  保存 `initParas` 解码后的参数
- `ReceiveDataData`
  保存 `reciveData` 解码后的单次输入信息
- `SendDataData`
  保存 `sendData` 最终要输出的结果字段

---

## 6. `class` 是什么

例如在 [seg_runtime.h](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/seg_runtime.h:13) 里：

```cpp
class SegRuntime {
public:
    bool init(...);
    bool run(...);
};
```

`class` 通常表示“对象”。

它不光有数据，还会有方法。

在这套代码里：

- `SegRuntime` 负责 ONNX Runtime 的推理
- `OH_Detect_Station_Module` 负责把协议层、图像层、推理层串起来

你可以先把 `class` 理解成：

`数据 + 可以操作这些数据的函数`

---

## 7. 成员变量和成员函数

看这个例子：

```cpp
class SegRuntime {
public:
    bool init(const std::wstring& model_path, QString* error = nullptr);
    bool run(const cv::Mat& input_bgr, cv::Mat& seg_map, cv::Mat& seg_map255, QString* error = nullptr);

private:
    std::unique_ptr<Ort::Session> session_;
};
```

这里有两类东西：

- `session_`
  这是成员变量
- `init`、`run`
  这是成员函数

你可以理解成：

- 成员变量是“这个对象随身带着的数据”
- 成员函数是“这个对象会做的动作”

---

## 8. `public` 和 `private`

例如：

```cpp
class SegRuntime {
public:
    bool init(...);

private:
    std::unique_ptr<Ort::Session> session_;
};
```

含义是：

- `public`
  外部可以直接调用
- `private`
  只允许类自己内部使用

所以外部可以调 `init()`，但不能直接碰 `session_`。

这样做的目的是防止外部把对象的内部状态改乱。

---

## 9. `const` 是什么

例如：

```cpp
bool run(const cv::Mat& input_bgr, cv::Mat& seg_map, QString* error = nullptr);
```

这里的 `const cv::Mat& input_bgr` 表示：

- 这是引用传参
- 而且这份输入图像不允许在函数里被修改

所以 `const` 的核心意思可以先记成：

`这是只读的，不应该改它。`

---

## 10. `&` 是什么

在这套代码里，`&` 大多数时候表示“引用”。

例如：

```cpp
bool decode_init_paras(const QByteArray& raw, InitParasData& result, QString* error = nullptr);
```

这里有两个典型用法：

- `const QByteArray& raw`
  只读引用，表示把原对象传进来，不复制
- `InitParasData& result`
  可写引用，表示函数会直接把解析结果写进这个对象

你可以先把引用理解成：

`给原对象起了一个别名，直接操作原对象本身。`

为什么常用引用？

- 避免复制大对象
- 函数里改了，外面也能看到结果

---

## 11. `*` 是什么

在这套代码里，`*` 主要有两种常见含义。

### 第一种：指针

例如：

```cpp
QString* error = nullptr
```

意思是：

- `error` 是一个指针
- 它指向一个 `QString`
- 默认值是 `nullptr`

`nullptr` 可以理解成“现在没有指向任何对象”。

所以这种写法的意思通常是：

- 如果调用者传了 `error`，函数就往里面写详细错误信息
- 如果调用者没传，函数就只返回成功或失败

### 第二种：解引用

例如：

```cpp
*error = "Input image is empty.";
```

表示把内容写到 `error` 指向的那个对象里。

---

## 12. `std::vector` 是什么

你可以把它理解成“可变长数组”。

例如：

```cpp
std::vector<cv::Mat> images
```

表示“装很多张 `cv::Mat` 图像的容器”。

再例如：

```cpp
std::vector<float> input_tensor
```

表示“装很多浮点数的数组”，这里用来准备 ONNX 输入张量。

你可以先把 `std::vector` 类比成 Python 的 `list`，只是类型更严格。

---

## 13. `std::map` 是什么

你可以把它理解成“键值对表”。

例如在 [OH_Detect_Station_Module.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/OH_Detect_Station_Module.cpp:32) 里有：

```cpp
std::map<const OH_Detect_Station_Module*, std::unique_ptr<AdapterState>>
```

这里的意思是：

- 键：某个 `OH_Detect_Station_Module` 对象的地址
- 值：这个对象对应的一份 `AdapterState`

为什么要这么做？

因为交付头文件没有给参考实现预留私有成员，所以作者把“实例状态”放到一个外部表里托管。

你可以粗略理解成：

`哪个模块对象，对应哪一份运行状态。`

---

## 14. `std::unique_ptr` 是什么

例如：

```cpp
std::unique_ptr<Ort::Session> session_;
```

你可以先把它理解成：

`一个自动管理生命周期的指针。`

含义是：

- 这份对象只归一个地方独占管理
- 不需要手动 `delete`
- 当 `unique_ptr` 自己销毁时，里面的对象也会自动释放

为什么这个很重要？

因为像 ONNX Runtime session 这种资源对象，如果手工管理，容易忘记释放。

---

## 15. `QString` 和 `QByteArray`

这两个是 Qt 里最常见的类型。

### `QString`

表示字符串。

例如：

```cpp
QString model_path;
```

### `QByteArray`

表示字节流。

例如：

```cpp
QByteArray config;
```

在这套代码里，`QByteArray` 很重要，因为交付接口里很多数据不是直接传结构体，而是传字节流。

当前参考实现采用的是：

`按行组织的 UTF-8 文本字节流`

所以 [protocol_codec.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.cpp:10) 里会先把 `QByteArray` 转成文本，再按行拆开。

---

## 16. `cv::Mat` 是什么

`cv::Mat` 是 OpenCV 里最核心的图像类型。

你可以把它理解成“图像对象”。

例如：

```cpp
cv::Mat image16;
cv::Mat seg_map;
cv::Mat seg_map255;
```

它们分别可能表示：

- 原始 16 位灰度图
- 类别分割图
- 255 二值掩码图

在这套代码里你会经常看到：

- `CV_16UC1`
  16 位无符号单通道图
- `CV_8UC1`
  8 位无符号单通道图
- `CV_8UC3`
  8 位无符号三通道图

你可以先死记这三个：

- `16U`：16 位
- `8U`：8 位
- `C1/C3`：1 通道 / 3 通道

---

## 17. 函数参数为什么这么写

例如：

```cpp
bool prepare_input_images(
    const cv::Mat& image16,
    double window_width,
    double window_level,
    PreprocessedImages& result,
    QString* error = nullptr);
```

这个函数签名可以这样拆：

- 返回值 `bool`
  表示成功或失败
- `const cv::Mat& image16`
  输入图像，只读
- `double window_width`
  一个普通数值参数
- `PreprocessedImages& result`
  输出结果会写到这里
- `QString* error = nullptr`
  如果失败，可以把错误信息写到这里

这就是 C++ 里非常常见的一种风格：

- 用返回值表示是否成功
- 用引用参数带回真正结果
- 用可选错误指针带回失败原因

---

## 18. `if`、`for`、`return`

这三个你在任何语言里都会遇到。

### `if`

```cpp
if (image16.empty()) {
    return false;
}
```

意思是：

如果图像是空的，就直接失败返回。

### `for`

```cpp
for (int y = 0; y < input_height; ++y) {
```

意思是：

从上到下，一行一行处理。

### `return`

```cpp
return true;
```

意思是：

函数到这里结束，并返回成功。

---

## 19. `::` 是什么

例如：

```cpp
cv::Mat
QString::fromUtf8
reference_qt_adapter::SegRuntime
```

这个符号读作“作用域运算符”。

你可以理解成：

`某个名字，属于谁。`

例如：

- `cv::Mat`
  `Mat` 属于 OpenCV 的 `cv`
- `QString::fromUtf8`
  `fromUtf8` 属于 `QString`

---

## 20. 看一个真实例子

下面这行出自 [protocol_codec.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.cpp:66) 一带的逻辑：

```cpp
return parse_with(parse_double_token, tokens, 3, result.pixel_to_physical_scale, error, "pixel_to_physical_scale");
```

第一次看很吓人，其实可以拆开：

- `parse_with(...)`
  调一个通用解析函数
- `parse_double_token`
  说明这次要按 `double` 来解析
- `tokens`
  原始字段列表
- `3`
  取第 4 个字段
- `result.pixel_to_physical_scale`
  解析成功后写到这个字段里
- `error`
  如果失败，把错误信息写进去
- `"pixel_to_physical_scale"`
  出错时提示字段名

拆开之后就不复杂了。

---

## 21. 你现在读代码时该怎么读

新手最容易犯的错，是一上来逐字符抠语法，结果越看越乱。

更稳的顺序是：

1. 先看这个文件的职责
2. 再看有哪些结构体和函数
3. 再看数据是从哪里进、到哪里出
4. 最后再看某一行语法细节

用在这个项目上，就是：

1. 先看 [执行流程图解.md](/D:/code/OH_Detect_Station_Module/docs/teach/执行流程图解.md:1)
2. 再看 `protocol_codec -> image_pipeline -> seg_runtime -> OH_Detect_Station_Module.cpp`
3. 不懂某个符号时，再回本文查

---

## 22. 先学到什么程度就够了

你现在不用追求“会写 C++”。

你先做到这 4 件事就够：

1. 看见 `struct`，知道它是在装数据
2. 看见 `class`，知道它是在组织功能
3. 看见 `const T&`，知道它是“只读引用”
4. 看见 `QString* error = nullptr`，知道它是“可选错误输出”

做到这一步，你已经可以开始读这套适配器源码了。

---

## 23. 下一步怎么学最有效

建议你按这个顺序继续：

1. 先读 [执行流程图解.md](/D:/code/OH_Detect_Station_Module/docs/teach/执行流程图解.md:1)
2. 然后打开 [protocol_codec.h](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.h:1) 和 [protocol_codec.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/protocol_codec.cpp:1)
3. 再看 [image_pipeline.h](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/image_pipeline.h:1) 和 [image_pipeline.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/image_pipeline.cpp:1)
4. 最后看 [seg_runtime.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/seg_runtime.cpp:1) 和 [OH_Detect_Station_Module.cpp](D:/code/OH_Detect_Station_Module/delivery/reference_qt_adapter/OH_Detect_Station_Module.cpp:1)

这样不会一开始就掉进最难的 ONNX 推理细节里。
