#include "platform/platform.h"
#include "render/vr.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace {
float Float(const char *key, float fallback)
{
    const char *text = Platform_GetSetting(key, "");
    return text != NULL && *text != '\0' ? (float)atof(text) : fallback;
}
} // namespace

VrConfig Vr_LoadConfig(void)
{
    VrConfig c;
    c.enabled = Platform_GetSettingInt("vr.enabled", 0) != 0;
    c.stereo = strncasecmp(Platform_GetSetting("vr.mode", "cinema"), "stereo", 6) == 0;
    c.lens = Platform_GetSettingInt("vr.lens_enabled", 1) != 0;
    c.k1 = Float("vr.lens_k1", c.k1);
    c.k2 = Float("vr.lens_k2", c.k2);
    c.shift = Float("vr.shift", c.shift);
    c.margin = Float("vr.margin", c.margin);
    c.renderScale = Float("vr.render_scale", c.renderScale);
    c.separation = Float("vr.separation", c.separation);
    c.convergence = Float("vr.convergence", c.convergence);
    if (!c.lens)
        c.k1 = c.k2 = 0.0f;
    return Vr_Clamp(c);
}
