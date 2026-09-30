#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kWidth = 1920;
constexpr int kHeight = 1080;
constexpr int kTile = 64;
constexpr std::size_t kMaxDamageRects = 64;
constexpr double kReferenceBitsPerSecond = 100'000'000.0;
using Pixel = std::uint32_t;
using Frame = std::vector<Pixel>;
using Clock = std::chrono::steady_clock;

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool empty() const { return w <= 0 || h <= 0; }
    int right() const { return x + w; }
    int bottom() const { return y + h; }
};

bool operator==(const Rect &a, const Rect &b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

std::uint64_t area(const Rect &r)
{
    return r.empty() ? 0U : static_cast<std::uint64_t>(r.w) * static_cast<std::uint64_t>(r.h);
}

Rect intersection(const Rect &a, const Rect &b)
{
    const int x0 = std::max(a.x, b.x);
    const int y0 = std::max(a.y, b.y);
    const int x1 = std::min(a.right(), b.right());
    const int y1 = std::min(a.bottom(), b.bottom());
    return {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
}

Rect bounds(const Rect &a, const Rect &b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    const int x0 = std::min(a.x, b.x);
    const int y0 = std::min(a.y, b.y);
    const int x1 = std::max(a.right(), b.right());
    const int y1 = std::max(a.bottom(), b.bottom());
    return {x0, y0, x1 - x0, y1 - y0};
}

bool contains(const Rect &outer, const Rect &inner)
{
    return !inner.empty() && outer.x <= inner.x && outer.y <= inner.y
           && outer.right() >= inner.right() && outer.bottom() >= inner.bottom();
}

std::vector<Rect> subtractRect(const Rect &source, const Rect &cut)
{
    const Rect hit = intersection(source, cut);
    if (hit.empty()) return {source};
    std::vector<Rect> out;
    for (const Rect &r : {Rect{source.x, source.y, source.w, hit.y - source.y},
                          Rect{source.x, hit.bottom(), source.w, source.bottom() - hit.bottom()},
                          Rect{source.x, hit.y, hit.x - source.x, hit.h},
                          Rect{hit.right(), hit.y, source.right() - hit.right(), hit.h}}) {
        if (!r.empty()) out.push_back(r);
    }
    return out;
}

class BoundedRegion {
public:
    void clear() { m_rects.clear(); }
    bool empty() const { return m_rects.empty(); }
    std::size_t rectCount() const { return m_rects.size(); }
    const std::vector<Rect> &rects() const { return m_rects; }

    void add(Rect rect)
    {
        if (rect.empty()) return;
        for (auto it = m_rects.begin(); it != m_rects.end();) {
            if (contains(*it, rect)) return;
            if (contains(rect, *it)) {
                it = m_rects.erase(it);
                continue;
            }
            ++it;
        }
        m_rects.push_back(rect);
        enforceBound();
    }

    void add(const BoundedRegion &other)
    {
        for (const Rect &r : other.m_rects) add(r);
    }

    BoundedRegion intersected(Rect request) const
    {
        BoundedRegion out;
        for (const Rect &r : m_rects) out.add(intersection(r, request));
        return out;
    }

    void subtract(const BoundedRegion &delivered)
    {
        for (const Rect &cut : delivered.m_rects) subtract(cut);
    }

    std::uint64_t conservativeArea() const
    {
        std::uint64_t total = 0;
        for (const Rect &r : m_rects) total += area(r);
        return total;
    }

private:
    void subtract(Rect cut)
    {
        if (cut.empty()) return;
        std::vector<Rect> next;
        for (const Rect &r : m_rects) {
            const auto pieces = subtractRect(r, cut);
            next.insert(next.end(), pieces.begin(), pieces.end());
        }
        m_rects = std::move(next);
        enforceBound();
    }

    void enforceBound()
    {
        if (m_rects.size() <= kMaxDamageRects) return;
        Rect all;
        for (const Rect &r : m_rects) all = bounds(all, r);
        m_rects.clear();
        if (!all.empty()) m_rects.push_back(all);
    }

    std::vector<Rect> m_rects;
};

BoundedRegion changedTiles(const Frame &before, const Frame &after)
{
    if (before.size() != static_cast<std::size_t>(kWidth * kHeight) || after.size() != before.size())
        throw std::runtime_error("unexpected frame size");

    BoundedRegion result;
    for (int y0 = 0; y0 < kHeight; y0 += kTile) {
        const int h = std::min(kTile, kHeight - y0);
        for (int x0 = 0; x0 < kWidth; x0 += kTile) {
            const int w = std::min(kTile, kWidth - x0);
            bool changed = false;
            for (int row = 0; row < h; ++row) {
                const Pixel *a = before.data() + static_cast<std::size_t>((y0 + row) * kWidth + x0);
                const Pixel *b = after.data() + static_cast<std::size_t>((y0 + row) * kWidth + x0);
                if (std::memcmp(a, b, static_cast<std::size_t>(w) * sizeof(Pixel)) != 0) {
                    changed = true;
                    break;
                }
            }
            if (changed) result.add({x0, y0, w, h});
        }
    }
    return result;
}

void fillRect(Frame &frame, Rect rect, Pixel color)
{
    rect = intersection(rect, {0, 0, kWidth, kHeight});
    for (int y = rect.y; y < rect.bottom(); ++y) {
        std::fill(frame.begin() + static_cast<std::ptrdiff_t>(y * kWidth + rect.x),
                  frame.begin() + static_cast<std::ptrdiff_t>(y * kWidth + rect.right()), color);
    }
}

Frame makeBase()
{
    Frame frame(static_cast<std::size_t>(kWidth * kHeight), 0x182028U);
    fillRect(frame, {80, 70, 1760, 940}, 0x25313cU);
    fillRect(frame, {120, 110, 520, 300}, 0x33495cU);
    fillRect(frame, {680, 110, 1120, 300}, 0x202a34U);
    fillRect(frame, {120, 680, 1680, 250}, 0x111820U);
    return frame;
}

Frame makeLocalized(const Frame &base)
{
    Frame frame = base;
    fillRect(frame, {1024, 512, 256, 96}, 0xe0a030U);
    return frame;
}

Frame makeInteractive(const Frame &base)
{
    Frame frame = base;
    fillRect(frame, {240, 450, 160, 64}, 0x3b8f60U);
    fillRect(frame, {820, 520, 192, 64}, 0xb8c4ccU);
    fillRect(frame, {1420, 790, 224, 80}, 0xe0a030U);
    return frame;
}

Frame makeHighMotion()
{
    Frame frame(static_cast<std::size_t>(kWidth * kHeight));
    std::uint32_t state = 0x6d2b79f5U;
    for (Pixel &pixel : frame) {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        pixel = state & 0x00ffffffU;
    }
    return frame;
}

void copyRegion(Frame &viewer, const Frame &current, const BoundedRegion &region)
{
    for (const Rect &r : region.rects()) {
        for (int y = r.y; y < r.bottom(); ++y) {
            const auto offset = static_cast<std::size_t>(y * kWidth + r.x);
            std::copy_n(current.begin() + static_cast<std::ptrdiff_t>(offset), r.w,
                        viewer.begin() + static_cast<std::ptrdiff_t>(offset));
        }
    }
}

class ViewerState {
public:
    void accumulate(const BoundedRegion &damage) { m_pending.add(damage); }
    bool hasPending() const { return !m_pending.empty(); }
    std::size_t pendingRectCount() const { return m_pending.rectCount(); }

    BoundedRegion request(bool incremental, Rect requested)
    {
        BoundedRegion delivered;
        if (incremental) {
            delivered = m_pending.intersected(requested);
            if (delivered.empty()) return delivered;
        } else {
            delivered.add(requested);
        }
        m_pending.subtract(delivered);
        return delivered;
    }

    void invalidateFull(int width, int height)
    {
        m_pending.clear();
        m_pending.add({0, 0, width, height});
    }

private:
    BoundedRegion m_pending;
};

double percentile(std::vector<double> samples, double p)
{
    std::sort(samples.begin(), samples.end());
    const auto index = static_cast<std::size_t>(p * static_cast<double>(samples.size() - 1U));
    return samples[index];
}

struct DiffResult {
    std::string name;
    BoundedRegion region;
    double p50Us = 0.0;
    double p95Us = 0.0;
};

DiffResult measureDiff(std::string name, const Frame &before, const Frame &after)
{
    constexpr int kIterations = 101;
    std::vector<double> timings;
    timings.reserve(kIterations - 1);
    BoundedRegion first;
    for (int i = 0; i < kIterations; ++i) {
        const auto begin = Clock::now();
        BoundedRegion region = changedTiles(before, after);
        const auto end = Clock::now();
        if (i == 0) first = region;
        else timings.push_back(std::chrono::duration<double, std::micro>(end - begin).count());
    }
    return {std::move(name), std::move(first), percentile(timings, 0.50), percentile(timings, 0.95)};
}

void printDiff(const DiffResult &result)
{
    const std::uint64_t pixels = result.region.conservativeArea();
    const std::uint64_t bytes = pixels * sizeof(Pixel);
    const double ratio = static_cast<double>(pixels) / static_cast<double>(kWidth * kHeight);
    const double wireMs = static_cast<double>(bytes) * 8.0 * 1000.0 / kReferenceBitsPerSecond;
    std::cout << std::fixed << std::setprecision(3)
              << "CASE=" << result.name
              << " DIRTY_RECTS=" << result.region.rectCount()
              << " DIRTY_AREA_RATIO=" << ratio
              << " RAW_DIRTY_BYTES=" << bytes
              << " RAW_DIRTY_WIRE_MS_100MBPS=" << wireMs
              << " DIFF_P50_US=" << result.p50Us
              << " DIFF_P95_US=" << result.p95Us << '\n';
}

bool runStateContracts(const Frame &base)
{
    bool pass = true;
    const Rect full{0, 0, kWidth, kHeight};

    ViewerState staticViewer;
    staticViewer.accumulate(changedTiles(base, base));
    const bool staticPass = staticViewer.request(true, full).empty() && !staticViewer.hasPending();
    std::cout << "STATIC_NO_REDUNDANT_PAYLOAD=" << (staticPass ? "PASS" : "FAIL") << '\n';
    pass = pass && staticPass;

    Frame b = base;
    fillRect(b, {256, 256, 128, 64}, 0xe0a030U);
    Frame c = base;
    fillRect(c, {640, 320, 128, 64}, 0xe0a030U);
    Frame d = base;
    fillRect(d, {1024, 384, 128, 64}, 0xe0a030U);

    ViewerState fast;
    ViewerState slow;
    Frame fastPixels = base;
    Frame slowPixels = base;
    const std::array<const Frame *, 4> sequence{{&base, &b, &c, &d}};
    for (std::size_t i = 1; i < sequence.size(); ++i) {
        const BoundedRegion damage = changedTiles(*sequence[i - 1], *sequence[i]);
        fast.accumulate(damage);
        slow.accumulate(damage);
        const BoundedRegion delivered = fast.request(true, full);
        copyRegion(fastPixels, *sequence[i], delivered);
    }
    const bool slowHadAccumulated = slow.hasPending();
    const BoundedRegion slowDelivered = slow.request(true, full);
    copyRegion(slowPixels, d, slowDelivered);
    const bool accumulatedPass = slowHadAccumulated && fastPixels == d && slowPixels == d
                                 && !fast.hasPending() && !slow.hasPending();
    std::cout << "SLOW_VIEWER_ABC_ACCUMULATION=" << (accumulatedPass ? "PASS" : "FAIL") << '\n';
    pass = pass && accumulatedPass;

    Frame partialTarget = d;
    const Rect left{128, 704, 256, 128};
    const Rect right{1536, 704, 256, 128};
    fillRect(partialTarget, left, 0x3b8f60U);
    fillRect(partialTarget, right, 0xb8c4ccU);
    ViewerState partial;
    Frame partialPixels = d;
    partial.accumulate(changedTiles(d, partialTarget));
    const BoundedRegion leftDelivered = partial.request(true, left);
    copyRegion(partialPixels, partialTarget, leftDelivered);
    const bool rightRemains = partial.hasPending();
    const bool leftSecondEmpty = partial.request(true, left).empty();
    const BoundedRegion rightDelivered = partial.request(true, right);
    copyRegion(partialPixels, partialTarget, rightDelivered);
    const bool partialPass = rightRemains && leftSecondEmpty && partialPixels == partialTarget
                             && !partial.hasPending();
    std::cout << "PARTIAL_REQUEST_PRESERVES_UNSENT_DAMAGE=" << (partialPass ? "PASS" : "FAIL") << '\n';
    pass = pass && partialPass;

    ViewerState nonIncremental;
    const BoundedRegion forced = nonIncremental.request(false, {320, 192, 256, 128});
    const bool nonIncrementalPass = forced.rectCount() == 1
                                    && forced.rects().front() == Rect{320, 192, 256, 128};
    std::cout << "NON_INCREMENTAL_ALWAYS_REFRESH=" << (nonIncrementalPass ? "PASS" : "FAIL") << '\n';
    pass = pass && nonIncrementalPass;

    ViewerState resize;
    resize.invalidateFull(1280, 720);
    const BoundedRegion resized = resize.request(true, {0, 0, 1280, 720});
    const bool resizePass = resized.rectCount() == 1 && resized.rects().front() == Rect{0, 0, 1280, 720}
                            && !resize.hasPending();
    std::cout << "RESIZE_FORCES_FULL_REFRESH=" << (resizePass ? "PASS" : "FAIL") << '\n';
    pass = pass && resizePass;

    Frame fragmented = base;
    for (int y = 16; y < kHeight; y += 128) {
        for (int x = 16; x < kWidth; x += 128)
            fragmented[static_cast<std::size_t>(y * kWidth + x)] ^= 0x00ffffffU;
    }
    const BoundedRegion fragmentedDamage = changedTiles(base, fragmented);
    ViewerState bounded;
    Frame boundedPixels = base;
    bounded.accumulate(fragmentedDamage);
    const bool wasBounded = bounded.pendingRectCount() <= kMaxDamageRects;
    const BoundedRegion all = bounded.request(true, full);
    copyRegion(boundedPixels, fragmented, all);
    const bool boundPass = wasBounded && boundedPixels == fragmented && !bounded.hasPending();
    std::cout << "BOUNDED_REGION_COALESCING=" << (boundPass ? "PASS" : "FAIL")
              << " MAX_RECTS=" << kMaxDamageRects << '\n';
    pass = pass && boundPass;

    return pass;
}

} // namespace

int main()
{
    try {
        const Frame base = makeBase();
        const Frame localized = makeLocalized(base);
        const Frame interactive = makeInteractive(base);
        const Frame highMotion = makeHighMotion();

        const std::array<DiffResult, 4> results{{
            measureDiff("W1_STATIC", base, base),
            measureDiff("W2_LOCALIZED", base, localized),
            measureDiff("W3_INTERACTIVE", base, interactive),
            measureDiff("W4_HIGH_MOTION", base, highMotion),
        }};
        for (const auto &result : results) printDiff(result);

        const bool states = runStateContracts(base);
        const bool staticEmpty = results[0].region.empty();
        const bool localizedSmaller = results[1].region.conservativeArea()
                                      < static_cast<std::uint64_t>(kWidth * kHeight) / 4U;
        const bool interactiveSmaller = results[2].region.conservativeArea()
                                        < static_cast<std::uint64_t>(kWidth * kHeight) / 2U;
        const bool highMotionFull = results[3].region.conservativeArea()
                                    >= static_cast<std::uint64_t>(kWidth * kHeight) * 9U / 10U;

        std::cout << "WORKLOAD_SHAPE_STATIC_EMPTY=" << (staticEmpty ? "PASS" : "FAIL") << '\n';
        std::cout << "WORKLOAD_SHAPE_LOCALIZED=" << (localizedSmaller ? "PASS" : "FAIL") << '\n';
        std::cout << "WORKLOAD_SHAPE_INTERACTIVE=" << (interactiveSmaller ? "PASS" : "FAIL") << '\n';
        std::cout << "WORKLOAD_SHAPE_HIGH_MOTION=" << (highMotionFull ? "PASS" : "FAIL") << '\n';

        const bool pass = states && staticEmpty && localizedSmaller && interactiveSmaller && highMotionFull;
        std::cout << "PREFLIGHT_RESULT=" << (pass ? "PASS" : "FAIL") << '\n';
        return pass ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << "PREFLIGHT_EXCEPTION=" << error.what() << '\n';
        std::cout << "PREFLIGHT_RESULT=FAIL\n";
        return 2;
    }
}
