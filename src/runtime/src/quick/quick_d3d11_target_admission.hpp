#pragma once
// #967 A2: ONE admission rule for the experimental Qt6 D3D11 staging provider.
//
// The Qt render pass is the only place that can see the real render target, so
// it is also the only place that can tell three very different states apart:
//
//   Admitted    - this frame is a valid capture candidate;
//   Transient   - this frame cannot be captured (Qt is mid-initialization, or a
//                 resize/DPR change exposed the previous geometry); a later Qt
//                 frame may legitimately become Admitted, so the provider must
//                 NOT report a capability failure and must NOT wake the Viewer;
//   Unsupported - no amount of waiting can help; the render target is
//                 multisampled, not a single-level 2D surface, in a format the
//                 staging copy cannot express, or the RHI is not Direct3D11.
//
// Before this header the provider collapsed all three into a bare `return`, so
// an Unsupported surface retired its accepted requests only through the 750 ms
// request deadline and never said why. The Qt-version/cmake-neutral rule lives
// here so a GPU-free regression can pin it on every Kit.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace HyRemote::detail {

enum class QuickD3d11SurfaceVerdict {
    Admitted,
    Transient,
    Unsupported
};

// Facts are exactly what onRenderPass() can observe. The device identity is
// validated separately in onAfterRender(), which is where it is needed.
struct QuickD3d11SurfaceFacts {
    bool direct3d11Api = false;
    bool hasDeviceContext = false;
    bool hasRenderTarget = false;
    bool hasTexture = false;
    std::uint32_t textureWidth = 0;
    std::uint32_t textureHeight = 0;
    std::uint32_t arraySize = 0;
    std::uint32_t mipLevels = 0;
    std::uint32_t sampleCount = 0;
    bool formatIsBgraOrRgba = false;
    int expectedWidth = 0;  // qRound(window->width() * effective DPR)
    int expectedHeight = 0;
};

struct QuickD3d11SurfaceAdmission {
    QuickD3d11SurfaceVerdict verdict = QuickD3d11SurfaceVerdict::Transient;
    // Stable literal for the CaptureEvent message. Never pixels, never
    // driver text; bounded so it can be logged once per episode.
    const char *reason = nullptr;
};

inline QuickD3d11SurfaceAdmission
evaluateQuickD3d11Surface(const QuickD3d11SurfaceFacts &facts) noexcept
{
    // Faithful extraction of the single boolean the provider used inline:
    // either the render pass is capturable, or the request keeps waiting for
    // the 750 ms deadline. No verdict and no reason exist yet.
    const bool capturable = facts.direct3d11Api
        && facts.hasDeviceContext && facts.hasRenderTarget && facts.hasTexture
        && facts.sampleCount == 1 && facts.arraySize == 1 && facts.mipLevels == 1
        && facts.formatIsBgraOrRgba && facts.expectedWidth > 0
        && facts.expectedHeight > 0
        && facts.textureWidth == static_cast<std::uint32_t>(facts.expectedWidth)
        && facts.textureHeight == static_cast<std::uint32_t>(facts.expectedHeight);
    if (capturable)
        return {QuickD3d11SurfaceVerdict::Admitted, nullptr};
    return {QuickD3d11SurfaceVerdict::Transient,
            "D3D11 render target is not capturable this frame"};
}

// Moves every identity the provider has already accepted into `revoked`, so one
// capability episode terminates each accepted request exactly once and never
// re-terminates one it already retired. Returns the number of revoked
// identities; a second call always returns 0 because the acceptance queues are
// then empty.
template <typename Request>
std::size_t collectUnsupportedRequests(std::vector<Request> &queued,
                                       std::vector<Request> &submitted,
                                       std::vector<Request> &converting,
                                       std::vector<Request> &revoked) noexcept
{
    const std::size_t before = revoked.size();
    revoked.insert(revoked.end(), queued.begin(), queued.end());
    revoked.insert(revoked.end(), submitted.begin(), submitted.end());
    revoked.insert(revoked.end(), converting.begin(), converting.end());
    queued.clear();
    submitted.clear();
    converting.clear();
    return revoked.size() - before;
}

// #967 A2 GPU-free regression result. The provider needs a real D3D11 device to
// capture, but the admission rule and its "terminate once" contract do not.
struct QuickD3d11AdmissionRegression {
    bool ran = false;
    bool ok = false;
    bool exactSurfaceAdmitted = false;
    bool multisampleIsUnsupported = false;
    bool unsupportedFormatIsUnsupported = false;
    bool non2dTargetIsUnsupported = false;
    bool nonD3d11ApiIsUnsupported = false;
    bool resizeIsTransient = false;
    bool missingResourcesAreTransient = false;
    bool reasonsAreDistinct = false;
    bool revokesEachAcceptedIdOnce = false;
    bool revokeIsIdempotent = false;
    std::string detail;
};

namespace detail_d3d11_admission {

struct TestRequest {
    unsigned id = 0;
};

inline unsigned countIds(const std::vector<TestRequest> &requests)
{
    unsigned seen = 0;
    for (const auto &request : requests)
        seen = seen * 31u + request.id;
    return seen;
}

} // namespace detail_d3d11_admission

inline QuickD3d11AdmissionRegression runQuickD3d11AdmissionRegression()
{
    using namespace detail_d3d11_admission;
    QuickD3d11AdmissionRegression out;
    out.ran = true;

    QuickD3d11SurfaceFacts exact;
    exact.direct3d11Api = true;
    exact.hasDeviceContext = true;
    exact.hasRenderTarget = true;
    exact.hasTexture = true;
    exact.textureWidth = 1920;
    exact.textureHeight = 1080;
    exact.arraySize = 1;
    exact.mipLevels = 1;
    exact.sampleCount = 1;
    exact.formatIsBgraOrRgba = true;
    exact.expectedWidth = 1920;
    exact.expectedHeight = 1080;
    const auto admitted = evaluateQuickD3d11Surface(exact);
    out.exactSurfaceAdmitted = admitted.verdict == QuickD3d11SurfaceVerdict::Admitted
        && admitted.reason == nullptr;

    auto multisample = exact;
    multisample.sampleCount = 4;
    const auto msaa = evaluateQuickD3d11Surface(multisample);
    out.multisampleIsUnsupported =
        msaa.verdict == QuickD3d11SurfaceVerdict::Unsupported && msaa.reason;

    auto format = exact;
    format.formatIsBgraOrRgba = false;
    const auto badFormat = evaluateQuickD3d11Surface(format);
    out.unsupportedFormatIsUnsupported =
        badFormat.verdict == QuickD3d11SurfaceVerdict::Unsupported
        && badFormat.reason;

    auto non2d = exact;
    non2d.arraySize = 2;
    const auto arrayed = evaluateQuickD3d11Surface(non2d);
    auto mips = exact;
    mips.mipLevels = 0;
    const auto noMips = evaluateQuickD3d11Surface(mips);
    out.non2dTargetIsUnsupported =
        arrayed.verdict == QuickD3d11SurfaceVerdict::Unsupported
        && noMips.verdict == QuickD3d11SurfaceVerdict::Unsupported;

    auto notD3d11 = exact;
    notD3d11.direct3d11Api = false;
    const auto wrongApi = evaluateQuickD3d11Surface(notD3d11);
    out.nonD3d11ApiIsUnsupported =
        wrongApi.verdict == QuickD3d11SurfaceVerdict::Unsupported
        && wrongApi.reason;

    // A resize/DPR change must NEVER be reported as a capability failure: the
    // next Qt frame describes the new geometry and becomes Admitted again.
    auto resized = exact;
    resized.textureWidth = 1280;
    resized.textureHeight = 720;
    const auto resize = evaluateQuickD3d11Surface(resized);
    out.resizeIsTransient = resize.verdict == QuickD3d11SurfaceVerdict::Transient
        && resize.reason
        && evaluateQuickD3d11Surface(exact).verdict
               == QuickD3d11SurfaceVerdict::Admitted;

    auto noTarget = exact;
    noTarget.hasRenderTarget = false;
    auto noDevice = exact;
    noDevice.hasDeviceContext = false;
    out.missingResourcesAreTransient =
        evaluateQuickD3d11Surface(noTarget).verdict
            == QuickD3d11SurfaceVerdict::Transient
        && evaluateQuickD3d11Surface(noDevice).verdict
            == QuickD3d11SurfaceVerdict::Transient;

    // Four different permanent mismatches must not share one message: the
    // CaptureEvent is the only diagnostic a Viewer-side trace will ever show.
    out.reasonsAreDistinct = msaa.reason && badFormat.reason
        && arrayed.reason && wrongApi.reason
        && std::string(msaa.reason) != badFormat.reason
        && std::string(badFormat.reason) != arrayed.reason
        && std::string(arrayed.reason) != wrongApi.reason;

    // One episode, one terminal per accepted identity: queued + submitted +
    // converting are all retired, in order, and a second episode retires
    // nothing (no duplicate completion, no RenderCallback re-entry).
    std::vector<TestRequest> queued{{701}, {702}};
    std::vector<TestRequest> submitted{{703}};
    std::vector<TestRequest> converting{{704}};
    std::vector<TestRequest> revoked;
    const std::size_t firstEpisode =
        collectUnsupportedRequests(queued, submitted, converting, revoked);
    const bool retiredAll = firstEpisode == 4 && revoked.size() == 4
        && revoked[0].id == 701 && revoked[1].id == 702
        && revoked[2].id == 703 && revoked[3].id == 704
        && queued.empty() && submitted.empty() && converting.empty();
    const std::size_t secondEpisode =
        collectUnsupportedRequests(queued, submitted, converting, revoked);
    out.revokesEachAcceptedIdOnce = retiredAll;
    out.revokeIsIdempotent = secondEpisode == 0 && revoked.size() == 4
        && countIds(revoked) == countIds(std::vector<TestRequest>{{701}, {702}, {703}, {704}});

    out.ok = out.exactSurfaceAdmitted && out.multisampleIsUnsupported
        && out.unsupportedFormatIsUnsupported && out.non2dTargetIsUnsupported
        && out.nonD3d11ApiIsUnsupported && out.resizeIsTransient
        && out.missingResourcesAreTransient && out.reasonsAreDistinct
        && out.revokesEachAcceptedIdOnce && out.revokeIsIdempotent;
    if (!out.ok) {
        out.detail = !out.multisampleIsUnsupported
            || !out.unsupportedFormatIsUnsupported || !out.non2dTargetIsUnsupported
            || !out.nonD3d11ApiIsUnsupported
            ? "persistent-capability-mismatch-is-not-a-verdict"
            : !out.revokesEachAcceptedIdOnce || !out.revokeIsIdempotent
            ? "accepted-identity-not-retired-exactly-once"
            : "admission-rule-regressed";
    }
    return out;
}

} // namespace HyRemote::detail
