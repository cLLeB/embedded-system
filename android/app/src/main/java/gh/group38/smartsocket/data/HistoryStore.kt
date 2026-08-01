package gh.group38.smartsocket.data

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.io.File

/**
 * Every cutoff, kept newest-first.
 *
 * A flat file of comma-separated lines rather than Room. Room brings the KSP
 * plugin, a schema, a DAO and migrations, and this table has four integer
 * columns and will never hold more than [LIMIT] rows - the cost is all
 * ceremony and none of it buys anything here. Reads are one `readLines`, writes
 * are one `writeText`, and the format is legible if anyone ever wants to look.
 */
class HistoryStore(context: Context) {

    private val file = File(context.filesDir, "charge_history.csv")

    private val _sessions = MutableStateFlow<List<ChargeSession>>(emptyList())
    val sessions: StateFlow<List<ChargeSession>> = _sessions.asStateFlow()

    init {
        _sessions.value = read()
    }

    fun record(session: ChargeSession) {
        // Newest first, so the UI never has to reverse and the trim drops the
        // oldest by taking a prefix.
        val updated = (listOf(session) + _sessions.value).take(LIMIT)
        _sessions.value = updated
        runCatching { file.writeText(updated.joinToString("\n") { it.encode() }) }
    }

    fun clear() {
        _sessions.value = emptyList()
        runCatching { file.delete() }
    }

    private fun read(): List<ChargeSession> = runCatching {
        if (!file.exists()) return emptyList()
        file.readLines().mapNotNull { ChargeSession.decode(it) }.take(LIMIT)
    }.getOrDefault(emptyList())

    private companion object {
        const val LIMIT = 60
    }
}
