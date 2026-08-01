package gh.group38.smartsocket.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed

/**
 * Click with no ripple.
 *
 * Material's default ripple is a grey wash, and on gold it reads as a smear
 * rather than a press. The buttons here signal touch by scale and colour
 * instead, which stays on-palette.
 */
fun Modifier.clickableNoRipple(onClick: () -> Unit): Modifier = composed {
    val interaction = remember { MutableInteractionSource() }
    clickable(
        interactionSource = interaction,
        indication = null,
        onClick = onClick,
    )
}
