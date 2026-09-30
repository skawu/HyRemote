#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

namespace {

constexpr std::size_t kWidth = 1920;
constexpr std::size_t kHeight = 1080;
constexpr std::size_t kTile = 64;
constexpr double kReferenceBitsPerSecond = 100'000'000.0;

using Pixel = std::uint32_t; // 0x00RRGGBB; matches the TRLE candidate workload representation.
using Frame = std::vector<Pixel>;
using Bytes = std::vector<std::uint8_t>;
using Clock = std::chrono::steady_clock;

struct CaseResult
{
    std::string name;
    std::size_t rawBytes = 0;
    std::size_t zrleBytes = 0;
    double encodeP50Us = 0.0;
    double encodeP95Us = 0.0;
    bool roundTrip = false;
};

std::uint8_t red(Pixel p) { return static_cast<std::uint8_t>((p >> 16U) & 0xffU); }
std::uint8_t green(Pixel p) { return static_cast<std::uint8_t>((p >> 8U) & 0xffU); }
std::uint8_t blue(Pixel p) { return static_cast<std::uint8_t>(p & 0xffU); }

void appendCpixel(Bytes &out, Pixel p)
{
    // Keep the same 3-byte CPIXEL model as the accepted TRLE preflight so the comparison differs only in
    // ZRLE's 64x64 tiling and zlib stage.
    out.push_back(red(p));
    out.push_back(green(p));
    out.push_back(blue(p));
}

Pixel readCpixel(const Bytes &data, std::size_t &offset)
{
    if (offset + 3U > data.size())
        throw std::runtime_error("truncated CPIXEL");
    const Pixel r = data[offset++];
    const Pixel g = data[offset++];
    const Pixel b = data[offset++];
    return (r << 16U) | (g << 8U) | b;
}

int packedBits(std::size_t paletteSize)
{
    if (paletteSize == 2U)
        return 1;
    if (paletteSize <= 4U)
        return 2;
    return 4;
}

Bytes encodeZrleTiles(const Frame &frame)
{
    if (frame.size() != kWidth * kHeight)
        throw std::runtime_error("unexpected frame size");

    Bytes out;
    out.reserve(frame.size() * 3U / 2U);

    for (std::size_t tileY = 0; tileY < kHeight; tileY += kTile) {
        const std::size_t tileHeight = std::min(kTile, kHeight - tileY);
        for (std::size_t tileX = 0; tileX < kWidth; tileX += kTile) {
            const std::size_t tileWidth = std::min(kTile, kWidth - tileX);

            std::array<Pixel, 16> palette{};
            std::size_t paletteSize = 0;
            bool paletteOverflow = false;
            Bytes indices(tileWidth * tileHeight);

            for (std::size_t y = 0; y < tileHeight && !paletteOverflow; ++y) {
                for (std::size_t x = 0; x < tileWidth; ++x) {
                    const Pixel p = frame[(tileY + y) * kWidth + tileX + x];
                    std::size_t index = 0;
                    while (index < paletteSize && palette[index] != p)
                        ++index;
                    if (index == paletteSize) {
                        if (paletteSize == palette.size()) {
                            paletteOverflow = true;
                            break;
                        }
                        palette[paletteSize++] = p;
                    }
                    indices[y * tileWidth + x] = static_cast<std::uint8_t>(index);
                }
            }

            if (paletteOverflow) {
                out.push_back(0); // ZRLE raw CPIXEL tile.
                for (std::size_t y = 0; y < tileHeight; ++y) {
                    for (std::size_t x = 0; x < tileWidth; ++x)
                        appendCpixel(out, frame[(tileY + y) * kWidth + tileX + x]);
                }
                continue;
            }

            if (paletteSize == 1U) {
                out.push_back(1); // Solid tile.
                appendCpixel(out, palette[0]);
                continue;
            }

            out.push_back(static_cast<std::uint8_t>(paletteSize)); // Packed palette: 2..16.
            for (std::size_t i = 0; i < paletteSize; ++i)
                appendCpixel(out, palette[i]);

            const int bits = packedBits(paletteSize);
            for (std::size_t y = 0; y < tileHeight; ++y) {
                std::uint8_t byte = 0;
                int used = 0;
                for (std::size_t x = 0; x < tileWidth; ++x) {
                    byte = static_cast<std::uint8_t>((byte << bits) | indices[y * tileWidth + x]);
                    used += bits;
                    if (used == 8) {
                        out.push_back(byte);
                        byte = 0;
                        used = 0;
                    }
                }
                if (used != 0) {
                    byte = static_cast<std::uint8_t>(byte << (8 - used));
                    out.push_back(byte);
                }
            }
        }
    }
    return out;
}

Frame decodeZrleTiles(const Bytes &data)
{
    Frame frame(kWidth * kHeight);
    std::size_t offset = 0;

    for (std::size_t tileY = 0; tileY < kHeight; tileY += kTile) {
        const std::size_t tileHeight = std::min(kTile, kHeight - tileY);
        for (std::size_t tileX = 0; tileX < kWidth; tileX += kTile) {
            const std::size_t tileWidth = std::min(kTile, kWidth - tileX);
            if (offset >= data.size())
                throw std::runtime_error("truncated ZRLE tile");
            const std::uint8_t subencoding = data[offset++];

            if (subencoding == 0) {
                for (std::size_t y = 0; y < tileHeight; ++y) {
                    for (std::size_t x = 0; x < tileWidth; ++x)
                        frame[(tileY + y) * kWidth + tileX + x] = readCpixel(data, offset);
                }
                continue;
            }

            if (subencoding == 1) {
                const Pixel p = readCpixel(data, offset);
                for (std::size_t y = 0; y < tileHeight; ++y) {
                    for (std::size_t x = 0; x < tileWidth; ++x)
                        frame[(tileY + y) * kWidth + tileX + x] = p;
                }
                continue;
            }

            if (subencoding < 2 || subencoding > 16)
                throw std::runtime_error("candidate encoder emitted unsupported ZRLE subencoding");

            const std::size_t paletteSize = subencoding;
            std::array<Pixel, 16> palette{};
            for (std::size_t i = 0; i < paletteSize; ++i)
                palette[i] = readCpixel(data, offset);

            const int bits = packedBits(paletteSize);
            const std::uint8_t mask = static_cast<std::uint8_t>((1U << bits) - 1U);
            for (std::size_t y = 0; y < tileHeight; ++y) {
                int remaining = 0;
                std::uint8_t byte = 0;
                for (std::size_t x = 0; x < tileWidth; ++x) {
                    if (remaining == 0) {
                        if (offset >= data.size())
                            throw std::runtime_error("truncated ZRLE palette row");
                        byte = data[offset++];
                        remaining = 8;
                    }
                    const int shift = remaining - bits;
                    const auto index = static_cast<std::uint8_t>((byte >> shift) & mask);
                    if (index >= paletteSize)
                        throw std::runtime_error("invalid ZRLE palette index");
                    frame[(tileY + y) * kWidth + tileX + x] = palette[index];
                    remaining -= bits;
                }
            }
        }
    }

    if (offset != data.size())
        throw std::runtime_error("unexpected trailing ZRLE tile bytes");
    return frame;
}

class DeflateStream
{
public:
    DeflateStream()
    {
        m_stream = {};
        if (deflateInit(&m_stream, Z_DEFAULT_COMPRESSION) != Z_OK)
            throw std::runtime_error("deflateInit failed");
    }

    ~DeflateStream() { deflateEnd(&m_stream); }

    DeflateStream(const DeflateStream &) = delete;
    DeflateStream &operator=(const DeflateStream &) = delete;

    Bytes encodeRectangle(const Bytes &plain)
    {
        if (plain.size() > static_cast<std::size_t>(std::numeric_limits<uInt>::max()))
            throw std::runtime_error("ZRLE payload exceeds zlib input bound");

        m_stream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(plain.data()));
        m_stream.avail_in = static_cast<uInt>(plain.size());

        Bytes out;
        std::array<std::uint8_t, 64 * 1024> buffer{};
        do {
            m_stream.next_out = reinterpret_cast<Bytef *>(buffer.data());
            m_stream.avail_out = static_cast<uInt>(buffer.size());
            const int result = deflate(&m_stream, Z_SYNC_FLUSH);
            if (result != Z_OK)
                throw std::runtime_error("ZRLE deflate failed");
            const std::size_t produced = buffer.size() - m_stream.avail_out;
            out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(produced));
        } while (m_stream.avail_in != 0U || m_stream.avail_out == 0U);
        return out;
    }

private:
    z_stream m_stream{};
};

class InflateStream
{
public:
    InflateStream()
    {
        m_stream = {};
        if (inflateInit(&m_stream) != Z_OK)
            throw std::runtime_error("inflateInit failed");
    }

    ~InflateStream() { inflateEnd(&m_stream); }

    InflateStream(const InflateStream &) = delete;
    InflateStream &operator=(const InflateStream &) = delete;

    Bytes decodeRectangle(const Bytes &compressed, std::size_t expectedPlainBytes)
    {
        if (compressed.size() > static_cast<std::size_t>(std::numeric_limits<uInt>::max()))
            throw std::runtime_error("compressed ZRLE payload exceeds zlib input bound");

        m_stream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(compressed.data()));
        m_stream.avail_in = static_cast<uInt>(compressed.size());

        Bytes out;
        out.reserve(expectedPlainBytes);
        std::array<std::uint8_t, 64 * 1024> buffer{};
        while (m_stream.avail_in != 0U || out.size() < expectedPlainBytes) {
            m_stream.next_out = reinterpret_cast<Bytef *>(buffer.data());
            m_stream.avail_out = static_cast<uInt>(buffer.size());
            const int result = inflate(&m_stream, Z_SYNC_FLUSH);
            if (result != Z_OK && result != Z_BUF_ERROR)
                throw std::runtime_error("ZRLE inflate failed");
            const std::size_t produced = buffer.size() - m_stream.avail_out;
            out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(produced));
            if (produced == 0U && m_stream.avail_in == 0U)
                break;
        }

        if (m_stream.avail_in != 0U || out.size() != expectedPlainBytes)
            throw std::runtime_error("ZRLE rectangle did not decode to the expected boundary");
        return out;
    }

private:
    z_stream m_stream{};
};

void fillRect(Frame &frame, std::size_t x0, std::size_t y0, std::size_t w, std::size_t h, Pixel color)
{
    const std::size_t x1 = std::min(kWidth, x0 + w);
    const std::size_t y1 = std::min(kHeight, y0 + h);
    for (std::size_t y = y0; y < y1; ++y)
        std::fill(frame.begin() + static_cast<std::ptrdiff_t>(y * kWidth + x0),
                  frame.begin() + static_cast<std::ptrdiff_t>(y * kWidth + x1), color);
}

Frame makeStatic()
{
    return Frame(kWidth * kHeight, 0x203040U);
}

Frame makeLocalizedUi()
{
    Frame frame(kWidth * kHeight, 0x182028U);
    fillRect(frame, 80, 70, 1760, 940, 0x25313cU);
    fillRect(frame, 120, 110, 520, 300, 0x33495cU);
    fillRect(frame, 680, 110, 1120, 300, 0x202a34U);
    for (std::size_t i = 0; i < 8; ++i) {
        fillRect(frame, 120 + i * 205, 470, 170, 90, (i % 2U) ? 0x3b8f60U : 0x31506dU);
        fillRect(frame, 120 + i * 205, 590, 170, 26, 0xd0d8e0U);
    }
    fillRect(frame, 120, 680, 1680, 250, 0x111820U);
    fillRect(frame, 1450, 840, 300, 50, 0xe0a030U);
    return frame;
}

Frame makeInteractivePalette()
{
    Frame frame(kWidth * kHeight);
    constexpr std::array<Pixel, 8> colors{{
        0x111820U, 0x1d2b36U, 0x294052U, 0x345d70U,
        0x3a785fU, 0x9a7a32U, 0xb8c4ccU, 0xe8eef2U,
    }};
    for (std::size_t y = 0; y < kHeight; ++y) {
        for (std::size_t x = 0; x < kWidth; ++x) {
            const std::size_t band = ((x / 5U) + (y / 7U) + ((x ^ y) >> 5U)) % colors.size();
            frame[y * kWidth + x] = colors[band];
        }
    }
    fillRect(frame, 760, 420, 400, 220, 0xf0c040U);
    return frame;
}

Frame makeHighEntropy()
{
    Frame frame(kWidth * kHeight);
    std::uint32_t state = 0x6d2b79f5U;
    for (Pixel &pixel : frame) {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        pixel = state & 0x00ffffffU;
    }
    return frame;
}

double percentile(std::vector<double> samples, double p)
{
    std::sort(samples.begin(), samples.end());
    const auto index = static_cast<std::size_t>(p * static_cast<double>(samples.size() - 1U));
    return samples[index];
}

CaseResult runCase(std::string name, const Frame &frame)
{
    // Use a fresh protocol connection for each sample. This deliberately includes zlib stream setup and prevents
    // repeated identical frames from making the candidate look artificially cheap through cross-rectangle history.
    constexpr int kIterations = 101;
    std::vector<double> timings;
    timings.reserve(kIterations - 1);
    Bytes firstPlain;
    Bytes firstCompressed;

    for (int i = 0; i < kIterations; ++i) {
        const auto begin = Clock::now();
        const Bytes plain = encodeZrleTiles(frame);
        DeflateStream encoder;
        const Bytes compressed = encoder.encodeRectangle(plain);
        const auto end = Clock::now();
        if (i != 0)
            timings.push_back(std::chrono::duration<double, std::micro>(end - begin).count());
        if (i == 0) {
            firstPlain = plain;
            firstCompressed = compressed;
        }
    }

    InflateStream decoder;
    const Bytes restoredPlain = decoder.decodeRectangle(firstCompressed, firstPlain.size());

    CaseResult result;
    result.name = std::move(name);
    result.rawBytes = frame.size() * 4U;
    result.zrleBytes = firstCompressed.size() + 4U; // RFC 6143 rectangle length prefix plus zlibData.
    result.encodeP50Us = percentile(timings, 0.50);
    result.encodeP95Us = percentile(timings, 0.95);
    result.roundTrip = restoredPlain == firstPlain && decodeZrleTiles(restoredPlain) == frame;
    return result;
}

bool persistentStreamRoundTrip(const std::array<Frame, 4> &frames)
{
    // RFC 6143 requires one zlib stream per RFB connection and strict rectangle ordering. Drive several differently
    // shaped payloads through one encoder/decoder pair so a per-rectangle compress() implementation cannot pass.
    DeflateStream encoder;
    InflateStream decoder;
    constexpr std::array<std::size_t, 8> order{{0, 1, 2, 3, 3, 2, 1, 0}};

    for (std::size_t index : order) {
        const Bytes plain = encodeZrleTiles(frames[index]);
        const Bytes compressed = encoder.encodeRectangle(plain);
        const Bytes restored = decoder.decodeRectangle(compressed, plain.size());
        if (restored != plain || decodeZrleTiles(restored) != frames[index])
            return false;
    }
    return true;
}

double wireMs(std::size_t bytes)
{
    return static_cast<double>(bytes) * 8.0 * 1000.0 / kReferenceBitsPerSecond;
}

void printResult(const CaseResult &result)
{
    const double ratio = static_cast<double>(result.zrleBytes) / static_cast<double>(result.rawBytes);
    std::cout << std::fixed << std::setprecision(3)
              << "CASE=" << result.name
              << " RAW_BYTES=" << result.rawBytes
              << " ZRLE_BYTES=" << result.zrleBytes
              << " ZRLE_TO_RAW=" << ratio
              << " RAW_WIRE_MS_100MBPS=" << wireMs(result.rawBytes)
              << " ZRLE_WIRE_MS_100MBPS=" << wireMs(result.zrleBytes)
              << " COLD_ENCODE_P50_US=" << result.encodeP50Us
              << " COLD_ENCODE_P95_US=" << result.encodeP95Us
              << " ROUNDTRIP=" << (result.roundTrip ? "PASS" : "FAIL") << '\n';
}

} // namespace

int main()
{
    try {
        const std::array<Frame, 4> frames{{
            makeStatic(),
            makeLocalizedUi(),
            makeInteractivePalette(),
            makeHighEntropy(),
        }};
        const std::array<CaseResult, 4> results{{
            runCase("W1_STATIC", frames[0]),
            runCase("W2_LOCALIZED", frames[1]),
            runCase("W3_INTERACTIVE", frames[2]),
            runCase("W4_HIGH_ENTROPY", frames[3]),
        }};

        bool pass = true;
        std::cout << "ZLIB_VERSION=" << zlibVersion() << '\n';
        for (const auto &result : results) {
            printResult(result);
            pass = pass && result.roundTrip && result.zrleBytes < result.rawBytes;
        }

        const bool streamOrder = persistentStreamRoundTrip(frames);
        std::cout << "PERSISTENT_STREAM_ORDER=" << (streamOrder ? "PASS" : "FAIL") << '\n';
        pass = pass && streamOrder;

        // W4 defeats palette compression. ZRLE still starts from 3-byte CPIXEL raw tiles, so zlib framing should
        // remain structurally below 77% of the current 4-byte Raw payload without inventing a product performance bar.
        const double w4Ratio = static_cast<double>(results[3].zrleBytes)
                               / static_cast<double>(results[3].rawBytes);
        const bool rawFallbackBound = w4Ratio < 0.77;
        std::cout << "ZRLE_RAW_CPIXEL_BOUND=" << (rawFallbackBound ? "PASS" : "FAIL") << '\n';
        pass = pass && rawFallbackBound;

        std::cout << "PREFLIGHT_RESULT=" << (pass ? "PASS" : "FAIL") << '\n';
        return pass ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << "PREFLIGHT_EXCEPTION=" << error.what() << '\n';
        std::cout << "PREFLIGHT_RESULT=FAIL\n";
        return 2;
    }
}
