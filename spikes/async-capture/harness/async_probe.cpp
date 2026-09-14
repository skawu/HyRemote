#include "harness/async_probe.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMetaObject>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QSet>
#include <QSharedPointer>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>

namespace hyremote {
namespace asyncspike {

namespace {

QElapsedTimer &monotonicClock()
{
    static QElapsedTimer clock = [] {
        QElapsedTimer timer;
        timer.start();
        return timer;
    }();
    return clock;
}

qint64 monotonicNs()
{
    return monotonicClock().nsecsElapsed();
}

void pumpEvents(int ms)
{
    if (ms <= 0) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
        return;
    }
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::AllEvents);
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

double percentileOfSorted(const QVector<double> &sorted, double fraction)
{
    if (sorted.isEmpty())
        return 0.0;
    const int index = qBound(0,
                             int(std::ceil(fraction * double(sorted.size()))) - 1,
                             sorted.size() - 1);
    return sorted.at(index);
}

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

QString sizeToString(const QImage &image)
{
    if (image.isNull())
        return QStringLiteral("<null>");
    return QStringLiteral("%1x%2 dpr=%3")
        .arg(image.width())
        .arg(image.height())
        .arg(image.devicePixelRatio());
}

QString colorToString(const QColor &color)
{
    if (!color.isValid())
        return QStringLiteral("<invalid>");
    return QStringLiteral("#%1%2%3%4")
        .arg(color.red(), 2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(), 2, 16, QLatin1Char('0'))
        .arg(color.alpha(), 2, 16, QLatin1Char('0'));
}

bool colorMatches(const QColor &color, const QColor &expected, int tolerance)
{
    if (!color.isValid())
        return false;
    return qAbs(color.red() - expected.red()) <= tolerance
           && qAbs(color.green() - expected.green()) <= tolerance
           && qAbs(color.blue() - expected.blue()) <= tolerance;
}

// Requests one asynchronous capture and runs the event loop until it completes.
QImage awaitGrab(QQuickItem *item, int timeoutMs, QString *error)
{
    if (error)
        error->clear();
    if (!item) {
        if (error)
            *error = QStringLiteral("target item is null");
        return QImage();
    }

    const QSharedPointer<QQuickItemGrabResult> result = item->grabToImage();
    if (result.isNull()) {
        if (error)
            *error = QStringLiteral("grabToImage() returned a null result");
        return QImage();
    }

    if (result->image().isNull()) {
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(result.data(), &QQuickItemGrabResult::ready, &loop, &QEventLoop::quit);
        timeout.start(timeoutMs);
        loop.exec(QEventLoop::AllEvents);
        if (result->image().isNull() && error)
            *error = QStringLiteral("grab did not complete within %1 ms").arg(timeoutMs);
    }
    return result->image();
}

struct ImageDiff
{
    bool sizeMatch = false;
    double meanAbsDiff = 0.0;
    int maxChannelDiff = 0;
    double differingPct = 0.0;
    double differingOutsideIgnoreRectPct = 0.0;
};

ImageDiff compareImages(const QImage &a, const QImage &b, const QRect &ignoreRectDevice)
{
    ImageDiff diff;
    if (a.isNull() || b.isNull() || a.size() != b.size())
        return diff;

    diff.sizeMatch = true;
    const QImage left = a.convertToFormat(QImage::Format_ARGB32);
    const QImage right = b.convertToFormat(QImage::Format_ARGB32);

    constexpr int kStride = 3;
    constexpr int kTolerance = 2;
    qint64 sampled = 0;
    qint64 differing = 0;
    qint64 differingOutside = 0;
    qint64 totalChannelDiff = 0;
    int maximum = 0;

    for (int y = 0; y < left.height(); y += kStride) {
        const QRgb *rowLeft = reinterpret_cast<const QRgb *>(left.constScanLine(y));
        const QRgb *rowRight = reinterpret_cast<const QRgb *>(right.constScanLine(y));
        for (int x = 0; x < left.width(); x += kStride) {
            const int dr = qAbs(qRed(rowLeft[x]) - qRed(rowRight[x]));
            const int dg = qAbs(qGreen(rowLeft[x]) - qGreen(rowRight[x]));
            const int db = qAbs(qBlue(rowLeft[x]) - qBlue(rowRight[x]));
            const int largest = qMax(dr, qMax(dg, db));
            totalChannelDiff += dr + dg + db;
            maximum = qMax(maximum, largest);
            ++sampled;
            if (largest > kTolerance) {
                ++differing;
                if (!ignoreRectDevice.contains(x, y))
                    ++differingOutside;
            }
        }
    }

    diff.meanAbsDiff = sampled > 0 ? double(totalChannelDiff) / double(sampled * 3) : 0.0;
    diff.maxChannelDiff = maximum;
    diff.differingPct = sampled > 0 ? 100.0 * double(differing) / double(sampled) : 0.0;
    diff.differingOutsideIgnoreRectPct =
        sampled > 0 ? 100.0 * double(differingOutside) / double(sampled) : 0.0;
    return diff;
}

QRect deviceRectForTargetPoint(const QRect &contentItemRect,
                               const QImage &image,
                               QQuickItem *target,
                               const SceneHost &scene)
{
    if (contentItemRect.isEmpty() || image.isNull() || !target)
        return QRect();

    const QPoint topLeft = scene.pointInTarget(contentItemRect.topLeft(), target);
    const QPoint bottomRight = scene.pointInTarget(contentItemRect.bottomRight(), target);
    const qreal dpr = image.devicePixelRatio() > 0.0 ? image.devicePixelRatio() : 1.0;
    return QRect(QPoint(qRound(topLeft.x() * dpr), qRound(topLeft.y() * dpr)),
                 QPoint(qRound(bottomRight.x() * dpr), qRound(bottomRight.y() * dpr)));
}

// ---------------------------------------------------------------------------
// Warning capture
// ---------------------------------------------------------------------------

QVector<QString> *g_warningSink = nullptr;
QtMessageHandler g_previousHandler = nullptr;

void spikeMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (g_warningSink)
        g_warningSink->append(message);

    if (type == QtCriticalMsg || type == QtFatalMsg) {
        if (g_previousHandler)
            g_previousHandler(type, context, message);
        else
            std::fprintf(stderr, "async-spike: %s\n", qPrintable(message));
    }
}

// ---------------------------------------------------------------------------
// Async pipeline
// ---------------------------------------------------------------------------

struct AsyncRequest
{
    int index = 0;
    int tickAtRequest = 0;
    qint64 requestedNs = 0;
    qint64 readyNs = 0;
    qint64 releaseAfterNs = 0;
    QSharedPointer<QQuickItemGrabResult> result;
    QImage image;
    int tickInImage = -1;
    int fboCounterInImage = -1;
    bool decoded = false;
};

struct PipelineState
{
    SceneHost *scene = nullptr;
    QQuickItem *target = nullptr;
    ProbeConfig config;
    QVector<QSharedPointer<AsyncRequest>> inFlight;
    QVector<QSharedPointer<AsyncRequest>> held;
    QVector<double> latenciesMs;
    QVector<double> tickLags;
    QSet<int> ticksSeen;
    QSet<int> fboCountersSeen;
    int fboCounterFirst = -1;
    int fboCounterLast = -1;
    int nextIndex = 0;
    int completedCount = 0;
    int completedInRequestOrder = 0;
    int lastCompletedIndex = -1;
    int outOfOrder = 0;
    int duplicateCompletions = 0;
    int nullImages = 0;
    int failedToStart = 0;
    int maxInFlightObserved = 0;
    quint64 retainedBytes = 0;
    quint64 peakRetainedBytes = 0;
    qint64 lastFrameBytes = 0;
    int maxHeldObserved = 0;
    int throttledSlices = 0;
    bool timedOut = false;
};

void decodeRequest(PipelineState &state, const QSharedPointer<AsyncRequest> &request)
{
    if (request->image.isNull())
        return;
    request->tickInImage = state.scene->decodeTickAt(request->image, state.target);
    request->decoded = true;
    if (request->tickInImage >= 0) {
        if (state.ticksSeen.contains(request->tickInImage))
            ++state.duplicateCompletions;
        state.ticksSeen.insert(request->tickInImage);
    }
    state.tickLags.append(double(request->tickInImage - request->tickAtRequest));
    state.lastFrameBytes = qint64(request->image.sizeInBytes());

    request->fboCounterInImage = state.scene->decodeFboCounterAt(request->image, state.target);
    if (request->fboCounterInImage >= 0) {
        state.fboCountersSeen.insert(request->fboCounterInImage);
        if (state.fboCounterFirst < 0)
            state.fboCounterFirst = request->fboCounterInImage;
        state.fboCounterLast = request->fboCounterInImage;
    }
}

}  // namespace

QString grabTargetName(GrabTarget target)
{
    switch (target) {
    case GrabTarget::ContentItem:
        return QStringLiteral("QQuickWindow::contentItem()");
    case GrabTarget::RootItem:
        return QStringLiteral("QML root item");
    }
    return QStringLiteral("<unknown>");
}

WarningRecorder::WarningRecorder()
{
    g_warningSink = new QVector<QString>();
    m_installed = true;
    g_previousHandler = qInstallMessageHandler(spikeMessageHandler);
}

WarningRecorder::~WarningRecorder()
{
    if (m_installed)
        qInstallMessageHandler(g_previousHandler);
    g_previousHandler = nullptr;
    delete g_warningSink;
    g_warningSink = nullptr;
}

QStringList WarningRecorder::takeWarnings()
{
    QStringList list;
    if (!g_warningSink)
        return list;
    list = *g_warningSink;
    g_warningSink->clear();
    return list;
}

void WarningRecorder::clear()
{
    if (g_warningSink)
        g_warningSink->clear();
}

PipelineResult runPipeline(SceneHost &scene, const ProbeConfig &config)
{
    PipelineResult result;
    result.targetName = grabTargetName(config.target);
    result.requests = config.requests;
    result.maxInFlight = config.maxInFlight;
    result.intervalMs = config.intervalMs;
    result.consumerDelayMs = config.consumerDelayMs;
    result.pacedRequests = config.pacedRequests;

    QQuickItem *target = config.target == GrabTarget::ContentItem ? scene.contentItem()
                                                                 : scene.sceneRootItem();
    if (!target) {
        result.notes.append(QStringLiteral("no asynchronous capture target is available for this scene"));
        return result;
    }

    // Frame counter: the scene graph renders on a different thread.
    std::atomic<int> frames{0};
    QMetaObject::Connection frameConnection;
    if (QQuickWindow *window = scene.window()) {
        frameConnection = QObject::connect(
            window, &QQuickWindow::beforeRendering,
            window, [&frames] { frames.fetch_add(1, std::memory_order_relaxed); },
            Qt::DirectConnection);
    }

    // GUI responsiveness proxy: jitter of a precise 16 ms timer.
    QVector<double> latencyMs;
    QElapsedTimer latencyClock;
    latencyClock.start();
    double nextExpectedMs = 0.0;
    QTimer latencyTimer;
    latencyTimer.setTimerType(Qt::PreciseTimer);
    latencyTimer.setInterval(16);
    QObject::connect(&latencyTimer, &QTimer::timeout, &latencyTimer,
                     [&latencyMs, &latencyClock, &nextExpectedMs] {
                         const double nowMs = double(latencyClock.nsecsElapsed()) / 1.0e6;
                         latencyMs.append(qMax(0.0, nowMs - nextExpectedMs));
                         nextExpectedMs = nowMs + 16.0;
                     });
    latencyTimer.start();

    const ProcessMetrics startMetrics = sampleProcessMetrics();
    QVector<double> rssSeries;

    auto state = QSharedPointer<PipelineState>::create();
    state->scene = &scene;
    state->target = target;
    state->config = config;

    QElapsedTimer wall;
    wall.start();

    while (state->completedCount < config.requests) {
        const qint64 nowNs = monotonicNs();

        // Release frames the slow consumer has finished with.
        for (int i = state->held.size() - 1; i >= 0; --i) {
            if (state->held.at(i)->releaseAfterNs <= nowNs) {
                state->retainedBytes -= state->held.at(i)->image.sizeInBytes();
                state->held.at(i)->image = QImage();
                state->held.removeAt(i);
            }
        }

        // Issue as many requests as the in-flight and consumer windows allow.
        bool issuedAny = false;
            const bool consumerWindowFull = config.consumerWindow > 0
                                            && state->held.size() >= config.consumerWindow;
            if (consumerWindowFull)
                ++state->throttledSlices;
            while (!consumerWindowFull && state->nextIndex < config.requests
                   && state->inFlight.size() < qMax(1, config.maxInFlight)) {
            const int index = state->nextIndex;
            scene.setTick(index);

            if (config.dryRun) {
                // Memory control: identical scene updates and bookkeeping, but no capture
                // at all. One request per loop slice so that the loop still pumps the
                // event loop between requests; pass --interval-ms equal to the real run's
                // average latency for a comparable duration and frame count.
                ++state->nextIndex;
                ++state->completedCount;
                issuedAny = true;
                break;
            }

            auto request = QSharedPointer<AsyncRequest>::create();
            request->index = index;
            request->tickAtRequest = index;
            request->requestedNs = monotonicNs();

            const QSharedPointer<QQuickItemGrabResult> grabResult = target->grabToImage();
            if (grabResult.isNull()) {
                ++state->failedToStart;
                ++state->nextIndex;
                continue;
            }

            request->result = grabResult;
            state->inFlight.append(request);
            ++state->nextIndex;
            issuedAny = true;

            QObject::connect(
                grabResult.data(), &QQuickItemGrabResult::ready, grabResult.data(),
                [state, request] {
                    request->readyNs = monotonicNs();
                    request->image = request->result->image();

                    if (request->image.isNull()) {
                        ++state->nullImages;
                    } else {
                        decodeRequest(*state, request);
                    }

                    state->latenciesMs.append(double(request->readyNs - request->requestedNs) / 1.0e6);

                    if (request->index < state->lastCompletedIndex) {
                        ++state->outOfOrder;
                    } else {
                        state->lastCompletedIndex = request->index;
                        ++state->completedInRequestOrder;
                    }

                    for (int i = 0; i < state->inFlight.size(); ++i) {
                        if (state->inFlight.at(i).data() == request.data()) {
                            state->inFlight.removeAt(i);
                            break;
                        }
                    }

                    ++state->completedCount;

                    if (state->config.consumerDelayMs > 0 && !request->image.isNull()) {
                        request->releaseAfterNs =
                            request->readyNs + qint64(state->config.consumerDelayMs) * 1000000LL;
                        state->retainedBytes += request->image.sizeInBytes();
                        state->peakRetainedBytes = qMax(state->peakRetainedBytes, state->retainedBytes);
                        state->held.append(request);
                        state->maxHeldObserved = qMax(state->maxHeldObserved, state->held.size());
                    }

                    // Release the result and its pixels as soon as the probe has copied
                    // what it needs. This also breaks the reference cycle
                    // result -> connection -> lambda -> request -> result, which would
                    // otherwise make the probe measure its own retention instead of the
                    // capture path's. Deferred so that the pixels are not destroyed while
                    // the ready() emission is still on the stack.
                    QTimer::singleShot(0, qApp, [request] {
                        if (request->result) {
                            request->result->disconnect();
                            request->result.clear();
                        }
                        if (request->releaseAfterNs == 0)
                            request->image = QImage();
                    });
                });

            state->maxInFlightObserved = qMax(state->maxInFlightObserved, state->inFlight.size());

            if (config.pacedRequests)
                pumpEvents(config.intervalMs > 0 ? config.intervalMs : 5);
        }

        if (!issuedAny || state->inFlight.isEmpty())
            pumpEvents(config.intervalMs > 0 ? config.intervalMs : 1);
        else
            pumpEvents(0);

        rssSeries.append(double(sampleProcessMetrics().rssBytes));

        if (wall.elapsed() > config.timeoutMs) {
            state->timedOut = true;
            result.notes.append(
                QStringLiteral("timed out after %1 ms with %2 request(s) still outstanding")
                    .arg(config.timeoutMs)
                    .arg(state->inFlight.size()));
            break;
        }
    }

    const double wallSeconds = double(wall.nsecsElapsed()) / 1.0e9;
    const ProcessMetrics endMetrics = sampleProcessMetrics();

    latencyTimer.stop();
    if (frameConnection)
        QObject::disconnect(frameConnection);

    result.requested = config.requests;
    result.completed = state->completedCount;
    result.failedToStart = state->failedToStart;
    result.nullImages = state->nullImages;
    result.maxInFlightObserved = state->maxInFlightObserved;
    result.timedOut = state->timedOut;
    result.wallSeconds = wallSeconds;
    result.completedPerSecond = wallSeconds > 0.0 ? double(state->completedCount) / wallSeconds : 0.0;

    QVector<double> sortedLatencies = state->latenciesMs;
    std::sort(sortedLatencies.begin(), sortedLatencies.end());
    result.latencyAvgMs = mean(state->latenciesMs);
    result.latencyP50Ms = percentileOfSorted(sortedLatencies, 0.50);
    result.latencyP95Ms = percentileOfSorted(sortedLatencies, 0.95);
    result.latencyMaxMs = sortedLatencies.isEmpty() ? 0.0 : sortedLatencies.last();

    result.distinctTicks = state->ticksSeen.size();
    result.duplicateCompletions = state->duplicateCompletions;
    result.outOfOrderCompletions = state->outOfOrder;
    result.tickLagAvg = mean(state->tickLags);
    QVector<double> sortedLags = state->tickLags;
    std::sort(sortedLags.begin(), sortedLags.end());
    result.tickLagP95 = percentileOfSorted(sortedLags, 0.95);
    result.tickLagMax = sortedLags.isEmpty() ? 0.0 : sortedLags.last();
    result.framesRendered = frames.load(std::memory_order_relaxed);
    result.lastFrameBytes = state->lastFrameBytes;
    result.distinctFboCounters = state->fboCountersSeen.size();
    result.fboCounterFirst = state->fboCounterFirst;
    result.fboCounterLast = state->fboCounterLast;
    result.consumerWindow = config.consumerWindow;
    result.maxHeldObserved = state->maxHeldObserved;
    result.throttledSlices = state->throttledSlices;
    result.dryRun = config.dryRun;

    result.rssStartBytes = rssSeries.isEmpty() ? 0 : quint64(rssSeries.first());
    result.rssEndBytes = rssSeries.isEmpty() ? 0 : quint64(rssSeries.last());
    result.peakRetainedBytes = state->peakRetainedBytes;
    result.rssSlopeBytesPer100Frames = slopePer100(rssSeries);
    result.rssGrowthBytes = qint64(result.rssEndBytes) - qint64(result.rssStartBytes);
    result.rssGrowthPerCompletionBytes =
        state->completedCount > 0 ? double(result.rssGrowthBytes) / double(state->completedCount)
                                 : 0.0;
    result.cpuSeconds = endMetrics.cpuSeconds - startMetrics.cpuSeconds;
    result.cpuPercentOfOneCore = wallSeconds > 0.0 ? 100.0 * result.cpuSeconds / wallSeconds : 0.0;

    result.guiLatencyAvgMs = mean(latencyMs);
    QVector<double> sortedGuiLatency = latencyMs;
    std::sort(sortedGuiLatency.begin(), sortedGuiLatency.end());
    result.guiLatencyP95Ms = percentileOfSorted(sortedGuiLatency, 0.95);
    result.guiLatencyMaxMs = sortedGuiLatency.isEmpty() ? 0.0 : sortedGuiLatency.last();

    if (state->inFlight.size() > 0 && !state->timedOut) {
        result.notes.append(QStringLiteral("%1 request(s) never completed").arg(state->inFlight.size()));
    }
    return result;
}

CompositionResult runComposition(SceneHost &scene)
{
    CompositionResult result;

    QQuickItem *root = scene.sceneRootItem();
    QQuickItem *content = scene.contentItem();
    if (!root || !content) {
        result.notes.append(
            QStringLiteral("this scene has no QML item tree, so QQuickItem::grabToImage() does not "
                           "apply"));
        return result;
    }

    result.applicable = true;
    result.rootItemParentIsContentItem = root->parentItem() == content;
    result.overlayColor = scene.overlayColor().name();
    result.expectedOverlayPixel = colorToString(scene.overlayColor());
    result.overlayRectInContentItem = QStringLiteral("%1,%2 %3x%4")
                                          .arg(scene.overlayRectInContentItem().x())
                                          .arg(scene.overlayRectInContentItem().y())
                                          .arg(scene.overlayRectInContentItem().width())
                                          .arg(scene.overlayRectInContentItem().height());
    if (QQuickWindow *window = scene.window()) {
        result.windowSize = QStringLiteral("%1x%2").arg(window->width()).arg(window->height());
    }
    result.contentItemSize = QStringLiteral("%1x%2").arg(content->width()).arg(content->height());
    result.rootItemSize = QStringLiteral("%1x%2").arg(root->width()).arg(root->height());
    const QPointF rootOrigin = root->mapToItem(content, QPointF(0.0, 0.0));
    result.rootItemPositionInContentItem =
        QStringLiteral("%1,%2").arg(rootOrigin.x()).arg(rootOrigin.y());

    const int fixedTick = 321;
    scene.setTick(fixedTick);
    pumpEvents(200);
    result.tickAtCapture = scene.tick();

    QString error;
    const QImage rootGrab = awaitGrab(root, 5000, &error);
    if (!error.isEmpty())
        result.notes.append(QStringLiteral("root item grab: %1").arg(error));
    const QImage contentGrab = awaitGrab(content, 5000, &error);
    if (!error.isEmpty())
        result.notes.append(QStringLiteral("content item grab: %1").arg(error));
    const QImage windowGrab = scene.window() ? scene.window()->grabWindow() : QImage();

    result.rootItemGrabSize = sizeToString(rootGrab);
    result.contentItemGrabSize = sizeToString(contentGrab);
    result.syncWindowGrabSize = sizeToString(windowGrab);

    const QRect overlayContentItem = scene.overlayRectInContentItem();
    const QPointF overlayCenter = overlayContentItem.center();

    const QColor rootOverlay = scene.sampleAtContentItemPoint(rootGrab, overlayCenter, root);
    const QColor contentOverlay = scene.sampleAtContentItemPoint(contentGrab, overlayCenter, content);
    const QColor windowOverlay = scene.sampleAtContentItemPoint(windowGrab, overlayCenter, content);
    result.rootItemOverlayPixel = colorToString(rootOverlay);
    result.contentItemOverlayPixel = colorToString(contentOverlay);
    result.syncWindowOverlayPixel = colorToString(windowOverlay);
    result.overlayPresentInRootItemGrab = colorMatches(rootOverlay, scene.overlayColor(), 8);
    result.overlayPresentInContentItemGrab = colorMatches(contentOverlay, scene.overlayColor(), 8);
    result.overlayPresentInSyncWindowGrab = colorMatches(windowOverlay, scene.overlayColor(), 8);

    const QPointF backgroundPoint(content->width() * 0.45, content->height() * 0.55);
    result.backgroundPixel = colorToString(
        scene.sampleAtContentItemPoint(contentGrab, backgroundPoint, content));

    result.tickInRootItemGrab = scene.decodeTickAt(rootGrab, root);
    result.tickInContentItemGrab = scene.decodeTickAt(contentGrab, content);
    result.tickInSyncWindowGrab = scene.decodeTickAt(windowGrab, content);

    if (result.overlayPresentInRootItemGrab) {
        result.notes.append(
            QStringLiteral("unexpected: the root item grab contains the overlay sibling"));
    }
    if (result.overlayPresentInContentItemGrab && !result.overlayPresentInRootItemGrab) {
        result.notes.append(
            QStringLiteral("the overlay sibling is only captured when the grabbed item is the "
                           "window's contentItem, not the application's root item"));
    }
    return result;
}

QVector<FidelityResult> runFidelity(SceneHost &scene)
{
    QVector<FidelityResult> results;

    QQuickItem *root = scene.sceneRootItem();
    QQuickItem *content = scene.contentItem();
    QQuickWindow *window = scene.window();
    if (!root || !content || !window)
        return results;

    // Freeze the scene and pin the encoded counter, then let it render once.
    scene.setFrozen(true);
    scene.setTick(4242);
    pumpEvents(500);

    QString error;
    const QImage asyncContent = awaitGrab(content, 5000, &error);
    const QImage asyncRoot = awaitGrab(root, 5000, &error);
    const QImage syncWindow = window->grabWindow();
    const QImage asyncContentAgain = awaitGrab(content, 5000, &error);

    const QRect overlayDevice = deviceRectForTargetPoint(
        scene.overlayRectInContentItem(), asyncContent, content, scene);

    struct Pair
    {
        QString labelA;
        QString labelB;
        QImage a;
        QImage b;
        QQuickItem *targetForDecode;
    };
    const QVector<Pair> pairs{
        {QStringLiteral("contentItem async"),
         QStringLiteral("QQuickWindow::grabWindow() sync"),
         asyncContent,
         syncWindow,
         content},
        {QStringLiteral("rootItem async"),
         QStringLiteral("QQuickWindow::grabWindow() sync"),
         asyncRoot,
         syncWindow,
         content},
        {QStringLiteral("rootItem async"),
         QStringLiteral("contentItem async"),
         asyncRoot,
         asyncContent,
         content},
        {QStringLiteral("contentItem async (repeat)"),
         QStringLiteral("contentItem async"),
         asyncContentAgain,
         asyncContent,
         content},
    };

    for (const Pair &pair : pairs) {
        FidelityResult result;
        result.labelA = pair.labelA;
        result.labelB = pair.labelB;
        result.sizeA = sizeToString(pair.a);
        result.sizeB = sizeToString(pair.b);

        const ImageDiff diff = compareImages(pair.a, pair.b, overlayDevice);
        result.sizeMatch = diff.sizeMatch;
        result.meanAbsDiff = diff.meanAbsDiff;
        result.maxChannelDiff = diff.maxChannelDiff;
        result.differingPixelPct = diff.differingPct;
        result.differingOutsideOverlayPct = diff.differingOutsideIgnoreRectPct;

        result.tickA = scene.decodeTickAt(pair.a, pair.targetForDecode);
        result.tickB = scene.decodeTickAt(pair.b, pair.targetForDecode);
        result.fboCounterA = scene.decodeFboCounterAt(pair.a, pair.targetForDecode);
        result.fboCounterB = scene.decodeFboCounterAt(pair.b, pair.targetForDecode);
        result.notesContainExpectedContent = result.tickA == 4242 && result.tickB == 4242;
        results.append(result);
    }

    scene.setFrozen(false);
    return results;
}

QVector<FailureModeResult> runFailureModes(SceneHost &scene)
{
    QVector<FailureModeResult> results;

    QQuickItem *root = scene.sceneRootItem();
    QQuickItem *content = scene.contentItem();
    QQuickWindow *window = scene.window();
    if (!root || !content || !window)
        return results;

    WarningRecorder recorder;

    // 1. Window hidden: the documented rejection path of grabToImage().
    {
        FailureModeResult result;
        result.name = QStringLiteral("async grab of a hidden window");
        result.api = QStringLiteral("QQuickItem::grabToImage() on contentItem()");

        scene.hide();
        recorder.clear();
        const qint64 startNs = monotonicNs();
        const QSharedPointer<QQuickItemGrabResult> grab = content->grabToImage();
        result.returnedNullRequest = grab.isNull();
        if (!grab.isNull()) {
            if (grab->image().isNull()) {
                QEventLoop loop;
                QTimer timeout;
                timeout.setSingleShot(true);
                QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
                QObject::connect(grab.data(), &QQuickItemGrabResult::ready, &loop, &QEventLoop::quit);
                timeout.start(3000);
                loop.exec(QEventLoop::AllEvents);
            }
            result.completed = true;
            result.completionMs = (monotonicNs() - startNs) / 1000000;
            result.imageNull = grab->image().isNull();
            result.imageSize = sizeToString(grab->image());
        } else {
            result.completionMs = (monotonicNs() - startNs) / 1000000;
        }
        result.qtWarnings = recorder.takeWarnings();
        result.note = QStringLiteral("window hidden");
        results.append(result);
    }

    // 2. Item detached from any window: the other documented rejection path.
    {
        FailureModeResult result;
        result.name = QStringLiteral("async grab of an item that is not in a window");
        result.api = QStringLiteral("QQuickItem::grabToImage() on a detached item");

        scene.show();
        root->setParentItem(nullptr);
        recorder.clear();
        const QSharedPointer<QQuickItemGrabResult> grab = root->grabToImage();
        result.returnedNullRequest = grab.isNull();
        result.completed = !grab.isNull();
        result.imageNull = grab.isNull() || grab->image().isNull();
        result.imageSize = grab.isNull() ? QStringLiteral("<no result>") : sizeToString(grab->image());
        result.qtWarnings = recorder.takeWarnings();
        result.note = QStringLiteral("root item temporarily detached from its parent");
        results.append(result);

        root->setParentItem(content);
        pumpEvents(100);
    }

    // 3. Recovery after both rejection cases.
    {
        FailureModeResult result;
        result.name = QStringLiteral("async grab again after re-showing the window");
        result.api = QStringLiteral("QQuickItem::grabToImage() on contentItem()");

        scene.show();
        scene.setTick(99);
        pumpEvents(200);
        recorder.clear();
        const qint64 startNs = monotonicNs();
        const QImage image = awaitGrab(content, 5000, nullptr);
        result.completed = !image.isNull();
        result.imageNull = image.isNull();
        result.imageSize = sizeToString(image);
        result.completionMs = (monotonicNs() - startNs) / 1000000;
        result.qtWarnings = recorder.takeWarnings();
        result.note = QStringLiteral("window visible again");
        results.append(result);
    }

    return results;
}

SyncBaselineResult runSyncBaseline(SceneHost &scene, int captures, int intervalMs)
{
    SyncBaselineResult result;
    result.api = scene.syncCaptureApi();

    QVector<double> latencyMs;
    QElapsedTimer latencyClock;
    latencyClock.start();
    double nextExpectedMs = 0.0;
    QTimer latencyTimer;
    latencyTimer.setTimerType(Qt::PreciseTimer);
    latencyTimer.setInterval(16);
    QObject::connect(&latencyTimer, &QTimer::timeout, &latencyTimer,
                     [&latencyMs, &latencyClock, &nextExpectedMs] {
                         const double nowMs = double(latencyClock.nsecsElapsed()) / 1.0e6;
                         latencyMs.append(qMax(0.0, nowMs - nextExpectedMs));
                         nextExpectedMs = nowMs + 16.0;
                     });
    latencyTimer.start();

    const ProcessMetrics startMetrics = sampleProcessMetrics();
    QVector<double> captureTimes;

    QElapsedTimer wall;
    wall.start();
    for (int i = 0; i < captures; ++i) {
        scene.setTick(i);
        QElapsedTimer timer;
        timer.start();
        const QImage image = scene.syncCapture();
        captureTimes.append(double(timer.nsecsElapsed()) / 1.0e6);
        if (image.isNull())
            ++result.nullImages;
        else
            result.lastImageSize = sizeToString(image);
        pumpEvents(intervalMs > 0 ? intervalMs : 16);
    }
    const double wallSeconds = double(wall.nsecsElapsed()) / 1.0e9;
    const ProcessMetrics endMetrics = sampleProcessMetrics();
    latencyTimer.stop();

    result.captures = captures;
    QVector<double> sorted = captureTimes;
    std::sort(sorted.begin(), sorted.end());
    result.avgMs = mean(captureTimes);
    result.p50Ms = percentileOfSorted(sorted, 0.50);
    result.p95Ms = percentileOfSorted(sorted, 0.95);
    result.maxMs = sorted.isEmpty() ? 0.0 : sorted.last();
    result.capturesPerSecond = wallSeconds > 0.0 ? double(captures) / wallSeconds : 0.0;
    result.rssGrowthBytes = qint64(endMetrics.rssBytes) - qint64(startMetrics.rssBytes);
    result.guiLatencyAvgMs = mean(latencyMs);
    QVector<double> sortedGui = latencyMs;
    std::sort(sortedGui.begin(), sortedGui.end());
    result.guiLatencyP95Ms = percentileOfSorted(sortedGui, 0.95);
    return result;
}

QVariantMap syncBaselineToVariant(const SyncBaselineResult &result)
{
    QVariantMap map;
    map.insert(QStringLiteral("api"), result.api);
    map.insert(QStringLiteral("captures"), result.captures);
    map.insert(QStringLiteral("nullImages"), result.nullImages);
    map.insert(QStringLiteral("avgMs"), result.avgMs);
    map.insert(QStringLiteral("p50Ms"), result.p50Ms);
    map.insert(QStringLiteral("p95Ms"), result.p95Ms);
    map.insert(QStringLiteral("maxMs"), result.maxMs);
    map.insert(QStringLiteral("capturesPerSecond"), result.capturesPerSecond);
    map.insert(QStringLiteral("rssGrowthBytes"), result.rssGrowthBytes);
    map.insert(QStringLiteral("guiLatencyAvgMs"), result.guiLatencyAvgMs);
    map.insert(QStringLiteral("guiLatencyP95Ms"), result.guiLatencyP95Ms);
    map.insert(QStringLiteral("lastImageSize"), result.lastImageSize);
    return map;
}

QVariantMap pipelineToVariant(const PipelineResult &result)
{
    QVariantMap map;
    map.insert(QStringLiteral("grabTarget"), result.targetName);
    map.insert(QStringLiteral("requests"), result.requests);
    map.insert(QStringLiteral("maxInFlight"), result.maxInFlight);
    map.insert(QStringLiteral("intervalMs"), result.intervalMs);
    map.insert(QStringLiteral("consumerDelayMs"), result.consumerDelayMs);
    map.insert(QStringLiteral("consumerWindow"), result.consumerWindow);
    map.insert(QStringLiteral("pacedRequests"), result.pacedRequests);
    map.insert(QStringLiteral("dryRun"), result.dryRun);

    QVariantMap outcome;
    outcome.insert(QStringLiteral("completed"), result.completed);
    outcome.insert(QStringLiteral("failedToStart"), result.failedToStart);
    outcome.insert(QStringLiteral("nullImages"), result.nullImages);
    outcome.insert(QStringLiteral("maxInFlightObserved"), result.maxInFlightObserved);
    outcome.insert(QStringLiteral("timedOut"), result.timedOut);
    outcome.insert(QStringLiteral("wallSeconds"), result.wallSeconds);
    outcome.insert(QStringLiteral("completedPerSecond"), result.completedPerSecond);
    map.insert(QStringLiteral("outcome"), outcome);

    QVariantMap latency;
    latency.insert(QStringLiteral("avgMs"), result.latencyAvgMs);
    latency.insert(QStringLiteral("p50Ms"), result.latencyP50Ms);
    latency.insert(QStringLiteral("p95Ms"), result.latencyP95Ms);
    latency.insert(QStringLiteral("maxMs"), result.latencyMaxMs);
    map.insert(QStringLiteral("latency"), latency);

    QVariantMap content;
    content.insert(QStringLiteral("distinctTicks"), result.distinctTicks);
    content.insert(QStringLiteral("duplicateCompletions"), result.duplicateCompletions);
    content.insert(QStringLiteral("outOfOrderCompletions"), result.outOfOrderCompletions);
    content.insert(QStringLiteral("tickLagAvg"), result.tickLagAvg);
    content.insert(QStringLiteral("tickLagP95"), result.tickLagP95);
    content.insert(QStringLiteral("tickLagMax"), result.tickLagMax);
    content.insert(QStringLiteral("framesRendered"), result.framesRendered);
    content.insert(QStringLiteral("lastFrameBytes"), result.lastFrameBytes);
    content.insert(QStringLiteral("distinctFboCounters"), result.distinctFboCounters);
    content.insert(QStringLiteral("fboCounterFirst"), result.fboCounterFirst);
    content.insert(QStringLiteral("fboCounterLast"), result.fboCounterLast);
    map.insert(QStringLiteral("content"), content);

    QVariantMap resources;
    resources.insert(QStringLiteral("rssStartBytes"), result.rssStartBytes);
    resources.insert(QStringLiteral("rssEndBytes"), result.rssEndBytes);
    resources.insert(QStringLiteral("peakRetainedBytes"), result.peakRetainedBytes);
    resources.insert(QStringLiteral("maxHeldObserved"), result.maxHeldObserved);
    resources.insert(QStringLiteral("throttledSlices"), result.throttledSlices);
    resources.insert(QStringLiteral("rssSlopeBytesPer100Frames"), result.rssSlopeBytesPer100Frames);
    resources.insert(QStringLiteral("rssGrowthBytes"), result.rssGrowthBytes);
    resources.insert(QStringLiteral("rssGrowthPerCompletionBytes"), result.rssGrowthPerCompletionBytes);
    resources.insert(QStringLiteral("cpuSeconds"), result.cpuSeconds);
    resources.insert(QStringLiteral("cpuPercentOfOneCore"), result.cpuPercentOfOneCore);
    resources.insert(QStringLiteral("guiLatencyAvgMs"), result.guiLatencyAvgMs);
    resources.insert(QStringLiteral("guiLatencyP95Ms"), result.guiLatencyP95Ms);
    resources.insert(QStringLiteral("guiLatencyMaxMs"), result.guiLatencyMaxMs);
    map.insert(QStringLiteral("resources"), resources);

    map.insert(QStringLiteral("notes"), result.notes);
    return map;
}

QVariantMap compositionToVariant(const CompositionResult &result)
{
    QVariantMap map;
    map.insert(QStringLiteral("applicable"), result.applicable);
    map.insert(QStringLiteral("rootItemParentIsContentItem"), result.rootItemParentIsContentItem);
    map.insert(QStringLiteral("windowSize"), result.windowSize);
    map.insert(QStringLiteral("contentItemSize"), result.contentItemSize);
    map.insert(QStringLiteral("rootItemSize"), result.rootItemSize);
    map.insert(QStringLiteral("rootItemPositionInContentItem"), result.rootItemPositionInContentItem);
    map.insert(QStringLiteral("overlayRectInContentItem"), result.overlayRectInContentItem);
    map.insert(QStringLiteral("overlayColor"), result.overlayColor);
    map.insert(QStringLiteral("tickAtCapture"), result.tickAtCapture);

    QVariantMap grabs;
    grabs.insert(QStringLiteral("rootItemAsyncSize"), result.rootItemGrabSize);
    grabs.insert(QStringLiteral("contentItemAsyncSize"), result.contentItemGrabSize);
    grabs.insert(QStringLiteral("syncWindowSize"), result.syncWindowGrabSize);
    grabs.insert(QStringLiteral("rootItemAsyncOverlayPixel"), result.rootItemOverlayPixel);
    grabs.insert(QStringLiteral("contentItemAsyncOverlayPixel"), result.contentItemOverlayPixel);
    grabs.insert(QStringLiteral("syncWindowOverlayPixel"), result.syncWindowOverlayPixel);
    grabs.insert(QStringLiteral("expectedOverlayPixel"), result.expectedOverlayPixel);
    grabs.insert(QStringLiteral("backgroundPixel"), result.backgroundPixel);
    grabs.insert(QStringLiteral("tickInRootItemAsync"), result.tickInRootItemGrab);
    grabs.insert(QStringLiteral("tickInContentItemAsync"), result.tickInContentItemGrab);
    grabs.insert(QStringLiteral("tickInSyncWindow"), result.tickInSyncWindowGrab);
    map.insert(QStringLiteral("grabs"), grabs);

    QVariantMap overlay;
    overlay.insert(QStringLiteral("inRootItemAsync"), result.overlayPresentInRootItemGrab);
    overlay.insert(QStringLiteral("inContentItemAsync"), result.overlayPresentInContentItemGrab);
    overlay.insert(QStringLiteral("inSyncWindow"), result.overlayPresentInSyncWindowGrab);
    map.insert(QStringLiteral("overlayPresence"), overlay);

    map.insert(QStringLiteral("notes"), result.notes);
    return map;
}

QVariantMap fidelityToVariant(const FidelityResult &result)
{
    QVariantMap map;
    map.insert(QStringLiteral("labelA"), result.labelA);
    map.insert(QStringLiteral("labelB"), result.labelB);
    map.insert(QStringLiteral("sizeMatch"), result.sizeMatch);
    map.insert(QStringLiteral("sizeA"), result.sizeA);
    map.insert(QStringLiteral("sizeB"), result.sizeB);
    map.insert(QStringLiteral("meanAbsChannelDiff"), result.meanAbsDiff);
    map.insert(QStringLiteral("maxChannelDiff"), result.maxChannelDiff);
    map.insert(QStringLiteral("differingPixelPct"), result.differingPixelPct);
    map.insert(QStringLiteral("differingOutsideOverlayPct"), result.differingOutsideOverlayPct);
    map.insert(QStringLiteral("tickA"), result.tickA);
    map.insert(QStringLiteral("tickB"), result.tickB);
    map.insert(QStringLiteral("fboCounterA"), result.fboCounterA);
    map.insert(QStringLiteral("fboCounterB"), result.fboCounterB);
    map.insert(QStringLiteral("bothContainExpectedTick"), result.notesContainExpectedContent);
    return map;
}

QVariantMap failureToVariant(const FailureModeResult &result)
{
    QVariantMap map;
    map.insert(QStringLiteral("name"), result.name);
    map.insert(QStringLiteral("api"), result.api);
    map.insert(QStringLiteral("returnedNullRequest"), result.returnedNullRequest);
    map.insert(QStringLiteral("completed"), result.completed);
    map.insert(QStringLiteral("completionMs"), result.completionMs);
    map.insert(QStringLiteral("imageNull"), result.imageNull);
    map.insert(QStringLiteral("imageSize"), result.imageSize);
    map.insert(QStringLiteral("qtWarnings"), result.qtWarnings);
    map.insert(QStringLiteral("note"), result.note);
    return map;
}

}  // namespace asyncspike
}  // namespace hyremote
