#pragma once

#include "hyremote/core/capture_source.hpp"
#include <HyRemote/RemoteAccessExport.h>
#include <memory>
#include <string>

class QQuickWindow;

namespace HyRemote::detail {

#if defined(HYREMOTE_ENABLE_PRIVATE_TEST_EXPORTS)
#  define HYREMOTE_QUICK_GL_TEST_EXPORT HYREMOTE_REMOTEACCESS_EXPORT
#else
#  define HYREMOTE_QUICK_GL_TEST_EXPORT
#endif

// Experimental Qt6.8 OpenGL-RHI SOURCE, not a general Qt6 GPU interface.
// Called only when a real QQuickWindow reports OpenGL as its graphics API.
// The normal public grabToImage route stays the unconditional default.
HYREMOTE_QUICK_GL_TEST_EXPORT std::unique_ptr<hyremote::CaptureSource>
createExperimentalQuickGlPboCaptureSource(QQuickWindow *window);

// #962 review B1/B2 regression seam. Runs a real offscreen OpenGL session:
// one PBO allocation reused by a second request, then a simulated
// scene-graph invalidation that retires the worker and reinitializes a new
// generation with fresh pixels. Returns counters and the first-pixel truth
// so the CI test can prove reuse and recovery instead of trusting CI green.
struct QuickGlPboSessionSelfTest {
    bool ran = false;
    bool ok = false;
    bool pixelsOk = false;
    bool zeroViewerNoReadback = false;
    unsigned allocations = 0;
    unsigned reuses = 0;
    unsigned workersCreated = 0;
    unsigned workersRetired = 0;
    unsigned invalidations = 0;
    std::string detail;
};

HYREMOTE_QUICK_GL_TEST_EXPORT QuickGlPboSessionSelfTest
runQuickGlPboSessionSelfTest();

// Number of real sceneGraphInvalidated deliveries observed by the source.
HYREMOTE_QUICK_GL_TEST_EXPORT unsigned quickGlPboInvalidationCount();

#undef HYREMOTE_QUICK_GL_TEST_EXPORT

} // namespace HyRemote::detail
