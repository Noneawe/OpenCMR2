package io.github.pablocpas.opencmr2

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class VrSettingsTest {
    @Test fun missingSectionOrKeyIsOff() {
        assertFalse(VrSettings.parseEnabled(""))
        assertFalse(VrSettings.parseEnabled("[video]\nenabled = 1\n"))
        assertFalse(VrSettings.parseEnabled("[vr]\n; enabled = 1\n"))
    }

    @Test fun enabledIsReadFromVrSectionOnly() {
        assertTrue(VrSettings.parseEnabled("[video]\nfullscreen = 1\n\n[VR]\nEnabled = 1\n"))
        assertFalse(VrSettings.parseEnabled("[vr]\nenabled = 0\n[net]\nenabled = 1\n"))
    }

    @Test fun flagsAreReadByKey() {
        val ini = "[vr]\nenabled = 1\nhead_tracking = 1\n[net]\nhead_tracking = 0\n"
        assertTrue(VrSettings.parseFlag(ini, "head_tracking"))
        assertFalse(VrSettings.parseFlag("[vr]\n; head_tracking = 1\n", "head_tracking"))
    }
}
