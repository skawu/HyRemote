#pragma once

// Qt5.15+ compatible internal capture-route policy; graphics APIs are data,
// never a hardware/OS whitelist or permission to access private Qt resources.
// The default public route is identical across Qt LTS versions. The provider
// is selected only when its exact Kit/build was explicitly qualified and armed.
// A Qt6-specific provider must not be mistaken for the product architecture.
// All supported scenegraph APIs are served by Qt's public
// QQuickItem::grabToImage contract; a graphics API name never grants
// permission to use an unqualified private QRhi/GL readback experiment.
#include <QtQuick/QSGRendererInterface>

#include <cstddef>

namespace HyRemote::detail {

enum class QuickCaptureBackend : std::size_t {
    Unknown, Software, OpenGL, Direct3D11, Direct3D12, Vulkan, Metal, Other
};

// Graphics API alone is not enough. The Qt5 direct-GL scenegraph and its
// optional RHI backend share a GPU API but have different render lifecycles.
// Qt6 uses a different default RHI architecture. Keep these paths distinct
// so an accelerated provider is never admitted by an API-name match alone.
enum class QuickRenderingPath {
    Unknown, Software, Qt5Direct, Qt5EarlyRhi, Qt6Rhi
};

enum class QuickCaptureRoute {
    PublicQtGrabToImage,
    ExperimentalOpenGlPbo
};

struct QuickCaptureSelection {
    QuickCaptureBackend backend = QuickCaptureBackend::Unknown;
    QuickRenderingPath renderingPath = QuickRenderingPath::Unknown;
    QuickCaptureRoute route = QuickCaptureRoute::PublicQtGrabToImage;
};

inline QuickCaptureSelection selectQuickCaptureRoute(
    QSGRendererInterface::GraphicsApi api,
    bool qualifiedOpenGlProviderEnabled = false) noexcept
{
    QuickCaptureBackend backend = QuickCaptureBackend::Other;
    QuickRenderingPath renderingPath = QuickRenderingPath::Unknown;
    switch (api) {
    case QSGRendererInterface::Unknown:
        backend = QuickCaptureBackend::Unknown;
        break;
    case QSGRendererInterface::Software:
        backend = QuickCaptureBackend::Software;
        renderingPath = QuickRenderingPath::Software;
        break;
    case QSGRendererInterface::OpenGL:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    case QSGRendererInterface::OpenGLRhi:
#endif
        backend = QuickCaptureBackend::OpenGL;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        renderingPath = QuickRenderingPath::Qt5Direct;
#else
        renderingPath = QuickRenderingPath::Qt6Rhi;
#endif
        break;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt 5.15 has both the default direct-GL scenegraph and opt-in early
    // RHI graphics APIs. Never refer to Qt6 enum names in a Qt5 build.
    case QSGRendererInterface::Direct3D12Rhi:
        backend = QuickCaptureBackend::Direct3D12;
        renderingPath = QuickRenderingPath::Qt5EarlyRhi;
        break;
    case QSGRendererInterface::OpenGLRhi:
        backend = QuickCaptureBackend::OpenGL;
        renderingPath = QuickRenderingPath::Qt5EarlyRhi;
        break;
    case QSGRendererInterface::Direct3D11Rhi:
        backend = QuickCaptureBackend::Direct3D11;
        renderingPath = QuickRenderingPath::Qt5EarlyRhi;
        break;
    case QSGRendererInterface::VulkanRhi:
        backend = QuickCaptureBackend::Vulkan;
        renderingPath = QuickRenderingPath::Qt5EarlyRhi;
        break;
    case QSGRendererInterface::MetalRhi:
        backend = QuickCaptureBackend::Metal;
        renderingPath = QuickRenderingPath::Qt5EarlyRhi;
        break;
#else
    case QSGRendererInterface::Direct3D11:
#endif
        backend = QuickCaptureBackend::Direct3D11;
        renderingPath = QuickRenderingPath::Qt6Rhi;
        break;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5 Direct3D12Rhi is handled above as an early-RHI path.
#else
    case QSGRendererInterface::Direct3D12:
        backend = QuickCaptureBackend::Direct3D12;
        renderingPath = QuickRenderingPath::Qt6Rhi;
        break;
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    case QSGRendererInterface::VulkanRhi:
#else
    case QSGRendererInterface::Vulkan:
#endif
        backend = QuickCaptureBackend::Vulkan;
        renderingPath = QuickRenderingPath::Qt6Rhi;
        break;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    case QSGRendererInterface::MetalRhi:
#else
    case QSGRendererInterface::Metal:
#endif
        backend = QuickCaptureBackend::Metal;
        renderingPath = QuickRenderingPath::Qt6Rhi;
        break;
    default:
        break;
    }
    // The caller owns the version/Kit capability gate. The route selector
    // never consults a chipset/OS name and never grants private QRhi access.
    // Current OpenGL PBO provider is research-only, enabled solely by the
    // exact Qt6.8.3 experimental CMake switch. Qt5.15 remains public until
    // its own render-phase/GL context capability is qualified.
    if (backend == QuickCaptureBackend::OpenGL
        && renderingPath == QuickRenderingPath::Qt6Rhi
        && qualifiedOpenGlProviderEnabled)
        return {backend, renderingPath, QuickCaptureRoute::ExperimentalOpenGlPbo};
    return {backend, renderingPath, QuickCaptureRoute::PublicQtGrabToImage};
}

} // namespace HyRemote::detail
