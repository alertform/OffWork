#include "layered_surface.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace offwork {
namespace {

// Coverage of one pixel by a quarter-disc of the given radius, centred at
// (radius, radius) in corner-local coordinates. A one-pixel band is enough to
// match the antialiasing DWM applies to its own rounded corners.
float CornerCoverage(float x, float y, float radius) {
    const float dx = radius - x;
    const float dy = radius - y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    return std::clamp(radius - distance + 0.5f, 0.0f, 1.0f);
}

void SetPixelAlpha(std::uint8_t* pixel, float coverage) {
    // The blend is AC_SRC_ALPHA, so the colour channels must be premultiplied.
    pixel[0] = static_cast<std::uint8_t>(pixel[0] * coverage + 0.5f);
    pixel[1] = static_cast<std::uint8_t>(pixel[1] * coverage + 0.5f);
    pixel[2] = static_cast<std::uint8_t>(pixel[2] * coverage + 0.5f);
    pixel[3] = static_cast<std::uint8_t>(coverage * 255.0f + 0.5f);
}

}  // namespace

LayeredSurface::~LayeredSurface() {
    Release();
}

void LayeredSurface::Release() {
    if (dc_ != nullptr) {
        if (previousBitmap_ != nullptr) {
            SelectObject(dc_, previousBitmap_);
            previousBitmap_ = nullptr;
        }
        DeleteDC(dc_);
        dc_ = nullptr;
    }
    if (bitmap_ != nullptr) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    pixels_ = nullptr;
    width_ = 0;
    height_ = 0;
}

bool LayeredSurface::EnsureSize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (dc_ != nullptr && width == width_ && height == height_) {
        return true;
    }

    Release();

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    if (screen == nullptr) {
        return false;
    }
    HDC memory = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (memory == nullptr) {
        return false;
    }

    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(
        memory, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (bitmap == nullptr || pixels == nullptr) {
        if (bitmap != nullptr) {
            DeleteObject(bitmap);
        }
        DeleteDC(memory);
        return false;
    }

    dc_ = memory;
    bitmap_ = bitmap;
    previousBitmap_ = SelectObject(memory, bitmap);
    pixels_ = pixels;
    width_ = width;
    height_ = height;
    return true;
}

void LayeredSurface::ApplyRoundedAlpha(int radius) {
    if (!valid()) {
        return;
    }

    auto* base = static_cast<std::uint8_t*>(pixels_);
    const std::size_t stride = static_cast<std::size_t>(width_) * 4u;

    // Everything is opaque unless a corner says otherwise.
    for (int y = 0; y < height_; ++y) {
        std::uint8_t* row = base + static_cast<std::size_t>(y) * stride;
        for (int x = 0; x < width_; ++x) {
            row[static_cast<std::size_t>(x) * 4u + 3u] = 255;
        }
    }

    const int limit = std::min({radius, width_ / 2, height_ / 2});
    if (limit <= 0) {
        return;
    }
    const float radiusF = static_cast<float>(limit);

    for (int y = 0; y < limit; ++y) {
        for (int x = 0; x < limit; ++x) {
            const float coverage =
                CornerCoverage(static_cast<float>(x) + 0.5f,
                               static_cast<float>(y) + 0.5f, radiusF);
            if (coverage >= 1.0f) {
                continue;
            }

            const int right = width_ - 1 - x;
            const int bottom = height_ - 1 - y;
            std::uint8_t* topRow = base + static_cast<std::size_t>(y) * stride;
            std::uint8_t* bottomRow = base + static_cast<std::size_t>(bottom) * stride;

            SetPixelAlpha(topRow + static_cast<std::size_t>(x) * 4u, coverage);
            SetPixelAlpha(topRow + static_cast<std::size_t>(right) * 4u, coverage);
            SetPixelAlpha(bottomRow + static_cast<std::size_t>(x) * 4u, coverage);
            SetPixelAlpha(bottomRow + static_cast<std::size_t>(right) * 4u, coverage);
        }
    }
}

bool LayeredSurface::Present(HWND window, POINT topLeft, BYTE opacity) const {
    if (!valid() || window == nullptr) {
        return false;
    }

    SIZE size = {width_, height_};
    POINT source = {0, 0};
    POINT destination = topLeft;
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = opacity;
    blend.AlphaFormat = AC_SRC_ALPHA;

    return UpdateLayeredWindow(
               window, nullptr, &destination, &size, dc_, &source, 0, &blend,
               ULW_ALPHA) != FALSE;
}

}  // namespace offwork
