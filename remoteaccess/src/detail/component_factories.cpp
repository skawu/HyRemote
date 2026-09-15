#include "detail/component_factories.hpp"

#ifdef HYREMOTE_HAS_WIDGETS_ADAPTER
#include "widgets/widget_target.hpp"
#endif
#ifdef HYREMOTE_HAS_QUICK_ADAPTER
#include "quick/quick_target.hpp"
#endif
#ifdef HYREMOTE_HAS_RFB_TRANSPORT
#include "transport/rfb_transport.hpp"
#endif

#include <mutex>
#include <utility>

namespace HyRemote::detail {
namespace {

std::mutex &factoryMutex()
{
    static std::mutex mutex;
    return mutex;
}

TargetFactory &targetFactory()
{
    static TargetFactory factory;
    return factory;
}

TransportFactory &transportFactory()
{
    static TransportFactory factory;
    return factory;
}

}  // namespace

TargetComponents createTargetComponents(QObject *target, bool remoteInputEnabled)
{
    TargetFactory factory;
    {
        std::lock_guard<std::mutex> lock(factoryMutex());
        factory = targetFactory();
    }

    // setTargetFactory() is an internal deterministic override used by tests and future custom
    // composition. Normal product builds fall through to built-in target adapters so ordinary
    // applications never register CaptureSource/InputSink objects themselves.
    if (factory)
        return factory(target, remoteInputEnabled);

#ifdef HYREMOTE_HAS_WIDGETS_ADAPTER
    TargetComponents widgets = createWidgetsTargetComponents(target, remoteInputEnabled);
    if (widgets.supported)
        return widgets;
#endif

#ifdef HYREMOTE_HAS_QUICK_ADAPTER
    TargetComponents quick = createQuickTargetComponents(target, remoteInputEnabled);
    if (quick.supported)
        return quick;
#endif

    TargetComponents result;
    result.error = QStringLiteral(
        "No HyRemote target adapter in this build supports the attached Qt object. Enable/link a "
        "supported Widgets or Quick adapter for the target type.");
    return result;
}

TransportComponent createDefaultTransport(const QHostAddress &listenAddress, quint16 port)
{
    TransportFactory factory;
    {
        std::lock_guard<std::mutex> lock(factoryMutex());
        factory = transportFactory();
    }

    // Tests/custom compositions retain an explicit override seam, but normal product code never
    // registers/selects a protocol backend itself.
    if (factory)
        return factory(listenAddress, port);

#ifdef HYREMOTE_HAS_RFB_TRANSPORT
    TransportComponent result;
    result.transport = createRfbTransport(listenAddress, port);
    if (!result.transport)
        result.error = QStringLiteral("failed to construct the built-in HyRemote RFB transport");
    return result;
#else
    TransportComponent result;
    result.error = QStringLiteral(
        "No HyRemote transport backend is linked for this build. Enable HYREMOTE_WITH_VNC or "
        "provide an internal product transport implementation.");
    return result;
#endif
}

void setTargetFactory(TargetFactory factory)
{
    std::lock_guard<std::mutex> lock(factoryMutex());
    targetFactory() = std::move(factory);
}

void setTransportFactory(TransportFactory factory)
{
    std::lock_guard<std::mutex> lock(factoryMutex());
    transportFactory() = std::move(factory);
}

void resetFactories()
{
    std::lock_guard<std::mutex> lock(factoryMutex());
    targetFactory() = {};
    transportFactory() = {};
}

}  // namespace HyRemote::detail
