// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>
#include <stack>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <d3d12.h>
#include <wil/com.h>

namespace tca::engine::rendering::win32::directx12 {

/**
 * @brief Configuration structure for the descriptor heap allocator. This structure contains all the
 * necessary information to initialize and configure the allocator.
 */
struct heap_allocator_config final {
    D3D12_DESCRIPTOR_HEAP_TYPE heap_type{
        D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES};
    std::size_t heap_handle_increment{};
    std::size_t num_descriptors{};
    D3D12_CPU_DESCRIPTOR_HANDLE heap_start_cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE heap_start_gpu{};
};

/**
 * @brief A simple free list based allocator for DirectX 12 descriptor heaps.
 */
class descriptor_heap_allocator {
public:
    /**
     * @brief Construct a new descriptor heap allocator object with the given configuration and
     * number of descriptors. The constructor initializes the allocator's internal state based on
     * the provided configuration, and populates the free list with the indices of all available
     * descriptors in the heap.
     *
     * @param config The configuration structure containing the necessary information to initialize
     * the allocator.
     * @param numDescriptors The total number of descriptors available in the heap, which determines
     * the size of the free list.
     */
    explicit descriptor_heap_allocator(heap_allocator_config&& config);

    /**
     * @brief Allocates a descriptor from the heap and returns its CPU and GPU handles.
     *
     * @param outCpuDescriptorHandle The CPU descriptor handle of the allocated descriptor will be
     * written to this parameter.
     * @param outGpuDescriptorHandle The GPU descriptor handle of the allocated descriptor will be
     * written to this parameter.
     */
    void allocate(D3D12_CPU_DESCRIPTOR_HANDLE& outCpuDescriptorHandle,
                  D3D12_GPU_DESCRIPTOR_HANDLE& outGpuDescriptorHandle);

    /**
     * @brief Frees a previously allocated descriptor, given its CPU and GPU handles.
     *
     * @param cpuDescriptorHandle The CPU descriptor handle of the descriptor to free.
     * @param gpuDescriptorHandle The GPU descriptor handle of the descriptor to free. Must
     * correspond to the CPU handle.
     */
    void free(const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescriptorHandle,
              const D3D12_GPU_DESCRIPTOR_HANDLE& gpuDescriptorHandle);

private:
    const heap_allocator_config m_heapConfig;
    std::stack<std::size_t> m_freeIndices{};
};

} // namespace tca::engine::rendering::win32::directx12
