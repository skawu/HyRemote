#include "automatic/surface_semantic_source_registry.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QStringList>
#include <QtGlobal>
#include <iostream>
#include <memory>
#include <vector>

namespace {

void drain()
{
    QEventLoop loop;
    QTimer::singleShot(20, &loop, &QEventLoop::quit);
    loop.exec();
}

bool check(bool ok, const char *message)
{
    if (!ok)
        std::cerr << "FAIL source-scoped surface registry: " << message << '\n';
    return ok;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using HyRemote::Runtime::Automatic::SurfaceSemanticSourceRegistry;

    std::vector<quint64> delivered;
    SurfaceSemanticSourceRegistry registry([&](quint64 id) { delivered.push_back(id); });

    QObject surfaceA;
    QObject surfaceB;
    QObject sourceA;
    QObject sourceB;

    if (!check(!registry.upsertSurface(0, &surfaceA, true)
               && !registry.upsertSurface(15, nullptr, true),
               "accepted invalid surface registration"))
        return 10;

    if (!check(registry.upsertSurface(11, &surfaceA, true)
               && registry.upsertSurface(22, &surfaceB, true)
               && registry.surfaceCount() == 2,
               "surface membership/identity not created"))
        return 11;

    if (!check(!registry.watchSource(11, &surfaceB, &sourceA,
                                    {QStringLiteral("objectName")})
               && !registry.isWatching(11),
               "wrong surface identity accepted"))
        return 12;

    if (!check(!registry.watchSource(11, &surfaceA, &sourceA,
                                    {QStringLiteral("noSuchProperty")})
               && !registry.isWatching(11),
               "undeclared source NOTIFY property accepted"))
        return 13;

    if (!check(registry.watchSource(11, &surfaceA, &sourceA,
                                   {QStringLiteral("objectName")})
               && registry.watchSource(22, &surfaceB, &sourceB,
                                       {QStringLiteral("objectName")}),
               "two valid surface-owned notification sources rejected"))
        return 14;

    sourceA.setObjectName(QStringLiteral("A-1"));
    sourceA.setObjectName(QStringLiteral("A-2"));
    drain();
    if (!check(delivered.size() == 1 && delivered[0] == 11,
               "A source failed coalescing or crossed into B"))
        return 15;

    sourceB.setObjectName(QStringLiteral("B-1"));
    drain();
    if (!check(delivered.size() == 2 && delivered[1] == 22,
               "B event woke unrelated surface"))
        return 16;

    // Hiding an existing real Automatic surface must revoke the fast path.
    if (!check(registry.upsertSurface(11, &surfaceA, false)
               && !registry.isWatching(11),
               "hidden surface retained live source watcher"))
        return 17;
    sourceA.setObjectName(QStringLiteral("A-hidden"));
    drain();
    if (!check(delivered.size() == 2, "hidden source generated fast wake"))
        return 18;

    // Reveal does not automatically re-enable a stale subscription. Only the
    // source-owner may explicitly re-attach a verified source and properties.
    if (!check(registry.upsertSurface(11, &surfaceA, true)
               && !registry.isWatching(11),
               "reappearing surface incorrectly reused old watcher"))
        return 19;
    sourceA.setObjectName(QStringLiteral("A-revealed-without-registration"));
    drain();
    if (!check(delivered.size() == 2, "reappearing source self-registered"))
        return 20;
    if (!check(registry.watchSource(11, &surfaceA, &sourceA,
                                   {QStringLiteral("objectName")}),
               "explicit reattach on visible source failed"))
        return 21;

    // Pending source wake is revoked when real surface membership changes.
    sourceA.setObjectName(QStringLiteral("A-pending-old"));
    registry.removeSurface(11);
    drain();
    if (!check(delivered.size() == 2 && !registry.isWatching(11)
               && registry.surfaceCount() == 1,
               "removed surface dispatched stale callback"))
        return 22;

    QObject replacementA;
    QObject replacementSource;
    if (!check(registry.upsertSurface(11, &replacementA, true),
               "replacement surface was not registered"))
        return 23;
    if (!check(!registry.watchSource(11, &surfaceA, &sourceA,
                                    {QStringLiteral("objectName")}),
               "reused surface ID accidentally accepted old surface"))
        return 24;
    if (!check(registry.watchSource(11, &replacementA, &replacementSource,
                                   {QStringLiteral("objectName")}),
               "replacement surface/source registration failed"))
        return 25;
    sourceA.setObjectName(QStringLiteral("A-stale-generation"));
    replacementSource.setObjectName(QStringLiteral("A-new-generation"));
    drain();
    if (!check(delivered.size() == 3 && delivered[2] == 11,
               "stale callback from previous generation leaked"))
        return 26;

    auto temporarySurface = std::make_unique<QObject>();
    auto temporarySource = std::make_unique<QObject>();
    if (!check(registry.upsertSurface(33, temporarySurface.get(), true)
               && registry.watchSource(33, temporarySurface.get(),
                                       temporarySource.get(),
                                       {QStringLiteral("objectName")}),
               "temporary target registration failed"))
        return 27;
    temporarySource->setObjectName(QStringLiteral("pending-teardown"));
    temporarySurface.reset();
    drain();
    if (!check(delivered.size() == 3 && registry.surfaceCount() == 2,
               "surface destroyed after pending notify triggered stale wake"))
        return 28;
    temporarySource.reset();

    auto ephemeralSource = std::make_unique<QObject>();
    if (!check(registry.watchSource(22, &surfaceB, ephemeralSource.get(),
                                   {QStringLiteral("objectName")}),
               "replacement source registration on surface B failed"))
        return 29;
    ephemeralSource->setObjectName(QStringLiteral("pending-source-delete"));
    ephemeralSource.reset();
    drain();
    if (!check(delivered.size() == 3 && !registry.isWatching(22),
               "destroyed source generated stale wake"))
        return 30;

    if (!check(!registry.watchSource(22, &surfaceB, &sourceB,
                                    {QStringLiteral("objectName"),
                                     QStringLiteral("objectName")}),
               "duplicate property was not refused"))
        return 31;

    if (!check(registry.watchSource(22, &surfaceB, &sourceB,
                                   {QStringLiteral("objectName")}),
               "valid registration after invalid property failure failed"))
        return 32;
    sourceB.setObjectName(QStringLiteral("B-after-recovery"));
    drain();
    if (!check(delivered.size() == 4 && delivered[3] == 22,
               "source event lost after re-register"))
        return 33;

    registry.clear();
    sourceB.setObjectName(QStringLiteral("stopped"));
    replacementSource.setObjectName(QStringLiteral("stopped"));
    drain();
    if (!check(registry.surfaceCount() == 0 && delivered.size() == 4,
               "stop/clear failed to revoke all pending notifications"))
        return 34;

    std::cout << "PASS Qt surface-scoped source NOTIFY: multi-Surface identity, "
                 "source coalescing, hide/show, re-register, generation replacement, "
                 "surface/source destruction, invalid-property fail-close, stop\n";
    std::cout << "QT_VERSION=" << QT_VERSION_STR << '\n';
    return 0;
}
