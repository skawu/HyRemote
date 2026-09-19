#include "hyremote/core/storage.hpp"

#include <utility>

namespace hyremote {

CpuFrameStorage::CpuFrameStorage(std::vector<PlaneLayout> planes, ReleaseHook onRelease)
    : m_planes(std::move(planes))
    , m_onRelease(std::move(onRelease))
{
    m_buffers.reserve(m_planes.size());
    for (const PlaneLayout &plane : m_planes) {
        m_buffers.emplace_back(plane.bytes, std::byte{0});
        m_totalBytes += plane.bytes;
    }
}

CpuFrameStorage::~CpuFrameStorage()
{
    // The single, final release point: a pool/recycler observes exactly one call, on whatever
    // thread dropped the last reference.
    if (m_onRelease)
        m_onRelease();
}

std::shared_ptr<CpuFrameStorage> CpuFrameStorage::create(const std::vector<PlaneLayout> &planes,
                                                         ReleaseHook onRelease)
{
    if (planes.empty())
        return nullptr;

    for (const PlaneLayout &plane : planes) {
        if (plane.bytes == 0)
            return nullptr;
    }

    // The hook is deliberately invoked from the destructor: it is the single, final release
    // point of this storage, which is exactly what a pool/recycler needs to observe.
    return std::shared_ptr<CpuFrameStorage>(new CpuFrameStorage(planes, std::move(onRelease)));
}

std::shared_ptr<CpuFrameStorage> CpuFrameStorage::createSinglePlane(std::size_t bytesPerLine,
                                                                   std::size_t height,
                                                                   ReleaseHook onRelease)
{
    if (bytesPerLine == 0 || height == 0)
        return nullptr;

    return create({PlaneLayout{bytesPerLine, bytesPerLine * height}}, std::move(onRelease));
}

std::size_t CpuFrameStorage::planeCount() const noexcept
{
    return m_buffers.size();
}

std::optional<PlaneView> CpuFrameStorage::mapRead(std::size_t plane) const
{
    if (plane >= m_buffers.size())
        return std::nullopt;

    PlaneView view;
    view.data = m_buffers[plane].data();
    view.bytes = m_buffers[plane].size();
    view.stride = m_planes[plane].bytesPerLine;
    return view;
}

std::shared_ptr<const StorageExtension> CpuFrameStorage::extension(std::string_view extensionId) const
{
    static_cast<void>(extensionId);
    return nullptr;  // CPU storage exposes no external extension domain
}

std::byte *CpuFrameStorage::mutablePlane(std::size_t plane)
{
    if (plane >= m_buffers.size())
        return nullptr;

    return m_buffers[plane].data();
}

}  // namespace hyremote
