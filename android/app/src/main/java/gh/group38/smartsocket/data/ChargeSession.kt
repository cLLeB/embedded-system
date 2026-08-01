package gh.group38.smartsocket.data

/** One completed charge, recorded at the moment power was cut. */
data class ChargeSession(
    val endedAtMillis: Long,
    val peakMa: Int,
    val cutAtMa: Int,
    val durationMs: Long,
) {
    val peakAmps: Float get() = peakMa / 1000f

    /**
     * How far the current fell before the socket cut, as a fraction of peak.
     * The number the firmware's learning is built on, shown back to the user.
     */
    val taperFraction: Float
        get() = if (peakMa <= 0) 0f else (cutAtMa.toFloat() / peakMa).coerceIn(0f, 1f)

    fun encode(): String = "$endedAtMillis,$peakMa,$cutAtMa,$durationMs"

    companion object {
        fun decode(line: String): ChargeSession? {
            val p = line.split(',')
            if (p.size != 4) return null
            return ChargeSession(
                endedAtMillis = p[0].toLongOrNull() ?: return null,
                peakMa = p[1].toIntOrNull() ?: return null,
                cutAtMa = p[2].toIntOrNull() ?: return null,
                durationMs = p[3].toLongOrNull() ?: return null,
            )
        }
    }
}
