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
    // Stereo, in the game's world units (about one metre each, estimated from
    // one speed reading; see NOTES.md): distance between the eyes, and the
    // distance of the plane that has no parallax.
    float separation = 0.065f;
    float convergence = 10.0f;
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
    c.separation = std::clamp(c.separation, 0.0f, 2.0f);
    c.convergence = std::max(c.convergence, 0.5f);
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


// The options adjusted in game with the gamepad (render/vr_adjust.cpp), in the
// order of the on-screen list. stereoOnly ones are skipped in cinema mode.
struct VrParam {
    const char *label;      // on screen
    const char *key;        // in [vr] of opencmr2.ini
    float min, max, step, def;
    int decimals;
    bool stereoOnly;
};

inline const VrParam *Vr_Params(int *count)
{
    static const VrParam params[] = {
        { "SHIFT", "vr.shift", -0.3f, 0.3f, 0.005f, 0.08f, 3, false },
        { "MARGIN", "vr.margin", 0.0f, 0.5f, 0.01f, 0.0f, 2, false },
        { "LENS K1", "vr.lens_k1", -0.5f, 1.0f, 0.01f, 0.176f, 3, false },
        { "LENS K2", "vr.lens_k2", -0.5f, 1.0f, 0.01f, 0.192f, 3, false },
        { "SEPARATION", "vr.separation", 0.0f, 0.2f, 0.005f, 0.065f, 3, true },
        { "CONVERGENCE", "vr.convergence", 1.0f, 100.0f, 0.5f, 10.0f, 1, true },
    };
    *count = (int)(sizeof(params) / sizeof(params[0]));
    return params;
}

// A parameter after n steps (negative: down), kept in its range and on its grid.
inline float Vr_StepParam(const VrParam &p, float value, int n)
{
    return std::clamp(std::round((value + n * p.step) / p.step) * p.step, p.min, p.max);
}

// True for a Direct3D perspective projection (row vectors, w = z): the draws
// that get a stereo offset. 2D, orthographic and anything else stays as it is.
inline bool Vr_IsPerspective(const float proj[16])
{
    return proj[11] == 1.0f && proj[15] == 0.0f;
}

// One eye's view and projection from the game's (row-major, Direct3D: v' = v M,
// translation in _41.._43). The eye sits eye * separation / 2 to the right of
// the camera (eye 0 = left = -1, 1 = right = +1) and looks straight ahead, so
// the frustum is off-axis, not toed in: the view is moved by -e along x, and
// the projection's _31 gets e * _11 / convergence, which puts the plane at
// that distance at the same screen position in both eyes. Nearer objects then
// shift left in the right eye (crossed disparity: they come out of the
// screen), farther ones right.
inline void Vr_EyeMatrices(const float view[16], const float proj[16], int eye, float separation, float convergence,
                           float outView[16], float outProj[16])
{
    const float e = (eye == 0 ? -0.5f : 0.5f) * separation;
    for (int i = 0; i < 16; i++) {
        outView[i] = view[i];
        outProj[i] = proj[i];
    }
    // (V * T)_i1 = V_i1 - e * V_i4, T = translation by -e along x.
    for (int row = 0; row < 4; row++)
        outView[row * 4] = view[row * 4] - e * view[row * 4 + 3];
    outProj[8] = proj[8] + e * proj[0] / convergence;
}

#endif
