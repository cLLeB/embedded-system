package gh.group38.smartsocket.data

import kotlinx.coroutines.flow.Flow

/**
 * How the bytes get there. Two radios, not one - "Bluetooth" is a name shared by
 * two incompatible systems, and the module in this project turned out to be the
 * other one.
 */
enum class LinkKind {
    /** Bluetooth Classic SPP over RFCOMM. A real HC-05. */
    CLASSIC,

    /**
     * Bluetooth Low Energy. No RFCOMM, no serial socket, no standard for
     * serial at all - just GATT characteristics chosen by the vendor. Most
     * modules sold today as an "HC-05" are actually this.
     */
    BLE,

    /** The socket's state machine in software. */
    DEMO,
}

/** A socket you can pair with. */
data class SocketDevice(
    val name: String,
    val address: String,
    val kind: LinkKind = LinkKind.CLASSIC,

    /**
     * Signal strength in dBm from the last scan, or null if not from a scan.
     *
     * Worth carrying because a BLE sweep in a building returns dozens of
     * results, most of them unnamed phones and watches using rotating private
     * addresses. The socket is the one on the desk in front of you, so it is
     * reliably the strongest - which makes this the only practical way to pick
     * it out of the list.
     */
    val rssi: Int? = null,
) {
    val isDemo: Boolean get() = kind == LinkKind.DEMO

    /** How it reads in a list, radio and all. */
    val display: String
        get() = when (kind) {
            LinkKind.BLE -> "$name  (Bluetooth LE)"
            else -> name
        }
}

sealed interface LinkState {
    data object Idle : LinkState
    data object Connecting : LinkState
    data class Connected(val device: SocketDevice) : LinkState

    /**
     * The link dropped on its own and is being rebuilt. Distinct from
     * [Connecting], which is a link the user just asked for: a reconnect must
     * not throw the user back to the device picker, and must not let go of the
     * foreground service that is the only reason the app is still running.
     */
    data class Reconnecting(val device: SocketDevice, val attempt: Int) : LinkState

    data class Failed(val reason: String) : LinkState
}

/**
 * The link to a socket.
 *
 * An interface with two implementations for the same reason the firmware has
 * `ICurrentSensor`: the UI must be buildable and demonstrable before the radio
 * exists. [MockTransport] runs the real state machine in software, so every
 * screen can be developed and shown with no HC-05 in the room.
 */
interface SocketTransport {
    val linkState: Flow<LinkState>
    val status: Flow<SocketStatus>

    /**
     * Opens the link and waits for the socket to prove it is one.
     *
     * Returns whether that succeeded, so a caller retrying on a schedule does
     * not have to race the [linkState] flow to find out.
     */
    suspend fun connect(device: SocketDevice): Boolean

    suspend fun send(command: SocketCommand)

    /**
     * One raw line, newline appended by the transport.
     *
     * For the commands that carry a value and so cannot be a [SocketCommand]:
     * `A1`/`A0` to take and hand back the full-charge decision, and
     * `B<percent>` to report this phone's battery for the socket's display.
     */
    suspend fun sendLine(line: String)

    fun disconnect()
}
