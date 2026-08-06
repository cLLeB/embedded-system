package gh.group38.smartsocket.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import gh.group38.smartsocket.data.LinkState
import gh.group38.smartsocket.data.SocketDevice

@Composable
fun ConnectScreen(
    devices: List<SocketDevice>,
    linkState: LinkState,
    bluetoothOn: Boolean,
    permissionGranted: Boolean,
    onRequestPermission: () -> Unit,
    onSelect: (SocketDevice) -> Unit,
    onDemo: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .background(Ink)
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 24.dp, vertical = 40.dp),
    ) {
        SocketMark(size = 40.dp)
        Spacer(Modifier.height(24.dp))

        Text(
            text = "Connect",
            style = MaterialTheme.typography.headlineMedium,
            color = Bone,
        )
        Spacer(Modifier.height(8.dp))
        Text(
            text = "Pair the socket in your phone's Bluetooth settings first, then pick it here.",
            style = MaterialTheme.typography.bodyMedium,
            color = Muted,
        )

        Spacer(Modifier.height(28.dp))

        when {
            !permissionGranted -> Panel(accent = true) {
                Text(
                    text = "Bluetooth permission",
                    style = MaterialTheme.typography.titleMedium,
                    color = Bone,
                )
                Spacer(Modifier.height(6.dp))
                Text(
                    text = "Android needs your permission before the app can see paired devices.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = Muted,
                )
                Spacer(Modifier.height(16.dp))
                GoldButton(text = "Allow", onClick = onRequestPermission)
            }

            !bluetoothOn -> Panel(accent = true) {
                Text(
                    text = "Bluetooth is off",
                    style = MaterialTheme.typography.titleMedium,
                    color = Bone,
                )
                Spacer(Modifier.height(6.dp))
                Text(
                    text = "Turn it on in your phone's settings, then come back.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = Muted,
                )
            }

            devices.isEmpty() -> Panel {
                Text(
                    text = "No paired devices",
                    style = MaterialTheme.typography.titleMedium,
                    color = Bone,
                )
                Spacer(Modifier.height(6.dp))
                Text(
                    text = "Pair the HC-05 in Android's Bluetooth settings. The code is usually 1234 or 0000.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = Muted,
                )
            }

            else -> Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                devices.forEach { device ->
                    DeviceRow(device = device, onClick = { onSelect(device) })
                }
            }
        }

        if (linkState is LinkState.Failed) {
            Spacer(Modifier.height(16.dp))
            Text(
                text = linkState.reason,
                style = MaterialTheme.typography.bodyMedium,
                color = Alarm,
            )
        }

        Spacer(Modifier.height(32.dp))

        // Present from the start, not hidden behind a debug flag: the socket is a
        // physical object that may be in another room, and the app should still
        // be worth opening.
        Panel {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(
                        text = "Try it without hardware",
                        style = MaterialTheme.typography.titleMedium,
                        color = Bone,
                    )
                    Spacer(Modifier.height(4.dp))
                    Text(
                        text = "Runs a simulated socket through a full charge and cutoff.",
                        style = MaterialTheme.typography.bodyMedium,
                        color = Muted,
                    )
                }
            }
            Spacer(Modifier.height(16.dp))
            OutlineButton(
                text = "Open demo",
                onClick = onDemo,
                tint = Gold,
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}

@Composable
private fun DeviceRow(device: SocketDevice, onClick: () -> Unit) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier
            .fillMaxWidth()
            .clip(androidx.compose.foundation.shape.RoundedCornerShape(16.dp))
            .background(Surface1)
            .clickableNoRipple(onClick)
            .padding(18.dp),
    ) {
        Box(
            Modifier
                .size(38.dp)
                .clip(CircleShape)
                .background(GoldFaint),
            contentAlignment = Alignment.Center,
        ) {
            SocketMark(size = 22.dp)
        }
        Spacer(Modifier.width(14.dp))
        Column(Modifier.weight(1f)) {
            Text(
                text = device.name,
                style = MaterialTheme.typography.titleMedium,
                color = Bone,
            )
            // The radio, alongside the address. Most modules sold as an "HC-05"
            // are Bluetooth LE parts wearing the name, and which one you have
            // decides everything about how the app talks to it - so it is worth
            // saying out loud rather than leaving the user to wonder why two
            // identically-named devices behave differently.
            Text(
                text = when (device.kind) {
                    gh.group38.smartsocket.data.LinkKind.BLE ->
                        "${device.address}  ·  Bluetooth LE"
                    gh.group38.smartsocket.data.LinkKind.CLASSIC ->
                        "${device.address}  ·  Bluetooth Classic"
                    gh.group38.smartsocket.data.LinkKind.DEMO -> device.address
                },
                style = MaterialTheme.typography.labelSmall,
                color = Faint,
            )
        }
        Text(
            text = "Connect",
            style = MaterialTheme.typography.labelSmall,
            color = Gold,
        )
    }
}

/** Shown while the socket is being reached. */
@Composable
fun ConnectingScreen(name: String) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Ink),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        Box(contentAlignment = Alignment.Center) {
            LoadingRing(size = 140.dp)
            SocketMark(size = 56.dp)
        }
        Spacer(Modifier.height(28.dp))
        Text(
            text = "Connecting",
            style = MaterialTheme.typography.titleMedium,
            color = Bone,
        )
        Spacer(Modifier.height(6.dp))
        Text(
            text = name,
            style = MaterialTheme.typography.bodyMedium,
            color = Muted,
        )
    }
}
