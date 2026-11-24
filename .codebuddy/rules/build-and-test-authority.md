---
description: HyRemote 的 canonical 构建/测试入口与单一配置权威。在改动 CMake、build.cmd/build.yml、CI 工作流或准备编译、跑测试、清理构建树时加载。
alwaysApply: false
enabled: true
paths:
  - "build.cmd"
  - "build.yml"
  - "**/CMakeLists.txt"
  - "**/*.cmake"
  - ".github/workflows/**"
---

# Build and test authority

Root `AGENTS.md` is the single execution entry and wins over this rule. This
rule is a focused reminder, not a second authority.

- One canonical command: from the configured repository workspace root, run
  `build.cmd test` on Windows (`sh ./build.cmd test` on POSIX) for the complete
  suite. Do not invent a PowerShell chain with `--qt-prefix`, `--build-dir`,
  manual `PATH`, `cd`, `cmake --build`, or a standalone `ctest` as all-suite
  signoff.
- One configuration authority: `build.yml` (or a repository-authorised named
  profile) plus the Kit resolver own Qt version/compiler and the build
  directory. Do not guess absolute install paths or write a speculative local
  override. When a profile is missing or ambiguous, report the missing
  configuration instead of improvising.
- One build tree: only the configured workspace `build` directory. No parallel
  build directories, no `clean/rebuild` against unverified paths, no worktrees.
- Build-system options are permitted only through the `build.cmd` interface;
  they are not permission to reconstruct the environment per invocation.
- Evidence: trust the canonical build's own exit code, `test.log`, and CI. Text
  filters read logs; they do not decide PASS. A direct CTest call is a bounded,
  explicitly non-authoritative debugging exception only.
