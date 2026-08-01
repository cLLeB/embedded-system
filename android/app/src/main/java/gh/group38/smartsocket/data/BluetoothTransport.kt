package gh.group38.smartsocket.data

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
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

    @SuppressLint("MissingPermission")
    fun pairedDevices(): List<SocketDevice> {
        val adapter = adapter() ?: return emptyList()
        return try {
            adapter.bondedDevices.orEmpty().map {
                SocketDevice(name = it.name ?: "Unnamed", address = it.address)
            }
        } catch (_: SecurityException) {
            // The permission was revoked between the check and the call. Not an
            // error worth crashing over - the caller shows an empty list.
            emptyList()
        }
    }

    fun isBluetoothOn(): Boolean = adapter()?.isEnabled == true

    @SuppressLint("MissingPermission")
    override suspend fun connect(device: SocketDevice) {
        disconnect()
        _linkState.value = LinkState.Connecting

        withContext(Dispatchers.IO) {
            try {
                val adapter = adapter() ?: error("This phone has no Bluetooth")
                val remote: BluetoothDevice = adapter.getRemoteDevice(device.address)

                // Discovery is expensive and slows a connection attempt badly.
                adapter.cancelDiscovery()

                val s = remote.createRfcommSocketToServiceRecord(SPP_UUID)
                s.connect()

                socket = s
                output = s.outputStream
                _linkState.value = LinkState.Connected(device)

                reader = scope.launch(Dispatchers.IO) { readLines(s.inputStream) }
            } catch (e: IOException) {
                closeQuietly()
                _linkState.value = LinkState.Failed(
                    "Could not connect. Is the socket powered and in range?"
                )
            } catch (e: SecurityException) {
                closeQuietly()
                _linkState.value = LinkState.Failed("Bluetooth permission was refused")
            } catch (e: Exception) {
                closeQuietly()
                _linkState.value = LinkState.Failed(e.message ?: "Could not connect")
            }
        }
    }

    private suspend fun readLines(input: InputStream) {
        val buffer = StringBuilder()
        val chunk = ByteArray(128)

        try {
            while (scope.isActive) {
                val read = input.read(chunk)
                if (read < 0) break

                for (i in 0 until read) {
                    val c = chunk[i].toInt().toChar()
                    if (c == '\n') {
                        StatusParser.parse(buffer.toString())?.let { _status.value = it }
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

    override suspend fun send(command: SocketCommand) {
        val out = output ?: return
        withContext(Dispatchers.IO) {
            try {
                out.write((command.wire + "\n").toByteArray())
                out.flush()
            } catch (_: IOException) {
                _linkState.value = LinkState.Failed("Connection lost")
            }
        }
    }

    override fun disconnect() {
        reader?.cancel()
        reader = null
        closeQuietly()
        _linkState.value = LinkState.Idle
    }

    private fun closeQuietly() {
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
    }
}
