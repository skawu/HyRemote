#pragma once
// #967 D0/D1: ONE backend-neutral capture capability contract.
//
// The architecture decision (2026-11-13) requires that every Qt backend be
// routed by the SAME "can this surface be captured natively?" decision, and
// that the backend itself only performs legal GPU copies / async readbacks.
// Before this header the decision existed twice and disagreed in shape:
//
//   * the GL/PBO provider rejected a redirected render target, MSAA, an
//     unsupported context and a lost target inline, each with its own
//     ad-hoc `onInvalidated()`/`return` ordering and one shared message;
//   * the D3D11 staging provider had its own three-way verdict
//     (Admitted/Transient/Unsupported) with distinct reasons.
//
// Neither could be reviewed against the other, and a new backend (GLES/EGLFS,
// Vulkan, Metal) would have had to invent a third table. This header freezes
// the single table: facts in, verdict + reason out. Backend-specific wording
// stays with the backend, so an operator still sees "multisampled D3D11 render
// target" rather than a generic string.
//
// The Qt-version-neutral public baseline (grabToImage) is always Admitted: it
// is the safe fallback and must never depend on these facts.
#include <cstddef>
#include <cstdint>
#include <string>

namespace HyRemote::detail {

enum class QuickCaptureCapabilityBackend {
    PublicQtGrab,
    OpenGlReadback,
    D3D11Staging
};

// Transient: not capturable *this* frame; a later Qt frame may be.
// Unsupported: cannot become capturable without a Qt scene-graph / render
//              target rebuild. Waiting (or the 750 ms deadline) cannot help.
enum class QuickCaptureVerdict {
    Admitted,
    Transient,
    Unsupported
};

enum class QuickCaptureReason {
    None,
    ApiNotReported,
    ApiMismatch,
    ContextNotReady,
    ContextUnsupported,
    TargetNotReady,
    RedirectedTarget,
    Multisampled,
    Non2dTarget,
    UnsupportedFormat,
    GeometryMismatch
};

// Facts are what a render-pass callback can observe. They are deliberately
// plain data so a GPU-free regression can exercise every row on every Kit.
struct QuickCaptureFacts {
    QuickCaptureCapabilityBackend backend = QuickCaptureCapabilityBackend::PublicQtGrab;
    bool apiReported = true;    // rendererInterface()->graphicsApi() is known
    bool apiMatches = true;     // ... and it is this backend's API
    bool contextReady = true;   // context/device/worker is usable now
    bool contextSupported = true;  // ... and it can do this backend's readback
    bool targetKnown = true;    // render target / RTV / texture resolvable
    // Persistent properties of the surface.
    bool redirectedTarget = false;  // Qt renders into its own FBO / render target
    bool multisampled = false;
    bool non2dTarget = false;       // arrayed or mipmapped surface
    bool formatSupported = true;
    // Geometry is re-validated at delivery as well; a resize can expose the
    // previous description for one frame.
    bool geometryMatches = true;
};

struct QuickCaptureAdmission {
    QuickCaptureVerdict verdict = QuickCaptureVerdict::Transient;
    QuickCaptureReason reason = QuickCaptureReason::ContextNotReady;
};

// The single decision table. Order matters: identity and readiness are decided
// before persistent surface properties, and geometry last, so a resize race can
// never be reported as a permanent capability failure.
inline QuickCaptureAdmission
evaluateQuickCaptureCapability(const QuickCaptureFacts &facts) noexcept
{
    if (facts.backend == QuickCaptureCapabilityBackend::PublicQtGrab)
        return {QuickCaptureVerdict::Admitted, QuickCaptureReason::None};
    if (!facts.apiReported)
        return {QuickCaptureVerdict::Transient, QuickCaptureReason::ApiNotReported};
    // Waiting for another Qt frame cannot change the RHI identity.
    if (!facts.apiMatches)
        return {QuickCaptureVerdict::Unsupported, QuickCaptureReason::ApiMismatch};
    // Qt-owned objects can be absent during adoption/teardown.
    if (!facts.contextReady)
        return {QuickCaptureVerdict::Transient, QuickCaptureReason::ContextNotReady};
    // The context exists but lacks this backend's readback capabilities
    // (PBO + fence for OpenGL). Another Qt frame cannot add them; only a new
    // context generation can, so this is a capability verdict, not a wait.
    if (!facts.contextSupported)
        return {QuickCaptureVerdict::Unsupported, QuickCaptureReason::ContextUnsupported};
    if (!facts.targetKnown)
        return {QuickCaptureVerdict::Transient, QuickCaptureReason::TargetNotReady};
    // Persistent properties: only a scene-graph rebuild can change them.
    if (facts.redirectedTarget)
        return {QuickCaptureVerdict::Unsupported, QuickCaptureReason::RedirectedTarget};
    if (facts.multisampled)
        return {QuickCaptureVerdict::Unsupported, QuickCaptureReason::Multisampled};
    if (facts.non2dTarget)
        return {QuickCaptureVerdict::Unsupported, QuickCaptureReason::Non2dTarget};
    if (!facts.formatSupported)
        return {QuickCaptureVerdict::Unsupported, QuickCaptureReason::UnsupportedFormat};
    if (!facts.geometryMatches)
        return {QuickCaptureVerdict::Transient, QuickCaptureReason::GeometryMismatch};
    return {QuickCaptureVerdict::Admitted, QuickCaptureReason::None};
}

// A readable token for diagnostics/tests. Never pixels, never driver text.
inline const char *quickCaptureReasonToken(QuickCaptureReason reason) noexcept
{
    switch (reason) {
    case QuickCaptureReason::None: return "none";
    case QuickCaptureReason::ApiNotReported: return "api_not_reported";
    case QuickCaptureReason::ApiMismatch: return "api_mismatch";
    case QuickCaptureReason::ContextNotReady: return "context_not_ready";
    case QuickCaptureReason::ContextUnsupported: return "context_unsupported";
    case QuickCaptureReason::TargetNotReady: return "target_not_ready";
    case QuickCaptureReason::RedirectedTarget: return "redirected_target";
    case QuickCaptureReason::Multisampled: return "multisampled";
    case QuickCaptureReason::Non2dTarget: return "non2d_target";
    case QuickCaptureReason::UnsupportedFormat: return "unsupported_format";
    case QuickCaptureReason::GeometryMismatch: return "geometry_mismatch";
    }
    return "unknown";
}

// #967 D0/D1 GPU-free regression. One row per verdict/reason so a new backend
// can be added as data instead of as a third hand-written state machine.
struct QuickCaptureCapabilityRegression {
    bool ran = false;
    bool ok = false;
    bool publicAlwaysAdmitted = false;
    bool missingApiIsTransient = false;
    bool apiMismatchIsUnsupported = false;
    bool missingContextIsTransient = false;
    bool unsupportedContextIsUnsupported = false;
    bool missingTargetIsTransient = false;
    bool redirectedTargetIsUnsupported = false;
    bool multisampleIsUnsupported = false;
    bool non2dIsUnsupported = false;
    bool formatIsUnsupported = false;
    bool geometryMismatchIsTransient = false;
    bool reasonsAreDistinct = false;
    bool admittedRowIsClean = false;
    std::string detail;
};

inline QuickCaptureCapabilityRegression runQuickCaptureCapabilityRegression()
{
    QuickCaptureCapabilityRegression out;
    out.ran = true;

    QuickCaptureFacts base;
    base.backend = QuickCaptureCapabilityBackend::OpenGlReadback;

    const auto row = [](QuickCaptureFacts facts) {
        return evaluateQuickCaptureCapability(facts);
    };

    // The public Qt baseline is the unconditional safe fallback: no fact may
    // ever downgrade it.
    QuickCaptureFacts publicFacts;
    publicFacts.backend = QuickCaptureCapabilityBackend::PublicQtGrab;
    publicFacts.apiMatches = false;
    publicFacts.multisampled = true;
    publicFacts.geometryMatches = false;
    out.publicAlwaysAdmitted =
        row(publicFacts).verdict == QuickCaptureVerdict::Admitted;

    auto f = base;
    f.apiReported = false;
    out.missingApiIsTransient = row(f).verdict == QuickCaptureVerdict::Transient
        && row(f).reason == QuickCaptureReason::ApiNotReported;

    f = base;
    f.apiMatches = false;
    out.apiMismatchIsUnsupported = row(f).verdict == QuickCaptureVerdict::Unsupported
        && row(f).reason == QuickCaptureReason::ApiMismatch;

    f = base;
    f.contextReady = false;
    out.missingContextIsTransient = row(f).verdict == QuickCaptureVerdict::Transient
        && row(f).reason == QuickCaptureReason::ContextNotReady;

    // GLES2 / a context without PBO+fence: permanent for this context
    // generation, so it must be a capability verdict rather than a wait.
    f = base;
    f.contextSupported = false;
    out.unsupportedContextIsUnsupported =
        row(f).verdict == QuickCaptureVerdict::Unsupported
        && row(f).reason == QuickCaptureReason::ContextUnsupported;

    f = base;
    f.targetKnown = false;
    out.missingTargetIsTransient = row(f).verdict == QuickCaptureVerdict::Transient
        && row(f).reason == QuickCaptureReason::TargetNotReady;

    f = base;
    f.redirectedTarget = true;
    out.redirectedTargetIsUnsupported =
        row(f).verdict == QuickCaptureVerdict::Unsupported
        && row(f).reason == QuickCaptureReason::RedirectedTarget;

    f = base;
    f.multisampled = true;
    out.multisampleIsUnsupported = row(f).verdict == QuickCaptureVerdict::Unsupported
        && row(f).reason == QuickCaptureReason::Multisampled;

    f = base;
    f.non2dTarget = true;
    out.non2dIsUnsupported = row(f).verdict == QuickCaptureVerdict::Unsupported
        && row(f).reason == QuickCaptureReason::Non2dTarget;

    f = base;
    f.formatSupported = false;
    out.formatIsUnsupported = row(f).verdict == QuickCaptureVerdict::Unsupported
        && row(f).reason == QuickCaptureReason::UnsupportedFormat;

    // A resize/DPR race must never look like a capability failure.
    f = base;
    f.geometryMatches = false;
    out.geometryMismatchIsTransient =
        row(f).verdict == QuickCaptureVerdict::Transient
        && row(f).reason == QuickCaptureReason::GeometryMismatch;

    const char *tokens[] = {
        quickCaptureReasonToken(QuickCaptureReason::ApiNotReported),
        quickCaptureReasonToken(QuickCaptureReason::ApiMismatch),
        quickCaptureReasonToken(QuickCaptureReason::ContextNotReady),
        quickCaptureReasonToken(QuickCaptureReason::ContextUnsupported),
        quickCaptureReasonToken(QuickCaptureReason::TargetNotReady),
        quickCaptureReasonToken(QuickCaptureReason::RedirectedTarget),
        quickCaptureReasonToken(QuickCaptureReason::Multisampled),
        quickCaptureReasonToken(QuickCaptureReason::Non2dTarget),
        quickCaptureReasonToken(QuickCaptureReason::UnsupportedFormat),
        quickCaptureReasonToken(QuickCaptureReason::GeometryMismatch)};
    out.reasonsAreDistinct = true;
    for (std::size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); ++i)
        for (std::size_t j = i + 1; j < sizeof(tokens) / sizeof(tokens[0]); ++j)
            if (std::string(tokens[i]) == tokens[j])
                out.reasonsAreDistinct = false;

    const auto admitted = row(base);
    out.admittedRowIsClean = admitted.verdict == QuickCaptureVerdict::Admitted
        && admitted.reason == QuickCaptureReason::None;

    out.ok = out.publicAlwaysAdmitted && out.missingApiIsTransient
        && out.apiMismatchIsUnsupported && out.missingContextIsTransient
        && out.unsupportedContextIsUnsupported
        && out.missingTargetIsTransient && out.redirectedTargetIsUnsupported
        && out.multisampleIsUnsupported && out.non2dIsUnsupported
        && out.formatIsUnsupported && out.geometryMismatchIsTransient
        && out.reasonsAreDistinct && out.admittedRowIsClean;
    if (!out.ok)
        out.detail = "backend-neutral-capability-table-regressed";
    return out;
}

} // namespace HyRemote::detail
