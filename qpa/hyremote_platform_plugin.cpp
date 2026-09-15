#include "hyremote_qpa_remote_controller.hpp"

#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtGui/private/qplatformintegrationfactory_p.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformintegrationplugin.h>

#include <memory>
#include <utility>

QT_BEGIN_NAMESPACE
namespace {

static_assert(QT_VERSION == QT_VERSION_CHECK(HYREMOTE_QPA_QT_VERSION_MAJOR,
                                              HYREMOTE_QPA_QT_VERSION_MINOR,
                                              HYREMOTE_QPA_QT_VERSION_PATCH),
              "HyRemote QPA Proxy must be rebuilt for the exact supported Qt private ABI");

QString referenceNativeDelegate()
{
#ifdef Q_OS_WIN
    return QStringLiteral("windows");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("xcb");
#else
    return {};
#endif
}

QString requestedDelegate(QStringList &parameters)
{
    const QString native = referenceNativeDelegate();
    QString selected = native;

    for (auto it = parameters.begin(); it != parameters.end();) {
        constexpr auto prefix = "delegate=";
        if (it->startsWith(QLatin1String(prefix), Qt::CaseInsensitive)) {
            selected = it->mid(static_cast<int>(sizeof(prefix) - 1)).trimmed().toLower();
            it = parameters.erase(it);
        } else {
            ++it;
        }
    }

    // The transparent mode may only decorate the reference native platform. Never permit this path
    // to silently become replacement-only qvnc/offscreen/minimal behavior.
    if (selected != native)
        return {};
    return selected;
}

class HyRemotePlatformIntegration final : public QPlatformIntegration
{
public:
    HyRemotePlatformIntegration(std::unique_ptr<QPlatformIntegration> delegate,
                                ::HyRemote::Qpa::RemoteConfig remoteConfig)
        : m_delegate(std::move(delegate))
        , m_remoteConfig(std::move(remoteConfig))
    {
    }

    bool hasCapability(Capability cap) const override { return m_delegate->hasCapability(cap); }
    QPlatformPixmap *createPlatformPixmap(QPlatformPixmap::PixelType type) const override
    {
        return m_delegate->createPlatformPixmap(type);
    }
    QPlatformWindow *createPlatformWindow(QWindow *window) const override
    {
        return m_delegate->createPlatformWindow(window);
    }
    QPlatformWindow *createForeignWindow(QWindow *window, WId id) const override
    {
        return m_delegate->createForeignWindow(window, id);
    }
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override
    {
        return m_delegate->createPlatformBackingStore(window);
    }
#ifndef QT_NO_OPENGL
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const override
    {
        return m_delegate->createPlatformOpenGLContext(context);
    }
#endif
    QPlatformSharedGraphicsCache *createPlatformSharedGraphicsCache(const char *cacheId) const override
    {
        return m_delegate->createPlatformSharedGraphicsCache(cacheId);
    }
    QPaintEngine *createImagePaintEngine(QPaintDevice *paintDevice) const override
    {
        return m_delegate->createImagePaintEngine(paintDevice);
    }
    QAbstractEventDispatcher *createEventDispatcher() const override
    {
        return m_delegate->createEventDispatcher();
    }
    void initialize() override
    {
        m_delegate->initialize();
        m_remoteController = std::make_unique<::HyRemote::Qpa::RemoteController>(m_remoteConfig);
        if (!m_remoteController->start()) {
            qWarning() << "HyRemote QPA Proxy could not arm automatic RemoteAccess composition";
            m_remoteController.reset();
        }
    }
    void destroy() override
    {
        if (m_remoteController)
            m_remoteController->stop();
        m_remoteController.reset();
        m_delegate->destroy();
    }
    QPlatformFontDatabase *fontDatabase() const override { return m_delegate->fontDatabase(); }
#ifndef QT_NO_CLIPBOARD
    QPlatformClipboard *clipboard() const override { return m_delegate->clipboard(); }
#endif
#if QT_CONFIG(draganddrop)
    QPlatformDrag *drag() const override { return m_delegate->drag(); }
#endif
    QPlatformInputContext *inputContext() const override { return m_delegate->inputContext(); }
#if QT_CONFIG(accessibility)
    QPlatformAccessibility *accessibility() const override { return m_delegate->accessibility(); }
#endif
    QPlatformNativeInterface *nativeInterface() const override { return m_delegate->nativeInterface(); }
    QPlatformServices *services() const override { return m_delegate->services(); }
    QVariant styleHint(StyleHint hint) const override { return m_delegate->styleHint(hint); }
    Qt::WindowState defaultWindowState(Qt::WindowFlags flags) const override
    {
        return m_delegate->defaultWindowState(flags);
    }
    QPlatformKeyMapper *keyMapper() const override { return m_delegate->keyMapper(); }
    QStringList themeNames() const override { return m_delegate->themeNames(); }
    QPlatformTheme *createPlatformTheme(const QString &name) const override
    {
        return m_delegate->createPlatformTheme(name);
    }
    QPlatformOffscreenSurface *createPlatformOffscreenSurface(QOffscreenSurface *surface) const override
    {
        return m_delegate->createPlatformOffscreenSurface(surface);
    }
#ifndef QT_NO_SESSIONMANAGER
    QPlatformSessionManager *createPlatformSessionManager(const QString &id,
                                                           const QString &key) const override
    {
        return m_delegate->createPlatformSessionManager(id, key);
    }
#endif
    void sync() override { m_delegate->sync(); }
#ifndef QT_NO_OPENGL
    QOpenGLContext::OpenGLModuleType openGLModuleType() override
    {
        return m_delegate->openGLModuleType();
    }
#endif
    void setApplicationIcon(const QIcon &icon) const override { m_delegate->setApplicationIcon(icon); }
    void setApplicationBadge(qint64 number) override { m_delegate->setApplicationBadge(number); }
    void beep() const override { m_delegate->beep(); }
    void quit() const override { m_delegate->quit(); }
#if QT_CONFIG(vulkan)
    QPlatformVulkanInstance *createPlatformVulkanInstance(QVulkanInstance *instance) const override
    {
        return m_delegate->createPlatformVulkanInstance(instance);
    }
#endif

protected:
    Qt::KeyboardModifiers queryKeyboardModifiers() const override
    {
        // QPlatformIntegration exposes this hook as protected, so it cannot legally be invoked on an
        // arbitrary delegate object. Qt's base implementation returns the application modifier state;
        // exact native query/possible-key parity remains an explicit final QPA qualification item.
        return QPlatformIntegration::queryKeyboardModifiers();
    }

    QList<int> possibleKeys(const QKeyEvent *event) const override
    {
        return QPlatformIntegration::possibleKeys(event);
    }

private:
    std::unique_ptr<QPlatformIntegration> m_delegate;
    ::HyRemote::Qpa::RemoteConfig m_remoteConfig;
    std::unique_ptr<::HyRemote::Qpa::RemoteController> m_remoteController;
};

class HyRemotePlatformIntegrationPlugin final : public QPlatformIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "hyremote.json")

public:
    QPlatformIntegration *create(const QString &key,
                                 const QStringList &paramList,
                                 int &argc,
                                 char **argv) override
    {
        if (key.compare(QStringLiteral("hyremote"), Qt::CaseInsensitive) != 0)
            return nullptr;

        QStringList delegateParameters = paramList;
        ::HyRemote::Qpa::RemoteConfig remoteConfig;
        QString remoteConfigError;
        if (!::HyRemote::Qpa::parseRemoteConfig(delegateParameters, remoteConfig, remoteConfigError)) {
            qWarning() << "HyRemote QPA Proxy rejected remote configuration:" << remoteConfigError;
            return nullptr;
        }

        const QString delegateName = requestedDelegate(delegateParameters);
        if (delegateName.isEmpty()) {
            qWarning() << "HyRemote QPA Proxy rejected delegate; this build only permits"
                       << referenceNativeDelegate();
            return nullptr;
        }

        std::unique_ptr<QPlatformIntegration> delegate(
            QPlatformIntegrationFactory::create(delegateName,
                                                delegateParameters,
                                                argc,
                                                argv));
        if (!delegate) {
            qWarning() << "HyRemote QPA Proxy could not create native delegate" << delegateName;
            return nullptr;
        }

        qInfo() << "HyRemote QPA Proxy active; native delegate:" << delegateName
                << "remote address:" << remoteConfig.listenAddress.toString()
                << "port:" << remoteConfig.port
                << "remote input:" << remoteConfig.remoteInputEnabled;
        return new HyRemotePlatformIntegration(std::move(delegate), std::move(remoteConfig));
    }
};

}  // namespace
QT_END_NAMESPACE

#include "hyremote_platform_plugin.moc"
