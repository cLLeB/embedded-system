package gh.group38.smartsocket

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.core.tween
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.ContextCompat
import gh.group38.smartsocket.data.LinkState
import gh.group38.smartsocket.ui.ConnectScreen
import gh.group38.smartsocket.ui.ConnectingScreen
import gh.group38.smartsocket.ui.DashboardScreen
import gh.group38.smartsocket.ui.HistoryScreen
import gh.group38.smartsocket.ui.Ink
import gh.group38.smartsocket.ui.OnboardingScreen
import gh.group38.smartsocket.ui.SmartSocketTheme
import gh.group38.smartsocket.ui.SplashScreen

class MainActivity : ComponentActivity() {

    private val vm: SocketViewModel by viewModels()

    /**
     * Android 12 split Bluetooth into runtime permissions. Below 31 the legacy
     * manifest permissions are granted at install, so there is nothing to ask
     * for and the request must be skipped rather than failed.
     *
     * BLUETOOTH_SCAN is required, not optional: the module is Bluetooth LE and
     * a BLE serial module does not bond, so it never reaches the paired list and
     * scanning is the only way to find it.
     *
     * Below 12 the platform withholds BLE scan results without a location
     * permission, however unrelated to location the scan is - so that has to be
     * asked for there and only there.
     */
    private val permissions: Array<String> = buildList {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            add(Manifest.permission.BLUETOOTH_CONNECT)
            add(Manifest.permission.BLUETOOTH_SCAN)
        } else {
            add(Manifest.permission.ACCESS_FINE_LOCATION)
        }
        // 13+ made notifications opt-in. Asked for alongside Bluetooth rather
        // than at the moment of the first cutoff, which would be the worst
        // possible time to interrupt with a dialog.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            add(Manifest.permission.POST_NOTIFICATIONS)
        }
    }.toTypedArray()

    /** Bluetooth is what gates the device list; notifications are a bonus. */
    private val bluetoothPermissions: Array<String> =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.BLUETOOTH_SCAN)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

    private val requestPermissions =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { _ ->
            // Judged on Bluetooth only. Refusing notifications should not lock
            // the user out of the device list.
            vm.onPermissionResult(hasPermissions())
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        vm.onPermissionResult(hasPermissions())

        setContent {
            SmartSocketTheme {
                Surface(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(Ink),
                ) {
                    App(
                        vm = vm,
                        onRequestPermissions = ::askForPermissions,
                    )
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        vm.refreshDevices()
        askOnceForBatteryExemption()
    }

    private fun hasPermissions(): Boolean = bluetoothPermissions.all {
        ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
    }

    /**
     * Asks Android to stop dozing this app, once, and never again if refused.
     *
     * A connectedDevice foreground service survives stock Android's Doze, but the
     * OEM skins common on cheap handsets kill background apps regardless of
     * service type - and when they do, the link dies silently and the cutoff
     * never fires. The user is told it happened by nothing at all, which is the
     * worst failure this app has.
     *
     * Deferred until Bluetooth has been granted so it is not a second dialog
     * stacked on the first launch, and recorded as asked whether or not it was
     * allowed: pestering is worse than going without.
     */
    private fun askOnceForBatteryExemption() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return
        if (!hasPermissions()) return

        val prefs = getSharedPreferences("smart_socket", Context.MODE_PRIVATE)
        if (prefs.getBoolean(KEY_ASKED_DOZE, false)) return

        val power = getSystemService(PowerManager::class.java) ?: return
        if (power.isIgnoringBatteryOptimizations(packageName)) return

        prefs.edit().putBoolean(KEY_ASKED_DOZE, true).apply()

        // Not every ROM ships this screen, and a missing one must not take the
        // app down on launch.
        runCatching {
            startActivity(
                Intent(
                    Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,
                    Uri.parse("package:$packageName"),
                )
            )
        }
    }

    private fun askForPermissions() {
        if (permissions.isEmpty()) {
            vm.onPermissionResult(true)
        } else {
            requestPermissions.launch(permissions)
        }
    }

    private companion object {
        const val KEY_ASKED_DOZE = "asked_doze_exemption"
    }
}

@Composable
private fun App(vm: SocketViewModel, onRequestPermissions: () -> Unit) {
    val screen by vm.screen.collectAsState()
    val status by vm.status.collectAsState()
    val link by vm.linkState.collectAsState()
    val devices by vm.devices.collectAsState()
    val granted by vm.permissionGranted.collectAsState()
    val battery by vm.batteryPercent.collectAsState()
    val limit by vm.batteryLimit.collectAsState()
    val managing by vm.appManaging.collectAsState()
    val scanning by vm.scanning.collectAsState()
    val phoneCharging by vm.phoneCharging.collectAsState()
    val history by vm.history.collectAsState()
    val context = LocalContext.current

    AnimatedContent(
        targetState = screen,
        transitionSpec = { fadeIn(tween(340)).togetherWith(fadeOut(tween(220))) },
        label = "screen",
    ) { current ->
        when (current) {
            Screen.SPLASH -> SplashScreen(onDone = vm::onSplashDone)

            Screen.ONBOARDING -> OnboardingScreen(onDone = vm::onOnboardingDone)

            Screen.CONNECT -> ConnectScreen(
                devices = devices,
                linkState = link,
                bluetoothOn = vm.bluetoothOn(),
                permissionGranted = granted,
                scanning = scanning,
                onRequestPermission = onRequestPermissions,
                onScan = vm::startScan,
                onSelect = vm::connect,
                onDemo = vm::openDemo,
                modifier = Modifier.windowInsetsPadding(WindowInsets.systemBars),
            )

            Screen.CONNECTING -> ConnectingScreen(name = vm.connectedName)

            Screen.DASHBOARD -> DashboardScreen(
                status = status,
                linkState = link,
                deviceName = vm.connectedName,
                batteryLimit = limit,
                batteryPercent = battery,
                resumeAt = vm.resumeAt,
                appManaging = managing,
                phoneCharging = phoneCharging,
                onCommand = vm::send,
                onBatteryLimit = vm::setBatteryLimit,
                onAppManaging = vm::setAppManaging,
                onDisconnect = vm::disconnect,
                onHistory = vm::openHistory,
                modifier = Modifier.windowInsetsPadding(WindowInsets.systemBars),
            )

            Screen.HISTORY -> HistoryScreen(
                sessions = history,
                onBack = vm::closeHistory,
                onClear = vm::clearHistory,
                onExport = {
                    vm.exportHistory()?.let {
                        context.startActivity(Intent.createChooser(it, "Export charge history"))
                    }
                },
                modifier = Modifier.windowInsetsPadding(WindowInsets.systemBars),
            )
        }
    }
}
