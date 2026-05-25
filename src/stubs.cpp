// Project Eve - Stub implementations
// ─────────────────────────────────────────────────────────────
// CommonLibSF's REX::Impl::Log/Fail and SFSE::GetPluginHandle are
// defined in LOG.cpp + API.cpp which we exclude from the build
// (they pull in spdlog wstring APIs incompatible with vcpkg spdlog).
//
// Many CommonLibSF files reference these symbols. We don't actually
// use the logging anywhere in our code path. Stubs let everything
// link without dragging in the broken files.

// PCH provides REX::ELogLevel and the function declarations.

#include "REX/LOG.h"

namespace REX::Impl {

    void Log(const std::source_location, const ELogLevel, const std::string_view) {
        // No-op
    }

    void Log(const std::source_location, const ELogLevel, const std::wstring_view) {
        // No-op
    }

    void Fail(const std::source_location, const std::string_view) {
        // No-op
    }

    void Fail(const std::source_location, const std::wstring_view) {
        // No-op
    }
}

// SFSE::GetPluginHandle - referenced by CommonLibSF's MessagingInterface
// but we use the raw SFSE plugin entry directly (g_plugin_handle in library.cpp).
namespace SFSE {
    std::uint32_t GetPluginHandle() {
        return 0xFFFFFFFFu;  // kPluginHandle_Invalid
    }
}

// MSVC vcruntime vectorized string-search intrinsics that vcpkg's cpr.lib
// expects but aren't exported by our linker's vcruntime. Provide non-SIMD
// fallback implementations.
// (Used internally by std::string_view::find_first_not_of / find_last_not_of)

#include <cstddef>

extern "C" {

[[maybe_unused]] std::size_t __cdecl __std_find_first_not_of_trivial_pos_1(
    const char* haystack, std::size_t haystack_length,
    const char* needle, std::size_t needle_length) noexcept
{
    for (std::size_t i = 0; i < haystack_length; ++i) {
        bool found = false;
        for (std::size_t j = 0; j < needle_length; ++j) {
            if (haystack[i] == needle[j]) { found = true; break; }
        }
        if (!found) return i;
    }
    return SIZE_MAX;
}

[[maybe_unused]] std::size_t __cdecl __std_find_last_not_of_trivial_pos_1(
    const char* haystack, std::size_t haystack_length,
    const char* needle, std::size_t needle_length) noexcept
{
    for (std::size_t i = haystack_length; i > 0; --i) {
        bool found = false;
        for (std::size_t j = 0; j < needle_length; ++j) {
            if (haystack[i - 1] == needle[j]) { found = true; break; }
        }
        if (!found) return i - 1;
    }
    return SIZE_MAX;
}

} // extern "C"
