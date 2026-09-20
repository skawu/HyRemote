cmake_minimum_required(VERSION 3.21)
project(openssl_probe CXX)

find_package(OpenSSL QUIET)

if(OpenSSL_FOUND)
    message(STATUS "OPENSSL_FOUND version=${OPENSSL_VERSION}")
    message(STATUS "  include=${OPENSSL_INCLUDE_DIR}")
    message(STATUS "  ssl=${OPENSSL_SSL_LIBRARY}")
    message(STATUS "  crypto=${OPENSSL_CRYPTO_LIBRARY}")
else()
    message(STATUS "OPENSSL_NOT_FOUND - the default-ON switch could not be satisfied on this host")
endif()
