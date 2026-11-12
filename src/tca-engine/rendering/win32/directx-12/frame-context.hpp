// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>

// Windows
#include "platform/win32/windows.hpp"

// DirectX 12
#include <d3d12.h>

// Third-party
#include <wil/com.h>

namespace tca::engine::rendering::win32::directx12 {

/**
 * @brief The FrameContext struct holds the command allocator and the fence value for a single frame
 * in flight. This is used to manage multiple frames in flight in a DirectX 12 application, allowing
 * for better performance by preparing multiple frames simultaneously. Each frame context
 * corresponds to a specific frame in flight, and the command allocator is used to record commands
 * for that frame, while the fence value is used to synchronize the CPU and GPU work for that frame.
 */
struct frame_context final {
    wil::com_ptr<ID3D12CommandAllocator> command_allocator{};
    std::uint64_t fence_value{};
};

} // namespace tca::engine::rendering::win32::directx12
