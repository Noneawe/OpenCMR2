// In-game adjustment of the [vr] options with a gamepad, drawn by the port.
//
// Press both stick buttons (L3 + R3) to open or close the list: the game's actions go neutral, the list of
// options shows over the picture, Up/Down choose one, Left/Right change it
// (hold to go faster), A restores its starting value, Y recentres the head tracking. Press L3 + R3 again to
// close it: the changes are saved to opencmr2.ini. The picture follows at once
// (lens, shift, margin, separation, convergence are read every frame).
#ifndef OPENCMR2_VR_ADJUST_H
#define OPENCMR2_VR_ADJUST_H

#include <stdint.h>

// held = the list is open (the caller toggles it); nowMs is a monotonic clock.
void Vr_AdjustUpdate(bool held, bool up, bool down, bool left, bool right, bool reset, bool recenter,
                     uint64_t nowMs);
bool Vr_AdjustActive(void);
// Draws the list on the game's screen (width x height) before the frame is
// presented; does nothing while closed.
void Vr_AdjustDraw(int width, int height);

#endif
