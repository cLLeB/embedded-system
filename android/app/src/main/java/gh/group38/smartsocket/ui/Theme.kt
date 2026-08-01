package gh.group38.smartsocket.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp

/**
 * Gold on black.
 *
 * DYNAMIC COLOUR IS DELIBERATELY NOT USED. On Android 12+ `dynamicDarkColorScheme`
 * derives the palette from the user's wallpaper, which most often lands on the
 * lavender-purple Material default. That would repaint this app in someone
 * else's colours and undo every decision below.
 */

// Gold. Three steps, because one gold flattens: the mid tone carries actions,
// the light tone carries text on gold, the deep tone carries borders and shadow.
val Gold = Color(0xFFD4A537)
val GoldLight = Color(0xFFE8C46A)
val GoldDeep = Color(0xFF8A6A1C)
val GoldFaint = Color(0x1FD4A537)

// Black, in four steps so surfaces can stack without borders everywhere.
val Ink = Color(0xFF0B0B0C)
val Surface1 = Color(0xFF141416)
val Surface2 = Color(0xFF1C1C1F)
val Surface3 = Color(0xFF26262A)

// Warm off-white rather than pure white: against gold, #FFFFFF reads cold and
// makes the gold look green.
val Bone = Color(0xFFF2EEE6)
val Muted = Color(0xFF9A9488)
val Faint = Color(0xFF5E5A52)

// Semantic. Deliberately desaturated so they sit beside gold instead of
// fighting it.
val Live = Color(0xFF3FA96A)
val Alarm = Color(0xFFD0492F)
val Caution = Color(0xFFE0A93B)

private val Scheme = darkColorScheme(
    primary = Gold,
    onPrimary = Ink,
    primaryContainer = GoldDeep,
    onPrimaryContainer = Bone,

    secondary = GoldLight,
    onSecondary = Ink,

    background = Ink,
    onBackground = Bone,

    surface = Surface1,
    onSurface = Bone,
    surfaceVariant = Surface2,
    onSurfaceVariant = Muted,

    outline = Surface3,
    outlineVariant = Surface2,

    error = Alarm,
    onError = Bone,
)

private val AppTypography = Typography(
    displayLarge = TextStyle(
        fontSize = 64.sp, lineHeight = 64.sp,
        fontWeight = FontWeight.Light, letterSpacing = (-2).sp,
    ),
    headlineMedium = TextStyle(
        fontSize = 26.sp, lineHeight = 32.sp,
        fontWeight = FontWeight.SemiBold, letterSpacing = (-0.5).sp,
    ),
    titleMedium = TextStyle(
        fontSize = 17.sp, lineHeight = 24.sp, fontWeight = FontWeight.Medium,
    ),
    bodyLarge = TextStyle(
        fontSize = 16.sp, lineHeight = 24.sp, fontWeight = FontWeight.Normal,
    ),
    bodyMedium = TextStyle(
        fontSize = 14.sp, lineHeight = 20.sp, fontWeight = FontWeight.Normal,
    ),
    // Wide tracking on small uppercase labels is what stops a dark UI reading
    // as cramped.
    labelLarge = TextStyle(
        fontSize = 12.sp, lineHeight = 16.sp,
        fontWeight = FontWeight.SemiBold, letterSpacing = 1.4.sp,
    ),
    labelSmall = TextStyle(
        fontSize = 11.sp, lineHeight = 14.sp,
        fontWeight = FontWeight.Medium, letterSpacing = 1.1.sp,
    ),
)

@Composable
fun SmartSocketTheme(
    @Suppress("UNUSED_PARAMETER") darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit,
) {
    // Always dark. A gold-on-black identity has no light counterpart that keeps
    // the same character, and half a design is worse than one.
    MaterialTheme(
        colorScheme = Scheme,
        typography = AppTypography,
        content = content,
    )
}
