// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "core/logging/logging.hpp"

// C++ STL
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

// Third-party
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <spdlog/sinks/msvc_sink.h>
#endif // _WIN32

namespace tca::engine::core::logging {

namespace {
// The log of the run index runs ago: engine.log for the current one (0), engine.<index>.log for
// the previous ones.
std::filesystem::path get_log_file_path(const std::filesystem::path& logFolder,
                                        const std::size_t index) {
    const std::filesystem::path currentLogFile{LOG_FILE_NAME};
    if (index == 0) {
        return logFolder / currentLogFile;
    }

    std::filesystem::path previousLogFile{currentLogFile.stem()};
    previousLogFile += "." + std::to_string(index);
    previousLogFile += currentLogFile.extension();
    return logFolder / previousLogFile;
}

// Shifts every kept log up by one (engine.log becomes engine.1.log, ...), deleting the one that
// would land beyond keptPreviousLogs. Walks from the oldest down so nothing is overwritten, and
// skips the missing ones, so a gap in the history doesn't stop the rest from moving.
void rotate_previous_logs(const std::filesystem::path& logFolder,
                          const std::size_t keptPreviousLogs) {
    if (keptPreviousLogs == 0) {
        // engine.log is truncated when the sink opens it, nothing to keep.
        return;
    }

    std::error_code ignoredError{};
    std::filesystem::remove(get_log_file_path(logFolder, keptPreviousLogs), ignoredError);

    for (std::size_t index{keptPreviousLogs}; index > 0; --index) {
        const auto source{get_log_file_path(logFolder, index - 1)};
        if (!std::filesystem::is_regular_file(source, ignoredError)) {
            continue;
        }

        std::filesystem::rename(source, get_log_file_path(logFolder, index), ignoredError);
    }
}

// Where the log goes when it can't go to a file: the debugger output on Windows, and stderr.
std::shared_ptr<spdlog::sinks::sink> make_fallback_sink() {
    auto sink{std::make_shared<spdlog::sinks::dist_sink_mt>()};
#ifdef _WIN32
    sink->add_sink(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif // _WIN32
    sink->add_sink(std::make_shared<spdlog::sinks::stderr_sink_mt>());
    return sink;
}

// The sink writing engine.log into the folder, or null if no file can be written there.
std::shared_ptr<spdlog::sinks::sink> make_file_sink(const std::filesystem::path& logFolder,
                                                    const std::size_t keptPreviousLogs) noexcept {
    if (logFolder.empty()) {
        return nullptr;
    }

    // create_directories() reports no error when the folder already exists, but does when
    // something that isn't a folder is in the way.
    if (std::error_code error{}; !std::filesystem::create_directories(logFolder, error) &&
                                 !std::filesystem::is_directory(logFolder, error)) {
        return nullptr;
    }

    try {
        rotate_previous_logs(logFolder, keptPreviousLogs);

        // Truncating, so the sink starts from an empty engine.log even when rotation couldn't
        // move the previous one away (it is open elsewhere, say).
        return std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            get_log_file_path(logFolder, 0).string(), true);
    } catch (...) {
        // The file couldn't be opened (permissions, a path the narrow encoding can't hold...):
        // losing the log must never stop the game from starting.
        return nullptr;
    }
}

// What get_log_file_sink() hands out, and the only sink of every engine logger. It forwards to the
// log file (or the fallback) and to the sinks added with add_sink(), so a sink added later still
// reaches the loggers that already exist: a logger's own sink list isn't safe to change while
// another thread logs through it, a dist_sink's is.
//
// The log file is only opened on the first line logged (or when get_log_file_sink() or
// get_log_folder() ask for it), not when a logger is created, so a game can take its loggers
// before run() names the log folder.
class shared_sink final : public spdlog::sinks::sink {
public:
    void log(const spdlog::details::log_msg& message) override;

    void flush() override {
        m_sinks->flush();
    }

    void set_pattern(const std::string& pattern) override {
        m_sinks->set_pattern(pattern);
    }

    void set_formatter(std::unique_ptr<spdlog::formatter> formatter) override {
        m_sinks->set_formatter(std::move(formatter));
    }

    // Empty until the log file is opened, apart from the sinks added with add_sink().
    spdlog::sinks::dist_sink_mt& get_sinks() noexcept {
        return *m_sinks;
    }

    // Set once the log file (or the fallback) is in get_sinks(), so a line logged after that
    // doesn't need the shared state's lock.
    std::atomic<bool> opened{false};

private:
    std::shared_ptr<spdlog::sinks::dist_sink_mt> m_sinks{
        std::make_shared<spdlog::sinks::dist_sink_mt>()};
};

struct shared_sink_state {
    // Taken before the shared sink's own lock, never after it, so opening the log file from a
    // logged line can't deadlock with add_sink().
    std::mutex mutex{};
    std::filesystem::path logFolder{};
    std::size_t keptPreviousLogs{DEFAULT_KEPT_PREVIOUS_LOGS};
    const std::shared_ptr<shared_sink> sink{std::make_shared<shared_sink>()};

    // The log file or the fallback, null until the shared sink is opened.
    std::shared_ptr<spdlog::sinks::sink> target{};

    // The sinks added with add_sink(), so adding one twice or removing one never added is refused.
    std::vector<std::shared_ptr<spdlog::sinks::sink>> addedSinks{};
};

// A function-local static, so loggers created during the static initialisation of other
// translation units still find it constructed.
shared_sink_state& get_shared_sink_state() {
    static shared_sink_state state{};
    return state;
}

// Opens the log file in the folder named so far, if it isn't open yet. Call with state.mutex held.
const std::shared_ptr<shared_sink>& open_shared_sink(shared_sink_state& state) {
    if (state.target == nullptr) {
        state.target = make_file_sink(state.logFolder, state.keptPreviousLogs);
        if (state.target == nullptr) {
            // No file is written, so there is no folder to report.
            state.logFolder.clear();
            state.target = make_fallback_sink();
        }

        std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks{state.target};
        sinks.insert(sinks.end(), state.addedSinks.begin(), state.addedSinks.end());
        state.sink->get_sinks().set_sinks(std::move(sinks));
        state.sink->opened.store(true, std::memory_order_release);
    }

    return state.sink;
}

void shared_sink::log(const spdlog::details::log_msg& message) {
    if (!opened.load(std::memory_order_acquire)) {
        auto& state{get_shared_sink_state()};
        const std::lock_guard lock{state.mutex};
        open_shared_sink(state);
    }

    m_sinks->log(message);
}

struct logger_registry {
    std::mutex mutex{};
    spdlog::level::level_enum level{DEFAULT_LOG_LEVEL};
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers{};
};

logger_registry& get_logger_registry() {
    static logger_registry state{};
    return state;
}
} // namespace

std::shared_ptr<spdlog::sinks::sink> make_log_sink(const std::filesystem::path& logFolder,
                                                   const std::size_t keptPreviousLogs) noexcept {
    if (auto sink{make_file_sink(logFolder, keptPreviousLogs)}; sink != nullptr) {
        return sink;
    }

    return make_fallback_sink();
}

bool set_log_folder(std::filesystem::path logFolder, const std::size_t keptPreviousLogs) noexcept {
    auto& state{get_shared_sink_state()};
    const std::lock_guard lock{state.mutex};
    if (state.target != nullptr) {
        return false;
    }

    state.logFolder = std::move(logFolder);
    state.keptPreviousLogs = keptPreviousLogs;
    return true;
}

std::shared_ptr<spdlog::sinks::sink> get_log_file_sink() {
    auto& state{get_shared_sink_state()};
    const std::lock_guard lock{state.mutex};
    return open_shared_sink(state);
}

std::filesystem::path get_log_folder() {
    auto& state{get_shared_sink_state()};
    const std::lock_guard lock{state.mutex};
    open_shared_sink(state);
    return state.logFolder;
}

std::shared_ptr<spdlog::logger> get_logger(const std::string_view name) {
    auto& state{get_logger_registry()};
    const std::lock_guard lock{state.mutex};

    const std::string loggerName{name};
    if (const auto found{state.loggers.find(loggerName)}; found != state.loggers.end()) {
        return found->second;
    }

    // The shared sink as it is, without opening the log file: the folder may not be named yet.
    auto logger{std::make_shared<spdlog::logger>(loggerName, get_shared_sink_state().sink)};
    logger->set_level(state.level);
    logger->flush_on(FLUSH_LEVEL);

    try {
        spdlog::register_logger(logger);
    } catch (const spdlog::spdlog_ex&) {
        // Something outside the engine already registered a logger with this name with spdlog.
        // The engine logger still works, spdlog::get() just doesn't see it.
    }

    state.loggers.emplace(loggerName, logger);
    return logger;
}

void set_log_level(const spdlog::level::level_enum level) noexcept {
    auto& state{get_logger_registry()};
    const std::lock_guard lock{state.mutex};

    state.level = level;
    for (const auto& [name, logger] : state.loggers) {
        logger->set_level(level);
    }
}

spdlog::level::level_enum get_log_level() noexcept {
    auto& state{get_logger_registry()};
    const std::lock_guard lock{state.mutex};
    return state.level;
}

std::optional<spdlog::level::level_enum> parse_log_level(const std::string_view text) noexcept {
    constexpr std::array<std::pair<std::string_view, spdlog::level::level_enum>, 8> LEVEL_NAMES{
        {
         {"trace", spdlog::level::trace},
         {"debug", spdlog::level::debug},
         {"info", spdlog::level::info},
         {"warn", spdlog::level::warn},
         {"warning", spdlog::level::warn},
         {"error", spdlog::level::err},
         {"critical", spdlog::level::critical},
         {"off", spdlog::level::off},
         }
    };

    const auto equalsIgnoringCase{[text](const std::string_view levelName) {
        return std::ranges::equal(text, levelName, [](const char left, const char right) {
            return std::tolower(static_cast<unsigned char>(left)) ==
                   std::tolower(static_cast<unsigned char>(right));
        });
    }};

    for (const auto& [levelName, level] : LEVEL_NAMES) {
        if (equalsIgnoringCase(levelName)) {
            return level;
        }
    }

    return std::nullopt;
}

bool add_sink(std::shared_ptr<spdlog::sinks::sink> sink) {
    if (sink == nullptr) {
        return false;
    }

    auto& state{get_shared_sink_state()};
    const std::lock_guard lock{state.mutex};
    if (std::ranges::find(state.addedSinks, sink) != state.addedSinks.end()) {
        return false;
    }

    state.sink->get_sinks().add_sink(sink);
    state.addedSinks.push_back(std::move(sink));
    return true;
}

bool remove_sink(const std::shared_ptr<spdlog::sinks::sink>& sink) {
    auto& state{get_shared_sink_state()};
    const std::lock_guard lock{state.mutex};

    const auto found{std::ranges::find(state.addedSinks, sink)};
    if (sink == nullptr || found == state.addedSinks.end()) {
        return false;
    }

    state.sink->get_sinks().remove_sink(sink);
    state.addedSinks.erase(found);
    return true;
}

} // namespace tca::engine::core::logging
