# HyRemote 跨模块功能 RFC：水印、语言、持久化路径与 Docker

- 2026-10-22；Product Owner 已明确**要求支持这三类能力**。本文定义约束和拆分，不声称已构建。
- 主体：Shared Runtime/Host 负责水印及宿主配置；HyRemoteTool 负责自身界面语言、连接工作台和本地非秘密设置；用户 Qt 应用负责进程生命周期/部署选择；未来独立 Management Plane 仍未批准。
- 关联：[首批文字水印 #589](https://github.com/skawu/HyRemote/issues/589)、[后续 Logo #593](https://github.com/skawu/HyRemote/issues/593)、[Tool 语言 #590](https://github.com/skawu/HyRemote/issues/590)、[路径策略 #591](https://github.com/skawu/HyRemote/issues/591)、[Docker #592](https://github.com/skawu/HyRemote/issues/592)；产品方向 [#579](https://github.com/skawu/HyRemote/issues/579) / Tool PRD，Windows-first #563，PR CI #573。
- 不自行占用新产品版本，也不追改 V0.3.0.0 历史；遵守四条接入方式共享一个 Runtime，旧产品行为兼容，实际未验证能力标未完成。

## 一、先认清已有机制

当前宿主 Runtime 已有 setSecurityConfigFile(path) 与 SecurityProfile；条件式 VNC 认证读取安全 descriptor，再读取单独 passwordFile；加密传输（AuthenticatedEncrypted）不可用时会 fail-closed。**这不是“HyRemote 有统一数据库”和“HyRemoteTool 可管理密码”的证据。** 当前 HyRemoteTool 是独立 RFB 客户端，它的语言启动默认中文、拥有中英切换控件和 --language 参数；没有完整的 OS 语言自动模式与持久化用户设置。当前仓库没有已确认的通用数据库路径契约，也无已交付的“业务 Qt 应用 + HyRemote” Docker 模板。

目标架构：

~~~text
用户的 Qt 应用（Windows 进程或 Docker 容器）
  │  明确选择宿主配置文件路径 / 可写数据目录 / 可选数据库文件 / 凭据引用
  │
  └─ C++ / QML / Generic / QPA（任选一条）
                   ↓
        HyRemote Shared Runtime / Core
          ├─ PolicyResolver ── non-secret policy (read-only 可挂载)
          ├─ RuntimeStorage ── writable state / optional DB (volume)
          ├─ SecretRef      ── security descriptor / passwordFile (secret mount)
          └─ WatermarkPolicy + FrameCompositor ── RFB transport (Raw/TRLE/damage)
                                              ↓
                                     任意标准 RFB Viewer
                                              ↓
                                 HyRemoteTool（独立本机 UI）
                                   local settings: language, saved endpoints
~~~

**禁止**把 Tool 的本地连接列表设置路径当成远程宿主数据库路径，也禁止让普通 RFB 客户端直接编辑容器里的水印/密码/持久化配置。远程修改宿主参数是另一个带认证授权的管理面产品决策。

## 二、水印：首批文字，后续 Logo（Shared Runtime 出画面时生效）

> **已获 Product Owner 明确确认：水印首阶段只交付「文字水印」，Logo/图片水印延期并独立由 #593 跟踪。** #589 的功能验收与 CI 不得纳入 Logo 文件解码、Logo 选择控件或图像素材。不得因此创建新产品版本号。

### 2.1 用户能力

- **文字水印（#589 首批必交）**：明确开启/关闭（兼容老应用默认未启用）；提供用户设定的静态 UTF-8 文字（包含中文/英文）、受限长度、字号/字体、颜色、透明度、位置或平铺配置。开启但文字为空或不合法须有可执行的配置错误，不能默默降级。具体默认显示位置/透明度数值尚未裁定，不以代码自动替 Owner 决定。
- **Logo 图片水印（#593 后期）**：不属于 #589 首批；需后续单独设计文件格式/透明度/尺寸/资源与容器挂载。动态时间、每 Viewer 个性化文字及身份模板亦未获首批授权，不能以 IP/ServerName 伪造认证用户。
- 服务端在**画面进入传输编码前**合成远程像素；本机 Qt 业务界面内容保持不变，不修改业务应用画布。用 RealVNC/TightVNC/HyRemoteTool 连接看到一致的公共水印效果，而不是只有 Tool 的浮层。
- 在真实已认证客户端身份可以使用前，不填“操作者账号”；IP/ServerName 也不是可靠已认证主体。模板不允许泄露密钥/数据库路径。
- 水印的可见性是标记手段，**不是 DRM**，不保证截图、摄影或重新编码后依然可以唯一归责。

### 2.2 内部不变量与风险

~~~text
Capture source frame (immutable)
    -> Shared frame geometry/damage
    -> watermark composition on outgoing frame copy
    -> add watermark change rectangles to damage
    -> Raw/TRLE/incremental RFB per-client updates
~~~

- 不破坏已有 C++/QML/Generic/QPA 单共享 Runtime 结构；不要四路各自重复涂水印。
- 水印文本/策略更新（仅当实际交付可重载配置时）要产生真实可发送脏矩形；Raw/TRLE/部分刷新和全帧行为可回归，不能只在第一帧有水印、滚动后一部分消失，也不能造成 idle 画面永远重绘。**首批不要求随时间动态改文字**。
- 全屏/局部 frame、窗口 resize、高 DPI、颜色格式和鼠标光标不改变输入映射。多个 Viewer 一致；如将来支持**每会话个性化文字**，必须采用 per-session outgoing frame，不能污染共享捕获缓存。
- 开关/策略不合法不能静默改为另一个水印或渲染未限制的多兆文本；明确报错和限制资源。密码不写在水印或日志里。
- 验收：像素级 compose、压缩/增量刷新、damage、策略变更、关水印零变更、典型分辨率 CPU/帧年龄的测量；真实 Windows Qt6.8.3 外部 Viewer 场景，PR CI/独立审查全部通过；Linux/嵌入式非硬阻塞。

### 2.3 水印产品决策登记

- **D-W1 已批准：文字水印先交付 #589，Logo 图片水印明确后置 #593**。#593 不阻挡文字合成、路径策略或标准 Viewer 的 #589 验收。
- **D-W2 未裁定：默认角落还是全屏平铺**。首阶段设计应可表达明确选用的放置模式；最终用户缺省样式待 Owner 确认，不自行写死。
- **D-W3：首阶段限定用户明确提供的静态文字**。动态日期/时间或已认证身份水印是后期另行决策；没有可信用户身份来源时严禁伪造身份模板。

## 三、HyRemoteTool 语言：本机系统自动 + 设置手动优先

**“宿主机环境”按运行 HyRemoteTool 的这台机器的操作系统 Locale 定义；不读取远端用户应用的 locale 作为自己的语言。** 正在连接到外文机器时，Tool 不应突然改变中文/英文界面。

### 3.1 产品优先级（无重复选择）

1. 仅本次启动明确输入 --language=zh 或 --language=en：本次启动有效、优先级最高，不默默覆盖持久化设置。
2. 用户设置中保存的 language = auto / zh / en：手动覆盖生效并跨重启；选择 auto 重新解析系统。
3. 首次启动（无已保存设置）：默认为 auto，按 QLocale::system() 选择已翻译语言。zh 类映射中文，其他 Locale 暂回退英文；无已翻译资源则回退英文，不弹窗、不下载。
4. 系统 Locale 的外部变化至少在下次启动时生效；是否实时监听 OS 语言变更需有可测试事件才承诺。

### 3.2 设置页

~~~text
设置 / Settings
  语言 Language
    (•) 跟随系统 / Follow system (Auto)    [当前预览：简体中文 / English]
    ( ) 简体中文
    ( ) English
~~~

- 语言切换应实时作用于连接页、会话页、诊断、错误帮助、所有 tooltips/dialogs；不能只改变两个按钮。用单一 I18nSettings/TranslationService 管理，不让每个界面有独立 language 状态。
- CLI --verify-ui 等机器可解析日志、退出码与字段稳定，不应因 UI 翻译改变受测 token。
- 设置持久化到 **Tool 本机自己的非秘密设置目录**，与用户应用宿主 Docker volume 隔离；配置损坏 fail-safe 回 Auto，不重置端点或连接权限。
- 测试 zh_CN/en_US/fr_FR、auto/manual/CLI 优先级、重启、翻译完整性、Windows Qt5 限定兼容场景及 Qt6.8.3 标准场景，800×600、High-DPI/实时连接不跳转。

**所属交付：首阶段 S1 界面 #581 + 集成验收 #586，独立实现 #590。** 不增加 SDK S3 依赖。

## 四、可选择的宿主配置与数据库路径（先建契约）

### 4.1 分离存放位置

| 逻辑类别 | 例子 | 读写、安全、现状 |
| --- | --- | --- |
| **config_file（必需时选择）** | 监听参数、是否启用水印、样式策略 | 非秘密配置文件；可通过只读挂载，采用版本化 schema；目前尚无已发布统一文件 |
| **state_dir（持久化启用时选择）** | 将来状态元数据、活动状态持久记录 | 可写卷目录；路径由用户选择，运行时按容器 UID 校验，不能默默切换到 /tmp |
| **database_file（数据库功能激活后）** | 未来本地数据库文件及迁移 | 独立可选择文件或存储目录，**当前没有强制数据库，也不新增空数据库**；先定义锁、迁移、备份、崩溃恢复 |
| **secret_ref / existing securityConfigFile** | 现有 descriptor 指向 secret passwordFile；未来证书/密钥 | 文件引用/安全文件挂载，避免写明文密码进普通配置、环境变量、DB 诊断、镜像层。现有 VNC Auth 有条件支持，但仍不加密画面 |

同一个业务 Qt 进程的各接入方式由 Shared Runtime 解析**同一组生效参数**。Tool 的应用设置（语言、常用设备）独立，不推导远端宿主真实路径。

### 4.2 建议的解析优先级（**新 API 提案，当前不能执行**）

1. 应用显式传入配置路径/指定状态目录（对于 QPA/Generic 则是公开且可测的启动选项）。
2. 经过文档约束的 HYREMOTE_* 专用环境变量（仅放路径引用，绝不传明文密码）。
3. 显式选择的配置文件内部的路径声明（相对路径只以**配置文件所在目录**为锚点，禁止依赖启动 CWD）。
4. 桌面应用未指定时可用 Qt 标准用户数据路径作为安全缺省；**Docker 的持久化承诺必须要求显式选定并挂载可写 state_dir/database_file**。不能默默选择容器临时目录然后宣称重启可恢复。

路径校验：绝对/相对规范化、UTF-8/空格、软连接逃逸、已存在但不可读、只读 volume、权限错误、容器非 root 用户、多进程共享/SQLite file lock、密钥引用跨 mount 保护。**显式配置错误时只对依赖该路径的功能失败关闭**，不能一边报错一边换用另一个无声创建的位置。仅需要看屏幕、不使用 DB 时不能被“数据库不存在”阻塞。

名字示意（如 HYREMOTE_CONFIG_FILE、HYREMOTE_STATE_DIR、HYREMOTE_DB_FILE）仅是 RFC **候选**：在实现 PR 通过之前，不应把这些名称放到用户指南当作已经支持的环境变量。

### 4.3 关于未来密码

现有 Runtime 条件式 SecurityProfile::Authenticated 使用独立 security descriptor + passwordFile，且传统 VNC 验证存在密码长度等限制，**流量仍未加密**；SecurityProfile::AuthenticatedEncrypted 尚未交付。后续真正的身份/密码管理（轮换、权限、审计）需要独立安全评审；建议利用只读 secrets mount/受保护文件引用，而非把密码塞进 config.json 或数据库明文。当前路径配置可先稳定，不以未来完整认证功能为必需依赖。

## 五、用户 Qt 应用 Docker 联合部署（不假装运行了实例）

目标是**业务容器**，不是 HyRemote 单独起一个空服务：用户 Qt 应用使用 HyRemote 的一个公开集成路径/Runtime，发布完整 Qt runtime/plugin 依赖，然后业务应用进入自己的 Qt 事件循环，访问面由 HyRemote 监听。

示意目录（**路径约定仅做设计，尚非可运行 Docker Compose**）：

~~~text
host-project/
  docker-compose.yml
  app-image/Dockerfile              # 用户自己的 Qt App + HyRemote SDK/runtime
  host-config/hyremote.yaml         # 非秘密，ro
  runtime-state/                    # 用户选择挂载，可写
  protected-secrets/                # 受保护的文件，ro
~~~

候选 volume 对应容器内路径：
- 选中的只读配置 -> /etc/myapp/hyremote.yaml
- 明确指定的可写 state_dir/可选数据库 -> /var/lib/myapp/hyremote/
- 受保护密码/certificate 引用 -> /run/secrets/...
- 端口映射（现有 RFB 默认 5921）必须符合真实有效监听地址；提醒默认的未认证/明文模式**仅可信网络**，不可直接开放公网。

要实测/说明：Qt GUI 图形运行需求（X11/xcb/Wayland/offscreen/eglfs 和容器显示/GPU 约束随用户应用实际路径而不同）、rootless UID 权限、read-only root filesystem、volume 创建与错误、不同 bind-mounted host 路径、docker restart/recreate 后水印策略与必要状态仍保留、容器 stdout 不暴露秘密、SIGTERM 正确停机。所有 Linux Docker 测试在 Windows-first #563 契约下作为 **用户目标/可选诊断证据**，未跑时写 NOT_RUN，绝不因 Linux 环境缺失暂停 Windows 新 Feature/RC/GA。

**不要发布无法运行的 compose 示例作为官方部署指导。** #591 先定义并实现公开参数 API 和路径合同，再由 #592 提供经实际应用容器验证的 Compose/Dockerfile。

## 六、落地任务与顺序

1. **S1 语言 #590**：并入 #581 AppShell 设置/双语工作，#586 Windows 验收要覆盖 Follow system/手动覆盖/持久化；不等密码、水印、Docker 才能完成。
2. **宿主配置底座 #591**：独立 shared Runtime、配置文件/数据目录/可选数据库/secret 引用。先 RFC、后 public APIs/Generic/QPA 统一入口和兼容/测试。
3. **文字水印 #589**：按文本像素合成、Raw/TRLE/damage 与跨 Viewer 回归推进；真实可持久的 policy 集成依赖 #591 配置路径契约，不耦合 Tool 的 Viewer 界面。**Logo 水印 #593** 排在文字水印真实验收后，独立开发，不阻塞 #589。
4. **Docker #592**：以 #591 可用为前提，提供经过真实 Qt 应用验证的镜像/compose/持久化与安全文档；#589 水印配置已稳定后增加包含水印的容器回归。
5. **未来密码与加密**：需求与安全契约可并行评估，但不能以现有 VNC Auth 等价 TLS 或授权的正式完成状态；远端管理 API 仍独立立项。

**版本、门禁、状态：** 无新增版本号、未自动修改 V0.3.3 机器 mandatory children；每一项均以单独 Ready PR、实际 Windows CI 和独立代码审查为合并条件。Tool S1 Windows 真实 Viewer 是独立的验收成果。水印和 Docker 路径的用户目标环境没有实测时必须保留缺失证据，不虚报通过。已公开 V0.3.0.0 事实不因本 RFC 改变。

## 七、待 User/Owner 最终选择的范围

1. **已决定，不再询问**：首阶段仅文字水印 #589；Logo 图像水印后期 #593。
2. **仍待确定**：默认 **全屏平铺**还是**某个角落**？首批静态文字已足够，日期时间/用户身份模板作为后期扩展（身份必须来自真实认证主体）。
3. 宿主配置是给所有四种集成方式提供统一 public config_file 入口，还是优先支持当前用户使用的某一种接入方式作为第一实现/验证路径？建议统一契约、按接口分步验证。
4. 当前用户业务应用 Docker 的显示方式（桌面 X11/Wayland、headless/offscreen 或 EGLFS）取决于真实场景，不能预先声称某个 compose 在所有模式通用。

以上不阻挡架构文档和独立测试设计，但会影响水印 UI/文本模板和实际容器示例的最终验收范围。
