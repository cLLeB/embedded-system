package gh.group38.smartsocket.data

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID

/**
 * Bluetooth Classic (SPP) link to an HC-05.
 *
 * Classic rather than BLE because that is what an HC-05 is, and because SPP maps
 * onto a plain socket with an InputStream - no GATT services, no characteristic
 * discovery, no MTU negotiation. The cost is that this will never work on iOS,
 * which blocks SPP without MFi certification.
 */
class BluetoothTransport(
    private val context: Context,
    private val scope: CoroutineScope,
) : SocketTransport {

    private val _linkState = MutableStateFlow<LinkState>(LinkState.Idle)
    override val linkState: Flow<LinkState> = _linkState.asStateFlow()

    private val _status = MutableStateFlow(SocketStatus.UNKNOWN)
    override val status: Flow<SocketStatus> = _status.asStateFlow()

    private var socket: BluetoothSocket? = null
    private var output: OutputStream? = null
    private var reader: Job? = null

    /**
     * Two separate facts, and the difference between them is the whole
     * diagnosis. Bytes with no status line means the link is fine and the two
     * ends disagree about the baud rate; no bytes at all means the socket is
     * not talking, which is a wire.
     */
    private val sawBytes = MutableStateFlow(false)
    private val sawStatus = MutableStateFlow(false)

    @SuppressLint("MissingPermission")
    fun pairedDevices(): List<SocketDevice> {
        val adapter = adapter() ?: return emptyList()
        return try {
            adapter.bondedDevices.orEmpty()
                // Classic and dual-mode only. A BLE-only device has no RFCOMM
                // socket to open, so listing it here would offer the user a
                // connection that can only ever fail - BleTransport lists those.
                .filter {
                    it.type == BluetoothDevice.DEVICE_TYPE_CLASSIC ||
                        it.type == BluetoothDevice.DEVICE_TYPE_DUAL
                }
                .map {
                    SocketDevice(
                        name = it.name ?: "Unnamed",
                        address = it.address,
                        kind = LinkKind.CLASSIC,
                    )
                }
        } catch (_: SecurityException) {
            // The permission was revoked between the check and the call. Not an
            // error worth crashing over - the caller shows an empty list.
            emptyList()
        }
    }

    fun isBluetoothOn(): Boolean = adapter()?.isEnabled == true

    @SuppressLint("MissingPermission")
    override suspend fun connect(device: SocketDevice): Boolean {
        teardown()
        _linkState.value = LinkState.Connecting

        return withContext(Dispatchers.IO) {
            try {
                val adapter = adapter() ?: error("This phone has no Bluetooth")
                val remote: BluetoothDevice = adapter.getRemoteDevice(device.address)

                // Discovery is expensive and slows a connection attempt badly.
                adapter.cancelDiscovery()

                val s = remote.createRfcommSocketToServiceRecord(SPP_UUID)
                s.connect()

                socket = s
                output = s.outputStream
                sawBytes.value = false
                sawStatus.value = false
                reader = scope.launch(Dispatchers.IO) { readLines(s.inputStream) }

                // AN OPEN SOCKET IS NOT A SMART SOCKET. Every bonded device
                // answers on SPP if it speaks SPP at all, and a bad TX wire or a
                // module left at the wrong baud rate connects perfectly and then
                // says nothing. Without this the app sits on a dashboard of
                // zeroes with no hint of which of the two it is.
                if (!handshake()) {
                    val reason = if (sawBytes.value) WRONG_BAUD else NO_REPLY
                    teardown()
                    _linkState.value = LinkState.Failed(reason)
                    return@withContext false
                }

                _linkState.value = LinkState.Connected(device)
                true
            } catch (e: IOException) {
                teardown()
                _linkState.value = LinkState.Failed(
                    "Could not connect. Is the socket powered and in range?"
                )
                false
            } catch (e: SecurityException) {
                teardown()
                _linkState.value = LinkState.Failed("Bluetooth permission was refused")
                false
            } catch (e: Exception) {
                teardown()
                _linkState.value = LinkState.Failed(e.message ?: "Could not connect")
                false
            }
        }
    }

    /**
     * Asks the socket to identify itself and waits for one line it can parse.
     *
     * It asks rather than only listening because `?` makes the firmware publish
     * immediately instead of at the top of its next second, and it asks more
     * than once because the first write can land while the module is still
     * settling after the connection.
     */
    private suspend fun handshake(): Boolean {
        repeat(HANDSHAKE_PROMPTS) {
            writeLine(SocketCommand.STATUS_NOW.wire)
            val heard = withTimeoutOrNull(HANDSHAKE_WAIT_MS) { sawStatus.first { it } }
            if (heard == true) return true
        }
        return false
    }

    private suspend fun readLines(input: InputStream) {
        val buffer = StringBuilder()
        val chunk = ByteArray(128)

        try {
            while (currentCoroutineContext().isActive) {
                val read = input.read(chunk)
                if (read < 0) break
                if (read > 0) sawBytes.value = true

                for (i in 0 until read) {
                    val c = chunk[i].toInt().toChar()
                    if (c == '\n') {
                        StatusParser.parse(buffer.toString())?.let {
                            _status.value = it
                            sawStatus.value = true
                        }
                        buffer.setLength(0)
                    } else if (c != '\r') {
                        // A line this long is a corrupt stream, not a status.
                        // Dropping it beats growing without bound.
                        if (buffer.length < 128) buffer.append(c)
                    }
                }
            }
        } catch (_: IOException) {
            // Out of range, or the socket lost power. Both are ordinary.
        } finally {
            if (_linkState.value is LinkState.Connected) {
                _linkState.value = LinkState.Failed("Connection lost")
            }
        }
    }

    override suspend fun send(command: SocketCommand) = sendLine(command.wire)

    override suspend fun sendLine(line: String) {
        withContext(Dispatchers.IO) {
            if (!writeLine(line)) {
                _linkState.value = LinkState.Failed("Connection lost")
            }
        }
    }

    /** False means the write did not reach the socket. */
    private suspend fun writeLine(text: String): Boolean = withContext(Dispatchers.IO) {
        val out = output ?: return@withContext false
        try {
            out.write((text + "\n").toByteArray())
            out.flush()
            true
        } catch (_: IOException) {
            false
        }
    }

    override fun disconnect() {
        teardown()
        _linkState.value = LinkState.Idle
    }

    /**
     * Closes everything without announcing anything.
     *
     * Separate from [disconnect] because a reconnect tears the old socket down
     * between attempts, and an `Idle` in the middle of that would read to
     * everyone upstream as the user having hung up.
     */
    private fun teardown() {
        reader?.cancel()
        reader = null
        try {
            output?.close()
        } catch (_: IOException) {
        }
        try {
            socket?.close()
        } catch (_: IOException) {
        }
        output = null
        socket = null
    }

    private fun adapter(): BluetoothAdapter? =
        (context.getSystemService(Context.BLUETOOTH_SERVICE) as? android.bluetooth.BluetoothManager)
            ?.adapter

    companion object {
        /** The well-known Serial Port Profile UUID. Every HC-05 answers on it. */
        private val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

        private const val HANDSHAKE_PROMPTS = 3
        private const val HANDSHAKE_WAIT_MS = 2_500L

        private const val NO_REPLY =
            "Paired, but the socket never sent anything. Check the module's TXD " +
                "goes to the Arduino's pin 0, and that the socket is powered on."

        private const val WRONG_BAUD =
            "The socket is sending something this app cannot read. That is almost " +
                "always the wrong baud rate - the firmware uses 9600."
    }
}
