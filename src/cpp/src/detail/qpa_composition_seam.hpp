#pragma once

// Source-private composition seam between the Transparent QPA payload and the shared RemoteAccess runtime.
//
// The QPA platform MODULE must not link Core directly - the release-readiness repository-layout gate enforces
// "QPA platform payload must not link Core directly", because Core stays static composition behind the
// HyRemote::RemoteAccess boundary. The composite capture/input code needs two Core-owned operations anyway,
// so the runtime exposes them here rather than letting the payload reach into Core:
//
//   - creation of a writable CPU RGBA frame storage, and writable access to its first plane;
//   - normalized pointer mapping, as used by composite input routing.
//
// This header is reachable only through the source-private include directory of the runtime
// (src/embedded/src). It is not installed as a public SDK header, it is not an application-facing API and
// it must not be documented as one; the symbols are private runtime exports so that a DLL build can be
// consumed by the QPA payload on Windows.

#include <HyRemote/RemoteAccessExport.h>

#include <cstddef>
#include <memory>
#include <optional>

#include "hyremote/core/frame.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/storage.hpp"

namespace HyRemote::detail {

/// A writable CPU frame storage together with a direct pointer to its first plane.
struct WritableCpuFrame
{
    std::shared_ptr<hyremote::FrameStorage> storage;
    std::byte *data = nullptr;
    std::size_t bytesPerLine = 0;
};

/// Creates a single-plane writable CPU RGBA storage of the requested geometry.
HYREMOTE_REMOTEACCESS_EXPORT WritableCpuFrame createWritableCpuFrame(std::size_t bytesPerLine,
                                                                    std::size_t height);

/// Returns the writable CPU view of a frame produced by the capture path, or an empty view when that frame
/// does not carry a writable CPU storage.
HYREMOTE_REMOTEACCESS_EXPORT WritableCpuFrame writableCpuFrameOf(const hyremote::RemoteFrame &frame);

/// Maps a pointer input event into the target coordinate space exactly as the runtime's own input path does.
/// Returns std::nullopt for events that carry no pointer position.
HYREMOTE_REMOTEACCESS_EXPORT std::optional<hyremote::MappedInputPoint>
mapPointerToNormalizedTarget(const hyremote::InputEvent &event, float targetWidth, float targetHeight);

} // namespace HyRemote::detail
