#pragma once

#include <QHostAddress>
#include <QString>

#include <functional>
#include <memory>

#include <HyRemote/RemoteAccessExport.h>

#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/transport.hpp"

#include "transport/rfb_transport.hpp"

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
using TransportFactory = std::function<TransportComponent(const QHostAddress &listenAddress,
                                                          quint16 port,
                                                          const RfbSecurityConfig &security)>;

// Internal composition seam. #6/#28/#29/#27 provide the production factories; the public facade
// never exposes them. Source tests replace them deterministically to verify product lifecycle
// semantics without opening a real listener. They require cross-DLL visibility on Windows only in
// a test-enabled build tree. A normal/release build leaves these declarations unannotated, and the
// runtime's hidden-by-default visibility keeps them out of the installed application ABI.
#if defined(HYREMOTE_ENABLE_PRIVATE_TEST_EXPORTS)
#  define HYREMOTE_PRIVATE_TEST_EXPORT HYREMOTE_REMOTEACCESS_EXPORT
#else
#  define HYREMOTE_PRIVATE_TEST_EXPORT
#endif

HYREMOTE_PRIVATE_TEST_EXPORT TargetComponents createTargetComponents(QObject *target,
                                                                      bool remoteInputEnabled);
HYREMOTE_PRIVATE_TEST_EXPORT TransportComponent createDefaultTransport(
    const QHostAddress &listenAddress,
    quint16 port,
    const RfbSecurityConfig &security);

HYREMOTE_PRIVATE_TEST_EXPORT void setTargetFactory(TargetFactory factory);
HYREMOTE_PRIVATE_TEST_EXPORT void setTransportFactory(TransportFactory factory);
HYREMOTE_PRIVATE_TEST_EXPORT void resetFactories();

#undef HYREMOTE_PRIVATE_TEST_EXPORT

}  // namespace HyRemote::detail
