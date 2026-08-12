#pragma once

#include <cstdint>
#include <string>

#include "base/ff_type.hpp"

/**
 * Binary ABI carried by ModuleMedia and its public base classes.
 *
 * Increment this value only for an intentional ABI-breaking SDK release.
 */
#define FFMEDIA_MODULE_ABI_VERSION 1u
#define FFMEDIA_SDK_VERSION "2.6.0"
#define FFMEDIA_GLIBCXX_USE_CXX11_ABI 1

#if FFMEDIA_GLIBCXX_USE_CXX11_ABI >= 0
#if !defined(_GLIBCXX_USE_CXX11_ABI)
#error "FFMedia SDK requires libstdc++ and a known _GLIBCXX_USE_CXX11_ABI value"
#elif _GLIBCXX_USE_CXX11_ABI != FFMEDIA_GLIBCXX_USE_CXX11_ABI
#error "FFMedia SDK and consumer use different libstdc++ C++11 ABIs"
#endif
#endif

extern "C" FFMEDIA_API std::uint32_t
ffmedia_module_abi_version() noexcept;

extern "C" FFMEDIA_API std::int32_t
ffmedia_glibcxx_use_cxx11_abi() noexcept;
