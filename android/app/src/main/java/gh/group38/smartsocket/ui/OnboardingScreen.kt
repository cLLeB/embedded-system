package gh.group38.smartsocket.ui

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
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
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp

private data class Page(
    val kicker: String,
    val title: String,
    val body: String,
)

private val pages = listOf(
    Page(
        kicker = "What it does",
        title = "It stops charging\nwhen you're full",
        body = "Leaving a laptop on the charger at 100% wears the battery down. " +
            "This socket watches the current, notices when charging tapers off, " +
            "and cuts the wall.",
    ),
    Page(
        kicker = "No buttons needed",
        title = "It looks again\non its own",
        body = "After cutting, the socket checks every so often to see whether " +
            "something new is plugged in, or whether your battery has drained. " +
            "It comes back by itself.",
    ),
    Page(
        kicker = "Your phone helps",
        title = "Set a charge limit",
        body = "The socket can't see a phone charging — the current is too small " +
            "to measure. But your phone knows its own battery, so it can tell the " +
            "socket when to stop.",
    ),
)

@Composable
fun OnboardingScreen(onDone: () -> Unit) {
    var index by remember { mutableIntStateOf(0) }
    val page = pages[index]
    val last = index == pages.lastIndex

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Ink)
            .padding(horizontal = 28.dp, vertical = 40.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            SocketMark(size = 34.dp)

            if (!last) {
                Text(
                    text = "Skip",
                    style = MaterialTheme.typography.bodyMedium,
                    color = Faint,
                    modifier = Modifier.clickableNoRipple(onDone),
                )
            }
        }

        Spacer(Modifier.height(56.dp))

        AnimatedContent(
            targetState = index,
            transitionSpec = {
                (slideInHorizontally { it / 3 } + fadeIn(tween(320)))
                    .togetherWith(slideOutHorizontally { -it / 3 } + fadeOut(tween(180)))
            },
            label = "page",
        ) { _ ->
            Column {
                Text(
                    text = page.kicker.uppercase(),
                    style = MaterialTheme.typography.labelSmall,
                    color = Gold,
                )
                Spacer(Modifier.height(18.dp))
                Text(
                    text = page.title,
                    style = MaterialTheme.typography.headlineMedium,
                    color = Bone,
                )
                Spacer(Modifier.height(18.dp))
                Text(
                    text = page.body,
                    style = MaterialTheme.typography.bodyLarge,
                    color = Muted,
                )
            }
        }

        Spacer(Modifier.weight(1f))

        Row(verticalAlignment = Alignment.CenterVertically) {
            pages.indices.forEach { i ->
                val active = i == index
                val w by animateDpAsState(
                    targetValue = if (active) 26.dp else 7.dp,
                    animationSpec = tween(260),
                    label = "dot",
                )
                Box(
                    Modifier
                        .height(7.dp)
                        .width(w)
                        .clip(CircleShape)
                        .background(if (active) Gold else Surface3),
                )
                Spacer(Modifier.width(7.dp))
            }
        }

        Spacer(Modifier.height(28.dp))

        GoldButton(
            text = if (last) "Get started" else "Next",
            onClick = { if (last) onDone() else index++ },
        )

        Spacer(Modifier.height(10.dp))

        Text(
            text = "Group 38 · Project 38",
            style = MaterialTheme.typography.labelSmall,
            color = Faint,
            textAlign = TextAlign.Center,
            modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
        )
    }
}
