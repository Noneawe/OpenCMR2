#include "platform/platform.h"
#include "render/vr.h"
#include "render/vr_head.h"

#include <string.h>

#include <mutex>

#ifdef __ANDROID__
#include <jni.h>
#endif

namespace {
std::mutex s_mutex;
float s_pose[9], s_reference[9];
bool s_havePose, s_haveReference;
} // namespace

void Vr_HeadSetRotation(const float deviceToWorld[9])
{
    std::lock_guard<std::mutex> lock(s_mutex);
    memcpy(s_pose, deviceToWorld, sizeof(s_pose));
    s_havePose = true;
    if (!s_haveReference) {
        memcpy(s_reference, s_pose, sizeof(s_reference));
        s_haveReference = true;
    }
}

void Vr_HeadRecenter(void)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_havePose) {
        memcpy(s_reference, s_pose, sizeof(s_reference));
        s_haveReference = true;
    }
}

bool Vr_HeadMatrix(float out[16])
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_havePose || !s_haveReference)
        return false;
    Vr_HeadViewMatrix(s_reference, s_pose, out);
    return true;
}

#ifdef __ANDROID__
// GameActivity's sensor listener (HeadTracker.kt), on the UI thread.
extern "C" JNIEXPORT void JNICALL Java_io_github_pablocpas_opencmr2_HeadTracker_nativeRotation(
    JNIEnv *env, jclass, jfloatArray matrix)
{
    float r[9];
    if (env->GetArrayLength(matrix) < 9)
        return;
    env->GetFloatArrayRegion(matrix, 0, 9, r);
    Vr_HeadSetRotation(r);
}
#endif
