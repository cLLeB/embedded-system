package gh.group38.smartsocket

import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.ServiceCompat
import gh.group38.smartsocket.data.SocketRepository
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch

/**
 * Keeps the process alive while the socket is connected.
 *
 * It does not own the link - [SocketRepository] does, on the Application. This
 * exists purely so Android does not kill the process when the activity goes
 * away, and so there is a notification to look at while it is up.
 *
 * Started when a connection succeeds and stopped when it ends, rather than
 * running always: a foreground notification the user cannot dismiss is a real
 * cost, and it should only be paid while it is buying something.
 */
class SocketService : Service() {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private var watcher: Job? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val repo = (application as SmartSocketApp).repository

        startForegroundCompat(
            Notifications.ongoing(this, repo.status.value, repo.deviceName)
        )

        watcher?.cancel()
        watcher = scope.launch {
            repo.status.collect { status ->
                // Same id, so this replaces rather than stacks.
                val manager = getSystemService(android.app.NotificationManager::class.java)
                manager?.notify(
                    Notifications.ONGOING_ID,
                    Notifications.ongoing(this@SocketService, status, repo.deviceName),
                )
            }
        }

        // The link is not ours to rebuild, so being restarted without an intent
        // would leave this service showing a notification for nothing.
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        watcher?.cancel()
        scope.cancel()
        super.onDestroy()
    }

    private fun startForegroundCompat(notification: android.app.Notification) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // Android 10+ requires the type to be declared, and 14+ rejects the
            // call outright without one. connectedDevice is the honest label: the
            // service exists to hold a Bluetooth link open.
            ServiceCompat.startForeground(
                this,
                Notifications.ONGOING_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE,
            )
        } else {
            startForeground(Notifications.ONGOING_ID, notification)
        }
    }

    companion object {
        fun start(context: Context) {
            val intent = Intent(context, SocketService::class.java)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }

        fun stop(context: Context) {
            context.stopService(Intent(context, SocketService::class.java))
        }
    }
}
