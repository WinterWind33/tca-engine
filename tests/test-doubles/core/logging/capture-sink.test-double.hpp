// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// Third-party
#include <spdlog/sinks/base_sink.h>

#include "core/logging/logging.hpp"

namespace tca::engine::test_doubles::core::logging {

/**
 * @brief A sink keeping every message it receives in memory, and counting its flushes, so a test
 * can check what an engine logger wrote and when it flushed.
 */
class capture_sink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    struct captured_message {
        std::string loggerName;
        spdlog::level::level_enum level;
        std::string text;
    };

    [[nodiscard]] std::vector<captured_message> messages() {
        const std::lock_guard lock{mutex_};
        return m_messages;
    }

    [[nodiscard]] bool contains(const std::string& text) {
        const std::lock_guard lock{mutex_};
        for (const auto& message : m_messages) {
            if (message.text.find(text) != std::string::npos) {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] std::size_t flush_count() {
        const std::lock_guard lock{mutex_};
        return m_flushCount;
    }

protected:
    void sink_it_(const spdlog::details::log_msg& message) override {
        m_messages.push_back({
            std::string{message.logger_name.data(), message.logger_name.size()},
            message.level,
            std::string{message.payload.data(),     message.payload.size()    }
        });
    }

    void flush_() override {
        ++m_flushCount;
    }

private:
    std::vector<captured_message> m_messages{};
    std::size_t m_flushCount{0};
};

/**
 * @brief Adds a capture_sink to every engine logger for the lifetime of the object.
 */
class scoped_capture_sink final {
public:
    scoped_capture_sink() {
        tca::engine::core::logging::add_sink(m_sink);
    }

    ~scoped_capture_sink() {
        tca::engine::core::logging::remove_sink(m_sink);
    }

    scoped_capture_sink(const scoped_capture_sink&) = delete;
    scoped_capture_sink& operator=(const scoped_capture_sink&) = delete;

    capture_sink& operator*() const noexcept {
        return *m_sink;
    }

    capture_sink* operator->() const noexcept {
        return m_sink.get();
    }

    [[nodiscard]] const std::shared_ptr<capture_sink>& get() const noexcept {
        return m_sink;
    }

private:
    std::shared_ptr<capture_sink> m_sink{std::make_shared<capture_sink>()};
};

/**
 * @brief Sets the engine log level for the lifetime of the object, restoring the previous one
 * afterwards so other tests see the level they started with.
 */
class scoped_log_level final {
public:
    explicit scoped_log_level(const spdlog::level::level_enum level)
        : m_previous{tca::engine::core::logging::get_log_level()} {
        tca::engine::core::logging::set_log_level(level);
    }

    ~scoped_log_level() {
        tca::engine::core::logging::set_log_level(m_previous);
    }

    scoped_log_level(const scoped_log_level&) = delete;
    scoped_log_level& operator=(const scoped_log_level&) = delete;

private:
    spdlog::level::level_enum m_previous;
};

} // namespace tca::engine::test_doubles::core::logging
