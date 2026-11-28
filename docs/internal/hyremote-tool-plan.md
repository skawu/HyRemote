# HyRemoteTool 历史访问兼容记录

 > **当前范围以 H-T01=B 为准。** HyRemoteTool 当前是 SDK 开发与诊断工作台；本文件只记录既有发布兼容事实，不再定义新的远程访问客户端功能。

## 已发布事实

V0.3.0.0 曾随 HyRemote 发布基础 RFB 访问兼容入口。该入口是历史发行内容，不因后续开发工作台调整而删除已发布兼容性。

## 当前维护边界

- 当前开发工作聚焦 SDK/Qt Kit 静态核对、四种接入方式的准确指导、只读协议/帧诊断、脱敏报告和本机语言。
- 既有访问入口仅按已发布兼容、安全和部署要求维护，不扩展访问功能。
- 本仓库不在该工具中另建第二套 Runtime、Session、采集、传输或输入实现。

## 权威依据

- 当前产品范围：[HyRemoteTool SDK 开发与诊断工作台 PRD](hyremote-tool-workbench-prd-2026-10-22.md)。
- 当前版本与验收：[产品版本路线图](product-version-delivery-roadmap-2026-10-23.md)及 `.github/release/release-trains.json`。
- H-T01=B 正式裁决：[Issue #594](https://github.com/skawu/HyRemote/issues/594)。
