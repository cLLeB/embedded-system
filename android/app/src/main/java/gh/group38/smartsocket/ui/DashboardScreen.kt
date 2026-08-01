package gh.group38.smartsocket.ui

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import gh.group38.smartsocket.data.SocketCommand
import gh.group38.smartsocket.data.SocketState
import gh.group38.smartsocket.data.SocketStatus
import java.util.Locale

@Composable
fun DashboardScreen(
    status: SocketStatus,
    deviceName: String,
    batteryLimit: Int,
    batteryPercent: Int,
    onCommand: (SocketCommand) -> Unit,
    onBatteryLimit: (Int) -> Unit,
    onDisconnect: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val tint = status.state.tint()

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(Ink)
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 22.dp, vertical = 32.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                SocketMark(size = 30.dp)
                Spacer(Modifier.height(0.dp))
                Column(Modifier.padding(start = 12.dp)) {
                    Text(
                        text = deviceName,
                        style = MaterialTheme.typography.titleMedium,
                        color = Bone,
                    )
                    Text(
                        text = if (status.relayClosed) "Outlet live" else "Outlet off",
                        style = MaterialTheme.typography.labelSmall,
                        color = if (status.relayClosed) Caution else Muted,
                    )
                }
            }
            Text(
                text = "Disconnect",
                style = MaterialTheme.typography.labelSmall,
                color = Faint,
                modifier = Modifier.clickableNoRipple(onDisconnect),
            )
        }

        Spacer(Modifier.height(30.dp))

        // --- the number ------------------------------------------------------
        Panel(accent = status.state.isAlarm, modifier = Modifier.fillMaxWidth()) {
            StatePill(label = status.state.label, tint = tint)
            Spacer(Modifier.height(22.dp))

            val amps by animateFloatAsState(
                targetValue = status.amps,
                animationSpec = tween(600),
                label = "amps",
            )
            Row(verticalAlignment = Alignment.Bottom) {
                Text(
                    text = String.format(Locale.UK, "%.2f", amps),
                    style = MaterialTheme.typography.displayLarge,
                    color = Bone,
                )
                Text(
                    text = "A",
                    fontSize = 24.sp,
                    fontWeight = FontWeight.Light,
                    color = Muted,
                    modifier = Modifier.padding(start = 8.dp, bottom = 12.dp),
                )
            }

            Spacer(Modifier.height(6.dp))
            Text(
                text = status.state.detail,
                style = MaterialTheme.typography.bodyMedium,
                color = Muted,
            )

            if (status.state == SocketState.CHARGING && status.peakMa > 0) {
                Spacer(Modifier.height(22.dp))
                TaperBar(status = status)
            }
        }

        Spacer(Modifier.height(16.dp))

        // --- controls --------------------------------------------------------
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            OutlineButton(
                text = "Cut power",
                onClick = { onCommand(SocketCommand.CUT) },
                enabled = status.state.isPowerOn,
                tint = Alarm,
                modifier = Modifier.weight(1f),
            )
            OutlineButton(
                text = if (status.state == SocketState.CUTOFF) "Turn back on" else "Re-arm",
                onClick = { onCommand(SocketCommand.REARM) },
                enabled = status.state == SocketState.CUTOFF || status.state.isAlarm,
                tint = Gold,
                modifier = Modifier.weight(1f),
            )
        }

        if (status.state == SocketState.CUTOFF) {
            Spacer(Modifier.height(10.dp))
            OutlineButton(
                text = "Check for a device now",
                onClick = { onCommand(SocketCommand.PROBE) },
                tint = Bone,
                modifier = Modifier.fillMaxWidth(),
            )
        }

        Spacer(Modifier.height(16.dp))

        // --- battery limit ---------------------------------------------------
        Panel(modifier = Modifier.fillMaxWidth()) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Column {
                    FieldLabel("This phone")
                    Spacer(Modifier.height(6.dp))
                    Text(
                        text = "Stop charging at $batteryLimit%",
                        style = MaterialTheme.typography.titleMedium,
                        color = Bone,
                    )
                }
                Text(
                    text = "$batteryPercent%",
                    style = MaterialTheme.typography.titleMedium,
                    color = if (batteryPercent >= batteryLimit) Live else Muted,
                )
            }

            Spacer(Modifier.height(8.dp))
            Text(
                text = "The socket can't measure a phone — the current is too small. " +
                    "This tells it directly.",
                style = MaterialTheme.typography.bodyMedium,
                color = Muted,
            )

            Spacer(Modifier.height(16.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf(80, 90, 100).forEach { limit ->
                    OutlineButton(
                        text = "$limit%",
                        onClick = { onBatteryLimit(limit) },
                        tint = if (limit == batteryLimit) Gold else Muted,
                        modifier = Modifier.weight(1f),
                    )
                }
            }
        }

        Spacer(Modifier.height(16.dp))

        // --- lifetime --------------------------------------------------------
        Panel(modifier = Modifier.fillMaxWidth()) {
            FieldLabel("Lifetime")
            Spacer(Modifier.height(16.dp))
            Row(modifier = Modifier.fillMaxWidth()) {
                Stat(
                    label = "Cutoffs",
                    value = status.cutoffCount.toString(),
                    modifier = Modifier.weight(1f),
                )
                Stat(
                    label = "Power saved",
                    value = formatDuration(status.totalSavedMs),
                    modifier = Modifier.weight(1f),
                )
                Stat(
                    label = "Peak",
                    value = String.format(Locale.UK, "%.2f A", status.peakAmps),
                    modifier = Modifier.weight(1f),
                )
            }
        }
    }
}

/**
 * Where the current sits between the cutoff threshold and the session peak.
 *
 * The one thing worth drawing: the whole product is "watch this fall past that
 * line", and a number alone does not show how close it is.
 */
@Composable
private fun TaperBar(status: SocketStatus) {
    val span = (status.peakMa - status.thresholdMa).coerceAtLeast(1)
    val above = (status.currentMa - status.thresholdMa).coerceAtLeast(0)
    val fraction by animateFloatAsState(
        targetValue = (above.toFloat() / span).coerceIn(0f, 1f),
        animationSpec = tween(600),
        label = "taper",
    )
    val barColor by animateColorAsState(
        targetValue = if (fraction < 0.15f) Caution else Live,
        animationSpec = tween(600),
        label = "taper-tint",
    )

    Column {
        Box(
            Modifier
                .fillMaxWidth()
                .height(6.dp)
                .background(Surface3, androidx.compose.foundation.shape.RoundedCornerShape(3.dp)),
        ) {
            Box(
                Modifier
                    .fillMaxWidth(fraction)
                    .height(6.dp)
                    .background(barColor, androidx.compose.foundation.shape.RoundedCornerShape(3.dp)),
            )
        }
        Spacer(Modifier.height(10.dp))
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(
                text = String.format(Locale.UK, "cuts below %.2f A", status.thresholdAmps),
                style = MaterialTheme.typography.labelSmall,
                color = Faint,
            )
            Text(
                text = String.format(Locale.UK, "peak %.2f A", status.peakAmps),
                style = MaterialTheme.typography.labelSmall,
                color = Faint,
            )
        }
    }
}

private fun SocketState.tint(): Color = when (this) {
    SocketState.CHARGING, SocketState.SETTLING -> Live
    SocketState.CUTOFF -> Gold
    SocketState.FAULT, SocketState.RELAY_STUCK -> Alarm
    SocketState.PROBING -> Caution
    else -> Muted
}

private fun formatDuration(ms: Long): String {
    val totalMinutes = ms / 60_000
    val hours = totalMinutes / 60
    val minutes = totalMinutes % 60
    return if (hours > 0) "${hours}h ${minutes}m" else "${minutes}m"
}
