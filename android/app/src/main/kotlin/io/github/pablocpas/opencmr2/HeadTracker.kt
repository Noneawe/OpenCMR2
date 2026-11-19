package io.github.pablocpas.opencmr2

import android.app.Activity
import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.view.Surface

/**
 * Head tracking for the stereo mode: the game rotation vector (gyro and accelerometer, no
 * compass) of the phone in the goggles, sent to the game as a rotation matrix in the device's
 * landscape axes (x right, y up, z out of the screen). The game turns its camera by it, as an
 * offset (src/render/vr_head.cpp). Only started with `[vr] enabled = 1` and `head_tracking = 1`.
 */
class HeadTracker(private val activity: Activity) : SensorEventListener {
    private val manager = activity.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private val sensor: Sensor? = manager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR)
        ?: manager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)
    private val rotation = FloatArray(9)
    private val remapped = FloatArray(9)

    fun start() {
        sensor?.let { manager.registerListener(this, it, SensorManager.SENSOR_DELAY_GAME) }
    }

    fun stop() = manager.unregisterListener(this)

    @Suppress("DEPRECATION")
    override fun onSensorChanged(event: SensorEvent) {
        SensorManager.getRotationMatrixFromVector(rotation, event.values)
        // The sensor's axes are those of the phone held upright; the goggles hold it on its side.
        val out = when (activity.windowManager.defaultDisplay.rotation) {
            Surface.ROTATION_90 ->
                SensorManager.remapCoordinateSystem(rotation, SensorManager.AXIS_Y, SensorManager.AXIS_MINUS_X, remapped)
            Surface.ROTATION_270 ->
                SensorManager.remapCoordinateSystem(rotation, SensorManager.AXIS_MINUS_Y, SensorManager.AXIS_X, remapped)
            Surface.ROTATION_180 ->
                SensorManager.remapCoordinateSystem(rotation, SensorManager.AXIS_MINUS_X, SensorManager.AXIS_MINUS_Y, remapped)
            else -> { rotation.copyInto(remapped); true }
        }
        if (out) nativeRotation(remapped)
    }

    override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {}

    companion object {
        @JvmStatic external fun nativeRotation(matrix: FloatArray)
    }
}
