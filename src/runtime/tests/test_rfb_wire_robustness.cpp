#include <QCoreApplication>

#include <chrono>
#include <cstdint>
#include <vector>

#include "rfb_interaction_latency_stats.hpp"

#define main hyremote_rfb_wire_robustness_base_main
#include "test_rfb_wire_robustness_base.cpp"
#undef main

namespace {

bool sendPointerButton(QTcpSocket &socket,
                       bool pressed,
                       std::uint16_t x = 17,
                       std::uint16_t y = 19)
{
    QByteArray pointer;
    pointer.append(char(5));
    pointer.append(static_cast<char>(pressed ? 1 : 0));
    appendU16(pointer, x);
    appendU16(pointer, y);
    return writeAll(socket, pointer);
}

void runInteractionLatencyProbe(const std::vector<std::int32_t> &encodings,
                                std::int32_t expectedEncoding,
                                const char *encodingLabel)
{
    constexpr std::size_t sampleCount = 12;
    constexpr std::uint32_t background = 0x203040U;
    constexpr std::uint32_t oddColor = 0xd04030U;
    constexpr std::uint32_t evenColor = 0x30a060U;
    constexpr hyremote::Rect markerRect{8, 8, 8, 8};

    const quint16 port = freePort();
    CHECK(port != 0);
    if (port == 0)
        return;

    Recorder recorder;
    auto transport = HyRemote::detail::createRfbTransport(QHostAddress::LocalHost, port,
                                                          HyRemote::detail::RfbSecurityConfig{});
    CHECK(transport != nullptr);
    if (!transport)
        return;

    hyremote::Transport *transportPtr = transport.get();
    std::size_t pressCount = 0;
    CHECK(transport->start(
        [&](const hyremote::InputEvent &event) {
            recorder.record(event);
            if (event.kind != hyremote::InputEventKind::PointerButton
                || event.button != hyremote::PointerButton::Left || !event.pressed) {
                return;
            }

            ++pressCount;
            const std::uint32_t color = (pressCount % 2U) == 1U ? oddColor : evenColor;
            transportPtr->enqueueFrame(makeFrame(64, 48, background, {{markerRect, color}}));
        },
        [&](const hyremote::TransportEvent &event) { recorder.record(event); }));
    transport->enqueueFrame(makeFrame(64, 48, background));

    QTcpSocket socket;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    CHECK(connectRawRfb(socket, port, false, &width, &height));
    CHECK(width == 64 && height == 48);
    CHECK(sendSetEncodings(socket, encodings));
    CHECK(sendUpdateRequest(socket, false, 0, 0, width, height));

    TestFramebuffer frame;
    frame.resize(width, height);
    std::vector<UpdateRect> rectangles;
    CHECK(readFramebufferUpdate(socket, frame, rectangles));
    CHECK(rectangles.size() == 1U && rectangles.front().encoding == expectedEncoding);
    CHECK(frame.get(markerRect.x, markerRect.y) == background);

    std::vector<std::chrono::microseconds> samples;
    samples.reserve(sampleCount);

    for (std::size_t index = 0; index < sampleCount; ++index) {
        CHECK(sendUpdateRequest(socket, true, 0, 0, width, height));

        const auto started = std::chrono::steady_clock::now();
        CHECK(sendPointerButton(socket, true));
        CHECK(readFramebufferUpdate(socket, frame, rectangles));
        const auto completed = std::chrono::steady_clock::now();

        CHECK(rectangles.size() == 1U && rectangles.front().encoding == expectedEncoding);
        const std::uint32_t expected = (index % 2U) == 0U ? oddColor : evenColor;
        CHECK(frame.get(markerRect.x, markerRect.y) == expected);
        samples.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(completed - started));

        CHECK(sendPointerButton(socket, false));
    }

    CHECK(samples.size() == sampleCount);
    const auto summary = HyRemote::test::summarizeInteractionLatencies(samples);
    CHECK(summary.sampleCount == sampleCount);
    CHECK(summary.minUs >= 0);
    CHECK(summary.p50Us >= summary.minUs);
    CHECK(summary.p95Us >= summary.p50Us);
    CHECK(summary.maxUs >= summary.p95Us);
    HyRemote::test::printInteractionLatencySummary(std::cout, encodingLabel, summary);

    socket.disconnectFromHost();
    waitForDisconnected(socket);
    transport->stop();
}

}  // namespace

int main(int argc, char **argv)
{
    const int baseResult = hyremote_rfb_wire_robustness_base_main(argc, argv);
    if (baseResult != 0)
        return baseResult;

    runInteractionLatencyProbe({15, 0}, 15, "trle");
    runInteractionLatencyProbe({0}, 0, "raw");

    if (failures != 0)
        std::cerr << failures << " RFB wire-robustness checks failed\n";
    return failures == 0 ? 0 : 1;
}
