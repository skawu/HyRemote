#pragma once

// Deterministic fakes for the Core contract tests.
//
// They model the two obligations the Core depends on:
//   - a CaptureSource may complete frames on a backend-defined thread;
//   - a Transport::enqueueFrame() call is a bounded/nonblocking post/enqueue.
//
// `EnqueueGate` lets a test block the transport hand-off *deterministically* (no sleeps): while
// the gate is closed the dispatch worker is provably stuck inside `enqueueFrame()`, which is how
// the capture-path-independence tests prove their point.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "hyremote/core/hyremote_core.hpp"
#include "test_support.hpp"

namespace hyremote::test {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

inline std::shared_ptr<CpuFrameStorage> makeStorage(std::uint32_t width = 8,
                                                    std::uint32_t height = 4,
                                                    CpuFrameStorage::ReleaseHook onRelease = {})
{
    return CpuFrameStorage::createSinglePlane(static_cast<std::size_t>(width) * 4U, height,
                                             std::move(onRelease));
}

// Builds a frame that is valid by default: CPU storage, single plane, completion timing filled
// with `Clock::now()`. Tests override whatever they want to exercise.
inline RemoteFrame makeFrame(std::uint32_t width = 8, std::uint32_t height = 4)
{
    RemoteFrame frame;
    frame.geometry.size = Size{width, height};
    frame.geometry.pixelFormat = PixelFormat::Bgra8888;
    frame.geometry.alphaMode = AlphaMode::Premultiplied;
    frame.geometry.planeCount = 1;
    frame.storage = makeStorage(width, height);
    frame.timing.completionTime = Clock::now();
    frame.timing.ptsSource = PtsSource::Completion;
    return frame;
}

// A frame plus the observable release counter of its storage: the counter outlives the frame on
// purpose, so a test can still read it after the frame has been moved into Core.
struct TrackedFrame
{
    RemoteFrame frame;
    std::shared_ptr<int> releases = std::make_shared<int>(0);
};

inline TrackedFrame makeTrackedFrame(std::uint32_t width = 8, std::uint32_t height = 4)
{
    TrackedFrame tracked;
    std::shared_ptr<int> counter = tracked.releases;
    tracked.frame.geometry.size = Size{width, height};
    tracked.frame.geometry.pixelFormat = PixelFormat::Bgra8888;
    tracked.frame.geometry.alphaMode = AlphaMode::Premultiplied;
    tracked.frame.geometry.planeCount = 1;
    tracked.frame.storage = makeStorage(width, height, [counter] { ++(*counter); });
    tracked.frame.timing.completionTime = Clock::now();
    tracked.frame.timing.ptsSource = PtsSource::Completion;
    return tracked;
}

inline void writePattern(const RemoteFrame &frame, std::uint8_t value)
{
    if (!frame.storage)
        return;
    // Producer-side write: legitimate before publication.
    auto *storage = static_cast<CpuFrameStorage *>(const_cast<FrameStorage *>(frame.storage.get()));
    if (std::byte *plane = storage->mutablePlane(0))
        plane[0] = static_cast<std::byte>(value);
}

inline std::uint8_t readPattern(const RemoteFrame &frame)
{
    if (!frame.storage)
        return 0;
    const std::optional<PlaneView> plane = frame.storage->mapRead(0);
    if (!plane.has_value() || plane->data == nullptr)
        return 0;
    return static_cast<std::uint8_t>(plane->data[0]);
}

// Closable gate that models a transport stalling inside enqueueFrame().
class EnqueueGate
{
public:
    void close()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_closed = true;
    }

    void open()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_closed = false;
        }
        m_cv.notify_all();
    }

    void enter()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        ++m_entered;
        m_cv.notify_all();
        m_cv.wait(lock, [this] { return !m_closed; });
    }

    std::size_t entered() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entered;
    }

    bool waitForEntered(std::size_t count,
                        std::chrono::milliseconds timeout = std::chrono::milliseconds(3000))
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_cv.wait_for(lock, timeout, [this, count] { return m_entered >= count; });
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_closed = false;
    std::size_t m_entered = 0;
};

// Reusable barrier: every participating thread blocks in wait() until `count` threads have
// arrived, so concurrent callers can be released at the same instant deterministically.
class TestBarrier
{
public:
    explicit TestBarrier(std::size_t count)
        : m_count(count)
    {
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        ++m_arrived;
        if (m_arrived == m_count) {
            m_released = true;
            lock.unlock();
            m_cv.notify_all();
            return;
        }
        m_cv.wait(lock, [this] { return m_released; });
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    const std::size_t m_count;
    std::size_t m_arrived = 0;
    bool m_released = false;
};

// ---------------------------------------------------------------------------
// CaptureSource
// ---------------------------------------------------------------------------

class FakeCaptureSource : public CaptureSource
{
public:
    FakeCaptureSource()
    {
        m_capabilities.asynchronous = true;
        m_capabilities.cpuReadable = true;
        m_capabilities.cpuFormats = {PixelFormat::Bgra8888};
    }

    ~FakeCaptureSource() override { ++(*destroyed); }

    void setCapabilities(CaptureCapabilities capabilities) { m_capabilities = std::move(capabilities); }

    CaptureCapabilities capabilities() const override
    {
        if (throwFromCapabilities)
            throw std::runtime_error("fake capture capabilities failure");
        return m_capabilities;
    }

    bool start(FrameReadyHandler onFrame, CaptureEventHandler onEvent) override
    {
        // A test can hold the startup here to keep the Session in `Starting` deterministically.
        if (startGate != nullptr)
            startGate->enter();

        if (throwFromStart)
            throw std::runtime_error("fake capture start failure");
        if (!startResult)
            return false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_onFrame = std::move(onFrame);
            m_onEvent = std::move(onEvent);
            m_started = true;
            m_outstanding = 0;
            m_maxOutstanding = 0;
        }

        if (onStart)
            onStart();
        return true;
    }

    void stop() noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_started = false;
            ++m_stopCalls;
        }

        if (onStop)
            onStop();
    }

    bool requestFrame(const CaptureRequest &request) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (throwFromRequestFrame)
            throw std::runtime_error("fake capture source failure");
        if (!m_started || m_rejectRequests)
            return false;

        ++m_outstanding;
        m_maxOutstanding = std::max(m_maxOutstanding, m_outstanding);
        m_requests.push_back(request);
        ++m_requestCalls;
        m_cv.notify_all();
        return true;
    }

    // ---- test controls -------------------------------------------------
    bool startResult = true;
    bool throwFromRequestFrame = false;
    bool throwFromCapabilities = false;
    bool throwFromStart = false;
    EnqueueGate *startGate = nullptr;

    // Set by the destructor: a test can prove that the Session never destroys a live component.
    std::shared_ptr<int> destroyed = std::make_shared<int>(0);

    // Optional hooks so a test can observe the Session state from inside start()/stop().
    std::function<void()> onStart;
    std::function<void()> onStop;

    void setRejectRequests(bool reject)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rejectRequests = reject;
    }

    std::vector<CaptureRequest> requests() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_requests;
    }

    std::size_t requestCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_requestCalls;
    }

    std::size_t maxOutstanding() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_maxOutstanding;
    }

    std::size_t outstanding() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_outstanding;
    }

    int stopCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stopCalls;
    }

    // Delivers a completion from the calling thread, which models a backend completion thread.
    //
    // Conforming: `stop()` quiesces callbacks, so the fake only calls back while it is started.
    bool deliver(RemoteFrame frame) { return deliverInternal(std::move(frame), false); }

    // Deliberately violates the quiescence rule, to prove that Core ignores late callbacks.
    bool forceDeliver(RemoteFrame frame) { return deliverInternal(std::move(frame), true); }

    void reportEvent(const CaptureEvent &event) { reportEventInternal(event, false); }
    void forceReportEvent(const CaptureEvent &event) { reportEventInternal(event, true); }

private:
    bool deliverInternal(RemoteFrame frame, bool force)
    {
        FrameReadyHandler handler;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!force && !m_started)
                return false;
            if (!m_onFrame)
                return false;
            if (m_outstanding > 0)
                --m_outstanding;
            handler = m_onFrame;
        }
        handler(std::move(frame));
        return true;
    }

    void reportEventInternal(const CaptureEvent &event, bool force)
    {
        CaptureEventHandler handler;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!force && !m_started)
                return;
            handler = m_onEvent;
        }
        if (handler)
            handler(event);
    }

private:
    mutable std::mutex m_mutex;
    CaptureCapabilities m_capabilities;
    FrameReadyHandler m_onFrame;
    CaptureEventHandler m_onEvent;
    std::vector<CaptureRequest> m_requests;
    bool m_started = false;
    bool m_rejectRequests = false;
    std::size_t m_outstanding = 0;
    std::size_t m_maxOutstanding = 0;
    std::size_t m_requestCalls = 0;
    int m_stopCalls = 0;
    std::condition_variable m_cv;
};

// ---------------------------------------------------------------------------
// Transport
// ---------------------------------------------------------------------------

class FakeTransport : public Transport
{
public:
    FakeTransport()
    {
        m_capabilities.acceptsCpu = true;
        m_capabilities.cpuFormats = {PixelFormat::Bgra8888};
    }

    ~FakeTransport() override { ++(*destroyed); }

    void setCapabilities(FrameConsumerCapabilities capabilities) { m_capabilities = std::move(capabilities); }

    FrameConsumerCapabilities frameCapabilities() const override
    {
        if (throwFromCapabilities)
            throw std::runtime_error("fake transport capabilities failure");
        return m_capabilities;
    }

    bool start(InputHandler onInput, TransportEventHandler onEvent) override
    {
        // A test can hold the transport startup to keep the Session in `Starting` deterministically.
        if (startGate != nullptr)
            startGate->enter();

        if (throwFromStart)
            throw std::runtime_error("fake transport start failure");
        if (!startResult)
            return false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_onInput = std::move(onInput);
            m_onEvent = std::move(onEvent);
            m_started = true;
            m_events.push_back("start");
        }

        if (onStart)
            onStart();
        return true;
    }

    void stop() noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ++m_stopCalls;
            m_started = false;
            m_events.push_back("stop");
        }

        if (onStop)
            onStop();
    }

    void enqueueFrame(RemoteFrame frame) override
    {
        if (throwFromEnqueue)
            throw std::runtime_error("fake transport failure");

        if (m_gate != nullptr)
            m_gate->enter();

        std::unique_lock<std::mutex> lock(m_mutex);
        ++m_enqueueCalls;
        m_enqueueThreads.insert(std::this_thread::get_id());
        m_events.push_back("enqueue");

        if (m_runtimeQueueCapacity > 0 && m_frames.size() >= m_runtimeQueueCapacity) {
            // A transport whose own runtime queue is full applies an explicit local policy
            // instead of blocking the Core dispatch worker.
            ++m_droppedByRuntimeQueue;
            return;
        }

        m_frames.push_back(std::move(frame));
        ++m_accepted;
        lock.unlock();
        m_cv.notify_all();
    }

    // ---- test controls -------------------------------------------------
    bool startResult = true;
    bool throwFromEnqueue = false;
    bool throwFromCapabilities = false;
    bool throwFromStart = false;
    EnqueueGate *startGate = nullptr;

    // Set by the destructor: a test can prove that the Session never destroys a live component.
    std::shared_ptr<int> destroyed = std::make_shared<int>(0);

    // Optional hooks so a test can observe the Session state from inside start()/stop().
    std::function<void()> onStart;
    std::function<void()> onStop;

    void setRuntimeQueueCapacity(std::size_t capacity)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_runtimeQueueCapacity = capacity;
    }

    void setGate(EnqueueGate *gate) { m_gate = gate; }

    std::size_t accepted() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_accepted;
    }

    std::size_t enqueueCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_enqueueCalls;
    }

    std::size_t droppedByRuntimeQueue() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_droppedByRuntimeQueue;
    }

    int stopCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stopCalls;
    }

    std::vector<RemoteFrame> frames() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_frames;
    }

    void clearFrames()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_frames.clear();
    }

    std::set<std::thread::id> enqueueThreads() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_enqueueThreads;
    }

    std::vector<std::string> events() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_events;
    }

    bool waitForAccepted(std::size_t count,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds(3000))
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_cv.wait_for(lock, timeout, [this, count] { return m_accepted >= count; });
    }

    // Models the transport runtime thread handing remote input to Core. Conforming: the runtime
    // stops delivering once it was stopped.
    void deliverInput(const InputEvent &event) { deliverInputInternal(event, false); }

    // Deliberately violates the quiescence rule, to prove that Core ignores late callbacks.
    void forceDeliverInput(const InputEvent &event) { deliverInputInternal(event, true); }

    void reportEvent(const TransportEvent &event) { reportEventInternal(event, false); }
    void forceReportEvent(const TransportEvent &event) { reportEventInternal(event, true); }

private:
    void deliverInputInternal(const InputEvent &event, bool force)
    {
        InputHandler handler;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!force && !m_started)
                return;
            handler = m_onInput;
        }
        if (handler)
            handler(event);
    }

    void reportEventInternal(const TransportEvent &event, bool force)
    {
        TransportEventHandler handler;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!force && !m_started)
                return;
            handler = m_onEvent;
        }
        if (handler)
            handler(event);
    }

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    FrameConsumerCapabilities m_capabilities;
    InputHandler m_onInput;
    TransportEventHandler m_onEvent;
    std::vector<RemoteFrame> m_frames;
    std::vector<std::string> m_events;
    std::set<std::thread::id> m_enqueueThreads;
    EnqueueGate *m_gate = nullptr;
    std::size_t m_runtimeQueueCapacity = 0;
    std::size_t m_accepted = 0;
    std::size_t m_enqueueCalls = 0;
    std::size_t m_droppedByRuntimeQueue = 0;
    int m_stopCalls = 0;
    bool m_started = false;
};

// Test-only forwarding adapters.
//
// They let a test keep ownership of the backend, so the backend can outlive the Session. That is
// exactly the scenario in which a late callback used to be able to dereference freed Core state.
class LoaningCaptureSource : public CaptureSource
{
public:
    explicit LoaningCaptureSource(FakeCaptureSource *backend)
        : m_backend(backend)
    {
    }

    CaptureCapabilities capabilities() const override { return m_backend->capabilities(); }
    bool start(FrameReadyHandler onFrame, CaptureEventHandler onEvent) override
    {
        return m_backend->start(std::move(onFrame), std::move(onEvent));
    }
    void stop() noexcept override { m_backend->stop(); }
    bool requestFrame(const CaptureRequest &request) override { return m_backend->requestFrame(request); }

private:
    FakeCaptureSource *m_backend;
};

class LoaningTransport : public Transport
{
public:
    explicit LoaningTransport(FakeTransport *backend)
        : m_backend(backend)
    {
    }

    FrameConsumerCapabilities frameCapabilities() const override
    {
        return m_backend->frameCapabilities();
    }
    bool start(InputHandler onInput, TransportEventHandler onEvent) override
    {
        return m_backend->start(std::move(onInput), std::move(onEvent));
    }
    void stop() noexcept override { m_backend->stop(); }
    void enqueueFrame(RemoteFrame frame) override { m_backend->enqueueFrame(std::move(frame)); }

private:
    FakeTransport *m_backend;
};

// ---------------------------------------------------------------------------
// InputSink
// ---------------------------------------------------------------------------

class FakeInputSink : public InputSink
{
public:
    ~FakeInputSink() override { ++(*destroyed); }

    bool throwFromPost = false;

    // Set by the destructor, so a test can observe runtime sink replacement.
    std::shared_ptr<int> destroyed = std::make_shared<int>(0);

    void post(const InputEvent &event) override
    {
        if (throwFromPost)
            throw std::runtime_error("fake input sink failure");

        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(event);
        m_threads.insert(std::this_thread::get_id());
        m_cv.notify_all();
    }

    std::size_t count() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_events.size();
    }

    std::vector<InputEvent> events() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_events;
    }

    std::set<std::thread::id> threads() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_threads;
    }

    bool waitForCount(std::size_t count,
                      std::chrono::milliseconds timeout = std::chrono::milliseconds(3000))
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_cv.wait_for(lock, timeout, [this, count] { return m_events.size() >= count; });
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<InputEvent> m_events;
    std::set<std::thread::id> m_threads;
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

// Owns a Session wired to a fake source and a fake transport, and stops it on destruction.
struct RunningSession
{
    FakeCaptureSource *source = nullptr;
    FakeTransport *transport = nullptr;
    std::unique_ptr<Session> session;

    RunningSession() = default;
    RunningSession(RunningSession &&) = default;
    RunningSession &operator=(RunningSession &&) = default;
    RunningSession(const RunningSession &) = delete;
    RunningSession &operator=(const RunningSession &) = delete;

    ~RunningSession() { stop(); }

    // Creates the Session and its fakes without starting anything, so a test can assert the
    // Stopped state and configuration errors before start().
    void prepare(SessionConfig config = {})
    {
        auto sourceOwner = std::make_unique<FakeCaptureSource>();
        auto transportOwner = std::make_unique<FakeTransport>();
        source = sourceOwner.get();
        transport = transportOwner.get();
        session = std::make_unique<Session>(config);
        session->setCaptureSource(std::move(sourceOwner));
        session->setTransport(std::move(transportOwner));
    }

    void setInputSink(std::shared_ptr<InputSink> sink) { session->setInputSink(std::move(sink)); }

    bool start(SessionConfig config = {})
    {
        if (!session)
            prepare(config);
        return session->start();
    }

    void stop()
    {
        if (session)
            session->stop();
    }
};

}  // namespace hyremote::test

  