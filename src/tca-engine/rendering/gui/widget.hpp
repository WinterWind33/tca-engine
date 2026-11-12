// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <concepts>

#include "rendering/tickables.hpp"

namespace tca::engine::widgets {

/**
 * @brief A widget must be renderable, but it doesn't need to be updatable, since it can be static
 * and not require any update logic.
 *
 * @tparam WidgetType The type of the widget to check.
 */
template <class WidgetType>
concept widget = std::derived_from<WidgetType, rendering::renderable>;

} // namespace tca::engine::widgets
