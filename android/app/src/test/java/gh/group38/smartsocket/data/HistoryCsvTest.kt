package gh.group38.smartsocket.data

import java.time.ZoneId
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class HistoryCsvTest {

    private val utc = ZoneId.of("UTC")

    private fun session(
        endedAtMillis: Long,
        peakMa: Int = 150,
        cutAtMa: Int = 60,
        durationMs: Long = 3_600_000,
    ) = ChargeSession(endedAtMillis, peakMa, cutAtMa, durationMs)

    @Test
    fun `an empty history is still a readable file, not a blank one`() {
        assertEquals(HistoryCsv.HEADER, HistoryCsv.render(emptyList(), utc))
    }

    @Test
    fun `every session is one row under one header`() {
        val csv = HistoryCsv.render(
            listOf(session(2_000), session(1_000), session(3_000)),
            utc,
        )
        assertEquals(4, csv.lines().size)
        assertEquals(HistoryCsv.HEADER, csv.lines().first())
    }

    /**
     * The store keeps newest first for the screen. A spreadsheet plotting these
     * needs the opposite, and silently plotting time backwards is the kind of
     * error nobody notices.
     */
    @Test
    fun `rows run oldest first whatever order they arrive in`() {
        val csv = HistoryCsv.render(
            listOf(session(3_000), session(1_000), session(2_000)),
            utc,
        )
        val stamps = csv.lines().drop(1).map { it.split(',')[1].toLong() }
        assertEquals(listOf(1_000L, 2_000L, 3_000L), stamps)
    }

    @Test
    fun `the human column is a real date in the phone's zone`() {
        val csv = HistoryCsv.render(listOf(session(1_700_000_000_000)), utc)
        assertEquals("2023-11-14 22:13:20", csv.lines()[1].split(',')[0])
    }

    @Test
    fun `columns match the header, in order`() {
        val csv = HistoryCsv.render(
            listOf(session(1_000, peakMa = 160, cutAtMa = 40, durationMs = 90_000)),
            utc,
        )
        val row = csv.lines()[1].split(',')

        assertEquals(HistoryCsv.HEADER.split(',').size, row.size)
        assertEquals("1000", row[1])
        assertEquals("160", row[2])
        assertEquals("40", row[3])
        assertEquals("90000", row[4])
        assertEquals("25", row[5])
    }

    @Test
    fun `a session with no peak does not divide by zero`() {
        val csv = HistoryCsv.render(listOf(session(1_000, peakMa = 0, cutAtMa = 0)), utc)
        assertEquals("0", csv.lines()[1].split(',')[5])
    }

    /**
     * Nothing written here is user-supplied text, so there is nothing to quote -
     * but if a field ever gains a comma the column count silently shifts and
     * every row after it is misread.
     */
    @Test
    fun `no field smuggles in a separator`() {
        val csv = HistoryCsv.render(
            listOf(session(1_000), session(2_000)),
            utc,
        )
        val columns = HistoryCsv.HEADER.split(',').size
        csv.lines().forEach { line ->
            assertTrue("`$line` has the wrong column count", line.split(',').size == columns)
        }
    }
}
