#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtGui/private/qplatformintegrationfactory_p.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformintegrationplugin.h>

#include <memory>

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

    // Gate 01 deliberately allows only the reference native delegate. This prevents accidentally
    // turning the product mode into a headless/replacement stack such as vnc/offscreen/minimal.
    if (selected != native)
        return {};
    return selected;
}

class HyRemotePlatformIntegration final : public QPlatformIntegration
{
public:
    explicit HyRemotePlatformIntegration(std::unique_ptr<QPlatformIntegration> delegate)
        : m_delegate(std::move(delegate))
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
    void initialize() override { m_delegate->initialize(); }
    void destroy() override { m_delegate->destroy(); }
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
        // arbitrary delegate object here. The native key mapper and native event dispatcher remain
        // delegated; this protected query is intentionally left at Qt's base implementation for Gate 01.
        return QPlatformIntegration::queryKeyboardModifiers();
    }

    QList<int> possibleKeys(const QKeyEvent *event) const override
    {
        return QPlatformIntegration::possibleKeys(event);
    }

private:
    std::unique_ptr<QPlatformIntegration> m_delegate;
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
        const QString delegateName = requestedDelegate(delegateParameters);
        if (delegateName.isEmpty()) {
            qWarning() << "HyRemote QPA Proxy rejected delegate; Gate 01 only permits"
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

        qInfo() << "HyRemote QPA Proxy active; native delegate:" << delegateName;
        return new HyRemotePlatformIntegration(std::move(delegate));
    }
};

}  // namespace
QT_END_NAMESPACE

#include "hyremote_platform_plugin.moc"
