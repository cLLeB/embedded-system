package gh.group38.smartsocket.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The contract with `HalTelemetry::publish`. If these and the firmware ever
 * disagree the app shows a number that was never measured, which is the one
 * failure mode a socket that switches mains must not have.
 */
class StatusParserTest {

    @Test
    fun `a real status line decodes field for field`() {
        val status = StatusParser.parse(LINE)!!

        assertEquals(SocketState.CHARGING, status.state)
        assertEquals(142, status.currentMa)
        assertEquals(158, status.peakMa)
        assertEquals(100, status.thresholdMa)
        assertEquals(45_230L, status.sessionElapsedMs)
        assertEquals(3, status.cutoffCount)
        assertEquals(1_800_000L, status.totalSavedMs)
        assertTrue(status.relayClosed)
    }

    @Test
    fun `the relay flag is a flag, not a truthy number`() {
        assertFalse(StatusParser.parse("S,4,0,0,0,0,3,1800000,0")!!.relayClosed)
        assertTrue(StatusParser.parse("S,4,0,0,0,0,3,1800000,1")!!.relayClosed)
    }

    @Test
    fun `a line ending survives the trip`() {
        assertNotNull(StatusParser.parse("$LINE\r"))
        assertNotNull(StatusParser.parse("  $LINE  "))
    }

    /**
     * A Bluetooth link drops mid-line every time the phone leaves the room, so
     * a truncated line is the normal case. Half a status is not a status.
     */
    @Test
    fun `a truncated line is rejected rather than half read`() {
        assertNull(StatusParser.parse("S,3,142,158"))
        assertNull(StatusParser.parse("S,3,142,158,100,45230,3,1800000"))
    }

    @Test
    fun `extra fields are rejected, not ignored`() {
        assertNull(StatusParser.parse("$LINE,99"))
    }

    @Test
    fun `noise on the wire is not a status`() {
        assertNull(StatusParser.parse(""))
        assertNull(StatusParser.parse("OK"))
        assertNull(StatusParser.parse("+CONNECTED"))
        assertNull(StatusParser.parse("3,142,158,100,45230,3,1800000,1"))
    }

    @Test
    fun `a field that is not a number is not guessed at`() {
        assertNull(StatusParser.parse("S,3,xx,158,100,45230,3,1800000,1"))
        assertNull(StatusParser.parse("S,3,142,158,100,45230,3,,1"))
    }

    /**
     * The firmware sends the enum ordinal. An ordinal this list does not know
     * means the two sides have drifted apart, and inventing a state for it
     * would hide exactly that.
     */
    @Test
    fun `an unknown state ordinal is refused`() {
        assertNull(StatusParser.parse("S,99,0,0,0,0,0,0,0"))
        assertNull(StatusParser.parse("S,-1,0,0,0,0,0,0,0"))
    }

    @Test
    fun `every ordinal the firmware can send maps to a state`() {
        SocketState.entries.forEachIndexed { ordinal, expected ->
            val status = StatusParser.parse("S,$ordinal,0,0,0,0,0,0,0")
            assertEquals("ordinal $ordinal", expected, status?.state)
        }
    }

    private companion object {
        /** Copied from a real `Serial.print` run, not invented. */
        const val LINE = "S,3,142,158,100,45230,3,1800000,1"
    }
}
