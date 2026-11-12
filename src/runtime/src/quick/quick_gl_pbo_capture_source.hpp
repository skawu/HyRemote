#pragma once

#include "hyremote/core/capture_source.hpp"
// The generated export header exists in the Runtime build tree, but the
// standalone GL-RHI fixture compiles this source directly. Only include it
// when the build actually provides it; without it the hooks are internal.
#if defined(__has_include)
#  if __has_include(<HyRemote/RemoteAccessExport.h>)
#    include <HyRemote/RemoteAccessExport.h>
#  endif
#endif
#include <memory>
#include <string>

class QQuickWindow;

namespace HyRemote::detail {

// This translation unit is built only for the experimental, default-OFF GL
// PBO provider. Its observability seam is therefore part of that TEST-ONLY
// provider, not the product ABI. Use the DLL export macro when the Runtime
// build provides it; the standalone GL-RHI fixture has no export header and
// compiles the same source with plain internal linkage.
#if defined(HYREMOTE_REMOTEACCESS_EXPORT)
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
    unsigned buffersCreated = 0;  // glGenBuffers calls
    unsigned bufferDataCalls = 0; // glBufferData calls
    unsigned reuses = 0;          // submissions that reused the PBO
    unsigned workersCreated = 0;
    unsigned workersRetired = 0;
    unsigned invalidations = 0;
    std::string detail;
};

HYREMOTE_QUICK_GL_TEST_EXPORT QuickGlPboSessionSelfTest
runQuickGlPboSessionSelfTest();

// Number of real sceneGraphInvalidated deliveries observed by the source.
HYREMOTE_QUICK_GL_TEST_EXPORT unsigned quickGlPboInvalidationCount();

// P1 (#962 review): inject a 5s retire delay, then requestStop+wait. Proves
// stop() is bounded by the worker's abort check, not by the 750ms fence
// ceiling or an unbounded join.
struct QuickGlPboStopProbe {
    bool ran = false;
    bool ok = false;
    bool timedOut = false;
    double joinMs = 0;
    std::string detail;
};

HYREMOTE_QUICK_GL_TEST_EXPORT QuickGlPboStopProbe
runQuickGlPboStopAbortProbe(int delayMs);

#undef HYREMOTE_QUICK_GL_TEST_EXPORT

} // namespace HyRemote::detail
