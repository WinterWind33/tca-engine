// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <filesystem>
#include <functional>
#include <ostream>

namespace tca::engine::platform {

/**
 * @brief Puts the contents of a file on the stream it is given, reporting whether it could.
 *
 * A writer that returns false leaves the file it was writing untouched: the half-written
 * temporary is removed and the previous contents stay where they were.
 */
using file_contents_writer = std::function<bool(std::ostream&)>;

/**
 * @brief Writes a file so that an interrupted write cannot destroy what was there before.
 *
 * The contents go to a temporary file next to the target, which is then renamed over it. A
 * rename within one directory is atomic on every platform the engine targets, so a crash, a
 * power loss or a full disk part-way through leaves either the previous file or the new one —
 * never a truncated mixture of the two. This is what makes it worth using for anything a user
 * would be annoyed to lose: settings, save games, launcher configuration.
 *
 * The folders leading to the target are created if they are not there yet.
 *
 * @note What this guarantees is that the *file* is never seen half-written. It does not force
 *  the contents out of the operating system's write-back cache, so a power loss immediately
 *  after this returns may still lose the new contents — the previous ones, however, remain
 *  intact and readable, which is the failure mode that matters here.
 *
 * @param targetPath The file to write.
 * @param writeContents Called with the stream to write the new contents to. Returning false
 *  aborts the write and leaves the target as it was.
 * @return True if the new contents are the ones now stored at targetPath.
 */
bool write_file_atomically(const std::filesystem::path& targetPath,
                           const file_contents_writer& writeContents) noexcept;

} // namespace tca::engine::platform
