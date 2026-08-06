package gh.group38.smartsocket

import android.app.Application
import gh.group38.smartsocket.data.BatteryWatcher
import gh.group38.smartsocket.data.LinkState
import gh.group38.smartsocket.data.SocketRepository
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

class SmartSocketApp : Application() {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    // Created eagerly: the repository restores charge history from disk, and the
    // history screen should never show an empty list that fills in a moment
    // later.
    val repository: SocketRepository by lazy { SocketRepository(this) }

    val battery: BatteryWatcher by lazy { BatteryWatcher(this, repository, scope) }

    override fun onCreate() {
        super.onCreate()
        Notifications.ensureChannels(this)

        // Starting the watch here, rather than from a ViewModel, is what makes
        // the battery cutoff work with the app closed - which is the only state
        // it was ever needed in.
        battery.start()

        // The foreground service belongs to the link, not to the screen. Started
        // from SocketViewModel it was left running whenever a link failed for
        // good after the activity was gone: a notification with nothing behind
        // it.
        scope.launch {
            repository.linkState.collect { state ->
                when (state) {
                    is LinkState.Connected -> SocketService.start(this@SmartSocketApp)

                    // The link is being rebuilt on its own. Letting go of the
                    // service here would let the process die during exactly the
                    // gap the service exists to cover.
                    is LinkState.Reconnecting -> Unit

                    else -> SocketService.stop(this@SmartSocketApp)
                }
            }
        }
    }
}
