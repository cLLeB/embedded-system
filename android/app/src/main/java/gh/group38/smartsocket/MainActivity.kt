package gh.group38.smartsocket

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
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
import androidx.core.content.ContextCompat
import gh.group38.smartsocket.data.LinkState
import gh.group38.smartsocket.ui.ConnectScreen
import gh.group38.smartsocket.ui.ConnectingScreen
import gh.group38.smartsocket.ui.DashboardScreen
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
     */
    private val permissions: Array<String> =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.BLUETOOTH_SCAN)
        } else {
            emptyArray()
        }

    private val requestPermissions =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { result ->
            vm.onPermissionResult(result.values.all { it })
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
    }

    private fun hasPermissions(): Boolean = permissions.all {
        ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
    }

    private fun askForPermissions() {
        if (permissions.isEmpty()) {
            vm.onPermissionResult(true)
        } else {
            requestPermissions.launch(permissions)
        }
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
                onRequestPermission = onRequestPermissions,
                onSelect = vm::connect,
                onDemo = vm::openDemo,
                modifier = Modifier.windowInsetsPadding(WindowInsets.systemBars),
            )

            Screen.CONNECTING -> ConnectingScreen(name = vm.connectedName)

            Screen.DASHBOARD -> DashboardScreen(
                status = status,
                deviceName = vm.connectedName,
                batteryLimit = limit,
                batteryPercent = battery,
                onCommand = vm::send,
                onBatteryLimit = vm::setBatteryLimit,
                onDisconnect = vm::disconnect,
                modifier = Modifier.windowInsetsPadding(WindowInsets.systemBars),
            )
        }
    }
}
