// HyRemote SPIKE-01 capture harness (issue #3).
//
// This program is throwaway spike code. It drives the capture cases listed in
// issue #3, measures them with identical scenes and identical timing rules, and
// emits the evidence recorded in docs/capture-spike.md.
//
// It intentionally does not define, link against, or freeze any HyRemote public
// API, and it contains no transport code.

#include "harness/sample_runner.h"

#include "samples/custom_fbo_sample.h"
#include "samples/opengl_widget_sample.h"
#include "samples/quick3d_sample.h"
#include "samples/quick_sample.h"
#include "samples/quick_widget_sample.h"
#include "samples/widgets_sample.h"

#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QSet>
#include <QTextStream>

#include <functional>

namespace {

struct CliOptions
{
    QStringList samples;
    int frames = 120;
    int intervalMs = 16;
    int warmupFrames = 15;
    int runs = 1;
    int submitEvery = 3;
    bool sink = true;
    QString rhi = QStringLiteral("default");
    QString jsonPath;
    QStringList disabledPaths;
    bool list = false;
    bool help = false;
    QStringList problems;
};

struct SampleDescriptor
{
    QString id;
    QString title;
    QString targetFamily;
    QStringList requiredEnv;
    std::function<hyremote::spike::CaptureSample *()> create;
};

void printUsage()
{
    QTextStream out(stdout);
    out << "usage: hyremote-capture-spike [options]\n\n"
        << "  --sample <id>[,<id>...]  capture case to run (repeatable, default: all)\n"
        << "  --frames <n>            measured frames per run (default 120)\n"
        << "  --interval-ms <n>       delay between captures (default 16, 0 = as fast as possible)\n"
        << "  --warmup <n>            discarded warm-up frames (default 15)\n"
        << "  --runs <n>              repeated start/stop runs (default 1)\n"
        << "  --submit-every <n>      hand every Nth frame to the sink probe (default 3, 0 = off)\n"
        << "  --no-sink               disable the frame hand-off probe\n"
        << "  --rhi <name>            set QSG_RHI_BACKEND (default: leave it to Qt)\n"
        << "  --disable-path <text>   skip capture paths whose label contains <text>\n"
        << "  --json <path>           write the machine readable report\n"
        << "  --list                  list capture cases and exit\n"
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
        } else if (argument == QLatin1String("--no-sink")) {
            options.sink = false;
        } else if (argument.startsWith(QLatin1String("--disable-path"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--disable-path requires a value"));
                continue;
            }
            options.disabledPaths.append(value);
        } else if (argument.startsWith(QLatin1String("--sample"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--sample requires a value"));
                continue;
            }
            for (const QString &part : value.split(QLatin1Char(','), Qt::SkipEmptyParts))
                options.samples.append(part.trimmed());
        } else if (argument.startsWith(QLatin1String("--frames"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--frames requires a value"));
                continue;
            }
            options.frames = value.toInt(&ok);
            if (!ok || options.frames < 1)
                options.problems.append(QStringLiteral("--frames must be a positive integer"));
        } else if (argument.startsWith(QLatin1String("--interval-ms"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--interval-ms requires a value"));
                continue;
            }
            options.intervalMs = value.toInt(&ok);
            if (!ok || options.intervalMs < 0)
                options.problems.append(QStringLiteral("--interval-ms must be a non-negative integer"));
        } else if (argument.startsWith(QLatin1String("--warmup"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--warmup requires a value"));
                continue;
            }
            options.warmupFrames = value.toInt(&ok);
            if (!ok || options.warmupFrames < 0)
                options.problems.append(QStringLiteral("--warmup must be a non-negative integer"));
        } else if (argument.startsWith(QLatin1String("--runs"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--runs requires a value"));
                continue;
            }
            options.runs = value.toInt(&ok);
            if (!ok || options.runs < 1)
                options.problems.append(QStringLiteral("--runs must be a positive integer"));
        } else if (argument.startsWith(QLatin1String("--submit-every"))) {
            if (!takeValue(arguments, &i, &value)) {
                options.problems.append(QStringLiteral("--submit-every requires a value"));
                continue;
            }
            options.submitEvery = value.toInt(&ok);
            if (!ok || options.submitEvery < 0)
                options.problems.append(QStringLiteral("--submit-every must be a non-negative integer"));
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

hyremote::spike::QuickSceneSpec quick2DSpec()
{
    using namespace hyremote::spike;
    return QuickSceneSpec{
        QStringLiteral("quick"),
        QStringLiteral("QQuickWindow / Qt Quick 2D scene"),
        QStringLiteral("QQuickWindow 2D"),
        QStringLiteral("QuickScene"),
        {},
        {QStringLiteral(
             "The scene mixes an animated region with static chrome, so a capture can be compared "
             "against the expected content."),
         QStringLiteral(
             "On EGLFS the same scene must render directly to the display device; the capture path "
             "is expected to be identical because grabWindow() renders into its own target.")},
    };
}

QVector<SampleDescriptor> sampleRegistry()
{
    using namespace hyremote::spike;

    QVector<SampleDescriptor> descriptors;

    descriptors.append({QStringLiteral("widgets"),
                        QStringLiteral("QWidget / raster baseline"),
                        QStringLiteral("QWidget/raster"),
                        {},
                        [] { return new WidgetsSample; }});

    descriptors.append({QStringLiteral("quick"),
                        QStringLiteral("QQuickWindow / Qt Quick 2D scene"),
                        QStringLiteral("QQuickWindow 2D"),
                        {},
                        [] { return new QuickSample(quick2DSpec()); }});

    descriptors.append({QStringLiteral("quick3d"),
                        QStringLiteral("Quick3D scene"),
                        QStringLiteral("Quick3D"),
                        {},
                        [] { return new Quick3DSample; }});

    descriptors.append({QStringLiteral("openglwidget"),
                        QStringLiteral("QOpenGLWidget inside a QWidget hierarchy"),
                        QStringLiteral("QOpenGLWidget"),
                        {},
                        [] { return new OpenGlWidgetSample; }});

    descriptors.append({QStringLiteral("quickwidget"),
                        QStringLiteral("QQuickWidget inside a QWidget hierarchy"),
                        QStringLiteral("QQuickWidget"),
                        {},
                        [] { return new QuickWidgetSample; }});

    descriptors.append({QStringLiteral("customfbo"),
                        QStringLiteral("custom QQuickFramebufferObject content"),
                        QStringLiteral("custom QQuickFramebufferObject"),
                        {QStringLiteral("QSG_RHI_BACKEND=opengl")},
                        [] { return new CustomFboSample; }});

    return descriptors;
}

QVector<SampleDescriptor> selectDescriptors(const QVector<SampleDescriptor> &registry,
                                            const QStringList &requested,
                                            QStringList *problems)
{
    if (requested.isEmpty())
        return registry;

    QVector<SampleDescriptor> selected;
    for (const QString &id : requested) {
        bool found = false;
        for (const SampleDescriptor &descriptor : registry) {
            if (descriptor.id == id) {
                selected.append(descriptor);
                found = true;
                break;
            }
        }
        if (!found)
            problems->append(QStringLiteral("unknown capture case '%1'").arg(id));
    }
    return selected;
}

void printRunSummary(const hyremote::spike::RunReport &report)
{
    using namespace hyremote::spike;

    QTextStream out(stdout);
    out << QStringLiteral("\n=== %1 (run %2) ===\n").arg(report.sampleId).arg(report.runIndex);
    out << QStringLiteral("target family : %1\n").arg(report.targetFamily);
    out << QStringLiteral("frames        : requested %1, captured %2 over %3 s (%4 fps)\n")
               .arg(report.framesRequested)
               .arg(report.framesCaptured)
               .arg(report.wallSeconds, 0, 'f', 2)
               .arg(report.achievedFps, 0, 'f', 1);
    out << QStringLiteral("process CPU   : %1 s (%2 % of one core)\n")
               .arg(report.cpuSeconds, 0, 'f', 2)
               .arg(report.cpuPercentOfOneCore, 0, 'f', 1);
    out << QStringLiteral("RSS           : start %1 MiB, end %2 MiB, slope %3 KiB / 100 frames\n")
               .arg(report.rssStartBytes / 1048576.0, 0, 'f', 1)
               .arg(report.rssEndBytes / 1048576.0, 0, 'f', 1)
               .arg(report.rssSlopeBytesPer100Frames / 1024.0, 0, 'f', 1);
    out << QStringLiteral("GUI latency   : avg %1 ms, p95 %2 ms, max %3 ms\n")
               .arg(report.guiLatencyAvgMs, 0, 'f', 2)
               .arg(report.guiLatencyP95Ms, 0, 'f', 2)
               .arg(report.guiLatencyMaxMs, 0, 'f', 2);
    out << QStringLiteral("iteration     : app events %1 ms, capture calls %2 ms, other %3 ms\n")
               .arg(report.avgPumpMs, 0, 'f', 2)
               .arg(report.avgCaptureLoopMs, 0, 'f', 2)
               .arg(report.wallSeconds * 1000.0 / qMax(1, report.framesRequested)
                        - report.avgPumpMs - report.avgCaptureLoopMs,
                    0,
                    'f',
                    2);
    if (report.damageTargetArea > 0) {
        out << QStringLiteral("damage        : app %1 %, capture-induced %2 % of the shared target "
                              "(%3 px^2); mapped paint events %4, excluded %5, update requests %6\n")
                   .arg(report.avgDamageRatio * 100.0, 0, 'f', 2)
                   .arg(report.avgCaptureDamageRatio * 100.0, 0, 'f', 2)
                   .arg(report.damageTargetArea)
                   .arg(report.mappedPaintEvents)
                   .arg(report.excludedPaintEvents)
                   .arg(report.updateRequests);
    } else {
        out << QStringLiteral("damage        : no shared QWidget target; mapped paint events %1, "
                              "excluded %2, update requests %3\n")
                   .arg(report.mappedPaintEvents)
                   .arg(report.excludedPaintEvents)
                   .arg(report.updateRequests);
    }

    for (const PathStats &path : report.paths) {
        out << QStringLiteral("\n  path        : %1%2\n")
                   .arg(path.path, path.primary ? QStringLiteral("  [primary]") : QString());
        out << QStringLiteral("  success     : %1/%2\n").arg(path.successes).arg(path.attempts);
        if (path.successes > 0) {
            out << QStringLiteral("  first       : %1 ms\n").arg(path.firstNs / 1.0e6, 0, 'f', 2);
            out << QStringLiteral("  capture     : avg %1 ms, p50 %2 ms, p95 %3 ms, max %4 ms\n")
                       .arg(path.avgNs / 1.0e6, 0, 'f', 2)
                       .arg(path.p50Ns / 1.0e6, 0, 'f', 2)
                       .arg(path.p95Ns / 1.0e6, 0, 'f', 2)
                       .arg(path.maxNs / 1.0e6, 0, 'f', 2);
            out << QStringLiteral("  frame       : %1 %2, caller %3\n")
                       .arg(path.frameSize, path.frameFormat, path.callerThread);
            out << QStringLiteral("  buffer      : producer reuses buffer = %1\n")
                       .arg(path.producerReusesBuffer ? QStringLiteral("yes")
                                                      : QStringLiteral("no"));
        }
        if (path.storageProbes > 0) {
            out << QStringLiteral("  storage     : %1 probes, %2 storage replacements (%3 -> %4)\n")
                       .arg(path.storageProbes)
                       .arg(path.storageReplacements)
                       .arg(path.storageAddressFirst, path.storageAddressLast);
        }
        for (const QString &note : path.notes)
            out << QStringLiteral("  note        : %1\n").arg(note);
    }

    for (const DamageControlResult &control : report.damageControls) {
        out << QStringLiteral("  control     : %1 -> %2\n")
                   .arg(control.description,
                        control.passed ? QStringLiteral("PASSED") : QStringLiteral("FAILED"));
        out << QStringLiteral("                expected %1, observed %2, area %3 px, "
                              "mapped events %4, excluded events %5\n")
                   .arg(control.expectEmpty
                            ? QStringLiteral("no damage")
                            : QStringLiteral("%1,%2 %3x%4")
                                  .arg(control.expectedTargetRect.x())
                                  .arg(control.expectedTargetRect.y())
                                  .arg(control.expectedTargetRect.width())
                                  .arg(control.expectedTargetRect.height()),
                        QStringLiteral("%1,%2 %3x%4")
                            .arg(control.observedBoundingRect.x())
                            .arg(control.observedBoundingRect.y())
                            .arg(control.observedBoundingRect.width())
                            .arg(control.observedBoundingRect.height()),
                        QString::number(control.observedArea),
                        QString::number(control.observedPaintEvents),
                        QString::number(control.observedExcludedPaintEvents));
    }

    const QVariantMap handoff = report.handoff;
    if (handoff.value(QStringLiteral("enabled")).toBool()) {
        out << QStringLiteral("\n  hand-off    : delivered %1, stable %2, mutated after hand-off %3, "
                              "overflows %4\n")
                   .arg(handoff.value(QStringLiteral("delivered")).toInt())
                   .arg(handoff.value(QStringLiteral("stable")).toInt())
                   .arg(handoff.value(QStringLiteral("mutated")).toInt())
                   .arg(handoff.value(QStringLiteral("droppedOverflow")).toInt());
    }

    for (const QString &note : report.observations)
        out << QStringLiteral("  observation : %1\n").arg(note);
    for (const QString &warning : report.warnings)
        out << QStringLiteral("  WARNING     : %1\n").arg(warning);
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

    QVector<SampleDescriptor> registry = sampleRegistry();

    if (options.list) {
        QTextStream out(stdout);
        out << "capture cases:\n";
        for (const SampleDescriptor &descriptor : registry) {
            out << QStringLiteral("  %1  %2  [%3]\n")
                       .arg(descriptor.id, -14)
                       .arg(descriptor.title, descriptor.targetFamily);
            for (const QString &required : descriptor.requiredEnv)
                out << QStringLiteral("      requires %1\n").arg(required);
        }
        out << "\nBuild/run notes are in spikes/capture/README.md\n";
        return 0;
    }

    QStringList problems = options.problems;
    const QVector<SampleDescriptor> selected =
        selectDescriptors(registry, options.samples, &problems);

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
        err << "error: no capture case selected\n";
        return 1;
    }

    // Resolve the process-wide environment before QApplication exists, and
    // detect contradictory requirements between selected cases.
    QVector<QPair<QString, QString>> requiredEnv;
    QSet<QString> skipped;
    for (const SampleDescriptor &descriptor : selected) {
        for (const QString &entry : descriptor.requiredEnv) {
            const int separator = entry.indexOf(QLatin1Char('='));
            if (separator <= 0)
                continue;
            const QString key = entry.left(separator);
            const QString value = entry.mid(separator + 1);

            bool conflict = false;
            for (const auto &existing : requiredEnv) {
                if (existing.first == key && existing.second != value)
                    conflict = true;
            }
            if (conflict) {
                skipped.insert(descriptor.id);
                continue;
            }
            bool alreadyPresent = false;
            for (const auto &existing : requiredEnv) {
                if (existing.first == key && existing.second == value)
                    alreadyPresent = true;
            }
            if (!alreadyPresent)
                requiredEnv.append({key, value});
        }
    }

    if (!options.rhi.isEmpty() && options.rhi != QLatin1String("default")) {
        for (auto &entry : requiredEnv) {
            if (entry.first == QLatin1String("QSG_RHI_BACKEND"))
                entry.second = options.rhi;
        }
        bool present = false;
        for (const auto &entry : requiredEnv) {
            if (entry.first == QLatin1String("QSG_RHI_BACKEND"))
                present = true;
        }
        if (!present)
            requiredEnv.append({QStringLiteral("QSG_RHI_BACKEND"), options.rhi});
    }

    for (const auto &entry : requiredEnv)
        qputenv(entry.first.toUtf8().constData(), entry.second.toUtf8());

    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("hyremote-capture-spike"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.0-spike"));

    const QVariantMap environment = hyremote::spike::environmentReport();

    QVariantList results;
    QVariantList skippedEntries;
    int failedRuns = 0;
    int executedRuns = 0;

    QTextStream out(stdout);
    out << QStringLiteral("HyRemote SPIKE-01 capture harness\n");
    out << QStringLiteral("Qt %1 on %2, QPA '%3', RHI '%4'\n")
               .arg(environment.value(QStringLiteral("qtRuntimeVersion")).toString(),
                    environment.value(QStringLiteral("productType")).toString(),
                    environment.value(QStringLiteral("platformName")).toString(),
                    environment.value(QStringLiteral("qsgRhiBackend")).toString());
    out.flush();

    for (const SampleDescriptor &descriptor : selected) {
        if (skipped.contains(descriptor.id)) {
            QVariantMap entry;
            entry.insert(QStringLiteral("sample"), descriptor.id);
            entry.insert(QStringLiteral("reason"),
                         QStringLiteral("required process environment conflicts with another "
                                        "selected case; run this case in its own invocation"));
            entry.insert(QStringLiteral("requiredEnv"), descriptor.requiredEnv);
            skippedEntries.append(entry);
            out << QStringLiteral("\n[suspended] %1: %2\n")
                       .arg(descriptor.id,
                            entry.value(QStringLiteral("reason")).toString());
            out.flush();
            continue;
        }

        for (int run = 0; run < options.runs; ++run) {
            hyremote::spike::RunConfig config;
            config.frames = options.frames;
            config.intervalMs = options.intervalMs;
            config.warmupFrames = options.warmupFrames;
            config.submitEvery = options.submitEvery;
            config.sink = options.sink;
            config.rhi = options.rhi;
            config.disabledPaths = options.disabledPaths;

            hyremote::spike::CaptureSample *sample = descriptor.create();
            const hyremote::spike::RunReport report =
                hyremote::spike::runSample(sample, config, run);
            delete sample;

            ++executedRuns;
            if (report.framesCaptured == 0)
                ++failedRuns;

            printRunSummary(report);
            results.append(hyremote::spike::runReportToVariant(report));
        }
    }

    if (!options.jsonPath.isEmpty()) {
        QVariantMap root;
        root.insert(QStringLiteral("schema"), QStringLiteral("hyremote-capture-spike/1"));
        root.insert(QStringLiteral("issue"), QStringLiteral("https://github.com/skawu/HyRemote/issues/3"));
        root.insert(QStringLiteral("environment"), environment);
        root.insert(QStringLiteral("arguments"), arguments.mid(1));
        root.insert(QStringLiteral("results"), results);
        root.insert(QStringLiteral("skippedCases"), skippedEntries);

        const QJsonDocument document(QJsonObject::fromVariantMap(root));
        QFile file(options.jsonPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream(stderr) << QStringLiteral("error: cannot write %1\n").arg(options.jsonPath);
            return 1;
        }
        file.write(document.toJson(QJsonDocument::Indented));
        file.close();

        out << QStringLiteral("\nreport written to %1\n").arg(options.jsonPath);
    }

    out << QStringLiteral("\nSUMMARY: %1 run(s) executed, %2 without any captured frame.\n")
               .arg(executedRuns)
               .arg(failedRuns);
    out.flush();

    return failedRuns == 0 ? 0 : 2;
}
