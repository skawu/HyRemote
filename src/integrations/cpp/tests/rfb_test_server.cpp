#include <QCoreApplication>
#include <QHostAddress>
#include <QThread>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "hyremote/core/frame.hpp"
#include "hyremote/core/storage.hpp"
#include "transport/rfb_transport.hpp"

namespace {

const char *kindName(hyremote::InputEventKind kind)
{
    using hyremote::InputEventKind;
    switch (kind) {
    case InputEventKind::PointerMove:
        return "pointer-move";
    case InputEventKind::PointerButton:
        return "pointer-button";
    case InputEventKind::PointerScroll:
        return "pointer-scroll";
    case InputEventKind::Key:
        return "key";
    case InputEventKind::Text:
        return "text";
    case InputEventKind::None:
        return "none";
    }
    return "unknown";
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2) {
        std::cerr << "usage: hyremote-rfb-test-server <port>\n";
        return 64;
    }

    bool ok = false;
    const int parsedPort = QString::fromLocal8Bit(argv[1]).toInt(&ok);
    if (!ok || parsedPort <= 0 || parsedPort > 65535)
        return 64;

    // This fixture exercises the insecure profile's explicit None handshake, so it asks for no authentication.
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost,
                                                          static_cast<quint16>(parsedPort),
                                                          HyRemote::detail::RfbSecurityConfig{});
    if (!transport)
        return 65;

    const bool started = transport->start(
        [](const hyremote::InputEvent &event) {
            std::cout << "INPUT " << kindName(event.kind) << " x=" << event.x << " y=" << event.y
                      << " button=" << static_cast<int>(event.button)
                      << " pressed=" << (event.pressed ? 1 : 0)
                      << " key=" << static_cast<int>(event.key)
                      << " modifiers=" << event.modifiers
                      << " scrollX=" << event.scrollX << " scrollY=" << event.scrollY;
            if (event.kind == hyremote::InputEventKind::Text)
                std::cout << " text=" << event.textUtf8;
            std::cout << std::endl;
        },
        [](const hyremote::TransportEvent &event) {
            std::cout << "EVENT " << static_cast<int>(event.code) << " " << event.message << std::endl;
        });
    if (!started) {
        std::cout << "START_FAILED" << std::endl;
        return 2;
    }

    constexpr std::uint32_t width = 64;
    constexpr std::uint32_t height = 48;
    auto storage = hyremote::CpuFrameStorage::createSinglePlane(width * 4U, height);
    if (!storage || !storage->mutablePlane(0))
        return 66;
    std::byte *pixels = storage->mutablePlane(0);
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i) {
        pixels[i * 4U] = std::byte{0x33};
        pixels[i * 4U + 1U] = std::byte{0x66};
        pixels[i * 4U + 2U] = std::byte{0x99};
        pixels[i * 4U + 3U] = std::byte{0xff};
    }

    hyremote::RemoteFrame frame;
    frame.id = 1;
    frame.geometry.size = {width, height};
    frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
    frame.geometry.alphaMode = hyremote::AlphaMode::Opaque;
    frame.geometry.planeCount = 1;
    frame.storage = std::move(storage);
    frame.timing.pts = hyremote::Clock::now();
    frame.timing.ptsSource = hyremote::PtsSource::Completion;
    frame.damage = hyremote::Damage::fullFrame();
    transport->enqueueFrame(std::move(frame));

    std::cout << "READY " << parsedPort << std::endl;

    // The workflow terminates the harness after its maintained-client round trip. Keeping the
    // process alive here also lets a human attach a normal VNC viewer when reproducing the probe.
    const char *durationEnv = std::getenv("HYREMOTE_RFB_TEST_SECONDS");
    int durationSeconds = durationEnv ? std::atoi(durationEnv) : 20;
    if (durationSeconds <= 0)
        durationSeconds = 20;
    for (int i = 0; i < durationSeconds * 20; ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(50);
    }

    transport->stop();
    std::cout << "STOPPED" << std::endl;
    return 0;
}
