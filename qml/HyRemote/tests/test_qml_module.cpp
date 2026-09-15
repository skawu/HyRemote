#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QThread>
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
    while (component.status() == QQmlComponent::Loading && timer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(1);
    }

    if (component.isError() || component.status() != QQmlComponent::Ready) {
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
                      QUrl());
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
                      QUrl());
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
