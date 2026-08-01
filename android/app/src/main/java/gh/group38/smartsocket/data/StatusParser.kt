package gh.group38.smartsocket.data

/**
 * Decodes one line of the socket's wire format.
 *
 *     S,<state>,<mA>,<peakMa>,<thresholdMa>,<elapsedMs>,<cutoffs>,<savedMs>,<relay>
 *
 * Returns null for anything it does not fully understand rather than guessing.
 * A Bluetooth link drops mid-line whenever the phone moves out of range, so
 * partial and corrupt lines are the normal case, not an exceptional one - and a
 * half-parsed line would show the user a number that never existed.
 */
object StatusParser {

    private const val FIELD_COUNT = 9

    fun parse(line: String): SocketStatus? {
        val trimmed = line.trim()
        if (!trimmed.startsWith("S,")) return null

        val parts = trimmed.split(',')
        if (parts.size != FIELD_COUNT) return null

        val state = SocketState.fromOrdinal(parts[1].toIntOrNull() ?: return null)
            ?: return null

        return SocketStatus(
            state = state,
            currentMa = parts[2].toIntOrNull() ?: return null,
            peakMa = parts[3].toIntOrNull() ?: return null,
            thresholdMa = parts[4].toIntOrNull() ?: return null,
            sessionElapsedMs = parts[5].toLongOrNull() ?: return null,
            cutoffCount = parts[6].toIntOrNull() ?: return null,
            totalSavedMs = parts[7].toLongOrNull() ?: return null,
            relayClosed = parts[8].trim() == "1",
        )
    }
}
