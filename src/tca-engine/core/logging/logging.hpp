// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

// Third-party
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>

namespace tca::engine::core::logging {

/**
 * @brief The name of the current run's log file. Previous runs are kept next to it as
 * `engine.1.log` (the most recent) up to `engine.<N>.log` (the oldest).
 */
inline constexpr std::string_view LOG_FILE_NAME{"engine.log"};

/**
 * @brief How many previous runs' logs are kept when the game does not ask for another count, so a
 * crash log survives the next few launches.
 */
inline constexpr std::size_t DEFAULT_KEPT_PREVIOUS_LOGS{5};

/**
 * @brief Builds the sink writing the engine log into the given folder.
 *
 * The folder and its parents are created if they don't exist. The logs of previous runs are
 * shifted by one (`engine.log` becomes `engine.1.log`, `engine.1.log` becomes `engine.2.log`, ...)
 * and the oldest beyond keptPreviousLogs is deleted, so the returned sink always starts from an
 * empty `engine.log`.
 *
 * If the folder is empty or cannot be created, the returned sink writes to the debugger output
 * (on Windows) and stderr instead: losing the log must never stop the game from starting.
 *
 * @param logFolder The folder the log files live in, normally platform::get_preferred_log_folder().
 * @param keptPreviousLogs How many previous runs' logs to keep. Zero keeps none.
 * @return The sink, never null.
 */
[[nodiscard]] std::shared_ptr<spdlog::sinks::sink> make_log_sink(
    const std::filesystem::path& logFolder,
    std::size_t keptPreviousLogs = DEFAULT_KEPT_PREVIOUS_LOGS) noexcept;

/**
 * @brief Names the folder the shared sink returned by get_log_file_sink() writes into.
 *
 * Only takes effect before the shared sink opens its log file, which happens on the first line
 * logged through an engine logger, or on a call to get_log_file_sink() or get_log_folder().
 * Creating loggers with get_logger() doesn't open it. The application calls this when run()
 * starts; a game that logs earlier calls it itself at the very start of main(). Core cannot know
 * the game's name or the platform's folders, so the folder is handed in (typically
 * platform::get_preferred_log_folder(appName)).
 *
 * @param logFolder The folder the log files live in.
 * @param keptPreviousLogs How many previous runs' logs to keep.
 * @return true if the folder will be used, false if the log file is already open.
 */
bool set_log_folder(std::filesystem::path logFolder,
                    std::size_t keptPreviousLogs = DEFAULT_KEPT_PREVIOUS_LOGS) noexcept;

/**
 * @brief Retrieves the shared sink used by all the engine loggers.
 *
 * @note This opens the log file, if it isn't open yet, with make_log_sink() for the folder given to
 * set_log_folder(), which can't be changed afterwards. If no folder was given, the sink logs to the
 * debugger output and stderr only: the engine never writes a file relative to the working
 * directory. Every call returns the same sink instance.
 *
 * @return A shared pointer to the sink instance.
 */
std::shared_ptr<spdlog::sinks::sink> get_log_file_sink();

/**
 * @brief The folder the shared sink writes its log files into, so that other files a player is
 * asked to send (crash dumps, say) can be written next to them.
 *
 * Like get_log_file_sink(), this opens the log file if it isn't open yet, so the answer can't
 * change afterwards.
 *
 * @return The folder, or an empty path if the shared sink writes no file: no folder was named
 * with set_log_folder(), or it could not be created.
 */
[[nodiscard]] std::filesystem::path get_log_folder();

/**
 * @brief The level engine loggers start at, until set_log_level() says otherwise.
 */
inline constexpr spdlog::level::level_enum DEFAULT_LOG_LEVEL{spdlog::level::info};

/**
 * @brief Messages at this level or above are flushed straight away, so the lines just before a
 * crash are on disk rather than in a buffer.
 */
inline constexpr spdlog::level::level_enum FLUSH_LEVEL{spdlog::level::warn};

/**
 * @brief Retrieves the engine logger with the given name, creating it on the first call.
 *
 * Every engine logger writes to the shared sink (and any sink added with add_sink()), follows the
 * level set with set_log_level() and flushes at FLUSH_LEVEL. It is also registered with spdlog, so
 * spdlog::get() and spdlog::flush_every() see it.
 *
 * Creating a logger doesn't open the log file, so a game can take its loggers at any time, even
 * before the log folder is named: the file is opened on the first line logged.
 *
 * @param name The logger's name, which appears on every line it writes.
 * @return The logger, the same instance for the same name.
 */
[[nodiscard]] std::shared_ptr<spdlog::logger> get_logger(std::string_view name);

/**
 * @brief Sets the level of every engine logger, those that already exist and those created later.
 *
 * @param level The lowest level that gets logged.
 */
void set_log_level(spdlog::level::level_enum level) noexcept;

/**
 * @brief The level engine loggers currently log at.
 */
[[nodiscard]] spdlog::level::level_enum get_log_level() noexcept;

/**
 * @brief Reads a level from text, as given on the command line or in a settings file:
 * "trace", "debug", "info", "warn" (or "warning"), "error", "critical" and "off", in any case.
 *
 * @param text The level's name.
 * @return The level, or std::nullopt if the text names none.
 */
[[nodiscard]] std::optional<spdlog::level::level_enum> parse_log_level(
    std::string_view text) noexcept;

/**
 * @brief Adds a sink every engine logger writes to, besides the shared one: a game's own console,
 * an in-game log window, or a test capturing what was logged.
 *
 * @param sink The sink to add.
 * @return false if the sink is null or was already added.
 */
bool add_sink(std::shared_ptr<spdlog::sinks::sink> sink);

/**
 * @brief Removes a sink added with add_sink().
 *
 * @param sink The sink to remove.
 * @return false if the sink was not added.
 */
bool remove_sink(const std::shared_ptr<spdlog::sinks::sink>& sink);

} // namespace tca::engine::core::logging
