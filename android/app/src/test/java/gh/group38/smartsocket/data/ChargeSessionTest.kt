package gh.group38.smartsocket.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ChargeSessionTest {

    @Test
    fun `a session survives a round trip through the file`() {
        val session = ChargeSession(
            endedAtMillis = 1_700_000_000_000,
            peakMa = 158,
            cutAtMa = 62,
            durationMs = 4_512_000,
        )
        assertEquals(session, ChargeSession.decode(session.encode()))
    }

    @Test
    fun `a corrupt line is dropped rather than half read`() {
        assertNull(ChargeSession.decode(""))
        assertNull(ChargeSession.decode("1,2,3"))
        assertNull(ChargeSession.decode("1,2,3,4,5"))
        assertNull(ChargeSession.decode("1,2,3,x"))
    }

    @Test
    fun `taper is the fraction of peak the current fell to`() {
        assertEquals(
            0.4f,
            ChargeSession(0, peakMa = 150, cutAtMa = 60, durationMs = 0).taperFraction,
            0.001f,
        )
    }

    /**
     * A cutoff recorded before any peak was seen would otherwise divide by zero.
     * It happens on the trickle-rejection path, where the socket cuts a load it
     * never saw charge.
     */
    @Test
    fun `a session with no peak reports no taper instead of dividing by zero`() {
        assertEquals(
            0f,
            ChargeSession(0, peakMa = 0, cutAtMa = 0, durationMs = 0).taperFraction,
            0.001f,
        )
    }

    @Test
    fun `a cut above the peak is clamped rather than shown over one hundred percent`() {
        assertEquals(
            1f,
            ChargeSession(0, peakMa = 100, cutAtMa = 180, durationMs = 0).taperFraction,
            0.001f,
        )
    }
}
