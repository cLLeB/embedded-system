package gh.group38.smartsocket.data

import android.content.Context
import gh.group38.smartsocket.Notifications
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
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

    private val prefs =
        context.getSharedPreferences(BatteryWatcher.PREFS, Context.MODE_PRIVATE)

    private val bluetooth = BluetoothTransport(context, scope)
    private val ble = BleTransport(context, scope)
    private val mock = MockTransport(scope)
    private var active: SocketTransport = bluetooth
    private var collectors: Job? = null

    /** Which BLE serial profile matched, once a BLE link is up. */
    val bleProfileName: String? get() = ble.profileName

    val history = HistoryStore(context)

    private val _status = MutableStateFlow(SocketStatus.UNKNOWN)
    val status: StateFlow<SocketStatus> = _status.asStateFlow()

    private val _linkState = MutableStateFlow<LinkState>(LinkState.Idle)
    val linkState: StateFlow<LinkState> = _linkState.asStateFlow()

    val deviceName: String
        get() = when (val state = _linkState.value) {
            is LinkState.Connected -> state.device.name
            is LinkState.Reconnecting -> state.device.name
            else -> "Smart Socket"
        }

    val isConnected: Boolean
        get() = _linkState.value is LinkState.Connected

    /** Peak of the session in progress, so a cutoff can be recorded with it. */
    private var sessionPeakMa = 0
    private var sessionStartedAt = 0L
    private var previousState: SocketState? = null

    /**
     * The socket's own lifetime cutoff count, as last seen. -1 until a first
     * status line arrives.
     *
     * Watched as well as the state edge because the edge is only visible if the
     * app happens to be connected at the instant it happens - and a socket left
     * charging overnight does its work precisely when nothing is watching. This
     * counter lives in the socket's EEPROM and survives both a reboot and a
     * dropped link, so a jump in it is proof of a cutoff that was missed.
     */
    private var lastCutoffCount = -1

    // --- reconnection ---------------------------------------------------------

    /** The socket the user asked to be connected to; null once they hang up. */
    private var target: SocketDevice? = null

    /**
     * Whether this target has ever answered. A first attempt that fails is a
     * wrong device or a socket that is switched off - retrying it for eight
     * minutes would only hide the error message that says so.
     */
    private var everAnswered = false

    private var reconnecting: Job? = null

    /**
     * Everything the socket could be, on both radios.
     *
     * Listed together rather than on separate screens because the user does not
     * know or care which radio their module uses - that is the whole problem
     * this solves. They pick the thing called HC-05 and the right transport is
     * chosen for them.
     */
    fun pairedDevices(): List<SocketDevice> = bluetooth.pairedDevices() + ble.pairedDevices()

    /**
     * Devices found on the air, on top of the bonded ones.
     *
     * Separate because they arrive one at a time over several seconds, where the
     * bonded list is a single synchronous answer.
     */
    private val _discovered = MutableStateFlow<List<SocketDevice>>(emptyList())
    val discovered: StateFlow<List<SocketDevice>> = _discovered.asStateFlow()

    /**
     * A BLE serial module does not bond, so it never reaches Android's paired
     * list. Scanning is the only way it can be found at all.
     */
    fun startScan() {
        _discovered.value = emptyList()
        ble.startScan { found ->
            // Same address repeatedly is the normal case - a device is reported
            // on every advertisement. Later sightings replace earlier ones so
            // the signal strength stays current, and a name that only appeared
            // in a later packet is not thrown away.
            val existing = _discovered.value.firstOrNull { it.address == found.address }

            _discovered.value = if (existing == null) {
                _discovered.value + found
            } else {
                _discovered.value.map {
                    if (it.address != found.address) {
                        it
                    } else {
                        found.copy(
                            name = if (found.name == "Unnamed") it.name else found.name
                        )
                    }
                }
            }
        }
    }

    fun stopScan() = ble.stopScan()

    fun bluetoothOn(): Boolean = bluetooth.isBluetoothOn()

    /**
     * The socket to reconnect to after a restart, or null if the user hung up.
     *
     * Only real sockets are remembered - reopening the demo on every boot would
     * be a lie about being connected to anything.
     */
    fun lastDevice(): SocketDevice? {
        val name = prefs.getString(KEY_LAST_NAME, null) ?: return null
        val address = prefs.getString(KEY_LAST_ADDRESS, null) ?: return null

        // The radio has to be remembered with it. Without this the stored device
        // defaulted to Classic, so reconnecting to a BLE module on launch or
        // after a reboot opened an RFCOMM socket that could never answer.
        val kind = runCatching {
            LinkKind.valueOf(prefs.getString(KEY_LAST_KIND, null) ?: LinkKind.CLASSIC.name)
        }.getOrDefault(LinkKind.CLASSIC)

        return SocketDevice(name = name, address = address, kind = kind)
    }

    fun connect(device: SocketDevice) {
        reconnecting?.cancel()
        reconnecting = null
        collectors?.cancel()

        val transport = when (device.kind) {
            LinkKind.DEMO -> mock
            LinkKind.BLE -> ble
            LinkKind.CLASSIC -> bluetooth
        }
        active = transport
        target = device
        everAnswered = false
        previousState = null

        if (!device.isDemo) {
            prefs.edit()
                .putString(KEY_LAST_NAME, device.name)
                .putString(KEY_LAST_ADDRESS, device.address)
                .putString(KEY_LAST_KIND, device.kind.name)
                .apply()
        }

        collectors = scope.launch {
            launch { transport.linkState.collect { onLink(it) } }
            launch { transport.status.collect { onStatus(it) } }
        }

        scope.launch { transport.connect(device) }
    }

    fun send(command: SocketCommand) {
        scope.launch { active.send(command) }
    }

    fun sendLine(line: String) {
        scope.launch { active.sendLine(line) }
    }

    fun disconnect() {
        reconnecting?.cancel()
        reconnecting = null
        collectors?.cancel()
        collectors = null
        target = null
        everAnswered = false
        active.disconnect()
        _status.value = SocketStatus.UNKNOWN
        _linkState.value = LinkState.Idle
        previousState = null

        // Only forgotten on a deliberate hang-up, not on a dropped link. Kept
        // across a reconnect it is exactly what catches a cutoff that happened
        // while the link was down; kept across a change of socket it would
        // invent one.
        lastCutoffCount = -1

        // Hanging up is a decision, and it has to survive a reboot too -
        // otherwise BootReceiver would reconnect to a socket the user walked
        // away from on purpose.
        prefs.edit()
            .remove(KEY_LAST_NAME)
            .remove(KEY_LAST_ADDRESS)
            .remove(KEY_LAST_KIND)
            .apply()
    }

    private fun onLink(state: LinkState) {
        when (state) {
            is LinkState.Connected -> {
                everAnswered = true
                reconnecting = null
                _linkState.value = state
            }

            is LinkState.Failed -> {
                // While the loop is running it narrates its own progress; the
                // failure of one attempt inside it is not news.
                if (reconnecting?.isActive == true) return

                val device = target
                if (everAnswered && device != null && !device.isDemo) {
                    startReconnecting(device)
                } else {
                    _linkState.value = state
                }
            }

            else -> if (reconnecting?.isActive != true) _linkState.value = state
        }
    }

    /**
     * Rebuilds a link that dropped on its own.
     *
     * A phone left charging goes out of range every time it is picked up, and
     * the cutoff it is waiting for might be an hour away. Returning to the
     * device picker and waiting to be noticed loses exactly the case the
     * background service exists to serve.
     */
    private fun startReconnecting(device: SocketDevice) {
        reconnecting = scope.launch {
            var attempt = 0
            var toldTheUser = false

            // FOREVER, until the user disconnects. Every reason a phone loses
            // this link is temporary - screen off, out of range, Doze quietening
            // the radio - and giving up after eight minutes meant the cutoff the
            // user was promised silently never happened.
            while (true) {
                val wait = ReconnectPolicy.delayMsFor(attempt) ?: ReconnectPolicy.CEILING_MS

                _linkState.value = LinkState.Reconnecting(device, attempt + 1)
                delay(wait)

                val reconnected = when (device.kind) {
                    // autoConnect, which hands the retry to the Bluetooth stack
                    // itself. It survives the screen going off far better than
                    // anything this process can do, because it is not this
                    // process doing it.
                    LinkKind.BLE -> ble.reconnect(device)
                    else -> bluetooth.connect(device)
                }

                if (reconnected) return@launch

                // Said once, when the escalating schedule runs out - not on
                // every retry for the rest of the night.
                if (!toldTheUser && attempt >= ReconnectPolicy.MAX_ATTEMPTS) {
                    toldTheUser = true
                    Notifications.alertLinkLost(context, device.name)
                }

                attempt++
            }
        }
    }

    private fun onStatus(status: SocketStatus) {
        _status.value = status

        if (status.peakMa > sessionPeakMa) sessionPeakMa = status.peakMa
        if (status.state == SocketState.SETTLING && previousState != SocketState.SETTLING) {
            sessionPeakMa = 0
            sessionStartedAt = System.currentTimeMillis()
        }

        // A cutoff the socket decided for itself while nobody was connected. Its
        // counter only moves on a real one, so a jump is evidence even though the
        // transition itself was never seen.
        val missedACutoff = lastCutoffCount >= 0 && status.cutoffCount > lastCutoffCount
        lastCutoffCount = status.cutoffCount

        val was = previousState
        previousState = status.state

        val enteredCutoff = was != null && was != status.state && status.state == SocketState.CUTOFF

        // Either signal records once, never twice: a cutoff seen live moves the
        // counter in the same status line that carries the transition.
        if (enteredCutoff || missedACutoff) {
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
            Notifications.alertCutoff(context, status.copy(peakMa = sessionPeakMa))
            sessionPeakMa = 0
            sessionStartedAt = 0
        }

        if (was == null || was == status.state) return

        if (status.state == SocketState.RELAY_STUCK) Notifications.alertStuckRelay(context)
    }

    private companion object {
        const val KEY_LAST_NAME = "last_device_name"
        const val KEY_LAST_ADDRESS = "last_device_address"
        const val KEY_LAST_KIND = "last_device_kind"
    }
}
