package gh.group38.smartsocket.data

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import java.util.UUID
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeoutOrNull

/**
 * Bluetooth Low Energy link to a GATT serial module.
 *
 * WHY THIS EXISTS. [BluetoothTransport] speaks Classic SPP over RFCOMM, which is
 * correct and the only option for a real HC-05. The module in this project is
 * not one: Windows reported a GATT service on it, the Classic pairing never
 * produced a serial port, and the desktop app identified it as `HM-10 / FFE0` -
 * a CC2541 BLE part sold in an HC-05-shaped package under the HC-05 name.
 *
 * A BLE module has no RFCOMM socket, no SPP UUID and no serial port, so nothing
 * in the Classic transport can reach it however long you stare at the paired
 * devices list. It has to be spoken to as GATT.
 *
 * Which characteristics carry the bytes is worked out by [BleSerialProfiles], in
 * the Android-free part of the app, so this class only has to deal with the
 * radio.
 */
@SuppressLint("MissingPermission")
class BleTransport(
    private val context: Context,
    private val scope: CoroutineScope,
) : SocketTransport {

    private companion object {
        /** Standard descriptor that turns notifications on. */
        val CccdUuid: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

        const val ConnectTimeoutMs = 15_000L

        /** Longer, because a stack-managed reconnect is meant to wait. */
        const val AutoConnectTimeoutMs = 45_000L
        const val DiscoverTimeoutMs = 10_000L
        const val HandshakeTimeoutMs = 10_000L

        /**
         * How long to wait for the stack to acknowledge one write before giving
         * up on it and letting the next through. Generous for BLE, and short
         * enough that a silent module cannot stall the queue.
         */
        const val WriteTimeoutMs = 600L

        /** A status line is longer than one BLE packet, so it arrives in pieces. */
        const val MaxLineLength = 128
    }

    private val _linkState = MutableStateFlow<LinkState>(LinkState.Idle)
    override val linkState: Flow<LinkState> = _linkState.asStateFlow()

    private val _status = MutableStateFlow(SocketStatus.UNKNOWN)
    override val status: Flow<SocketStatus> = _status.asStateFlow()

    private var gatt: BluetoothGatt? = null
    private var notifyCharacteristic: BluetoothGattCharacteristic? = null
    private var writeCharacteristic: BluetoothGattCharacteristic? = null

    private val line = StringBuilder()

    /**
     * Two separate facts, and the difference between them is the whole
     * diagnosis. Bytes with no status line means the link is fine and the two
     * ends disagree about the module's serial baud rate; no bytes at all means
     * nothing is reaching the module from the Arduino, which is a wire.
     */
    @Volatile private var sawBytes = false
    @Volatile private var sawStatus = false

    @Volatile private var connected = false
    @Volatile private var servicesFound = false
    @Volatile private var closing = false

    /** Which profile matched, once a link is up. Names the module in the UI. */
    @Volatile var profileName: String? = null
        private set

    private fun adapter(): BluetoothAdapter? =
        (context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter

    /**
     * BLE devices Android has actually bonded.
     *
     * USUALLY EMPTY, AND THAT IS NORMAL. Bonding is optional for BLE, and the
     * serial modules this app talks to do not ask for it - you simply connect.
     * So an HM-10 typically never appears in Android's Bluetooth settings at
     * all, never becomes bonded, and cannot be found this way. [startScan] is
     * how it is actually located; this is here for the modules that do bond.
     */
    fun pairedDevices(): List<SocketDevice> {
        val adapter = adapter() ?: return emptyList()

        return try {
            adapter.bondedDevices.orEmpty()
                .filter { it.type == BluetoothDevice.DEVICE_TYPE_LE || it.type == BluetoothDevice.DEVICE_TYPE_DUAL }
                .map {
                    SocketDevice(
                        name = it.name ?: "Unnamed",
                        address = it.address,
                        kind = LinkKind.BLE,
                    )
                }
        } catch (_: SecurityException) {
            emptyList()
        }
    }

    private var scanCallback: ScanCallback? = null

    /**
     * Looks for BLE devices on the air.
     *
     * Necessary rather than nice-to-have: a BLE serial module does not bond, so
     * it never reaches Android's paired list and a bonded-devices lookup can
     * never find it. Scanning is the only way it can be seen.
     *
     * Unfiltered on purpose. The module can be renamed by whoever configured it
     * - this one is an HM-10 calling itself HC-05 - so filtering by name would
     * hide exactly the device that needs finding. Unnamed results are kept too,
     * because a module that answers a scan without a name is still connectable
     * and its address identifies it.
     */
    fun startScan(onFound: (SocketDevice) -> Unit) {
        stopScan()

        val scanner = adapter()?.bluetoothLeScanner ?: return

        val callback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                val device = result.device ?: return
                onFound(
                    SocketDevice(
                        name = device.name ?: result.scanRecord?.deviceName ?: "Unnamed",
                        address = device.address,
                        kind = LinkKind.BLE,
                        rssi = result.rssi,
                    )
                )
            }

            override fun onBatchScanResults(results: MutableList<ScanResult>) {
                results.forEach { onScanResult(0, it) }
            }
        }

        scanCallback = callback

        // Low latency: the user is watching a list and waiting. A balanced scan
        // can take tens of seconds to surface a device that is right there.
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        try {
            scanner.startScan(null, settings, callback)
        } catch (_: SecurityException) {
            scanCallback = null
        }
    }

    fun stopScan() {
        val callback = scanCallback ?: return
        scanCallback = null

        try {
            adapter()?.bluetoothLeScanner?.stopScan(callback)
        } catch (_: SecurityException) {
            // Permission revoked, or the radio went away. Either way the scan is
            // already over.
        }
    }

    /**
     * Rebuilds a dropped link, handing the waiting to the Bluetooth stack.
     *
     * autoConnect true is the difference between a link that survives a screen
     * being switched off and one that does not. With it, Android itself watches
     * for the device and reconnects whenever it comes back - at a lower duty
     * cycle, outside this process, and unaffected by Doze quietening the app.
     * It is slow to make the first connection, which is why the user-initiated
     * path does not use it.
     */
    suspend fun reconnect(device: SocketDevice): Boolean = connect(device, autoConnect = true)

    override suspend fun connect(device: SocketDevice): Boolean =
        connect(device, autoConnect = false)

    private suspend fun connect(device: SocketDevice, autoConnect: Boolean): Boolean {
        disconnect()
        closing = false
        _linkState.value = LinkState.Connecting

        sawBytes = false
        sawStatus = false
        connected = false
        servicesFound = false
        synchronized(line) { line.setLength(0) }

        val adapter = adapter() ?: run {
            fail("Bluetooth is off.")
            return false
        }

        val remote = try {
            adapter.getRemoteDevice(device.address)
        } catch (e: IllegalArgumentException) {
            fail("${device.name} is not a device this phone knows. Pair it again.")
            return false
        }

        // False for a connection the user just asked for: a direct attempt fails
        // fast, where autoConnect would sit waiting indefinitely on a socket
        // that is switched off while somebody watches a spinner.
        //
        // True when rebuilding a link that dropped on its own, where waiting is
        // exactly what is wanted and the stack does it better than this process
        // can - see reconnect().
        gatt = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            remote.connectGatt(context, autoConnect, callback, BluetoothDevice.TRANSPORT_LE)
        } else {
            remote.connectGatt(context, autoConnect, callback)
        }

        if (gatt == null) {
            fail("Could not open a link to ${device.name}.")
            return false
        }

        // A stack-managed reconnect is meant to take its time - that is the
        // point of it - so it gets a longer window than a user standing there
        // waiting would tolerate.
        val connectWindow = if (autoConnect) AutoConnectTimeoutMs else ConnectTimeoutMs

        if (!awaitFlag(connectWindow) { connected }) {
            disconnect()
            fail("${device.name} did not answer. Check the socket is powered and in range.")
            return false
        }

        if (!awaitFlag(DiscoverTimeoutMs) { servicesFound }) {
            disconnect()
            fail("${device.name} connected but would not list its services.")
            return false
        }

        val profile = BleSerialProfiles.resolve(describeServices())
        if (profile == null) {
            val seen = gatt?.services.orEmpty().joinToString { it.uuid.toString() }
            disconnect()
            fail(
                "${device.name} is a Bluetooth LE device with no serial channel - " +
                    "nothing on it can carry the socket's data. Services found: " +
                    seen.ifEmpty { "none" }
            )
            return false
        }

        profileName = profile.name

        val service = gatt?.getService(profile.service)
        notifyCharacteristic = service?.getCharacteristic(profile.notify)
        writeCharacteristic = service?.getCharacteristic(profile.write)

        val notify = notifyCharacteristic
        if (notify == null || writeCharacteristic == null) {
            disconnect()
            fail("${device.name} did not offer the characteristics it advertised.")
            return false
        }

        if (!subscribe(notify)) {
            disconnect()
            fail("${device.name} refused a subscription, so it can never send anything.")
            return false
        }

        // Ask, rather than wait. The socket publishes once a second on its own,
        // but "?" makes it answer immediately, so a good link proves itself fast.
        send(SocketCommand.STATUS_NOW)

        if (!awaitFlag(HandshakeTimeoutMs) { sawStatus }) {
            val reason = if (sawBytes) {
                "${device.name} is sending data this app cannot read. The firmware uses 9600."
            } else {
                "${device.name} connected but never sent anything. Check the socket is " +
                    "powered, and that the module's TXD reaches Arduino pin 0."
            }
            disconnect()
            fail(reason)
            return false
        }

        _linkState.value = LinkState.Connected(device)
        return true
    }

    override suspend fun send(command: SocketCommand) = sendLine(command.wire)

    /**
     * ONE WRITE AT A TIME, AND NOT NEGOTIABLE.
     *
     * Android's GATT stack allows a single outstanding operation per connection.
     * Issue a second before the first has completed and it is rejected outright
     * and silently - writeCharacteristic simply returns false, nothing is sent,
     * and nothing tells you.
     *
     * This cost real debugging: the app pushes battery, limit and charging state
     * together, and only the first of the three ever reached the socket. The LCD
     * showed the percentage and kept saying "Ready" over a charging phone,
     * looking for all the world like a firmware bug.
     */
    private val writeLock = Mutex()

    /** Completed by onCharacteristicWrite, so the next write can start. */
    @Volatile private var writeAck: CompletableDeferred<Boolean>? = null

    override suspend fun sendLine(line: String) = writeLock.withLock {
        writeLineLocked(line)
    }

    private suspend fun writeLineLocked(line: String) {
        val characteristic = writeCharacteristic ?: return
        val target = gatt ?: return

        val bytes = (line + "\n").toByteArray(Charsets.US_ASCII)
        val ack = CompletableDeferred<Boolean>()
        writeAck = ack

        // WriteWithoutResponse where the module allows it: these parts are slow
        // to acknowledge and a two-byte command does not need one.
        val type =
            if (characteristic.properties and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE != 0) {
                BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            } else {
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            }

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                target.writeCharacteristic(characteristic, bytes, type)
            } else {
                @Suppress("DEPRECATION")
                characteristic.value = bytes
                @Suppress("DEPRECATION")
                characteristic.writeType = type
                @Suppress("DEPRECATION")
                target.writeCharacteristic(characteristic)
            }
        } catch (_: SecurityException) {
            // Permission revoked mid-session. The link is about to die anyway and
            // the callback reports it once.
            writeAck = null
            return
        }

        // Wait for the stack to report the write done before releasing the lock.
        // Timed out rather than awaited forever: a module that never
        // acknowledges must not wedge every later command behind it, and the
        // next write is more useful than this one was.
        withTimeoutOrNull(WriteTimeoutMs) { ack.await() }
        writeAck = null
    }

    override fun disconnect() {
        closing = true

        val target = gatt
        gatt = null
        notifyCharacteristic = null
        writeCharacteristic = null
        connected = false
        servicesFound = false

        try {
            target?.disconnect()
            target?.close()
        } catch (_: SecurityException) {
            // Nothing useful to do; the handle is being dropped regardless.
        }

        synchronized(line) { line.setLength(0) }
        _linkState.value = LinkState.Idle
    }

    private fun describeServices(): List<BleService> =
        gatt?.services.orEmpty().map { service ->
            BleService(
                uuid = service.uuid,
                characteristics = service.characteristics.orEmpty().map { c ->
                    val p = c.properties
                    BleCharacteristic(
                        uuid = c.uuid,
                        canNotify = (p and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0) ||
                            (p and BluetoothGattCharacteristic.PROPERTY_INDICATE != 0),
                        canWrite = (p and BluetoothGattCharacteristic.PROPERTY_WRITE != 0) ||
                            (p and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE != 0),
                    )
                },
            )
        }

    /**
     * Turning notifications on takes two steps, and missing the second is the
     * classic way a BLE link ends up connected and permanently silent: the local
     * flag only tells Android to deliver them, while the descriptor write is
     * what tells the module to send them.
     */
    private fun subscribe(characteristic: BluetoothGattCharacteristic): Boolean {
        val target = gatt ?: return false

        return try {
            if (!target.setCharacteristicNotification(characteristic, true)) return false

            val cccd = characteristic.getDescriptor(CccdUuid) ?: return false

            val value = if (characteristic.properties and
                BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0
            ) {
                BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            } else {
                BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                target.writeDescriptor(cccd, value) == BluetoothGatt.GATT_SUCCESS
            } else {
                @Suppress("DEPRECATION")
                cccd.value = value
                @Suppress("DEPRECATION")
                target.writeDescriptor(cccd)
            }
        } catch (_: SecurityException) {
            false
        }
    }

    private suspend fun awaitFlag(timeoutMs: Long, predicate: () -> Boolean): Boolean =
        withTimeoutOrNull(timeoutMs) {
            while (!predicate()) kotlinx.coroutines.delay(50)
            true
        } ?: false

    private fun fail(reason: String) {
        _linkState.value = LinkState.Failed(reason)
    }

    private fun onBytes(bytes: ByteArray) {
        if (bytes.isEmpty()) return
        sawBytes = true

        for (b in bytes) {
            val c = b.toInt().toChar()
            if (c == '\r') continue

            if (c != '\n') {
                synchronized(line) {
                    // BLE delivers 20-byte packets, so a status line always
                    // arrives in pieces and has to be reassembled. The cap stops
                    // a module that never sends a newline from growing it
                    // without bound.
                    if (line.length < MaxLineLength) line.append(c)
                }
                continue
            }

            val complete = synchronized(line) {
                val text = line.toString()
                line.setLength(0)
                text
            }

            StatusParser.parse(complete)?.let {
                sawStatus = true
                _status.value = it
            }
        }
    }

    private val callback = object : BluetoothGattCallback() {

        override fun onConnectionStateChange(g: BluetoothGatt, statusCode: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    connected = true
                    try {
                        g.discoverServices()
                    } catch (_: SecurityException) {
                        // Handled by the discovery timeout.
                    }
                }

                BluetoothProfile.STATE_DISCONNECTED -> {
                    connected = false
                    if (!closing) {
                        _linkState.value = LinkState.Failed("The link to the socket dropped.")
                    }
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, statusCode: Int) {
            servicesFound = statusCode == BluetoothGatt.GATT_SUCCESS
        }

        /** Releases the next queued write. See [writeLock]. */
        override fun onCharacteristicWrite(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            statusCode: Int,
        ) {
            writeAck?.complete(statusCode == BluetoothGatt.GATT_SUCCESS)
        }

        @Deprecated("Kept for API < 33, which never calls the byte-array overload.")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
        ) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                onBytes(characteristic.value ?: ByteArray(0))
            }
        }

        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
        ) {
            onBytes(value)
        }
    }
}
