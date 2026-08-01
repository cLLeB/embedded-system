package gh.group38.smartsocket.ui

import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.unit.dp
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import kotlinx.coroutines.delay

/**
 * Holds for a beat, then hands over.
 *
 * A splash exists to cover a slow start, and this app has none - so it earns its
 * place by being where the mark is introduced, not by pretending to load. Hence
 * the short, fixed hold rather than a spinner tied to nothing.
 */
@Composable
fun SplashScreen(onDone: () -> Unit) {
    var shown by remember { mutableStateOf(false) }

    val scale by animateFloatAsState(
        targetValue = if (shown) 1f else 0.82f,
        animationSpec = spring(dampingRatio = Spring.DampingRatioMediumBouncy, stiffness = Spring.StiffnessLow),
        label = "mark-scale",
    )
    val fade by animateFloatAsState(
        targetValue = if (shown) 1f else 0f,
        animationSpec = tween(600),
        label = "mark-fade",
    )

    LaunchedEffect(Unit) {
        shown = true
        delay(1500)
        onDone()
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.radialGradient(
                    colors = listOf(Surface1, Ink),
                    radius = 900f,
                )
            ),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center,
        ) {
            SocketMark(
                size = 104.dp,
                modifier = Modifier
                    .scale(scale)
                    .alpha(fade),
            )
            Spacer(Modifier.height(28.dp))
            Text(
                text = "SMART SOCKET",
                style = MaterialTheme.typography.labelLarge,
                color = Gold,
                modifier = Modifier.alpha(fade),
            )
            Spacer(Modifier.height(8.dp))
            Text(
                text = "Cuts the wall when your battery is full",
                style = MaterialTheme.typography.bodyMedium,
                color = Muted,
                modifier = Modifier.alpha(fade * 0.9f),
            )
        }
    }
}
