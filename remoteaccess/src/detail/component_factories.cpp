#include "detail/component_factories.hpp"
#include "detail/target_component_provider.hpp"

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

TargetComponents createBuiltinTargetComponents(QObject *target, bool remoteInputEnabled)
{
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

}  // namespace

TargetComponents createTargetComponents(QObject *target, bool remoteInputEnabled)
{
    TargetFactory factory;
    {
        std::lock_guard<std::mutex> lock(factoryMutex());
        factory = targetFactory();
    }

    // The explicit global override remains a deterministic test seam and therefore has highest
    // precedence. Production composition must not register itself globally.
    if (factory)
        return factory(target, remoteInputEnabled);

    // Product-internal composite targets may provide components per QObject instance. The resolver
    // intentionally bypasses providers and reaches only the normal built-in Widgets/Quick chain,
    // which lets a composite reuse the same adapters without recursion and without changing any
    // other RemoteAccess instance in the process.
    if (target) {
        if (auto *provider = dynamic_cast<TargetComponentProvider *>(target)) {
            const BuiltinTargetResolver resolver = [](QObject *child, bool childRemoteInputEnabled) {
                return createBuiltinTargetComponents(child, childRemoteInputEnabled);
            };
            return provider->createTargetComponents(remoteInputEnabled, resolver);
        }
    }

    return createBuiltinTargetComponents(target, remoteInputEnabled);
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
