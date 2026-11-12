// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <format>
#include <optional>
#include <string>

// Third-party
#include <catch2/catch_tostring.hpp>

// Engine
#include "input/mouse-state.hpp"

// How Catch2 prints the mouse's value types in failure messages, instead of "{?}". Every test file
// that compares one of them in an assertion must include this: a file that doesn't instantiates
// Catch2's default printer for the same type, and the linker may keep that one for the whole
// binary.

template <>
struct Catch::StringMaker<tca::engine::input::pointer_position> {
    static std::string convert(const tca::engine::input::pointer_position position) {
        return std::format("{{x = {}, y = {}}}", position.x, position.y);
    }
};

template <>
struct Catch::StringMaker<std::optional<tca::engine::input::pointer_position>> {
    static std::string convert(const std::optional<tca::engine::input::pointer_position> position) {
        return position.has_value()
                   ? StringMaker<tca::engine::input::pointer_position>::convert(*position)
                   : std::string{"nullopt"};
    }
};

template <>
struct Catch::StringMaker<tca::engine::input::wheel_delta> {
    static std::string convert(const tca::engine::input::wheel_delta delta) {
        return std::format("{{vertical = {}, horizontal = {}}}", delta.vertical, delta.horizontal);
    }
};
