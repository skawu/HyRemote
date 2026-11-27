---
name: V0.3.0.0 已发布（2026-10-19）
description: V0.3.0.0 Product Preview 的最终 SHA、tag、L2、物理验收、制品与 backmerge 事实
type: project
---

**V0.3.0.0 Product Preview 已于 2026-10-19 发布。**

```text
release SHA   35bae71fcd0c5345f8165120c62db384b050dc80
main HEAD     eba04f6fb2e688f56ef95ba729b63d2c37e4094d
tag           v0.3.0.0 (annotated, points to eba04f6)
develop       5853aa9668100d1c64a6f6f7568422d0af2d2a6f (VERSION 0.0.0)
```

- L2 `release-readiness` PASS on `35bae71f`: run `39232134597`, Windows/Linux Qt 6.8.3 four routes.
- Physical acceptance on installed Windows client: connect/frame/FPS/throughput, keyboard (`APP_TEXT`), pointer (`APP_POINTER`/`APP_CLICKED`), reconnect (`CLIENT_COUNT 0→1`). Host was offscreen `hyremote-qml-basic --port 5931 --remote-input`; no font database so glyphs were boxes, inputs proven by host log. Window resize not separately recorded.
- GitHub Release: https://github.com/skawu/HyRemote/releases/tag/v0.3.0.0 with source zip/tar.gz, Windows zip, Linux tar.gz, SHA256SUMS.
- Backmerge PR #482 merged to develop as `5853aa96`; version sentinel restored.
- Tool baseline shipped as out-of-process access-plane client; management plane, VNC-auth passwords, clipboard, multi-monitor and TLS remain out of scope for V0.3.0.0.

**Next priority:** engineering-health milestone (test rationalization 111→~80, QPA 25→~10, examples-v2 task mapping, CI fast lane/wall-time reduction) before V0.3.x feature work. The release itself is done; do not reopen V0.3.0.0 scope.
