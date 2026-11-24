#include "quick/quick_image_frame_storage.hpp"

#include <QColor>
#include <QImage>

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>

namespace {

bool verify(bool condition, const char *message)
{
    if (!condition)
        std::fprintf(stderr, "quick_image_frame_storage: %s\n", message);
    return condition;
}

bool testImplicitSharedLifetime()
{
    QImage owner(3, 2, QImage::Format_RGBA8888_Premultiplied);
    owner.fill(QColor(16, 36, 58));
    if (!verify(!owner.isNull(), "cannot allocate fixture"))
        return false;

    const auto *originalBytes = owner.constBits();
    const std::size_t originalStride = static_cast<std::size_t>(owner.bytesPerLine());
    const auto storage = HyRemote::detail::QuickImageFrameStorage::create(owner);
    if (!verify(storage && storage->kind() == hyremote::StorageKind::Cpu,
                "a valid image must yield CPU-readable storage"))
        return false;

    auto mapped = storage->mapRead(0);
    if (!verify(mapped && mapped->data == reinterpret_cast<const std::byte *>(originalBytes),
                "create must retain the same pixel allocation, not memcpy"))
        return false;
    if (!verify(mapped->stride == originalStride
                    && mapped->bytes >= originalStride * static_cast<std::size_t>(owner.height())
                    && storage->planeCount() == 1U && !storage->mapRead(1),
                "plane geometry/stride contract"))
        return false;

    // A mutable QImage copy must detach without changing published pixels.
    QImage changed = owner;
    changed.bits()[0] = 0;
    if (!verify(changed.constBits() != originalBytes
                    && mapped->data == reinterpret_cast<const std::byte *>(originalBytes),
                "source mutation detached shared image"))
        return false;

    owner = QImage(); // Simulates release of the grab result's owning QImage.
    changed = QImage();
    const auto afterRelease = storage->mapRead(0);
    if (!verify(afterRelease && afterRelease->data == mapped->data,
                "storage must outlive the originating QImage"))
        return false;

    // Transport threads only call the const read interface and retain a
    // shared_ptr until they have finished reading the mapped bytes.
    std::atomic<bool> readerOk{false};
    std::thread reader([held = storage, &readerOk] {
        const auto crossThread = held->mapRead(0);
        readerOk = crossThread && crossThread->data && crossThread->stride >= 12U;
    });
    reader.join();
    return verify(readerOk.load(), "cross-thread read-only mapping");
}

bool testFormatGateAndConvertedPixels()
{
    if (!verify(!HyRemote::detail::QuickImageFrameStorage::create(QImage()),
                "null image must be rejected"))
        return false;

    QImage source(2, 2, QImage::Format_ARGB32_Premultiplied);
    source.fill(QColor(255, 64, 32, 128));
    if (!verify(!HyRemote::detail::QuickImageFrameStorage::create(source),
                "Qt5 ARGB32 must never be published as RGBA8888"))
        return false;

    const QImage normalized = source.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    const auto storage = HyRemote::detail::QuickImageFrameStorage::create(normalized);
    const auto mapped = storage ? storage->mapRead(0) : std::nullopt;
    if (!verify(mapped && mapped->data
                    == reinterpret_cast<const std::byte *>(normalized.constBits()),
                "normalized image must share the converted pixel allocation"))
        return false;

    // Correctness is byte-for-byte relative to Qt's normalized, premultiplied
    // color result, not an endianness-dependent ARGB32 memory reinterpretation.
    const auto *actual = reinterpret_cast<const unsigned char *>(mapped->data);
    return verify(std::memcmp(actual, normalized.constBits(), 4U) == 0
                      && actual[3] == 128U,
                  "premultiplied alpha/color bytes must be preserved");
}

} // namespace

int main()
{
    if (!testImplicitSharedLifetime() || !testFormatGateAndConvertedPixels())
        return 1;
    std::puts("quick_image_frame_storage PASS");
    return 0;
}
