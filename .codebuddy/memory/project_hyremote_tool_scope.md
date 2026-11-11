---
name: HyRemoteTool 重新定义为远端连接/调试客户端
description: 2026-10-19 Owner 要求 HyRemoteTool 必须随 V0.3.0.0 发布，定位为业务之外的远端连接与调试客户端（输入远端 IP/端口，连接后调试、监测、后续做私有 Viewer）
type: project
---

**2026-10-19 最终定位（覆盖此前的宿主侧单页控制台方案）：HyRemoteTool 是业务之外的独立远端连接与调试客户端，必须随 V0.3.0.0 发布。**

- 工具不再分享自己的窗口，也不嵌入业务示例控件；它是 operator 侧的 companion，明确输入**远端 IP + 端口**连接到一个已启用 HyRemote 的应用。
- 连接成功后的调试能力：打开/关闭远程访问（会话连接/断开）、打开/关闭远程控制（是否发送输入）、帧率监测、速率/吞吐监测、设置访问密码（连接凭据）等。
- 后续演进：作为私有 Viewer 直接访问远程应用。
- 不再保留：假业务 workspace（Asset/Process load/Notes）、宿主 build/deployment identity、宿主 Listener 配置、活动日志堆叠。这些让工具"目的不明确"，已被 Owner 明确否定。

**架构含义：**

- Tool 从 `HyRemote::RemoteAccess`（服务端 facade）转为 **RFB 客户端**：3.8 握手、Raw/DesktopSize、帧缓冲渲染、Pointer/Key 注入、FPS/码率统计。仓库已有 `src/integrations/qpa/tests/rfb_test_client.hpp` 可作为无头客户端基础。
- 标准 RFB 只能做"连接/看画面/发输入"；**远程启停主机监听器、修改主机密码、主机侧策略**需要新增带认证的管理通道（新架构），不是 RFB 标准能力。V0.3.0.0 是否包含这些，仍需 Owner 明确；当前倾向先做客户端侧能力。
- 密码若指连接用的 VNC auth 口令，需要客户端 DES（服务端已有 OpenSSL `computeVncAuthResponse`，但它是私有 detail，不应直接复用）；若指修改主机密码，则必须有管理通道。

**发布状态（重要）：**

- 当前 `release/v0.3.0.0@9d072347` 已版本化并通过 L2，但它是**旧宿主侧 Tool**。新定义要求换掉 Tool，因此必须产生新候选并让 release 分支重新指向新树；由于该分支受保护且已版本化，**最干净的做法是删除并重建 release/v0.3.0.0（当前无 tag、无 GitHub Release、无 main 合并）**，需要 Owner 对受保护分支的破坏性操作明确同意。备用方案是一次性 post-version correction 门禁，但会削弱刚建立的两阶段保护。
- `release→main` 仅为 Draft #476；未打 tag、未发布。

**Why:** Owner 看到宿主侧 Tool 后明确批评"一塌糊涂且目的不明确"，并反感为验收反复弹出窗口；随后给出上述客户端定位。此前"单页紧凑重布局"只是布局调整，没有解决产品目的问题。

**How to apply:** 新 Tool 只做"输入远端 IP/端口 → 连接 → 画面/调试/监测"这一条主线；不做宿主业务示例、不做宿主配置台。任何可见 GUI 启动先征得同意，优先无头/CLI 验证。发布前必须重新冻结候选、重跑同 SHA L2、做安装版实机验收。
