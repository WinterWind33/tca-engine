// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/images/image-decoder.hpp"

// C++ STL
#include <stdexcept>
#include <string>

// Windows
#include "platform/win32/windows.hpp"

// Third-party
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result.h>
#include <wincodec.h>

namespace tca::engine::platform::images {

namespace {

// Decoded pixels are always converted to this format, so callers never need to know anything
// about the source file's original pixel format. GUID_WICPixelFormat32bppRGBA is an extern const
// GUID (not a compile-time constant, since INITGUID is not defined here), so this cannot be
// constexpr.
const WICPixelFormatGUID DECODED_PIXEL_FORMAT{GUID_WICPixelFormat32bppRGBA};
constexpr std::uint32_t BYTES_PER_PIXEL{4};

} // namespace

decoded_image decode_image_from_file(const std::filesystem::path& path) {
    // WIC is a COM-based API, so the calling apartment must be initialized before any WIC object
    // can be created. Both S_OK and S_FALSE indicate a successful call that must be matched by a
    // CoUninitialize() (S_FALSE means COM was already initialized on this thread, but the
    // reference count still needs balancing); only a genuine failure (e.g. RPC_E_CHANGED_MODE)
    // means we must not call CoUninitialize() ourselves.
    const HRESULT coInitResult{::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)};
    const wil::unique_couninitialize_call comCleanup{SUCCEEDED(coInitResult)};

    try {
        const wil::com_ptr<IWICImagingFactory> wicFactory{
            wil::CoCreateInstance<IWICImagingFactory>(CLSID_WICImagingFactory)};

        wil::com_ptr<IWICBitmapDecoder> decoder{};
        THROW_IF_FAILED(wicFactory->CreateDecoderFromFilename(
            path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder));

        wil::com_ptr<IWICBitmapFrameDecode> frame{};
        THROW_IF_FAILED(decoder->GetFrame(0, &frame));

        wil::com_ptr<IWICFormatConverter> converter{};
        THROW_IF_FAILED(wicFactory->CreateFormatConverter(&converter));
        THROW_IF_FAILED(converter->Initialize(frame.get(), DECODED_PIXEL_FORMAT,
                                              WICBitmapDitherTypeNone, nullptr, 0.0,
                                              WICBitmapPaletteTypeCustom));

        UINT width{};
        UINT height{};
        THROW_IF_FAILED(converter->GetSize(&width, &height));

        decoded_image result{
            .pixels =
                std::vector<std::byte>(static_cast<std::size_t>(width) * height * BYTES_PER_PIXEL),
            .width = width,
            .height = height,
        };

        // Stride = bytes per pixels row, how many bytes each pixel raw has.
        const UINT stride{width * BYTES_PER_PIXEL};
        THROW_IF_FAILED(converter->CopyPixels(nullptr, stride,
                                              static_cast<UINT>(result.pixels.size()),
                                              reinterpret_cast<BYTE*>(result.pixels.data())));

        return result;
    } catch (const wil::ResultException& resultException) {
        throw std::runtime_error{std::string{"Failed to decode image '"} + path.string() +
                                 "': " + resultException.what()};
    }
}

} // namespace tca::engine::platform::images
