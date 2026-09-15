#include "hyremote_qpa_remote_controller.hpp"

#include <HyRemote/RemoteAccess.h>

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QGuiApplication>
#include <QSet>
#include <QTimer>
#include <QWindow>

#ifdef HYREMOTE_QPA_HAS_WIDGETS
#include <QApplication>
#include <QWidget>
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

bool isPrimaryCandidateWindowType(Qt::WindowType type)
{
    return type != Qt::Popup && type != Qt::ToolTip && type != Qt::SplashScreen;
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

    application->installEventFilter(this);
    m_started = true;
    scheduleRefresh();
    return true;
}

void RemoteController::stop() noexcept
{
    if (!m_started && !m_access)
        return;

    if (QCoreApplication *application = QCoreApplication::instance())
        application->removeEventFilter(this);
    m_started = false;
    m_refreshQueued = false;
    stopCurrent();
}

bool RemoteController::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);
    switch (event->type()) {
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::Close:
    case QEvent::Destroy:
    case QEvent::WindowStateChange:
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
    if (!m_started)
        return;

    if (m_access) {
        if (m_target && targetIsVisible(m_target.data()))
            return;
        stopCurrent();
    }

    const QList<QObject *> candidates = candidateTargets();
    for (QObject *candidate : candidates) {
        auto access = std::make_unique<::HyRemote::RemoteAccess>(candidate);
        if (!access->setListenAddress(m_config.listenAddress)
            || !access->setPort(m_config.port)
            || !access->setRemoteInputEnabled(m_config.remoteInputEnabled)) {
            const auto error = access->lastError();
            qWarning() << "HyRemote QPA Proxy rejected its remote configuration:"
                       << (error ? error->message : QStringLiteral("unknown configuration error"));
            return;
        }

        if (!access->start()) {
            const auto error = access->lastError();
            if (error && error->code == RemoteAccessErrorCode::TargetAdapterUnavailable)
                continue;

            qWarning() << "HyRemote QPA Proxy could not start RemoteAccess for the selected target:"
                       << (error ? error->message : QStringLiteral("unknown runtime error"));
            return;
        }

        m_target = candidate;
        m_access = std::move(access);
        m_targetDestroyedConnection = QObject::connect(candidate, &QObject::destroyed, this, [this] {
            scheduleRefresh();
        });
        qInfo() << "HyRemote QPA remote access active on" << m_config.listenAddress.toString()
                << m_config.port << "remote input:" << m_config.remoteInputEnabled;
        return;
    }
}

void RemoteController::stopCurrent() noexcept
{
    if (m_targetDestroyedConnection)
        QObject::disconnect(m_targetDestroyedConnection);
    m_targetDestroyedConnection = {};

    if (m_access)
        m_access->stop();
    m_access.reset();
    m_target.clear();
}

QList<QObject *> RemoteController::candidateTargets() const
{
    QList<QObject *> candidates;
    QSet<QWindow *> widgetWindowHandles;

#ifdef HYREMOTE_QPA_HAS_WIDGETS
    if (qobject_cast<QApplication *>(QCoreApplication::instance())) {
        const QWidgetList widgets = QApplication::topLevelWidgets();
        for (QWidget *widget : widgets) {
            if (!widget || !widget->isWindow() || !widget->isVisible()
                || !isPrimaryCandidateWindowType(widget->windowType())) {
                continue;
            }
            candidates.push_back(widget);
            if (QWindow *handle = widget->windowHandle())
                widgetWindowHandles.insert(handle);
        }
    }
#endif

    const QWindowList windows = QGuiApplication::topLevelWindows();
    for (QWindow *window : windows) {
        if (!window || !window->isVisible() || widgetWindowHandles.contains(window))
            continue;
        const auto type = static_cast<Qt::WindowType>(window->flags() & Qt::WindowType_Mask);
        if (!isPrimaryCandidateWindowType(type))
            continue;
        candidates.push_back(window);
    }

    return candidates;
}

bool RemoteController::targetIsVisible(QObject *target)
{
#ifdef HYREMOTE_QPA_HAS_WIDGETS
    if (auto *widget = qobject_cast<QWidget *>(target))
        return widget->isVisible();
#endif
    if (auto *window = qobject_cast<QWindow *>(target))
        return window->isVisible();
    return false;
}

}  // namespace HyRemote::Qpa
