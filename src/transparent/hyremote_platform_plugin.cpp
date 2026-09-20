#include "hyremote_qpa_interception.hpp"
#include "hyremote_qpa_remote_controller.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtGui/QPalette>
#include <QtGui/private/qguiapplication_p.h>
// Qt 6.8.3 ships this header under QtGui/qpa/, not QtGui/private/: the factory lives in the QPA
// header set (compare qplatformintegration.h / qplatformintegrationplugin.h below, which are already
// included in the qpa/ form). The private/ path does not exist in the exact qualified Qt build, so the
// QPA module failed to compile on every platform while the rest of the graph linked normally.
#include <qpa/qplatformintegrationfactory_p.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformintegrationplugin.h>
#include <qpa/qplatformkeymapper.h>
#include <qpa/qplatformopenglcontext.h>

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

    // The transparent product mode decorates only the qualified native delegate. Never silently
    // degrade to replacement/headless qvnc, offscreen or minimal behavior.
    if (selected != native)
        return {};
    return selected;
}

class HyRemotePlatformIntegration final : public QPlatformIntegration
#ifdef Q_OS_WIN
#ifndef QT_NO_OPENGL
    , public QNativeInterface::Private::QWindowsGLIntegration
#endif
    , public QNativeInterface::Private::QWindowsApplication
#elif defined(Q_OS_LINUX)
#ifndef QT_NO_OPENGL
#if QT_CONFIG(xcb_glx_plugin)
    , public QNativeInterface::Private::QGLXIntegration
#endif
#if QT_CONFIG(egl)
    , public QNativeInterface::Private::QEGLIntegration
#endif
#endif
#endif
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
        QPlatformWindow *platformWindow = m_delegate->createPlatformWindow(window);
        if (platformWindow) {
            m_interception.observePlatformWindowCreated(window);
            ensureRemoteControllerStarted();
        }
        return platformWindow;
    }
    QPlatformWindow *createForeignWindow(QWindow *window, WId id) const override
    {
        QPlatformWindow *platformWindow = m_delegate->createForeignWindow(window, id);
        if (platformWindow) {
            m_interception.observePlatformWindowCreated(window);
            ensureRemoteControllerStarted();
        }
        return platformWindow;
    }
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override
    {
        QPlatformBackingStore *backingStore = m_delegate->createPlatformBackingStore(window);
        if (backingStore)
            m_interception.observeBackingStoreCreated(window);
        return backingStore;
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
        // Loading/initializing the QPA plugin itself must never open the remote listener. The
        // shared RemoteAccess controller is armed only after the native delegate successfully
        // creates a real platform window; it still waits for a supported visible target.
        m_delegate->initialize();
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

#ifdef Q_OS_WIN
    using WindowsApplication = QNativeInterface::Private::QWindowsApplication;

    void setTouchWindowTouchType(WindowsApplication::TouchWindowTouchTypes type) override
    {
        m_delegate->call<&WindowsApplication::setTouchWindowTouchType>(type);
    }
    WindowsApplication::TouchWindowTouchTypes touchWindowTouchType() const override
    {
        return m_delegate->call<&WindowsApplication::touchWindowTouchType>();
    }
    WindowsApplication::WindowActivationBehavior windowActivationBehavior() const override
    {
        return m_delegate->call<&WindowsApplication::windowActivationBehavior>();
    }
    void setWindowActivationBehavior(WindowsApplication::WindowActivationBehavior behavior) override
    {
        m_delegate->call<&WindowsApplication::setWindowActivationBehavior>(behavior);
    }
    void setHasBorderInFullScreenDefault(bool border) override
    {
        m_delegate->call<&WindowsApplication::setHasBorderInFullScreenDefault>(border);
    }
    bool isTabletMode() const override { return m_delegate->call<&WindowsApplication::isTabletMode>(); }
    bool isWinTabEnabled() const override
    {
        return m_delegate->call<&WindowsApplication::isWinTabEnabled>();
    }
    bool setWinTabEnabled(bool enabled) override
    {
        return m_delegate->call<&WindowsApplication::setWinTabEnabled>(enabled);
    }
    WindowsApplication::DarkModeHandling darkModeHandling() const override
    {
        return m_delegate->call<&WindowsApplication::darkModeHandling>();
    }
    void setDarkModeHandling(WindowsApplication::DarkModeHandling handling) override
    {
        m_delegate->call<&WindowsApplication::setDarkModeHandling>(handling);
    }
    void registerMime(QWindowsMimeConverter *mime) override
    {
        m_delegate->call<&WindowsApplication::registerMime>(mime);
    }
    void unregisterMime(QWindowsMimeConverter *mime) override
    {
        m_delegate->call<&WindowsApplication::unregisterMime>(mime);
    }
    int registerMimeType(const QString &mime) override
    {
        return m_delegate->call<&WindowsApplication::registerMimeType>(mime);
    }
    HWND createMessageWindow(const QString &classNameTemplate,
                             const QString &windowName,
                             QFunctionPointer eventProc) const override
    {
        return m_delegate->call<&WindowsApplication::createMessageWindow>(classNameTemplate,
                                                                          windowName,
                                                                          eventProc);
    }
    bool asyncExpose() const override { return m_delegate->call<&WindowsApplication::asyncExpose>(); }
    void setAsyncExpose(bool value) override
    {
        m_delegate->call<&WindowsApplication::setAsyncExpose>(value);
    }
    QVariant gpu() const override { return m_delegate->call<&WindowsApplication::gpu>(); }
    QVariant gpuList() const override { return m_delegate->call<&WindowsApplication::gpuList>(); }
    void populateLightSystemPalette(QPalette &palette) const override
    {
        m_delegate->call<&WindowsApplication::populateLightSystemPalette>(palette);
    }

#ifndef QT_NO_OPENGL
    HMODULE openGLModuleHandle() const override
    {
        return m_delegate->call<&QNativeInterface::Private::QWindowsGLIntegration::openGLModuleHandle>();
    }
    QOpenGLContext *createOpenGLContext(HGLRC context,
                                        HWND window,
                                        QOpenGLContext *shareContext) const override
    {
        return m_delegate->call<&QNativeInterface::Private::QWindowsGLIntegration::createOpenGLContext>(
            context, window, shareContext);
    }
#endif
#elif defined(Q_OS_LINUX)
#ifndef QT_NO_OPENGL
#if QT_CONFIG(xcb_glx_plugin)
    QOpenGLContext *createOpenGLContext(GLXContext context,
                                        void *visualInfo,
                                        QOpenGLContext *shareContext) const override
    {
        return m_delegate->call<&QNativeInterface::Private::QGLXIntegration::createOpenGLContext>(
            context, visualInfo, shareContext);
    }
#endif
#if QT_CONFIG(egl)
    QOpenGLContext *createOpenGLContext(EGLContext context,
                                        EGLDisplay display,
                                        QOpenGLContext *shareContext) const override
    {
        return m_delegate->call<&QNativeInterface::Private::QEGLIntegration::createOpenGLContext>(
            context, display, shareContext);
    }
#endif
#endif
#endif

protected:
    Qt::KeyboardModifiers queryKeyboardModifiers() const override
    {
        if (QPlatformKeyMapper *mapper = m_delegate->keyMapper())
            return mapper->queryKeyboardModifiers();
        return QPlatformIntegration::queryKeyboardModifiers();
    }

    QList<int> possibleKeys(const QKeyEvent *event) const override
    {
        QList<int> result;
        if (QPlatformKeyMapper *mapper = m_delegate->keyMapper()) {
            const auto combinations = mapper->possibleKeyCombinations(event);
            result.reserve(combinations.size());
            for (const auto &combination : combinations)
                result.push_back(combination.toCombined());
        }
        return result;
    }

private:
    void ensureRemoteControllerStarted() const
    {
        if (m_remoteController || m_remoteArmRefused)
            return;

        auto controller = std::make_unique<::HyRemote::Qpa::RemoteController>(m_remoteConfig);
        if (!controller->start()) {
            // This is reached from window events (show, resize, move, activate, ...), so without the
            // latch the same unstartable controller would be rebuilt - and the same warning reissued -
            // on every window change. Report once and stop: the conditions that made it fail are not
            // changed by another attempt.
            m_remoteArmRefused = true;
            qWarning() << "HyRemote QPA Proxy could not arm automatic RemoteAccess composition; "
                          "this is reported once and not retried";
            return;
        }
        m_remoteController = std::move(controller);
    }

    // Destruction is reverse declaration order: controller is torn down first, then its config,
    // then the QPA-02 observation seam, and finally the native delegate. This prevents automatic
    // remote runtime callbacks from outliving native-semantics qualification state.
    std::unique_ptr<QPlatformIntegration> m_delegate;
    mutable ::HyRemote::Qpa::Internal::InterceptionSeam m_interception;
    ::HyRemote::Qpa::RemoteConfig m_remoteConfig;
    mutable std::unique_ptr<::HyRemote::Qpa::RemoteController> m_remoteController;
    // Latches a failed arming attempt so later window events cannot retry it indefinitely.
    mutable bool m_remoteArmRefused = false;
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

        // Runtime identity first: this payload is qualified for one exact Qt private ABI, so a runtime that
        // is not that exact version is rejected here - before a delegate is created, and long before the
        // shared RemoteAccess runtime can be armed by the first visible window.
        const QString identityError =
            ::HyRemote::Qpa::runtimeIdentityError(QString::fromLatin1(qVersion()),
                                                 HYREMOTE_QPA_QT_VERSION_MAJOR,
                                                 HYREMOTE_QPA_QT_VERSION_MINOR,
                                                 HYREMOTE_QPA_QT_VERSION_PATCH);
        if (!identityError.isEmpty()) {
            qCritical().noquote() << identityError;
            return nullptr;
        }

        QStringList delegateParameters = paramList;
        ::HyRemote::Qpa::RemoteConfig remoteConfig;
        QString remoteConfigError;
        if (!::HyRemote::Qpa::parseRemoteConfig(delegateParameters, remoteConfig, remoteConfigError)) {
            qWarning() << "HyRemote QPA Proxy rejected remote configuration:" << remoteConfigError;
            return nullptr;
        }

        const QString delegateName = requestedDelegate(delegateParameters);
        if (delegateName.isEmpty()) {
            // One actionable diagnostic. The accepted set is not broadened and there is no fallback to
            // another delegate: a rejected delegate ends this process's platform setup.
            qCritical().noquote()
                << QStringLiteral("HyRemote QPA Proxy rejected the requested platform delegate; this payload only "
                                  "decorates '%1' for the exact Qt %2.%3.%4 private ABI, and never falls back to "
                                  "another delegate.")
                       .arg(referenceNativeDelegate())
                       .arg(HYREMOTE_QPA_QT_VERSION_MAJOR)
                       .arg(HYREMOTE_QPA_QT_VERSION_MINOR)
                       .arg(HYREMOTE_QPA_QT_VERSION_PATCH);
            return nullptr;
        }

        std::unique_ptr<QPlatformIntegration> delegate(
            QPlatformIntegrationFactory::create(delegateName,
                                                delegateParameters,
                                                argc,
                                                argv));
        if (!delegate) {
            // One actionable diagnostic: the delegate that was required, where Qt looked for platform
            // plugins, and the exact Qt this payload requires.
            qCritical().noquote()
                << QStringLiteral("HyRemote QPA Proxy could not create the native delegate '%1' required for the "
                                  "exact Qt %2.%3.%4 private ABI; platform plugin path '%5'; library paths: %6.")
                       .arg(delegateName)
                       .arg(HYREMOTE_QPA_QT_VERSION_MAJOR)
                       .arg(HYREMOTE_QPA_QT_VERSION_MINOR)
                       .arg(HYREMOTE_QPA_QT_VERSION_PATCH)
                       .arg(QDir::toNativeSeparators(
                           QLibraryInfo::path(QLibraryInfo::PluginsPath)),
                            QCoreApplication::libraryPaths().join(QStringLiteral(", ")));
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
