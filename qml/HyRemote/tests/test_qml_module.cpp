#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTimer>
#include <QVariant>

#include <iostream>
#include <memory>

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

void waitForComponent(QQmlComponent &component)
{
    QElapsedTimer timer;
    timer.start();
    while (component.status() == QQmlComponent::Loading && timer.elapsed() < 5000) {
        // processEvents(maxTime) does not wait when the queue is momentarily empty. Give dynamic
        // QML plugin/type loading a bounded real event-loop slice so Windows and Linux exercise the
        // same asynchronous import contract.
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::AllEvents);
    }

    if (component.isError() || component.status() != QQmlComponent::Ready) {
        std::cerr << "QML component status=" << static_cast<int>(component.status()) << '\n';
        const auto errors = component.errors();
        for (const QQmlError &error : errors)
            std::cerr << error.toString().toStdString() << '\n';
    }
}

std::unique_ptr<QObject> createInline(QQmlEngine &engine, const char *qml, const char *name)
{
    QQmlComponent component(&engine);
    component.setData(qml, QUrl(QString::fromLatin1(name)));
    waitForComponent(component);
    CHECK(component.status() == QQmlComponent::Ready);
    if (component.status() != QQmlComponent::Ready)
        return {};
    return std::unique_ptr<QObject>(component.create());
}

void testDeclarativeImportAndSafeDefaults()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(HYREMOTE_QML_IMPORT_PATH));

    std::unique_ptr<QObject> object = createInline(
        engine,
        R"QML(
            import HyRemote 1.0
            RemoteAccess {
                port: 5901
                remoteInputEnabled: true
            }
        )QML",
        "inline:hyremote-test.qml");
    CHECK(object != nullptr);
    if (!object)
        return;

    // Construction must stay inert: the QML wrapper has the same no-implicit-listener contract as
    // the public C++ facade. Configuration is accepted while Stopped.
    CHECK(!object->property("enabled").toBool());
    CHECK(object->property("state").toInt() == 0); // Stopped
    CHECK(object->property("listenAddress").toString() == QStringLiteral("127.0.0.1"));
    CHECK(object->property("port").toInt() == 5901);
    CHECK(object->property("remoteInputEnabled").toBool());
    CHECK(object->property("errorCode").toInt() == 0); // NoError
}

void testInvalidConfigurationDoesNotMutateAcceptedValue()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(HYREMOTE_QML_IMPORT_PATH));

    std::unique_ptr<QObject> object = createInline(
        engine,
        R"QML(
            import HyRemote 1.0
            RemoteAccess {}
        )QML",
        "inline:hyremote-invalid-config.qml");
    CHECK(object != nullptr);
    if (!object)
        return;

    CHECK(object->property("port").toInt() == 5900);
    CHECK(object->setProperty("port", 0));
    CHECK(object->property("port").toInt() == 5900);
    CHECK(object->property("errorCode").toInt() == 1); // InvalidConfiguration
    CHECK(!object->property("errorString").toString().isEmpty());

    CHECK(QMetaObject::invokeMethod(object.get(), "clearError"));
    CHECK(object->property("errorCode").toInt() == 0);
    CHECK(object->property("errorString").toString().isEmpty());
}

void testEnabledStartFailureIsTransactional()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(HYREMOTE_QML_IMPORT_PATH));

    // A declarative request to enable the service is allowed, but with no target the shared C++
    // facade must reject start(). The wrapper must roll the request back instead of leaving QML
    // claiming that a listener/service is enabled.
    std::unique_ptr<QObject> object = createInline(
        engine,
        R"QML(
            import HyRemote 1.0
            RemoteAccess {
                enabled: true
            }
        )QML",
        "inline:hyremote-transactional-start.qml");
    CHECK(object != nullptr);
    if (!object)
        return;

    CHECK(!object->property("enabled").toBool());
    CHECK(object->property("state").toInt() == 0); // Stopped
    CHECK(object->property("errorCode").toInt() == 1); // InvalidConfiguration: no live target
    CHECK(!object->property("errorString").toString().isEmpty());
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testDeclarativeImportAndSafeDefaults();
    testInvalidConfigurationDoesNotMutateAcceptedValue();
    testEnabledStartFailureIsTransactional();

    if (failures != 0)
        std::cerr << failures << " QML module checks failed\n";
    return failures == 0 ? 0 : 1;
}
