// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace tca::engine::testing {

/**
 * @brief RAII helper creating a unique temporary directory for a test case, removed again when
 * the test case finishes, regardless of outcome.
 */
class scoped_temp_directory final {
public:
    /**
     * @brief Construct a new scoped temp directory object, creating a fresh, uniquely-named
     * directory under a subFolderName-named subfolder of the system temp directory.
     *
     * @param subFolderName Name of the subfolder grouping this test suite's temporary
     * directories, so they're easy to identify (and clean up manually, if a test crashes before
     * its destructor runs).
     */
    explicit scoped_temp_directory(const std::string_view subFolderName)
        : m_path{std::filesystem::temp_directory_path() / subFolderName / make_unique_name()} {
        std::filesystem::create_directories(m_path);
    }

    ~scoped_temp_directory() {
        std::error_code ignoredError{};
        std::filesystem::remove_all(m_path, ignoredError);
    }

    scoped_temp_directory(const scoped_temp_directory&) = delete;
    scoped_temp_directory& operator=(const scoped_temp_directory&) = delete;

    const std::filesystem::path& path() const noexcept {
        return m_path;
    }

private:
    /**
     * @brief Builds the leaf directory name. The clock reading alone is not enough: two of these
     * created back to back can read the same tick on a coarse clock, which would hand both objects
     * the same directory and let the first destructor delete the second one's contents. The
     * counter makes the name unique regardless of clock resolution.
     */
    static std::string make_unique_name() {
        static std::atomic<std::uint64_t> counter{};

        return std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + '-' +
               std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
    }

    std::filesystem::path m_path;
};

} // namespace tca::engine::testing
