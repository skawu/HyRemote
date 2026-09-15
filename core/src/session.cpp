#include "hyremote/core/session.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

#include "detail/mailbox.hpp"

namespace hyremote {

std::string validateSessionConfig(const SessionConfig &config)
{
    if (config.frameQueue.capacity == 0)
        return "frameQueue.capacity must be at least 1 (the dispatch-worker-owned frame is in "
               "addition to the queue)";

    if (config.capture.maxInFlight == 0)
        return "capture.maxInFlight must be at least 1";

    if (config.capture.targetFramesPerSecond.has_value()
        && !(*config.capture.targetFramesPerSecond > 0.0))
        return "capture.targetFramesPerSecond must be positive when set";

    if (config.capture.rejectedRequestRetryDelay.count() < 0)
        return "capture.rejectedRequestRetryDelay must not be negative";

    return {};
}

std::string_view sessionStateName(SessionState state)
{
    switch (state) {
    case SessionState::Stopped:
        return "Stopped";
    case SessionState::Starting:
        return "Starting";
    case SessionState::Running:
        return "Running";
    case SessionState::Stopping:
        return "Stopping";
    case SessionState::Faulted:
        return "Faulted";
    }
    return "Stopped";
}

std::string_view sessionErrorCodeName(SessionErrorCode code)
{
    switch (code) {
    case SessionErrorCode::InvalidConfiguration:
        return "InvalidConfiguration";
    case SessionErrorCode::IncompatibleFrameCapabilities:
        return "IncompatibleFrameCapabilities";
    case SessionErrorCode::CaptureStartFailed:
        return "CaptureStartFailed";
    case SessionErrorCode::TransportStartFailed:
        return "TransportStartFailed";
    case SessionErrorCode::TargetLost:
        return "TargetLost";
    case SessionErrorCode::ComponentFailure:
        return "ComponentFailure";
    }
    return "ComponentFailure";
}

// Implementation type of the Session. The helper routines that the worker threads and the
// capture/transport callbacks run are static members so that they can stay file-local without
// exposing the type in the public header.
struct Session::Impl
{
    SessionConfig config;
    std::unique_ptr<CaptureSource> source;
    std::unique_ptr<Transport> transport;
    std::shared_ptr<InputSink> sink;

    // Guards state, in-flight accounting, counters and stop coordination.
    //
    // Lock order: this mutex may be held while calling into the mailbox (which has its own
    // mutex); the mailbox never calls back into the Session while holding its mutex.
    mutable std::mutex mutex;
    std::condition_variable cv;

    SessionState state = SessionState::Stopped;
    std::optional<SessionError> lastError;
    bool stopRequested = false;
    bool sourceStarted = false;
    bool transportStarted = false;
    std::size_t inFlight = 0;
    CaptureRequestId nextRequestId = 1;
    SessionStats stats;
    std::unique_ptr<detail::Mailbox> mailbox;
    std::thread schedulerThread;
    std::thread dispatcherThread;
    std::optional<TimePoint> nextIssueTime;  // pacing bookkeeping

    static void setFaultedLocked(Impl &impl, SessionErrorCode code, const std::string &message)
    {
        impl.state = SessionState::Faulted;
        impl.lastError = SessionError{code, message, false};
    }

    // Admission control, evaluated with `mutex` held.
    //
    // DropOldest deliberately ignores the mailbox depth: capture stays decoupled from the
    // transport and overload is absorbed by dropping the oldest waiting frame.
    //
    // ProducerThrottle also counts the requests still in flight, because a completion that was
    // already granted cannot be un-issued: only `waiting + inFlight < capacity` guarantees that
    // no overflow is possible (the same accounting rule PR #19 round 2 established for the
    // spike's consumer queue).
    static bool admissionAllowsLocked(const Impl &impl)
    {
        if (impl.inFlight >= impl.config.capture.maxInFlight)
            return false;

        if (impl.config.frameQueue.policy != BackpressurePolicy::ProducerThrottle)
            return true;

        if (!impl.mailbox)
            return true;

        const std::size_t waiting = impl.mailbox->stats().waiting;
        return waiting + impl.inFlight < impl.config.frameQueue.capacity;
    }

    // Capture callback path. May run on a backend-defined thread; performs bounded
    // bookkeeping/enqueue work only and never touches the transport.
    static void onFrameReady(Impl &impl, RemoteFrame frame)
    {
        const FrameValidationResult validation = validateFrame(frame);

        std::string timingReason;
        bool timingOk = false;
        if (validation.ok)
            timingOk = normalizeFrameTiming(frame, &timingReason);

        {
            std::lock_guard<std::mutex> lock(impl.mutex);

            // Every completion releases its in-flight slot, valid or not.
            if (impl.inFlight > 0)
                --impl.inFlight;
            impl.stats.inFlight = impl.inFlight;

            if (impl.state != SessionState::Running || !impl.mailbox) {
                ++impl.stats.framesArrivedAfterStop;
            } else if (!validation.ok) {
                ++impl.stats.framesRejectedInvalid;
                impl.stats.lastFrameRejection = validation.reason;
            } else if (!timingOk) {
                ++impl.stats.framesRejectedTiming;
                impl.stats.lastFrameRejection = timingReason;
            } else {
                switch (impl.mailbox->push(std::move(frame))) {
                case detail::PushResult::Stored:
                case detail::PushResult::StoredAfterDroppingOldest:
                    ++impl.stats.framesAccepted;
                    break;
                case detail::PushResult::RejectedOverflow:
                    ++impl.stats.framesRejectedOverflow;
                    impl.stats.lastFrameRejection =
                        "ProducerThrottle mailbox capacity reached: the capture backend produced "
                        "more frames than the admitted in-flight bound allowed";
                    break;
                case detail::PushResult::RejectedClosed:
                    ++impl.stats.framesArrivedAfterStop;
                    break;
                }
            }
        }

        // Wake the scheduler (an in-flight slot may have freed) and the dispatch worker.
        impl.cv.notify_all();
    }

    static void onCaptureEvent(Impl &impl, const CaptureEvent &event)
    {
        {
            std::lock_guard<std::mutex> lock(impl.mutex);
            ++impl.stats.captureEvents;
            if (!event.recoverable) {
                ++impl.stats.captureEventsNonRecoverable;
                if (impl.state == SessionState::Running) {
                    const SessionErrorCode code = event.code == CaptureEventCode::TargetLost
                                                      ? SessionErrorCode::TargetLost
                                                      : SessionErrorCode::ComponentFailure;
                    setFaultedLocked(impl, code, event.message);
                }
            }
            // A recoverable capture event (hidden target, single rejected request) never faults
            // the Session; it stays observable through the counters.
        }
        impl.cv.notify_all();
    }

    static void onTransportEvent(Impl &impl, const TransportEvent &event)
    {
        {
            std::lock_guard<std::mutex> lock(impl.mutex);
            ++impl.stats.transportEvents;
            if (event.code == TransportEventCode::FatalFailure) {
                ++impl.stats.transportEventsFatal;
                if (impl.state == SessionState::Running)
                    setFaultedLocked(impl, SessionErrorCode::ComponentFailure, event.message);
            }
            // Client disconnects, authentication rejections and recoverable failures leave the
            // Session Running: they are transport-scoped, not session-scoped.
        }
        impl.cv.notify_all();
    }

    static void onInput(Impl &impl, const InputEvent &event)
    {
        std::shared_ptr<InputSink> sink;
        {
            std::lock_guard<std::mutex> lock(impl.mutex);
            if (impl.state != SessionState::Running) {
                ++impl.stats.inputEventsDropped;
                return;
            }
            sink = impl.sink;
            if (sink)
                ++impl.stats.inputEventsPosted;
            else
                ++impl.stats.inputEventsDropped;
        }

        // Called without any Core lock held: the sink marshals to the thread the target requires
        // and must not wait for synchronous GUI execution.
        if (sink)
            sink->post(event);
    }

    // Owns capture admission and request issuing.
    static void runScheduler(Impl &impl)
    {
        const bool paced = impl.config.capture.targetFramesPerSecond.has_value();
        const double fps = paced ? *impl.config.capture.targetFramesPerSecond : 0.0;
        const Clock::duration period =
            paced ? std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / fps))
                  : Clock::duration::zero();

        for (;;) {
            std::unique_lock<std::mutex> lock(impl.mutex);
            if (impl.stopRequested || impl.state != SessionState::Running)
                return;

            if (paced) {
                const TimePoint now = Clock::now();
                if (!impl.nextIssueTime.has_value() || *impl.nextIssueTime < now)
                    impl.nextIssueTime = now;
                if (now < *impl.nextIssueTime) {
                    impl.cv.wait_until(lock, *impl.nextIssueTime, [&impl] {
                        return impl.stopRequested || impl.state != SessionState::Running;
                    });
                    continue;
                }
            }

            if (!admissionAllowsLocked(impl)) {
                impl.cv.wait(lock, [&impl] {
                    return impl.stopRequested || impl.state != SessionState::Running
                           || admissionAllowsLocked(impl);
                });
                continue;
            }

            CaptureRequest request;
            request.id = impl.nextRequestId++;
            request.requestTime = Clock::now();

            ++impl.inFlight;
            ++impl.stats.captureRequestsIssued;
            impl.stats.inFlight = impl.inFlight;
            impl.stats.maxInFlightObserved = std::max(impl.stats.maxInFlightObserved, impl.inFlight);
            if (paced)
                impl.nextIssueTime = request.requestTime + period;

            CaptureSource *source = impl.source.get();
            lock.unlock();

            bool accepted = false;
            bool threw = false;
            try {
                accepted = source->requestFrame(request);
            } catch (...) {
                // A backend bug must not terminate the host process from a Core-owned thread.
                threw = true;
            }

            if (accepted)
                continue;

            // The backend refused the request before scheduling any work: release the slot, count
            // it and retry after a bounded delay so a permanently unavailable target cannot spin.
            std::unique_lock<std::mutex> relock(impl.mutex);
            if (impl.inFlight > 0)
                --impl.inFlight;
            impl.stats.inFlight = impl.inFlight;
            ++impl.stats.captureRequestsRejected;
            if (threw) {
                setFaultedLocked(impl, SessionErrorCode::ComponentFailure,
                                 "capture source threw from requestFrame()");
            }
            impl.cv.wait_for(relock, impl.config.capture.rejectedRequestRetryDelay, [&impl] {
                return impl.stopRequested || impl.state != SessionState::Running;
            });
        }
    }

    // Ordered cleanup of a partially started run.
    //
    // Used when start() fails after some components already started. It stops whatever started and
    // leaves the Session in `Faulted` (the observable startup failure), so the owner can call
    // stop() for the `Stopped` state exactly like the ADR-0003 sequence describes:
    // Starting -> Faulted -> Stopping -> Stopped.
    static void abortStartup(Impl &impl, SessionErrorCode code, const std::string &message)
    {
        CaptureSource *source = nullptr;
        {
            std::lock_guard<std::mutex> lock(impl.mutex);
            impl.stopRequested = true;
            source = impl.sourceStarted ? impl.source.get() : nullptr;
            impl.transportStarted = false;
        }
        impl.cv.notify_all();

        if (source != nullptr) {
            try {
                source->stop();
            } catch (...) {
                // Cleanup must not be prevented by a misbehaving adapter.
            }
        }

        {
            std::lock_guard<std::mutex> lock(impl.mutex);
            impl.sourceStarted = false;
            setFaultedLocked(impl, code, message);
        }
        impl.cv.notify_all();
    }

    // Owns the transport hand-off. Runs on its own thread so the capture callback path never
    // touches the transport.
    static void runDispatcher(Impl &impl)
    {
        detail::Mailbox &mailbox = *impl.mailbox;
        Transport *transport = impl.transport.get();

        for (;;) {
            RemoteFrame frame;
            const detail::PopResult result = mailbox.tryPop(&frame);

            if (result == detail::PopResult::Frame) {
                // No Core lock is held across the transport call: the hand-off is a bounded
                // post/enqueue by contract, so a slow transport cannot block capture bookkeeping.
                bool threw = false;
                try {
                    transport->enqueueFrame(std::move(frame));
                } catch (...) {
                    // A transport that throws violates its contract; it must not terminate the
                    // host process, and the failure must be visible rather than silent.
                    threw = true;
                }
                mailbox.releaseDispatcherOwnership();
                {
                    std::lock_guard<std::mutex> lock(impl.mutex);
                    ++impl.stats.framesDispatched;
                    if (threw) {
                        ++impl.stats.transportEnqueueFailures;
                        setFaultedLocked(impl, SessionErrorCode::ComponentFailure,
                                         "transport threw from enqueueFrame()");
                    }
                }
                impl.cv.notify_all();
                continue;
            }

            if (result == detail::PopResult::Closed)
                return;  // closed and drained: deterministic shutdown

            mailbox.waitForWork();
        }
    }
};

Session::Session(SessionConfig config)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->config = config;
}

Session::~Session()
{
    stop();
}

void Session::setCaptureSource(std::unique_ptr<CaptureSource> source)
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->source = std::move(source);
}

void Session::setTransport(std::unique_ptr<Transport> transport)
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->transport = std::move(transport);
}

void Session::setInputSink(std::shared_ptr<InputSink> sink)
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->sink = std::move(sink);
}

bool Session::start()
{
    Impl &impl = *m_impl;

    const std::string configProblem = validateSessionConfig(impl.config);
    if (!configProblem.empty()) {
        std::lock_guard<std::mutex> lock(impl.mutex);
        impl.lastError = SessionError{SessionErrorCode::InvalidConfiguration, configProblem, false};
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        if (impl.state != SessionState::Stopped) {
            impl.lastError = SessionError{SessionErrorCode::InvalidConfiguration,
                                          std::string("Session::start() requires the Stopped state "
                                                      "(current: ")
                                              + std::string(sessionStateName(impl.state))
                                              + "); call stop() first",
                                          false};
            return false;
        }
        if (!impl.source) {
            impl.lastError = SessionError{SessionErrorCode::InvalidConfiguration,
                                          "no capture source installed", false};
            return false;
        }
        if (!impl.transport) {
            impl.lastError =
                SessionError{SessionErrorCode::InvalidConfiguration, "no transport installed", false};
            return false;
        }

        const CompatibilityResult compatibility =
            checkFrameCompatibility(impl.source->capabilities(), impl.transport->frameCapabilities());
        if (!compatibility.compatible) {
            // A capability mismatch is a configuration error: it fails before `Running`.
            impl.lastError = SessionError{SessionErrorCode::IncompatibleFrameCapabilities,
                                          compatibility.reason, false};
            return false;
        }

        // Reset per-run observability so repeated start/stop cycles stay comparable.
        impl.stats = SessionStats{};
        impl.inFlight = 0;
        impl.nextRequestId = 1;
        impl.stopRequested = false;
        impl.lastError.reset();
        impl.nextIssueTime.reset();
        impl.state = SessionState::Starting;
    }
    impl.cv.notify_all();

    CaptureSource *source = impl.source.get();
    Transport *transport = impl.transport.get();

    const bool sourceStarted =
        source->start([&impl](RemoteFrame frame) { Impl::onFrameReady(impl, std::move(frame)); },
                      [&impl](const CaptureEvent &event) { Impl::onCaptureEvent(impl, event); });
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        impl.sourceStarted = sourceStarted;
    }
    if (!sourceStarted) {
        Impl::abortStartup(impl, SessionErrorCode::CaptureStartFailed,
                           "capture source failed to start");
        return false;
    }

    const bool transportStarted =
        transport->start([&impl](const InputEvent &event) { Impl::onInput(impl, event); },
                         [&impl](const TransportEvent &event) { Impl::onTransportEvent(impl, event); });
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        impl.transportStarted = transportStarted;
    }
    if (!transportStarted) {
        // The capture source that did start is stopped here; the Session stays Faulted so the
        // startup failure is observable until the owner calls stop().
        Impl::abortStartup(impl, SessionErrorCode::TransportStartFailed,
                           "transport failed to start");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        impl.mailbox = std::make_unique<detail::Mailbox>(impl.config.frameQueue.capacity,
                                                         impl.config.frameQueue.policy);
    }

    impl.schedulerThread = std::thread([&impl] { Impl::runScheduler(impl); });
    impl.dispatcherThread = std::thread([&impl] { Impl::runDispatcher(impl); });

    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        impl.state = SessionState::Running;
    }
    impl.cv.notify_all();
    return true;
}

void Session::stop() noexcept
{
    Impl &impl = *m_impl;

    CaptureSource *source = nullptr;
    Transport *transport = nullptr;
    detail::Mailbox *mailbox = nullptr;
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        if (impl.state == SessionState::Stopped)
            return;

        impl.state = SessionState::Stopping;
        impl.stopRequested = true;
        source = impl.sourceStarted ? impl.source.get() : nullptr;
        transport = impl.transportStarted ? impl.transport.get() : nullptr;
        mailbox = impl.mailbox.get();
    }
    impl.cv.notify_all();

    // 1./2. Stop scheduling and ask the capture source to stop.
    if (source != nullptr) {
        try {
            source->stop();
        } catch (...) {
            // stop() is noexcept: a misbehaving adapter must not prevent local cleanup.
        }
    }

    // 3. Close the mailbox; the dispatch worker drains the frames Core already accepted.
    if (mailbox != nullptr)
        mailbox->close();

    // 4. Join the workers. This terminates because the transport hand-off is bounded by contract
    // (ADR-0003), so the dispatch worker can never be stuck waiting on a remote peer.
    if (impl.schedulerThread.joinable())
        impl.schedulerThread.join();
    if (impl.dispatcherThread.joinable())
        impl.dispatcherThread.join();

    // 5. Stop the transport runtime.
    if (transport != nullptr) {
        try {
            transport->stop();
        } catch (...) {
            // As above: cleanup continues.
        }
    }

    // 6. Release the run-scoped state.
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        impl.mailbox.reset();
        impl.sourceStarted = false;
        impl.transportStarted = false;
        impl.inFlight = 0;
        impl.stats.inFlight = 0;
        impl.state = SessionState::Stopped;
    }
    impl.cv.notify_all();
}

SessionState Session::state() const
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->state;
}

std::optional<SessionError> Session::lastError() const
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->lastError;
}

SessionStats Session::stats() const
{
    const Impl &impl = *m_impl;
    std::lock_guard<std::mutex> lock(impl.mutex);

    SessionStats snapshot = impl.stats;
    snapshot.inFlight = impl.inFlight;

    if (impl.mailbox) {
        const detail::MailboxStats mailbox = impl.mailbox->stats();
        snapshot.mailboxWaiting = mailbox.waiting;
        snapshot.mailboxDispatcherOwned = mailbox.dispatcherOwned;
        snapshot.maxMailboxWaitingObserved = mailbox.maxWaitingObserved;
        snapshot.maxMailboxOwnedObserved = mailbox.maxOwnedObserved;
        snapshot.framesDroppedByPolicy = mailbox.droppedOldest;
        snapshot.framesRejectedOverflow = mailbox.rejectedOverflow;
        snapshot.lastFrameId = mailbox.lastFrameId;
    }

    return snapshot;
}

SessionConfig Session::config() const
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->config;
}

}  // namespace hyremote
