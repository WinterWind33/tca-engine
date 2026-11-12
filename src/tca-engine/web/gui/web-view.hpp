// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// Engine
#include "rendering/tickables.hpp"

namespace tca::engine::web::gui {

class web_view : public rendering::renderable {
public:
    void render() override;
};

} // namespace tca::engine::web::gui
