# 任务：在 Ubuntu 上用真实第三方查看器验证 HyRemote 的认证行为（只做这一件事）

## 目标

证明三件事，用**真实的 VNC 客户端**（`vncdotool`，CI 自己就在用它）：

1. 口令正确 → 能连上并取到一帧；
2. 口令错误 → 被拒绝；
3. 客户端**不带口令**（等价于请求 `SecurityType None`）→ 被拒绝，**绝不降级**。

只读验证：不 commit、不 push、不改仓库文件（下面的服务端程序放在仓库之外）。

## 1. 环境

```bash
lsb_release -a; uname -m            # 回传这两行
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build python3-pip python3-venv git libssl-dev \
  xvfb libxkbcommon-x11-0 libxkbcommon-dev libxkbcommon-x11-dev libgl1-mesa-dev
python3 -m pip install --user --disable-pip-version-check aqtinstall ninja 'vncdotool==1.3.0'
export QT_ROOT="$HOME/Qt"
python3 -m aqt install-qt linux desktop 6.8.3 linux_gcc_64 --outputdir "$QT_ROOT"
export PATH="$QT_ROOT/6.8.3/gcc_64/bin:$PATH"
```

## 2. 取代码（认证那条 PR）

```bash
cd ~ && git clone https://github.com/skawu/HyRemote.git hyremote-verify && cd hyremote-verify
git fetch origin pull/199/head:pr-199 && git checkout -q pr-199
git log --oneline -1                # 回传这一行
```

## 3. 起一个"要求认证"的最简服务端（公开 API，仓库外）

`~/auth-server/main.cpp`：

```cpp
#include <HyRemote/RemoteAccess.h>

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <iostream>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QStringLiteral("HyRemote auth server"));
    window.resize(320, 120);
    auto *layout = new QVBoxLayout(&window);
    layout->addWidget(new QLabel(QStringLiteral("Authenticated remote view target")));
    window.show();

    HyRemote::RemoteAccess remote(&window);
    remote.setSecurityProfile(HyRemote::RemoteSecurityProfile::Authenticated);
    remote.setSecurityConfigFile(QStringLiteral(HYREMOTE_AUTH_DESCRIPTOR));
    if (!remote.start()) {
        const auto error = remote.lastError();
        std::cerr << "START_FAILED "
                  << (error ? error->message.toStdString() : std::string("no diagnostic")) << std::endl;
        return 2;
    }
    std::cout << "READY " << remote.port() << std::endl;
    const int result = app.exec();
    remote.stop();
    return result;
}
```

`~/auth-server/CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.21)
project(HyRemoteAuthServer LANGUAGES CXX)

set(HYREMOTE_SOURCE_DIR "$ENV{HOME}/hyremote-verify" CACHE PATH "")
add_subdirectory("${HYREMOTE_SOURCE_DIR}" hyremote EXCLUDE_FROM_ALL)

add_executable(auth-server main.cpp)
target_compile_features(auth-server PRIVATE cxx_std_17)
target_compile_definitions(auth-server PRIVATE
    HYREMOTE_AUTH_DESCRIPTOR="${CMAKE_CURRENT_SOURCE_DIR}/auth.conf")
target_link_libraries(auth-server PRIVATE Qt6::Widgets HyRemote::RemoteAccess)
```

描述符与口令（**口令必须 ≤ 8 字节**；空口令或更长会被产品校验拒绝，这是 VNC 认证的既有边界）：

```bash
cd ~/auth-server
printf 's3cret!\n' > password.txt
printf 'version=1\ncredentialId=maintenance-console\npasswordFile=password.txt\n' > auth.conf
cat auth.conf

cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_ROOT/6.8.3/gcc_64" \
  -DHYREMOTE_WITH_TRANSPORT_SECURITY=ON -DHYREMOTE_BUILD_TESTS=OFF 2>&1 | tail -30
cmake --build build -j"$(nproc)" 2>&1 | tail -20
```

配置日志里应有一行 `HyRemote: authenticated/encrypted transport available from the system provider (OpenSSL <版本>)` —— 回传它。

## 4. 三次连接

```bash
cd ~/auth-server
xvfb-run -a ./build/auth-server > ~/v2-server.log 2>&1 &
sleep 3; cat ~/v2-server.log          # 期望含 READY <port>；把 <port> 代进下面三条

# (a) 口令正确：期望成功取到一帧
python3 -m vncdotool -s 127.0.0.1::<port> -p 's3cret!' capture ~/v2-ok.png ; echo "V2a exit=$? size=$(stat -c%s ~/v2-ok.png 2>/dev/null)"

# (b) 口令错误：期望被拒
python3 -m vncdotool -s 127.0.0.1::<port> -p 'wr0ng!!!' capture ~/v2-bad.png ; echo "V2b exit=$?"

# (c) 不带口令（= 请求 None）：期望被拒，不降级
python3 -m vncdotool -s 127.0.0.1::<port> capture ~/v2-none.png ; echo "V2c exit=$?"

kill %1 2>/dev/null
```

## 5. 回传格式（照抄填写，不要只回结论）

```
环境: <lsb_release 与 uname -m 原文>
pr-199 HEAD: <git log --oneline -1 原文>
provider 状态行: <整行原文>
READY 行: <原文>
V2a: exit=<码>  png 字节数=<数字>  关键 stderr=<原文>
V2b: exit=<码>  关键 stderr=<原文>
V2c: exit=<码>  关键 stderr=<原文>
若 V2a 失败: <~/v2-server.log 末尾 30 行原文>
结论: PASS / FAIL（FAIL 就给证据，不要先修）
```

## 6. 纪律

- **只读**：不 commit、不 push、不改仓库文件。
- **失败也是结果**：按格式回传原文即可，**不要**先用放宽条件、跳过步骤的方式把它变绿。
- 保留 `~/v2-server.log`、`~/v2-*.png` 与 `~/auth-server/build`，便于复核。
- 命令跑不起来（缺包、无权限等）也要回传原文 —— 那本身是有价值的结论。
