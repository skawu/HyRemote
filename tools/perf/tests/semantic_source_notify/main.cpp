#include "detail/semantic_source_property_observer.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QtGlobal>

#include <iostream>
#include <memory>

namespace {

void drain()
{
    QEventLoop loop;
    QTimer::singleShot(20, &loop, &QEventLoop::quit);
    loop.exec();
}

bool check(bool condition, const char *reason)
{
    if (!condition)
        std::cerr << "FAIL semantic source notifier: " << reason << '\n';
    return condition;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    using HyRemote::detail::SemanticSourcePropertyObserver;
    SemanticSourcePropertyObserver observer;
    QObject source;
    source.setObjectName(QStringLiteral("original"));
    int wakes = 0;

    if (!check(!observer.attach(&source, {}, [&] { ++wakes; }),
               "empty property set accepted"))
        return 10;
    if (!check(!observer.attach(&source, {QStringLiteral("unknown")}, [&] { ++wakes; }),
               "missing QMetaProperty accepted"))
        return 11;
    if (!check(!observer.attach(&source, {QStringLiteral("objectName"),
                                         QStringLiteral("objectName")},
                                [&] { ++wakes; }),
               "duplicate property accepted"))
        return 12;
    if (!check(!observer.active(), "failed registration retained active subscription"))
        return 13;

    if (!check(observer.attach(&source, {QStringLiteral("objectName")},
                               [&] { ++wakes; }),
               "valid Qt public QMetaProperty NOTIFY registration failed"))
        return 14;
    source.setObjectName(QStringLiteral("A"));
    source.setObjectName(QStringLiteral("B"));
    source.setObjectName(QStringLiteral("C"));
    drain();
    if (!check(wakes == 1, "three same-turn semantic mutations not coalesced"))
        return 15;

    source.setObjectName(QStringLiteral("C")); // no actual value change
    drain();
    if (!check(wakes == 1, "unchanged source value caused phantom NOTIFY"))
        return 16;

    source.setObjectName(QStringLiteral("D"));
    observer.detach(); // cancel source event before its 0ms callback
    drain();
    if (!check(wakes == 1 && !observer.active(), "detach did not cancel pending wake"))
        return 17;

    if (!check(observer.attach(&source, {QStringLiteral("objectName")},
                               [&] { ++wakes; }),
               "same source could not be reattached"))
        return 18;
    source.setObjectName(QStringLiteral("E"));
    drain();
    if (!check(wakes == 2, "fresh semantic source mutation was lost after reattach"))
        return 19;

    // Destroy one registered source during a pending coalesced event. There
    // must be no use-after-free and no wake after the source disappears.
    auto temporary = std::unique_ptr<QObject>(new QObject);
    if (!check(observer.attach(temporary.get(), {QStringLiteral("objectName")},
                               [&] { ++wakes; }),
               "temporary source registration failed"))
        return 20;
    temporary->setObjectName(QStringLiteral("destroying"));
    temporary.reset();
    drain();
    if (!check(wakes == 2 && !observer.active(), "destroyed source fired stale wake"))
        return 21;

    // Wrong/invalid replacement must leave the old source disconnected.
    if (!check(observer.attach(&source, {QStringLiteral("objectName")},
                               [&] { ++wakes; }),
               "valid source unavailable for replacement test"))
        return 22;
    if (!check(!observer.attach(&source, {QStringLiteral("notAProperty")},
                                [&] { ++wakes; }),
               "invalid replacement accepted"))
        return 23;
    source.setObjectName(QStringLiteral("F"));
    drain();
    if (!check(wakes == 2 && !observer.active(), "invalid replacement retained old source"))
        return 24;

    if (!check(observer.attach(&source, {QStringLiteral("objectName")}, [&] {
                   ++wakes;
                   observer.detach(); // reentrant lifecycle
               }),
               "reentrant callback setup failed"))
        return 25;
    source.setObjectName(QStringLiteral("G"));
    drain();
    if (!check(wakes == 3 && !observer.active(), "self-detach callback failed"))
        return 26;
    source.setObjectName(QStringLiteral("H"));
    drain();
    if (!check(wakes == 3, "wake after stopped source registration"))
        return 27;

    // Integration-scoped semantics: two surface-owned observers must never
    // steal or cross-deliver another surface's notification. This is the
    // minimum foundation for Automatic composite's multi-Surface identity;
    // it does NOT automatically discover arbitrary QML scene changes.
    QObject surfaceA;
    QObject surfaceB;
    SemanticSourcePropertyObserver observerA;
    SemanticSourcePropertyObserver observerB;
    int surfaceAWakes = 0;
    int surfaceBWakes = 0;
    if (!check(observerA.attach(&surfaceA, {QStringLiteral("objectName")},
                                [&] { ++surfaceAWakes; })
               && observerB.attach(&surfaceB, {QStringLiteral("objectName")},
                                   [&] { ++surfaceBWakes; }),
               "two independent surface subscriptions failed"))
        return 28;
    surfaceA.setObjectName(QStringLiteral("A-content"));
    drain();
    if (!check(surfaceAWakes == 1 && surfaceBWakes == 0,
               "surface-A event improperly woke surface B"))
        return 29;
    surfaceB.setObjectName(QStringLiteral("B-content"));
    drain();
    if (!check(surfaceAWakes == 1 && surfaceBWakes == 1,
               "surface-B event improperly woke surface A"))
        return 30;
    observerA.detach();
    surfaceA.setObjectName(QStringLiteral("A-removed"));
    surfaceB.setObjectName(QStringLiteral("B-updated"));
    drain();
    if (!check(surfaceAWakes == 1 && surfaceBWakes == 2,
               "removing one surface disrupted or reused its notifications"))
        return 31;
    observerB.detach();

    std::cout
        << "PASS public QtCore source-property NOTIFY contract:"
        << " coalescing, no phantom source changes, missing property,"
        << " duplicate refusal, detach, source destruction, reattach,"
        << " reentrant teardown, no stale callbacks, multi-surface isolation\n";
    std::cout << "QT_VERSION=" << QT_VERSION_STR << '\n';
    return 0;
}
