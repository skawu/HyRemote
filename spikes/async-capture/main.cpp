// HyRemote asynchronous capture spike (issue #16).
//
// This program is throwaway spike code. It evaluates the public asynchronous
// capture path - QQuickItem::grabToImage() - against HyRemote's whole-window
// requirements before any lower-level GL/PBO/RHI mechanism is considered.
//
// It defines no public HyRemote API, links no HyRemote target and contains no
// transport code.

#include "harness/async_probe.h"
#include "scenes/scene_host.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QProcess>
#include <QQuickWindow>
#include <QScreen>
#include <QSet>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>

#include <functional>

namespace {

using namespace hyremote::asyncspike;

struct CliOptions
{
    QStringList scenes;
    QString mode = QStringLiteral("all");
    int requests = 60;
    int syncCaptures = 60;
    int maxInFlight = 4;
    int intervalMs = 0;
    int consumerDelayMs = 0;
    int consumerWindow = 0;
    bool paced = false;
    bool dryRun = false;
    GrabTarget target = GrabTarget::ContentItem;
    QString rhi = QStringLiteral("default");
    QString jsonPath;
    bool list = false;
    bool help = false;
    QStringList problems;
};

struct SceneDescriptor
{
    SceneSpec spec;
    std::function<SceneHost *()> create;
};

void printUsage()
{
    QTextStream out(stdout);
    out << "usage: hyremote-async-spike [options]\n\n"
        << "  --scene <id>[,<id>...]  scene to run (repeatable, default: all)\n"
        << "  --mode <name>           composition | fidelity | pipeline | failure | sync-baseline | all | hidden-sync\n"
        << "  --requests <n>          async requests to issue in the pipeline mode (default 60)\n"
        << "  --sync-captures <n>     synchronous baseline captures (default 60)\n"
        << "  --max-inflight <n>      concurrent grabToImage() requests (default 4)\n"
        << "  --interval-ms <n>       event-loop time granted between request slices\n"
        << "  --consumer-delay <ms>   hold each completed frame this long (slow consumer)\n"
        << "  --consumer-window <n>   stop issuing while this many frames are held (0 = unlimited)\n"
        << "  --paced                 pump between individual requests (one request per frame)\n"
        << "  --dry-run               run the pipeline loop without issuing any grab (memory control)\n"
        << "  --target <name>         contentItem | rootItem (default contentItem)\n"
        << "  --rhi <name>            set QSG_RHI_BACKEND (default: leave it to Qt)\n"
        << "  --json <path>           write the machine readable report\n"
        << "  --list                  list scenes and exit\n"
        << "  --help                  show this help\n";
}

bool takeValue(const QStringList &arguments, int *index, QString *value)
{
    const QString argument = arguments.at(*index);
    const int separator = argument.indexOf(QLatin1Char('='));
    if (separator > 0) {
        *value = argument.mid(separator + 1);
        return true;
    }
    if (*index + 1 >= arguments.size())
        return false;
    ++(*index);
    *value = arguments.at(*index);
    return true;
}

CliOptions parseArguments(const QStringList &arguments)
{
    CliOptions options;

    for (int i = 1; i < arguments.size(); ++i) {
        const QString argument = arguments.at(i);
        QString value;
        bool ok = false;

        if (argument == QLatin1String("--help") || argument == QLatin1String("-h")) {
            options.help = true;
        } else if (argument == QLatin1String("--list")) {
            options.list = true;
        } else if (argument == QLatin1String("--paced")) {
            options.paced = true;
        } else if (argument == QLatin1String("--dry-run")) {
            options.dryRun = true;
        } else if (argument.startsWith(QLatin1String("--scene"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--scene requires a value"));
                continue;
            }
            for (const QString &part : value.split(QLatin1Char(','), Qt::SkipEmptyParts))
                options.scenes.append(part.trimmed());
        } else if (argument.startsWith(QLatin1String("--mode"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--mode requires a value"));
                continue;
            }
            options.mode = value;
        } else if (argument.startsWith(QLatin1String("--requests"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--requests requires a value"));
                continue;
            }
            options.requests = value.toInt(&ok);
            if (!ok || options.requests < 1)
                options.problems.append(QStringLiteral("--requests must be a positive integer"));
        } else if (argument.startsWith(QLatin1String("--sync-captures"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--sync-captures requires a value"));
                continue;
            }
            options.syncCaptures = value.toInt(&ok);
            if (!ok || options.syncCaptures < 1)
                options.problems.append(QStringLiteral("--sync-captures must be a positive integer"));
        } else if (argument.startsWith(QLatin1String("--max-inflight"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--max-inflight requires a value"));
                continue;
            }
            options.maxInFlight = value.toInt(&ok);
            if (!ok || options.maxInFlight < 1)
                options.problems.append(QStringLiteral("--max-inflight must be a positive integer"));
        } else if (argument.startsWith(QLatin1String("--interval-ms"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--interval-ms requires a value"));
                continue;
            }
            options.intervalMs = value.toInt(&ok);
            if (!ok || options.intervalMs < 0)
                options.problems.append(QStringLiteral("--interval-ms must be non-negative"));
        } else if (argument.startsWith(QLatin1String("--consumer-delay"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--consumer-delay requires a value"));
                continue;
            }
            options.consumerDelayMs = value.toInt(&ok);
            if (!ok || options.consumerDelayMs < 0)
                options.problems.append(QStringLiteral("--consumer-delay must be non-negative"));
        } else if (argument.startsWith(QLatin1String("--consumer-window"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--consumer-window requires a value"));
                continue;
            }
            options.consumerWindow = value.toInt(&ok);
            if (!ok || options.consumerWindow < 0)
                options.problems.append(QStringLiteral("--consumer-window must be non-negative"));
        } else if (argument.startsWith(QLatin1String("--target"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--target requires a value"));
                continue;
            }
            if (value == QLatin1String("rootItem"))
                options.target = GrabTarget::RootItem;
            else if (value == QLatin1String("contentItem"))
                options.target = GrabTarget::ContentItem;
            else
                options.problems.append(QStringLiteral("--target must be contentItem or rootItem"));
        } else if (argument.startsWith(QLatin1String("--rhi"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--rhi requires a value"));
                continue;
            }
            options.rhi = value;
        } else if (argument.startsWith(QLatin1String("--json"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--json requires a value"));
                continue;
            }
            options.jsonPath = value;
        } else {
            options.problems.append(QStringLiteral("unknown argument '%1'").arg(argument));
        }
    }

    return options;
}

QVector<SceneDescriptor> sceneRegistry()
{
    QVector<SceneDescriptor> descriptors;

    {
        SceneSpec spec;
        spec.id = QStringLiteral("quick2d");
        spec.title = QStringLiteral("QQuickWindow 2D scene");
        spec.family = QStringLiteral("QQuickWindow 2D");
        spec.kind = SceneKind::QuickView;
        spec.qmlType = QStringLiteral("AsyncScene");
        spec.notes = QStringList{
            QStringLiteral("The scene animates continuously and its encoded tick patch is driven from "
                           "the harness, so every captured image can be attributed to the request "
                           "that produced it.")};
        descriptors.append({spec, [spec] { return new SceneHost(spec); }});
    }

#ifdef HYREMOTE_ASYNC_HAVE_QUICK3D
    {
        SceneSpec spec;
        spec.id = QStringLiteral("quick3d");
        spec.title = QStringLiteral("Quick3D scene");
        spec.family = QStringLiteral("Quick3D");
        spec.kind = SceneKind::QuickView;
        spec.qmlType = QStringLiteral("Quick3DScene");
        spec.notes = QStringList{
            QStringLiteral("Quick3D renders through its own renderer inside the scene graph, so this "
                           "case checks whether the asynchronous item grab reproduces composed 3D "
                           "content.")};
        descriptors.append({spec, [spec] { return new SceneHost(spec); }});
    }
#endif

    {
        SceneSpec spec;
        spec.id = QStringLiteral("customfbo");
        spec.title = QStringLiteral("custom QQuickFramebufferObject content");
        spec.family = QStringLiteral("custom QQuickFramebufferObject");
        spec.kind = SceneKind::QuickView;
        spec.qmlType = QStringLiteral("CustomFboScene");
        spec.requiredEnv = QStringList{QStringLiteral("QSG_RHI_BACKEND=opengl")};
        spec.notes = QStringList{
            QStringLiteral("The custom FBO renderer encodes its own render counter in its clear "
                           "color, so both the freshness and the composition of the grabbed region "
                           "can be checked.")};
        descriptors.append({spec, [spec] { return new SceneHost(spec); }});
    }

    {
        SceneSpec spec;
        spec.id = QStringLiteral("quickwidget");
        spec.title = QStringLiteral("QQuickWidget inside a QWidget hierarchy");
        spec.family = QStringLiteral("QQuickWidget");
        spec.kind = SceneKind::QuickWidget;
        spec.qmlType = QStringLiteral("AsyncScene");
        spec.notes = QStringList{
            QStringLiteral("Tests whether a QQuickWidget's Quick content can be captured "
                           "asynchronously through its public rootObject(), and what that does and "
                           "does not cover.")};
        descriptors.append({spec, [spec] { return new SceneHost(spec); }});
    }

    {
        SceneSpec spec;
        spec.id = QStringLiteral("openglwidget");
        spec.title = QStringLiteral("QOpenGLWidget inside a QWidget hierarchy");
        spec.family = QStringLiteral("QOpenGLWidget");
        spec.kind = SceneKind::OpenGlWidget;
        spec.notes = QStringList{
            QStringLiteral("Control case: Qt exposes no public asynchronous capture API for a "
                           "QOpenGLWidget, so only the synchronous baseline is measurable.")};
        descriptors.append({spec, [spec] { return new SceneHost(spec); }});
    }

    return descriptors;
}

QVector<SceneDescriptor> selectScenes(const QVector<SceneDescriptor> &registry,
                                     const QStringList &requested,
                                     QStringList *problems)
{
    if (requested.isEmpty())
        return registry;

    QVector<SceneDescriptor> selected;
    for (const QString &id : requested) {
        bool found = false;
        for (const SceneDescriptor &descriptor : registry) {
            if (descriptor.spec.id == id) {
                selected.append(descriptor);
                found = true;
                break;
            }
        }
        if (!found)
            problems->append(QStringLiteral("unknown scene '%1'").arg(id));
    }
    return selected;
}

QVariantMap environmentReport()
{
    QVariantMap report;
    report.insert(QStringLiteral("qtRuntimeVersion"), QString::fromLatin1(qVersion()));
    report.insert(QStringLiteral("qtCompileVersion"), QStringLiteral(QT_VERSION_STR));
    report.insert(QStringLiteral("buildAbi"), QSysInfo::buildAbi());
    report.insert(QStringLiteral("kernelType"), QSysInfo::kernelType());
    report.insert(QStringLiteral("kernelVersion"), QSysInfo::kernelVersion());
    report.insert(QStringLiteral("productType"), QSysInfo::productType());
    report.insert(QStringLiteral("productVersion"), QSysInfo::productVersion());
    report.insert(QStringLiteral("platformName"),
                  QGuiApplication::platformName().isEmpty() ? QStringLiteral("<none>")
                                                            : QGuiApplication::platformName());
    report.insert(QStringLiteral("qsgRhiBackend"),
                  qEnvironmentVariableIsSet("QSG_RHI_BACKEND")
                      ? qEnvironmentVariable("QSG_RHI_BACKEND")
                      : QStringLiteral("<default>"));

    QVariantList screens;
    for (const QScreen *screen : QGuiApplication::screens()) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), screen->name());
        entry.insert(QStringLiteral("geometry"),
                     QStringLiteral("%1x%2")
                         .arg(screen->geometry().width())
                         .arg(screen->geometry().height()));
        entry.insert(QStringLiteral("devicePixelRatio"), screen->devicePixelRatio());
        screens.append(entry);
    }
    report.insert(QStringLiteral("screens"), screens);
    return report;
}

// Internal child-process mode: probe the synchronous whole-window capture while the
// window is hidden. It runs in its own process so that a potential stall cannot take
// the evidence run with it.
int runHiddenSyncChild(const SceneSpec &spec)
{
    SceneHost scene(spec);
    scene.prepare();
    scene.show();

    // Render once while visible so the scene graph is initialized.
    {
        QEventLoop loop;
        QTimer::singleShot(400, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::AllEvents);
    }

    scene.hide();
    QElapsedTimer timer;
    timer.start();
    const QImage image = scene.window() ? scene.window()->grabWindow() : QImage();
    const qint64 elapsedMs = timer.elapsed();

    QTextStream out(stdout);
    out << QStringLiteral("hidden-sync: scene=%1 image=%2x%3 elapsedMs=%4\n")
               .arg(spec.id)
               .arg(image.width())
               .arg(image.height())
               .arg(elapsedMs);
    out.flush();

    scene.shutdown();
    return image.isNull() ? 2 : 0;
}

struct HiddenSyncResult
{
    QString scene;
    QString status;
    int exitCode = -1;
    QString stdoutText;
    QString stderrText;
};

HiddenSyncResult runHiddenSyncInChildProcess(const QString &sceneId, const QString &rhi)
{
    HiddenSyncResult result;
    result.scene = sceneId;

    QStringList arguments{QStringLiteral("--mode"), QStringLiteral("hidden-sync"),
                          QStringLiteral("--scene"), sceneId};
    if (rhi != QLatin1String("default"))
        arguments << QStringLiteral("--rhi") << rhi;

    QProcess process;
    process.setProgram(QCoreApplication::applicationFilePath());
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(10000)) {
        result.status = QStringLiteral("failed to start the child process");
        return result;
    }
    if (!process.waitForFinished(20000)) {
        process.kill();
        process.waitForFinished(2000);
        result.status = QStringLiteral("child process did not finish within 20000 ms (killed)");
        result.stdoutText = QString::fromLocal8Bit(process.readAllStandardOutput());
        result.stderrText = QString::fromLocal8Bit(process.readAllStandardError());
        return result;
    }

    result.exitCode = process.exitCode();
    result.stdoutText = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    result.stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    result.status = result.exitCode == 0 ? QStringLiteral("completed with an image")
                                         : (result.exitCode == 2
                                                ? QStringLiteral("completed, grabWindow() returned a null image")
                                                : QStringLiteral("completed with exit code %1")
                                                      .arg(result.exitCode));
    return result;
}

void printPipelineSummary(const PipelineResult &result)
{
    QTextStream out(stdout);
    out << QStringLiteral("\n  pipeline     : target %1, requests %2, max in flight %3, paced %4, "
                          "consumer delay %5 ms\n")
               .arg(result.targetName)
               .arg(result.requests)
               .arg(result.maxInFlight)
               .arg(result.pacedRequests ? QStringLiteral("yes") : QStringLiteral("no"))
               .arg(result.consumerDelayMs);
    out << QStringLiteral("  consumer     : window %1 (0 = unlimited), max frames held %2, "
                          "throttled slices %3\n")
               .arg(result.consumerWindow)
               .arg(result.maxHeldObserved)
               .arg(result.throttledSlices);
    out << QStringLiteral("  outcome      : completed %1 (fps %2), failed to start %3, null images %4, "
                          "max in flight observed %5, timed out %6\n")
               .arg(result.completed)
               .arg(result.completedPerSecond, 0, 'f', 1)
               .arg(result.failedToStart)
               .arg(result.nullImages)
               .arg(result.maxInFlightObserved)
               .arg(result.timedOut ? QStringLiteral("yes") : QStringLiteral("no"));
    out << QStringLiteral("  latency      : avg %1 ms, p50 %2 ms, p95 %3 ms, max %4 ms\n")
               .arg(result.latencyAvgMs, 0, 'f', 2)
               .arg(result.latencyP50Ms, 0, 'f', 2)
               .arg(result.latencyP95Ms, 0, 'f', 2)
               .arg(result.latencyMaxMs, 0, 'f', 2);
    out << QStringLiteral("  content      : distinct ticks %1, duplicate completions %2, out of order "
                          "%3, tick lag avg %4 / p95 %5 / max %6, frames rendered %7\n")
               .arg(result.distinctTicks)
               .arg(result.duplicateCompletions)
               .arg(result.outOfOrderCompletions)
               .arg(result.tickLagAvg, 0, 'f', 2)
               .arg(result.tickLagP95, 0, 'f', 2)
               .arg(result.tickLagMax, 0, 'f', 2)
               .arg(result.framesRendered);
    out << QStringLiteral("  resources    : RSS %1 -> %2 MiB (slope %3 KiB/100), peak retained %4 KiB, "
                          "CPU %5 %% of one core, GUI latency avg %6 / p95 %7 ms\n")
               .arg(result.rssStartBytes / 1048576.0, 0, 'f', 1)
               .arg(result.rssEndBytes / 1048576.0, 0, 'f', 1)
               .arg(result.rssSlopeBytesPer100Frames / 1024.0, 0, 'f', 1)
               .arg(result.peakRetainedBytes / 1024.0, 0, 'f', 1)
               .arg(result.cpuPercentOfOneCore, 0, 'f', 1)
               .arg(result.guiLatencyAvgMs, 0, 'f', 2)
               .arg(result.guiLatencyP95Ms, 0, 'f', 2);
    for (const QString &note : result.notes)
        out << QStringLiteral("  note         : %1\n").arg(note);
    out.flush();
}

}  // namespace

int main(int argc, char *argv[])
{
    const QStringList arguments = [&] {
        QStringList list;
        for (int i = 0; i < argc; ++i)
            list.append(QString::fromLocal8Bit(argv[i]));
        return list;
    }();

    const CliOptions options = parseArguments(arguments);

    if (options.help) {
        printUsage();
        return 0;
    }

    const QVector<SceneDescriptor> registry = sceneRegistry();

    if (options.list) {
        QTextStream out(stdout);
        out << "async capture scenes:\n";
        for (const SceneDescriptor &descriptor : registry) {
            out << QStringLiteral("  %1  %2  [%3]\n")
                       .arg(descriptor.spec.id, -14)
                       .arg(descriptor.spec.title, descriptor.spec.family);
            for (const QString &required : descriptor.spec.requiredEnv)
                out << QStringLiteral("      requires %1\n").arg(required);
        }
        out << "\nBuild/run notes are in spikes/async-capture/README.md\n";
        return 0;
    }

    QStringList problems = options.problems;
    const QVector<SceneDescriptor> selected = selectScenes(registry, options.scenes, &problems);
    if (!problems.isEmpty()) {
        QTextStream err(stderr);
        for (const QString &problem : problems)
            err << QStringLiteral("error: %1\n").arg(problem);
        err << QStringLiteral("\n");
        printUsage();
        return 1;
    }
    if (selected.isEmpty()) {
        QTextStream err(stderr);
        err << "error: no scene selected\n";
        return 1;
    }

    // Resolve process-wide environment before QApplication exists.
    QVector<QPair<QString, QString>> requiredEnv;
    QSet<QString> skipped;
    for (const SceneDescriptor &descriptor : selected) {
        for (const QString &entry : descriptor.spec.requiredEnv) {
            const int separator = entry.indexOf(QLatin1Char('='));
            if (separator <= 0)
                continue;
            const QString key = entry.left(separator);
            const QString value = entry.mid(separator + 1);

            bool conflict = false;
            bool present = false;
            for (const auto &existing : requiredEnv) {
                if (existing.first == key) {
                    if (existing.second != value)
                        conflict = true;
                    else
                        present = true;
                }
            }
            if (conflict) {
                skipped.insert(descriptor.spec.id);
                continue;
            }
            if (!present)
                requiredEnv.append({key, value});
        }
    }

    if (!options.rhi.isEmpty() && options.rhi != QLatin1String("default")) {
        bool present = false;
        for (auto &entry : requiredEnv) {
            if (entry.first == QLatin1String("QSG_RHI_BACKEND")) {
                entry.second = options.rhi;
                present = true;
            }
        }
        if (!present)
            requiredEnv.append({QStringLiteral("QSG_RHI_BACKEND"), options.rhi});
    }

    if (options.mode == QLatin1String("hidden-sync")) {
        // Child mode: apply the environment, then run the probe in this process.
        for (const auto &entry : requiredEnv)
            qputenv(entry.first.toUtf8().constData(), entry.second.toUtf8());
        QApplication application(argc, argv);
        const SceneSpec spec = selected.first().spec;
        return runHiddenSyncChild(spec);
    }

    for (const auto &entry : requiredEnv)
        qputenv(entry.first.toUtf8().constData(), entry.second.toUtf8());

    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("hyremote-async-spike"));

    const QVariantMap environment = environmentReport();

    QTextStream out(stdout);
    out << QStringLiteral("HyRemote async capture spike (issue #16)\n");
    out << QStringLiteral("Qt %1 on %2, QPA '%3', RHI '%4', mode '%5'\n")
               .arg(environment.value(QStringLiteral("qtRuntimeVersion")).toString(),
                    environment.value(QStringLiteral("productType")).toString(),
                    environment.value(QStringLiteral("platformName")).toString(),
                    environment.value(QStringLiteral("qsgRhiBackend")).toString(),
                    options.mode);
    out.flush();

    QVariantList results;
    QVariantList skippedScenes;

    for (const SceneDescriptor &descriptor : selected) {
        if (skipped.contains(descriptor.spec.id)) {
            QVariantMap entry;
            entry.insert(QStringLiteral("scene"), descriptor.spec.id);
            entry.insert(QStringLiteral("reason"),
                         QStringLiteral("required process environment conflicts with another selected "
                                        "scene; run this scene in its own invocation"));
            entry.insert(QStringLiteral("requiredEnv"), descriptor.spec.requiredEnv);
            skippedScenes.append(entry);
            out << QStringLiteral("\n[suspended] %1: %2\n")
                       .arg(descriptor.spec.id, entry.value(QStringLiteral("reason")).toString());
            out.flush();
            continue;
        }

        out << QStringLiteral("\n=== %1 (%2) ===\n")
                   .arg(descriptor.spec.id, descriptor.spec.family);
        out.flush();

        QVariantMap sceneResult;
        sceneResult.insert(QStringLiteral("scene"), descriptor.spec.id);
        sceneResult.insert(QStringLiteral("title"), descriptor.spec.title);
        sceneResult.insert(QStringLiteral("family"), descriptor.spec.family);

        SceneHost *scene = descriptor.create();
        scene->prepare();
        scene->show();

        // The overlay sibling is present in every mode: it is the representative of
        // QML overlays, popups and tooltips, and both the composition and the fidelity
        // comparisons need it to be there.
        if (descriptor.spec.kind != SceneKind::OpenGlWidget)
            scene->addOverlaySibling();

        const bool wantComposition = options.mode == QLatin1String("composition")
                                     || options.mode == QLatin1String("all");
        const bool wantFidelity = options.mode == QLatin1String("fidelity")
                                  || options.mode == QLatin1String("all");
        const bool wantPipeline = options.mode == QLatin1String("pipeline")
                                  || options.mode == QLatin1String("all");
        const bool wantFailure = options.mode == QLatin1String("failure")
                                 || options.mode == QLatin1String("all");
        const bool wantSyncBaseline = options.mode == QLatin1String("sync-baseline")
                                      || options.mode == QLatin1String("all");

        if (wantSyncBaseline) {
            const SyncBaselineResult baseline =
                runSyncBaseline(*scene, options.syncCaptures, options.intervalMs);
            sceneResult.insert(QStringLiteral("syncBaseline"), syncBaselineToVariant(baseline));
            out << QStringLiteral("  sync baseline: %1 -> %2 captures, avg %3 ms, p50 %4 ms, "
                                  "p95 %5 ms, %6 captures/s, null images %7, GUI latency avg %8 / "
                                  "p95 %9 ms\n")
                       .arg(baseline.api)
                       .arg(baseline.captures)
                       .arg(baseline.avgMs, 0, 'f', 2)
                       .arg(baseline.p50Ms, 0, 'f', 2)
                       .arg(baseline.p95Ms, 0, 'f', 2)
                       .arg(baseline.capturesPerSecond, 0, 'f', 1)
                       .arg(baseline.nullImages)
                       .arg(baseline.guiLatencyAvgMs, 0, 'f', 2)
                       .arg(baseline.guiLatencyP95Ms, 0, 'f', 2);
            out.flush();
        }

        if (wantComposition) {
            const CompositionResult composition = runComposition(*scene);
            sceneResult.insert(QStringLiteral("composition"), compositionToVariant(composition));
            out << QStringLiteral("  composition  : applicable %1, overlay in rootItem async %2, in "
                                  "contentItem async %3, in sync window %4\n")
                       .arg(composition.applicable ? QStringLiteral("yes") : QStringLiteral("no"))
                       .arg(composition.overlayPresentInRootItemGrab ? QStringLiteral("yes")
                                                                     : QStringLiteral("no"))
                       .arg(composition.overlayPresentInContentItemGrab ? QStringLiteral("yes")
                                                                        : QStringLiteral("no"))
                       .arg(composition.overlayPresentInSyncWindowGrab ? QStringLiteral("yes")
                                                                      : QStringLiteral("no"));
            out << QStringLiteral("  composition  : sizes rootItem %1 | contentItem %2 | sync window "
                                  "%3, overlay pixel rootItem %4, contentItem %5, sync %6 (expected %7)\n")
                       .arg(composition.rootItemGrabSize,
                            composition.contentItemGrabSize,
                            composition.syncWindowGrabSize,
                            composition.rootItemOverlayPixel,
                            composition.contentItemOverlayPixel,
                            composition.syncWindowOverlayPixel,
                            composition.expectedOverlayPixel);
            for (const QString &note : composition.notes)
                out << QStringLiteral("  note         : %1\n").arg(note);
            out.flush();
        }

        if (wantFidelity) {
            const QVector<FidelityResult> fidelity = runFidelity(*scene);
            QVariantList fidelityList;
            for (const FidelityResult &entry : fidelity)
                fidelityList.append(fidelityToVariant(entry));
            sceneResult.insert(QStringLiteral("fidelity"), fidelityList);
            for (const FidelityResult &entry : fidelity) {
                out << QStringLiteral("  fidelity     : %1 vs %2 -> sizeMatch %3, mean abs diff %4, "
                                      "max channel diff %5, differing pixels %6 %%, outside overlay "
                                      "%7 %%\n")
                           .arg(entry.labelA,
                                entry.labelB,
                                entry.sizeMatch ? QStringLiteral("yes") : QStringLiteral("no"))
                           .arg(entry.meanAbsDiff, 0, 'f', 3)
                           .arg(entry.maxChannelDiff)
                           .arg(entry.differingPixelPct, 0, 'f', 2)
                           .arg(entry.differingOutsideOverlayPct, 0, 'f', 2);
            }
            out.flush();
        }

        if (wantPipeline && descriptor.spec.kind != SceneKind::OpenGlWidget) {
            ProbeConfig config;
            config.requests = options.requests;
            config.maxInFlight = options.maxInFlight;
            config.intervalMs = options.intervalMs;
            config.consumerDelayMs = options.consumerDelayMs;
            config.consumerWindow = options.consumerWindow;
            config.pacedRequests = options.paced;
            config.target = options.target;
            config.dryRun = options.dryRun;
            const PipelineResult pipeline = runPipeline(*scene, config);
            sceneResult.insert(QStringLiteral("pipeline"), pipelineToVariant(pipeline));
            printPipelineSummary(pipeline);
        } else if (wantPipeline) {
            out << QStringLiteral("  pipeline     : not applicable, this scene has no QML item tree\n");
            sceneResult.insert(QStringLiteral("pipelineApplicable"), false);
        }

        if (wantFailure) {
            const QVector<FailureModeResult> failures = runFailureModes(*scene);
            QVariantList failureList;
            for (const FailureModeResult &entry : failures)
                failureList.append(failureToVariant(entry));
            sceneResult.insert(QStringLiteral("failureModes"), failureList);
            for (const FailureModeResult &entry : failures) {
                out << QStringLiteral("  failure mode : %1 -> null result %2, completed %3, image null "
                                      "%4, warnings %5\n")
                           .arg(entry.name,
                                entry.returnedNullRequest ? QStringLiteral("yes") : QStringLiteral("no"),
                                entry.completed ? QStringLiteral("yes") : QStringLiteral("no"),
                                entry.imageNull ? QStringLiteral("yes") : QStringLiteral("no"))
                           .arg(entry.qtWarnings.size());
                for (const QString &warning : entry.qtWarnings)
                    out << QStringLiteral("                 warning: %1\n").arg(warning);
            }
            out.flush();
        }

        // Synchronous whole-window baseline and the hidden-window comparison run in a
        // child process because the synchronous grab on a hidden window may stall.
        if (descriptor.spec.kind != SceneKind::OpenGlWidget) {
            const HiddenSyncResult hiddenSync =
                runHiddenSyncInChildProcess(descriptor.spec.id, options.rhi);
            QVariantMap entry;
            entry.insert(QStringLiteral("scene"), hiddenSync.scene);
            entry.insert(QStringLiteral("status"), hiddenSync.status);
            entry.insert(QStringLiteral("exitCode"), hiddenSync.exitCode);
            entry.insert(QStringLiteral("stdout"), hiddenSync.stdoutText);
            entry.insert(QStringLiteral("stderr"), hiddenSync.stderrText);
            sceneResult.insert(QStringLiteral("hiddenWindowSyncGrab"), entry);
            out << QStringLiteral("  hidden-window: %1 (exit %2)\n")
                       .arg(hiddenSync.status)
                       .arg(hiddenSync.exitCode);
            if (!hiddenSync.stdoutText.isEmpty())
                out << QStringLiteral("                 %1\n").arg(hiddenSync.stdoutText);
            out.flush();
        }

        sceneResult.insert(QStringLiteral("sceneInfo"), scene->info());
        QStringList observations = scene->observations();
        sceneResult.insert(QStringLiteral("observations"), observations);
        for (const QString &observation : observations)
            out << QStringLiteral("  observation  : %1\n").arg(observation);

        scene->shutdown();
        delete scene;

        results.append(sceneResult);
    }

    if (!options.jsonPath.isEmpty()) {
        QVariantMap root;
        root.insert(QStringLiteral("schema"), QStringLiteral("hyremote-async-capture-spike/1"));
        root.insert(QStringLiteral("issue"),
                    QStringLiteral("https://github.com/skawu/HyRemote/issues/16"));
        root.insert(QStringLiteral("environment"), environment);
        root.insert(QStringLiteral("arguments"), arguments.mid(1));
        root.insert(QStringLiteral("results"), results);
        root.insert(QStringLiteral("skippedScenes"), skippedScenes);

        const QJsonDocument document(QJsonObject::fromVariantMap(root));
        QFile file(options.jsonPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream(stderr) << QStringLiteral("error: cannot write %1\n").arg(options.jsonPath);
            return 1;
        }
        file.write(document.toJson(QJsonDocument::Indented));
        file.close();

        out << QStringLiteral("\nreport written to %1\n").arg(options.jsonPath);
        out.flush();
    }

    return 0;
}
