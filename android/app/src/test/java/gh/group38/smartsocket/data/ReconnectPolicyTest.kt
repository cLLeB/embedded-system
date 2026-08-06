package gh.group38.smartsocket.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ReconnectPolicyTest {

    @Test
    fun `first retry is quick enough to cover a radio glitch`() {
        assertEquals(2_000L, ReconnectPolicy.delayMsFor(0))
    }

    @Test
    fun `waits double until they hit the ceiling`() {
        assertEquals(2_000L, ReconnectPolicy.delayMsFor(0))
        assertEquals(4_000L, ReconnectPolicy.delayMsFor(1))
        assertEquals(8_000L, ReconnectPolicy.delayMsFor(2))
        assertEquals(16_000L, ReconnectPolicy.delayMsFor(3))
        assertEquals(32_000L, ReconnectPolicy.delayMsFor(4))
    }

    @Test
    fun `never waits more than a minute`() {
        for (attempt in 0 until ReconnectPolicy.MAX_ATTEMPTS) {
            val delay = ReconnectPolicy.delayMsFor(attempt)
            assertNotNull("attempt $attempt should have a delay", delay)
            assertTrue("attempt $attempt waited ${delay}ms", delay!! <= 60_000L)
        }
    }

    @Test
    fun `gives up rather than retrying forever`() {
        assertNull(ReconnectPolicy.delayMsFor(ReconnectPolicy.MAX_ATTEMPTS))
        assertNull(ReconnectPolicy.delayMsFor(ReconnectPolicy.MAX_ATTEMPTS + 50))
    }

    @Test
    fun `a negative attempt is refused rather than shifted by a negative amount`() {
        assertNull(ReconnectPolicy.delayMsFor(-1))
    }

    /**
     * The point of the ceiling. Someone who walks out of range and comes back
     * after a few minutes should still find the socket connected, so the budget
     * has to be minutes rather than the seconds a plain exponential would give.
     */
    @Test
    fun `keeps trying for several minutes before it gives up`() {
        assertTrue(
            "budget was ${ReconnectPolicy.budgetMs}ms",
            ReconnectPolicy.budgetMs in 5 * 60_000L..15 * 60_000L,
        )
    }
}
