// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/tickables.hpp"

#include "input/input-frame.hpp"
#include "input/key.hpp"

#include "utilities/update-context.hpp"

// C++ STL
#include <chrono>
#include <concepts>
#include <functional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <gsl/gsl>

namespace {

class renderable_fake : public tca::engine::rendering::renderable {
public:
    void render() override {
        m_rendered = true;
    }

    bool is_rendered() const noexcept {
        return m_rendered;
    }

private:
    bool m_rendered{};
};

class renderable_view_tss : public tca::engine::rendering::renderable_view {
public:
    explicit renderable_view_tss(std::function<void(renderable&)> onRenderableAddedCallback = {},
                                 std::function<void(renderable&)> onRenderableRemovingCallback = {})
        : m_onRenderableAddedCallback(std::move(onRenderableAddedCallback)),
          m_onRenderableRemovingCallback(std::move(onRenderableRemovingCallback)) {}

    auto get_renderables() const noexcept {
        return std::views::all(m_renderables);
    }

private:
    std::function<void(renderable&)> m_onRenderableAddedCallback{};
    std::function<void(renderable&)> m_onRenderableRemovingCallback{};

    void on_renderable_added(renderable& renderable) override {
        renderable_view::on_renderable_added(renderable);
        if (m_onRenderableAddedCallback) {
            m_onRenderableAddedCallback(renderable);
        }
    }

    void on_renderable_removing(renderable& renderable) override {
        renderable_view::on_renderable_removing(renderable);
        if (m_onRenderableRemovingCallback) {
            m_onRenderableRemovingCallback(renderable);
        }
    }
};

class updatable_fake : public tca::engine::rendering::updatable {
public:
    void update(tca::engine::rendering::update_context& context) override {
        m_deltaTime = context.delta();
        m_updated = true;
    }

    bool is_updated() const noexcept {
        return m_updated;
    }

    delta_time get_delta_time() const noexcept {
        return m_deltaTime;
    }

private:
    delta_time m_deltaTime{};
    bool m_updated{};
};

class updatable_view_tss : public tca::engine::rendering::updatable_view {
public:
    explicit updatable_view_tss(std::function<void(updatable&)> onUpdatableAddedCallback = {},
                                std::function<void(updatable&)> onUpdatableRemovingCallback = {})
        : m_onUpdatableAddedCallback(std::move(onUpdatableAddedCallback)),
          m_onUpdatableRemovingCallback(std::move(onUpdatableRemovingCallback)) {}

    auto get_updatables() const noexcept {
        return std::views::all(m_updatables);
    }

private:
    std::function<void(updatable&)> m_onUpdatableAddedCallback{};
    std::function<void(updatable&)> m_onUpdatableRemovingCallback{};

    void on_updatable_added(updatable& updatable) override {
        updatable_view::on_updatable_added(updatable);
        if (m_onUpdatableAddedCallback) {
            m_onUpdatableAddedCallback(updatable);
        }
    }

    void on_updatable_removing(updatable& updatable) override {
        updatable_view::on_updatable_removing(updatable);
        if (m_onUpdatableRemovingCallback) {
            m_onUpdatableRemovingCallback(updatable);
        }
    }
};

class pausable_renderables_view_tss : public tca::engine::rendering::pausable_renderables_view {
public:
    auto get_renderables() const noexcept {
        return std::views::all(m_renderables);
    }
};

class pausable_updatables_view_tss : public tca::engine::rendering::pausable_updatables_view {
public:
    auto get_updatables() const noexcept {
        return std::views::all(m_updatables);
    }
};

using call_log = std::vector<std::string>;

/**
 * @brief An updatable that records, for every update() it receives, its name in a shared log plus
 * the context it was handed and what that context's input showed for one key at that moment.
 * Optionally consumes that key (or the whole input) once it has looked at it.
 */
class context_spy : public tca::engine::rendering::updatable {
public:
    enum class consumption {
        none,
        watched_key,
        everything
    };

    context_spy(call_log& log, std::string name,
                const tca::engine::input::key watchedKey = tca::engine::input::key::space,
                const consumption consumes = consumption::none)
        : m_log{&log},
          m_name{std::move(name)},
          m_watchedKey{watchedKey},
          m_consumes{consumes} {}

    void update(tca::engine::rendering::update_context& context) override {
        m_log->push_back(m_name);
        ++m_updateCount;
        m_lastContext = &context;
        m_lastDelta = context.delta();

        const auto input{context.input()};
        m_sawWatchedKeyDown = input.keyboard().is_down(m_watchedKey);
        m_sawWatchedKeyPressed = input.keyboard().was_pressed(m_watchedKey);
        m_sawText = std::string{input.text()};

        switch (m_consumes) {
            case consumption::watched_key:
                input.consume(m_watchedKey);
                break;
            case consumption::everything:
                input.consume_all();
                break;
            case consumption::none:
                break;
        }
    }

    [[nodiscard]] int update_count() const noexcept {
        return m_updateCount;
    }

    [[nodiscard]] const tca::engine::rendering::update_context* last_context() const noexcept {
        return m_lastContext;
    }

    [[nodiscard]] delta_time last_delta() const noexcept {
        return m_lastDelta;
    }

    [[nodiscard]] bool saw_watched_key_down() const noexcept {
        return m_sawWatchedKeyDown;
    }

    [[nodiscard]] bool saw_watched_key_pressed() const noexcept {
        return m_sawWatchedKeyPressed;
    }

    [[nodiscard]] const std::string& saw_text() const noexcept {
        return m_sawText;
    }

private:
    call_log* m_log{};
    std::string m_name{};
    tca::engine::input::key m_watchedKey{};
    consumption m_consumes{};

    int m_updateCount{};
    const tca::engine::rendering::update_context* m_lastContext{};
    delta_time m_lastDelta{};
    bool m_sawWatchedKeyDown{};
    bool m_sawWatchedKeyPressed{};
    std::string m_sawText{};
};

} // namespace

TEST_CASE("renderable_view class tests", "[tca][engine][rendering][tickables][renderable_view]") {
    using namespace tca::engine::rendering;

    GIVEN("A renderable view") {
        bool onRenderableAddedCalled{};
        bool onRenderableRemovingCalled{};
        renderable_view_tss viewUnderTest{[&onRenderableAddedCalled](renderable&) {
                                              onRenderableAddedCalled = true;
                                          },
                                          [&onRenderableRemovingCalled](renderable&) {
                                              onRenderableRemovingCalled = true;
                                          }};

        THEN("The renderables list should be empty") {
            REQUIRE(viewUnderTest.get_renderables().empty());
        }

        WHEN("Adding a new renderable to the view") {
            renderable_fake renderable{};
            REQUIRE_NOTHROW(viewUnderTest.add_renderable(renderable));

            THEN("The renderables list should contain the added renderable") {
                REQUIRE(viewUnderTest.get_renderables().size() == 1);
                REQUIRE(viewUnderTest.get_renderables().front().get() == &renderable);
            }

            THEN("The on_renderable_added callback should be called") {
                REQUIRE(onRenderableAddedCalled);
                REQUIRE_FALSE(onRenderableRemovingCalled);
            }

            WHEN("Removing the renderable from the view") {
                REQUIRE_NOTHROW(viewUnderTest.remove_renderable(renderable));

                THEN("The renderables list should be empty") {
                    REQUIRE(viewUnderTest.get_renderables().empty());
                }

                THEN("The on_renderable_removing callback should be called") {
                    REQUIRE(onRenderableRemovingCalled);
                }
            }

            AND_WHEN("Rendering the view") {
                REQUIRE_NOTHROW(viewUnderTest.render());

                THEN("The renderable's render method should be called") {
                    REQUIRE(renderable.is_rendered());
                }
            }

            AND_WHEN("Adding the same renderable to the view multiple times") {
                REQUIRE_NOTHROW(viewUnderTest.add_renderable(renderable));

                THEN("The renderable should be added only once to the view") {
                    REQUIRE(viewUnderTest.get_renderables().size() == 1);
                    REQUIRE(viewUnderTest.get_renderables().front().get() == &renderable);
                }
            }

            AND_WHEN("Trying to remove a renderable that is not in the view") {
                renderable_fake anotherRenderable{};
                REQUIRE_NOTHROW(viewUnderTest.remove_renderable(anotherRenderable));

                THEN("The renderables list should still contain the original renderable") {
                    REQUIRE(viewUnderTest.get_renderables().size() == 1);
                    REQUIRE(viewUnderTest.get_renderables().front().get() == &renderable);
                }

                THEN("The on_renderable_removing callback should not be called") {
                    REQUIRE_FALSE(onRenderableRemovingCalled);
                }
            }
        }
    }
}

TEST_CASE("updatable_view class tests", "[tca][engine][rendering][tickables][updatable_view]") {
    using namespace tca::engine::rendering;

    GIVEN("An updatable view") {
        bool onUpdatableAddedCalled{};
        bool onUpdatableRemovingCalled{};
        updatable_view_tss viewUnderTest{[&onUpdatableAddedCalled](updatable&) {
                                             onUpdatableAddedCalled = true;
                                         },
                                         [&onUpdatableRemovingCalled](updatable&) {
                                             onUpdatableRemovingCalled = true;
                                         }};

        THEN("The updatables list should be empty") {
            REQUIRE(viewUnderTest.get_updatables().empty());
        }

        WHEN("Adding a new updatable to the view") {
            updatable_fake updatable{};
            REQUIRE_NOTHROW(viewUnderTest.add_updatable(updatable));

            THEN("The updatables list should contain the added updatable") {
                REQUIRE(viewUnderTest.get_updatables().size() == 1);
                REQUIRE(viewUnderTest.get_updatables().front().get() == &updatable);
            }

            THEN("The on_updatable_added callback should be called") {
                REQUIRE(onUpdatableAddedCalled);
                REQUIRE_FALSE(onUpdatableRemovingCalled);
            }

            WHEN("Removing the updatable from the view") {
                REQUIRE_NOTHROW(viewUnderTest.remove_updatable(updatable));

                THEN("The updatables list should be empty") {
                    REQUIRE(viewUnderTest.get_updatables().empty());
                }

                THEN("The on_updatable_removing callback should be called") {
                    REQUIRE(onUpdatableRemovingCalled);
                }
            }

            AND_WHEN("Updating the view with a delta time of 100us") {
                constexpr updatable::delta_time DELTA{100};
                REQUIRE_NOTHROW(tca::tests::utilities::update_with_delta(viewUnderTest, DELTA));

                THEN(
                    "The updatable's update method should be called with the correct delta "
                    "time") {
                    REQUIRE(updatable.is_updated());
                    REQUIRE(updatable.get_delta_time() == DELTA);
                }
            }

            AND_WHEN("Adding the same updatable to the view multiple times") {
                REQUIRE_NOTHROW(viewUnderTest.add_updatable(updatable));

                THEN("The updatable should be added only once to the view") {
                    REQUIRE(viewUnderTest.get_updatables().size() == 1);
                    REQUIRE(viewUnderTest.get_updatables().front().get() == &updatable);
                }
            }

            AND_WHEN("Trying to remove an updatable that is not in the view") {
                updatable_fake anotherUpdatable{};
                REQUIRE_NOTHROW(viewUnderTest.remove_updatable(anotherUpdatable));

                THEN("The updatables list should still contain the original updatable") {
                    REQUIRE(viewUnderTest.get_updatables().size() == 1);
                    REQUIRE(viewUnderTest.get_updatables().front().get() == &updatable);
                }

                THEN("The on_updatable_removing callback should not be called") {
                    REQUIRE_FALSE(onUpdatableRemovingCalled);
                }
            }
        }
    }
}

TEST_CASE("pausable_renderables_view class tests",
          "[tca][engine][rendering][tickables][pausable_renderables_view]") {
    using namespace tca::engine::rendering;

    GIVEN("A pausable renderables view") {
        pausable_renderables_view_tss viewUnderTest{};

        THEN("The renderables list should be empty") {
            REQUIRE(viewUnderTest.get_renderables().empty());
        }

        WHEN("Adding a new renderable to the view") {
            renderable_fake renderable{};
            REQUIRE_NOTHROW(viewUnderTest.add_renderable(renderable));

            THEN("The renderables list should contain the added renderable") {
                REQUIRE(viewUnderTest.get_renderables().size() == 1);
                REQUIRE(viewUnderTest.get_renderables().front().get() == &renderable);
            }

            AND_WHEN("Pausing the renderable") {
                REQUIRE_NOTHROW(viewUnderTest.pause_renderable(renderable));

                THEN("The renderable should not be rendered when rendering the view") {
                    REQUIRE_NOTHROW(viewUnderTest.render());
                    REQUIRE_FALSE(renderable.is_rendered());
                }

                AND_WHEN("Resuming the renderable") {
                    REQUIRE_NOTHROW(viewUnderTest.resume_renderable(renderable));

                    THEN("The renderable should be rendered when rendering the view") {
                        REQUIRE_NOTHROW(viewUnderTest.render());
                        REQUIRE(renderable.is_rendered());
                    }
                }
            }
        }

        WHEN("Adding multiple renderables to the view") {
            renderable_fake renderable1{};
            renderable_fake renderable2{};
            REQUIRE_NOTHROW(viewUnderTest.add_renderable(renderable1));
            REQUIRE_NOTHROW(viewUnderTest.add_renderable(renderable2));

            THEN("The renderables list should contain the added renderables") {
                REQUIRE(viewUnderTest.get_renderables().size() == 2);
                REQUIRE(viewUnderTest.get_renderables().front().get() == &renderable1);
                REQUIRE(viewUnderTest.get_renderables().back().get() == &renderable2);
            }

            AND_WHEN("Pausing the first renderable") {
                REQUIRE_NOTHROW(viewUnderTest.pause_renderable(renderable1));

                THEN("Only the first renderable should be paused when rendering the view") {
                    REQUIRE_NOTHROW(viewUnderTest.render());
                    REQUIRE_FALSE(renderable1.is_rendered());
                    REQUIRE(renderable2.is_rendered());
                }
            }

            AND_WHEN("Removing the first renderable from the view") {
                REQUIRE_NOTHROW(viewUnderTest.remove_renderable(renderable1));

                THEN(
                    "The first renderable should be removed from the view and the second "
                    "should "
                    "still be rendered") {
                    REQUIRE_NOTHROW(viewUnderTest.render());
                    REQUIRE_FALSE(renderable1.is_rendered());
                    REQUIRE(renderable2.is_rendered());
                }
            }
        }

        WHEN("Trying to pause a renderable that is not in the view") {
            renderable_fake renderable{};

            THEN("It should throw an exception") {
                REQUIRE_THROWS_AS(viewUnderTest.pause_renderable(renderable), std::runtime_error);
            }
        }

        WHEN("Trying to resume a renderable that is not in the view") {
            renderable_fake renderable{};

            THEN("It should throw an exception") {
                REQUIRE_THROWS_AS(viewUnderTest.resume_renderable(renderable), std::runtime_error);
            }
        }

        WHEN("A renderable has been added to the view") {
            renderable_fake renderable{};
            viewUnderTest.add_renderable(renderable);

            THEN("is_renderable_paused() reports it as not paused") {
                REQUIRE_FALSE(viewUnderTest.is_renderable_paused(renderable));
            }

            AND_WHEN("The renderable is paused") {
                viewUnderTest.pause_renderable(renderable);

                THEN("is_renderable_paused() reports it as paused") {
                    REQUIRE(viewUnderTest.is_renderable_paused(renderable));
                }

                AND_WHEN("The renderable is resumed again") {
                    viewUnderTest.resume_renderable(renderable);

                    THEN("is_renderable_paused() reports it as not paused") {
                        REQUIRE_FALSE(viewUnderTest.is_renderable_paused(renderable));
                    }
                }
            }
        }

        WHEN("Querying is_renderable_paused() for a renderable that is not in the view") {
            renderable_fake renderable{};

            THEN("It should throw an exception") {
                REQUIRE_THROWS_AS(viewUnderTest.is_renderable_paused(renderable),
                                  std::runtime_error);
            }
        }
    }
}

TEST_CASE("pausable_updatables_view class tests",
          "[tca][engine][rendering][tickables][pausable_updatables_view]") {
    using namespace tca::engine::rendering;

    GIVEN("A pausable updatables view") {
        pausable_updatables_view_tss viewUnderTest{};

        THEN("The updatables list should be empty") {
            REQUIRE(viewUnderTest.get_updatables().empty());
        }

        WHEN("Adding a new updatable to the view") {
            updatable_fake updatable{};
            REQUIRE_NOTHROW(viewUnderTest.add_updatable(updatable));

            THEN("The updatables list should contain the added updatable") {
                REQUIRE(viewUnderTest.get_updatables().size() == 1);
                REQUIRE(viewUnderTest.get_updatables().front().get() == &updatable);
            }

            AND_WHEN("Pausing the updatable") {
                REQUIRE_NOTHROW(viewUnderTest.pause_updatable(updatable));

                THEN("The updatable should not be updated when updating the view") {
                    constexpr updatable::delta_time DELTA{100};
                    REQUIRE_NOTHROW(tca::tests::utilities::update_with_delta(viewUnderTest, DELTA));
                    REQUIRE_FALSE(updatable.is_updated());
                }

                AND_WHEN("Resuming the updatable") {
                    REQUIRE_NOTHROW(viewUnderTest.resume_updatable(updatable));

                    THEN("The updatable should be updated when updating the view") {
                        constexpr updatable::delta_time DELTA{100};
                        REQUIRE_NOTHROW(
                            tca::tests::utilities::update_with_delta(viewUnderTest, DELTA));
                        REQUIRE(updatable.is_updated());
                        REQUIRE(updatable.get_delta_time() == DELTA);
                    }
                }
            }

            WHEN("Trying to pause an updatable that is not in the view") {
                updatable_fake otherUpdatable{};

                THEN("It should throw an exception") {
                    REQUIRE_THROWS_AS(viewUnderTest.pause_updatable(otherUpdatable),
                                      std::runtime_error);
                }
            }

            WHEN("Trying to resume an updatable that is not in the view") {
                updatable_fake otherUpdatable{};

                THEN("It should throw an exception") {
                    REQUIRE_THROWS_AS(viewUnderTest.resume_updatable(otherUpdatable),
                                      std::runtime_error);
                }
            }
        }
    }
}

TEST_CASE("update_context type traits", "[tca][engine][rendering][tickables][update_context]") {
    using namespace tca::engine;
    using rendering::update_context;

    // A context always looks at a frame, so there is no empty one to default-construct.
    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<update_context>);
    STATIC_REQUIRE(std::is_nothrow_constructible_v<update_context, update_context::delta_time,
                                                   input::input_view>);
    STATIC_REQUIRE(std::same_as<update_context::delta_time, rendering::updatable::delta_time>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<const update_context&>().delta()),
                                update_context::delta_time>);

    // Updatables get a view of the frame, never the frame itself: they can't write input.
    STATIC_REQUIRE(
        std::same_as<decltype(std::declval<const update_context&>().input()), input::input_view>);

    // update() takes the context by reference, so every updatable of a pass gets the same one.
    STATIC_REQUIRE(std::is_invocable_v<decltype(&rendering::updatable::update),
                                       rendering::updatable&, update_context&>);
    STATIC_REQUIRE_FALSE(std::is_invocable_v<decltype(&rendering::updatable::update),
                                             rendering::updatable&, update_context::delta_time>);
}

TEST_CASE("update_context class tests", "[tca][engine][rendering][tickables][update_context]") {
    using namespace tca::engine;
    using rendering::update_context;
    using namespace std::chrono_literals;

    input::input_frame frame{};

    SECTION("delta() returns the delta it was created with") {
        for (const update_context::delta_time delta :
             {update_context::delta_time::zero(), update_context::delta_time{1},
              update_context::delta_time{16'667}, update_context::delta_time{3s}}) {
            const update_context context{delta, input::input_view{frame}};

            REQUIRE(context.delta() == delta);
        }
    }

    SECTION("input() shows the keys of the frame it was created with") {
        frame.keyboard().press(input::key::space);
        frame.keyboard().press(input::key::a);
        frame.keyboard().release(input::key::a);

        const update_context context{16ms, input::input_view{frame}};
        const auto keyboard{context.input().keyboard()};

        REQUIRE(keyboard.is_down(input::key::space));
        REQUIRE(keyboard.was_pressed(input::key::space));
        REQUIRE_FALSE(keyboard.is_down(input::key::a));
        REQUIRE(keyboard.was_pressed(input::key::a));
        REQUIRE(keyboard.was_released(input::key::a));
        REQUIRE_FALSE(keyboard.is_down(input::key::escape));
        REQUIRE_FALSE(keyboard.was_pressed(input::key::escape));
    }

    SECTION("input() shows the text typed in the frame it was created with") {
        frame.append_text("h\xC3\xA9");

        const update_context context{16ms, input::input_view{frame}};

        REQUIRE(context.input().text() == "h\xC3\xA9");
    }

    SECTION("input() looks at the frame rather than a copy of it") {
        const update_context context{16ms, input::input_view{frame}};

        frame.keyboard().press(input::key::escape);
        frame.append_text("x");

        REQUIRE(context.input().keyboard().was_pressed(input::key::escape));
        REQUIRE(context.input().text() == "x");
    }

    SECTION("Consuming a key through input() hides it on the frame") {
        frame.keyboard().press(input::key::space);
        const update_context context{16ms, input::input_view{frame}};

        context.input().consume(input::key::space);

        REQUIRE_FALSE(frame.keyboard().is_down(input::key::space));
        REQUIRE_FALSE(frame.keyboard().was_pressed(input::key::space));
        REQUIRE_FALSE(context.input().keyboard().is_down(input::key::space));
    }

    SECTION("Consuming everything through input() hides keys and text on the frame") {
        frame.keyboard().press(input::key::space);
        frame.append_text("a");
        const update_context context{16ms, input::input_view{frame}};

        context.input().consume_all();

        REQUIRE_FALSE(frame.keyboard().is_down(input::key::space));
        REQUIRE(frame.text().empty());
        REQUIRE(context.input().text().empty());
    }

    SECTION("Two contexts on the same frame see each other's consumption") {
        frame.keyboard().press(input::key::space);
        const update_context first{16ms, input::input_view{frame}};
        const update_context second{16ms, input::input_view{frame}};

        first.input().consume(input::key::space);

        REQUIRE_FALSE(second.input().keyboard().was_pressed(input::key::space));
    }

    SECTION("Contexts on different frames don't share consumption") {
        input::input_frame otherFrame{};
        frame.keyboard().press(input::key::space);
        otherFrame.keyboard().press(input::key::space);
        const update_context first{16ms, input::input_view{frame}};
        const update_context second{16ms, input::input_view{otherFrame}};

        first.input().consume(input::key::space);

        REQUIRE(second.input().keyboard().was_pressed(input::key::space));
    }
}

TEST_CASE("updatable_view passes its context on",
          "[tca][engine][rendering][tickables][updatable_view][update_context]") {
    using namespace tca::engine;
    using namespace std::chrono_literals;

    input::input_frame frame{};
    frame.keyboard().press(input::key::space);
    rendering::update_context context{16ms, input::input_view{frame}};
    call_log log{};

    GIVEN("An empty updatable view") {
        rendering::updatable_view view{};

        THEN("Updating it does nothing") {
            REQUIRE_NOTHROW(view.update(context));
        }
    }

    GIVEN("An updatable view with three updatables") {
        rendering::updatable_view view{};
        context_spy a{log, "A"};
        context_spy b{log, "B"};
        context_spy c{log, "C"};
        view.add_updatable(a);
        view.add_updatable(b);
        view.add_updatable(c);

        WHEN("The view is updated") {
            view.update(context);

            THEN("They are updated once each, in registration order") {
                const call_log expected{"A", "B", "C"};
                REQUIRE(log == expected);
            }

            THEN("Each of them is handed the very context the view got") {
                REQUIRE(a.last_context() == &context);
                REQUIRE(b.last_context() == &context);
                REQUIRE(c.last_context() == &context);
            }

            THEN("Each of them sees the context's delta and input") {
                for (const context_spy* spy : {&a, &b, &c}) {
                    REQUIRE(spy->last_delta() == rendering::update_context::delta_time{16ms});
                    REQUIRE(spy->saw_watched_key_pressed());
                }
            }
        }

        WHEN("One of them is removed and the view is updated") {
            view.remove_updatable(b);
            view.update(context);

            THEN("Only the others are updated, still in registration order") {
                const call_log expected{"A", "C"};
                REQUIRE(log == expected);
                REQUIRE(b.update_count() == 0);
            }
        }
    }

    GIVEN("A view nested inside another one") {
        rendering::updatable_view outer{};
        rendering::updatable_view inner{};
        context_spy before{log, "before"};
        context_spy nested{log, "nested"};
        context_spy after{log, "after"};
        inner.add_updatable(nested);
        outer.add_updatable(before);
        outer.add_updatable(inner);
        outer.add_updatable(after);

        WHEN("The outer view is updated") {
            outer.update(context);

            THEN("The nested updatable runs in its place in the order") {
                const call_log expected{"before", "nested", "after"};
                REQUIRE(log == expected);
            }

            THEN("The nested updatable is handed the same context") {
                REQUIRE(nested.last_context() == &context);
            }
        }
    }
}

TEST_CASE("pausable_updatables_view passes its context on",
          "[tca][engine][rendering][tickables][pausable_updatables_view][update_context]") {
    using namespace tca::engine;
    using namespace std::chrono_literals;

    input::input_frame frame{};
    frame.keyboard().press(input::key::space);
    rendering::update_context context{16ms, input::input_view{frame}};
    call_log log{};

    GIVEN("A pausable updatables view with three updatables") {
        rendering::pausable_updatables_view view{};
        context_spy a{log, "A"};
        context_spy b{log, "B"};
        context_spy c{log, "C"};
        view.add_updatable(a);
        view.add_updatable(b);
        view.add_updatable(c);

        WHEN("The view is updated") {
            view.update(context);

            THEN("They are updated once each, in registration order") {
                const call_log expected{"A", "B", "C"};
                REQUIRE(log == expected);
            }

            THEN("Each of them is handed the very context the view got") {
                REQUIRE(a.last_context() == &context);
                REQUIRE(b.last_context() == &context);
                REQUIRE(c.last_context() == &context);
                REQUIRE(b.last_delta() == rendering::update_context::delta_time{16ms});
            }
        }

        WHEN("The middle one is paused and the view is updated") {
            view.pause_updatable(b);
            view.update(context);

            THEN("The paused one receives nothing") {
                REQUIRE(b.update_count() == 0);
                REQUIRE(b.last_context() == nullptr);
            }

            THEN("The others still get the context, in registration order") {
                const call_log expected{"A", "C"};
                REQUIRE(log == expected);
                REQUIRE(a.last_context() == &context);
                REQUIRE(c.last_context() == &context);
            }

            AND_WHEN("It is resumed and the view is updated with a new context") {
                rendering::update_context nextContext{8ms, input::input_view{frame}};
                view.resume_updatable(b);
                view.update(nextContext);

                THEN("It gets the new context, in its place in the order") {
                    const call_log expected{"A", "C", "A", "B", "C"};
                    REQUIRE(log == expected);
                    REQUIRE(b.update_count() == 1);
                    REQUIRE(b.last_context() == &nextContext);
                    REQUIRE(b.last_delta() == rendering::update_context::delta_time{8ms});
                }
            }
        }

        WHEN("Every updatable is paused and the view is updated") {
            view.pause_updatable(a);
            view.pause_updatable(b);
            view.pause_updatable(c);
            view.update(context);

            THEN("None of them receives anything") {
                REQUIRE(log.empty());
            }
        }
    }
}

TEST_CASE("Input consumed by an updatable is hidden from the ones after it",
          "[tca][engine][rendering][tickables][updatable_view][pausable_updatables_view]"
          "[update_context]") {
    using namespace tca::engine;
    using namespace std::chrono_literals;
    using consumption = context_spy::consumption;

    input::input_frame frame{};
    frame.keyboard().press(input::key::space);
    frame.keyboard().press(input::key::escape);
    frame.append_text(" ");
    rendering::update_context context{16ms, input::input_view{frame}};
    call_log log{};

    GIVEN("An updatable view where the second updatable consumes space") {
        rendering::updatable_view view{};
        context_spy before{log, "before"};
        context_spy consumer{log, "consumer", input::key::space, consumption::watched_key};
        context_spy after{log, "after"};
        context_spy afterOnEscape{log, "after on escape", input::key::escape};
        view.add_updatable(before);
        view.add_updatable(consumer);
        view.add_updatable(after);
        view.add_updatable(afterOnEscape);

        WHEN("The view is updated") {
            view.update(context);

            THEN("Updatables before the consumer, and the consumer itself, see space") {
                REQUIRE(before.saw_watched_key_down());
                REQUIRE(before.saw_watched_key_pressed());
                REQUIRE(consumer.saw_watched_key_down());
                REQUIRE(consumer.saw_watched_key_pressed());
            }

            THEN("Updatables after the consumer see space as untouched") {
                REQUIRE_FALSE(after.saw_watched_key_down());
                REQUIRE_FALSE(after.saw_watched_key_pressed());
            }

            THEN("Keys the consumer didn't take are still seen after it") {
                REQUIRE(afterOnEscape.saw_watched_key_down());
                REQUIRE(afterOnEscape.saw_watched_key_pressed());
            }

            THEN("The text is still seen after it") {
                REQUIRE(after.saw_text() == " ");
            }

            THEN("The consumption reached the frame itself") {
                REQUIRE_FALSE(frame.keyboard().is_down(input::key::space));
                REQUIRE(frame.keyboard().is_down(input::key::escape));
            }
        }
    }

    GIVEN("An updatable view where the first updatable consumes everything") {
        rendering::updatable_view view{};
        context_spy consumer{log, "consumer", input::key::space, consumption::everything};
        context_spy after{log, "after", input::key::escape};
        view.add_updatable(consumer);
        view.add_updatable(after);

        WHEN("The view is updated") {
            view.update(context);

            THEN("The consumer saw the input") {
                REQUIRE(consumer.saw_watched_key_pressed());
                REQUIRE(consumer.saw_text() == " ");
            }

            THEN("Updatables after it see no keys and no text") {
                REQUIRE_FALSE(after.saw_watched_key_down());
                REQUIRE_FALSE(after.saw_watched_key_pressed());
                REQUIRE(after.saw_text().empty());
            }
        }
    }

    GIVEN("A consumer inside a nested view, followed by an updatable of the outer view") {
        rendering::updatable_view outer{};
        rendering::updatable_view inner{};
        context_spy consumer{log, "consumer", input::key::space, consumption::watched_key};
        context_spy after{log, "after"};
        inner.add_updatable(consumer);
        outer.add_updatable(inner);
        outer.add_updatable(after);

        WHEN("The outer view is updated") {
            outer.update(context);

            THEN("The updatable after the nested view sees space as untouched") {
                REQUIRE(consumer.saw_watched_key_pressed());
                REQUIRE_FALSE(after.saw_watched_key_pressed());
            }
        }
    }

    GIVEN("A pausable updatables view where the first updatable consumes space") {
        rendering::pausable_updatables_view view{};
        context_spy consumer{log, "consumer", input::key::space, consumption::watched_key};
        context_spy after{log, "after"};
        view.add_updatable(consumer);
        view.add_updatable(after);

        WHEN("The view is updated") {
            view.update(context);

            THEN("The updatable after it sees space as untouched") {
                REQUIRE_FALSE(after.saw_watched_key_down());
                REQUIRE_FALSE(after.saw_watched_key_pressed());
            }
        }

        WHEN("The consumer is paused and the view is updated") {
            view.pause_updatable(consumer);
            view.update(context);

            THEN("A paused consumer takes nothing: the updatable after it sees space") {
                REQUIRE(consumer.update_count() == 0);
                REQUIRE(after.saw_watched_key_down());
                REQUIRE(after.saw_watched_key_pressed());
            }
        }
    }
}
