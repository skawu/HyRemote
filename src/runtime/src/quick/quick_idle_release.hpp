#pragma once

// #790: one deferred-release decision shared by every Quick capture backend that
// keeps a pixel/GPU allocation between requests.
//
// The Shared Runtime owns the single zero-viewer edge and delivers it through
// CaptureSource::releaseIdleResources() on the capture scheduler thread. A
// backend may still be servicing an already accepted request at that moment, so
// it latches the edge and consumes it at its next genuinely idle point instead
// of freeing a buffer that a fence, a map or a queued request still uses.
//
// The latch is not a timer and not a second demand signal: it carries no
// platform, Qt-version or graphics-API decision, so GL/GLES/D3D11/public Qt all
// share the same semantics.
namespace HyRemote::detail {

class QuickIdleRelease final
{
public:
    // Records one zero-viewer edge. Returns false for a repeated edge while a
    // release is still pending, so duplicate events cannot queue two frees.
    bool latch() noexcept
    {
        if (m_pending)
            return false;
        m_pending = true;
        return true;
    }

    // The caller decided the backend is idle. Consumes the edge exactly once;
    // false means "nothing latched, keep the allocation".
    bool take() noexcept
    {
        if (!m_pending)
            return false;
        m_pending = false;
        return true;
    }

    bool pending() const noexcept { return m_pending; }

    // start()/stop() are hard lifecycle boundaries: the source itself is being
    // (re)created or torn down, so a latched edge must not leak into the next
    // run.
    void clear() noexcept { m_pending = false; }

private:
    bool m_pending = false;
};

} // namespace HyRemote::detail
