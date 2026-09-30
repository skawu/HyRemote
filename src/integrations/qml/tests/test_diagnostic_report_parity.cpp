#include <HyRemote/RemoteAccess.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QObject>
#include <QString>

#include <iostream>
#include <memory>

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';          \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    HyRemote::RemoteAccess cppAccess;
    CHECK(cppAccess.setPort(5933));
    CHECK(cppAccess.setRemoteInputEnabled(true));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(HYREMOTE_QML_IMPORT_PATH));
    QQmlComponent component(&engine);
    component.setData("import HyRemote\nRemoteAccess { port: 5933; remoteInputEnabled: true }", QUrl());
    if (component.isError()) {
        for (const QQmlError &error : component.errors())
            std::cerr << error.toString().toStdString() << '\n';
        return 1;
    }

    std::unique_ptr<QObject> qmlAccess(component.create());
    CHECK(static_cast<bool>(qmlAccess));
    if (!qmlAccess)
        return 1;

    QString qmlReport;
    const bool invoked = QMetaObject::invokeMethod(qmlAccess.get(),
                                                    "diagnosticReport",
                                                    Q_RETURN_ARG(QString, qmlReport));
    CHECK(invoked);

    const QString cppReport = cppAccess.diagnosticReport();
    CHECK(!cppReport.isEmpty());
    CHECK(qmlReport == cppReport);
    CHECK(cppReport.contains(QStringLiteral("STATE=Stopped\n")));
    CHECK(cppReport.contains(QStringLiteral("LISTENER_CONFIGURED=address:0.0.0.0:5933\n")));
    CHECK(cppReport.contains(QStringLiteral("REMOTE_INPUT=true\n")));

    if (failures == 0) {
        std::cout << "C++/QML diagnostic report parity passed\n";
        return 0;
    }

    std::cerr << failures << " diagnostic report parity check(s) failed\n";
    return 1;
}
