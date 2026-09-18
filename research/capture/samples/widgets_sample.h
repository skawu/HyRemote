// SPDX-License-Identifier: Apache-2.0
#pragma once

// SPIKE-01 throwaway QWidget/raster capture case (issue #3, case A).

#include "harness/spike_types.h"

#include <QByteArray>
#include <QImage>
#include <QPointer>
#include <QSize>

#include <functional>

class QLabel;
class QMenu;
class QDialog;
class QWidget;

namespace hyremote {
namespace spike {

class WidgetsSample : public CaptureSample
{
public:
    WidgetsSample();
    ~WidgetsSample() override;

    QString id() const override { return QStringLiteral("widgets"); }
    QString title() const override;
    QString targetFamily() const override { return QStringLiteral("QWidget/raster"); }

    void prepare() override;
    void show() override;
    void tick(int frameIndex) override;
    QVector<CaptureOutcome> captureAll() override;
    QStringList observations() const override;
    void shutdown() override;
    QVariantMap info() const override;

    // The top-level window widget is the shared target: its coordinate system is
    // the one damage regions are mapped into.
    QWidget *damageTargetWidget() const override;
    void setDamageTracker(DamageTracker *tracker) override;
    QVector<DamageControlResult> runDamageControls() override;

private:
    CaptureOutcome grabWidget();
    CaptureOutcome renderIntoReusedImage();
    CaptureOutcome renderIntoFreshImage();
    CaptureOutcome renderIntoBorrowedBuffer();

    std::function<void(int)> m_advanceScene;

    QPointer<QWidget> m_root;
    QPointer<QLabel> m_counter;
    QPointer<QWidget> m_pulsing;  // used by the damage mapping controls
    QPointer<QDialog> m_dialog;
    QPointer<QLabel> m_dialogLabel;
    QPointer<QMenu> m_menu;
    DamageTracker *m_damageTracker = nullptr;

    // Reused producer target: its pixel storage identity is probed across
    // captures.
    QImage m_targetImage;
    quint64 m_reuseProbes = 0;
    quint64 m_reuseStorageReplacements = 0;
    quint64 m_reuseStorageMovedBetweenCaptures = 0;
    quintptr m_reuseLastAddress = 0;

    // Deterministic controls for the storage-identity probe (the synthetic
    // detach case and the unique-write case). They are not capture paths; they
    // validate that the probe really observes storage replacement.
    quint64 m_detachControlProbes = 0;
    quint64 m_detachControlDetections = 0;
    quint64 m_uniqueWriteControlProbes = 0;
    quint64 m_uniqueWriteControlStable = 0;

    void runStorageIdentityControls();

    // Caller-owned raw pixel buffer: stable address by construction.
    QByteArray m_borrowedBuffer;
    QSize m_borrowedSize;
    quint64 m_borrowedProbes = 0;
    quint64 m_borrowedStorageReplacements = 0;
    quintptr m_borrowedLastAddress = 0;
};

}  // namespace spike
}  // namespace hyremote
