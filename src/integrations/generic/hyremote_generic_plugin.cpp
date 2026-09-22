#include "automatic/automatic_access_controller.hpp"
#include "automatic/automatic_access_config.hpp"
#include "generic_config.hpp"

#include <QDebug>
#include <QGenericPlugin>
#include <QHostAddress>
#include <QStringList>

#include <memory>
#include <utility>

namespace HyRemote::Generic {
namespace {

using Runtime::Automatic::AccessConfig;
using Runtime::Automatic::AccessController;
using Runtime::SecurityProfile;

class GenericRuntime final : public QObject
{
public:
    explicit GenericRuntime(AccessConfig config)
        : m_controller(std::make_unique<AccessController>(std::move(config)))
    {
    }

    bool start() { return m_controller && m_controller->start(); }

private:
    std::unique_ptr<AccessController> m_controller;
};

}  // namespace

class HyRemoteGenericPlugin final : public QGenericPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QGenericPluginFactoryInterface_iid FILE "hyremote.json")

public:
    QObject *create(const QString &key, const QString &specification) override
    {
        if (key.compare(QStringLiteral("hyremote"), Qt::CaseInsensitive) != 0)
            return nullptr;

        AccessConfig config;
        QString error;
        if (!parseSpecification(specification, config, error)) {
            qWarning() << "HyRemote Generic Plugin rejected its specification:" << error;
            return nullptr;
        }

        auto runtime = std::make_unique<GenericRuntime>(std::move(config));
        if (!runtime->start()) {
            qWarning() << "HyRemote Generic Plugin could not arm automatic access";
            return nullptr;
        }
        return runtime.release();
    }
};

}  // namespace HyRemote::Generic

#include "hyremote_generic_plugin.moc"
