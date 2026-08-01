package gh.group38.smartsocket.data

import kotlinx.coroutines.flow.Flow

/** A socket you can pair with. */
data class SocketDevice(
    val name: String,
    val address: String,
    val isDemo: Boolean = false,
)

sealed interface LinkState {
    data object Idle : LinkState
    data object Connecting : LinkState
    data class Connected(val device: SocketDevice) : LinkState
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

    suspend fun connect(device: SocketDevice)
    suspend fun send(command: SocketCommand)
    fun disconnect()
}
