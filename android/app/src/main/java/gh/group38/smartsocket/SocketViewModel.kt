package gh.group38.smartsocket

import android.app.Application
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import gh.group38.smartsocket.data.LinkState
import gh.group38.smartsocket.data.MockTransport
import gh.group38.smartsocket.data.SocketCommand
import gh.group38.smartsocket.data.SocketDevice
import gh.group38.smartsocket.data.SocketStatus
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

enum class Screen { SPLASH, ONBOARDING, CONNECT, CONNECTING, DASHBOARD, HISTORY }

/**
 * Screen state and the phone's own battery.
 *
 * The link itself lives in [gh.group38.smartsocket.data.SocketRepository] on the
 * Application, not here - a connection owned by a ViewModel dies with the
 * screen, and can only notify someone already looking at it.
 */
class SocketViewModel(app: Application) : AndroidViewModel(app) {

    private val prefs = app.getSharedPreferences("smart_socket", Context.MODE_PRIVATE)
    private val repo = (app as SmartSocketApp).repository

    private val _screen = MutableStateFlow(Screen.SPLASH)
    val screen: StateFlow<Screen> = _screen.asStateFlow()

    val status: StateFlow<SocketStatus> = repo.status
    val linkState: StateFlow<LinkState> = repo.linkState
    val history = repo.history.sessions

    private val _devices = MutableStateFlow<List<SocketDevice>>(emptyList())
    val devices: StateFlow<List<SocketDevice>> = _devices.asStateFlow()

    private val _permissionGranted = MutableStateFlow(false)
    val permissionGranted: StateFlow<Boolean> = _permissionGranted.asStateFlow()

    private val _batteryPercent = MutableStateFlow(0)
    val batteryPercent: StateFlow<Int> = _batteryPercent.asStateFlow()

    private val _batteryLimit = MutableStateFlow(prefs.getInt(KEY_LIMIT, 90))
    val batteryLimit: StateFlow<Int> = _batteryLimit.asStateFlow()

    val connectedName: String get() = repo.deviceName

    /** Set once per connection, so crossing the limit only fires one cut. */
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

        // Follow the link, so a drop while the app is open returns to the picker
        // and the foreground service is not left running for nothing.
        viewModelScope.launch {
            repo.linkState.collect { state ->
                when (state) {
                    is LinkState.Connected -> {
                        SocketService.start(getApplication())
                        if (_screen.value == Screen.CONNECTING) _screen.value = Screen.DASHBOARD
                    }

                    is LinkState.Connecting -> _screen.value = Screen.CONNECTING

                    else -> {
                        SocketService.stop(getApplication())
                        if (_screen.value == Screen.DASHBOARD || _screen.value == Screen.CONNECTING) {
                            _screen.value = Screen.CONNECT
                        }
                    }
                }
            }
        }
    }

    override fun onCleared() {
        runCatching { getApplication<Application>().unregisterReceiver(batteryReceiver) }
        super.onCleared()
    }

    fun onSplashDone() {
        _screen.value = when {
            repo.isConnected -> Screen.DASHBOARD
            prefs.getBoolean(KEY_ONBOARDED, false) -> Screen.CONNECT
            else -> Screen.ONBOARDING
        }
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
        _devices.value = if (_permissionGranted.value) repo.pairedDevices() else emptyList()
    }

    fun bluetoothOn(): Boolean = repo.bluetoothOn()

    fun connect(device: SocketDevice) {
        limitHandled = false
        _screen.value = Screen.CONNECTING
        repo.connect(device)
    }

    fun openDemo() = connect(MockTransport.DEVICE)

    fun send(command: SocketCommand) = repo.send(command)

    fun openHistory() {
        _screen.value = Screen.HISTORY
    }

    fun closeHistory() {
        _screen.value = if (repo.isConnected) Screen.DASHBOARD else Screen.CONNECT
    }

    fun clearHistory() = repo.history.clear()

    fun setBatteryLimit(limit: Int) {
        _batteryLimit.value = limit
        prefs.edit().putInt(KEY_LIMIT, limit).apply()
        limitHandled = false
        maybeCutForBattery()
    }

    fun disconnect() {
        repo.disconnect()
        _screen.value = Screen.CONNECT
        refreshDevices()
    }

    /**
     * The feature the sensor cannot provide.
     *
     * A phone charging on 230 V draws about 20 mA, inside the ACS712's noise, so
     * the socket cannot tell a charging phone from an empty outlet. The phone
     * knows its own battery, so it says so.
     */
    private fun maybeCutForBattery() {
        if (limitHandled) return
        if (!repo.isConnected) return
        if (_batteryPercent.value < _batteryLimit.value) return
        if (!repo.status.value.state.isPowerOn) return

        limitHandled = true
        send(SocketCommand.CUT)
    }

    private companion object {
        const val KEY_ONBOARDED = "onboarded"
        const val KEY_LIMIT = "battery_limit"
    }
}
