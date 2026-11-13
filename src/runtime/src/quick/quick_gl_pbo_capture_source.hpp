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
HYREMOTE_QUICK_GL_TEST_EXPORT bool runQuickGlPboBatchOwnershipRegression();

// #967 fallback-policy regression seam. Drives the same arming state machine
// the GL source uses, without a GPU window: no inherited 17ms wait, at most
// one forced update per render generation, the arm is released by the real
// RenderCallbackGuard after a frame, and the QTimer releases it fail-closed
// when no frame is requested (empty queue, pending GPU work, invalid or
// hidden window).
struct QuickGlPboFallbackPolicyRegression {
    bool ran = false;
    bool ok = false;
    bool oneArmPerRender = false;
    bool armReleasedByRender = false;
    // #967 P95 tail: a timer that produces no Qt frame must release its own
    // arm, while a real forced update keeps it until the render callback.
    bool noUpdateReleasesArm = false;
    bool updateKeepsArm = false;
    bool negativeControlBlocksNextArm = false;
    // #967 review: window invalid / hidden between arming and the timer must
    // be fail-closed, and a usable window must allow the next request to arm.
    bool invalidWindowReleasesArm = false;
    bool hiddenWindowReleasesArm = false;
    bool windowRestoredRearms = false;
    int glFallbackDelayMs = -1;
    int d3dFallbackDelayMs = -1;
    std::string detail;
};

HYREMOTE_QUICK_GL_TEST_EXPORT QuickGlPboFallbackPolicyRegression
runQuickGlPboFallbackPolicyRegression();

// #967 review: geometry admission for the GL/PBO readback. GPU-free: it pins
// the ONE rule the render callback and the session retire worker share, so a
// frame the submit path accepts can never be one the worker refuses to map.
// The measured #962/#987 cells were all 1920x1080; a high-DPI window is a
// larger, equally legitimate geometry and must stay deliverable.
struct QuickGlPboMapPolicyRegression {
    bool ran = false;
    bool ok = false;
    bool baselineFits = false;
    bool highDpiWindowMaps = false;
    bool denseHighDpiMaps = false;
    bool emptyGeometryRejected = false;
    std::size_t baselineBytes = 0;
    std::size_t highDpiBytes = 0;
    std::size_t denseHighDpiBytes = 0;
    std::string detail;
};

HYREMOTE_QUICK_GL_TEST_EXPORT QuickGlPboMapPolicyRegression
runQuickGlPboMapPolicyRegression();

struct QuickGlPboResourceCounts {
    unsigned buffersCreated;
    unsigned buffersDeleted;
    unsigned workersCreated;
    unsigned workersRetired;
};
HYREMOTE_QUICK_GL_TEST_EXPORT QuickGlPboResourceCounts quickGlPboResourceCounts();

// #790: real offscreen GL proof of the zero-viewer idle release. The worker
// deletes the one full-frame PBO in the context that created it, the source
// state stops referencing it, and the next request allocates a usable buffer
// again. It runs in the same dedicated GL lane as the #962 session self-test.
struct QuickGlPboIdleReleaseProbe {
    bool ran = false;
    bool ok = false;
    bool hookInvoked = false;
    bool bufferReleased = false;
    bool bufferDeleted = false;
    bool bufferRecreated = false;
    std::string detail;
};

HYREMOTE_QUICK_GL_TEST_EXPORT QuickGlPboIdleReleaseProbe
runQuickGlPboIdleReleaseProbe();

// P1 (#962 review): inject a 5s retire delay, then requestStop+wait. Proves
// stop() is bounded by the worker's abort check, not by the 750ms fence
// ceiling or an unbounded join.
struct QuickGlPboStopProbe {
    bool ran = false;
    bool ok = false;
    bool timedOut = false;
    bool bufferDeleted = false;
    double joinMs = 0;
    std::string detail;
};

HYREMOTE_QUICK_GL_TEST_EXPORT QuickGlPboStopProbe
runQuickGlPboStopAbortProbe(int delayMs);

#undef HYREMOTE_QUICK_GL_TEST_EXPORT

} // namespace HyRemote::detail
