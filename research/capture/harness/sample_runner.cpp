#include "harness/sample_runner.h"

#include "harness/damage_tracker.h"
#include "harness/frame_sink.h"
#include "harness/process_metrics.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QScreen>
#include <QSurfaceFormat>
#include <QSysInfo>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace hyremote {
namespace spike {

namespace {

struct PathAccumulator
{
    PathStats stats;
    QVector<double> times;
};

QString rectToString(const QRect &rect)
{
    if (rect.isNull())
        return QStringLiteral("empty");
    return QStringLiteral("%1,%2 %3x%4")
        .arg(rect.x())
        .arg(rect.y())
        .arg(rect.width())
        .arg(rect.height());
}

QString imageFormatName(QImage::Format format)
{
    switch (format) {
    case QImage::Format_ARGB32:
        return QStringLiteral("ARGB32");
    case QImage::Format_ARGB32_Premultiplied:
        return QStringLiteral("ARGB32_Premultiplied");
    case QImage::Format_RGB32:
        return QStringLiteral("RGB32");
    case QImage::Format_RGB888:
        return QStringLiteral("RGB888");
    case QImage::Format_RGBX8888:
        return QStringLiteral("RGBX8888");
    case QImage::Format_RGBA8888:
        return QStringLiteral("RGBA8888");
    case QImage::Format_RGBA8888_Premultiplied:
        return QStringLiteral("RGBA8888_Premultiplied");
    case QImage::Format_Indexed8:
        return QStringLiteral("Indexed8");
    case QImage::Format_Grayscale8:
        return QStringLiteral("Grayscale8");
    default:
        return QStringLiteral("format_%1").arg(int(format));
    }
}

QString threadDescription(QThread *thread)
{
    if (!thread)
        return QStringLiteral("<null>");
    const QString name = thread->objectName();
    return name.isEmpty() ? QStringLiteral("<unnamed>") : name;
}

void appendUnique(QStringList &target, const QString &value)
{
    if (value.isEmpty())
        return;
    for (const QString &existing : target) {
        if (existing == value)
            return;
    }
    target.append(value);
}

PathAccumulator &accumulatorFor(QVector<PathAccumulator> &accumulators,
                                const QString &path,
                                bool primary)
{
    for (PathAccumulator &accumulator : accumulators) {
        if (accumulator.stats.path == path)
            return accumulator;
    }
    PathAccumulator fresh;
    fresh.stats.path = path;
    fresh.stats.primary = primary;
    accumulators.append(fresh);
    return accumulators.last();
}

double percentile(const QVector<double> &sortedValues, double fraction)
{
    if (sortedValues.isEmpty())
        return 0.0;
    const int index = qBound(0,
                             int(std::ceil(fraction * double(sortedValues.size()))) - 1,
                             sortedValues.size() - 1);
    return sortedValues.at(index);
}

double mean(const QVector<double> &values)
{
    if (values.isEmpty())
        return 0.0;
    double total = 0.0;
    for (double value : values)
        total += value;
    return total / double(values.size());
}

// Least-squares slope of `values` against the sample index.
double slopePer100(const QVector<double> &values)
{
    const int count = values.size();
    if (count < 2)
        return 0.0;

    const double meanX = double(count - 1) / 2.0;
    const double meanY = mean(values);
    double numerator = 0.0;
    double denominator = 0.0;
    for (int i = 0; i < count; ++i) {
        const double dx = double(i) - meanX;
        numerator += dx * (values.at(i) - meanY);
        denominator += dx * dx;
    }
    if (denominator == 0.0)
        return 0.0;
    return (numerator / denominator) * 100.0;
}

void appendGlInfo(QVariantMap &report)
{
    QOpenGLContext context;
    if (!context.create()) {
        report.insert(QStringLiteral("glInfo"), QStringLiteral("context creation failed"));
        return;
    }

    QOffscreenSurface surface;
    surface.setFormat(context.format());
    surface.create();
    if (!surface.isValid()) {
        report.insert(QStringLiteral("glInfo"), QStringLiteral("offscreen surface creation failed"));
        return;
    }

    if (!context.makeCurrent(&surface)) {
        report.insert(QStringLiteral("glInfo"), QStringLiteral("makeCurrent failed"));
        return;
    }

    QOpenGLFunctions *functions = context.functions();
    const auto glString = [functions](GLenum name) {
        const GLubyte *value = functions->glGetString(name);
        return value ? QString::fromLatin1(reinterpret_cast<const char *>(value))
                     : QStringLiteral("<null>");
    };

    QVariantMap info;
    info.insert(QStringLiteral("vendor"), glString(GL_VENDOR));
    info.insert(QStringLiteral("renderer"), glString(GL_RENDERER));
    info.insert(QStringLiteral("version"), glString(GL_VERSION));
    info.insert(QStringLiteral("shadingLanguage"), glString(GL_SHADING_LANGUAGE_VERSION));
    info.insert(QStringLiteral("contextFormat"),
                QStringLiteral("%1.%2 %3")
                    .arg(context.format().majorVersion())
                    .arg(context.format().minorVersion())
                    .arg(context.format().profile() == QSurfaceFormat::CoreProfile
                             ? QStringLiteral("core")
                             : (context.format().profile() == QSurfaceFormat::CompatibilityProfile
                                    ? QStringLiteral("compatibility")
                                    : QStringLiteral("no-profile"))));
    report.insert(QStringLiteral("glInfo"), info);

    context.doneCurrent();
}

}  // namespace

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
    report.insert(QStringLiteral("cpuArchitecture"), QSysInfo::currentCpuArchitecture());
    report.insert(QStringLiteral("platformName"),
                  QGuiApplication::platformName().isEmpty() ? QStringLiteral("<none>")
                                                            : QGuiApplication::platformName());
    report.insert(QStringLiteral("qsgRhiBackend"),
                  qEnvironmentVariableIsSet("QSG_RHI_BACKEND")
                      ? qEnvironmentVariable("QSG_RHI_BACKEND")
                      : QStringLiteral("<default>"));

    const QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    report.insert(QStringLiteral("defaultSurfaceFormat"),
                  QStringLiteral("%1.%2 profile=%3 swapInterval=%4 samples=%5")
                      .arg(format.majorVersion())
                      .arg(format.minorVersion())
                      .arg(int(format.profile()))
                      .arg(format.swapInterval())
                      .arg(format.samples()));

    QVariantList screens;
    for (const QScreen *screen : QGuiApplication::screens()) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), screen->name());
        entry.insert(QStringLiteral("geometry"),
                     QStringLiteral("%1x%2+%3+%4")
                         .arg(screen->geometry().width())
                         .arg(screen->geometry().height())
                         .arg(screen->geometry().x())
                         .arg(screen->geometry().y()));
        entry.insert(QStringLiteral("devicePixelRatio"), screen->devicePixelRatio());
        entry.insert(QStringLiteral("logicalDpi"), screen->logicalDotsPerInch());
        screens.append(entry);
    }
    report.insert(QStringLiteral("screens"), screens);

    appendGlInfo(report);

    const QString unsupported = processMetricsUnsupportedReason();
    if (!unsupported.isEmpty())
        report.insert(QStringLiteral("processMetricsUnsupported"), unsupported);

    return report;
}

RunReport runSample(CaptureSample *sample, const RunConfig &config, int runIndex)
{
    RunReport report;
    report.sampleId = sample->id();
    report.title = sample->title();
    report.targetFamily = sample->targetFamily();
    report.runIndex = runIndex;
    report.framesRequested = config.frames;
    report.intervalMs = config.intervalMs;

    sample->setDisabledPaths(config.disabledPaths);

    sample->prepare();
    sample->show();

    DamageTracker damage;
    // Bind the tracker to the shared target so that child paint regions can be
    // mapped into the target's coordinate system instead of being unioned in
    // their own coordinate systems.
    damage.start(sample->damageTargetWidget());
    sample->setDamageTracker(&damage);

    FrameSink sink;
    if (config.sink)
        sink.start();

    // GUI responsiveness probe: how much does capture work delay the GUI
    // thread's own timers? This is a serviceable automated proxy for "does the
    // local UI visibly stutter while a remote client is served?".
    QVector<double> latencyMs;
    QElapsedTimer latencyClock;
    latencyClock.start();
    double nextExpectedMs = 0.0;
    const int latencyInterval = qMax(1, config.intervalMs);

    QTimer latencyTimer;
    latencyTimer.setTimerType(Qt::PreciseTimer);
    latencyTimer.setInterval(latencyInterval);
    QObject::connect(&latencyTimer, &QTimer::timeout, [&latencyMs, &latencyClock, &nextExpectedMs,
                                                      latencyInterval] {
        const double nowMs = double(latencyClock.nsecsElapsed()) / 1.0e6;
        latencyMs.append(qMax(0.0, nowMs - nextExpectedMs));
        nextExpectedMs = nowMs + double(latencyInterval);
    });
    latencyTimer.start();

    QVector<PathAccumulator> accumulators;

    // Warm-up: the first captures pay for lazy scene graph, GL context and
    // buffer initialization. They are recorded as "firstNs" but excluded from
    // the steady-state statistics.
    bool firstAttemptsRecorded = false;
    for (int i = 0; i < config.warmupFrames; ++i) {
        sample->tick(i);
        pumpEvents(config.intervalMs);
        const QVector<CaptureOutcome> outcomes = sample->captureAll();
        if (!firstAttemptsRecorded) {
            for (const CaptureOutcome &outcome : outcomes) {
                PathAccumulator &accumulator = accumulatorFor(accumulators, outcome.path, outcome.primary);
                accumulator.stats.firstNs = double(outcome.elapsedNs);
                accumulator.stats.producerReusesBuffer = outcome.producerReusesBuffer;
                accumulator.stats.callerThread = outcome.callerThread;
                appendUnique(accumulator.stats.notes, outcome.notes);
            }
            firstAttemptsRecorded = true;
        }
    }

    damage.takeSnapshot();
    latencyMs.clear();
    latencyClock.restart();
    nextExpectedMs = 0.0;

    QVector<double> rssSeries;
    qint64 pumpNsTotal = 0;
    qint64 captureLoopNsTotal = 0;
    qint64 frameArea = 0;
    qint64 damageTargetArea = 0;
    qint64 appDamageArea = 0;
    qint64 captureDamageArea = 0;
    quint64 mappedPaintEvents = 0;
    quint64 excludedPaintEvents = 0;
    quint64 emptyDamageRegions = 0;
    quint64 updateRequests = 0;
    int observedObjects = 0;
    int framesCaptured = 0;

    const ProcessMetrics startMetrics = sampleProcessMetrics();
    QElapsedTimer wallTimer;
    wallTimer.start();

    QElapsedTimer phaseTimer;
    for (int i = 0; i < config.frames; ++i) {
        sample->tick(i);
        // Let the application render normally first, so that damage observed
        // here is application-driven rather than capture-driven.
        phaseTimer.start();
        pumpEvents(config.intervalMs);
        pumpNsTotal += phaseTimer.nsecsElapsed();
        const DamageSnapshot appDamage = damage.takeSnapshot();

        phaseTimer.restart();
        const QVector<CaptureOutcome> outcomes = sample->captureAll();
        captureLoopNsTotal += phaseTimer.nsecsElapsed();
        const DamageSnapshot captureDamage = damage.takeSnapshot();

        mappedPaintEvents += appDamage.paintEvents + captureDamage.paintEvents;
        excludedPaintEvents += appDamage.excludedPaintEvents + captureDamage.excludedPaintEvents;
        emptyDamageRegions += appDamage.emptyRegions + captureDamage.emptyRegions;
        updateRequests += appDamage.updateRequests + captureDamage.updateRequests;
        observedObjects = qMax(observedObjects, appDamage.observedWidgets);
        damageTargetArea = qMax(damageTargetArea, appDamage.targetArea);

        bool anySuccess = false;
        for (const CaptureOutcome &outcome : outcomes) {
            PathAccumulator &accumulator = accumulatorFor(accumulators, outcome.path, outcome.primary);
            ++accumulator.stats.attempts;
            accumulator.stats.producerReusesBuffer = outcome.producerReusesBuffer;
            accumulator.stats.callerThread = outcome.callerThread;
            appendUnique(accumulator.stats.notes, outcome.notes);

            if (outcome.storageProbeAvailable) {
                ++accumulator.stats.storageProbes;
                if (outcome.storageReplacedDuringCapture)
                    ++accumulator.stats.storageReplacements;
                const QString first = QStringLiteral("0x%1").arg(outcome.storageAddressBefore, 0, 16);
                const QString last = QStringLiteral("0x%1").arg(outcome.storageAddressAfter, 0, 16);
                if (accumulator.stats.storageAddressFirst.isEmpty())
                    accumulator.stats.storageAddressFirst = first;
                accumulator.stats.storageAddressLast = last;
            }

            if (outcome.ok && !outcome.frame.isNull()) {
                ++accumulator.stats.successes;
                accumulator.times.append(double(outcome.elapsedNs));
                accumulator.stats.frameFormat = imageFormatName(outcome.frame.format());
                accumulator.stats.frameSize =
                    QStringLiteral("%1x%2 dpr=%3")
                        .arg(outcome.frame.width())
                        .arg(outcome.frame.height())
                        .arg(outcome.frame.devicePixelRatio());
                // Fallback damage denominator when the sample reports no shared
                // QWidget target: the largest captured surface. A path that
                // returns only a sub-region (for example
                // QQuickWidget::grabFramebuffer()) must not make the ratio
                // exceed 100 %.
                const qint64 area = qint64(outcome.frame.width()) * qint64(outcome.frame.height());
                frameArea = qMax(frameArea, area);
                anySuccess = true;
            }
        }
        if (anySuccess)
            ++framesCaptured;

        appDamageArea += appDamage.area();
        captureDamageArea += captureDamage.area();

        rssSeries.append(double(sampleProcessMetrics().rssBytes));

        if (config.sink && config.submitEvery > 0 && (i % config.submitEvery) == 0) {
            for (const CaptureOutcome &outcome : outcomes) {
                if (outcome.ok && !outcome.frame.isNull())
                    sink.submit(outcome.path, outcome.frame, frameChecksum(outcome.frame),
                                outcome.borrowedView);
            }
        }
    }

    const double wallSeconds = double(wallTimer.nsecsElapsed()) / 1.0e9;
    const ProcessMetrics endMetrics = sampleProcessMetrics();

    // Deterministic damage-mapping controls, executed with captures stopped so
    // that capture-induced repaints cannot be attributed to a control step.
    const QVector<DamageControlResult> damageControls = sample->runDamageControls();
    for (const DamageControlResult &control : damageControls) {
        if (!control.passed) {
            report.warnings.append(
                QStringLiteral("damage mapping control failed: %1 (expected %2, observed %3, "
                               "area %4)")
                    .arg(control.description,
                         control.expectEmpty
                             ? QStringLiteral("no damage")
                             : QStringLiteral("[%1,%2 %3x%4]")
                                   .arg(control.expectedTargetRect.x())
                                   .arg(control.expectedTargetRect.y())
                                   .arg(control.expectedTargetRect.width())
                                   .arg(control.expectedTargetRect.height()),
                         QStringLiteral("[%1,%2 %3x%4]")
                             .arg(control.observedBoundingRect.x())
                             .arg(control.observedBoundingRect.y())
                             .arg(control.observedBoundingRect.width())
                             .arg(control.observedBoundingRect.height()),
                         QString::number(control.observedArea)));
        }
    }

    // Snapshot everything the sample and the sink can report before they are
    // torn down; after shutdown() the target objects no longer exist.
    const QVariantMap handoffReport = sink.report();
    const QVariantMap sampleInfo = sample->info();
    const QStringList sampleObservations = sample->observations();

    latencyTimer.stop();
    sample->shutdown();
    damage.stop();
    sink.stop();

    report.sampleInfo = sampleInfo;
    report.observations = sampleObservations;
    report.framesCaptured = framesCaptured;
    report.wallSeconds = wallSeconds;
    report.achievedFps = wallSeconds > 0.0 ? double(framesCaptured) / wallSeconds : 0.0;
    report.cpuSeconds = endMetrics.cpuSeconds - startMetrics.cpuSeconds;
    report.cpuPercentOfOneCore =
        wallSeconds > 0.0 ? 100.0 * report.cpuSeconds / wallSeconds : 0.0;
    report.rssStartBytes = rssSeries.isEmpty() ? 0 : quint64(rssSeries.first());
    report.rssEndBytes = rssSeries.isEmpty() ? 0 : quint64(rssSeries.last());
    report.rssPeakBytes = endMetrics.peakRssBytes;
    report.rssSlopeBytesPer100Frames = slopePer100(rssSeries);

    const double measuredFrames = double(qMax(1, config.frames));
    report.avgPumpMs = double(pumpNsTotal) / 1.0e6 / measuredFrames;
    report.avgCaptureLoopMs = double(captureLoopNsTotal) / 1.0e6 / measuredFrames;

    report.guiLatencyAvgMs = mean(latencyMs);
    QVector<double> sortedLatency = latencyMs;
    std::sort(sortedLatency.begin(), sortedLatency.end());
    report.guiLatencyP95Ms = percentile(sortedLatency, 0.95);
    report.guiLatencyMaxMs = sortedLatency.isEmpty() ? 0.0 : sortedLatency.last();

    // The damage denominator is the shared target rect when the sample reports
    // one, because the mapped damage region lives in that coordinate system.
    // Otherwise fall back to the largest captured surface.
    const qint64 damageDenominator = damageTargetArea > 0 ? damageTargetArea : frameArea;
    if (damageDenominator > 0) {
        report.avgDamageRatio =
            double(appDamageArea) / double(damageDenominator * qMax(1, config.frames));
        report.avgCaptureDamageRatio =
            double(captureDamageArea) / double(damageDenominator * qMax(1, config.frames));
    }
    report.damageControls = damageControls;
    report.damageTargetArea = damageTargetArea;
    report.mappedPaintEvents = mappedPaintEvents;
    report.excludedPaintEvents = excludedPaintEvents;
    report.emptyDamageRegions = emptyDamageRegions;
    report.updateRequests = updateRequests;
    report.observedObjects = observedObjects;

    for (PathAccumulator &accumulator : accumulators) {
        QVector<double> sortedTimes = accumulator.times;
        std::sort(sortedTimes.begin(), sortedTimes.end());
        accumulator.stats.avgNs = mean(accumulator.times);
        accumulator.stats.p50Ns = percentile(sortedTimes, 0.50);
        accumulator.stats.p95Ns = percentile(sortedTimes, 0.95);
        accumulator.stats.maxNs = sortedTimes.isEmpty() ? 0.0 : sortedTimes.last();

        if (accumulator.stats.successes == 0)
            report.warnings.append(QStringLiteral("capture path '%1' never produced a frame")
                                       .arg(accumulator.stats.path));

        report.paths.append(accumulator.stats);
    }

    report.handoff = handoffReport;

    // Storage-identity aggregate across all probed paths.
    QStringList storageNotes;
    for (const PathStats &path : report.paths) {
        if (path.storageProbes == 0)
            continue;
        storageNotes.append(QStringLiteral("'%1' probed %2 captures and replaced its pixel storage "
                                           "%3 times")
                                .arg(path.path)
                                .arg(path.storageProbes)
                                .arg(path.storageReplacements));
    }
    if (!storageNotes.isEmpty()) {
        report.observations.append(
            QStringLiteral("Storage-identity probe (real backing-store address from "
                           "QImage::constBits(), not QImage::cacheKey()): %1.")
                .arg(storageNotes.join(QStringLiteral("; "))));
    }

    // Data-driven damage conclusion for this target.
    if (framesCaptured > 0) {
        if (damageTargetArea == 0) {
            report.observations.append(
                QStringLiteral("Damage probe: this target family has no QWidget root whose "
                               "coordinate system damage could be mapped into, so no "
                               "region-carrying paint event can be attributed to the shared "
                               "target; only %1 application-wide update requests (QEvent::"
                               "UpdateRequest carries no geometry) and %2 excluded paint events "
                               "were observed. Region-level damage is not observable through "
                               "public APIs for this target.")
                    .arg(updateRequests)
                    .arg(excludedPaintEvents));
        } else if (mappedPaintEvents == 0) {
            report.observations.append(
                QStringLiteral("Damage probe: no region-carrying paint event could be mapped into "
                               "the shared target rect of %1 px^2, although %2 update requests "
                               "were observed, so no usable damage stream was obtained for this "
                               "target in this run.")
                    .arg(damageTargetArea)
                    .arg(updateRequests));
        } else {
            report.observations.append(
                QStringLiteral("Damage probe (mapped into the shared target coordinate system): %1 "
                               "paint events from the target subtree were accepted, %2 paint "
                               "events were excluded because they belong to separate top-level "
                               "windows or non-widget objects, and %3 mapped events fell outside "
                               "the target rect. Application repaints covered %4 % of the shared "
                               "target rect per frame on average, while the capture calls added "
                               "%5 %. This is an observable child-region stream after mapping and "
                               "clipping; it is not yet a transport-ready damage protocol.")
                    .arg(mappedPaintEvents)
                    .arg(excludedPaintEvents)
                    .arg(emptyDamageRegions)
                    .arg(report.avgDamageRatio * 100.0, 0, 'f', 1)
                    .arg(report.avgCaptureDamageRatio * 100.0, 0, 'f', 1));
        }
    }

    if (config.sink && report.handoff.value(QStringLiteral("delivered")).toInt() == 0
        && framesCaptured > 0) {
        report.warnings.append(
            QStringLiteral("frame hand-off probe delivered no frames; ownership evidence is incomplete"));
    }

    return report;
}

QVariantMap runReportToVariant(const RunReport &report)
{
    QVariantMap map;
    map.insert(QStringLiteral("sample"), report.sampleId);
    map.insert(QStringLiteral("title"), report.title);
    map.insert(QStringLiteral("targetFamily"), report.targetFamily);
    map.insert(QStringLiteral("run"), report.runIndex);
    map.insert(QStringLiteral("sampleInfo"), report.sampleInfo);

    QVariantMap timing;
    timing.insert(QStringLiteral("framesRequested"), report.framesRequested);
    timing.insert(QStringLiteral("framesCaptured"), report.framesCaptured);
    timing.insert(QStringLiteral("intervalMs"), report.intervalMs);
    timing.insert(QStringLiteral("wallSeconds"), report.wallSeconds);
    timing.insert(QStringLiteral("achievedFps"), report.achievedFps);
    timing.insert(QStringLiteral("cpuSeconds"), report.cpuSeconds);
    timing.insert(QStringLiteral("cpuPercentOfOneCore"), report.cpuPercentOfOneCore);
    timing.insert(QStringLiteral("guiLatencyAvgMs"), report.guiLatencyAvgMs);
    timing.insert(QStringLiteral("guiLatencyP95Ms"), report.guiLatencyP95Ms);
    timing.insert(QStringLiteral("guiLatencyMaxMs"), report.guiLatencyMaxMs);
    timing.insert(QStringLiteral("avgPumpMs"), report.avgPumpMs);
    timing.insert(QStringLiteral("avgCaptureLoopMs"), report.avgCaptureLoopMs);
    map.insert(QStringLiteral("timing"), timing);

    QVariantMap memory;
    memory.insert(QStringLiteral("rssStartBytes"), report.rssStartBytes);
    memory.insert(QStringLiteral("rssEndBytes"), report.rssEndBytes);
    memory.insert(QStringLiteral("rssPeakBytes"), report.rssPeakBytes);
    memory.insert(QStringLiteral("rssSlopeBytesPer100Frames"), report.rssSlopeBytesPer100Frames);
    map.insert(QStringLiteral("memory"), memory);

    QVariantMap damage;
    damage.insert(QStringLiteral("ratioCoordinateSystem"),
                  QStringLiteral("shared target QWidget (mapped and clipped)"));
    damage.insert(QStringLiteral("targetAreaPx"), report.damageTargetArea);
    damage.insert(QStringLiteral("avgAppDamageRatio"), report.avgDamageRatio);
    damage.insert(QStringLiteral("avgCaptureDamageRatio"), report.avgCaptureDamageRatio);
    damage.insert(QStringLiteral("mappedPaintEvents"), report.mappedPaintEvents);
    damage.insert(QStringLiteral("excludedPaintEvents"), report.excludedPaintEvents);
    damage.insert(QStringLiteral("emptyMappedRegions"), report.emptyDamageRegions);
    damage.insert(QStringLiteral("updateRequests"), report.updateRequests);
    damage.insert(QStringLiteral("observedObjects"), report.observedObjects);
    map.insert(QStringLiteral("damage"), damage);

    QVariantList damageControls;
    for (const DamageControlResult &control : report.damageControls) {
        QVariantMap entry;
        entry.insert(QStringLiteral("description"), control.description);
        entry.insert(QStringLiteral("expectEmpty"), control.expectEmpty);
        entry.insert(QStringLiteral("expectedTargetRect"), rectToString(control.expectedTargetRect));
        entry.insert(QStringLiteral("observedBoundingRect"), rectToString(control.observedBoundingRect));
        entry.insert(QStringLiteral("observedAreaPx"), control.observedArea);
        entry.insert(QStringLiteral("observedPaintEvents"), control.observedPaintEvents);
        entry.insert(QStringLiteral("observedExcludedPaintEvents"),
                     control.observedExcludedPaintEvents);
        entry.insert(QStringLiteral("passed"), control.passed);
        damageControls.append(entry);
    }
    map.insert(QStringLiteral("damageMappingControls"), damageControls);

    QVariantList paths;
    for (const PathStats &path : report.paths) {
        QVariantMap entry;
        entry.insert(QStringLiteral("path"), path.path);
        entry.insert(QStringLiteral("primary"), path.primary);
        entry.insert(QStringLiteral("attempts"), path.attempts);
        entry.insert(QStringLiteral("successes"), path.successes);
        entry.insert(QStringLiteral("firstNs"), path.firstNs);
        entry.insert(QStringLiteral("avgNs"), path.avgNs);
        entry.insert(QStringLiteral("p50Ns"), path.p50Ns);
        entry.insert(QStringLiteral("p95Ns"), path.p95Ns);
        entry.insert(QStringLiteral("maxNs"), path.maxNs);
        entry.insert(QStringLiteral("frameFormat"), path.frameFormat);
        entry.insert(QStringLiteral("frameSize"), path.frameSize);
        entry.insert(QStringLiteral("producerReusesBuffer"), path.producerReusesBuffer);
        entry.insert(QStringLiteral("callerThread"), path.callerThread);
        entry.insert(QStringLiteral("storageProbes"), path.storageProbes);
        entry.insert(QStringLiteral("storageReplacements"), path.storageReplacements);
        entry.insert(QStringLiteral("storageAddressFirst"), path.storageAddressFirst);
        entry.insert(QStringLiteral("storageAddressLast"), path.storageAddressLast);
        entry.insert(QStringLiteral("notes"), path.notes);
        paths.append(entry);
    }
    map.insert(QStringLiteral("paths"), paths);

    map.insert(QStringLiteral("handoff"), report.handoff);
    map.insert(QStringLiteral("observations"), report.observations);
    map.insert(QStringLiteral("warnings"), report.warnings);
    return map;
}

}  // namespace spike
}  // namespace hyremote
