#pragma once

// Qt5.15+ compatible internal capture-route policy; graphics APIs are data,
// never a hardware/OS whitelist or permission to access private Qt resources.
// The default public route is identical across Qt LTS versions. The provider
// is selected only when its exact Kit/build was explicitly qualified and armed.
// A Qt6-specific provider must not be mistaken for the product architecture.
#include <QtQuick/QSGRendererInterface>
#include <QtGlobal>

#include <cstddef>

namespace HyRemote::detail {

enum class QuickCaptureBackend : std::size_t {
    Unknown, Software, OpenGL, Direct3D11, Direct3D12, Vulkan, Metal, Other
};

enum class QuickCaptureRoute {
    PublicQtGrabToImage,
    ExperimentalOpenGlPbo
};

struct QuickCaptureSelection {
    QuickCaptureBackend backend = QuickCaptureBackend::Unknown;
    QuickCaptureRoute route = QuickCaptureRoute::PublicQtGrabToImage;
};

inline QuickCaptureSelection selectQuickCaptureRoute(
    QSGRendererInterface::GraphicsApi api,
    bool qualifiedOpenGlProviderEnabled = false) noexcept
{
    QuickCaptureBackend backend = QuickCaptureBackend::Other;
    switch (api) {
    case QSGRendererInterface::Unknown:
        backend = QuickCaptureBackend::Unknown;
        break;
    case QSGRendererInterface::Software:
        backend = QuickCaptureBackend::Software;
        break;
    case QSGRendererInterface::OpenGL:
        backend = QuickCaptureBackend::OpenGL;
        break;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt 5.15 has both the default direct-GL scenegraph and opt-in early
    // RHI graphics APIs. Never refer to Qt6 enum names in a Qt5 build.
    case QSGRendererInterface::Direct3D12:
        backend = QuickCaptureBackend::Direct3D12;
        break;
    case QSGRendererInterface::OpenGLRhi:
        backend = QuickCaptureBackend::OpenGL;
        break;
    case QSGRendererInterface::Direct3D11Rhi:
        backend = QuickCaptureBackend::Direct3D11;
        break;
    case QSGRendererInterface::VulkanRhi:
        backend = QuickCaptureBackend::Vulkan;
        break;
    case QSGRendererInterface::MetalRhi:
        backend = QuickCaptureBackend::Metal;
        break;
#else
    // Qt 6 RHI reuses these enum names, but not Qt5's enum identities.
    case QSGRendererInterface::Direct3D11:
        backend = QuickCaptureBackend::Direct3D11;
        break;
#  if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    case QSGRendererInterface::Direct3D12:
        backend = QuickCaptureBackend::Direct3D12;
        break;
#  endif
    case QSGRendererInterface::Vulkan:
        backend = QuickCaptureBackend::Vulkan;
        break;
    case QSGRendererInterface::Metal:
        backend = QuickCaptureBackend::Metal;
        break;
#endif
    default:
        break;
    }
    // The caller owns the version/Kit capability gate. The route selector
    // never consults a chipset/OS name and never grants private QRhi access.
    // Current OpenGL PBO provider is research-only, enabled solely by the
    // exact Qt6.8.3 experimental CMake switch. Qt5.15 remains public until
    // its own render-phase/GL context capability is qualified.
    if (backend == QuickCaptureBackend::OpenGL && qualifiedOpenGlProviderEnabled)
        return {backend, QuickCaptureRoute::ExperimentalOpenGlPbo};
    return {backend, QuickCaptureRoute::PublicQtGrabToImage};
}

} // namespace HyRemote::detail
