package gh.group38.smartsocket.data

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/**
 * Cuts mains power when this device's own battery reaches the user's limit.
 *
 * THE POINT OF THIS CLASS IS ITS LIFETIME, for the same reason as
 * [SocketRepository]: it is held by the Application, not by a ViewModel.
 *
 * It used to live in `SocketViewModel`, which is activity-scoped. Swiping the
 * app away ran `onCleared`, which unregistered the battery receiver and stopped
 * the one feature the app exists for - while the foreground service kept the
 * link up and the notification updating, so nothing looked wrong. Pressing Home
 * worked; swiping away silently did not.
 *
 * WHY THIS FEATURE EXISTS AT ALL: a phone charging on 230 V draws about 20 mA,
 * inside the ACS712's noise, so the socket cannot tell a charging phone from an
 * empty outlet. It is not a threshold that needs tuning - it is below the
 * sensor's resolution. But the phone knows its own battery, so it says so.
 */
class BatteryWatcher(
    private val context: Context,
    private val repo: SocketRepository,
    private val scope: CoroutineScope,
) {

    private val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    private val _percent = MutableStateFlow(0)
    val percent: StateFlow<Int> = _percent.asStateFlow()

    /**
     * Whether this phone is taking a charge, straight from Android.
     *
     * The socket cannot determine this - a charging phone draws about 20 mA,
     * below one ADC count on the ACS712-5A - so it reports a true zero and
     * concludes the outlet is empty. This is the only place the fact exists.
     */
    private val _charging = MutableStateFlow(false)
    val charging: StateFlow<Boolean> = _charging.asStateFlow()

    private val _limit = MutableStateFlow(prefs.getInt(KEY_LIMIT, DEFAULT_LIMIT))
    val limit: StateFlow<Int> = _limit.asStateFlow()

    private val _managing = MutableStateFlow(prefs.getBoolean(KEY_MANAGING, true))
    val managing: StateFlow<Boolean> = _managing.asStateFlow()

    /**
     * How far the battery must fall before power comes back.
     *
     * A window, not a point. Cutting at 90 and resuming at 90 would chatter the
     * relay every time the reading crossed the line - and these are mains
     * contacts, which weld. Ten points of hysteresis means one cut and one
     * resume per cycle, which is also how a battery prefers to be treated.
     */
    val resumeAt: Int get() = maxOf(5, _limit.value - RESUME_GAP)

    /** Set once per connection, so crossing the limit only ever fires one cut. */
    private var limitHandled = false

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            val level = intent?.getIntExtra(BatteryManager.EXTRA_LEVEL, -1) ?: -1
            val scale = intent?.getIntExtra(BatteryManager.EXTRA_SCALE, -1) ?: -1
            val batteryStatus = intent?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
            val nowCharging = batteryStatus == BatteryManager.BATTERY_STATUS_CHARGING ||
                batteryStatus == BatteryManager.BATTERY_STATUS_FULL

            val chargingChanged = nowCharging != _charging.value
            _charging.value = nowCharging

            // Plugging in or unplugging is exactly when the socket's display is
            // wrong and cannot know it, so tell it immediately rather than
            // waiting for the next heartbeat.
            if (chargingChanged) pushBattery()

            if (level >= 0 && scale > 0) {
                _percent.value = level * 100 / scale
                pushBattery()
                maybeCut()
                maybeResume()
            }
        }
    }

    fun start() {
        // ACTION_BATTERY_CHANGED is sticky, so this returns the current level
        // immediately rather than leaving percent at 0 until the next change.
        //
        // NOT_EXPORTED because nothing outside this app has any business sending
        // it; the system broadcast is delivered regardless of the flag, and 34+
        // wants the intent stated either way.
        ContextCompat.registerReceiver(
            context,
            receiver,
            IntentFilter(Intent.ACTION_BATTERY_CHANGED),
            ContextCompat.RECEIVER_NOT_EXPORTED,
        )

        scope.launch {
            repo.linkState.collect { state ->
                if (state is LinkState.Connected) {
                    // A command sent over a link that was already dying may never
                    // have arrived. Re-arming the check costs nothing - maybeCut
                    // only acts if power is still on.
                    limitHandled = false

                    // Tell the socket who is deciding, on every fresh connection
                    // rather than once: it may have rebooted while the link was
                    // down, and it comes up deciding for itself.
                    pushMode()
                    pushBattery()
                    maybeCut()
                }
            }
        }

        // The socket drops the claim after three minutes of silence - so that a
        // phone carried out of range cannot leave it switched off with nothing
        // able to turn it back on. This is the heartbeat that keeps it.
        scope.launch {
            while (true) {
                delay(KEEPALIVE_MS)
                pushMode()
                pushBattery()
            }
        }
    }

    /**
     * Hands the full-charge decision to this app, or gives it back.
     *
     * While the app is cutting on a real battery percentage, the socket's own
     * taper rule is worse than useless - it fires on a guess made from 26 mA of
     * resolution, and on a phone it cannot see the load at all. When the app is
     * not doing the cutting, the socket must go straight back to deciding for
     * itself, because then it is the only thing that can.
     */
    private fun pushMode() {
        if (!repo.isConnected) return
        repo.sendLine(if (_managing.value) "A1" else "A0")
    }

    /**
     * Sends the battery level for the socket's own display. Nothing in the
     * firmware's state machine reads it - a socket with no client attached has
     * to behave identically - but someone standing at the socket should be able
     * to see the number the cutoff is actually based on.
     */
    private fun pushBattery() {
        if (!repo.isConnected) return

        val value = _percent.value
        if (value in 0..100) repo.sendLine("B$value")

        repo.sendLine("T${_limit.value}")

        // The one fact the socket cannot obtain for itself. Without it the LCD
        // says "Ready - waiting for something to be plugged in" over an outlet
        // that is charging a phone, because 20 mA is below what its sensor can
        // resolve.
        repo.sendLine(if (_charging.value) "L1" else "L0")
    }

    /**
     * Hands the decision back immediately, for a deliberate disconnect.
     *
     * The claim would lapse on its own after three minutes, but those minutes
     * are a hole: the socket suppresses both its taper cutoff and its recovery
     * probe while the claim stands, so nothing at all is watching the charge.
     * Waiting it out is right when the phone vanished without warning - there is
     * nobody to ask. When the user pressed Disconnect there is.
     */
    fun handBack() {
        if (!repo.isConnected) return
        repo.sendLine("A0")
    }

    fun setLimit(limit: Int) {
        _limit.value = limit.coerceIn(50, 100)
        prefs.edit().putInt(KEY_LIMIT, _limit.value).apply()
        limitHandled = false
        maybeCut()
        maybeResume()
    }

    fun setManaging(enabled: Boolean) {
        _managing.value = enabled
        prefs.edit().putBoolean(KEY_MANAGING, enabled).apply()
        limitHandled = false

        // Turning this off must hand the decision straight back to the socket,
        // or nothing is watching the charge at all.
        pushMode()
        maybeCut()
    }

    private fun maybeCut() {
        if (!_managing.value) return
        if (limitHandled) return
        if (!repo.isConnected) return
        if (_percent.value < _limit.value) return
        if (!repo.status.value.state.isPowerOn) return

        limitHandled = true
        repo.send(SocketCommand.CUT)
    }

    /**
     * Puts power back when the battery has fallen far enough.
     *
     * This is the half that makes the socket something you can leave a phone on
     * indefinitely rather than a one-shot cutoff somebody has to go and reset.
     * It only ever acts on a socket this app cut - a cutoff the socket decided
     * on its own, or one a person asked for, is not this code's to undo.
     */
    private fun maybeResume() {
        if (!_managing.value) return
        if (!limitHandled) return
        if (!repo.isConnected) return
        if (_percent.value > resumeAt) return

        // Only from Cutoff. Re-arming out of a fault or a stuck relay would be
        // closing contacts on a socket that raised its hand for a reason.
        if (repo.status.value.state != SocketState.CUTOFF) return

        limitHandled = false
        repo.send(SocketCommand.REARM)
    }

    companion object {
        const val PREFS = "smart_socket"
        const val KEY_LIMIT = "battery_limit"
        const val KEY_MANAGING = "app_managed"
        const val DEFAULT_LIMIT = 90

        /** Hysteresis between cutting and charging again. */
        const val RESUME_GAP = 10

        /**
         * Comfortably inside the socket's three-minute claim timeout, and cheap:
         * three bytes on a link that is already carrying a status line a second.
         */
        const val KEEPALIVE_MS = 30_000L
    }
}
