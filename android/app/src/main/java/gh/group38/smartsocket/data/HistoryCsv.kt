package gh.group38.smartsocket.data

import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

/**
 * Charge history as a spreadsheet, oldest first.
 *
 * Oldest first even though [HistoryStore] keeps newest first: on screen the
 * newest charge belongs at the top, but a column of timestamps that a
 * spreadsheet is going to plot has to run forwards.
 *
 * Two time columns on purpose. The ISO one is for a human reading the file, the
 * millisecond one is for a spreadsheet that will not parse it - and neither can
 * be reconstructed from the other without knowing which zone the phone was in.
 */
object HistoryCsv {

    const val HEADER = "ended_at,ended_at_ms,peak_ma,cut_at_ma,duration_ms,taper_percent"

    const val FILE_NAME = "smart-socket-history.csv"

    const val MIME_TYPE = "text/csv"

    fun render(
        sessions: List<ChargeSession>,
        zone: ZoneId = ZoneId.systemDefault(),
    ): String {
        val stamp = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss").withZone(zone)

        return buildString {
            append(HEADER)
            sessions.sortedBy { it.endedAtMillis }.forEach { session ->
                append('\n')
                append(stamp.format(Instant.ofEpochMilli(session.endedAtMillis)))
                append(',')
                append(session.endedAtMillis)
                append(',')
                append(session.peakMa)
                append(',')
                append(session.cutAtMa)
                append(',')
                append(session.durationMs)
                append(',')
                append((session.taperFraction * 100).toInt())
            }
        }
    }
}
