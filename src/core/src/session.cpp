#include "hyremote/core/session.hpp"

#include <algorithm>
#include <atomic>
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
    case SessionErrorCode::StartCancelled:
        return "StartCancelled";
    }
    return "ComponentFailure";
}

namespace {

// Stops a component without letting an adapter failure interfere with cleanup. Component `stop()`
// is `noexcept` in the interface; the guard is defensive.
template <typename Component>
void stopComponentQuietly(Component *component) noexcept
{
    if (component == nullptr)
        return;

    try {
        component->stop();
    } catch (...) {
        // Cleanup continues.
    }
}

}  // namespace

// Core-owned callback gate.
//
// The callbacks installed into adapters hold a shared reference to this gate instead of a raw
// Session pointer. That is what makes `~Session()` safe even when an adapter violates the
// quiescence rule of `CaptureSource::stop()` / `Transport::stop()`: teardown closes the gate,
// drains the callbacks already inside Core, and only then invalidates the target, so a late
// callback finds `target == nullptr` and is ignored instead of touching freed memory.
struct CallbackGate
{
    std::mutex mutex;
    std::condition_variable cv;
    void *target = nullptr;  // guarded; the Session implementation while the run is alive
    std::size_t active = 0;  // guarded; callbacks currently inside Core
    bool closed = false;     // guarded
    std::atomic<std::uint64_t> ignored{0};

    // Enters the gate. Returns false when the run is over and the callback must be ignored.
    bool enter(void **targetOut)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (closed || target == nullptr)
            return false;

        ++active;
        *targetOut = target;
        return true;
    }

    void leave()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (active > 0)
            --active;
        if (active == 0)
            cv.notify_all();
    }

    // Closes the gate, waits for in-flight callbacks and invalidates the target. After this returns,
    // no adapter callback can reach the Session implementation any more.
    void closeAndDrain()
    {
        std::unique_lock<std::mutex> lock(mutex);
        closed = true;
        cv.wait(lock, [this] { return active == 0; });
        target = nullptr;
    }
};

// RAII helper used inside the callbacks Core installs into adapters.
class GateGuard
{
public:
    explicit GateGuard(std::shared_ptr<CallbackGate> gate)
        : m_gate(std::move(gate))
    {
    }

    GateGuard(const GateGuard &) = delete;
    GateGuard &operator=(const GateGuard &) = delete;

    ~GateGuard()
    {
        if (m_entered && m_gate)
            m_gate->leave();
    }

    // Returns the Session implementation, or nullptr when the callback must be ignored. Ignored
    // callbacks are counted on the gate, which outlives the implementation object.
    void *enter()
    {
        if (!m_gate)
            return nullptr;

        void *target = nullptr;
        m_entered = m_gate->enter(&target);
        if (!m_entered) {
            m_gate->ignored.fetch_add(1, std::memory_order_relaxed);
            return nullptr;
        }
        return target;
    }

private:
    std::shared_ptr<CallbackGate> m_gate;
    bool m_entered = false;
};

// Implementation type of the Session. The helper routines that the worker threads and the
// capture/transport callbacks run are static members so that they can stay file-local without
// exposing the type in the public header.
struct Session::Impl
{
    SessionConfig config;
    // Shared ownership so that start(), the workers and teardown can hold a strong pin across their
    // unlocked component calls. A concurrent stop() may return the Session to Stopped (which re-allows the
    // public setters) while an in-flight start() is still calling into these objects; with unique ownership
    // that setter destroyed the object the other thread was using. The public setter signatures stay
    // unchanged - an incoming unique_ptr is converted here (#159).
    std::shared_ptr<CaptureSource> source;
    std::shared_ptr<Transport> transport;
    std::shared_ptr<InputSink> sink;

    // Guards state, in-flight accounting, counters and stop coordination.
    //
    // Lock order: this mutex may be held while calling into the mailbox (which has its own
    // mutex); the mailbox never calls back into the Session while holding its mutex.
    mutable std::mutex mutex;
    std::condition_variable cv;

    // Callback lifetime gate of the current run. Created by start(), closed by stop()/failRun().
    std::shared_ptr<CallbackGate> gate;

    SessionState state = SessionState::Stopped;
    std::optional<SessionError> lastError;
    bool stopRequested = false;

    // Adapter change notification; see Session::setChangeCallback(). Called while `mutex` is held, so it is handed
    // the values instead of being allowed to query them - state() and lastError() take the same non-recursive lock.
    std::function<void(Session::Change, SessionState, std::optional<SessionError>)> changeCallback;

    void notifyLocked(Session::Change change)
    {
        if (changeCallback)
            changeCallback(change, state, lastError);
    }

    // Every asynchronous change funnels through here: a component failing on its own thread, a capture target
    // disappearing, a transport event. Owner-driven transitions (start/stop) deliberately do not notify - the caller
    // is already inside the call that caused them - so this stays small.
    void notifyStateLocked()
    {
        notifyLocked(Session::Change::State);
    }

    void notifyDiagnosticLocked()
    {
        notifyLocked(Session::Change::Diagnostic);
    }

    // Lifecycle ownership (all guarded by `mutex`).
    //
    // `teardownStarted` is the single teardown claim of a run: it is acquired *while holding the
    // mutex* (never in a check-then-release-then-set sequence), so exactly one stop() caller or
    // startup-failure path can own the component stop/join sequence.
    //
    // `runGeneration` identifies the run that start() is establishing. Every claim (start() run
    // claim, teardown claim) increments it, so an in-progress start() can tell at its next boundary
    // that its run was cancelled by a concurrent stop().
    bool teardownStarted = false;
    std::uint64_t runGeneration = 0;

    // Resources of the current run, each owned by exactly one side: the flag is cleared under the
    // mutex by whoever takes over the cleanup, so a component is stopped/joined exactly once.
    bool sourceStarted = false;
    bool transportStarted = false;
    bool workersCreated = false;
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
        // A fault is exactly the transition an adapter cannot infer by driving the Session itself: it arrives from a
        // worker thread, a transport callback or a capture callback, so it is notified from here.
        impl.notifyStateLocked();
        impl.notifyDiagnosticLocked();
    }

    // Worker startup handshake.
    //
    // The Core workers are created while the Session is still `Starting`, so "not Running yet" must
    // not be interpreted as "exit": that race could leave a Session that reports `Running` but has
    // no scheduler left to issue requests. The worker waits here until `start()` publishes
    // `Running`, or until the run fails/stops, and returns whether it may proceed.
    static bool waitForRunning(Impl &impl, std::unique_lock<std::mutex> &lock)
    {
        impl.cv.wait(lock, [&impl] {
            return impl.state != SessionState::Starting || impl.stopRequested;
        });
        return impl.state == SessionState::Running && !impl.stopRequested;
    }

    // Variant for the dispatch worker: it must wait for the handshake but must never decide to exit
    // here, because the frames Core already accepted still have to be drained out of a closed
    // mailbox even when the run never reached `Running`.
    static void awaitStartup(Impl &impl, std::unique_lock<std::mutex> &lock)
    {
        impl.cv.wait(lock, [&impl] {
            return impl.state != SessionState::Starting || impl.stopRequested;
        });
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

    // Callback boundary for the three backend-invoked callbacks below. The backend calls them from its
    // own stack - a capture thread, a transport runtime - so an exception allowed to escape would unwind
    // into code HyRemote does not own, and a backend that is not exception-aware would terminate the
    // process. The realistic source is allocation inside the bounded bookkeeping below (a mailbox push, a
    // rejection reason string), so the handler itself must not allocate: the failure may well have been
    // bad_alloc, and formatting a message here would throw out of a noexcept boundary.
    //
    // The failure is reported through the existing error model, following the established input path (see
    // onInput): it is recoverable, because a single lost frame does not invalidate the session, and the
    // reporting is best effort because the alternative is terminating the process in the caller's stack.
    static void reportCallbackFailure(Impl &impl, const char *message) noexcept
    {
        try {
            std::lock_guard<std::mutex> lock(impl.mutex);
            impl.lastError = SessionError{SessionErrorCode::ComponentFailure, message, true};
            impl.notifyDiagnosticLocked();
        } catch (...) {
            // Best effort by design; see above.
        }
        impl.cv.notify_all();
    }

    template <typename Callback>
    static void atCallbackBoundary(Impl &impl, Callback &&callback) noexcept
    {
        try {
            std::forward<Callback>(callback)();
        } catch (...) {
            reportCallbackFailure(impl, "core callback threw from a backend invocation");
        }
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
            if (!sink) {
                ++impl.stats.inputEventsDropped;
                return;
            }
        }

        // Called without any Core lock held: the sink marshals to the thread the target requires
        // and must not wait for synchronous GUI execution.
        try {
            sink->post(event);
        } catch (...) {
            // The exception must not unwind through the transport runtime. Remote input is not on
            // the capture path, so this is reported as a recoverable error instead of faulting the
            // Session: frame delivery keeps working and the failure stays observable.
            std::lock_guard<std::mutex> lock(impl.mutex);
            ++impl.stats.inputPostFailures;
            impl.lastError = SessionError{SessionErrorCode::ComponentFailure,
                                          "input sink threw from post()", true};
            impl.notifyDiagnosticLocked();
            return;
        }

        std::lock_guard<std::mutex> lock(impl.mutex);
        ++impl.stats.inputEventsPosted;
    }

    // Worker threads are std::thread entry points, so an exception escaping one terminates the process -
    // forbidden for an avoidable internal allocation or formatting failure (#164 criterion 2). The guard sits
    // at the launch site so it covers the whole thread function: the startup handshake, every bookkeeping step
    // and the transport call, not only the piece that already had a local catch.
    //
    // A worker that dies cannot make progress, because nothing drains the mailbox, so the failure is reported
    // as a non-recoverable ComponentFailure and the session faults instead of staying `Running` with a dead
    // worker. Returning normally is also what lets the owning thread join and a later stop() finish rather than
    // block forever.
    static void atWorkerBoundary(Impl &impl, void (*worker)(Impl &), const char *label) noexcept
    {
        try {
            worker(impl);
        } catch (...) {
            reportWorkerFailure(impl, label);
        }
    }

    static void reportWorkerFailure(Impl &impl, const char *label) noexcept
    {
        try {
            std::lock_guard<std::mutex> lock(impl.mutex);
            setFaultedLocked(impl, SessionErrorCode::ComponentFailure, label);
        } catch (...) {
            // Best effort by design: the alternative is terminating the process, which is exactly what this
            // boundary exists to prevent. The label is a static string, so only the guarded scope allocates.
        }
        impl.cv.notify_all();
    }

    // Owns capture admission and request issuing.
    static void runScheduler(Impl &impl)
    {
        {
            // Observable "the worker thread actually began", before the handshake.
            std::lock_guard<std::mutex> lock(impl.mutex);
            ++impl.stats.workersStarted;
        }
        impl.cv.notify_all();

        const bool paced = impl.config.capture.targetFramesPerSecond.has_value();
        const double fps = paced ? *impl.config.capture.targetFramesPerSecond : 0.0;
        const Clock::duration period =
            paced ? std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / fps))
                  : Clock::duration::zero();

        // Startup handshake: created during `Starting`, may only work once `Running` is published.
        {
            std::unique_lock<std::mutex> lock(impl.mutex);
            if (!waitForRunning(impl, lock))
                return;
        }

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

    // --- lifecycle ownership ---------------------------------------------------------------

    // Claims the teardown of the current run atomically. Must be called with `mutex` held, and only
    // when the caller has established that its run is still current: taking the claim also
    // invalidates the run generation, so an in-progress start() stops committing to that run.
    static void claimTeardownLocked(Impl &impl)
    {
        impl.teardownStarted = true;
        ++impl.runGeneration;
    }

    // Blocking claim used by stop(): waits for an in-flight teardown of this run to finish and then
    // takes the claim atomically. It never returns while another caller owns the teardown, and it
    // never uses a check-then-release-then-set sequence.
    static void claimTeardown(Impl &impl)
    {
        std::unique_lock<std::mutex> lock(impl.mutex);
        impl.cv.wait(lock, [&impl] { return !impl.teardownStarted; });
        claimTeardownLocked(impl);
    }

    // True while the run the calling start() is establishing is still the current one and may
    // commit its next step.
    static bool runStillValidLocked(const Impl &impl, std::uint64_t generation)
    {
        return impl.runGeneration == generation && impl.state == SessionState::Starting;
    }

    // Startup-failure helper: takes the teardown claim for a run that is still current. Reports
    // false when a concurrent stop() already owns the teardown of that run.
    static bool claimFailureTeardown(Impl &impl, std::uint64_t generation)
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        if (!Impl::runStillValidLocked(impl, generation) || impl.teardownStarted)
            return false;

        claimTeardownLocked(impl);
        return true;
    }

    // Publishes the diagnostic for a start() whose run a concurrent stop() took over.
    //
    // Locking contract: the caller must hold `mutex`; this function never acquires it. (The R3-B1
    // deadlock was exactly a helper that acquired `mutex` while the caller already held it.) It only
    // records the error; the teardown - not this call - owns the run resources, and the caller
    // notifies waiters after releasing the lock.
    static void markStartCancelledLocked(Impl &impl)
    {
        if (impl.state != SessionState::Running) {
            impl.lastError =
                SessionError{SessionErrorCode::StartCancelled,
                             "Session::start() was cancelled by a concurrent stop(); the Session "
                             "is Stopped",
                             false};
        }
    }

    // Ordered teardown of a run.
    //
    // The caller owns the teardown claim and this function releases it at the end. The run's
    // resources are taken over under the mutex (their flags are cleared there), so exactly one side
    // stops/joins each of them: whatever start() committed before the claim, and whatever start()
    // completed afterwards, is cleaned up by exactly one of the two paths.
    //
    // Stops whatever started, unblocks and joins the workers, closes the callback gate and finally
    // publishes `Faulted` (for a startup failure) or `Stopped` (when called from stop()), so that
    // the observable sequence of ADR-0003 is preserved:
    //   Starting -> Faulted -> (stop) -> Stopping -> Stopped
    static void teardownRun(Impl &impl, bool faulted, SessionErrorCode code, const std::string &message)
    {
        CaptureSource *source = nullptr;
        Transport *transport = nullptr;
        detail::Mailbox *mailbox = nullptr;
        bool workers = false;
        std::shared_ptr<CallbackGate> gate;
        {
            std::lock_guard<std::mutex> lock(impl.mutex);
            impl.stopRequested = true;
            source = impl.sourceStarted ? impl.source.get() : nullptr;
            transport = impl.transportStarted ? impl.transport.get() : nullptr;
            workers = impl.workersCreated;
            mailbox = workers ? impl.mailbox.get() : nullptr;
            gate = impl.gate;
            impl.sourceStarted = false;
            impl.transportStarted = false;
            impl.workersCreated = false;

            // Publish the terminal state before touching the components so that observers inside the
            // components see the documented sequence: a startup failure is `Faulted` from here on,
            // an owner-initiated stop is `Stopping` until the teardown completes.
            if (faulted)
                setFaultedLocked(impl, code, message);
            else
                impl.state = SessionState::Stopping;

            // Observable proof that exactly one owner performed the ordered teardown of this run.
            ++impl.stats.teardownsPerformed;
        }
        impl.cv.notify_all();  // let the workers leave their startup handshake

        // Close the callback gate first: from here on no adapter callback can reach this run.
        if (gate)
            gate->closeAndDrain();

        stopComponentQuietly(source);

        if (mailbox != nullptr)
            mailbox->close();  // the dispatch worker drains what Core already accepted

        if (workers) {
            if (impl.schedulerThread.joinable())
                impl.schedulerThread.join();
            if (impl.dispatcherThread.joinable())
                impl.dispatcherThread.join();
        }

        stopComponentQuietly(transport);

        {
            std::lock_guard<std::mutex> lock(impl.mutex);
            if (impl.mailbox) {
                // Fold the mailbox-owned per-run values into impl.stats before the mailbox is destroyed.
                // stats() can only overlay them while the mailbox exists, so without this a post-stop query
                // silently reported zeros for a run that really did drop or reject frames, the maximum
                // mailbox depth and the last frame id - contradicting the header contract that cumulative
                // counters keep their run value until the next start() reset (#159).
                const detail::MailboxStats finalMailbox = impl.mailbox->stats();
                impl.stats.maxMailboxWaitingObserved = finalMailbox.maxWaitingObserved;
                impl.stats.maxMailboxOwnedObserved = finalMailbox.maxOwnedObserved;
                impl.stats.framesDroppedByPolicy = finalMailbox.droppedOldest;
                impl.stats.framesRejectedOverflow = finalMailbox.rejectedOverflow;
                impl.stats.lastFrameId = finalMailbox.lastFrameId;
                // The gauges (mailboxWaiting / mailboxDispatcherOwned) are deliberately not folded: an empty
                // mailbox after stop is the truthful current value, whereas the maxima and counters above are
                // historical facts about the run that just ended.
            }
            impl.mailbox.reset();
            impl.inFlight = 0;
            impl.stats.inFlight = 0;
            impl.teardownStarted = false;  // release the claim for the next claimant or start()
            if (!faulted)
                impl.state = SessionState::Stopped;
        }
        impl.cv.notify_all();
    }

    // Owns the transport hand-off. Runs on its own thread so the capture callback path never
    // touches the transport.
    static void runDispatcher(Impl &impl)
    {
        {
            // Observable "the worker thread actually began", before the handshake.
            std::lock_guard<std::mutex> lock(impl.mutex);
            ++impl.stats.workersStarted;
        }
        impl.cv.notify_all();

        detail::Mailbox &mailbox = *impl.mailbox;
        Transport *transport = impl.transport.get();

        // Startup handshake: dispatching must not begin before `Running` is published. The worker
        // then always enters the drain loop, which exits only once the mailbox is closed and empty.
        {
            std::unique_lock<std::mutex> lock(impl.mutex);
            awaitStartup(impl, lock);
        }

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

bool Session::setCaptureSource(std::unique_ptr<CaptureSource> source)
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (m_impl->state != SessionState::Stopped)
        return false;  // never destroy a component that a worker or callback may still be using

    // Converting to shared ownership does not release the previous component here if an in-flight start()
    // is still holding a pin; it is destroyed when that pin goes away, which is the whole point.
    m_impl->source = std::shared_ptr<CaptureSource>(std::move(source));
    return true;
}

bool Session::setTransport(std::unique_ptr<Transport> transport)
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (m_impl->state != SessionState::Stopped)
        return false;

    m_impl->transport = std::shared_ptr<Transport>(std::move(transport));
    return true;
}

void Session::setInputSink(std::shared_ptr<InputSink> sink)
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->sink = std::move(sink);
}

bool Session::start()
{
    Impl &impl = *m_impl;

    // Strong pins to the run's components, assigned under the lock below and held until this call returns
    // (including every failure path). They are what keeps the objects alive while start() calls into them
    // without the mutex, even if a concurrent stop() plus a public setter replaces them (#159).
    std::shared_ptr<CaptureSource> sourcePin;
    std::shared_ptr<Transport> transportPin;

    // Generation of the run this call is establishing; every startup boundary re-checks it.
    std::uint64_t generation = 0;

    const std::string configProblem = validateSessionConfig(impl.config);
    if (!configProblem.empty()) {
        std::lock_guard<std::mutex> lock(impl.mutex);
        impl.lastError = SessionError{SessionErrorCode::InvalidConfiguration, configProblem, false};
        return false;
    }

    // A Core-owned callback gate makes late adapter callbacks harmless: the callbacks capture the
    // gate, not `&impl`, and closing the gate invalidates the target before Impl is destroyed. It is
    // installed together with the run claim below (one critical section), so there is never a moment
    // where the run exists but a late callback could still reach it.
    auto gate = std::make_shared<CallbackGate>();
    gate->target = &impl;

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

        // Adapter exceptions must not escape start(): this path runs before anything is started, so
        // it fails cleanly while the Session is still Stopped.
        CompatibilityResult compatibility;
        std::string capabilityFailure;
        try {
            compatibility =
                checkFrameCompatibility(impl.source->capabilities(), impl.transport->frameCapabilities());
        } catch (const std::exception &error) {
            capabilityFailure =
                std::string("capability query threw: ") + error.what();
        } catch (...) {
            capabilityFailure = "capability query threw an unknown exception";
        }

        if (!capabilityFailure.empty()) {
            impl.lastError = SessionError{SessionErrorCode::ComponentFailure, capabilityFailure, false};
            return false;
        }
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

        // Claim the run and install its callback gate in one critical section: the generation is
        // what a concurrent stop() invalidates when it takes the teardown claim, and every startup
        // boundary below re-checks it before committing the next step, so a cancelled start() can
        // never publish `Running`, install a new run, or leave a gate that no teardown owns.
        generation = ++impl.runGeneration;
        impl.gate = gate;
        impl.state = SessionState::Starting;

        // Pin both components under the lock for the whole duration of this start(). A concurrent stop()
        // can return the Session to Stopped and re-allow the public setters, so without a strong pin the
        // object behind these raw pointers could be destroyed while this call is still using it. The pins
        // are released when start() returns, including on every failure path (#159).
        sourcePin = impl.source;
        transportPin = impl.transport;
    }
    impl.cv.notify_all();

    CaptureSource *source = sourcePin.get();
    Transport *transport = transportPin.get();

    // ---- boundary 1: capture source start ------------------------------------------------
    bool sourceStarted = false;
    std::string sourceMessage;
    try {
        sourceStarted =
            source->start([gate](RemoteFrame frame) {
                              GateGuard guard(gate);
                              if (auto *target = static_cast<Impl *>(guard.enter()))
                                  Impl::atCallbackBoundary(*target, [target, frame = std::move(frame)]() mutable {
                                      Impl::onFrameReady(*target, std::move(frame));
                                  });
                          },
                          [gate](const CaptureEvent &event) {
                              GateGuard guard(gate);
                              // Captured by reference, not by value: a copy of an event carrying a long message
                              // allocates, and a closure that is materialised before the call would perform that
                              // allocation outside the boundary - so an allocation failure would escape into the
                              // backend stack instead of being contained, which is the contract this boundary
                              // exists to hold. The boundary invokes the callable synchronously, so the reference
                              // is valid for the whole call.
                              if (auto *target = static_cast<Impl *>(guard.enter()))
                                  Impl::atCallbackBoundary(*target, [target, &event] {
                                      Impl::onCaptureEvent(*target, event);
                                  });
                          });
    } catch (const std::exception &error) {
        // A throwing start may have left partial state behind, so the component is stopped again
        // before the run is torn down.
        sourceMessage = std::string("capture source threw from start(): ") + error.what();
    } catch (...) {
        sourceMessage = "capture source threw from start()";
    }

    bool cancelled = false;
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        if (!Impl::runStillValidLocked(impl, generation)) {
            Impl::markStartCancelledLocked(impl);
            cancelled = true;
        } else {
            impl.sourceStarted = sourceStarted;  // committed: the teardown now owns this component
        }
    }

    if (cancelled) {
        // The run was taken over while the source was starting. The teardown snapshot ran before
        // this component was committed, so cleaning it up here is what keeps the stop count at
        // exactly one.
        if (sourceStarted || !sourceMessage.empty())
            stopComponentQuietly(source);
        impl.cv.notify_all();
        return false;
    }

    if (!sourceMessage.empty()) {
        stopComponentQuietly(source);
        if (Impl::claimFailureTeardown(impl, generation)) {
            Impl::teardownRun(impl, true, SessionErrorCode::CaptureStartFailed, sourceMessage);
        } else {
            {
                std::lock_guard<std::mutex> lock(impl.mutex);
                Impl::markStartCancelledLocked(impl);
            }
            impl.cv.notify_all();
        }
        return false;
    }
    if (!sourceStarted) {
        if (Impl::claimFailureTeardown(impl, generation)) {
            Impl::teardownRun(impl, true, SessionErrorCode::CaptureStartFailed,
                              "capture source failed to start");
        } else {
            {
                std::lock_guard<std::mutex> lock(impl.mutex);
                Impl::markStartCancelledLocked(impl);
            }
            impl.cv.notify_all();
        }
        return false;
    }

    // ---- boundary 2: Core workers --------------------------------------------------------
    //
    // The mailbox and both worker threads are created while the Session is still `Starting`, and
    // they wait in the startup handshake, so no capture request is issued before `Running` and the
    // transport is guaranteed to be started before any frame can be handed over.
    //
    // Creating them and validating the run are one step under the mutex: a teardown sees either
    // "no workers" or "both threads assigned", never a half state it would join incorrectly.
    std::string workerMessage;
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        if (!Impl::runStillValidLocked(impl, generation)) {
            Impl::markStartCancelledLocked(impl);
            cancelled = true;
        } else {
            try {
                impl.mailbox = std::make_unique<detail::Mailbox>(impl.config.frameQueue.capacity,
                                                                impl.config.frameQueue.policy);
                // Set before creating the threads so that a partial failure still hands the
                // existing thread to the teardown.
                impl.workersCreated = true;
                impl.schedulerThread = std::thread([&impl] {
                    Impl::atWorkerBoundary(impl, Impl::runScheduler, "core scheduler worker threw");
                });
                impl.dispatcherThread = std::thread([&impl] {
                    Impl::atWorkerBoundary(impl, Impl::runDispatcher, "core dispatch worker threw");
                });
            } catch (const std::exception &error) {
                workerMessage = std::string("failed to create a Core worker thread: ") + error.what();
            } catch (...) {
                workerMessage = "failed to create a Core worker thread";
            }
        }
    }

    if (cancelled) {
        impl.cv.notify_all();
        return false;  // nothing was created, so nothing to clean up here
    }
    if (!workerMessage.empty()) {
        // A half-started run must never be left behind: the failure teardown joins whichever worker
        // exists, stops the capture source and leaves the Session Faulted.
        if (Impl::claimFailureTeardown(impl, generation)) {
            Impl::teardownRun(impl, true, SessionErrorCode::ComponentFailure, workerMessage);
        } else {
            {
                std::lock_guard<std::mutex> lock(impl.mutex);
                Impl::markStartCancelledLocked(impl);
            }
            impl.cv.notify_all();
        }
        return false;
    }

    // ---- boundary 3: transport start -----------------------------------------------------
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        if (!Impl::runStillValidLocked(impl, generation)) {
            Impl::markStartCancelledLocked(impl);
            cancelled = true;
        }
    }
    if (cancelled) {
        impl.cv.notify_all();
        return false;  // do not call the external start after the run was cancelled
    }

    bool transportStarted = false;
    std::string transportMessage;
    try {
        transportStarted =
            transport->start([gate](const InputEvent &event) {
                                 GateGuard guard(gate);
                                 if (auto *target = static_cast<Impl *>(guard.enter()))
                                     // By reference for the same reason as the capture-event path: the copy would
                                     // allocate before the boundary, where nothing can contain the failure.
                                     Impl::atCallbackBoundary(*target, [target, &event] {
                                         Impl::onInput(*target, event);
                                     });
                             },
                             [gate](const TransportEvent &event) {
                                 GateGuard guard(gate);
                                 if (auto *target = static_cast<Impl *>(guard.enter()))
                                     Impl::atCallbackBoundary(*target, [target, &event] {
                                         Impl::onTransportEvent(*target, event);
                                     });
                             });
    } catch (const std::exception &error) {
        transportMessage = std::string("transport threw from start(): ") + error.what();
    } catch (...) {
        transportMessage = "transport threw from start()";
    }

    // ---- boundary 3 commit and `Running` publication --------------------------------------
    //
    // The transport commit, the cancellation check and the publication of `Running` are one critical
    // section, so there is no window in which a stop() could claim the run after the transport was
    // committed but before the state became `Running`. If a stop() won earlier, the teardown owns
    // every component of this run (it snapshotted the committed flags) and this start() only reports
    // the cancellation - and it never publishes `Running`.
    cancelled = false;
    bool published = false;
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        if (!Impl::runStillValidLocked(impl, generation)) {
            Impl::markStartCancelledLocked(impl);
            cancelled = true;
        } else {
            impl.transportStarted = transportStarted;
            if (transportMessage.empty() && transportStarted) {
                impl.state = SessionState::Running;
                published = true;
            }
        }
    }

    if (cancelled) {
        // The transport finished starting after the cancellation, so it belongs to this start().
        if (transportStarted || !transportMessage.empty())
            stopComponentQuietly(transport);
        impl.cv.notify_all();
        return false;
    }

    if (!transportMessage.empty()) {
        stopComponentQuietly(transport);
        if (Impl::claimFailureTeardown(impl, generation)) {
            Impl::teardownRun(impl, true, SessionErrorCode::TransportStartFailed, transportMessage);
        } else {
            {
                std::lock_guard<std::mutex> lock(impl.mutex);
                Impl::markStartCancelledLocked(impl);
            }
            impl.cv.notify_all();
        }
        return false;
    }
    if (!transportStarted) {
        // The capture source that did start is stopped here; the Session stays Faulted so the
        // startup failure is observable until the owner calls stop().
        if (Impl::claimFailureTeardown(impl, generation)) {
            Impl::teardownRun(impl, true, SessionErrorCode::TransportStartFailed,
                              "transport failed to start");
        } else {
            {
                std::lock_guard<std::mutex> lock(impl.mutex);
                Impl::markStartCancelledLocked(impl);
            }
            impl.cv.notify_all();
        }
        return false;
    }

    if (!published) {
        // Unreachable: the critical section above publishes `Running` exactly when the run is still
        // valid and the transport started. Kept as a guard so the invariant is checked, not assumed.
        impl.cv.notify_all();
        return false;
    }

    impl.cv.notify_all();  // release the workers from the startup handshake
    return true;
}

void Session::stop() noexcept
{
    Impl &impl = *m_impl;

    // Fast path: this run is already down.
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        if (impl.state == SessionState::Stopped)
            return;
    }

    // Take the teardown claim atomically. A concurrent stop() (or an in-progress start() that is
    // being cancelled) waits here until the current teardown has finished, and then either finds the
    // run already `Stopped` or completes the transition of a faulted run. The claim also invalidates
    // the run generation, which is how a concurrent start() learns that its run was cancelled.
    Impl::claimTeardown(impl);

    bool alreadyStopped = false;
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        if (impl.state == SessionState::Stopped) {
            // Another caller completed the teardown while we waited for the claim. Release it so
            // further waiting callers can finish as well, and touch nothing else.
            impl.teardownStarted = false;
            alreadyStopped = true;
        }
    }
    if (alreadyStopped) {
        impl.cv.notify_all();
        return;
    }

    // The ordered teardown closes the callback gate before touching the components, so a late
    // frame/event/input callback from an adapter that violates its quiescence rule is ignored
    // instead of reaching a half-destroyed Session.
    Impl::teardownRun(impl, false, SessionErrorCode::ComponentFailure, {});
}

SessionState Session::state() const
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->state;
}

void Session::setChangeCallback(std::function<void(Change, SessionState, std::optional<SessionError>)> callback)
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->changeCallback = std::move(callback);
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
    if (impl.gate) {
        // The counter lives on the gate so that a callback arriving after teardown can still be
        // counted without touching the implementation object.
        snapshot.callbacksIgnoredAfterStop =
            impl.gate->ignored.load(std::memory_order_relaxed);
    }

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