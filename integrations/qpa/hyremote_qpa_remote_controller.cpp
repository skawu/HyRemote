// SPDX-License-Identifier: Apache-2.0
#include "hyremote_qpa_remote_controller.hpp"

#include "interactive_composite_target.hpp"

#include <HyRemote/RemoteAccess.h>

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QGuiApplication>
#include <QPoint>
#include <QTimer>
#include <QWindow>

#ifdef HYREMOTE_QPA_HAS_WIDGETS
#include <QApplication>
#include <QWidget>
#endif

#ifdef HYREMOTE_QPA_HAS_QUICK
#include <QtQuick/QQuickWindow>
#endif

#include <utility>

namespace HyRemote::Qpa {
namespace {

bool parseBoolean(const QString &text, bool &value)
{
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("1") || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("on") || normalized == QStringLiteral("yes")) {
        value = true;
        return true;
    }
    if (normalized == QStringLiteral("0") || normalized == QStringLiteral("false")
        || normalized == QStringLiteral("off") || normalized == QStringLiteral("no")) {
        value = false;
        return true;
    }
    return false;
}

bool isEligibleWindowType(Qt::WindowType type)
{
    // HyRemote is an application remote-access framework, not an OS desktop server. Foreign/native
    // handles and transient presentation-only shells that are not meaningful application content
    // are deliberately excluded. Real application Popup/Tool/Dialog windows remain eligible.
    return type != Qt::Desktop && type != Qt::SplashScreen && type != Qt::ToolTip
           && type != Qt::ForeignWindow;
}

}  // namespace

bool parseRemoteConfig(QStringList &parameters, RemoteConfig &config, QString &error)
{
    RemoteConfig parsed;

    for (auto it = parameters.begin(); it != parameters.end();) {
        const QString parameter = *it;
        const int separator = parameter.indexOf(QLatin1Char('='));
        const QString key = (separator >= 0 ? parameter.left(separator) : parameter).trimmed().toLower();
        const QString value = separator >= 0 ? parameter.mid(separator + 1).trimmed() : QString{};

        if (key == QStringLiteral("hyremote-address")) {
            if (separator < 0 || value.isEmpty()) {
                error = QStringLiteral("hyremote-address requires an explicit IP address");
                return false;
            }
            const QHostAddress address(value);
            if (address.isNull()) {
                error = QStringLiteral("invalid hyremote-address: %1").arg(value);
                return false;
            }
            parsed.listenAddress = address;
            it = parameters.erase(it);
            continue;
        }

        if (key == QStringLiteral("hyremote-port")) {
            bool ok = false;
            const int port = value.toInt(&ok);
            if (separator < 0 || !ok || port < 1 || port > 65535) {
                error = QStringLiteral("hyremote-port must be in the range 1..65535");
                return false;
            }
            parsed.port = static_cast<quint16>(port);
            it = parameters.erase(it);
            continue;
        }

        if (key == QStringLiteral("hyremote-input")) {
            bool enabled = false;
            if (separator < 0 || !parseBoolean(value, enabled)) {
                error = QStringLiteral("hyremote-input must be one of 0/1, false/true, off/on or no/yes");
                return false;
            }
            parsed.remoteInputEnabled = enabled;
            it = parameters.erase(it);
            continue;
        }

        ++it;
    }

    config = parsed;
    error.clear();
    return true;
}

RemoteController::RemoteController(RemoteConfig config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
{
}

RemoteController::~RemoteController()
{
    stop();
}

bool RemoteController::start()
{
    if (m_started)
        return true;

    QCoreApplication *application = QCoreApplication::instance();
    if (!application) {
        qWarning() << "HyRemote QPA remote controller cannot start before QCoreApplication exists";
        return false;
    }

    if (!m_compositeTarget)
        m_compositeTarget = std::make_unique<InteractiveCompositeTarget>();

    application->installEventFilter(this);
    m_started = true;
    scheduleRefresh();
    return true;
}

void RemoteController::stop() noexcept
{
    if (!m_started && !m_access && !m_compositeTarget)
        return;

    if (QCoreApplication *application = QCoreApplication::instance())
        application->removeEventFilter(this);

    m_started = false;
    m_refreshQueued = false;
    m_raisePending.clear();
    stopCurrent();
}

bool RemoteController::eventFilter(QObject *watched, QEvent *event)
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
        // Record only identity. refresh() validates that this object is still an eligible live
        // top-level application surface before applying a raise.
        m_raisePending.insert(watched);
        scheduleRefresh();
        break;
    default:
        break;
    }
    return false;
}

void RemoteController::scheduleRefresh()
{
    if (!m_started || m_refreshQueued)
        return;
    m_refreshQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_refreshQueued = false;
        refresh();
    });
}

void RemoteController::refresh()
{
    if (!m_started || !m_compositeTarget)
        return;

    const QList<SurfaceCandidate> surfaces = candidateSurfaces();
    QSet<QObject *> liveTargets;
    bool haveVisibleSurface = false;

    for (const SurfaceCandidate &surface : surfaces) {
        QObject *target = surface.target.data();
        if (!target)
            continue;

        liveTargets.insert(target);
        quint64 id = m_surfaceIds.value(target, 0);
        if (id == 0) {
            id = m_nextSurfaceId++;
            m_surfaceIds.insert(target, id);
        }

        m_compositeTarget->upsertSurface(id, target, surface.globalGeometry, surface.visible);
        if (surface.visible)
            haveVisibleSurface = true;

        if (surface.visible && m_raisePending.contains(target))
            m_compositeTarget->raiseSurface(id);
    }

    // Remove only objects that no longer exist as eligible application-owned top levels. Hidden or
    // minimized windows remain tracked with visible=false so re-show preserves identity and does
    // not imply a transport or RemoteAccess restart.
    for (auto it = m_surfaceIds.begin(); it != m_surfaceIds.end();) {
        if (!liveTargets.contains(it.key())) {
            const quint64 removedId = it.value();
            m_compositeTarget->removeSurface(removedId);
            if (m_activeSurfaceId && *m_activeSurfaceId == removedId) {
                m_activeSurfaceId.reset();
                m_compositeTarget->clearActiveSurface();
            }
            it = m_surfaceIds.erase(it);
        } else {
            ++it;
        }
    }
    m_raisePending.clear();

    // Keyboard/text follows actual Qt application activation. Never infer focus from z-order.
    QObject *activeTarget = activeSurfaceTarget(surfaces);
    if (activeTarget) {
        const quint64 activeId = m_surfaceIds.value(activeTarget, 0);
        if (activeId != 0) {
            if (!m_activeSurfaceId || *m_activeSurfaceId != activeId)
                m_compositeTarget->raiseSurface(activeId);
            m_activeSurfaceId = activeId;
            m_compositeTarget->setActiveSurface(activeId);
        }
    } else {
        m_activeSurfaceId.reset();
        m_compositeTarget->clearActiveSurface();
    }

    // Listener construction remains inert until the application has real visible content. Once
    // started, surface churn — including a temporary zero-visible-surface interval — never stops or
    // recreates RemoteAccess. The runtime ends only when the controller/application itself stops.
    if (!m_access && haveVisibleSurface)
        ensureRuntimeStarted();
}

bool RemoteController::ensureRuntimeStarted()
{
    if (m_access)
        return true;
    if (!m_compositeTarget)
        return false;

    auto access = std::make_unique<::HyRemote::RemoteAccess>(m_compositeTarget.get());
    if (!access->setListenAddress(m_config.listenAddress)
        || !access->setPort(m_config.port)
        || !access->setRemoteInputEnabled(m_config.remoteInputEnabled)) {
        const auto error = access->lastError();
        qWarning() << "HyRemote QPA Proxy rejected its remote configuration:"
                   << (error ? error->message : QStringLiteral("unknown configuration error"));
        return false;
    }

    if (!access->start()) {
        const auto error = access->lastError();
        qWarning() << "HyRemote QPA Proxy could not start the composite RemoteAccess runtime:"
                   << (error ? error->message : QStringLiteral("unknown runtime error"));
        return false;
    }

    m_access = std::move(access);
    qInfo() << "HyRemote QPA application remote access active on"
            << m_config.listenAddress.toString() << m_config.port
            << "remote input:" << m_config.remoteInputEnabled;
    return true;
}

void RemoteController::stopCurrent() noexcept
{
    if (m_access)
        m_access->stop();
    m_access.reset();

    m_activeSurfaceId.reset();
    m_surfaceIds.clear();
    m_compositeTarget.reset();
    m_nextSurfaceId = 1;
}

QList<RemoteController::SurfaceCandidate> RemoteController::candidateSurfaces() const
{
    QList<SurfaceCandidate> result;
    QSet<QWindow *> widgetWindowHandles;

#ifdef HYREMOTE_QPA_HAS_WIDGETS
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

#ifdef HYREMOTE_QPA_HAS_QUICK
    const QWindowList windows = QGuiApplication::topLevelWindows();
    for (QWindow *window : windows) {
        if (!window || widgetWindowHandles.contains(window) || !isEligibleWindowType(window->type()))
            continue;

        // The V1 product surface is Widgets + Qt Quick. A generic/foreign QWindow that is not a
        // QQuickWindow has no qualified built-in target adapter and is therefore not silently
        // represented as remotely supported.
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

QObject *RemoteController::activeSurfaceTarget(const QList<SurfaceCandidate> &surfaces) const
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

#ifdef HYREMOTE_QPA_HAS_WIDGETS
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

#ifdef HYREMOTE_QPA_HAS_WIDGETS
    for (const SurfaceCandidate &surface : surfaces) {
        auto *widget = qobject_cast<QWidget *>(surface.target.data());
        if (widget && surface.visible && widget->windowHandle() == focusWindow)
            return widget;
    }
#endif

#ifdef HYREMOTE_QPA_HAS_QUICK
    if (liveVisible(focusWindow) && qobject_cast<QQuickWindow *>(focusWindow))
        return focusWindow;
#endif

    return nullptr;
}

}  // namespace HyRemote::Qpa
