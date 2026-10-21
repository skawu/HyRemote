# Qt 6.12 LTS 兼容性预检（2026-10-21）

## 状态

```text
BASELINE_HEAD=c18d284
RESULT=NOT_RUN / DEFER
AUTHORITY=#343/#57（受支持矩阵未变更）
```

Qt 官方已在 2026-09-30 发布 Qt 6.12 LTS。本预检只判断是否值得进入后续兼容工作，**不把 Qt 6.12 加入当前 Supported 声明**，也不修改 `.github/release/release-trains.json`。

## 当前环境

```text
已安装 Qt: 5.15.2、6.8.3（msvc2022_64 / msvc2022_arm64）
Qt 6.12:    未安装
```

当前参考版本仍是 **Qt 6.8.3**。QPA proxy 明确依赖 6.8.3 的私有 QPA ABI，因此不能从公共 API 兼容性推断 QPA 可跨补丁使用。

## 预检矩阵

| 路线 | 结论 | 依据/缺口 |
| --- | --- | --- |
| C++ API + Widgets/Quick | **NOT_RUN** | 需要独立的 Qt 6.12 prefix；当前未安装 |
| QML API | **NOT_RUN** | 需要 Qt 6.12 QML/Quick 目标；未安装 |
| Generic Plugin | **NOT_RUN** | 需要 Qt 6.12 公共 Qt 目标；未安装 |
| QPA proxy | **BLOCKED** | `find_package(Qt6 6.8.3 EXACT)` 与私有 Gui ABI；不能推断 6.12 兼容 |
| Qt 5.15 | **不变** | 仍属于 #343/#57 的既有基线，不由本预检改变 |

## 后续可执行预检

在隔离 prefix 安装 Qt 6.12 后，按以下顺序执行，不提交支持声明：

```text
build.cmd clean
build.cmd build --tests --examples --qt-prefix=<qt-6.12-prefix>
ctest --output-on-failure
build.cmd build --mode=generic --tests --qt-prefix=<qt-6.12-prefix>
build.cmd build --mode=qml --tests --qt-prefix=<qt-6.12-prefix>
```

QPA 必须单独使用 6.8.3 exact prefix 验证；不能把 6.12 结果外推到 QPA。

## Owner 决策

当前默认：**DEFER**。只有 Qt 6.12 公共路线预检通过、资源/支持成本明确后，才由 Owner 决定 GO / DEFER / NO-GO；任何 GO 都需要单独更新 #343/#57 和机器权威。
