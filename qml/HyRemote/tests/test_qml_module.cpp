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

void testDeclarativeImportAndSafeDefaults()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(HYREMOTE_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"QML(
        import HyRemote 1.0
        RemoteAccess {
            port: 5901
            remoteInputEnabled: true
        }
    )QML",
                      QUrl(QStringLiteral("inline:hyremote-test.qml")));
    waitForComponent(component);
    CHECK(component.status() == QQmlComponent::Ready);

    std::unique_ptr<QObject> object(component.create());
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

    QQmlComponent component(&engine);
    component.setData(R"QML(
        import HyRemote 1.0
        RemoteAccess {}
    )QML",
                      QUrl(QStringLiteral("inline:hyremote-invalid-config.qml")));
    waitForComponent(component);
    CHECK(component.status() == QQmlComponent::Ready);

    std::unique_ptr<QObject> object(component.create());
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

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testDeclarativeImportAndSafeDefaults();
    testInvalidConfigurationDoesNotMutateAcceptedValue();

    if (failures != 0)
        std::cerr << failures << " QML module checks failed\n";
    return failures == 0 ? 0 : 1;
}
