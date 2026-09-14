#pragma once

// Async capture spike probes (issue #16).
//
// Four probes, all built on public Qt APIs only:
//
//  * composition - which item must be grabbed so that the whole window content,
//    including overlay/sibling items, is captured;
//  * fidelity    - whether the asynchronous capture reproduces the composed
//    result, compared pixel-wise against the synchronous whole-window capture;
//  * pipeline    - latency, cadence, in-flight behaviour, completion ordering,
//    content duplication and consumer backpressure of the asynchronous path;
//  * failure     - the documented rejection cases (item not in a window, window
//    not visible) and whether the target recovers afterwards.

#include "harness/process_metrics.h"
#include "scenes/scene_host.h"

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

namespace hyremote {
namespace asyncspike {

enum class GrabTarget {
    ContentItem,  // QQuickWindow::contentItem(): the whole window scene root
    RootItem,     // the QML root item: a child of contentItem()
};

QString grabTargetName(GrabTarget target);

struct ProbeConfig
{
    int requests = 60;
    int maxInFlight = 1;
    int intervalMs = 0;          // event-loop time granted between request slices
    int consumerDelayMs = 0;     // hold a completed frame this long (slow consumer)
    int consumerWindow = 0;      // 0 = unlimited; else stop issuing while this many frames are held
    bool pacedRequests = false;  // pump between individual requests
    GrabTarget target = GrabTarget::ContentItem;
    int timeoutMs = 30000;
    bool dryRun = false;  // run the same loop without issuing any grab (memory control)
};

struct CompositionResult
{
    bool applicable = false;
    bool rootItemParentIsContentItem = false;
    QString windowSize;
    QString contentItemSize;
    QString rootItemSize;
    QString rootItemPositionInContentItem;
    QString overlayRectInContentItem;
    QString overlayColor;

    QString rootItemGrabSize;
    QString contentItemGrabSize;
    QString syncWindowGrabSize;

    QString rootItemOverlayPixel;
    QString contentItemOverlayPixel;
    QString syncWindowOverlayPixel;
    QString expectedOverlayPixel;
    QString backgroundPixel;

    bool overlayPresentInRootItemGrab = false;
    bool overlayPresentInContentItemGrab = false;
    bool overlayPresentInSyncWindowGrab = false;

    int tickInRootItemGrab = -1;
    int tickInContentItemGrab = -1;
    int tickInSyncWindowGrab = -1;
    int tickAtCapture = -1;

    QStringList notes;
};

struct FidelityResult
{
    QString labelA;
    QString labelB;
    bool sizeMatch = false;
    QString sizeA;
    QString sizeB;
    double meanAbsDiff = 0.0;
    int maxChannelDiff = 0;
    double differingPixelPct = 0.0;         // any channel differing by more than 2
    double differingOutsideOverlayPct = 0.0;
    int tickA = -1;
    int tickB = -1;
    int fboCounterA = -1;
    int fboCounterB = -1;
    bool notesContainExpectedContent = false;
};

struct PipelineResult
{
    // configuration
    QString targetName;
    int requests = 0;
    int maxInFlight = 0;
    int intervalMs = 0;
    int consumerDelayMs = 0;
    bool pacedRequests = false;

    // outcome
    int requested = 0;
    int failedToStart = 0;
    int completed = 0;
    int nullImages = 0;
    int maxInFlightObserved = 0;
    bool timedOut = false;
    double wallSeconds = 0.0;
    double completedPerSecond = 0.0;

    // latency (request -> ready), milliseconds
    double latencyAvgMs = 0.0;
    double latencyP50Ms = 0.0;
    double latencyP95Ms = 0.0;
    double latencyMaxMs = 0.0;

    // content behaviour
    int distinctTicks = 0;
    int duplicateCompletions = 0;
    int outOfOrderCompletions = 0;
    double tickLagAvg = 0.0;
    double tickLagP95 = 0.0;
    double tickLagMax = 0.0;
    int framesRendered = 0;
    qint64 lastFrameBytes = 0;
    int distinctFboCounters = 0;
    int fboCounterFirst = -1;
    int fboCounterLast = -1;
    int consumerWindow = 0;
    int maxHeldObserved = 0;
    int throttledSlices = 0;
    bool dryRun = false;

    // resources
    quint64 rssStartBytes = 0;
    quint64 rssEndBytes = 0;
    quint64 peakRetainedBytes = 0;
    double rssSlopeBytesPer100Frames = 0.0;
    qint64 rssGrowthBytes = 0;
    double rssGrowthPerCompletionBytes = 0.0;
    double cpuSeconds = 0.0;
    double cpuPercentOfOneCore = 0.0;
    double guiLatencyAvgMs = 0.0;
    double guiLatencyP95Ms = 0.0;
    double guiLatencyMaxMs = 0.0;

    QStringList notes;
};

struct SyncBaselineResult
{
    QString api;
    int captures = 0;
    int nullImages = 0;
    double avgMs = 0.0;
    double p50Ms = 0.0;
    double p95Ms = 0.0;
    double maxMs = 0.0;
    double capturesPerSecond = 0.0;
    qint64 rssGrowthBytes = 0;
    double guiLatencyAvgMs = 0.0;
    double guiLatencyP95Ms = 0.0;
    QString lastImageSize;
};

struct FailureModeResult
{
    QString name;
    QString api;
    bool returnedNullRequest = false;
    bool completed = false;
    qint64 completionMs = -1;
    bool imageNull = false;
    QString imageSize;
    QStringList qtWarnings;
    QString note;
};

// Captures Qt warnings emitted while a probe runs.
class WarningRecorder
{
public:
    WarningRecorder();
    ~WarningRecorder();
    QStringList takeWarnings();
    void clear();

private:
    bool m_installed = false;
};

PipelineResult runPipeline(SceneHost &scene, const ProbeConfig &config);
CompositionResult runComposition(SceneHost &scene);
QVector<FidelityResult> runFidelity(SceneHost &scene);
QVector<FailureModeResult> runFailureModes(SceneHost &scene);
SyncBaselineResult runSyncBaseline(SceneHost &scene, int captures, int intervalMs);

QVariantMap syncBaselineToVariant(const SyncBaselineResult &result);
QVariantMap pipelineToVariant(const PipelineResult &result);
QVariantMap compositionToVariant(const CompositionResult &result);
QVariantMap fidelityToVariant(const FidelityResult &result);
QVariantMap failureToVariant(const FailureModeResult &result);

}  // namespace asyncspike
}  // namespace hyremote
