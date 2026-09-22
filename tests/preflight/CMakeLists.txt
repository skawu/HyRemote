# Focused preflight harness for the V0.2.0.0 authenticated/encrypted profile (issue #258).
#
# This is deliberately a standalone project rather than a subdirectory of the product build: the harness is evidence
# for a technical decision, it fails closed when the OpenSSL TLS backend is unavailable (which is the correct product
# behaviour and the wrong behaviour for an ordinary developer's default suite), and it needs an OpenSSL development
# package that the product build itself only requires when the transport-security capability is enabled. Keeping it
# separate leaves the product build graph, its options and its default suite untouched.
#
# Reproduction (see docs/internal/v0.2-technical-preflight.md for the full record):
#   cmake -S tests/preflight -B build/tls-preflight -G Ninja \
#         -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt 6.8.3 prefix> \
#         -DOPENSSL_ROOT_DIR=<OpenSSL with libssl/libcrypto and headers>
#   cmake --build build/tls-preflight
#   ctest --test-dir build/tls-preflight --output-on-failure
#
# The OpenSSL runtime that carries the TLS session is the one Qt's openssl backend plugin loads at run time, so its
# directory (for example <QtRoot>/Tools/OpenSSLv3/Win_x64/bin or the system library path) must be on the loader path.

cmake_minimum_required(VERSION 3.21)

project(HyRemoteTlsPreflight LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)
find_package(OpenSSL REQUIRED)

add_executable(hyremote-tls-transition-preflight test_qt_tls_transition_spike.cpp)

target_link_libraries(hyremote-tls-transition-preflight
    PRIVATE
        Qt6::Core
        Qt6::Network
        OpenSSL::SSL
        OpenSSL::Crypto)

# The harness prints the exact OpenSSL libraries it was linked against, because "openssl is available" is not evidence.
target_compile_definitions(hyremote-tls-transition-preflight
    PRIVATE
        HYREMOTE_PREFLIGHT_OPENSSL_CRYPTO="${OPENSSL_CRYPTO_LIBRARY}"
        HYREMOTE_PREFLIGHT_OPENSSL_SSL="${OPENSSL_SSL_LIBRARY}")

enable_testing()

add_test(NAME hyremote-tls-transition-preflight COMMAND hyremote-tls-transition-preflight)
set_tests_properties(hyremote-tls-transition-preflight PROPERTIES
    TIMEOUT 300
    LABELS "preflight")
