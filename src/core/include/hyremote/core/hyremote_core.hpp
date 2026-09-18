// SPDX-License-Identifier: Apache-2.0
#pragma once

// Umbrella header for the HyRemote Core contract.
//
// Semantic source of truth:
//   docs/adr/0001-core-boundaries.md
//   docs/adr/0002-remoteframe-lifetime-timestamps.md
//   docs/adr/0003-threading-backpressure.md
//   docs/core-architecture.md
//
// The design-level proposal in docs/proposals/hyremote_core.hpp was the input to this
// implementation; names and signatures may be refined here as long as those invariants hold.
//
// Dependency rule: nothing in this header (or anywhere in hyremote-core) may require Qt
// Widgets/Quick/QML, Qt private/QPA, NeatVNC/AML, DRM/GBM/DMA-BUF or RKMPP/V4L2/VA-API types.

#include "hyremote/core/capabilities.hpp"
#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/session.hpp"
#include "hyremote/core/storage.hpp"
#include "hyremote/core/transport.hpp"
#include "hyremote/core/types.hpp"
