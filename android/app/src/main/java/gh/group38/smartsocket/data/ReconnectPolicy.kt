package gh.group38.smartsocket.data

/**
 * How long to wait before the nth attempt to rebuild a dropped link.
 *
 * Pure arithmetic with no Android and no coroutines in it, so the schedule can
 * be asserted in a unit test rather than discovered by standing in a doorway
 * with a phone. [SocketRepository] supplies the clock and the socket.
 *
 * The shape is exponential up to a one-minute ceiling. The two cases it has to
 * serve pull in opposite directions: a radio glitch wants a retry now, and a
 * user who walked to another room wants one in ten minutes' time. Doubling from
 * two seconds covers the first in the first few attempts, and the ceiling stops
 * the rest from costing anything - a retry a minute is nothing next to holding
 * the Bluetooth link open in the first place.
 *
 * It gives up rather than retrying forever. A foreground notification the user
 * cannot dismiss, sitting over a socket that is switched off at the wall, is
 * worse than an honest "lost it".
 */
object ReconnectPolicy {

    /** Roughly eight minutes of trying, in total. */
    const val MAX_ATTEMPTS = 12

    private const val FIRST_DELAY_MS = 2_000L
    private const val CEILING_MS = 60_000L

    /**
     * Wait before attempt [attempt], counted from zero, or null once the
     * schedule is exhausted and the caller should stop.
     */
    fun delayMsFor(attempt: Int): Long? {
        if (attempt < 0 || attempt >= MAX_ATTEMPTS) return null
        return minOf(FIRST_DELAY_MS shl attempt, CEILING_MS)
    }

    /** Total time the schedule spends waiting before it gives up. */
    val budgetMs: Long
        get() = (0 until MAX_ATTEMPTS).sumOf { delayMsFor(it) ?: 0L }
}
