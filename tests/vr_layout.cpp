// The Cardboard/SBS layout and lens maths (src/render/vr_layout.h).
#include "render/vr_layout.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    // Stereo: with the eye offset and the off-axis shear, a point at the
    // convergence distance lands at the same x in both eyes and in the middle
    // view; a nearer point shifts left in the right eye (crossed disparity)
    // and right in the left eye; a farther one the other way; y never changes.
    const float view[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 3, 4, 1 };   // camera at (-2,-3,-4)
    const float proj[16] = { 1.5f, 0, 0, 0, 0, 2.0f, 0, 0, 0, 0, 1.01f, 1, 0, 0, -0.1f, 0 };
    Check(Vr_IsPerspective(proj), "perspective");
    const float ortho[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    Check(!Vr_IsPerspective(ortho), "not perspective");
    const float sep = 0.065f, conv = 10.0f;
    auto ndc = [&](int eye, float wx, float wy, float wz, float *y) {
        float v[16], p[16];
        if (eye < 0) {
            for (int i = 0; i < 16; i++) { v[i] = view[i]; p[i] = proj[i]; }
        } else {
            Vr_EyeMatrices(view, proj, eye, sep, conv, v, p);
        }
        const float vx = wx * v[0] + wy * v[4] + wz * v[8] + v[12];
        const float vy = wx * v[1] + wy * v[5] + wz * v[9] + v[13];
        const float vz = wx * v[2] + wy * v[6] + wz * v[10] + v[14];
        const float cx = vx * p[0] + vy * p[4] + vz * p[8] + p[12];
        const float cy = vx * p[1] + vy * p[5] + vz * p[9] + p[13];
        const float cw = vx * p[3] + vy * p[7] + vz * p[11] + p[15];
        *y = cy / cw;
        return cx / cw;
    };
    float y0, y1, y2;
    // The camera is at (-2,-3,-4) looking along +z: a point 10 ahead and 1.5 to the side.
    const float px = -2.0f + 1.5f, py = -3.0f + 0.5f;
    float xc = ndc(-1, px, py, -4.0f + conv, &y0), xl = ndc(0, px, py, -4.0f + conv, &y1),
          xr = ndc(1, px, py, -4.0f + conv, &y2);
    Check(Near(xc, xl, 1e-5f) && Near(xc, xr, 1e-5f), "no parallax at the convergence distance");
    Check(Near(y0, y1, 1e-6f) && Near(y0, y2, 1e-6f), "no vertical parallax");
    xl = ndc(0, px, py, -4.0f + 3.0f, &y1);
    xr = ndc(1, px, py, -4.0f + 3.0f, &y2);
    Check(xr < xl, "nearer than convergence: crossed disparity");
    xl = ndc(0, px, py, -4.0f + 40.0f, &y1);
    xr = ndc(1, px, py, -4.0f + 40.0f, &y2);
    Check(xr > xl, "farther than convergence: uncrossed disparity");
    // Zero separation is the middle view.
    float v0[16], p0[16];
    Vr_EyeMatrices(view, proj, 1, 0.0f, conv, v0, p0);
    Check(memcmp(v0, view, sizeof(v0)) == 0 && memcmp(p0, proj, sizeof(p0)) == 0, "separation 0 is the same camera");
    // The eyes are separation apart along the camera's x axis.
    float vl[16], pl[16], vr[16], pr[16];
    Vr_EyeMatrices(view, proj, 0, sep, conv, vl, pl);
    Vr_EyeMatrices(view, proj, 1, sep, conv, vr, pr);
    Check(Near(vl[12] - vr[12], sep), "eyes one separation apart");

    // The in-game adjustment: steps stay on the grid and in range.
    int count;
    const VrParam *params = Vr_Params(&count);
    Check(count == 8, "eight adjustable options");
    for (int i = 0; i < count; i++) {
        const VrParam &q = params[i];
        Check(Near(Vr_StepParam(q, q.def, 0), q.def, q.step * 0.01f) || q.def != std::round(q.def / q.step) * q.step,
              "default on its grid");
        Check(Vr_StepParam(q, q.max, 5) == q.max && Vr_StepParam(q, q.min, -5) == q.min, "clamped to the range");
        Check(Vr_StepParam(q, q.def, 1) > q.def || q.def == q.max, "step up");
        Check(Vr_StepParam(q, q.def, -1) < q.def || q.def == q.min, "step down");
    }
    Check(Near(Vr_StepParam(params[0], 0.08f, 1), 0.085f, 1e-5f), "shift steps by 0.005");

    // Per-view stereo values come from the projection's near plane (0.1 in the car, 1.0 outside).
    VrConfig sv;
    auto projNear = [](float n, float f, float out[16]) {
        for (int i = 0; i < 16; i++) out[i] = 0;
        out[0] = 1.5f; out[5] = 2.0f; out[10] = f / (f - n); out[11] = 1; out[14] = -n * f / (f - n);
    };
    float pn[16], sp, cv;
    projNear(1.0f, 250.0f, pn);
    Vr_ViewStereo(pn, sv, &sp, &cv);
    Check(Near(sp, sv.separation) && Near(cv, sv.convergence), "outside view: separation and convergence");
    projNear(0.1f, 250.0f, pn);
    Vr_ViewStereo(pn, sv, &sp, &cv);
    Check(Near(sp, sv.interiorSeparation) && Near(cv, sv.interiorConvergence), "in-car view: interior values");
    projNear(0.55f, 150.0f, pn);
    Vr_ViewStereo(pn, sv, &sp, &cv);
    Check(sp > sv.interiorSeparation && sp < sv.separation && Near(sp, 0.5f * (sv.separation + sv.interiorSeparation), 1e-3f),
          "between: blended");
    projNear(0.01f, 150.0f, pn);
    Vr_ViewStereo(pn, sv, &sp, &cv);
    Check(Near(sp, sv.interiorSeparation), "near plane below 0.1 clamps");

    // Head tracking. Device axes: x right, y up, z out of the screen (towards the face);
    // R maps device to world, columns of a right-handed rotation, row-major here.
    const float I3[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    float M[16];
    Vr_HeadViewMatrix(I3, I3, M);
    for (int i = 0; i < 16; i++)
        Check(M[i] == ((i % 5 == 0) ? 1.0f : 0.0f), "same pose: identity");
    auto ahead = [&](const float m[16], float *x, float *y, float *z) {   // (0,0,1) * m
        *x = m[8]; *y = m[9]; *z = m[10];
    };
    const float a = 0.5235988f;   // 30 degrees
    // Head turned right: right-handed rotation about -y.
    const float yawRight[9] = { std::cos(a), 0, -std::sin(a), 0, 1, 0, std::sin(a), 0, std::cos(a) };
    float x, y, z;
    Vr_HeadViewMatrix(I3, yawRight, M);
    ahead(M, &x, &y, &z);
    Check(x < -0.4f && Near(y, 0) && z > 0.8f && Near(x * x + y * y + z * z, 1.0f), "head right: the point ahead moves left");
    // Head looking up: right-handed rotation about +x.
    const float pitchUp[9] = { 1, 0, 0, 0, std::cos(a), -std::sin(a), 0, std::sin(a), std::cos(a) };
    Vr_HeadViewMatrix(I3, pitchUp, M);
    ahead(M, &x, &y, &z);
    Check(Near(x, 0) && y < -0.4f && z > 0.8f, "head up: the point ahead moves down");
    // Relative to the reference: the same pose as the reference is identity again.
    Vr_HeadViewMatrix(yawRight, yawRight, M);
    for (int i = 0; i < 16; i++)
        Check(Near(M[i], (i % 5 == 0) ? 1.0f : 0.0f), "recentred pose: identity");
    // Orthonormal (no stretch).
    Vr_HeadViewMatrix(I3, yawRight, M);
    Check(Near(M[0] * M[0] + M[1] * M[1] + M[2] * M[2], 1.0f), "rotation row has unit length");
    // Matrix product: view * head applies the view first.
    const float shiftX[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 0, 0, 1 };
    float P[16];
    Vr_MulMatrices(shiftX, M, P);
    Check(Near(P[12], 5.0f * M[0]) && Near(P[14], 5.0f * M[2]), "translation then rotation");

    printf("vr layout: ok\n");
    return 0;
}
