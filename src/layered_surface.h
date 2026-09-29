#pragma once

// A top-down 32-bit BGRA DIB plus the one call that makes the ghosting fix
// work: UpdateLayeredWindow changes window size, position and pixels in a
// single atomic operation.
//
// That matters because the old code could not avoid a stale frame no matter how
// promptly it repainted. SetWindowPos changes the geometry, DWM composes with
// whatever pixels exist, and the repaint lands one present later. Size and
// content were two operations, so there was always a window in between.

#include <windows.h>

namespace offwork {

class LayeredSurface {
public:
    LayeredSurface() = default;
    ~LayeredSurface();

    LayeredSurface(const LayeredSurface&) = delete;
    LayeredSurface& operator=(const LayeredSurface&) = delete;

    // Reallocates only when the size actually changes.
    bool EnsureSize(int width, int height);

    HDC dc() const { return dc_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool valid() const { return dc_ != nullptr && pixels_ != nullptr; }

    // GDI leaves the alpha byte undefined, so the whole surface is opaque until
    // this runs: it writes alpha 255 inside a rounded rectangle, 0 outside, and
    // premultiplies the antialiased edge. Call after drawing, before Present.
    void ApplyRoundedAlpha(int radius);

    // One call: position, size and pixels together. opacity is the app's
    // 25-100% slider, folded into the same blend.
    bool Present(HWND window, POINT topLeft, BYTE opacity) const;

private:
    void Release();

    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previousBitmap_ = nullptr;
    void* pixels_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace offwork
