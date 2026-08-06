package gh.group38.smartsocket

import android.app.Application
import android.content.Context
import android.content.Intent
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import gh.group38.smartsocket.data.HistoryExport
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
 * Screen state, and nothing that has to outlive the screen.
 *
 * The link lives in [gh.group38.smartsocket.data.SocketRepository] and the
 * battery cutoff in [gh.group38.smartsocket.data.BatteryWatcher], both on the
 * Application - anything owned by a ViewModel dies when the activity is
 * destroyed, and can only ever serve someone already looking at it. What is left
 * here is which screen to show, which is exactly what should die with the screen.
 */
class SocketViewModel(app: Application) : AndroidViewModel(app) {

    private val prefs = app.getSharedPreferences("smart_socket", Context.MODE_PRIVATE)
    private val repo = (app as SmartSocketApp).repository
    private val battery = (app as SmartSocketApp).battery

    private val _screen = MutableStateFlow(Screen.SPLASH)
    val screen: StateFlow<Screen> = _screen.asStateFlow()

    val status: StateFlow<SocketStatus> = repo.status
    val linkState: StateFlow<LinkState> = repo.linkState
    val history = repo.history.sessions

    private val _devices = MutableStateFlow<List<SocketDevice>>(emptyList())
    val devices: StateFlow<List<SocketDevice>> = _devices.asStateFlow()

    private val _permissionGranted = MutableStateFlow(false)
    val permissionGranted: StateFlow<Boolean> = _permissionGranted.asStateFlow()

    /** Read-only here. The watch itself runs on the Application. */
    val batteryPercent: StateFlow<Int> = battery.percent
    val batteryLimit: StateFlow<Int> = battery.limit
    val appManaging: StateFlow<Boolean> = battery.managing

    /** Where power comes back on, ten points below the limit. */
    val resumeAt: Int get() = battery.resumeAt

    /** Names the BLE module once a Bluetooth LE link is up. */
    val bleProfileName: String? get() = repo.bleProfileName

    val connectedName: String get() = repo.deviceName

    init {
        // Screen navigation only. Starting and stopping the foreground service
        // used to happen here too, which left it running whenever a link failed
        // for good after this ViewModel was gone; SmartSocketApp owns that now.
        viewModelScope.launch {
            repo.linkState.collect { state ->
                when (state) {
                    is LinkState.Connected -> {
                        if (_screen.value == Screen.CONNECTING) _screen.value = Screen.DASHBOARD
                    }

                    is LinkState.Connecting -> _screen.value = Screen.CONNECTING

                    // Deliberately does nothing. The link is coming back on its
                    // own, so the screen stays where it is - bouncing the user to
                    // the picker would throw away the one case the background
                    // service exists for.
                    is LinkState.Reconnecting -> Unit

                    else -> {
                        if (_screen.value == Screen.DASHBOARD || _screen.value == Screen.CONNECTING) {
                            _screen.value = Screen.CONNECT
                        }
                    }
                }
            }
        }
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

    /** Null when there is nothing recorded yet, or the copy could not be made. */
    fun exportHistory(): Intent? =
        HistoryExport.shareIntent(getApplication(), repo.history.sessions.value)

    fun setBatteryLimit(limit: Int) = battery.setLimit(limit)

    fun setAppManaging(enabled: Boolean) = battery.setManaging(enabled)

    fun disconnect() {
        // Give the socket its own judgement back before dropping the link, so it
        // is never left with nothing watching the charge. A link that dies on
        // its own cannot do this, which is why the claim also expires on a timer.
        battery.handBack()

        repo.disconnect()
        _screen.value = Screen.CONNECT
        refreshDevices()
    }

    private companion object {
        const val KEY_ONBOARDED = "onboarded"
    }
}
