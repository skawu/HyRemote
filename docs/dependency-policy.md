# 第三方依赖政策

> 语言 / Language：**中文** ｜ [English](en/dependency-policy.md)

HyRemote 使用 **Apache License 2.0**，面向商业和开源 Qt 产品复用，也包括未来嵌入式部署。

依赖必须具备技术理由、可维护、法律兼容，并且对普通应用接入不可见，除非产品能力确实需要它。

## 产品工具链规则

HyRemote 有一个共享 Runtime 和四个并列接入前端：

```text
C++ API ---------\
QML API ----------\
Generic Plugin ----> Shared Runtime -> Core
QPA --------------/
```

正常产品实现使用 C++17、CMake 和 Qt。

C++/QML/Generic/QPA 是接入前端，不是独立后端工具链。普通 Windows/Linux SDK 消费者不需要为了使用 HyRemote 安装 Rust、Cargo、Go、Node、Python 或其他后端专用工具链。

未来实现可以在内部使用额外语言/工具链，前提是 SDK 仍保持文档化应用 API、包、部署和平台契约。

## 依赖类别

HyRemote 区分三类依赖：

| 类别 | 含义 |
| --- | --- |
| **应用/runtime 依赖** | 已发布 HyRemote 能力在运行时需要 |
| **构建依赖** | 构建某个启用能力需要，但不暴露为应用 API |
| **开发/验证工具** | 用于构建、测试、基准、打包或认证 HyRemote，不属于安装产品契约 |

开发工具不会因为仓库使用它就成为 SDK/runtime 前提。

## 没有隐藏依赖下载

HyRemote 产品构建不会静默下载或补丁主要产品依赖。

具体规则：

- Qt 由选定的开发者/CI/工具链环境提供；
- 安全传输构建需要 OpenSSL 时，由环境或显式选择的 prefix 提供；
- 产品依赖不会通过隐藏的 `FetchContent`/`ExternalProject` 下载流被静默加入；
- 用于资格认证的第三方应用源码不作为 HyRemote SDK 的一部分安装。

这样保持离线、企业和嵌入式构建环境可预测。

## Qt

Core 保持与 Qt UI 技术无关。

共享 Runtime 和前端按以下规则使用 Qt：

- 普通产品接入优先使用公共 Qt API；
- Generic Plugin 保持公共 Qt 插件 API，并保留应用原生平台接入；
- QPA 是唯一允许因产品角色依赖 Qt private/QPA ABI 的前端；
- QPA 兼容性按精确 Qt 补丁/平台组合认证；
- 公共 API 兼容不表示 QPA private-ABI 兼容。

当前桌面参考：Qt **6.8.3**。

Qt 仍受其自身许可条款约束。分发者负责其重新分发的精确 Qt 二进制所附带的许可、通知、重链接/源码可用性和其他义务。

HyRemote 不改变这些义务。

## 传输安全与 OpenSSL

传输安全是 Runtime 能力，不是第五种接入模式。

构建能力由以下宏表示：

```text
HYREMOTE_WITH_TRANSPORT_SECURITY
```

请求安全传输时：

- 所需加密/TLS 依赖必须显式可用；
- 依赖不可用时配置失败关闭；
- HyRemote 不会把请求的安全构建静默降级为不安全构建；
- 加密实现类型保持私有 Runtime 细节，不作为下游 SDK 目标。

当前 V0.1 `AuthenticatedEncrypted` 不可用，会失败关闭。

> **TODO V0.2：** 完成加密传输/证书产品能力，并发布精确第三方 runtime/包义务。

## RFB/VNC 传输基线

当前 Windows/Linux 正确性基线是私有 Runtime 传输边界后的有界 C++ RFB 3.8 传输。

RFB 是实现和互操作基线，不是 HyRemote 永久公共身份。

四个前端共享同一传输无关 Runtime/Core 模型，未来接受的新传输可以替换或补充 RFB，不产生前端专用 Runtime 变体。

当前安全语义：

- `Insecure` 只适用于文档化可信/loopback 边界；
- `Authenticated` 可以使用 RFB VNC 认证，但仍未加密；
- 加密传输不可用时 `AuthenticatedEncrypted` 失败关闭。

## 增加新产品依赖

新依赖必须有文档化产品理由，覆盖：

1. 上游项目和规范源；
2. 选定 release/tag/commit 政策；
3. 许可证和归属义务；
4. 与 HyRemote 分发模型兼容；
5. 静态/动态链接影响；
6. 活跃度、维护和安全更新状态；
7. 必需或可选状态；
8. 支持平台/工具链；
9. 对 C++/QML/Generic/QPA 打包和部署的影响；
10. fallback/移除策略；
11. 离线/交叉构建影响；
12. 上游贡献是否优于维护 fork。

依赖不得仅因为使用 HyRemote 就静默强迫下游应用接受不兼容许可证或新的必需工具链。

## Fork 政策

变更具有普遍价值时，优先上游协作，不建立永久 HyRemote 专用 fork。

临时 pinned patch 或 fork 必须有清晰产品理由和删除/上游化计划，不能变成用户需要理解或维护的长期隐形基础设施。

## 产品分发规则

两条规则尤其重要：

### Qt 保持在 HyRemote 源码树之外

HyRemote 使用选定 Qt SDK，不在正常产品构建中获取、重建或补丁 Qt。

QPA 可以针对合格 Qt private 接口编译，但这不会把 Qt 变成 HyRemote vendored 源码，也不会扩大 QPA 兼容承诺。

### 第三方资格应用不是 HyRemote runtime payload

用于认证 HyRemote 的真实应用保持独立上游项目。HyRemote 记录精确上游 revision 和测试方法，但不把这些应用重新品牌化或作为自身 SDK 的一部分再分发，除非未来分发决策显式处理相关许可义务。

## 平台和硬件库

GBM、DRM、DMA-BUF 相关库、RKMPP、V4L2、VA-API、D3D 类 API 和类似平台/硬件依赖保持可选实现依赖，除非产品线显式要求。

它们不得：

- 进入通用 Core；
- 改变四个应用接入契约；
- 强迫 Generic/QPA 应用直接链接 SoC 专用库；
- 仅为了优化一个目标平台就成为普通桌面用户的前提。

> **TODO：** 按支持的 BSP/产品包发布精确嵌入式/平台依赖清单。嵌入式 Linux/ARM64/EGLFS 参考单元是 #343 下 V0.3 家族 GA 前强制兼容基线；声明 GA 单元之外的 BSP/SoC 扩展单独准入，不预分配版本号（#333）。

## 历史与未来候选

过去对其他 VNC/传输实现的实验是工程证据，不是当前应用依赖。

未来传输或低拷贝依赖只有在保持正常 C++/Qt SDK 和部署体验的同时提供可测量产品价值时才被接受。

依赖不会仅因为技术上有趣就被提升。

## 当前限制

当前产品线没有引入新的传输安全运行时依赖；加密传输仍未实现。

任何新依赖必须同时更新 `LICENSE`、`NOTICE.md` 和部署 payload 列表，并在同一个变更中完成。
