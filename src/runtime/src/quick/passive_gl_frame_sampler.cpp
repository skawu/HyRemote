#include "quick/passive_gl_frame_sampler.hpp"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSGRendererInterface>
#include <QtQuick/QQuickWindow>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "hyremote/core/storage.hpp"

namespace HyRemote::detail {

// Proof of feasibility, NOT a general Qt 6 RHI readback implementation.
// The window's native OpenGL default framebuffer must be current. Offscreen
// grabs / QuickRenderControl / non-OpenGL backends fail closed to the original
// grabToImage capture route; do not change the application's graphics API.
std::optional<hyremote::RemoteFrame> readPassiveQuickGlFrame(
    QQuickWindow *window, const QSize &expectedPixelSize, float devicePixelRatio)
{
    if (!window || expectedPixelSize.width() <= 0 || expectedPixelSize.height() <= 0
        || expectedPixelSize.width() > 8192 || expectedPixelSize.height() > 8192
        || devicePixelRatio <= 0.0F
        || !window->rendererInterface()
        || window->rendererInterface()->graphicsApi() != QSGRendererInterface::OpenGL)
        return std::nullopt;

    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context || !context->functions())
        return std::nullopt;

    QOpenGLFunctions *gl = context->functions();
    GLint framebuffer = -1;
    GLint viewport[4] = {0, 0, 0, 0};
    gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    gl->glGetIntegerv(GL_VIEWPORT, viewport);

    // In particular, never treat an offscreen item grab's temporary FBO as
    // the native screen frame. Also reject unverified scaled/subwindow targets.
    if (framebuffer != static_cast<GLint>(context->defaultFramebufferObject())
        || viewport[0] != 0 || viewport[1] != 0
        || viewport[2] != expectedPixelSize.width()
        || viewport[3] != expectedPixelSize.height())
        return std::nullopt;

    const std::size_t width = static_cast<std::size_t>(viewport[2]);
    const std::size_t height = static_cast<std::size_t>(viewport[3]);
    const std::size_t stride = width * 4U;
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(stride, height);
    if (!storage || !storage->mutablePlane(0))
        return std::nullopt;

    // DirectConnection on QQuickWindow::afterRendering: renderer thread with
    // a current GL context. This does NOT request another scenegraph frame.
    // Qt 6 RHI submission/swap semantics vary by backend: pixels must be
    // compared to actual native/viewer output before enabling production.
    window->beginExternalCommands();
    GLint previousPackAlignment = 4;
    gl->glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    gl->glPixelStorei(GL_PACK_ALIGNMENT, 4);
    gl->glReadPixels(0, 0, viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE,
                     storage->mutablePlane(0));
    const GLenum error = gl->glGetError();
    gl->glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    window->endExternalCommands();
    if (error != GL_NO_ERROR)
        return std::nullopt;

    // OpenGL framebuffer coordinates begin at bottom left, RFB at top left.
    std::byte *bytes = storage->mutablePlane(0);
    std::vector<std::byte> row(stride);
    for (std::size_t y = 0; y < height / 2U; ++y) {
        std::byte *top = bytes + y * stride;
        std::byte *bottom = bytes + (height - 1U - y) * stride;
        std::memcpy(row.data(), top, stride);
        std::memcpy(top, bottom, stride);
        std::memcpy(bottom, row.data(), stride);
    }

    hyremote::RemoteFrame frame;
    frame.geometry.size = {static_cast<std::uint32_t>(width),
                           static_cast<std::uint32_t>(height)};
    frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
    frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
    frame.geometry.planeCount = 1;
    frame.geometry.devicePixelRatio = devicePixelRatio;
    frame.storage = std::move(storage);
    frame.timing.ptsSource = hyremote::PtsSource::Completion;
    frame.timing.completionTime = hyremote::Clock::now();
    frame.damage = hyremote::Damage::fullFrame();
    return frame;
}

}  // namespace HyRemote::detail
