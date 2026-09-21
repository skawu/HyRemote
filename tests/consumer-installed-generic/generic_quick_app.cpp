#include <QGenericPluginFactory>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSizeF>
#include <QStringList>
#include <QTimer>

#include <cstdio>

namespace {

// Platforms Qt itself provides. The point of this list is not to enumerate every Qt platform, but to refuse a
// verdict when the application is running on something the HyRemote Generic Plugin installed.
const char *const kNativelyProvidedPlatforms[] = {
    "windows", "xcb", "wayland", "offscreen", "minimal", "eglfs", "vnc", "cocoa", "directfb",
};

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    // Assertion 1, standing alone: adding the Generic Plugin must not change the application's platform identity.
    const QString platform = QGuiApplication::platformName();
    if (platform.compare(QStringLiteral("hyremote"), Qt::CaseInsensitive) == 0) {
        std::fprintf(stderr,
                     "FAIL: Generic Plugin replaced the native Qt platform with '%s'\n",
                     qPrintable(platform));
        return 2;
    }
    bool nativelyProvided = false;
    for (const char *candidate : kNativelyProvidedPlatforms) {
        if (platform.compare(QLatin1String(candidate), Qt::CaseInsensitive) == 0) {
            nativelyProvided = true;
            break;
        }
    }
    if (!nativelyProvided) {
        std::fprintf(stderr,
                     "FAIL: deployed Generic consumer ran on an unexpected platform '%s'\n",
                     qPrintable(platform));
        return 2;
    }
    std::printf("HYREMOTE_GENERIC_QUICK_NATIVE_PLATFORM=%s\n", qPrintable(platform));

    // Assertion 2 is deliberately separate: a plugin that loaded is not evidence that native platform identity
    // survived, so the two facts are proved independently instead of one standing in for the other.
    const QStringList keys = QGenericPluginFactory::keys();
    if (!keys.contains(QStringLiteral("hyremote"), Qt::CaseInsensitive)) {
        std::fprintf(stderr,
                     "FAIL: deployed Generic Plugin is not discoverable; generic keys=[%s]\n",
                     qPrintable(keys.join(QLatin1Char(','))));
        return 3;
    }

    int port = 5938;
    bool portConfigured = false;
    const int configuredPort = qEnvironmentVariableIntValue("HYREMOTE_GENERIC_PROBE_PORT", &portConfigured);
    if (portConfigured) {
        port = configuredPort;
    }
    QObject *plugin = QGenericPluginFactory::create(QStringLiteral("hyremote"),
                                                    QStringLiteral("port=%1").arg(port));
    if (!plugin) {
        std::fprintf(stderr, "FAIL: deployed Generic Plugin refused 'port=%d'\n", port);
        return 4;
    }
    std::printf("HYREMOTE_GENERIC_QUICK_PLUGIN=loaded\n");

    // The scene graph is built in C++ on purpose. A Qt-only Generic consumer is not a QML consumer: loading a QML
    // document would make this smoke depend on QML modules the deployment never scans, and QML module deployment is
    // the QML frontend's evidence rather than this contract's. What must hold here is that a Qt Quick window opens
    // and renders while the Generic Plugin is active and the native platform identity is intact.
    QQuickWindow window;
    window.resize(320, 200);
    window.setTitle(QStringLiteral("HyRemote Generic consumer"));
    QQuickItem *rootItem = new QQuickItem;
    rootItem->setParentItem(window.contentItem());
    rootItem->setSize(QSizeF(320, 200));
    window.show();

    QTimer::singleShot(1200, &app, [&app] { app.exit(0); });
    return app.exec();
}
