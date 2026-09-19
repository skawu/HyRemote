#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
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

    CHECK(!object->property("enabled").toBool());
    CHECK(object->property("state").toInt() == 0); // Stopped
    CHECK(object->property("connectedClientCount").toULongLong() == 0);
    CHECK(object->property("listenAddress").toString() == QStringLiteral("127.0.0.1"));
    CHECK(object->property("port").toInt() == 5901);
    CHECK(object->property("remoteInputEnabled").toBool());
    CHECK(object->property("securityProfile").toInt() == 0); // Insecure compatibility profile
    CHECK(object->property("securityConfigFile").toString().isEmpty());
    CHECK(object->property("errorCode").toInt() == 0); // NoError

    CHECK(!object->setProperty("connectedClientCount", QVariant::fromValue<qulonglong>(1)));
    CHECK(object->property("connectedClientCount").toULongLong() == 0);
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

    // #170 freezes one transport-neutral profile and a non-secret descriptor path. Raw secret
    // values are intentionally absent from the QML object model.
    CHECK(object->setProperty("securityProfile", 1)); // Authenticated
    CHECK(object->property("securityProfile").toInt() == 1);
    CHECK(object->setProperty("securityConfigFile", QStringLiteral("support-security.conf")));
    CHECK(object->property("securityConfigFile").toString()
          == QStringLiteral("support-security.conf"));
    CHECK(!object->property("password").isValid());
    CHECK(!object->property("authenticationEnabled").isValid());
    CHECK(object->setProperty("securityProfile", 0));
    CHECK(object->property("securityProfile").toInt() == 0);
}

void testEnabledStartFailureIsTransactional()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(HYREMOTE_QML_IMPORT_PATH));

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
    CHECK(object->property("connectedClientCount").toULongLong() == 0);
    CHECK(object->property("errorCode").toInt() == 1); // InvalidConfiguration: no target
    CHECK(!object->property("errorString").toString().isEmpty());
}

void testInitialEnabledDoesNotRaceLaterTargetBinding()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(HYREMOTE_QML_IMPORT_PATH));

    // The declaration deliberately requests enabled before assigning target. QQmlParserStatus must
    // defer start until componentComplete(), at which point the target is present. A plain QtObject
    // is intentionally unsupported, so the expected final error is TargetAdapterUnavailable rather
    // than InvalidConfiguration/no-target. This distinguishes correct deferred startup from a setter-
    // order race without requiring a GUI target in this deterministic unit test.
    std::unique_ptr<QObject> object = createInline(
        engine,
        R"QML(
            import QtQml
            import HyRemote 1.0
            RemoteAccess {
                enabled: true
                property QtObject dummyTarget: QtObject {}
                target: dummyTarget
            }
        )QML",
        "inline:hyremote-deferred-enabled.qml");
    CHECK(object != nullptr);
    if (!object)
        return;

    CHECK(object->property("target").value<QObject *>() != nullptr);
    CHECK(!object->property("enabled").toBool());
    CHECK(object->property("state").toInt() == 0); // Stopped after transactional failure
    CHECK(object->property("errorCode").toInt() == 2); // TargetAdapterUnavailable
}

void testTargetDestructionNotifiesDeclarativeProperty()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(HYREMOTE_QML_IMPORT_PATH));

    std::unique_ptr<QObject> object = createInline(
        engine,
        R"QML(
            import QtQml
            import HyRemote 1.0
            RemoteAccess {
                property QtObject dummyTarget: QtObject {}
                property int targetChangeCount: 0
                target: dummyTarget
                onTargetChanged: ++targetChangeCount
            }
        )QML",
        "inline:hyremote-target-lifetime.qml");
    CHECK(object != nullptr);
    if (!object)
        return;

    QObject *target = object->property("target").value<QObject *>();
    CHECK(target != nullptr);
    if (!target)
        return;

    const int before = object->property("targetChangeCount").toInt();
    delete target;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    CHECK(object->property("target").value<QObject *>() == nullptr);
    CHECK(object->property("targetChangeCount").toInt() == before + 1);
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testDeclarativeImportAndSafeDefaults();
    testInvalidConfigurationDoesNotMutateAcceptedValue();
    testEnabledStartFailureIsTransactional();
    testInitialEnabledDoesNotRaceLaterTargetBinding();
    testTargetDestructionNotifiesDeclarativeProperty();

    if (failures != 0)
        std::cerr << failures << " QML module checks failed\n";
    return failures == 0 ? 0 : 1;
}
