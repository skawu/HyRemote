#include "../hyremote_qpa_interception.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QWindow>

#include <cstdio>
#include <memory>
#include <vector>

using HyRemote::Qpa::Internal::InterceptionEvent;
using HyRemote::Qpa::Internal::InterceptionEventType;
using HyRemote::Qpa::Internal::InterceptionObject;
using HyRemote::Qpa::Internal::InterceptionSeam;

namespace {

bool require(bool condition, const char *message)
{
    if (!condition)
        std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

int countEvents(const std::vector<InterceptionEvent> &events,
                quint64 token,
                InterceptionObject object,
                InterceptionEventType type)
{
    int count = 0;
    for (const auto &event : events) {
        if (event.windowToken == token && event.object == object && event.type == type)
            ++count;
    }
    return count;
}

}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    std::vector<InterceptionEvent> events;
    QWindow survivingWindow;
    survivingWindow.setGeometry(10, 20, 320, 180);

    quint64 survivingToken = 0;
    {
        InterceptionSeam seam([&events](const InterceptionEvent &event) { events.push_back(event); });
        seam.observePlatformWindowCreated(&survivingWindow);
        seam.observeBackingStoreCreated(&survivingWindow);

        if (!require(seam.trackedWindowCount() == 1, "one QWindow must map to one neutral token"))
            return 2;
        if (!require(events.size() >= 2, "platform-window and backing-store creation must be observable"))
            return 3;

        survivingToken = events.front().windowToken;
        if (!require(survivingToken != 0, "window token must be non-zero"))
            return 4;
        if (!require(events[1].windowToken == survivingToken,
                     "platform window and backing store must share the same window token"))
            return 5;

        // Multiple delegate-created backing stores for one public QWindow must each be observable;
        // QPA-02 deliberately does not retain them or synthesize a destruction lifetime.
        seam.observeBackingStoreCreated(&survivingWindow);
        if (!require(countEvents(events,
                                 survivingToken,
                                 InterceptionObject::BackingStore,
                                 InterceptionEventType::Created) == 2,
                     "each backing-store creation call must produce a fresh neutral observation"))
            return 6;

        survivingWindow.setGeometry(30, 40, 400, 220);
        QCoreApplication::processEvents();

        QPlatformSurfaceEvent aboutToDestroy(QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed);
        QCoreApplication::sendEvent(&survivingWindow, &aboutToDestroy);

        if (!require(countEvents(events,
                                 survivingToken,
                                 InterceptionObject::PlatformWindow,
                                 InterceptionEventType::SurfaceAboutToBeDestroyed) == 1,
                     "platform-window destruction boundary must be observed exactly once"))
            return 7;

        // A recreated native surface keeps the public QWindow identity/token but emits a new
        // creation observation. No native pointer is retained across the destruction boundary.
        seam.observePlatformWindowCreated(&survivingWindow);
        if (!require(countEvents(events,
                                 survivingToken,
                                 InterceptionObject::PlatformWindow,
                                 InterceptionEventType::Created) == 2,
                     "surface recreation must be observable without changing the QWindow token"))
            return 8;

        auto transientWindow = std::make_unique<QWindow>();
        seam.observePlatformWindowCreated(transientWindow.get());
        const quint64 transientToken = events.back().windowToken;
        transientWindow.reset();

        if (!require(countEvents(events,
                                 transientToken,
                                 InterceptionObject::PlatformWindow,
                                 InterceptionEventType::WindowDestroyed) == 1,
                     "QWindow destruction must close the neutral observation lifetime"))
            return 9;
    }

    const std::size_t afterSeamDestruction = events.size();
    survivingWindow.setGeometry(50, 60, 420, 240);
    QCoreApplication::processEvents();
    QPlatformSurfaceEvent lateEvent(QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed);
    QCoreApplication::sendEvent(&survivingWindow, &lateEvent);

    if (!require(events.size() == afterSeamDestruction,
                 "callbacks must not outlive the interception seam/proxy integration"))
        return 10;

    std::printf("PASS: neutral QPA interception seam preserves native ownership and bounded callback lifetime\n");
    return 0;
}
