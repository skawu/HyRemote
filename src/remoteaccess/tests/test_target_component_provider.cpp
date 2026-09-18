// SPDX-License-Identifier: Apache-2.0
#include "detail/component_factories.hpp"
#include "detail/target_component_provider.hpp"

#include <QObject>

#include <iostream>
#include <memory>

#include "hyremote/core/capture_source.hpp"

namespace {

class FakeCapture final : public hyremote::CaptureSource
{
public:
    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities caps;
        caps.asynchronous = true;
        caps.cpuReadable = true;
        caps.cpuFormats = {hyremote::PixelFormat::Rgba8888};
        return caps;
    }

    bool start(hyremote::FrameReadyHandler, hyremote::CaptureEventHandler) override { return true; }
    void stop() noexcept override {}
    bool requestFrame(const hyremote::CaptureRequest &) override { return true; }
};

class ProviderTarget final : public QObject, public HyRemote::detail::TargetComponentProvider
{
public:
    explicit ProviderTarget(QObject *child = nullptr)
        : m_child(child)
    {
    }

    HyRemote::detail::TargetComponents createTargetComponents(
        bool remoteInputEnabled,
        const HyRemote::detail::BuiltinTargetResolver &resolveBuiltinTarget) override
    {
        ++calls;
        sawRemoteInput = remoteInputEnabled;

        if (m_child) {
            childResult = resolveBuiltinTarget(m_child, false);
            childResolverCalled = true;
        }

        HyRemote::detail::TargetComponents result;
        result.supported = true;
        result.capture = std::make_unique<FakeCapture>();
        return result;
    }

    int calls = 0;
    bool sawRemoteInput = false;
    bool childResolverCalled = false;
    HyRemote::detail::TargetComponents childResult;

private:
    QObject *m_child = nullptr;
};

bool check(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

}  // namespace

int main()
{
    using namespace HyRemote::detail;

    resetFactories();

    QObject ordinary;
    ProviderTarget provider(&ordinary);

    TargetComponents provided = createTargetComponents(&provider, true);
    if (!check(provider.calls == 1, "provider is resolved exactly for its own QObject instance")
        || !check(provider.sawRemoteInput, "remote-input policy is passed to the provider")
        || !check(provided.supported && provided.capture != nullptr,
                  "provider supplies the target components")
        || !check(provider.childResolverCalled,
                  "provider receives a resolver for normal built-in child adapters")
        || !check(!provider.childResult.supported,
                  "builtin resolver bypasses the provider path for an unsupported child")) {
        return 1;
    }

    TargetComponents ordinaryResult = createTargetComponents(&ordinary, false);
    if (!check(!ordinaryResult.supported,
               "provider registration is per-target and does not alter unrelated QObject targets")
        || !check(provider.calls == 1,
                  "unrelated target resolution does not re-enter another target provider")) {
        return 2;
    }

    int overrideCalls = 0;
    setTargetFactory([&overrideCalls](QObject *, bool) {
        ++overrideCalls;
        TargetComponents result;
        result.supported = true;
        result.capture = std::make_unique<FakeCapture>();
        return result;
    });

    TargetComponents overridden = createTargetComponents(&provider, false);
    if (!check(overridden.supported && overridden.capture != nullptr,
               "deterministic global test override still works")
        || !check(overrideCalls == 1, "global test override has explicit precedence")
        || !check(provider.calls == 1, "global override does not accidentally invoke production provider")) {
        resetFactories();
        return 3;
    }

    resetFactories();
    std::cout << "PASS: per-target provider is isolated and built-in resolver is recursion-safe\n";
    return 0;
}
