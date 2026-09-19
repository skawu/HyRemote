#pragma once

#include <QHostAddress>
#include <QString>

#include <functional>
#include <memory>

#include <HyRemote/RemoteAccessExport.h>

#include "detail/transport_security.hpp"
#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/transport.hpp"

class QObject;

namespace HyRemote::detail {

struct TargetComponents
{
    bool supported = false;
    std::unique_ptr<hyremote::CaptureSource> capture;
    std::shared_ptr<hyremote::InputSink> input;
    QString error;
};

struct TransportComponent
{
    std::unique_ptr<hyremote::Transport> transport;
    QString error;
};

using TargetFactory = std::function<TargetComponents(QObject *target, bool remoteInputEnabled)>;
using TransportFactory =
    std::function<TransportComponent(const QHostAddress &listenAddress, quint16 port)>;

// Internal composition seam. #6/#28/#29/#27 provide the production factories; the public facade
// never exposes them. Tests replace them deterministically to verify product lifecycle semantics
// without opening a real network listener. The declarations live only in the source-private tree;
// exporting the symbols is required for Windows tests against the shared runtime and does not add an
// installed/public SDK header or application-facing API.
HYREMOTE_REMOTEACCESS_EXPORT TargetComponents createTargetComponents(QObject *target,
                                                                      bool remoteInputEnabled);
HYREMOTE_REMOTEACCESS_EXPORT TransportComponent createDefaultTransport(
    const QHostAddress &listenAddress,
    quint16 port);
HYREMOTE_REMOTEACCESS_EXPORT TransportComponent createDefaultTransport(
    const QHostAddress &listenAddress,
    quint16 port,
    TransportSecurityConfiguration security);

HYREMOTE_REMOTEACCESS_EXPORT void setTargetFactory(TargetFactory factory);
HYREMOTE_REMOTEACCESS_EXPORT void setTransportFactory(TransportFactory factory);
HYREMOTE_REMOTEACCESS_EXPORT void resetFactories();

}  // namespace HyRemote::detail
