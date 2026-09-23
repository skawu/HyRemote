#pragma once

// #174: the Generic Plugin's launch syntax lives on its own, the way the Transparent QPA frontend's does, so the
// mapping it performs can be tested directly instead of only through a plugin Qt has already loaded. It maps this
// frontend's own vocabulary onto the one frontend-neutral runtime configuration and decides nothing else: it never
// resolves an interface, never chooses an address and never widens a bind.

#include "automatic/automatic_access_config.hpp"

#include <QString>

namespace HyRemote::Generic {

// Parses "key=value;key=value". An empty specification is the runtime's own default, which is the IPv4 wildcard.
// `address` and `interface` describe the same decision, so supplying both is refused rather than resolved.
bool parseSpecification(const QString &specification, Runtime::Automatic::AccessConfig &config, QString &error);

}  // namespace HyRemote::Generic
