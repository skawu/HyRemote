#pragma once

// Minimal dependency-free test support for hyremote-core.
//
// The Core tests must run on a host without Qt GUI/GPU modules and without adding a third-party
// test framework, so this is a tiny registry + assertion helper on top of CTest: each test file
// is one executable, and a failing check makes the process exit non-zero.

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace hyremote::test {

struct Failure
{
    std::string message;
};

struct Case
{
    std::string name;
    std::function<void()> body;
};

inline std::vector<Case> &registry()
{
    static std::vector<Case> cases;
    return cases;
}

struct Registrar
{
    Registrar(const char *name, std::function<void()> body)
    {
        registry().push_back(Case{name, std::move(body)});
    }
};

inline void fail(const char *file, int line, const std::string &message)
{
    throw Failure{std::string(file) + ":" + std::to_string(line) + ": " + message};
}

inline int runAll()
{
    // Unbuffered so that the per-case progress prints are never lost when a case crashes and stay
    // interleaved correctly with anything the run writes to stderr. (MSVC requires a null buffer to
    // come with _IONBF; _IOLBF with a null buffer trips the invalid-parameter handler.)
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    int failed = 0;
    for (const Case &testCase : registry()) {
        try {
            testCase.body();
            std::printf("[ PASS ] %s\n", testCase.name.c_str());
        } catch (const Failure &failure) {
            ++failed;
            std::printf("[ FAIL ] %s\n         %s\n", testCase.name.c_str(), failure.message.c_str());
        } catch (const std::exception &error) {
            ++failed;
            std::printf("[ FAIL ] %s\n         unexpected exception: %s\n", testCase.name.c_str(),
                        error.what());
        }
    }

    std::printf("%zu case(s), %d failure(s)\n", registry().size(), failed);
    return failed == 0 ? 0 : 1;
}

// Bounded wait for a *condition* (never a fixed sleep used as synchronization): it polls the
// predicate and fails the caller's assertion if the condition never becomes true in time.
template <typename Predicate>
bool waitFor(Predicate predicate, std::chrono::milliseconds timeout = std::chrono::milliseconds(3000))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return predicate();
}

}  // namespace hyremote::test

#define HYR_TEST(name)                                                            \
    static void name();                                                           \
    static ::hyremote::test::Registrar hyr_registrar_##name(#name, name);         \
    static void name()

#define HYR_CHECK(condition)                                                      \
    do {                                                                          \
        if (!(condition))                                                         \
            ::hyremote::test::fail(__FILE__, __LINE__, "CHECK failed: " #condition); \
    } while (false)

#define HYR_CHECK_MSG(condition, message)                                         \
    do {                                                                          \
        if (!(condition))                                                         \
            ::hyremote::test::fail(__FILE__, __LINE__,                            \
                                   std::string("CHECK failed: " #condition " - ") + (message)); \
    } while (false)

#define HYR_CHECK_EQ(actual, expected)                                            \
    do {                                                                          \
        const auto hyr_actual = (actual);                                         \
        const auto hyr_expected = (expected);                                     \
        if (!(hyr_actual == hyr_expected))                                        \
            ::hyremote::test::fail(__FILE__, __LINE__,                            \
                                   std::string("CHECK_EQ failed: " #actual " == " #expected)); \
    } while (false)

#define HYR_TEST_MAIN()                                                           \
    int main() { return ::hyremote::test::runAll(); }
