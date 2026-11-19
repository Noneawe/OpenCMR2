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
}
