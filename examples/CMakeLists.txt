# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT

# Example applications built on top of the engine's public API only. They are not part of the
# engine and nothing links them, so they are opt-in (TCA_ENGINE_BUILD_EXAMPLES, default OFF) and a
# consumer add_subdirectory()ing the engine never pays for them.
add_subdirectory(demo-app)
