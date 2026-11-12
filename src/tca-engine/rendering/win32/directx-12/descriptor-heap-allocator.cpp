// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/directx-12/descriptor-heap-allocator.hpp"

// C++ STL
#include <stdexcept>

#ifdef _DEBUG
#include <algorithm>
#include <vector>
#endif // _DEBUG

// Third-party
#include <gsl/gsl>

namespace tca::engine::rendering::win32::directx12 {

namespace {
/**
 * @brief Advances a descriptor handle by a given index, using the provided start handle and handle
 * increment.
 *
 * @param startHandle The starting descriptor handle (CPU or GPU).
 * @param handleIncrement The size of a single descriptor handle increment, as returned by
 * ID3D12Device::GetDescriptorHandleIncrementSize.
 * @param index The index of the descriptor to advance to (0-based).
 * @return std::size_t The new descriptor handle's pointer value after advancing by the given index.
 */
template <typename DescriptorHandleType>
std::size_t advance_descriptor_handle(const DescriptorHandleType& startHandle,
                                      const std::size_t handleIncrement,
                                      const std::size_t index) noexcept {
    return startHandle.ptr + (index * handleIncrement);
}
} // namespace

descriptor_heap_allocator::descriptor_heap_allocator(heap_allocator_config&& config)
    : m_heapConfig(std::move(config)) {
    for (std::size_t i{m_heapConfig.num_descriptors}; i > 0; --i) {
        m_freeIndices.push(i - 1);
    }
}

void descriptor_heap_allocator::allocate(D3D12_CPU_DESCRIPTOR_HANDLE& outCpuDescriptorHandle,
                                         D3D12_GPU_DESCRIPTOR_HANDLE& outGpuDescriptorHandle) {
    if (m_freeIndices.empty()) {
        throw std::runtime_error("No more descriptors available in the heap");
    }

    const auto idx{m_freeIndices.top()};
    m_freeIndices.pop();
    outCpuDescriptorHandle.ptr = advance_descriptor_handle(m_heapConfig.heap_start_cpu,
                                                           m_heapConfig.heap_handle_increment, idx);
    outGpuDescriptorHandle.ptr = advance_descriptor_handle(m_heapConfig.heap_start_gpu,
                                                           m_heapConfig.heap_handle_increment, idx);
}

void descriptor_heap_allocator::free(const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescriptorHandle,
                                     const D3D12_GPU_DESCRIPTOR_HANDLE& gpuDescriptorHandle) {
    if (cpuDescriptorHandle.ptr < m_heapConfig.heap_start_cpu.ptr ||
        gpuDescriptorHandle.ptr < m_heapConfig.heap_start_gpu.ptr) {
        throw std::out_of_range("Descriptor handle is out of heap bounds");
    }

    // Before freeing the descriptors, we need to ensure that they point to valid locations within
    // the heap, to prevent invalid indices from being added to the free list.
    const std::size_t cpuMemDiff{cpuDescriptorHandle.ptr - m_heapConfig.heap_start_cpu.ptr};
    const std::size_t gpuMemDiff{gpuDescriptorHandle.ptr - m_heapConfig.heap_start_gpu.ptr};

    // Check that the descriptor handles are aligned to the increment size, to prevent invalid
    // indices from being added to the free list.
    if (cpuMemDiff % m_heapConfig.heap_handle_increment != 0 ||
        gpuMemDiff % m_heapConfig.heap_handle_increment != 0) {
        throw std::invalid_argument(
            "Descriptor handles are not properly aligned to the increment size");
    }

    const std::size_t cpuIdx{cpuMemDiff / m_heapConfig.heap_handle_increment};
    const std::size_t gpuIdx{gpuMemDiff / m_heapConfig.heap_handle_increment};
    if (cpuIdx != gpuIdx) {
        throw std::invalid_argument("CPU and GPU descriptor handles do not match");
    }

    if (cpuIdx >= m_heapConfig.num_descriptors) {
        throw std::out_of_range("Descriptor handle index is out of range");
    }

#ifdef _DEBUG
    // Check whether the descriptor being freed is already in the free list, to prevent double
    // frees.
    std::stack<std::size_t> tempStack{m_freeIndices};
    std::vector<std::size_t> freeIndicesVector{};
    while (!tempStack.empty()) {
        freeIndicesVector.push_back(tempStack.top());
        tempStack.pop();
    }

    Ensures(std::find(std::cbegin(freeIndicesVector), std::cend(freeIndicesVector), cpuIdx) ==
                std::cend(freeIndicesVector) &&
            "Descriptor handle is already freed");
#endif // _DEBUG

    m_freeIndices.push(cpuIdx);
}

} // namespace tca::engine::rendering::win32::directx12
