package gh.group38.smartsocket.data

import android.content.Context
import gh.group38.smartsocket.Notifications
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/**
 * Owns the link to the socket, and outlives the screen showing it.
 *
 * THE POINT OF THIS CLASS IS ITS LIFETIME. Held by the Application rather than a
 * ViewModel, the connection survives the activity being destroyed - a rotation,
 * or the user leaving the app - which is what makes "tell me when charging
 * finishes" possible at all. A link owned by a ViewModel dies with the screen,
 * and can only ever notify someone already looking at it.
 *
 * It is also the only place that watches for state *transitions* rather than
 * state. A cutoff notification must fire on the edge into Cutoff, once - not on
 * every status line that happens to say Cutoff.
 */
class SocketRepository(private val context: Context) {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    private val bluetooth = BluetoothTransport(context, scope)
    private val mock = MockTransport(scope)
    private var active: SocketTransport = bluetooth
    private var collectors: Job? = null

    val history = HistoryStore(context)

    private val _status = MutableStateFlow(SocketStatus.UNKNOWN)
    val status: StateFlow<SocketStatus> = _status.asStateFlow()

    private val _linkState = MutableStateFlow<LinkState>(LinkState.Idle)
    val linkState: StateFlow<LinkState> = _linkState.asStateFlow()

    val deviceName: String
        get() = (_linkState.value as? LinkState.Connected)?.device?.name ?: "Smart Socket"

    val isConnected: Boolean
        get() = _linkState.value is LinkState.Connected

    /** Peak of the session in progress, so a cutoff can be recorded with it. */
    private var sessionPeakMa = 0
    private var sessionStartedAt = 0L
    private var previousState: SocketState? = null

    fun pairedDevices(): List<SocketDevice> = bluetooth.pairedDevices()

    fun bluetoothOn(): Boolean = bluetooth.isBluetoothOn()

    fun connect(device: SocketDevice) {
        val transport = if (device.isDemo) mock else bluetooth
        collectors?.cancel()
        active = transport
        previousState = null

        collectors = scope.launch {
            launch { transport.linkState.collect { _linkState.value = it } }
            launch { transport.status.collect { onStatus(it) } }
        }

        scope.launch { transport.connect(device) }
    }

    fun send(command: SocketCommand) {
        scope.launch { active.send(command) }
    }

    fun disconnect() {
        collectors?.cancel()
        collectors = null
        active.disconnect()
        _status.value = SocketStatus.UNKNOWN
        _linkState.value = LinkState.Idle
        previousState = null
    }

    private fun onStatus(status: SocketStatus) {
        _status.value = status

        if (status.peakMa > sessionPeakMa) sessionPeakMa = status.peakMa
        if (status.state == SocketState.SETTLING && previousState != SocketState.SETTLING) {
            sessionPeakMa = 0
            sessionStartedAt = System.currentTimeMillis()
        }

        val was = previousState
        previousState = status.state
        if (was == null || was == status.state) return

        // --- edges only ------------------------------------------------------
        when (status.state) {
            SocketState.CUTOFF -> {
                history.record(
                    ChargeSession(
                        endedAtMillis = System.currentTimeMillis(),
                        peakMa = sessionPeakMa,
                        cutAtMa = status.currentMa,
                        durationMs = if (sessionStartedAt > 0) {
                            System.currentTimeMillis() - sessionStartedAt
                        } else {
                            0L
                        },
                    )
                )
                Notifications.alertCutoff(
                    context,
                    status.copy(peakMa = sessionPeakMa),
                )
                sessionPeakMa = 0
                sessionStartedAt = 0
            }

            SocketState.RELAY_STUCK -> Notifications.alertStuckRelay(context)

            else -> Unit
        }
    }
}
