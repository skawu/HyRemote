# Real-world integration studies

The HyRemote-owned examples in `examples/learning/` are deliberately small. These studies show what integrating
HyRemote into **large, real, maintained open-source Qt applications** actually involves: which CMake file is touched,
which target is linked or deployed, where the remote-access object is created, how the runnable tree is produced and how
a viewer connects.

**Upstream sources are never vendored here.** Each study pins an exact upstream revision, records it, and ships only the
documented patch plus the reasoning. Clone the upstream project yourself, apply the patch, and read the study's README.

| Study | Upstream | Stars at selection | UI family | Route | Patch applies |
| --- | --- | --- | --- | --- | --- |
| [qbittorrent/](qbittorrent) | `qbittorrent/qBittorrent` | 40,266 | Qt Widgets / C++ | C++ API | verified |
| [qgroundcontrol/](qgroundcontrol) | `mavlink/qgroundcontrol` | 4,974 | Qt Quick / QML | QML API + C++ API | verified |

Star counts were read from the GitHub API at the moment each project was selected (2026-09-23) and are recorded in each
study so they can be re-verified.

## What these studies are, and are not

- They are **integration reasoning**, not endorsements. Neither project endorses HyRemote, and nothing here changes
  third-party branding, licensing headers or project identity.
- They record the **real** state of the work, including what could not be completed on the machine used. A study that
  ends in "the upstream dependency set is not present here" says exactly that; it never claims a build that was not run.
- Only the patch and documentation are added to this repository. Third-party code stays in the upstream project under
  its own licence.

## Licences and attribution

| Upstream | Licence (as reported upstream) |
| --- | --- |
| qBittorrent | GPL-2.0-or-later (GitHub reports `NOASSERTION` because the tree mixes licence files: `COPYING`, `COPYING.GPLv2`, `COPYING.GPLv3`) |
| QGroundControl | Apache-2.0 |

Both are compatible with **studying** an integration; neither is copied into this repository. If you redistribute a
patched upstream build, that distribution stays under the upstream licence and its attribution requirements.

## Reproducing a study

```sh
git clone --depth 1 https://github.com/<upstream>.git
cd <upstream>
git apply /path/to/hyremote-integration.patch
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/opt/Qt/6.8.3/gcc_64;/path/to/hyremote-sdk"
cmake --build build
cmake --install build
```

The HyRemote side of every study is the same three facts, which is the point: **point CMake at the SDK, link or deploy
the route, then run and connect a viewer**. The full user-facing flow is
[`docs/guide/integrate-your-project.md`](../../docs/guide/integrate-your-project.md).
