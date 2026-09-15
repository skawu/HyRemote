#pragma once

#include <QHostAddress>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <memory>

namespace HyRemote {
class RemoteAccess;
}

namespace HyRemote::Qpa {

struct RemoteConfig
{
    QHostAddress listenAddress = QHostAddress::LocalHost;
    quint16 port = 5900;
    bool remoteInputEnabled = false;
};

// Removes HyRemote-owned platform parameters from `parameters` while leaving native delegate
// parameters untouched. Invalid HyRemote values fail closed and provide a user-facing diagnostic.
bool parseRemoteConfig(QStringList &parameters, RemoteConfig &config, QString &error);

// Gate-02 controller for the Transparent QPA Proxy. It never implements capture/input/transport
// itself: once a supported visible top-level target exists, it composes the same public
// HyRemote::RemoteAccess runtime used by the C++ and QML product modes.
class RemoteController final : public QObject
{
public:
    explicit RemoteController(RemoteConfig config, QObject *parent = nullptr);
    ~RemoteController() override;

    bool start();
    void stop() noexcept;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void scheduleRefresh();
    void refresh();
    void stopCurrent() noexcept;
    QList<QObject *> candidateTargets() const;
    static bool targetIsVisible(QObject *target);

    RemoteConfig m_config;
    std::unique_ptr<::HyRemote::RemoteAccess> m_access;
    QPointer<QObject> m_target;
    QMetaObject::Connection m_targetDestroyedConnection;
    bool m_started = false;
    bool m_refreshQueued = false;
};

}  // namespace HyRemote::Qpa
