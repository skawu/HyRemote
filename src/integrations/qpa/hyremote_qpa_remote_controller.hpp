#pragma once

#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QSet>
#include <QStringList>

#include <memory>
#include <optional>

namespace HyRemote {
class RemoteAccess;
}

namespace HyRemote::Qpa {

class InteractiveCompositeTarget;

enum class SecurityProfile {
    Insecure,
    Authenticated,
    AuthenticatedEncrypted,
};

struct RemoteConfig
{
    QHostAddress listenAddress = QHostAddress::LocalHost;
    // The configured product default; see HYREMOTE_DEFAULT_PORT in cmake/HyRemoteProjectOptions.cmake.
    quint16 port = static_cast<quint16>(HYREMOTE_DEFAULT_PORT);
    bool remoteInputEnabled = false;
    SecurityProfile securityProfile = SecurityProfile::Insecure;
    // A non-secret descriptor path is allowed at process start. The descriptor references secret
    // files; raw passwords/private keys are never accepted in the -platform argument string.
    QString securityConfigFile;
};

// Removes HyRemote-owned platform parameters from `parameters` while leaving native delegate
// parameters untouched. Invalid HyRemote values fail closed and provide a user-facing diagnostic.
bool parseRemoteConfig(QStringList &parameters, RemoteConfig &config, QString &error);

// Runtime identity of the exact-Qt qualification claim. The payload is compiled against one exact Qt
// private ABI (a static_assert in the plugin) and the deploy helper checks a deployed payload against
// the Qt it will run with, but neither covers a process whose Qt differs at run time. This is the check
// that runs before HyRemote creates a delegate or starts the shared RemoteAccess runtime.
//
// It is declared in this internal, uninstalled header - so no public QPA API is added - and it is a
// pure function over strings, which gives the rejection path a deterministic negative test that does
// not need a second Qt installation. An empty result means accepted; otherwise the text names the exact
// requirement and the version that was actually found.
QString runtimeIdentityError(const QString &runningVersion,
                             int expectedMajor,
                             int expectedMinor,
                             int expectedPatch);

// QPA-04 controller for Transparent QPA mode.
//
// One long-lived InteractiveCompositeTarget represents the current Qt application's eligible
// top-level surfaces. RemoteAccess is started at most once after the first visible surface appears;
// later window/dialog/popup churn mutates that composite target without replacing the Core Session
// or Transport listener. Native qwindows/qxcb ownership and local input/display remain untouched.
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
    struct SurfaceCandidate
    {
        QPointer<QObject> target;
        QRect globalGeometry;
        bool visible = false;
    };

    void scheduleRefresh();
    void refresh();
    bool ensureRuntimeStarted();
    void stopCurrent() noexcept;

    QList<SurfaceCandidate> candidateSurfaces() const;
    QObject *activeSurfaceTarget(const QList<SurfaceCandidate> &surfaces) const;

    RemoteConfig m_config;
    std::unique_ptr<::HyRemote::RemoteAccess> m_access;
    std::unique_ptr<InteractiveCompositeTarget> m_compositeTarget;

    // QObject addresses are used only as identity keys on the Qt GUI thread. Entries are removed
    // during the deferred refresh after destruction; no stale key is dereferenced.
    QHash<QObject *, quint64> m_surfaceIds;
    QSet<QObject *> m_raisePending;
    quint64 m_nextSurfaceId = 1;
    std::optional<quint64> m_activeSurfaceId;

    bool m_started = false;
    bool m_refreshQueued = false;
};

}  // namespace HyRemote::Qpa
