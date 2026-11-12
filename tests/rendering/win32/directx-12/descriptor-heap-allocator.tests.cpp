// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "rendering/win32/directx-12/descriptor-heap-allocator.hpp"

// C++ STL
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <vector>

// Third-party
#include <catch2/catch_test_macros.hpp>

TEST_CASE("descriptor_heap_allocator class tests",
          "[tca][engine][rendering][win32][directx-12][descriptor-heap-allocator]") {
    using namespace tca::engine::rendering::win32;

    GIVEN("A test heap allocator configuration") {
        directx12::heap_allocator_config config{
            .heap_type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .heap_handle_increment = 32, // Example increment size
            .num_descriptors = 10, // Example number of descriptors
            .heap_start_cpu =
                D3D12_CPU_DESCRIPTOR_HANDLE{
                                            .ptr = 0,
                                            }, // Example starting CPU handle
            .heap_start_gpu =
                D3D12_GPU_DESCRIPTOR_HANDLE{
                                            .ptr = 1000,
                                            }, // Example starting GPU handle
        };

        directx12::descriptor_heap_allocator allocator{std::move(config)};

        WHEN("We allocate a pair of descriptors") {
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
            REQUIRE_NOTHROW(allocator.allocate(cpuHandle, gpuHandle));

            THEN(
                "The CPU and GPU handles should be correctly calculated based on the starting "
                "handles and the increment") {
                REQUIRE(cpuHandle.ptr == 0);
                REQUIRE(gpuHandle.ptr == 1000);
            }

            AND_WHEN("We try to free them") {
                THEN("It should not throw any exceptions") {
                    REQUIRE_NOTHROW(allocator.free(cpuHandle, gpuHandle));
                }
            }

            AND_WHEN("We try to free them with mismatched handles") {
                THEN("It should throw an invalid_argument exception") {
                    D3D12_CPU_DESCRIPTOR_HANDLE invalidCpuHandle{.ptr = 32};   // Next descriptor
                    D3D12_GPU_DESCRIPTOR_HANDLE invalidGpuHandle{.ptr = 2000}; // Invalid GPU handle
                    REQUIRE_THROWS_AS(allocator.free(invalidCpuHandle, invalidGpuHandle),
                                      std::invalid_argument);
                }
            }

            AND_WHEN("We allocate a second pair of descriptors") {
                D3D12_CPU_DESCRIPTOR_HANDLE secondCpuHandle{};
                D3D12_GPU_DESCRIPTOR_HANDLE secondGpuHandle{};
                REQUIRE_NOTHROW(allocator.allocate(secondCpuHandle, secondGpuHandle));

                THEN(
                    "The second pair of CPU and GPU handles should be correctly calculated based "
                    "on "
                    "the first allocation") {
                    REQUIRE(secondCpuHandle.ptr == 32);   // Next descriptor after the first
                    REQUIRE(secondGpuHandle.ptr == 1032); // Next descriptor after the first
                }
            }

            AND_WHEN("We free the first pair of descriptors and allocate again") {
                D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleToFree{.ptr = 0};
                D3D12_GPU_DESCRIPTOR_HANDLE gpuHandleToFree{.ptr = 1000};
                REQUIRE_NOTHROW(allocator.free(cpuHandleToFree, gpuHandleToFree));

                D3D12_CPU_DESCRIPTOR_HANDLE newCpuHandle{};
                D3D12_GPU_DESCRIPTOR_HANDLE newGpuHandle{};
                REQUIRE_NOTHROW(allocator.allocate(newCpuHandle, newGpuHandle));

                THEN("The newly allocated descriptors should reuse the freed handles") {
                    REQUIRE(newCpuHandle.ptr == 0);    // Reuse the first CPU handle
                    REQUIRE(newGpuHandle.ptr == 1000); // Reuse the first GPU handle
                }
            }

            AND_WHEN("We free out of range descriptors") {
                THEN("It should throw an out_of_range exception") {
                    D3D12_CPU_DESCRIPTOR_HANDLE outOfRangeCpuHandle{
                        .ptr = 320}; // Beyond the last descriptor
                    D3D12_GPU_DESCRIPTOR_HANDLE outOfRangeGpuHandle{
                        .ptr = 1320}; // Beyond the last descriptor
                    REQUIRE_THROWS_AS(allocator.free(outOfRangeCpuHandle, outOfRangeGpuHandle),
                                      std::out_of_range);
                }
            }

            AND_WHEN("We free unaligned descriptors") {
                THEN("It should throw an invalid_argument exception") {
                    D3D12_CPU_DESCRIPTOR_HANDLE unalignedCpuHandle{
                        .ptr = 15}; // Not aligned to increment
                    D3D12_GPU_DESCRIPTOR_HANDLE unalignedGpuHandle{
                        .ptr = 1015}; // Not aligned to increment
                    REQUIRE_THROWS_AS(allocator.free(unalignedCpuHandle, unalignedGpuHandle),
                                      std::invalid_argument);
                }
            }
        }

        WHEN("The allocator is fully allocated") {
            std::vector<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>>
                allocatedDescriptors{};

            for (std::size_t i = 0; i < config.num_descriptors; ++i) {
                D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
                D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
                REQUIRE_NOTHROW(allocator.allocate(cpuHandle, gpuHandle));
                allocatedDescriptors.emplace_back(cpuHandle, gpuHandle);
            }

            AND_WHEN("We try to allocate one more descriptor") {
                THEN("It should throw a runtime error indicating the heap is full") {
                    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
                    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
                    REQUIRE_THROWS_AS(allocator.allocate(cpuHandle, gpuHandle), std::runtime_error);
                }
            }
        }
    }
}
