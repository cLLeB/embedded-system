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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.ui.draw.clip
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
import gh.group38.smartsocket.data.LinkState
import gh.group38.smartsocket.data.ReconnectPolicy
import gh.group38.smartsocket.data.SocketCommand
import gh.group38.smartsocket.data.SocketState
import gh.group38.smartsocket.data.SocketStatus
import java.util.Locale

@Composable
fun DashboardScreen(
    status: SocketStatus,
    linkState: LinkState,
    deviceName: String,
    batteryLimit: Int,
    batteryPercent: Int,
    resumeAt: Int,
    appManaging: Boolean,

    /**
     * Whether Android says this phone is taking a charge. The socket cannot
     * tell - that is the whole premise - so this is the only source of the fact.
     */
    phoneCharging: Boolean,
    onCommand: (SocketCommand) -> Unit,
    onBatteryLimit: (Int) -> Unit,
    onAppManaging: (Boolean) -> Unit,
    onDisconnect: () -> Unit,
    onHistory: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val reconnecting = linkState as? LinkState.Reconnecting
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
                        text = when {
                            reconnecting != null -> "Reconnecting"
                            status.relayClosed -> "Outlet live"
                            else -> "Outlet off"
                        },
                        style = MaterialTheme.typography.labelSmall,
                        color = when {
                            reconnecting != null -> Gold
                            status.relayClosed -> Caution
                            else -> Muted
                        },
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

        if (reconnecting != null) {
            Spacer(Modifier.height(20.dp))
            ReconnectingBanner(attempt = reconnecting.attempt)
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
                // "Waiting for something to be plugged in" is what the socket
                // honestly believes, and it is wrong in the one case this app
                // was built for. A phone charging on 230 V draws about 20 mA -
                // below a single ADC count on the ACS712-5A, and below
                // NoiseFloorMa - so the socket reads a true zero and concludes
                // the outlet is empty. The phone knows better, so it says so.
                text = if (phoneCharging && status.state == SocketState.READY) {
                    "Your phone is charging. The socket can't see it — 20 mA is " +
                        "smaller than its sensor can measure, which is why this " +
                        "app exists."
                } else {
                    status.state.detail
                },
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
            // Nothing can be commanded down a link that is not there. A button
            // that looks like it worked and did not is worse than a dead one on
            // a device that switches mains.
            val live = reconnecting == null

            OutlineButton(
                text = "Cut power",
                onClick = { onCommand(SocketCommand.CUT) },
                enabled = live && status.state.isPowerOn,
                tint = Alarm,
                modifier = Modifier.weight(1f),
            )
            OutlineButton(
                text = if (status.state == SocketState.CUTOFF) "Turn back on" else "Re-arm",
                onClick = { onCommand(SocketCommand.REARM) },
                enabled = live && (status.state == SocketState.CUTOFF || status.state.isAlarm),
                tint = Gold,
                modifier = Modifier.weight(1f),
            )
        }

        if (status.state == SocketState.CUTOFF) {
            Spacer(Modifier.height(10.dp))
            OutlineButton(
                text = "Check for a device now",
                onClick = { onCommand(SocketCommand.PROBE) },
                enabled = reconnecting == null,
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
                // Weighted, and the percentage unwrappable. Without this the
                // longer window text squeezed "72%" until the % wrapped onto its
                // own line under the number.
                Column(Modifier.weight(1f)) {
                    FieldLabel("This phone")
                    Spacer(Modifier.height(6.dp))
                    Text(
                        text = if (appManaging) {
                            "Charge to $batteryLimit%, back on at $resumeAt%"
                        } else {
                            "The socket decides for itself"
                        },
                        style = MaterialTheme.typography.titleMedium,
                        color = Bone,
                    )
                }
                Spacer(Modifier.width(12.dp))
                Text(
                    text = "$batteryPercent%",
                    style = MaterialTheme.typography.titleMedium,
                    color = if (batteryPercent >= batteryLimit) Live else Muted,
                    maxLines = 1,
                    softWrap = false,
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
                        tint = if (limit == batteryLimit && appManaging) Gold else Muted,
                        modifier = Modifier.weight(1f),
                    )
                }
            }

            Spacer(Modifier.height(12.dp))

            // Switching this off tells the socket to go back to deciding for
            // itself, which is the only sensible thing for it to do when nobody
            // is managing it - not to sit there doing nothing.
            OutlineButton(
                text = if (appManaging) "Let the socket decide" else "Let this app manage charging",
                onClick = { onAppManaging(!appManaging) },
                tint = if (appManaging) Muted else Gold,
                modifier = Modifier.fillMaxWidth(),
            )
        }

        Spacer(Modifier.height(16.dp))

        // --- lifetime --------------------------------------------------------
        Panel(modifier = Modifier.fillMaxWidth()) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                FieldLabel("Lifetime")
                Text(
                    text = "History",
                    style = MaterialTheme.typography.labelSmall,
                    color = Gold,
                    modifier = Modifier.clickableNoRipple(onHistory),
                )
            }
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
 * Says the reading is old, and that something is being done about it.
 *
 * The numbers below stay on screen rather than being blanked: the last thing
 * the socket said is still the most useful thing on the page, as long as it is
 * labelled as the last thing rather than the current one.
 */
@Composable
private fun ReconnectingBanner(attempt: Int) {
    Panel(accent = true, modifier = Modifier.fillMaxWidth()) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(
                Modifier
                    .size(7.dp)
                    .clip(androidx.compose.foundation.shape.CircleShape)
                    .background(Gold),
            )
            Text(
                text = "Reconnecting",
                style = MaterialTheme.typography.titleMedium,
                color = Bone,
                modifier = Modifier.padding(start = 12.dp),
            )
            Spacer(Modifier.weight(1f))
            Text(
                text = "$attempt of ${ReconnectPolicy.MAX_ATTEMPTS}",
                style = MaterialTheme.typography.labelSmall,
                color = Faint,
            )
        }
        Spacer(Modifier.height(8.dp))
        Text(
            text = "The link dropped. The readings below are the last ones the " +
                "socket sent. It keeps working on its own while this reconnects.",
            style = MaterialTheme.typography.bodyMedium,
            color = Muted,
        )
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
