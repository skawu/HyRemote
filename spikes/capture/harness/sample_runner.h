#pragma once

// SPIKE-01 throwaway measurement harness (see spike_types.h).

#include "harness/spike_types.h"

#include <QVariantMap>
#include <QVector>

namespace hyremote {
namespace spike {

struct RunConfig
{
    QStringList samples;  // empty => every registered sample
    int frames = 120;
    int intervalMs = 16;
    int warmupFrames = 15;
    int runs = 1;
    int submitEvery = 3;  // hand every Nth captured frame to the sink
    bool sink = true;
    QString rhi = QStringLiteral("default");
    QString jsonPath;
    QStringList disabledPaths;  // substrings of capture paths to skip
};

struct PathStats
{
    QString path;
    bool primary = false;
    int attempts = 0;
    int successes = 0;
    double firstNs = 0.0;
    double avgNs = 0.0;
    double p50Ns = 0.0;
    double p95Ns = 0.0;
    double maxNs = 0.0;
    QString frameFormat;
    QString frameSize;
    bool producerReusesBuffer = false;
    QString callerThread;

    // Storage-identity probe (real QImage/backing-store address, not cacheKey).
    int storageProbes = 0;
    int storageReplacements = 0;
    QString storageAddressFirst;
    QString storageAddressLast;

    QStringList notes;
};

struct RunReport
{
    QString sampleId;
    QString title;
    QString targetFamily;
    int runIndex = 0;
    QVariantMap sampleInfo;

    int framesRequested = 0;
    int framesCaptured = 0;
    int intervalMs = 0;
    double wallSeconds = 0.0;
    double achievedFps = 0.0;

    double cpuSeconds = 0.0;
    double cpuPercentOfOneCore = 0.0;
    quint64 rssStartBytes = 0;
    quint64 rssEndBytes = 0;
    quint64 rssPeakBytes = 0;
    double rssSlopeBytesPer100Frames = 0.0;

    double guiLatencyAvgMs = 0.0;
    double guiLatencyP95Ms = 0.0;
    double guiLatencyMaxMs = 0.0;

    // Where the GUI thread time of one measurement iteration went.
    double avgPumpMs = 0.0;         // application event processing
    double avgCaptureLoopMs = 0.0;  // all capture calls of one iteration

    // Damage, expressed relative to the shared target rect. The ratios are only
    // meaningful when damageTargetArea is non-zero, i.e. when the target family
    // has a QWidget root whose coordinate system the regions could be mapped into.
    double avgDamageRatio = 0.0;
    double avgCaptureDamageRatio = 0.0;
    qint64 damageTargetArea = 0;
    QVector<DamageControlResult> damageControls;
    quint64 mappedPaintEvents = 0;
    quint64 excludedPaintEvents = 0;
    quint64 emptyDamageRegions = 0;
    quint64 updateRequests = 0;
    int observedObjects = 0;

    QVector<PathStats> paths;
    QVariantMap handoff;
    QStringList observations;
    QStringList warnings;
};

// Static description of the machine/toolchain used for a run.
QVariantMap environmentReport();

RunReport runSample(CaptureSample *sample, const RunConfig &config, int runIndex);
QVariantMap runReportToVariant(const RunReport &report);

}  // namespace spike
}  // namespace hyremote
