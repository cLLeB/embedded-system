package gh.group38.smartsocket.ui

import androidx.compose.foundation.Canvas
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
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.unit.dp
import gh.group38.smartsocket.data.ChargeSession
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun HistoryScreen(
    sessions: List<ChargeSession>,
    onBack: () -> Unit,
    onClear: () -> Unit,
    modifier: Modifier = Modifier,
) {
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
            Text(
                text = "Back",
                style = MaterialTheme.typography.labelSmall,
                color = Faint,
                modifier = Modifier.clickableNoRipple(onBack),
            )
            if (sessions.isNotEmpty()) {
                Text(
                    text = "Clear",
                    style = MaterialTheme.typography.labelSmall,
                    color = Faint,
                    modifier = Modifier.clickableNoRipple(onClear),
                )
            }
        }

        Spacer(Modifier.height(24.dp))
        Text(
            text = "History",
            style = MaterialTheme.typography.headlineMedium,
            color = Bone,
        )
        Spacer(Modifier.height(6.dp))
        Text(
            text = if (sessions.isEmpty()) {
                "Charges appear here once the socket has cut power at least once."
            } else {
                "${sessions.size} ${if (sessions.size == 1) "charge" else "charges"} recorded"
            },
            style = MaterialTheme.typography.bodyMedium,
            color = Muted,
        )

        if (sessions.isEmpty()) return@Column

        Spacer(Modifier.height(24.dp))

        Panel(modifier = Modifier.fillMaxWidth()) {
            FieldLabel("Peak current per charge")
            Spacer(Modifier.height(18.dp))
            PeakChart(
                sessions = sessions.take(20).reversed(),
                modifier = Modifier
                    .fillMaxWidth()
                    .height(120.dp),
            )
            Spacer(Modifier.height(12.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Text(
                    text = "oldest",
                    style = MaterialTheme.typography.labelSmall,
                    color = Faint,
                )
                Text(
                    text = "newest",
                    style = MaterialTheme.typography.labelSmall,
                    color = Faint,
                )
            }
        }

        Spacer(Modifier.height(16.dp))

        sessions.forEach { session ->
            SessionRow(session)
            Spacer(Modifier.height(10.dp))
        }
    }
}

/**
 * Bars, not a line.
 *
 * Each charge is a discrete event, not a sample of something continuous, and a
 * line drawn between them implies values in the gaps that were never measured.
 */
@Composable
private fun PeakChart(sessions: List<ChargeSession>, modifier: Modifier = Modifier) {
    val maxMa = (sessions.maxOfOrNull { it.peakMa } ?: 1).coerceAtLeast(1)

    Canvas(modifier = modifier) {
        if (sessions.isEmpty()) return@Canvas

        val gap = size.width * 0.02f
        val barWidth = ((size.width - gap * (sessions.size - 1)) / sessions.size)
            .coerceAtLeast(2f)

        sessions.forEachIndexed { index, session ->
            val fraction = session.peakMa.toFloat() / maxMa
            val barHeight = (size.height * fraction).coerceAtLeast(2f)
            val x = index * (barWidth + gap)

            // Faint full-height track, so short bars still read as "short"
            // rather than as missing.
            drawRoundRect(
                color = Surface3,
                topLeft = Offset(x, 0f),
                size = Size(barWidth, size.height),
                cornerRadius = CornerRadius(barWidth / 2, barWidth / 2),
            )
            drawRoundRect(
                color = if (index == sessions.lastIndex) GoldLight else Gold,
                topLeft = Offset(x, size.height - barHeight),
                size = Size(barWidth, barHeight),
                cornerRadius = CornerRadius(barWidth / 2, barWidth / 2),
            )
        }
    }
}

@Composable
private fun SessionRow(session: ChargeSession) {
    val stamp = remember(session.endedAtMillis) {
        SimpleDateFormat("d MMM · HH:mm", Locale.UK).format(Date(session.endedAtMillis))
    }

    Panel(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = stamp,
                style = MaterialTheme.typography.titleMedium,
                color = Bone,
            )
            Text(
                text = String.format(Locale.UK, "%.2f A peak", session.peakAmps),
                style = MaterialTheme.typography.bodyMedium,
                color = Gold,
            )
        }

        Spacer(Modifier.height(14.dp))

        Row(modifier = Modifier.fillMaxWidth()) {
            Stat(
                label = "Charged for",
                value = formatShort(session.durationMs),
                modifier = Modifier.weight(1f),
            )
            Stat(
                label = "Cut at",
                value = String.format(Locale.UK, "%.2f A", session.cutAtMa / 1000f),
                modifier = Modifier.weight(1f),
            )
            Stat(
                label = "Taper",
                value = "${(session.taperFraction * 100).toInt()}%",
                modifier = Modifier.weight(1f),
            )
        }
    }
}

private fun formatShort(ms: Long): String {
    val minutes = ms / 60_000
    return when {
        minutes >= 60 -> "${minutes / 60}h ${minutes % 60}m"
        minutes > 0 -> "${minutes}m"
        else -> "${ms / 1000}s"
    }
}
