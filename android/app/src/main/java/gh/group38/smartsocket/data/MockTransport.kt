package gh.group38.smartsocket.data

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * A socket that exists only in software.
 *
 * Not a stub that returns canned values - it runs the same sequence the real
 * firmware does, at 20x speed: settle, charge, taper, cut, wait, probe, resume.
 * That makes every screen developable and demonstrable with no HC-05, and it
 * makes the states that are slow or awkward to reach on real hardware - a stuck
 * relay, a 15 minute probe wait - reachable in seconds.
 *
 * Timings are compressed on purpose. Nothing here is a claim about the real
 * device's timing; `Config.h` is the authority on that.
 */
class MockTransport(private val scope: CoroutineScope) : SocketTransport {

    private val _linkState = MutableStateFlow<LinkState>(LinkState.Idle)
    override val linkState: Flow<LinkState> = _linkState.asStateFlow()

    private val _status = MutableStateFlow(SocketStatus.UNKNOWN)
    override val status: Flow<SocketStatus> = _status.asStateFlow()

    private var loop: Job? = null

    override suspend fun connect(device: SocketDevice): Boolean {
        _linkState.value = LinkState.Connecting
        delay(700)
        _linkState.value = LinkState.Connected(device)
        loop?.cancel()
        loop = scope.launch { run() }
        return true
    }

    /**
     * Accepted and ignored. The demo has no display to update and no charge
     * policy to hand over, so `A` and `B` lines have nothing to do here.
     */
    override suspend fun sendLine(line: String) = Unit

    override suspend fun send(command: SocketCommand) {
        val now = _status.value
        when (command) {
            SocketCommand.CUT ->
                if (now.state.isPowerOn) {
                    _status.value = now.copy(state = SocketState.CUTOFF, currentMa = 0, relayClosed = false)
                }

            SocketCommand.REARM ->
                if (now.state == SocketState.CUTOFF || now.state.isAlarm) {
                    _status.value = now.copy(state = SocketState.READY, relayClosed = true)
                }

            SocketCommand.PROBE ->
                if (now.state == SocketState.CUTOFF) {
                    _status.value = now.copy(state = SocketState.PROBING, relayClosed = true)
                }

            SocketCommand.STATUS_NOW -> Unit
        }
    }

    override fun disconnect() {
        loop?.cancel()
        loop = null
        _linkState.value = LinkState.Idle
        _status.value = SocketStatus.UNKNOWN
    }

    private suspend fun run() {
        var cutoffs = 0
        var saved = 0L

        while (scope.isActive) {
            // Idle, waiting for something to be plugged in.
            emit(SocketState.READY, currentMa = 0, relayClosed = true, cutoffs = cutoffs, saved = saved)
            delay(3_000)

            emit(SocketState.SETTLING, currentMa = 155, relayClosed = true, cutoffs = cutoffs, saved = saved)
            delay(1_500)

            // Charge, then taper the way a real charger does: a long plateau, a
            // knee, then a tail.
            var peak = 0
            val curve = buildList {
                repeat(14) { add(150 + (it % 3) * 4) }
                addAll(listOf(142, 134, 126, 118, 110, 102, 95, 88, 82, 78, 74, 71, 69, 68))
            }
            for (ma in curve) {
                if (ma > peak) peak = ma
                emit(
                    SocketState.CHARGING, currentMa = ma, peakMa = peak,
                    thresholdMa = 100, elapsed = 0, relayClosed = true,
                    cutoffs = cutoffs, saved = saved,
                )
                delay(450)
            }

            cutoffs += 1
            emit(SocketState.CUTOFF, currentMa = 0, peakMa = 0, relayClosed = false, cutoffs = cutoffs, saved = saved)

            // Sit cut for a while, accumulating saved time, then probe.
            repeat(10) {
                saved += 1_000
                emit(SocketState.CUTOFF, currentMa = 0, relayClosed = false, cutoffs = cutoffs, saved = saved)
                delay(1_000)
            }

            emit(SocketState.PROBING, currentMa = 0, relayClosed = true, cutoffs = cutoffs, saved = saved)
            delay(2_500)
        }
    }

    private fun emit(
        state: SocketState,
        currentMa: Int,
        peakMa: Int = 0,
        thresholdMa: Int = 0,
        elapsed: Long = 0,
        relayClosed: Boolean,
        cutoffs: Int,
        saved: Long,
    ) {
        _status.value = SocketStatus(
            state = state,
            currentMa = currentMa,
            peakMa = peakMa,
            thresholdMa = thresholdMa,
            sessionElapsedMs = elapsed,
            cutoffCount = cutoffs,
            totalSavedMs = saved,
            relayClosed = relayClosed,
        )
    }

    companion object {
        val DEVICE = SocketDevice(
            name = "Demo socket",
            address = "00:00:00:00:00:00",
            kind = LinkKind.DEMO,
        )
    }
}
