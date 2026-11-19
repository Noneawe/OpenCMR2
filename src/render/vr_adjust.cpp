#include "platform/platform.h"
#include "port/gfx.h"
#include "port/sys.h"
#include "render/vr.h"
#include "render/vr_adjust.h"
#include "render/vr_head.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

namespace {

bool s_active;
int s_selected;
bool s_changed;
bool s_prevUp, s_prevDown, s_prevReset, s_prevRecenter;
uint64_t s_nextRepeat, s_pressedAt;
int s_lastDirection;

float Value(const VrParam &p)
{
    const char *text = Platform_GetSetting(p.key, "");
    return text != NULL && *text != '\0' ? (float)atof(text) : p.def;
}

void SetValue(const VrParam &p, float value, bool save)
{
    char text[32];
    snprintf(text, sizeof(text), "%.*f", p.decimals + 1, value);
    Platform_SetSettingText(p.key, text, save);
}

bool Shown(const VrParam &p, bool stereo) { return stereo || !p.stereoOnly; }

void Move(int direction, int count)
{
    int n;
    const VrParam *params = Vr_Params(&n);
    SetValue(params[s_selected], Vr_StepParam(params[s_selected], Value(params[s_selected]), direction * count), false);
    s_changed = true;
}

void Select(int direction)
{
    int n;
    const VrParam *params = Vr_Params(&n);
    const bool stereo = Vr_LoadConfig().stereo;
    for (int i = 0; i < n; i++) {
        s_selected = (s_selected + direction + n) % n;
        if (Shown(params[s_selected], stereo))
            return;
    }
}

} // namespace

void Vr_AdjustUpdate(bool held, bool up, bool down, bool left, bool right, bool reset, bool recenter, uint64_t nowMs)
{
    int n;
    const VrParam *params = Vr_Params(&n);
    if (held != s_active) {
        s_active = held;
        s_prevUp = up;
        s_prevDown = down;
        s_prevReset = reset;
        s_prevRecenter = recenter;
        s_lastDirection = 0;
        if (held) {
            s_selected = 0;
            s_changed = false;
        } else if (s_changed) {
            // Saved once, as the list closes.
            for (int i = 0; i < n; i++)
                SetValue(params[i], Value(params[i]), true);
            s_changed = false;
        }
        return;
    }
    if (!s_active)
        return;
    if (up && !s_prevUp)
        Select(-1);
    if (down && !s_prevDown)
        Select(1);
    if (reset && !s_prevReset) {
        SetValue(params[s_selected], params[s_selected].def, false);
        s_changed = true;
    }
    if (recenter && !s_prevRecenter)
        Vr_HeadRecenter();
    s_prevUp = up;
    s_prevDown = down;
    s_prevReset = reset;
    s_prevRecenter = recenter;
    const int direction = (right ? 1 : 0) - (left ? 1 : 0);
    if (direction == 0) {
        s_lastDirection = 0;
    } else if (direction != s_lastDirection) {
        s_lastDirection = direction;
        s_pressedAt = nowMs;
        s_nextRepeat = nowMs + 400;
        Move(direction, 1);
    } else if (nowMs >= s_nextRepeat) {
        s_nextRepeat = nowMs + 70;
        Move(direction, nowMs - s_pressedAt > 1500 ? 5 : 1);   // faster once held
    }
}

bool Vr_AdjustActive(void) { return s_active; }

// ---- drawing ------------------------------------------------------------------------
// A 5x7 font for what the list needs, in a 128x64 atlas of 8x8 cells (ASCII
// 32..127); cell 127 is solid, for the background.

namespace {

struct Glyph { char c; const char *rows[7]; };

const Glyph kGlyphs[] = {
    {'A', {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {'C', {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}},
    {'D', {"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."}},
    {'E', {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
    {'F', {"#####", "#....", "#....", "####.", "#....", "#....", "#...."}},
    {'G', {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."}},
    {'H', {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {'I', {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'J', {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
    {'K', {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}},
    {'L', {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}},
    {'M', {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}},
    {'N', {"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"}},
    {'O', {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'P', {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}},
    {'R', {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}},
    {'S', {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
    {'T', {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
    {'U', {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'Y', {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}},
    {'V', {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."}},
    {'0', {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
    {'1', {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'2', {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
    {'3', {"####.", "....#", "....#", ".###.", "....#", "....#", "####."}},
    {'4', {"#...#", "#...#", "#...#", "#####", "....#", "....#", "....#"}},
    {'5', {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
    {'6', {".###.", "#....", "#....", "####.", "#...#", "#...#", ".###."}},
    {'7', {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
    {'8', {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
    {'9', {".###.", "#...#", "#...#", ".####", "....#", "....#", ".###."}},
    {'.', {".....", ".....", ".....", ".....", ".....", ".##..", ".##.."}},
    {'-', {".....", ".....", ".....", "#####", ".....", ".....", "....."}},
    {'+', {".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."}},
    {'>', {"#....", ".#...", "..#..", "...#.", "..#..", ".#...", "#...."}},
    {'(', {"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."}},
    {')', {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."}},
};

const int kAtlasW = 128, kAtlasH = 64, kCell = 8, kSolid = 127;
GfxTexture *s_font;

GfxTexture *Font()
{
    if (s_font != NULL)
        return s_font;
    GfxTexture *t = Gfx_CreateTexture(kAtlasW, kAtlasH, GFX_FORMAT_BGRA8, 1, 0);
    GfxLockedRect r;
    if (t == NULL || !Gfx_LockTexture(t, 0, 0, &r))
        return NULL;
    memset(r.pixels, 0, (size_t)r.pitch * kAtlasH);
    auto put = [&](int code, int x, int y) {
        const int cell = code - 32;
        ((uint32_t *)((uint8_t *)r.pixels + (size_t)r.pitch * ((cell / 16) * kCell + y)))[(cell % 16) * kCell + x] =
            0xffffffffu;
    };
    for (const Glyph &g : kGlyphs)
        for (int y = 0; y < 7; y++)
            for (int x = 0; x < 5; x++)
                if (g.rows[y][x] == '#')
                    put(g.c, x, y);
    for (int y = 0; y < kCell; y++)
        for (int x = 0; x < kCell; x++)
            put(kSolid, x, y);
    Gfx_UnlockTexture(t, 0, 0);
    return s_font = t;
}

// One glyph (or the solid cell) as a quad of the given colour.
void Quad(GfxTLVertex *v, int code, float x, float y, float w, float h, DWORD color)
{
    const int cell = code - 32;
    const float u0 = (float)((cell % 16) * kCell) / kAtlasW, v0 = (float)((cell / 16) * kCell) / kAtlasH;
    const float du = (float)kCell / kAtlasW, dv = (float)kCell / kAtlasH;
    const float xs[4] = { x, x + w, x, x + w }, ys[4] = { y, y, y + h, y + h };
    const float us[4] = { u0, u0 + du, u0, u0 + du }, vs[4] = { v0, v0, v0 + dv, v0 + dv };
    for (int i = 0; i < 4; i++) {
        memset(&v[i], 0, sizeof(v[i]));
        v[i].sx = xs[i];
        v[i].sy = ys[i];
        v[i].rhw = 1.0f;
        v[i].color = color;
        v[i].tu = us[i];
        v[i].tv = vs[i];
    }
}

void Text(const char *text, float x, float y, float scale, DWORD color)
{
    for (; *text != '\0'; text++, x += 6 * scale) {
        if (*text == ' ')
            continue;
        GfxTLVertex v[4];
        Quad(v, *text, x, y, kCell * scale, kCell * scale, color);
        Gfx_DrawPrimitiveTL(GFX_TRIANGLESTRIP, v, 4);
    }
}

} // namespace

void Vr_AdjustDraw(int width, int height)
{
    if (!s_active || Font() == NULL)
        return;
    int n;
    const VrParam *params = Vr_Params(&n);
    const bool stereo = Vr_LoadConfig().stereo;
    // The list is 33 characters wide: it takes at most 85% of the eye's width.
    const float scale = (float)std::max(1, std::min(height / 200, (int)(width * 0.85f / (33 * 6))));
    const float line = 10 * scale;
    const VrConfig config = Vr_LoadConfig();
    const bool headHint = config.stereo && config.headTracking;
    int lines = 1 + (headHint ? 1 : 0);
    for (int i = 0; i < n; i++)
        lines += Shown(params[i], stereo);

    Gfx_PushState();
    Gfx_SetRenderTarget(NULL);
    Gfx_SetVirtualScreen(0, 0);
    GfxViewport viewport = { 0, 0, (DWORD)width, (DWORD)height, 0.0f, 1.0f };
    Gfx_SetViewport(&viewport);
    Gfx_BeginScene();
    Gfx_SetDepthTest(FALSE);
    Gfx_SetDepthWrite(FALSE);
    Gfx_SetAlphaBlend(TRUE);
    Gfx_SetSrcBlend(GFX_BLEND_SRCALPHA);
    Gfx_SetDestBlend(GFX_BLEND_INVSRCALPHA);
    Gfx_SetAlphaTest(FALSE);
    Gfx_SetCullMode(GFX_CULL_NONE);
    Gfx_SetFog(FALSE);
    Gfx_SetTexture(0, Font());
    Gfx_SetStageColorOp(0, GFX_TOP_MODULATE);
    Gfx_SetStageColorArg1(0, GFX_TA_TEXTURE);
    Gfx_SetStageColorArg2(0, GFX_TA_DIFFUSE);
    Gfx_SetStageAlphaOp(0, GFX_TOP_MODULATE);
    Gfx_SetStageAlphaArg1(0, GFX_TA_TEXTURE);
    Gfx_SetStageAlphaArg2(0, GFX_TA_DIFFUSE);
    Gfx_SetStageColorOp(1, GFX_TOP_DISABLE);
    Gfx_SetStageAlphaOp(1, GFX_TOP_DISABLE);
    Gfx_SetStageMinFilter(0, GFX_FILTER_POINT);
    Gfx_SetStageMagFilter(0, GFX_FILTER_POINT);
    Gfx_SetStageAddress(0, GFX_ADDRESS_CLAMP);

    // In the middle of the picture, where the lens shows it best.
    const float boxW = 33 * 6 * scale, boxH = lines * line + 2 * scale;
    const float x0 = (width - boxW) * 0.5f + 3 * scale, y0 = height * 0.25f;
    GfxTLVertex box[4];
    Quad(box, kSolid, x0 - 3 * scale, y0 - 3 * scale, boxW, boxH, 0xb0000000);
    Gfx_DrawPrimitiveTL(GFX_TRIANGLESTRIP, box, 4);
    Text("VR ADJUST  (L3 + R3)", x0, y0, scale, 0xffffff80);
    float y = y0 + line;
    for (int i = 0; i < n; i++) {
        if (!Shown(params[i], stereo))
            continue;
        char text[64];
        snprintf(text, sizeof(text), "%c %-11s %.*f", i == s_selected ? '>' : ' ', params[i].label,
                 params[i].decimals, Value(params[i]));
        Text(text, x0, y, scale, i == s_selected ? 0xffffffff : 0xffb0b0b0);
        y += line;
    }
    if (headHint)
        Text("Y  RECENTER HEAD", x0, y, scale, 0xffffff80);
    Gfx_EndScene();
    Gfx_PopState();
}
