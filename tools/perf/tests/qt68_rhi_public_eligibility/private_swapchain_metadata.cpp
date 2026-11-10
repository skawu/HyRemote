// Diagnostic-only exact Qt6.8.3 GuiPrivate SWAPCHAIN METADATA audit.
// No mutation, GPU readback, build of product Runtime, or performance claim.
// A borrowed pointer is consumed only inside the Qt scenegraph callback.
// Qt RHI headers are private; never promote this file to production.
#include <QGuiApplication>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QThread>
#include <QTimer>
#include <QtGlobal>
#include <rhi/qrhi.h>
#include <atomic>
#include <iostream>
#include <cstdint>

struct Metadata {
    std::atomic<int> renderCount{0};
    std::atomic<int> transferSource{-1};
    std::atomic<int> flags{-1};
    std::atomic<int> samples{-1};
    std::atomic<int> width{-1};
    std::atomic<int> height{-1};
    std::atomic<int> maxAsyncFrames{-1};
    std::atomic<int> deviceLost{-1};
    std::atomic<int> rhiBackend{-1};
    std::atomic<int> renderThreadSeparate{0};
    std::atomic<int> inconsistent{0};
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
#ifdef Q_OS_WIN
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#endif
    QQuickWindow window;
    window.resize(640, 360);
    window.setColor(QColor(24, 101, 220));

    Metadata m;
    QObject::connect(&window, &QQuickWindow::afterRendering,
                     &window, [&] {
        m.renderCount.fetch_add(1);
        if (QThread::currentThread() != app.thread())
            m.renderThreadSeparate.store(1);
        auto *ri = window.rendererInterface();
        if (!ri)
            return;
        void *resource = ri->getResource(
            &window, QSGRendererInterface::RhiSwapchainResource);
        if (!resource)
            return;
        // This is a borrowed pointer owned by Qt. No retain and no mutation.
        const auto *swapchain = static_cast<const QRhiSwapChain *>(resource);
        const int flags = int(swapchain->flags());
        const int transfer =
            swapchain->flags().testFlag(QRhiSwapChain::UsedAsTransferSource) ? 1 : 0;
        if (m.transferSource.load() >= 0
            && m.transferSource.load() != transfer)
            m.inconsistent.store(1);
        m.transferSource.store(transfer);
        m.flags.store(flags);
        m.samples.store(swapchain->sampleCount());
        const QSize size = swapchain->currentPixelSize();
        m.width.store(size.width());
        m.height.store(size.height());
        if (auto *rhi = window.rhi()) {
            m.maxAsyncFrames.store(
                rhi->resourceLimit(QRhi::MaxAsyncReadbackFrames));
            m.deviceLost.store(rhi->isDeviceLost() ? 1 : 0);
            m.rhiBackend.store(int(rhi->backend()));
        }
    }, Qt::DirectConnection);

    auto report = [&] {
        const bool valid = m.renderCount.load() > 0
            && m.transferSource.load() >= 0
            && m.width.load() > 0 && m.height.load() > 0
            && m.rhiBackend.load() >= 0
            && m.deviceLost.load() == 0
            && m.inconsistent.load() == 0;
        std::cout << "R2_PRIVATE_METADATA_JSON={"
                  << "\"qt\":\"" << QT_VERSION_STR << "\","
                  << "\"graphics_api\":"
                  << int(window.rendererInterface()->graphicsApi()) << ","
                  << "\"rhi_backend_enum\":" << m.rhiBackend.load() << ","
                  << "\"render_callbacks\":" << m.renderCount.load() << ","
                  << "\"render_thread_separate\":"
                  << (m.renderThreadSeparate.load() ? "true" : "false") << ","
                  << "\"borrowed_swapchain_observed\":"
                  << (m.transferSource.load() >= 0 ? "true" : "false") << ","
                  << "\"used_as_transfer_source\":"
                  << (m.transferSource.load() == 1 ? "true" : "false") << ","
                  << "\"swapchain_flags\":" << m.flags.load() << ","
                  << "\"sample_count\":" << m.samples.load() << ","
                  << "\"pixel_size\":[" << m.width.load()
                  << "," << m.height.load() << "],"
                  << "\"max_async_readback_frames\":"
                  << m.maxAsyncFrames.load() << ","
                  << "\"device_lost\":"
                  << (m.deviceLost.load() == 1 ? "true" : "false") << ","
                  << "\"metadata_inconsistent\":"
                  << (m.inconsistent.load() ? "true" : "false") << ","
                  << "\"no_gpu_readback_performed\":true,"
                  << "\"no_swapchain_mutation\":true,"
                  << "\"does_not_grant_product_qualification\":true"
                  << "}" << std::endl;
        std::cout << "R2_PRIVATE_METADATA_VERDICT="
                  << (valid ? "PASS_READ_ONLY_METADATA"
                            : "BLOCKED_RESOURCE_NOT_VERIFIED")
                  << std::endl;
        app.exit(valid ? 0 : 21);
    };

    int ticks = 0;
    QTimer redraw;
    redraw.setInterval(30);
    QObject::connect(&redraw, &QTimer::timeout, &app, [&] {
        if (++ticks <= 12)
            window.update();
        else
            report();
    });
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &app, report);
    window.show();
    redraw.start();
    timeout.start(10000);
    return app.exec();
}
