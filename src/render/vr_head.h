// Head tracking for the stereo mode: the phone's orientation (Android's game
// rotation vector, fed from the activity through JNI) turns the game's camera
// as an offset on top of it, never instead of it. Off unless [vr] head_tracking = 1.
#ifndef OPENCMR2_VR_HEAD_H
#define OPENCMR2_VR_HEAD_H

// A sensor sample: row-major 3x3 rotation, device axes to world axes, in the
// device's landscape orientation (see Vr_HeadViewMatrix). The first one after
// start-up is the reference pose.
void Vr_HeadSetRotation(const float deviceToWorld[9]);
// The current pose becomes the straight-ahead one.
void Vr_HeadRecenter(void);
// The camera-space rotation (row-major 4x4, Direct3D row vectors) for this frame;
// false (out untouched) when head tracking is off or no sample has come yet.
bool Vr_HeadMatrix(float out[16]);

#endif
