#!/usr/bin/env python3
"""Apply the SAME temporary diagnostic-only overlap probe to two pinned worktrees.

This script intentionally edits each checked-out build tree, not a product branch.
The generated changes are restricted to HYREMOTE_ENABLE_CPU_PROBE=ON and a
standalone benchmark target; the real scheduling mechanism is not toggled.
"""
import argparse
from pathlib import Path


def replace_once(path: Path, old: str, new: str):
    data = path.read_text(encoding="utf-8")
    if data.count(old) != 1:
        raise RuntimeError(f"{path}: expected exactly one diagnostic seam, got {data.count(old)}")
    path.write_text(data.replace(old, new), encoding="utf-8")


def install(root: Path):
    probe = root / "src/runtime/src/detail/perf_probe.hpp"
    quick = root / "src/runtime/src/quick/quick_target.cpp"
    tests = root / "src/runtime/tests/CMakeLists.txt"
    if not (probe.exists() and quick.exists() and tests.exists()
            and (root / "src/runtime/tests/test_quick_overlap_profile.cpp").exists()):
        raise RuntimeError(f"{root}: expected benchmark and Qt Quick sources missing")

    replace_once(probe,
        "        (void)processCpuNs(m_startCpuNs);\n",
        """        (void)processCpuNs(m_startCpuNs);
        // Capture the terminal stage counters for standalone capture tests that
        // intentionally do not create an AccessInstance reporter.
        std::atexit([] { Recorder::instance().snapshot(); });
""")
    replace_once(probe,
        "    void recordQuickBackend(QuickGraphicsBackend backend) noexcept\n",
        """    void quickGrabStarted() noexcept
    {
        if (!m_enabled) return;
        const auto now = m_activeQuickGrabs.fetch_add(1, std::memory_order_relaxed) + 1;
        auto previous = m_peakQuickGrabs.load(std::memory_order_relaxed);
        while (now > previous
               && !m_peakQuickGrabs.compare_exchange_weak(
                   previous, now, std::memory_order_relaxed)) {}
    }

    void quickGrabFinished() noexcept
    {
        if (m_enabled) m_activeQuickGrabs.fetch_sub(1, std::memory_order_relaxed);
    }

    void recordQuickBackend(QuickGraphicsBackend backend) noexcept
""")
    replace_once(probe,
        '        std::fputs("}}\\n", file);\n',
        """        std::fprintf(file,
                     "},\\\\\\"quick_grab_activity\\\\\\":{\\\\\\"active\\\\\\":%llu,"
                     "\\\\\\"peak\\\\\\":%llu}}\\\\n",
                     static_cast<unsigned long long>(
                         m_activeQuickGrabs.load(std::memory_order_relaxed)),
                     static_cast<unsigned long long>(
                         m_peakQuickGrabs.load(std::memory_order_relaxed)));
""".replace('\\\\\\"', '\\"').replace('\\\\n', '\\n'))
    replace_once(probe,
        "    std::mutex m_writeMutex;\n",
        """    std::atomic<std::uint64_t> m_activeQuickGrabs{0};
    std::atomic<std::uint64_t> m_peakQuickGrabs{0};
    std::mutex m_writeMutex;
""")
    replace_once(probe,
        "#define HYREMOTE_P1_QUICK_BACKEND(backend) \\\n",
        """#define HYREMOTE_P1_QUICK_GRAB_BEGIN() \\
    ::HyRemote::detail::p1perf::Recorder::instance().quickGrabStarted()
#define HYREMOTE_P1_QUICK_GRAB_END() \\
    ::HyRemote::detail::p1perf::Recorder::instance().quickGrabFinished()
#define HYREMOTE_P1_QUICK_BACKEND(backend) \\
""")
    replace_once(probe,
        "#define HYREMOTE_P1_QUICK_BACKEND(backend) ((void)0)\n",
        """#define HYREMOTE_P1_QUICK_GRAB_BEGIN() ((void)0)
#define HYREMOTE_P1_QUICK_GRAB_END() ((void)0)
#define HYREMOTE_P1_QUICK_BACKEND(backend) ((void)0)
""")
    replace_once(quick,
        "        grab->readyConnection = QObject::connect(\n",
        """        HYREMOTE_P1_QUICK_GRAB_BEGIN();
        grab->readyConnection = QObject::connect(
""")
    replace_once(quick,
        "                grab->finished = true;\n                QObject::disconnect(grab->readyConnection);\n",
        """                grab->finished = true;
                HYREMOTE_P1_QUICK_GRAB_END();
                QObject::disconnect(grab->readyConnection);
""")
    replace_once(quick,
        "            grab->finished = true;\n            QObject::disconnect(grab->readyConnection);\n",
        """            grab->finished = true;
            HYREMOTE_P1_QUICK_GRAB_END();
            QObject::disconnect(grab->readyConnection);
""")

    addition = """
# Research-only direct execution, never an automatic product CTest.
if(HYREMOTE_REMOTEACCESS_WITH_QUICK AND HYREMOTE_ENABLE_CPU_PROBE)
    add_executable(hyremote-quick-overlap-profile
        test_quick_overlap_profile.cpp
    )
    target_compile_features(hyremote-quick-overlap-profile PRIVATE cxx_std_17)
    target_include_directories(hyremote-quick-overlap-profile PRIVATE ../src)
    target_link_libraries(hyremote-quick-overlap-profile PRIVATE
        hyremote-remoteaccess HyRemote::Core Qt6::Core Qt6::Gui Qt6::Quick
    )
endif()
"""
    with tests.open("a", encoding="utf-8") as file:
        file.write(addition)
    print(f"DIAGNOSTIC_ONLY_PATCH_APPLIED={root}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    install(args.root.resolve())
