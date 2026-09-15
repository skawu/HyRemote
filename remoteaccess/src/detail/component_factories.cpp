#include "detail/component_factories.hpp"

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

    if (!factory) {
        TargetComponents result;
        result.error = QStringLiteral(
            "No HyRemote target adapter is linked yet for this build. Widgets and Quick adapters "
            "are provided by the V0.0.1.0 target-integration work.");
        return result;
    }

    return factory(target, remoteInputEnabled);
}

TransportComponent createDefaultTransport(const QHostAddress &listenAddress, quint16 port)
{
    TransportFactory factory;
    {
        std::lock_guard<std::mutex> lock(factoryMutex());
        factory = transportFactory();
    }

    if (!factory) {
        TransportComponent result;
        result.error = QStringLiteral(
            "No HyRemote transport backend is linked yet for this build. The product facade does "
            "not expose or require an application to select a backend manually.");
        return result;
    }

    return factory(listenAddress, port);
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
