// The Cardboard/SBS layout and lens maths (src/render/vr_layout.h).
#include "render/vr_layout.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static void Check(bool ok, const char *message)
{
    if (!ok) {
        fprintf(stderr, "vr layout: %s\n", message);
        exit(1);
    }
}

static bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main()
{
    const int W = 2400, H = 1080;
    VrConfig c;
    c.shift = 0;
    c.margin = 0;

    // Without shift and margin each eye's picture fills its half of the screen.
    VrEyeLayout left = Vr_EyeLayout(W, H, 0, c), right = Vr_EyeLayout(W, H, 1, c);
    Check(Near(left.cx, 600) && Near(right.cx, 1800) && Near(left.hw, 600) && Near(left.hh, 540), "full halves");

    // Shift moves the two pictures towards the middle, symmetrically; negative moves them apart.
    c.shift = 0.1f;
    left = Vr_EyeLayout(W, H, 0, c);
    right = Vr_EyeLayout(W, H, 1, c);
    Check(Near(left.cx, 660) && Near(right.cx, 1740), "shift towards the middle");
    Check(Near(left.cx + right.cx, W), "shift symmetric");
    c.shift = -0.1f;
    Check(Near(Vr_EyeLayout(W, H, 0, c).cx, 540), "negative shift moves apart");

    // Margin shrinks the picture about its centre and keeps its shape.
    c.shift = 0;
    c.margin = 0.25f;
    left = Vr_EyeLayout(W, H, 0, c);
    Check(Near(left.hw, 450) && Near(left.hh, 405) && Near(left.hw / left.hh, 600.0f / 540.0f), "margin keeps aspect");
    Check(Near(left.cx, 600) && Near(left.cy, 540), "margin keeps centre");

    // Without lens correction the screen maps linearly onto the image.
    c.margin = 0;
    left = Vr_EyeLayout(W, H, 0, c);
    float u, v;
    Check(Vr_SampleUV(left, 0, 0, left.cx, left.cy, &u, &v) && Near(u, 0.5f) && Near(v, 0.5f), "identity centre");
    Check(Vr_SampleUV(left, 0, 0, 0.0f, 0.0f, &u, &v) && Near(u, 0.0f) && Near(v, 0.0f), "identity top left");

    // With it, the middle of the nearest edge (top and bottom here: the half is
    // wider than tall) stays on that edge, so nothing is cut off there...
    const float k1 = 0.176f, k2 = 0.192f;
    Check(Vr_SampleUV(left, k1, k2, left.cx, 0.0f, &u, &v) && Near(u, 0.5f) && Near(v, 0.0f), "nearest edge fixed");
    Check(Vr_SampleUV(left, k1, k2, left.cx, (float)H, &u, &v) || Near(v, 1.0f, 1e-3f), "bottom edge fixed");
    // ...the centre stays put, the corners sample outside the image (drawn black)...
    Check(Vr_SampleUV(left, k1, k2, left.cx, left.cy, &u, &v) && Near(u, 0.5f) && Near(v, 0.5f), "lens centre");
    Check(!Vr_SampleUV(left, k1, k2, 0.0f, 0.0f, &u, &v), "corner outside");
    // ...and the picture is magnified in the middle: a point halfway to the
    // edge reads nearer the centre than without the correction.
    Vr_SampleUV(left, k1, k2, left.cx, left.cy - 270.0f, &u, &v);
    Check(v > 0.25f, "centre magnified");

    // Disabled lens parameters are the identity everywhere.
    Check(Vr_SampleUV(left, 0, 0, 100.0f, 200.0f, &u, &v) && Near(u, 100.0f / 1200.0f) && Near(v, 200.0f / 1080.0f),
          "k = 0 identity");

    // Clamp keeps the options in their ranges.
    VrConfig bad;
    bad.shift = 5;
    bad.margin = 2;
    bad.renderScale = 0;
    bad = Vr_Clamp(bad);
    Check(bad.shift <= 0.3f && bad.margin <= 0.5f && bad.renderScale >= 0.25f, "clamp");
    printf("vr layout: ok\n");
    return 0;
}
