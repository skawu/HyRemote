# 命名规范

> 语言 / Language：**中文** ｜ [English](../en/internal/naming-conventions.md)

本文记录 HyRemote **已经**使用的命名约定，目的是让贡献者有明确依据，不必从目录结构反推。本文不引入重命名；范围边界见文末“V1 不做什么”，公开 API 冻结面见 [`v1-api-stability.md`](../v1-api-stability.md)。

新对象放在哪个目录、未来扩展点如何组织，见 [`repository-layout.md`](repository-layout.md)。

## 产品身份与四种拼写

`HyRemote` 是产品身份和公开命名空间。它周围有四种刻意保留的拼写，每种**只**在对应语境中正确：

| 拼写 | 适用范围 |
| --- | --- |
| `HyRemote` | 产品名、`HyRemote::` C++ 命名空间别名、安装头目录 `HyRemote/`、QML 模块与 URI `HyRemote`、产物名 `HyRemoteRemoteAccess` |
| `hyremote` | C++ 命名空间 `hyremote`、内部源文件名，以及不会离开库的小写标识符 |
| `HYREMOTE_` | CMake 选项和预处理宏 |
| `hyremote-` | CMake target、库和测试 target 名 |
| `qhyremote` | Transparent QPA 平台模块，遵循 Qt 自己的平台插件命名约定 |

新增的全局 C 风格符号或宏使用 `HYREMOTE_` 前缀。`HyRemote::` 内的公开 C++ 类型保持正常 PascalCase，不再额外加前缀，避免出现 `HYRemoteAccess` 这类重复。内部类型留在自己的命名空间或类作用域内。

## C++

- 命名空间：实现使用 `hyremote`，公开别名命名空间使用 `HyRemote::`。
- 类型、类和结构体：`PascalCase`（`RemoteAccess`、`RemoteFrame`、`SessionStats`）。
- 枚举：`enum class`，枚举值使用 `PascalCase`。
- 函数和方法：`camelCase`。
- 私有数据成员：`m_` 加 camelCase；使用 pimpl 的实现通常把数据放在 `struct Impl` 中，因此公开头文件很少出现成员命名。
- 公开头文件：文件名可以跟随它声明的公开类型，例如 `src/cpp/include/HyRemote/RemoteAccess.h`。这是唯一使用 `.h`/PascalCase 文件名的地方。
- 新增内部源文件和测试文件：`snake_case`。

### 两种公开头文件形式是刻意保留的

两种形式同时存在，不是漂移：

- `src/core/include/hyremote/core/*.hpp`：小写目录和扩展名，对应内部 STATIC target 的内部命名空间，不是安装型应用 SDK；
- `src/cpp/include/HyRemote/RemoteAccess.h`：PascalCase 目录和 `.h`，对应安装后的公开头目录名和公开类型。

新增头文件跟随所在模块已有的形式。

## CMake

- 选项和预处理宏：`HYREMOTE_`（`HYREMOTE_BUILD_TESTS`、`HYREMOTE_WITH_QPA_PROXY`）。
- target、库和测试 target：`hyremote-`（`hyremote-core`、`hyremote-remoteaccess`、`hyremote-qpa-native-semantics`）。
- 导出包和别名 target：`HyRemote::<Component>`。
- 平台模块沿用 Qt 约定：`qhyremote`。

## QML

- 模块和 URI：`HyRemote`，使用 `import HyRemote`。
- 模块目录必须等于 URI，因此 payload 位于 `src/qml`，不是扁平目录；这是 QML 的要求，不是风格选择。
- 元素使用 `QML_NAMED_ELEMENT(RemoteAccess)`，没有前缀，因为 QML import 形成单一全局命名空间。这是命名面唯一真实的冲突风险，**1.x 接受该风险**；改变它会破坏公开 API。

## V1 不做什么

- 不为了统一拼写而批量重命名，也不为了风格重命名已有 V1 文件。
- 不重命名 `src/cpp`、其 target、`HyRemote::RemoteAccess`、其 `EXPORT_NAME` 或安装包契约；布局、构建和包预期已经冻结（见 `repository-layout.md` 和 `v1-api-stability.md`）。
- 不为 QML 元素增加前缀或别名，也不对 V1.1 做预先承诺。
- 不因为语言或注释改动而修改标识符、日志 token 或 CMake target 名。
