// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/win32/win32-application-window.hpp"

// C++ STL
#include <algorithm>
#include <memory>
#include <tuple> // for std::ignore
#include <type_traits>
#include <utility>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

#include "input/input-collector.hpp"
#include "input/input-frame.hpp"

#include "utilities/win32/wm-quit-drain.hpp"

/**
 * These tests will create windows in the system. Be careful when running them.
 *
 * The window procedure finds the object a message belongs to through the native window's
 * GWLP_USERDATA. Moving a window object must therefore update GWLP_USERDATA to the new object, or
 * messages keep going to the moved-from one (freed memory, once it is destroyed).
 */
namespace {

namespace epn = tca::engine::platform;
namespace epw32n = epn::win32;
namespace ein = tca::engine::input;

/**
 * @brief A test window that can:
 * - record the HWND of every message routed into it, in a list living outside the object (so the
 *   list survives the object being moved or destroyed);
 * - receive a character without going through a native window, which is the only way to talk to a
 *   moved-from object.
 */
class probe_window final : public epw32n::win32_application_window {
public:
    explicit probe_window(std::vector<HWND>* const routedHwnds = nullptr)
        : win32_application_window(TEXT("Move test window")),
          m_routedHwnds{routedHwnds} {}

    void receive_char_directly(const char16_t codeUnit) noexcept {
        std::ignore = win32_application_window::routed_window_procedure(
            nullptr, epw32n::windows_messages::character, static_cast<WPARAM>(codeUnit), 1);
    }

protected:
    route_result_t routed_window_procedure(
        const HWND hwnd, const epw32n::windows_messages message,
        const epw32n::details::wparam_t wparam,
        const epw32n::details::lparam_t lparam) noexcept override {
        if (m_routedHwnds != nullptr) {
            m_routedHwnds->push_back(hwnd);
        }
        return win32_application_window::routed_window_procedure(hwnd, message, wparam, lparam);
    }

private:
    std::vector<HWND>* m_routedHwnds{};
};

[[nodiscard]] std::unique_ptr<probe_window> make_window(
    std::vector<HWND>* const routedHwnds = nullptr) {
    auto window{
        epw32n::win32_application_window::two_phase_window_creation<probe_window>(routedHwnds)};
    REQUIRE(window->get_native_handle().get() != nullptr);
    return window;
}

[[nodiscard]] HWND hwnd_of(const epw32n::win32_application_window& window) noexcept {
    return window.get_native_handle().get();
}

// The object the window procedure will route this window's messages to.
[[nodiscard]] const epw32n::win32_application_window* user_data_of(const HWND hwnd) noexcept {
    return reinterpret_cast<const epw32n::win32_application_window*>(
        ::GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

} // namespace

TEST_CASE("win32_application_window move - the move operations are noexcept",
          "[tca][engine][platform][win32][win32_application_window][move]") {
    STATIC_REQUIRE(std::is_nothrow_move_constructible_v<epw32n::win32_application_window>);
    STATIC_REQUIRE(std::is_nothrow_move_assignable_v<epw32n::win32_application_window>);
}

TEST_CASE("win32_application_window move construction - the window's messages go to the new object",
          "[tca][engine][platform][win32][win32_application_window][move][input]") {
    tests::utilities::drain_wm_quit_messages();

    ein::input_frame frame{};
    ein::input_collector collector{frame};

    auto source{make_window()};
    source->set_input_collector(&collector);
    const HWND hwnd{hwnd_of(*source)};

    const probe_window target{std::move(*source)};
    // From here on, a message routed to the source would reach freed memory.
    source.reset();

    REQUIRE(hwnd_of(target) == hwnd);
    REQUIRE(user_data_of(hwnd) == &target);

    // The input collector came along too.
    ::SendMessage(hwnd, WM_CHAR, L'a', 1);
    REQUIRE(frame.text() == "a");
}

TEST_CASE("win32_application_window move assignment - the window's messages go to the new object",
          "[tca][engine][platform][win32][win32_application_window][move][input]") {
    tests::utilities::drain_wm_quit_messages();

    ein::input_frame frame{};
    ein::input_collector collector{frame};

    auto source{make_window()};
    source->set_input_collector(&collector);
    const HWND hwnd{hwnd_of(*source)};

    const auto target{make_window()};
    *target = std::move(*source);
    // From here on, a message routed to the source would reach freed memory.
    source.reset();

    REQUIRE(hwnd_of(*target) == hwnd);
    REQUIRE(user_data_of(hwnd) == target.get());

    // The input collector came along too.
    ::SendMessage(hwnd, WM_CHAR, L'a', 1);
    REQUIRE(frame.text() == "a");

    // Destroying the target's old window posted a WM_QUIT: leave the queue clean for the next test.
    tests::utilities::drain_wm_quit_messages();
}

TEST_CASE("win32_application_window move construction - the moved-from object is left empty",
          "[tca][engine][platform][win32][win32_application_window][move][input]") {
    tests::utilities::drain_wm_quit_messages();

    ein::input_frame frame{};
    ein::input_collector collector{frame};

    auto source{make_window()};
    source->set_input_collector(&collector);
    const HWND hwnd{hwnd_of(*source)};

    const probe_window target{std::move(*source)};

    // It has no native window anymore...
    REQUIRE(hwnd_of(*source) == nullptr);

    // ...and no input collector.
    source->receive_char_directly(u'x');
    REQUIRE(frame.text().empty());

    // Destroying it doesn't touch the window it gave away.
    source.reset();
    REQUIRE(::IsWindow(hwnd) != 0);
    REQUIRE(user_data_of(hwnd) == &target);
}

TEST_CASE("win32_application_window move assignment - the moved-from object is left empty",
          "[tca][engine][platform][win32][win32_application_window][move][input]") {
    tests::utilities::drain_wm_quit_messages();

    ein::input_frame frame{};
    ein::input_collector collector{frame};

    auto source{make_window()};
    source->set_input_collector(&collector);
    const HWND hwnd{hwnd_of(*source)};

    const auto target{make_window()};
    *target = std::move(*source);

    // It has no native window anymore...
    REQUIRE(hwnd_of(*source) == nullptr);

    // ...and no input collector.
    source->receive_char_directly(u'x');
    REQUIRE(frame.text().empty());

    // Destroying it doesn't touch the window it gave away.
    source.reset();
    REQUIRE(::IsWindow(hwnd) != 0);
    REQUIRE(user_data_of(hwnd) == target.get());

    // Destroying the target's old window posted a WM_QUIT: leave the queue clean for the next test.
    tests::utilities::drain_wm_quit_messages();
}

TEST_CASE("win32_application_window move - assignment destroys the target's old window quietly",
          "[tca][engine][platform][win32][win32_application_window][move]") {
    tests::utilities::drain_wm_quit_messages();

    std::vector<HWND> routedHwnds{};
    auto source{make_window(&routedHwnds)};
    auto target{make_window(&routedHwnds)};
    const HWND oldTargetHwnd{hwnd_of(*target)};

    routedHwnds.clear();
    *target = std::move(*source);

    REQUIRE(::IsWindow(oldTargetHwnd) == 0);

    // The old window is destroyed while the target is halfway through being overwritten: its
    // WM_DESTROY, WM_NCDESTROY, ... must not be routed into the target.
    REQUIRE(std::ranges::count(routedHwnds, oldTargetHwnd) == 0);

    // Destroying the old window posted a WM_QUIT: leave the queue clean for the next test.
    tests::utilities::drain_wm_quit_messages();
}

TEST_CASE("win32_application_window move construction - from a window that was never created",
          "[tca][engine][platform][win32][win32_application_window][move]") {
    tests::utilities::drain_wm_quit_messages();

    probe_window source{};
    REQUIRE(hwnd_of(source) == nullptr);

    const probe_window target{std::move(source)};

    REQUIRE(hwnd_of(target) == nullptr);
    REQUIRE(hwnd_of(source) == nullptr);
}

TEST_CASE("win32_application_window move assignment - from a window that was never created",
          "[tca][engine][platform][win32][win32_application_window][move]") {
    tests::utilities::drain_wm_quit_messages();

    probe_window source{};
    REQUIRE(hwnd_of(source) == nullptr);

    const auto target{make_window()};
    const HWND oldTargetHwnd{hwnd_of(*target)};

    *target = std::move(source);

    REQUIRE(hwnd_of(*target) == nullptr);
    REQUIRE(::IsWindow(oldTargetHwnd) == 0);

    // Destroying the target's old window posted a WM_QUIT: leave the queue clean for the next test.
    tests::utilities::drain_wm_quit_messages();
}

TEST_CASE("win32_application_window move - self move-assignment keeps the window",
          "[tca][engine][platform][win32][win32_application_window][move]") {
    tests::utilities::drain_wm_quit_messages();

    const auto window{make_window()};
    const HWND hwnd{hwnd_of(*window)};

    // Through a reference, so the compiler's self-move warning doesn't fire on purpose-written
    // code.
    probe_window& alias{*window};
    *window = std::move(alias);

    REQUIRE(hwnd_of(*window) == hwnd);
    REQUIRE(::IsWindow(hwnd) != 0);
    REQUIRE(user_data_of(hwnd) == window.get());
}
