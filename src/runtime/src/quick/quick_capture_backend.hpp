#pragma once

// Qt Quick capture-route policy, not a hardware/OS whitelist. All Qt 6
// scenegraph APIs are served by Qt's public QQuickItem::grabToImage contract.
// A graphics API name is diagnostic context only: it never grants permission
// to use an unqualified private QRhi readback experiment.
#include <QtQuick/QSGRendererInterface>

#include <cstddef>

namespace HyRemote::detail {

enum class QuickCaptureBackend : std::size_t {
    Unknown, Software, OpenGL, Direct3D11, Direct3D12, Vulkan, Metal, Other
};

enum class QuickCaptureRoute {
    PublicQtGrabToImage
};

struct QuickCaptureSelection {
    QuickCaptureBackend backend = QuickCaptureBackend::Unknown;
    QuickCaptureRoute route = QuickCaptureRoute::PublicQtGrabToImage;
};

inline QuickCaptureSelection selectQuickCaptureRoute(
    QSGRendererInterface::GraphicsApi api) noexcept
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
    case QSGRendererInterface::Direct3D11:
        backend = QuickCaptureBackend::Direct3D11;
        break;
    case QSGRendererInterface::Direct3D12:
        backend = QuickCaptureBackend::Direct3D12;
        break;
    case QSGRendererInterface::Vulkan:
        backend = QuickCaptureBackend::Vulkan;
        break;
    case QSGRendererInterface::Metal:
        backend = QuickCaptureBackend::Metal;
        break;
    default:
        break;
    }
    return {backend, QuickCaptureRoute::PublicQtGrabToImage};
}

} // namespace HyRemote::detail
