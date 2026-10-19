#include "automatic/automatic_access_controller.hpp"

#include "access_instance.hpp"
#include "automatic/interactive_composite_target.hpp"
#include "detail/runtime_diagnostics.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QGuiApplication>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QSet>
#include <QTimer>
#include <QWindow>

#ifdef HYREMOTE_HAS_WIDGETS_ADAPTER
#include <QApplication>
#include <QWidget>
#endif

#ifdef HYREMOTE_HAS_QUICK_ADAPTER
#include <QtQuick/QQuickWindow>
#endif

#include <atomic>
#include <optional>
#include <utility>

namespace HyRemote::Runtime::Automatic {
namespace {

bool isEligibleWindowType(Qt::WindowType type)
{
    // HyRemote is an application remote-access framework, not an OS desktop server. Foreign/native
    // handles and transient presentation-only shells that are not meaningful application content
    // are deliberately excluded. Real application Popup/Tool/Dialog windows remain eligible.
    return type != Qt::Desktop && type != Qt::SplashScreen && type != Qt::ToolTip
           && type != Qt::ForeignWindow;
}

}  // namespace

struct AccessController::Impl final : QObject
{
    struct SurfaceCandidate
    {
        QPointer<QObject> target;
        QRect globalGeometry;
        bool visible = false;
    };

    explicit Impl(AccessConfig config)
        : config(std::move(config))
    {
    }

    ~Impl() override { stop(); }

    bool start(bool scheduleInitialRefresh)
    {
        if (started)
            return true;

        QCoreApplication *application = QCoreApplication::instance();
        if (!application) {
            qWarning() << "HyRemote automatic access cannot start before QCoreApplication exists";
            return false;
        }

        if (!compositeTarget)
            compositeTarget = std::make_unique<::HyRemote::Runtime::Automatic::InteractiveCompositeTarget>();

        application->installEventFilter(this);
        started = true;
        if (scheduleInitialRefresh)
            scheduleRefresh();
        return true;
    }

    void stop() noexcept
    {
        if (!started && !access && !compositeTarget)
            return;

        if (QCoreApplication *application = QCoreApplication::instance())
            application->removeEventFilter(this);

        started = false;
        refreshQueued = false;
        raisePending.clear();
        stopCurrent();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::Close:
        case QEvent::Destroy:
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::WindowStateChange:
        case QEvent::ParentChange:
        case QEvent::WindowDeactivate:
            scheduleRefresh();
            break;
        case QEvent::WindowActivate:
        case QEvent::ZOrderChange:
            raisePending.insert(watched);
            scheduleRefresh();
            break;
        default:
            break;
        }
        return false;
    }

private:
    void scheduleRefresh()
    {
        if (!started || refreshQueued)
            return;
        refreshQueued = true;
        QTimer::singleShot(0, this, [this] {
            refreshQueued = false;
            refresh();
        });
    }

    void refresh()
    {
        if (!started || !compositeTarget)
            return;

        const QList<SurfaceCandidate> surfaces = candidateSurfaces();
        QSet<QObject *> liveTargets;
        bool haveVisibleSurface = false;

        for (const SurfaceCandidate &surface : surfaces) {
            QObject *target = surface.target.data();
            if (!target)
                continue;

            liveTargets.insert(target);
            quint64 id = surfaceIds.value(target, 0);
            if (id == 0) {
                id = nextSurfaceId++;
                surfaceIds.insert(target, id);
            }

            compositeTarget->upsertSurface(id, target, surface.globalGeometry, surface.visible);
            if (surface.visible)
                haveVisibleSurface = true;

            if (surface.visible && raisePending.contains(target))
                compositeTarget->raiseSurface(id);
        }

        for (auto it = surfaceIds.begin(); it != surfaceIds.end();) {
            if (!liveTargets.contains(it.key())) {
                const quint64 removedId = it.value();
                compositeTarget->removeSurface(removedId);
                if (activeSurfaceId && *activeSurfaceId == removedId) {
                    activeSurfaceId.reset();
                    compositeTarget->clearActiveSurface();
                }
                it = surfaceIds.erase(it);
            } else {
                ++it;
            }
        }
        raisePending.clear();

        QObject *activeTarget = activeSurfaceTarget(surfaces);
        if (activeTarget) {
            const quint64 activeId = surfaceIds.value(activeTarget, 0);
            if (activeId != 0) {
                if (!activeSurfaceId || *activeSurfaceId != activeId)
                    compositeTarget->raiseSurface(activeId);
                activeSurfaceId = activeId;
                compositeTarget->setActiveSurface(activeId);
            }
        } else {
            activeSurfaceId.reset();
            compositeTarget->clearActiveSurface();
        }

        // Runtime construction stays inert until real visible content exists. Once started, temporary
        // surface churn does not recreate the Session/listener; only controller teardown stops it.
        if (!access && haveVisibleSurface)
            ensureRuntimeStarted();
    }

    DiagnosticSnapshot diagnosticSnapshot(const Runtime::AccessInstance &instance) const
    {
        DiagnosticSnapshot snapshot = instance.diagnosticSnapshot();
        snapshot.integrationRoute = config.integrationRoute;
        // Re-evaluate the same eligible, visible surface set used by refresh(). No retained
        // frontend counter or guessed family based on the Generic/QPA route.
        bool hasWidgets = false;
        bool hasQuick = false;
        for (const SurfaceCandidate &surface : candidateSurfaces()) {
            if (!surface.visible || !surface.target)
                continue;
#ifdef HYREMOTE_HAS_WIDGETS_ADAPTER
            hasWidgets = hasWidgets || qobject_cast<QWidget *>(surface.target.data());
#endif
#ifdef HYREMOTE_HAS_QUICK_ADAPTER
            hasQuick = hasQuick || qobject_cast<QQuickWindow *>(surface.target.data());
#endif
        }
        snapshot.uiFamily = classifyUiFamily(hasWidgets, hasQuick);
        return snapshot;
    }

    void writeDiagnosticReport(const Runtime::AccessInstance &instance) const
    {
        const DiagnosticSnapshot snapshot = diagnosticSnapshot(instance);
        const QString report = formatDiagnosticReport(snapshot);
        if (snapshot.lastError)
            qWarning().noquote() << report;
        else
            qInfo().noquote() << report;
    }

    void writeCurrentDiagnosticReport() const
    {
        if (access)
            writeDiagnosticReport(*access);
    }

    bool isCurrentDiagnosticRun(const std::shared_ptr<std::atomic<bool>> &activity) const noexcept
    {
        return activity && activity->load(std::memory_order_acquire)
               && diagnosticRunActive == activity;
    }

    void queueDiagnosticReportOnOwnerThread(const std::shared_ptr<std::atomic<bool>> &activity)
    {
        if (!isCurrentDiagnosticRun(activity) || !access || diagnosticReportQueued)
            return;

        diagnosticReportQueued = true;
        QTimer::singleShot(0, this, [this, activity] {
            // A callback queued by an old run must not clear the new run's coalescing flag or read its
            // replacement AccessInstance. Teardown invalidates the captured activity token first.
            if (!isCurrentDiagnosticRun(activity))
                return;

            diagnosticReportQueued = false;
            writeCurrentDiagnosticReport();
        });
    }

    void subscribeDiagnostics()
    {
        if (!access || diagnosticToken.isValid())
            return;

        diagnosticRunActive = std::make_shared<std::atomic<bool>>(true);
        const auto activity = diagnosticRunActive;
        const QPointer<Impl> guard(this);
        const auto notify = [guard, activity] {
            if (!guard || !activity->load(std::memory_order_acquire))
                return;

            QMetaObject::invokeMethod(guard.data(), [guard, activity] {
                if (guard && activity->load(std::memory_order_acquire))
                    guard->queueDiagnosticReportOnOwnerThread(activity);
            }, Qt::QueuedConnection);
        };

        RuntimeNotificationHandlers handlers;
        handlers.stateChanged = [notify](AccessState) { notify(); };
        handlers.connectedClientCountChanged = [notify](std::size_t) { notify(); };
        handlers.errorChanged = [notify](std::optional<Error>) { notify(); };
        diagnosticToken = access->subscribeNotifications(std::move(handlers));
    }

    void unsubscribeDiagnostics() noexcept
    {
        // Invalidate every copied/queued handler before removing the live subscription or touching the
        // current coalescing flag. Already-posted callbacks then become inert across stop/restart.
        if (diagnosticRunActive)
            diagnosticRunActive->store(false, std::memory_order_release);
        if (access && diagnosticToken.isValid())
            access->unsubscribeNotifications(diagnosticToken);
        diagnosticRunActive.reset();
        diagnosticToken = RuntimeNotificationToken{};
        diagnosticReportQueued = false;
    }

    bool ensureRuntimeStarted()
    {
        if (access)
            return true;
        if (!compositeTarget)
            return false;

        auto instance = std::make_unique<Runtime::AccessInstance>(compositeTarget.get());
        // #174: exactly one of the two selects the binding. setListenAddress clears an interface selection, so calling
        // both in sequence would silently discard the interface the configuration asked for - the interface wins when
        // it is configured, and only then is the address left alone.
        const bool bindingAccepted = config.listenInterface.isEmpty()
                                         ? instance->setListenAddress(config.listenAddress)
                                         : instance->setListenInterface(config.listenInterface);
        if (!bindingAccepted
            || !instance->setPort(config.port)
            || !instance->setRemoteInputEnabled(config.remoteInputEnabled)
            || !instance->setSecurityProfile(config.securityProfile)
            || !instance->setSecurityConfigFile(config.securityConfigFile)) {
            writeDiagnosticReport(*instance);
            return false;
        }

        if (!instance->start()) {
            writeDiagnosticReport(*instance);
            return false;
        }

        access = std::move(instance);
        subscribeDiagnostics();
        writeCurrentDiagnosticReport();
        return true;
    }

    void stopCurrent() noexcept
    {
        // Stop live notifications before teardown; the final stopped snapshot is emitted synchronously
        // after Runtime quiescence so no queued worker notification can dereference a destroyed instance.
        unsubscribeDiagnostics();
        if (access) {
            access->stop();
            writeCurrentDiagnosticReport();
        }
        access.reset();

        activeSurfaceId.reset();
        surfaceIds.clear();
        compositeTarget.reset();
        nextSurfaceId = 1;
    }

    QList<SurfaceCandidate> candidateSurfaces() const
    {
        QList<SurfaceCandidate> result;
        QSet<QWindow *> widgetWindowHandles;

#ifdef HYREMOTE_HAS_WIDGETS_ADAPTER
        if (qobject_cast<QApplication *>(QCoreApplication::instance())) {
            const QWidgetList widgets = QApplication::topLevelWidgets();
            for (QWidget *widget : widgets) {
                if (!widget || !widget->isWindow() || !isEligibleWindowType(widget->windowType()))
                    continue;

                QWindow *handle = widget->windowHandle();
                if (handle)
                    widgetWindowHandles.insert(handle);

                const bool visible = widget->isVisible() && !widget->isMinimized()
                                     && widget->width() > 0 && widget->height() > 0;
                const QRect geometry(widget->mapToGlobal(QPoint(0, 0)), widget->size());
                result.push_back(SurfaceCandidate{widget, geometry, visible});
            }
        }
#endif

#ifdef HYREMOTE_HAS_QUICK_ADAPTER
        const QWindowList windows = QGuiApplication::topLevelWindows();
        for (QWindow *window : windows) {
            if (!window || widgetWindowHandles.contains(window) || !isEligibleWindowType(window->type()))
                continue;

            auto *quickWindow = qobject_cast<QQuickWindow *>(window);
            if (!quickWindow)
                continue;

            const bool visible = quickWindow->isVisible()
                                 && quickWindow->visibility() != QWindow::Minimized
                                 && quickWindow->width() > 0 && quickWindow->height() > 0;
            result.push_back(SurfaceCandidate{quickWindow, quickWindow->geometry(), visible});
        }
#endif

        return result;
    }

    QObject *activeSurfaceTarget(const QList<SurfaceCandidate> &surfaces) const
    {
        const auto liveVisible = [&surfaces](QObject *target) {
            if (!target)
                return false;
            for (const SurfaceCandidate &surface : surfaces) {
                if (surface.target.data() == target)
                    return surface.visible;
            }
            return false;
        };

#ifdef HYREMOTE_HAS_WIDGETS_ADAPTER
        if (qobject_cast<QApplication *>(QCoreApplication::instance())) {
            if (QWidget *popup = QApplication::activePopupWidget(); liveVisible(popup))
                return popup;
            if (QWidget *active = QApplication::activeWindow(); liveVisible(active))
                return active;
        }
#endif

        QWindow *focusWindow = QGuiApplication::focusWindow();
        if (!focusWindow)
            return nullptr;

#ifdef HYREMOTE_HAS_WIDGETS_ADAPTER
        for (const SurfaceCandidate &surface : surfaces) {
            auto *widget = qobject_cast<QWidget *>(surface.target.data());
            if (widget && surface.visible && widget->windowHandle() == focusWindow)
                return widget;
        }
#endif

#ifdef HYREMOTE_HAS_QUICK_ADAPTER
        if (liveVisible(focusWindow) && qobject_cast<QQuickWindow *>(focusWindow))
            return focusWindow;
#endif

        return nullptr;
    }

    AccessConfig config;
    std::unique_ptr<Runtime::AccessInstance> access;
    RuntimeNotificationToken diagnosticToken;
    std::shared_ptr<std::atomic<bool>> diagnosticRunActive;
    std::unique_ptr<::HyRemote::Runtime::Automatic::InteractiveCompositeTarget> compositeTarget;
    QHash<QObject *, quint64> surfaceIds;
    QSet<QObject *> raisePending;
    quint64 nextSurfaceId = 1;
    std::optional<quint64> activeSurfaceId;
    bool started = false;
    bool refreshQueued = false;
    bool diagnosticReportQueued = false;
};

AccessController::AccessController(AccessConfig config)
    : m_impl(std::make_unique<Impl>(std::move(config)))
{
}

AccessController::~AccessController()
{
    stop();
}

AccessController::AccessController(AccessController &&) noexcept = default;
AccessController &AccessController::operator=(AccessController &&) noexcept = default;

bool AccessController::start(bool scheduleInitialRefresh)
{
    return m_impl && m_impl->start(scheduleInitialRefresh);
}

void AccessController::stop() noexcept
{
    if (m_impl)
        m_impl->stop();
}

}  // namespace HyRemote::Runtime::Automatic
