// Cardboard / SBS presentation maths: where each eye's picture sits on the
// screen and where a screen pixel samples it through the lens correction.
// Pure functions, no SDL: tests/vr_layout_test.cpp checks them and
// shaders/lens.frag repeats the formula per pixel.
#ifndef OPENCMR2_VR_LAYOUT_H
#define OPENCMR2_VR_LAYOUT_H

#include <algorithm>
#include <cmath>

struct VrConfig {
    bool enabled = false;
    bool stereo = false;        // mode = stereo (Stage 4); cinema otherwise
    bool lens = true;           // lens_enabled
    float k1 = 0.176f, k2 = 0.192f;
    float shift = 0.08f;        // 0..0.3, of an eye's half width, towards the screen's middle
    float margin = 0.0f;        // 0..0.5, shrinks each eye's picture, aspect kept
    float renderScale = 0.75f;  // size of the game's screen relative to an eye (needs a restart)
};

// An eye's picture on the screen, in pixels (origin top left).
struct VrEyeLayout {
    float cx, cy;       // centre of the picture
    float hw, hh;       // half size of the picture
    float cellX, cellW; // the half of the screen the eye owns (scissor)
    float cellH;
};

inline VrConfig Vr_Clamp(VrConfig c)
{
    c.shift = std::clamp(c.shift, -0.3f, 0.3f);
    c.margin = std::clamp(c.margin, 0.0f, 0.5f);
    c.renderScale = std::clamp(c.renderScale, 0.25f, 1.0f);
    return c;
}

// eye 0 = left, 1 = right. The picture has the shape of the eye's half of the
// screen; margin shrinks it about its centre, shift moves it towards the
// middle of the screen (negative values move the two pictures apart).
inline VrEyeLayout Vr_EyeLayout(int screenW, int screenH, int eye, const VrConfig &c)
{
    VrEyeLayout l;
    l.cellW = screenW * 0.5f;
    l.cellH = (float)screenH;
    l.cellX = eye == 0 ? 0.0f : l.cellW;
    const float s = 1.0f - c.margin;
    l.hw = l.cellW * 0.5f * s;
    l.hh = l.cellH * 0.5f * s;
    l.cy = l.cellH * 0.5f;
    l.cx = l.cellX + l.cellW * 0.5f + (eye == 0 ? 1.0f : -1.0f) * c.shift * l.cellW * 0.5f;
    return l;
}

// f(r) = 1 + k1 r^2 + k2 r^4.
inline float Vr_LensF(float r, float k1, float k2)
{
    const float r2 = r * r;
    return 1.0f + k1 * r2 + k2 * r2 * r2;
}

// Where the screen pixel (x, y) reads the eye's image, as 0..1 coordinates.
// The sample is c + d * f(r) / f(re): d is the pixel's offset from the
// picture's centre, r its length over the half diagonal and re the same
// ratio at the nearest edge, so that edge stays where it is (normalising to
// the corners would cut the top and bottom). Returns false outside the image
// (drawn black).
inline bool Vr_SampleUV(const VrEyeLayout &l, float k1, float k2, float x, float y, float *u, float *v)
{
    const float dx = x - l.cx, dy = y - l.cy;
    const float halfDiagonal = std::sqrt(l.hw * l.hw + l.hh * l.hh);
    float ratio = 1.0f;
    if (k1 != 0.0f || k2 != 0.0f) {
        const float r = std::sqrt(dx * dx + dy * dy) / halfDiagonal;
        const float re = std::min(l.hw, l.hh) / halfDiagonal;
        ratio = Vr_LensF(r, k1, k2) / Vr_LensF(re, k1, k2);
    }
    *u = 0.5f + dx * ratio / (2.0f * l.hw);
    *v = 0.5f + dy * ratio / (2.0f * l.hh);
    return *u >= 0.0f && *u <= 1.0f && *v >= 0.0f && *v <= 1.0f;
}

#endif
