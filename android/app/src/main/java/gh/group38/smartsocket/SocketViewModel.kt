package gh.group38.smartsocket

import android.app.Application
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import gh.group38.smartsocket.data.BluetoothTransport
import gh.group38.smartsocket.data.LinkState
import gh.group38.smartsocket.data.MockTransport
import gh.group38.smartsocket.data.SocketCommand
import gh.group38.smartsocket.data.SocketDevice
import gh.group38.smartsocket.data.SocketState
import gh.group38.smartsocket.data.SocketStatus
import gh.group38.smartsocket.data.SocketTransport
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

enum class Screen { SPLASH, ONBOARDING, CONNECT, CONNECTING, DASHBOARD }

class SocketViewModel(app: Application) : AndroidViewModel(app) {

    private val prefs = app.getSharedPreferences("smart_socket", Context.MODE_PRIVATE)

    private val bluetooth = BluetoothTransport(app, viewModelScope)
    private val mock = MockTransport(viewModelScope)
    private var active: SocketTransport = bluetooth
    private var collectors: Job? = null

    private val _screen = MutableStateFlow(Screen.SPLASH)
    val screen: StateFlow<Screen> = _screen.asStateFlow()

    private val _status = MutableStateFlow(SocketStatus.UNKNOWN)
    val status: StateFlow<SocketStatus> = _status.asStateFlow()

    private val _linkState = MutableStateFlow<LinkState>(LinkState.Idle)
    val linkState: StateFlow<LinkState> = _linkState.asStateFlow()

    private val _devices = MutableStateFlow<List<SocketDevice>>(emptyList())
    val devices: StateFlow<List<SocketDevice>> = _devices.asStateFlow()

    private val _permissionGranted = MutableStateFlow(false)
    val permissionGranted: StateFlow<Boolean> = _permissionGranted.asStateFlow()

    private val _batteryPercent = MutableStateFlow(0)
    val batteryPercent: StateFlow<Int> = _batteryPercent.asStateFlow()

    private val _batteryLimit = MutableStateFlow(prefs.getInt(KEY_LIMIT, 90))
    val batteryLimit: StateFlow<Int> = _batteryLimit.asStateFlow()

    val connectedName: String
        get() = (_linkState.value as? LinkState.Connected)?.device?.name ?: "Socket"

    /** Set once per connection, so crossing the limit only fires a cut once. */
    private var limitHandled = false

    private val batteryReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            val level = intent?.getIntExtra(BatteryManager.EXTRA_LEVEL, -1) ?: -1
            val scale = intent?.getIntExtra(BatteryManager.EXTRA_SCALE, -1) ?: -1
            if (level >= 0 && scale > 0) {
                _batteryPercent.value = level * 100 / scale
                maybeCutForBattery()
            }
        }
    }

    init {
        app.registerReceiver(batteryReceiver, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
    }

    override fun onCleared() {
        runCatching { getApplication<Application>().unregisterReceiver(batteryReceiver) }
        bluetooth.disconnect()
        mock.disconnect()
        super.onCleared()
    }

    fun onSplashDone() {
        _screen.value =
            if (prefs.getBoolean(KEY_ONBOARDED, false)) Screen.CONNECT else Screen.ONBOARDING
    }

    fun onOnboardingDone() {
        prefs.edit().putBoolean(KEY_ONBOARDED, true).apply()
        _screen.value = Screen.CONNECT
    }

    fun onPermissionResult(granted: Boolean) {
        _permissionGranted.value = granted
        if (granted) refreshDevices()
    }

    fun refreshDevices() {
        _devices.value = if (_permissionGranted.value) bluetooth.pairedDevices() else emptyList()
    }

    fun bluetoothOn(): Boolean = bluetooth.isBluetoothOn()

    fun connect(device: SocketDevice) = start(bluetooth, device)

    fun openDemo() = start(mock, MockTransport.DEVICE)

    private fun start(transport: SocketTransport, device: SocketDevice) {
        collectors?.cancel()
        active = transport
        limitHandled = false
        _screen.value = Screen.CONNECTING

        collectors = viewModelScope.launch {
            launch {
                transport.linkState.collect { state ->
                    _linkState.value = state
                    _screen.value = when (state) {
                        is LinkState.Connected -> Screen.DASHBOARD
                        is LinkState.Connecting -> Screen.CONNECTING
                        else -> Screen.CONNECT
                    }
                }
            }
            launch { transport.status.collect { _status.value = it } }
        }

        viewModelScope.launch { transport.connect(device) }
    }

    fun send(command: SocketCommand) {
        viewModelScope.launch { active.send(command) }
    }

    fun setBatteryLimit(limit: Int) {
        _batteryLimit.value = limit
        prefs.edit().putInt(KEY_LIMIT, limit).apply()
        limitHandled = false
        maybeCutForBattery()
    }

    fun disconnect() {
        collectors?.cancel()
        collectors = null
        active.disconnect()
        _status.value = SocketStatus.UNKNOWN
        _linkState.value = LinkState.Idle
        _screen.value = Screen.CONNECT
        refreshDevices()
    }

    /**
     * The feature the sensor cannot provide.
     *
     * A phone charging on 230 V draws about 20 mA, which is inside the ACS712's
     * noise - the socket genuinely cannot tell a charging phone from an empty
     * outlet. But the phone knows its own battery, so it says so.
     */
    private fun maybeCutForBattery() {
        if (limitHandled) return
        if (_linkState.value !is LinkState.Connected) return
        if (_batteryPercent.value < _batteryLimit.value) return
        if (!_status.value.state.isPowerOn) return

        limitHandled = true
        send(SocketCommand.CUT)
    }

    private companion object {
        const val KEY_ONBOARDED = "onboarded"
        const val KEY_LIMIT = "battery_limit"
    }
}
