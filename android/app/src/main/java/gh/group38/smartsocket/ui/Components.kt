package gh.group38.smartsocket.ui

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.foundation.Canvas
import androidx.compose.material3.MaterialTheme

/** The socket face from the launcher icon, redrawn so it can be animated. */
@Composable
fun SocketMark(
    size: Dp = 96.dp,
    color: Color = Gold,
    modifier: Modifier = Modifier,
) {
    Canvas(modifier = modifier.size(size)) {
        val s = this.size.minDimension
        val stroke = s * 0.055f
        val inset = s * 0.16f
        val corner = s * 0.16f

        drawRoundRect(
            color = color,
            topLeft = Offset(inset, inset * 1.1f),
            size = Size(s - inset * 2, s - inset * 2.2f),
            cornerRadius = androidx.compose.ui.geometry.CornerRadius(corner, corner),
            style = Stroke(width = stroke),
        )

        val pin = s * 0.085f
        // Earth, top centre.
        drawLine(
            color = color,
            start = Offset(s * 0.5f, s * 0.33f),
            end = Offset(s * 0.5f, s * 0.44f),
            strokeWidth = pin,
            cap = StrokeCap.Round,
        )
        // Live and neutral, below.
        drawLine(
            color = color,
            start = Offset(s * 0.33f, s * 0.60f),
            end = Offset(s * 0.44f, s * 0.60f),
            strokeWidth = pin,
            cap = StrokeCap.Round,
        )
        drawLine(
            color = color,
            start = Offset(s * 0.56f, s * 0.60f),
            end = Offset(s * 0.67f, s * 0.60f),
            strokeWidth = pin,
            cap = StrokeCap.Round,
        )
    }
}

/** A gold arc that sweeps while something is happening. */
@Composable
fun LoadingRing(size: Dp = 132.dp, modifier: Modifier = Modifier) {
    val spin = rememberInfiniteTransition(label = "ring")
    val angle by spin.animateFloat(
        initialValue = 0f,
        targetValue = 360f,
        animationSpec = infiniteRepeatable(
            animation = tween(1400, easing = LinearEasing),
            repeatMode = RepeatMode.Restart,
        ),
        label = "sweep",
    )

    Canvas(modifier = modifier.size(size)) {
        val stroke = this.size.minDimension * 0.035f

        drawCircle(
            color = GoldFaint,
            radius = (this.size.minDimension - stroke) / 2f,
            style = Stroke(width = stroke),
        )
        drawArc(
            brush = Brush.sweepGradient(listOf(Color.Transparent, GoldDeep, Gold, GoldLight)),
            startAngle = angle,
            sweepAngle = 110f,
            useCenter = false,
            style = Stroke(width = stroke, cap = StrokeCap.Round),
        )
    }
}

/** Small uppercase label above a value. */
@Composable
fun FieldLabel(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text.uppercase(),
        style = MaterialTheme.typography.labelSmall,
        color = Faint,
        modifier = modifier,
    )
}

/** A raised panel. The one container used everywhere, so nothing drifts. */
@Composable
fun Panel(
    modifier: Modifier = Modifier,
    accent: Boolean = false,
    content: @Composable ColumnScope.() -> Unit,
) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(20.dp))
            .background(Surface1)
            .border(
                BorderStroke(1.dp, if (accent) GoldFaint else Surface3),
                RoundedCornerShape(20.dp),
            )
            .padding(20.dp),
        content = content,
    )
}

/** Status pill: a dot and a word. */
@Composable
fun StatePill(label: String, tint: Color, modifier: Modifier = Modifier) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = modifier
            .clip(CircleShape)
            .background(tint.copy(alpha = 0.12f))
            .border(BorderStroke(1.dp, tint.copy(alpha = 0.35f)), CircleShape)
            .padding(horizontal = 14.dp, vertical = 7.dp),
    ) {
        Box(
            Modifier
                .size(7.dp)
                .clip(CircleShape)
                .background(tint),
        )
        Spacer(Modifier.width(9.dp))
        Text(
            text = label.uppercase(),
            style = MaterialTheme.typography.labelSmall,
            color = tint,
        )
    }
}

/** Primary action. Solid gold, dark text. */
@Composable
fun GoldButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
) {
    val bg = if (enabled) Gold else Surface3
    val fg = if (enabled) Ink else Faint

    Box(
        modifier = modifier
            .fillMaxWidth()
            .height(54.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(bg)
            .then(if (enabled) Modifier.clickableNoRipple(onClick) else Modifier),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.titleMedium,
            color = fg,
            textAlign = TextAlign.Center,
        )
    }
}

/** Secondary action. Outline only, so it never competes with the gold. */
@Composable
fun OutlineButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    tint: Color = Bone,
) {
    val edge = if (enabled) Surface3 else Surface2
    val fg = if (enabled) tint else Faint

    Box(
        modifier = modifier
            .height(50.dp)
            .clip(RoundedCornerShape(14.dp))
            .border(BorderStroke(1.dp, edge), RoundedCornerShape(14.dp))
            .then(if (enabled) Modifier.clickableNoRipple(onClick) else Modifier),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.bodyMedium,
            color = fg,
            modifier = Modifier.padding(horizontal = 18.dp),
        )
    }
}

/** A labelled value in a row of statistics. */
@Composable
fun Stat(label: String, value: String, modifier: Modifier = Modifier) {
    Column(modifier = modifier, verticalArrangement = Arrangement.spacedBy(6.dp)) {
        FieldLabel(label)
        Text(
            text = value,
            style = MaterialTheme.typography.titleMedium,
            color = Bone,
        )
    }
}
