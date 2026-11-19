package io.github.pablocpas.opencmr2

import android.content.Context
import java.io.File

/**
 * The `[vr]` section of opencmr2.ini, which the native side owns (src/platform/sys.cpp).
 * Only what the Android shell needs: in VR mode the phone sits in goggles, so the
 * on-screen controls never show and the game is played with a gamepad.
 */
object VrSettings {
    // SDL_GetPrefPath on Android is the app's internal files directory.
    fun enabled(context: Context): Boolean =
        runCatching { parseEnabled(File(context.filesDir, "opencmr2.ini").readText()) }.getOrDefault(false)

    /** `[vr] enabled = 1` (section and key are case-insensitive, `;` and `#` start comments). */
    fun parseEnabled(ini: String): Boolean {
        var section = ""
        for (raw in ini.lineSequence()) {
            val line = raw.trim()
            if (line.isEmpty() || line[0] == ';' || line[0] == '#') continue
            if (line[0] == '[') {
                section = line.substringAfter('[').substringBefore(']').trim().lowercase()
                continue
            }
            val key = line.substringBefore('=', "").trim().lowercase()
            if (section == "vr" && key == "enabled") return line.substringAfter('=').trim().startsWith("1")
        }
        return false
    }
}
