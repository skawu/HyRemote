// Focused #259 test for the declarative frontend: the three product properties are driven by the shared
// runtime's typed notifications instead of a 100 ms timer, delivery is marshalled onto this object's
// thread, and a frontend-local validation error is not erased by an unrelated runtime event.
//
// Nothing here waits on a wall clock to prove "no polling" (#259 forbids that kind of test). The proof is
// that an injected runtime event produces the signal, plus a source-level check that the timer is gone;
// the bounded event-loop waits below only guard against a hang and never decide a result.
//
// The product signals are connected by meta-object signature: a signal is resolved at run time, so the
// test needs no exported symbol from the QML payload - which is exactly the property the payload should
// keep. A missing or renamed signal is reported by the failed connection check instead of silently
// testing nothing.

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QThread>
#include <QTimer>
#include <QVariant>

#include "detail/component_factories.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';          \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

struct RuntimeState
{
    int inputPosts = 0;
    bool throwOnInputPost = false;
    hyremote::InputHandler inputHandler;
    hyremote::CaptureEventHandler captureEventHandler;
    hyremote::TransportEventHandler transportEventHandler;
};

class FakeInput final : public hyremote::InputSink
{
public:
    explicit FakeInput(std::shared_ptr<RuntimeState> state)
        : m_state(std::move(state))
    {
    }

    void post(const hyremote::InputEvent &) override
    {
        ++m_state->inputPosts;
        if (m_state->throwOnInputPost) {
            throw std::runtime_error("deterministic input failure");
        }
    }

    void shutdown() noexcept override {}

private:
    std::shared_ptr<RuntimeState> m_state;
};

class FakeCapture final : public hyremote::CaptureSource
{
public:
    explicit FakeCapture(std::shared_ptr<RuntimeState> state)
        : m_state(std::move(state))
    {
    }

    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities result;
        result.asynchronous = true;
        result.cpuReadable = true;
        result.cpuFormats = {hyremote::PixelFormat::Bgra8888};
        return result;
    }

    bool start(hyremote::FrameReadyHandler, hyremote::CaptureEventHandler onEvent) override
    {
        m_state->captureEventHandler = std::move(onEvent);
        return true;
    }

    void stop() noexcept override { m_state->captureEventHandler = {}; }

    bool requestFrame(const hyremote::CaptureRequest &) override { return false; }

private:
    std::shared_ptr<RuntimeState> m_state;
};

class FakeTransport final : public hyremote::Transport
{
public:
    explicit FakeTransport(std::shared_ptr<RuntimeState> state)
        : m_state(std::move(state))
    {
    }

    hyremote::FrameConsumerCapabilities frameCapabilities() const override
    {
        hyremote::FrameConsumerCapabilities result;
        result.acceptsCpu = true;
        result.cpuFormats = {hyremote::PixelFormat::Bgra8888};
        return result;
    }

    bool start(hyremote::InputHandler onInput, hyremote::TransportEventHandler onEvent) override
    {
        m_state->inputHandler = std::move(onInput);
        m_state->transportEventHandler = std::move(onEvent);
        return true;
    }

    void stop() noexcept override
    {
        m_state->inputHandler = {};
        m_state->transportEventHandler = {};
    }

    void enqueueFrame(hyremote::RemoteFrame) override {}

private:
    std::shared_ptr<RuntimeState> m_state;
};

void installRuntime(const std::shared_ptr<RuntimeState> &state)
{
    HyRemote::detail::setTargetFactory([state](QObject *target, bool remoteInputEnabled) {
        HyRemote::detail::TargetComponents result;
        result.supported = target != nullptr;
        if (!result.supported) {
            result.error = QStringLiteral("no adapter supports this target");
            return result;
        }
        result.capture = std::make_unique<FakeCapture>(state);
        if (remoteInputEnabled) {
            result.input = std::make_shared<FakeInput>(state);
        }
        return result;
    });

    HyRemote::detail::setTransportFactory([state](const QHostAddress &, quint16,
                                                  const HyRemote::detail::RfbSecurityConfig &) {
        HyRemote::detail::TransportComponent result;
        result.transport = std::make_unique<FakeTransport>(state);
        return result;
    });
}

void waitForComponent(QQmlComponent &component)
{
    QElapsedTimer timer;
    timer.start();
    while (component.status() == QQmlComponent::Loading && timer.elapsed() < 5000) {
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::AllEvents);
    }

    if (component.isError() || component.status() != QQmlComponent::Ready) {
        std::cerr << "QML component status=" << static_cast<int>(component.status()) << '\n';
        for (const QQmlError &error : component.errors()) {
            std::cerr << error.toString().toStdString() << '\n';
        }
    }
}

std::unique_ptr<QObject> createAccess(QQmlEngine &engine, const char *name)
{
    QQmlComponent component(&engine);
    component.setData("import HyRemote\n"
                      "RemoteAccess {\n"
                      "}\n",
                      QUrl(QString::fromLatin1(name)));
    waitForComponent(component);
    CHECK(component.status() == QQmlComponent::Ready);
    if (component.status() != QQmlComponent::Ready) {
        return {};
    }
    return std::unique_ptr<QObject>(component.create());
}

// Drains the event queue so a notification queued from another thread is delivered. It is a bounded
// guard against a hang, never a timing assertion.
void settle(int rounds = 3)
{
    for (int round = 0; round < rounds; ++round) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// Counts the three product signals and records whether any of them was delivered off the QML object's
// own thread.
class SignalCounters : public QObject
{
    Q_OBJECT

public:
    explicit SignalCounters(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    void watch(QThread *ownerThread, QObject *emitter)
    {
        m_ownerThread = ownerThread;
        const auto stateConnection = QObject::connect(emitter, SIGNAL(stateChanged()), this, SLOT(onStateChanged()));
        const auto countConnection = QObject::connect(emitter,
                                                      SIGNAL(connectedClientCountChanged()),
                                                      this,
                                                      SLOT(onConnectedClientCountChanged()));
        const auto errorConnection = QObject::connect(emitter, SIGNAL(errorChanged()), this, SLOT(onErrorChanged()));
        CHECK(static_cast<bool>(stateConnection));
        CHECK(static_cast<bool>(countConnection));
        CHECK(static_cast<bool>(errorConnection));
    }

    std::atomic<int> stateChanged{0};
    std::atomic<int> connectedClientCountChanged{0};
    std::atomic<int> errorChanged{0};
    std::atomic<int> offThreadSignals{0};

public slots:
    void onStateChanged()
    {
        ++stateChanged;
        noteThread();
    }

    void onConnectedClientCountChanged()
    {
        ++connectedClientCountChanged;
        noteThread();
    }

    void onErrorChanged()
    {
        ++errorChanged;
        noteThread();
    }

private:
    void noteThread()
    {
        if (m_ownerThread != nullptr && QThread::currentThread() != m_ownerThread) {
            ++offThreadSignals;
        }
    }

    QThread *m_ownerThread = nullptr;
};

// --------------------------------------------------------------------------------------------------- cases

void testStateAndClientCountAreEventDriven(QQmlEngine &engine)
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    auto access = createAccess(engine, "notifications_state.qml");
    CHECK(access != nullptr);
    if (!access) {
        HyRemote::detail::resetFactories();
        return;
    }

    SignalCounters counters;
    counters.watch(access->thread(), access.get());

    access->setProperty("target", QVariant::fromValue<QObject *>(&target));
    access->setProperty("remoteInputEnabled", true);
    access->setProperty("enabled", true);
    settle();

    CHECK(access->property("state").toInt() == 2);  // Running
    // Starting and Running are two distinct values, so the state signal fires twice: the frozen order
    // is observable in the frontend as well.
    CHECK(counters.stateChanged.load() == 2);
    CHECK(counters.offThreadSignals.load() == 0);

    counters.stateChanged.store(0);
    counters.connectedClientCountChanged.store(0);
    state->transportEventHandler(hyremote::TransportEvent{hyremote::TransportEventCode::ClientConnected, "connect"});
    CHECK(access->property("connectedClientCount").toULongLong() == 1u);
    CHECK(counters.connectedClientCountChanged.load() == 1);
    // A client-count change is not a state change: a timer-free frontend must not emit an unrelated
    // signal, which is what makes these signals evidence rather than periodic noise.
    CHECK(counters.stateChanged.load() == 0);

    state->transportEventHandler(hyremote::TransportEvent{hyremote::TransportEventCode::ClientConnected, "connect"});
    CHECK(access->property("connectedClientCount").toULongLong() == 2u);
    state->transportEventHandler(hyremote::TransportEvent{hyremote::TransportEventCode::ClientDisconnected, "bye"});
    state->transportEventHandler(hyremote::TransportEvent{hyremote::TransportEventCode::ClientDisconnected, "bye"});
    CHECK(access->property("connectedClientCount").toULongLong() == 0u);
    CHECK(counters.connectedClientCountChanged.load() == 4);

    access->setProperty("enabled", false);
    CHECK(access->property("state").toInt() == 0);  // Stopped

    HyRemote::detail::resetFactories();
}

void testTargetLossNotifiesStateAndError(QQmlEngine &engine)
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    auto access = createAccess(engine, "notifications_target_loss.qml");
    CHECK(access != nullptr);
    if (!access) {
        HyRemote::detail::resetFactories();
        return;
    }

    SignalCounters counters;
    counters.watch(access->thread(), access.get());

    access->setProperty("target", QVariant::fromValue<QObject *>(&target));
    access->setProperty("enabled", true);
    settle();
    CHECK(access->property("state").toInt() == 2);  // Running

    counters.stateChanged.store(0);
    counters.errorChanged.store(0);
    state->captureEventHandler(hyremote::CaptureEvent{hyremote::CaptureEventCode::TargetLost,
                                                      "target destroyed during the run", false});
    settle();

    CHECK(access->property("state").toInt() == 4);  // Faulted
    CHECK(counters.stateChanged.load() == 1);
    CHECK(counters.errorChanged.load() == 1);
    CHECK(access->property("errorCode").toInt() != 0);
    CHECK(!access->property("errorString").toString().isEmpty());
    CHECK(!access->property("recoverableError").toBool());

    HyRemote::detail::resetFactories();
}

void testRecoverableErrorAndClearErrorParity(QQmlEngine &engine)
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    auto access = createAccess(engine, "notifications_error.qml");
    CHECK(access != nullptr);
    if (!access) {
        HyRemote::detail::resetFactories();
        return;
    }

    SignalCounters counters;
    counters.watch(access->thread(), access.get());

    access->setProperty("target", QVariant::fromValue<QObject *>(&target));
    access->setProperty("remoteInputEnabled", true);
    access->setProperty("enabled", true);
    settle();

    counters.errorChanged.store(0);
    state->throwOnInputPost = true;
    state->inputHandler(hyremote::InputEvent{});
    CHECK(state->inputPosts == 1);
    CHECK(counters.errorChanged.load() == 1);
    CHECK(access->property("errorCode").toInt() != 0);
    CHECK(access->property("recoverableError").toBool());

    counters.errorChanged.store(0);
    CHECK(QMetaObject::invokeMethod(access.get(), "clearError"));
    CHECK(counters.errorChanged.load() == 1);
    CHECK(access->property("errorCode").toInt() == 0);
    CHECK(access->property("errorString").toString().isEmpty());
    CHECK(!access->property("recoverableError").toBool());

    access->setProperty("enabled", false);
    HyRemote::detail::resetFactories();
}

void testLocalValidationErrorSurvivesUnrelatedRuntimeEvents(QQmlEngine &engine)
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    auto access = createAccess(engine, "notifications_local_error.qml");
    CHECK(access != nullptr);
    if (!access) {
        HyRemote::detail::resetFactories();
        return;
    }

    SignalCounters counters;
    counters.watch(access->thread(), access.get());

    access->setProperty("target", QVariant::fromValue<QObject *>(&target));
    access->setProperty("enabled", true);
    settle();

    // A frontend-local validation error while running. setProperty() itself reports success because a
    // QML WRITE property returns void; the refusal shows up as the local error asserted next.
    access->setProperty("port", 0);
    CHECK(access->property("errorCode").toInt() == 1);  // InvalidConfiguration
    const QString localMessage = access->property("errorString").toString();
    CHECK(!localMessage.isEmpty());

    // An unrelated runtime event (a client connecting) must neither emit errorChanged nor erase what
    // this frontend refused: the effective error stays the local one.
    counters.errorChanged.store(0);
    state->transportEventHandler(hyremote::TransportEvent{hyremote::TransportEventCode::ClientConnected, "connect"});
    CHECK(counters.errorChanged.load() == 0);
    CHECK(access->property("errorCode").toInt() == 1);
    CHECK(access->property("errorString").toString() == localMessage);

    // clearError() clears both the local and the runtime side.
    CHECK(QMetaObject::invokeMethod(access.get(), "clearError"));
    CHECK(access->property("errorCode").toInt() == 0);
    CHECK(access->property("errorString").toString().isEmpty());

    access->setProperty("enabled", false);
    HyRemote::detail::resetFactories();
}

void testNotificationFromAWorkerThreadIsMarshalledToTheOwnerThread(QQmlEngine &engine)
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    auto access = createAccess(engine, "notifications_thread.qml");
    CHECK(access != nullptr);
    if (!access) {
        HyRemote::detail::resetFactories();
        return;
    }

    SignalCounters counters;
    counters.watch(access->thread(), access.get());

    access->setProperty("target", QVariant::fromValue<QObject *>(&target));
    access->setProperty("enabled", true);
    settle();

    // Drive a genuine runtime event from another thread - as a transport worker would - and prove the
    // signal still arrives on this object's thread.
    counters.connectedClientCountChanged.store(0);
    std::thread worker([state] {
        state->transportEventHandler(hyremote::TransportEvent{hyremote::TransportEventCode::ClientConnected, "worker"});
    });
    worker.join();
    settle();

    CHECK(access->property("connectedClientCount").toULongLong() == 1u);
    CHECK(counters.connectedClientCountChanged.load() == 1);
    CHECK(counters.offThreadSignals.load() == 0);

    access->setProperty("enabled", false);
    HyRemote::detail::resetFactories();
}

void testNoSignalAfterDestructionWithAQueuedNotification(QQmlEngine &engine)
{
    const auto state = std::make_shared<RuntimeState>();
    installRuntime(state);
    QObject target;

    SignalCounters counters;

    {
        auto access = createAccess(engine, "notifications_destroy.qml");
        CHECK(access != nullptr);
        if (!access) {
            HyRemote::detail::resetFactories();
            return;
        }

        counters.watch(access->thread(), access.get());
        access->setProperty("target", QVariant::fromValue<QObject *>(&target));
        access->setProperty("enabled", true);
        settle();

        // Queue a notification from a worker thread and destroy the object before the event loop can
        // deliver it. Qt cancels a queued invocation whose context object is gone, and the wrapper's
        // QPointer covers the rest; neither may crash or reach a destroyed object.
        std::thread worker([state] {
            state->transportEventHandler(hyremote::TransportEvent{hyremote::TransportEventCode::ClientConnected, "late"});
        });
        worker.join();
    }

    const int stateSignals = counters.stateChanged.load();
    const int countSignals = counters.connectedClientCountChanged.load();
    settle(5);
    CHECK(counters.stateChanged.load() == stateSignals);
    CHECK(counters.connectedClientCountChanged.load() == countSignals);

    HyRemote::detail::resetFactories();
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(HYREMOTE_QML_IMPORT_PATH));

    testStateAndClientCountAreEventDriven(engine);
    testTargetLossNotifiesStateAndError(engine);
    testRecoverableErrorAndClearErrorParity(engine);
    testLocalValidationErrorSurvivesUnrelatedRuntimeEvents(engine);
    testNotificationFromAWorkerThreadIsMarshalledToTheOwnerThread(engine);
    testNoSignalAfterDestructionWithAQueuedNotification(engine);

    if (failures == 0) {
        std::cout << "QML notification tests passed\n";
        return 0;
    }

    std::cerr << failures << " QML notification check(s) failed\n";
    return 1;
}

#include "test_qml_notifications.moc"
