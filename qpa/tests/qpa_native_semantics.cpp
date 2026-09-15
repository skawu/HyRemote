#include <QGuiApplication>
#include <QKeyEvent>
#include <QWindow>

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qguiapplication_platform.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformkeymapper.h>
#include <qpa/qplatformopenglcontext.h>

#include <cstdio>

namespace {

bool require(bool condition, const char *message)
{
    if (!condition)
        std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    if (!require(QGuiApplication::platformName().compare(QStringLiteral("hyremote"), Qt::CaseInsensitive) == 0,
                 "HyRemote QPA plugin must be active"))
        return 2;

    QPlatformIntegration *integration = QGuiApplicationPrivate::platformIntegration();
    if (!require(integration != nullptr, "platform integration must exist"))
        return 3;

    // Qt 6.8.3 routes public keyboard-modifier and shortcut-combination queries through the
    // platform key mapper. HyRemote must keep the reference delegate's native mapper authoritative.
    QPlatformKeyMapper *mapper = integration->keyMapper();
    if (!require(mapper != nullptr, "qualified native delegate must provide a key mapper"))
        return 4;

    const Qt::KeyboardModifiers mapperModifiers = mapper->queryKeyboardModifiers();
    if (!require(QGuiApplication::queryKeyboardModifiers() == mapperModifiers,
                 "QGuiApplication modifier query must resolve through the native mapper"))
        return 5;

    QKeyEvent keyEvent(QEvent::KeyPress,
                       Qt::Key_A,
                       Qt::ShiftModifier,
                       QStringLiteral("A"));
    (void) mapper->possibleKeyCombinations(&keyEvent);

#ifdef Q_OS_WIN
    using WindowsApplication = QNativeInterface::Private::QWindowsApplication;
    auto *windowsApplication = dynamic_cast<WindowsApplication *>(integration);
    if (!require(windowsApplication != nullptr,
                 "proxy must preserve QWindowsApplication native-interface dynamic_cast"))
        return 6;

    // Exercise a read-only method so the test proves forwarding, not only inheritance.
    (void) windowsApplication->windowActivationBehavior();
    (void) windowsApplication->darkModeHandling();

#ifndef QT_NO_OPENGL
    auto *windowsGl = dynamic_cast<QNativeInterface::Private::QWindowsGLIntegration *>(integration);
    if (!require(windowsGl != nullptr,
                 "proxy must preserve QWindowsGLIntegration native-interface dynamic_cast"))
        return 7;
    (void) windowsGl->openGLModuleHandle();
#endif
#elif defined(Q_OS_LINUX)
    QPlatformNativeInterface *nativeInterface = integration->nativeInterface();
    if (!require(nativeInterface != nullptr, "qxcb native interface must remain delegated"))
        return 8;

#if QT_CONFIG(xcb)
    auto *x11 = dynamic_cast<QNativeInterface::QX11Application *>(nativeInterface);
    if (!require(x11 != nullptr, "qxcb QX11Application native interface must remain reachable"))
        return 9;
    if (!require(x11->connection() != nullptr, "qxcb native connection must remain live"))
        return 10;
#endif

#ifndef QT_NO_OPENGL
#if QT_CONFIG(xcb_glx_plugin)
    if (!require(dynamic_cast<QNativeInterface::Private::QGLXIntegration *>(integration) != nullptr,
                 "proxy must preserve QGLXIntegration dynamic_cast when Qt provides it"))
        return 11;
#endif
#if QT_CONFIG(egl)
    if (!require(dynamic_cast<QNativeInterface::Private::QEGLIntegration *>(integration) != nullptr,
                 "proxy must preserve QEGLIntegration dynamic_cast when Qt provides it"))
        return 12;
#endif
#endif
#endif

    QWindow window;
    window.resize(320, 180);
    window.show();
    QCoreApplication::processEvents();
    if (!require(window.handle() != nullptr,
                 "native delegate must still create and retain the real platform window"))
        return 13;

    std::printf("PASS: exact-Qt native interfaces, key mapper and native window semantics are preserved\n");
    return 0;
}
