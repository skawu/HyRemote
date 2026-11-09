#!/usr/bin/env python3
"""Patch *application fixtures only* in pinned temporary Git worktrees.

This is NOT product source code and must never be applied to develop or a
release branch. Same test-only file bytes on old/new Runtime revisions.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

APP = Path("examples/learning/03-generic/quick-app")
OLD_RESIZE = "view.resize(460, 190);"
NEW_RESIZE = "view.resize(1920, 1080);"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def patch(tree: Path, fixture: Path, manifest: Path):
    tree = tree.resolve()
    app = tree / APP
    original_main = (app / "main.cpp").read_bytes()
    # Normalize newline differences only for the fixture edit. Source SHA is
    # separately fixed and verified by the caller before this operation.
    original_text = original_main.decode("utf-8")
    assert original_text.count(OLD_RESIZE) == 1, (
        "historical app main.cpp no longer has the expected 460x190 viewport"
    )
    # A C++ QTimer, not the Qt logging subsystem, produces an ASCII stage
    # line on stdout after updating the QML property. std::endl flushes it so
    # an independent Windows Viewer pixel observer can correlate UTC clocks.
    import_line = "#include <QGuiApplication>"
    assert original_text.count(import_line) == 1
    altered = original_text.replace(
        import_line,
        import_line + "\n#include <QDateTime>\n#include <QQuickItem>\n#include <QQuickWindow>\n#include <QJsonArray>\n#include <QJsonDocument>\n#include <QJsonObject>\n#include <QMetaObject>\n#include <QMetaMethod>\n#include <QMetaProperty>",
    )
    show_line = "    view.show();"
    assert altered.count(show_line) == 1
    stage_driver = r'''
    // Diagnostic ONLY: monitor Qt's public QQuickWindow render-loop signals.
    // Neither callback makes a capture request, calls update(), or affects
    // Runtime's private ViewerCaptureDemand. Record only 1s aggregate counts.
    // Retain source-correlatable per-event UTC stamps in bounded 1-second
    // batches, not per-frame stdout (which itself can disturb scene timing).
    // Counts include overflow so a hot signal cannot silently masquerade as
    // a quiet signal just because the in-memory array reached its bound.
    QJsonArray afterAnimatingStamps;
    QJsonArray frameSwappedStamps;
    quint64 afterAnimatingOverflow = 0;
    quint64 frameSwappedOverflow = 0;
    QObject::connect(&view, &QQuickWindow::afterAnimating, &view, [&] {
        if (afterAnimatingStamps.size() < 256)
            afterAnimatingStamps.append(QDateTime::currentMSecsSinceEpoch());
        else
            ++afterAnimatingOverflow;
    }, Qt::AutoConnection);
    QObject::connect(&view, &QQuickWindow::frameSwapped, &view, [&] {
        if (frameSwappedStamps.size() < 256)
            frameSwappedStamps.append(QDateTime::currentMSecsSinceEpoch());
        else
            ++frameSwappedOverflow;
    }, Qt::AutoConnection);
    QTimer sceneSignalReportTimer;
    QObject::connect(&sceneSignalReportTimer, &QTimer::timeout, &view, [&] {
        const QJsonObject record{
            {"utc_ms", QDateTime::currentMSecsSinceEpoch()},
            {"after_animating_1s", afterAnimatingStamps.size()
                                 + static_cast<int>(afterAnimatingOverflow)},
            {"frame_swapped_1s", frameSwappedStamps.size()
                               + static_cast<int>(frameSwappedOverflow)},
            {"after_animating_utc_ms", afterAnimatingStamps},
            {"frame_swapped_utc_ms", frameSwappedStamps},
            {"after_animating_overflow", static_cast<double>(afterAnimatingOverflow)},
            {"frame_swapped_overflow", static_cast<double>(frameSwappedOverflow)},
            {"signal_kind", "passive_main_visible_QQuickWindow"},
        };
        std::cout << "HYREMOTE_QUICK_SCENE_SIGNALS_JSON="
                  << QJsonDocument(record).toJson(QJsonDocument::Compact).toStdString()
                  << std::endl;
        afterAnimatingStamps = QJsonArray{};
        frameSwappedStamps = QJsonArray{};
        afterAnimatingOverflow = 0;
        frameSwappedOverflow = 0;
    });
    sceneSignalReportTimer.start(1000);
    view.show();

    // This observer is scoped deliberately to the fixture's app-owned
    // source property. It never treats a render-loop notification as source
    // change, and it does not call window.update() or HyRemote code.
    // QMetaMethod -> zero-arg QTimer::start() is supported in Qt 5.15/6.8;
    // the Qt 6.10 QMetaMethod-to-lambda overload is NOT used.
    QQuickItem *sourceRoot = view.rootObject();
    if (!sourceRoot) {
        std::cerr << "PROPERTY_NOTIFY_PROBE=BLOCKED_NO_ROOT" << std::endl;
        return 72;
    }
    const QMetaObject *sourceMeta = sourceRoot->metaObject();
    const int propertyIndex = sourceMeta->indexOfProperty("phase");
    if (propertyIndex < 0 || !sourceMeta->property(propertyIndex).hasNotifySignal()) {
        std::cerr << "PROPERTY_NOTIFY_PROBE=BLOCKED_NO_PHASE_NOTIFY" << std::endl;
        return 73;
    }
    const QMetaProperty sourcePhase = sourceMeta->property(propertyIndex);
    QTimer sourceNotifyCoalescer;
    sourceNotifyCoalescer.setSingleShot(true);
    sourceNotifyCoalescer.setInterval(0);
    const int startSlotIndex = sourceNotifyCoalescer.metaObject()->indexOfSlot("start()");
    if (startSlotIndex < 0) {
        std::cerr << "PROPERTY_NOTIFY_PROBE=BLOCKED_TIMER_META_SLOT" << std::endl;
        return 74;
    }
    const QMetaMethod startSlot = sourceNotifyCoalescer.metaObject()->method(startSlotIndex);
    const QMetaObject::Connection notifyConnection =
        QObject::connect(sourceRoot, sourcePhase.notifySignal(),
                         &sourceNotifyCoalescer, startSlot, Qt::AutoConnection);
    if (!notifyConnection) {
        std::cerr << "PROPERTY_NOTIFY_PROBE=BLOCKED_META_CONNECT" << std::endl;
        return 75;
    }
    QJsonArray phaseSourceNotifyStamps;
    quint64 phaseNotifyOverflow = 0;
    QObject::connect(&sourceNotifyCoalescer, &QTimer::timeout, &view, [&] {
        if (phaseSourceNotifyStamps.size() < 256)
            phaseSourceNotifyStamps.append(QDateTime::currentMSecsSinceEpoch());
        else
            ++phaseNotifyOverflow;
    });
    QTimer phaseNotifyReportTimer;
    QObject::connect(&phaseNotifyReportTimer, &QTimer::timeout, &view, [&] {
        const QJsonObject row{
            {"utc_ms", QDateTime::currentMSecsSinceEpoch()},
            {"monitored_qml_property", "root.phase"},
            {"notify_signal_signature",
                QString::fromLatin1(sourcePhase.notifySignal().methodSignature())},
            {"phase_notify_1s", phaseSourceNotifyStamps.size()
                                + static_cast<int>(phaseNotifyOverflow)},
            {"phase_notify_utc_ms", phaseSourceNotifyStamps},
            {"phase_notify_overflow", static_cast<double>(phaseNotifyOverflow)},
            {"not_a_general_QML_scene_dirty_detector", true},
        };
        std::cout << "HYREMOTE_QML_PROPERTY_NOTIFIES_JSON="
                  << QJsonDocument(row).toJson(QJsonDocument::Compact).toStdString()
                  << std::endl;
        phaseSourceNotifyStamps = QJsonArray{};
        phaseNotifyOverflow = 0;
    });
    phaseNotifyReportTimer.start(1000);
    std::cout << "PROPERTY_NOTIFY_PROBE=CONNECTED_ROOT_PHASE" << std::endl;

    int fixturePhase = 0;
    const auto publishFixturePhase = [&] {
        std::cout << "HYREMOTE_DYNAMIC_STAGE_JSON={\"phase\":" << fixturePhase
                  << ",\"utc_ms\":" << QDateTime::currentMSecsSinceEpoch()
                  << ",\"source\":\"QTimer_property_before_render\"}" << std::endl;
    };
    publishFixturePhase();
    QTimer fixtureStageTimer;
    QObject::connect(&fixtureStageTimer, &QTimer::timeout, &view, [&] {
        fixturePhase = 1 - fixturePhase;
        if (view.rootObject()) {
            view.rootObject()->setProperty("phase", fixturePhase);
            publishFixturePhase();
        }
    });
    // A test-only launch switch uses the identical binary/Qt scene to
    // compare prolonged 0V/1V static CPU without rebuilding a different app.
    // In dynamic mode the exact same scene continues at 1200ms intervals.
    if (!QCoreApplication::arguments().contains(QStringLiteral("--fixture-static")))
        fixtureStageTimer.start(1200);
'''.strip("\n")
    altered = altered.replace(show_line, stage_driver)
    adjusted = altered.replace(OLD_RESIZE, NEW_RESIZE).encode("utf-8")
    qml = fixture.read_bytes()
    assert b"phase" in qml and b"#173cdb" in qml and b"#e8263b" in qml
    assert b"1920" in qml and b"1080" in qml
    (app / "main.cpp").write_bytes(adjusted)
    (app / "Main.qml").write_bytes(qml)
    # Git audit in bytes: stderr CRLF conversion warnings MUST NOT become
    # filenames. No other tracked source may be changed by the diagnostic.
    audited = subprocess.run(
        ["git", "diff", "--name-only", "-z", "--no-ext-diff"],
        cwd=tree, capture_output=True, check=False,
    )
    assert audited.returncode == 0, audited.stderr.decode(errors="replace")[-500:]
    changed = set(p.replace("\\", "/") for p in
                  audited.stdout.decode("utf-8").split("\0") if p)
    expected = {str(APP / "main.cpp").replace("\\", "/"),
                str(APP / "Main.qml").replace("\\", "/")}
    assert changed == expected, f"unexpected product source changes: {changed}"
    info = {
        "schema": "hyremote-windows-dynamic-quick-fixture/1",
        "fixture_is_app_only_not_product_patch": True,
        "app_main_original_sha256": digest(original_main),
        "app_main_modified_sha256": digest(adjusted),
        "app_qml_sha256": digest(qml),
        "framebuffer": [1920, 1080],
        "stage_interval_ms": 1200,
        "static_mode_switch": "--fixture-static",
        "signal_logging": "HYREMOTE_QUICK_SCENE_SIGNALS_JSON bounded 1s batches with UTC ms stamps and overflow on original main QQuickWindow",
        "diagnostic_does_not_trigger_capture": True,
        "property_probe": "QMetaProperty root.phase notifySignal -> QTimer.start() 0ms coalescer; no capture and no Qt6.10-only dynamic lambda",
        "property_logging": "HYREMOTE_QML_PROPERTY_NOTIFIES_JSON per-second UTC-millisecond stamps",
        "viewer_paint_not_measured_by_fixture": True,
    }
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
    print("DYNAMIC_REFERENCE_APP_PATCHED="+json.dumps(info, separators=(",", ":")))


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--worktree", required=True, type=Path)
    p.add_argument("--fixture", required=True, type=Path)
    p.add_argument("--manifest", required=True, type=Path)
    args = p.parse_args()
    patch(args.worktree, args.fixture, args.manifest)
