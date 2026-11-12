// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstddef> // for std::size_t
#include <cstdint>
#include <functional>
#include <vector>

namespace tca::engine::core {

/**
 * @brief A simple event system that allows to subscribe handlers to events and execute them when
 * the event is triggered. This particular implementation of the event system decouples the event
 * execution from the subscription, allowing for more flexibility in how events are triggered and
 * handled. This way, users can separate the logic that triggers the event from the logic that
 * handles the event, which can prevent issues such as non-authored code triggering events that
 * should only be triggered by specific parts of the code.
 *
 * @tparam EventArgs Params that will be passed to the handlers when the event is executed.
 */
template <typename... EventArgs>
class event {
public:
    /**
     * @brief Subscriber class that allows to subscribe handlers to the event and unsubscribe them
     * later if needed. The subscribe() method returns a handler index that can be used to
     * unsubscribe the handler later using the unsubscribe() method.
     */
    class subscriber {
        friend class event<EventArgs...>;

    public:
        /**
         * @brief Type of the handler ID returned by the subscribe() method, which can be used to
         * unsubscribe the handler later using the unsubscribe() method.
         */
        using handler_id_t = std::size_t;

        /**
         * @brief Subscribes a handler to the event. The handler is a function that takes the event
         * arguments as parameters and returns void. The handler will be called when the event is
         * executed, and it will receive the event arguments as parameters. Returns a handler ID
         * that can be used to unsubscribe the handler later using the unsubscribe() method.
         *
         * @param handler The handler to subscribe to the event.
         * @return handler_id_t A handler ID that can be used to unsubscribe the handler later using
         * the unsubscribe() method.
         */
        handler_id_t subscribe(std::function<void(EventArgs...)>&& handler) {
            m_handlers.push_back(std::move(handler));
            // Return the index of the newly added handler, so that
            // callers can use it to unsubscribe the handler later if needed.
            return m_handlers.size() - 1;
        }

        /**
         * @brief Unsubscribes a handler from the event using the handler ID returned by the
         * subscribe() method.
         *
         * @param handlerIndex The ID of the handler to unsubscribe.
         */
        void unsubscribe(const handler_id_t handlerIndex) noexcept {
            if (handlerIndex < m_handlers.size()) {
                // Right now we can't remove the handler from the vector, since that would change
                // the indices of all the handlers after the removed one, which would make it
                // impossible to unsubscribe them later using their indices. In the future we can
                // consider using a different data structure for the handlers.
                m_handlers[handlerIndex] = nullptr;
            }
        }

    private:
        std::vector<std::function<void(EventArgs...)>> m_handlers{};
    };

    /**
     * @brief Executes the event by calling all the subscribed handlers with the provided event
     * arguments. The event arguments are perfectly forwarded to the handlers, allowing for any type
     * of event arguments to be used with this event system.
     *
     * @tparam Args The types of the event arguments to pass to the handlers when executing the
     * event.
     * @param args The event arguments to pass to the handlers when executing the event.
     */
    template <typename... Args>
    void execute(Args&&... args) {
        for (const auto& handler : m_subscriber.m_handlers) {
            if (handler) {
                handler(std::forward<Args>(args)...);
            }
        }
    }

    /**
     * @brief Clears all the subscribed handlers from the event.
     */
    void clear() noexcept {
        m_subscriber.m_handlers.clear();
    }

    /**
     * @brief Returns a reference to the subscriber object associated with this event.
     *
     * @return subscriber& A reference to the subscriber object.
     */
    subscriber& get_subscriber() noexcept {
        return m_subscriber;
    }

private:
    subscriber m_subscriber{};
};

} // namespace tca::engine::core
