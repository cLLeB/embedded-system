package gh.group38.smartsocket

import android.app.Application
import gh.group38.smartsocket.data.SocketRepository

class SmartSocketApp : Application() {

    // Created eagerly: the repository restores charge history from disk, and the
    // history screen should never show an empty list that fills in a moment
    // later.
    val repository: SocketRepository by lazy { SocketRepository(this) }

    override fun onCreate() {
        super.onCreate()
        Notifications.ensureChannels(this)
    }
}
