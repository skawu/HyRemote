#pragma once

#include "access_types.hpp"
#include "detail/integration_route.hpp"

#include <QHostAddress>
#include <QString>

#include <optional>

namespace HyRemote::Runtime::Automatic {

// Frontend-neutral configuration for application-level zero-code access. Generic Plugin and QPA
// parse their own launch syntax into this common runtime shape; no frontend-specific vocabulary is
// retained after bootstrap except the private diagnostic route token used by the shared report.
struct AccessConfig
{
    IntegrationRoute integrationRoute = IntegrationRoute::Unknown;

    // #174: exactly one of address/interface selects the binding. An empty interface means the address decides,
    // which is why the wildcard default still means "all IPv4 interfaces" with no interface configured.
    QString listenInterface;
    QHostAddress listenAddress = QHostAddress::AnyIPv4;
    quint16 port = static_cast<quint16>(HYREMOTE_DEFAULT_PORT);
    bool remoteInputEnabled = false;
    Runtime::SecurityProfile securityProfile = Runtime::SecurityProfile::Insecure;
    QString securityConfigFile;

    // #589: the zero-code routes can only offer what their launch syntax can express, so the watermark is described
    // here in route-neutral terms. An empty position means "shared default"; the optional numbers stay optional so the
    // one shared policy owns every default and every bound.
    //
    // #761: "an empty text means disabled" conflated three different launches, and a launch that only changed a
    // position therefore cleared a stored policy. The two intents are now separate: text plus attributes selects a
    // watermark for this run, `watermarkOff` is the explicit request to clear a stored one, and saying nothing keeps
    // whatever a previous launch stored.
    QString watermarkText;
    QString watermarkPosition;
    std::optional<double> watermarkOpacity;
    std::optional<int> watermarkFontPointSize;
    std::optional<QString> watermarkColor;
    bool watermarkOff = false;
};

}  // namespace HyRemote::Runtime::Automatic
