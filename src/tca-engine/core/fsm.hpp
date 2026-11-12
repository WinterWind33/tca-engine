// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <utility>
#include <variant>

// Third-party
#include <gsl/gsl>

namespace tca::engine::core {

namespace base_fsm_events {
/**
 * @brief Base FSM state event called when entering a state.
 * @note In the on_enter transition, the return state is ignored.
 */
struct on_enter {};

/**
 * @brief Base FSM state event called when exiting a state.
 * @note In the on_exit transition, the return state is ignored.
 */
struct on_exit {};
} // namespace base_fsm_events

/**
 * @brief Helper alias template to define the event type of an FSM. It includes the base FSM events
 * (on_enter and on_exit) and any additional events specified by the user.
 *
 * @tparam events Additional events to be included in the FSM event type.
 */
template <typename... events>
using fsm_event = std::variant<base_fsm_events::on_enter, base_fsm_events::on_exit, events...>;

/**
 * @brief A simple and efficient finite state machine (FSM) implementation. The FSM is defined by an
 * owner class and a set of events. The owner class must define the state handler functions, which
 * are member functions that take an event as a parameter and return the next state. The FSM handles
 * state transitions and ensures that the on_enter and on_exit events are properly called during
 * state transitions.
 *
 * @note This implementation was adapted from ChefFSM. See:
 * https://gitlab.com/libchef/chef-fsm/-/blob/main/include/ChefFsm/StateMachine.hh
 *
 * @tparam owner_type The type of the owner class that will use the FSM and define the state handler
 * functions.
 * @tparam events Additional events that the FSM will handle, in addition to the base FSM events
 * (on_enter and on_exit).
 */
template <class owner_type, typename... events>
class fsm {
public:
    /**
     * @brief The type of the events that the FSM will handle. It includes the base FSM events
     * (on_enter and on_exit) and any additional events specified by the user in the events...
     * template parameter.
     */
    using event_t = fsm_event<events...>;

    /**
     * @brief The state type of the FSM. Its implementation is a pointer to a member function of the
     * owner class that takes an event as a parameter and returns the next state. The state type
     * also includes a default constructor that creates a null state.
     */
    class state {
    public:
        /**
         * @brief Type of the implementation of the state, which is a pointer to a member function
         * of the owner class that takes an event as a parameter and returns the next state.
         */
        using impl_t = state (owner_type::*)(const event_t&);

        /**
         * @brief Constructs a null state by default.
         */
        constexpr state() = default;

        /**
         * @brief Constructs a state with the given implementation. This is willingly implicit to
         * allow for more concise state definitions in the owner class.
         */
        constexpr state(impl_t impl) noexcept : m_impl{impl} {}

        /**
         * @brief Default comparison operator.
         *
         * @param other The state to compare with.
         * @return true if the states are equal, false otherwise.
         */
        constexpr bool operator==(const state& other) const noexcept = default;

        /**
         * @brief Checks if the state is a null state, i.e., it has no implementation. A null state
         * is used to indicate that there is no state transition or that the FSM should stay in the
         * current state.
         *
         * @return true if the state is a null state, false otherwise.
         */
        constexpr bool is_null() const noexcept {
            return m_impl == nullptr;
        }

        /**
         * @brief Invokes the state handler function of the owner class with the given event and
         * returns the next state. This is the core of the FSM, where the state handler function is
         * called to determine the next state based on the current state and the event.
         *
         * @param owner The owner of the FSM.
         * @param event The event to be processed.
         * @return state The next state of the FSM.
         */
        state operator()(owner_type& owner, const event_t& event) const;

    private:
        impl_t m_impl{nullptr};
    };

    /**
     * @brief A special state implementation that indicates that the FSM should stay in the current
     * state. This is used as a return value for state handler functions to indicate that there is
     * no state transition and that the FSM should remain in the current state.
     */
    static constexpr state::impl_t stay{};

    /**
     * @brief Construct a new fsm object with the given owner and initial state.
     *
     * @note The on_enter event of the initial state will be automatically called during
     * construction if the initial state is not a null state.
     *
     * @param owner The owner of the FSM.
     * @param initialState The initial state of the FSM.
     */
    explicit fsm(gsl::not_null<owner_type*> owner, state initialState);

    /**
     * @brief Feeds an event to the FSM, causing a state transition if applicable.
     *
     * @note If the state handler function of the current state returns a new state that is
     * different from the current state, the FSM will perform a state transition by calling the
     * on_exit event of the current state, updating the current state to the new state, and then
     * calling the on_enter event of the new state. The return value of the on_enter and on_exit
     * events is ignored, so they can return either a new state or the stay state without affecting
     * the FSM behavior.
     *
     * @tparam event_type The type of the event to be fed to the FSM.
     * @param event The event to be processed by the FSM.
     */
    template <typename event_type>
    void feed(event_type&& event);

private:
    gsl::not_null<owner_type*> m_owner;
    state m_currentState{};
};

// Implementations

template <typename owner_type, typename... events>
auto fsm<owner_type, events...>::state::operator()(owner_type& owner, const event_t& event) const
    -> state {
    Expects(m_impl != nullptr);
    return (owner.*m_impl)(event);
}

template <typename owner_type, typename... events>
fsm<owner_type, events...>::fsm(const gsl::not_null<owner_type*> owner, state initialState)
    : m_owner{owner},
      m_currentState{std::move(initialState)} {
    if (!m_currentState.is_null()) {
        m_currentState(*m_owner, base_fsm_events::on_enter{});
    }
}

template <typename owner_type, typename... events>
template <typename event_type>
void fsm<owner_type, events...>::feed(event_type&& event) {
    Expects(!m_currentState.is_null());
    auto newState = m_currentState(*m_owner, std::forward<event_type>(event));
    if (!newState.is_null() && newState != m_currentState) {
        m_currentState(*m_owner, base_fsm_events::on_exit{});
        m_currentState = std::move(newState);
        m_currentState(*m_owner, base_fsm_events::on_enter{});
    }
}

/**
 * @brief Helper struct to create an overloaded set of call operators from a list of handler types.
 * This is useful for handling multiple events in a state handler function using std::visit with a
 * std::variant event type. By inheriting from multiple handler types, we can define a call operator
 * for each event type, and the overloaded struct will automatically dispatch the call to the
 * correct handler based on the event type.
 *
 * @tparam handlers The list of handler types to be combined into an overloaded set of call
 * operators.
 */
template <typename... handlers>
struct overloaded : handlers... {
    using handlers::operator()...;
};

// No need for CTAD since we are using C++20.

} // namespace tca::engine::core
