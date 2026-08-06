package gh.group38.smartsocket.data

import android.content.Context
import android.content.Intent
import androidx.core.content.FileProvider
import java.io.File

/**
 * Charge history out of the app and into anything else.
 *
 * A share intent rather than a Downloads write: the phone is the only place
 * this data exists - the socket has no clock and 1 KB of RAM - and "email it to
 * myself" or "put it in Drive" is what someone actually wants from a record of
 * how their charger behaves. The share sheet does all of those and needs no
 * storage permission on any Android version.
 *
 * The file goes to the cache directory on purpose. It is a copy made for one
 * share, the system may reclaim it whenever it likes, and the real history
 * stays in [HistoryStore].
 */
object HistoryExport {

    private const val CACHE_DIR = "export"

    /** Null when there is nothing to share, or the copy could not be written. */
    fun shareIntent(context: Context, sessions: List<ChargeSession>): Intent? {
        if (sessions.isEmpty()) return null

        return runCatching {
            val dir = File(context.cacheDir, CACHE_DIR).apply { mkdirs() }
            val file = File(dir, HistoryCsv.FILE_NAME)
            file.writeText(HistoryCsv.render(sessions))

            val uri = FileProvider.getUriForFile(context, authority(context), file)

            Intent(Intent.ACTION_SEND).apply {
                type = HistoryCsv.MIME_TYPE
                putExtra(Intent.EXTRA_STREAM, uri)
                putExtra(Intent.EXTRA_SUBJECT, "Smart Socket charge history")
                putExtra(
                    Intent.EXTRA_TEXT,
                    "${sessions.size} charges recorded by the Smart Socket.",
                )
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
        }.getOrNull()
    }

    /** Must match `android:authorities` in the manifest. */
    private fun authority(context: Context): String = "${context.packageName}.files"
}
