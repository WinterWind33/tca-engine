// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/atomic-file-write.hpp"

// C++ STL
#include <filesystem>
#include <fstream>
#include <ios>
#include <system_error>
#include <utility>

namespace tca::engine::platform {

namespace {

// The temporary sits next to the target rather than in the system temp folder: a rename is only
// atomic — and, on Windows, only allowed to replace an existing file — within one volume.
[[nodiscard]] std::filesystem::path temporary_path_for(const std::filesystem::path& targetPath) {
    std::filesystem::path temporaryPath{targetPath};
    temporaryPath += ".tmp";

    return temporaryPath;
}

void remove_quietly(const std::filesystem::path& path) noexcept {
    std::error_code ignoredError{};
    std::filesystem::remove(path, ignoredError);
}

} // namespace

bool write_file_atomically(const std::filesystem::path& targetPath,
                           const file_contents_writer& writeContents) noexcept {
    if (targetPath.empty() || !writeContents) {
        return false;
    }

    try {
        if (const auto parentPath{targetPath.parent_path()}; !parentPath.empty()) {
            std::error_code errorCode{};
            std::filesystem::create_directories(parentPath, errorCode);
        }

        const auto temporaryPath{temporary_path_for(targetPath)};

        try {
            bool temporaryWasWritten{false};

            {
                std::ofstream outputStream{temporaryPath, std::ios::out | std::ios::trunc};
                if (!outputStream.is_open()) {
                    return false;
                }

                // Flushed while the stream is still in hand: a destructor cannot report that the
                // disk was full, and a file that was not written whole must not be renamed over a
                // good one.
                temporaryWasWritten = writeContents(outputStream);
                outputStream.flush();
                temporaryWasWritten = temporaryWasWritten && static_cast<bool>(outputStream);
            }

            // Removed only once the stream above is closed: Windows will not delete a file that
            // is still open, which would leave the temporary behind.
            if (!temporaryWasWritten) {
                remove_quietly(temporaryPath);

                return false;
            }

            std::error_code errorCode{};
            std::filesystem::rename(temporaryPath, targetPath, errorCode);
            if (errorCode) {
                remove_quietly(temporaryPath);

                return false;
            }

            return true;
        } catch (...) {
            // A writer that throws part-way through is the same story as one that returns false:
            // the half-written temporary goes, and the target keeps what it had. The stream it
            // was writing to is closed by then, unwound out of the scope above.
            remove_quietly(temporaryPath);

            return false;
        }
    } catch (...) {
        // Writing a file is never worth tearing the program down for: callers are told it did not
        // happen and decide what that means for them.
        return false;
    }
}

} // namespace tca::engine::platform
