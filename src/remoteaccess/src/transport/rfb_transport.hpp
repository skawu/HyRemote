#pragma once

#include <QHostAddress>

#include <memory>

#include "hyremote/core/transport.hpp"

namespace HyRemote::detail {

// Creates the product's bounded, dependency-light RFB correctness transport. The concrete type is
// deliberately private to RemoteAccess: applications and hyremote-core only see hyremote::Transport.
std::unique_ptr<hyremote::Transport> createRfbTransport(const QHostAddress &listenAddress,
                                                        quint16 port);

}  // namespace HyRemote::detail
