package gh.group38.smartsocket.data

/**
 * Mirrors `SocketState` in `SmartSocket/src/core/Types.h`.
 *
 * The ordinals are the wire format - the firmware sends the number, not the
 * name, because a 32 KB part has better uses for its flash than strings the
 * phone already has. **If the enum in Types.h gains a member anywhere but the
 * end, this list has to move with it.**
 */
enum class SocketState(val label: String, val detail: String) {
    CALIBRATING("Calibrating", "Measuring the sensor's zero point"),
    READY("Ready", "Waiting for something to be plugged in"),
    SETTLING("Settling", "Ignoring the charger's inrush"),
    CHARGING("Charging", "Watching for the current to taper"),
    CUTOFF("Power cut", "Charged. Power is off"),
    FAULT("Overcurrent", "Something is wrong. Unplug at the wall"),
    MANUAL_OFF("Switched off", "You turned it off"),
    PROBING("Checking", "Looking for a load"),
    RELAY_STUCK("Relay stuck", "Power is still flowing. Unplug at the wall");

    val isPowerOn: Boolean
        get() = this == READY || this == SETTLING || this == CHARGING || this == PROBING

    val isAlarm: Boolean
        get() = this == FAULT || this == RELAY_STUCK

    companion object {
        fun fromOrdinal(value: Int): SocketState? = entries.getOrNull(value)
    }
}

/** One decoded status line. */
data class SocketStatus(
    val state: SocketState,
    val currentMa: Int,
    val peakMa: Int,
    val thresholdMa: Int,
    val sessionElapsedMs: Long,
    val cutoffCount: Int,
    val totalSavedMs: Long,
    val relayClosed: Boolean,
) {
    val amps: Float get() = currentMa / 1000f
    val peakAmps: Float get() = peakMa / 1000f
    val thresholdAmps: Float get() = thresholdMa / 1000f

    companion object {
        val UNKNOWN = SocketStatus(
            state = SocketState.CALIBRATING,
            currentMa = 0,
            peakMa = 0,
            thresholdMa = 0,
            sessionElapsedMs = 0,
            cutoffCount = 0,
            totalSavedMs = 0,
            relayClosed = false,
        )
    }
}

/** Commands the socket accepts. One byte each - see `HalTelemetry::pump`. */
enum class SocketCommand(val wire: String) {
    CUT("C"),
    REARM("R"),
    PROBE("P"),
    STATUS_NOW("?"),
}
