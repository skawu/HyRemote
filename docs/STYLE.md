# HyRemote 文档写作规范

> 语言 / Language：**中文** ｜ [English](en/STYLE.md)

本文约束 `docs/` 下的产品文档、参考文档、ADR 和内部文档。目标是让读者先看到事实，不被模板化语气和抽象隐喻干扰。

## 1. 语言政策

- `docs/` 根目录使用中文；
- `docs/en/` 使用英文；
- 每篇用户可见文档应同时存在中文 root 版和英文 `en/` 版；
- 两种语言按同一套规则写作，不做机械直译；
- 代码、命令、API 名称、文件名保持原样。

内部文档可以先英文后补中文，但最终需要成对。

## 2. 句子规则

### 2.1 少用破折号

中文使用逗号、冒号、句号或括号。英文使用逗号、冒号、分号或从句。

```text
不写：The Runtime owns behavior — including capture, input and transport.
改写：The Runtime owns capture, input and transport behavior.
```

### 2.2 不用 `rather than` 堆砌对比

直接说做了什么，需要时再补“不做什么”。

```text
不写：The policy favors the newest frame rather than building a queue.
改写：The policy keeps the newest frame and drops stale intermediate frames.
```

### 2.3 删掉自证式设计语气

不要写 `designed to`、`intentionally`、`by design` 来解释设计意图。直接写行为。

```text
不写：The QML layer is intentionally thin.
改写：The QML layer forwards to the same Runtime and adds no second Session or transport.
```

### 2.4 用具体名词代替隐喻

避免 `facade`、`seam`、`personality`、`truthful` 这类词。使用“入口”“封装层”“内部扩展点”“产品变体”“实际状态”等具体表达。

```text
不写：The C++ frontend is a facade into the Runtime.
改写：C++ 前端是 Shared Runtime 的一个入口。
```

### 2.5 少用否定式定义

先正向说明能力，再说明边界。

```text
不写：There is no separate Widgets Runtime.
改写：所有 Widgets 目标共用同一个 Shared Runtime。
```

### 2.6 列表只放需要逐项对照的内容

能写成一段话的，不拆成八股列表。列表项应能独立回答一个问题，不堆同义句。

## 3. 结构规则

- 每节先给结论，再给依据；
- 一段只讲一个主题；
- 版本、平台、兼容性声明必须写条件和边界；
- 未实现的能力直接标“未实现”或“后续”，不要留模糊的 TODO 语气；
- 安全、性能、兼容性声明必须能被测试或证据支持；
- 示例优先给最小可运行片段，不写伪代码长文。

## 4. 允许的例外

- 代码块中的注释、命令输出和第三方引用保持原样；
- 需要精确对比两个方案时，可以使用 “instead of”“not” 等直接表达；
- 法律、许可证和安全否定声明可以使用明确的否定句；
- 破折号在表格、代码和引用中不按散文规则处理。

## 5. 检查方式

扫描器：

```text
python docs/check_ai_flavor.py
```

默认只输出报告，不阻塞。全库迁移完成后再切换为 `--strict` 并接入 CI。

报告关注：

- 根目录文档中的英文散文；
- `docs/en/` 中的中文；
- em dash 密度；
- `rather than`、`facade`、`seam`、`truthful`、`designed`、`intentionally` 等标记；
- 中英混排。

## 6. 审阅清单

- 标题是否直接说明主题；
- 第一段是否给出结论；
- 是否有自证式设计语气；
- 是否有无法落到行为的形容词；
- 是否有超过两处破折号；
- 中文 root 和英文 `en/` 是否都存在；
- 版本/平台/安全声明是否与当前 release authority 一致。
