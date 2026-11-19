// [vr] settings and the VR gate of the renderer (see docs/VR.md once written).
#ifndef OPENCMR2_VR_H
#define OPENCMR2_VR_H

#include "render/vr_layout.h"

// The [vr] section as it is now: read again on every call, so the live
// options (lens, shift, margin) take effect on the next frame.
VrConfig Vr_LoadConfig(void);

#endif
