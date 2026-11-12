// Standalone public QtCore API smoke: verify Qt 6.8 QMetaProperty NOTIFY
// -> QMetaMethod QTimer::start connection, without any renderer or capture.
#include <QCoreApplication>
#include <QMetaMethod>
#include <QMetaObject>
#include <QMetaProperty>
#include <QObject>
#include <QTimer>

#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QObject source;
    const int index = source.metaObject()->indexOfProperty("objectName");
    if (index < 0 || !source.metaObject()->property(index).hasNotifySignal())
        return 11;
    QTimer coalescer;
    coalescer.setSingleShot(true);
    coalescer.setInterval(0);
    const int slotIndex = coalescer.metaObject()->indexOfSlot("start()");
    if (slotIndex < 0)
        return 12;
    const QMetaObject::Connection connection =
        QObject::connect(&source, source.metaObject()->property(index).notifySignal(),
                         &coalescer, coalescer.metaObject()->method(slotIndex),
                         Qt::AutoConnection);
    if (!connection)
        return 13;
    int observedWakeups = 0;
    QObject::connect(&coalescer, &QTimer::timeout, &app, [&] {
        ++observedWakeups;
    });
    // Two independent mutations in one GUI/event-loop turn must be bounded
    // to one wake. A later true mutation generates one further wake.
    source.setObjectName(QStringLiteral("A"));
    source.setObjectName(QStringLiteral("B"));
    QTimer::singleShot(25, &app, [&] {
        source.setObjectName(QStringLiteral("C"));
    });
    QTimer::singleShot(80, &app, &QCoreApplication::quit);
    const int status = app.exec();
    if (status != 0 || observedWakeups != 2) {
        std::cerr << "FAIL Qt QMetaProperty NOTIFY to QTimer::start: wakeups="
                  << observedWakeups << " status=" << status << '\n';
        return 14;
    }
    std::cout << "PASS Qt QMetaProperty NOTIFY -> QTimer::start "
                 "coalesces two same-turn changes and detects later source mutation\n";
    return 0;
}
