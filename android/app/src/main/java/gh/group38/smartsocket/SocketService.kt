package gh.group38.smartsocket

import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.ServiceCompat
import gh.group38.smartsocket.data.LinkState
import gh.group38.smartsocket.data.SocketRepository
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.combine
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
            // The link as well as the status: while the app is closed this
            // notification is the only thing the user can see, so it is the only
            // place a dropped link can be reported.
            combine(repo.status, repo.linkState) { status, link -> status to link }
                .collect { (status, link) ->
                    // Same id, so this replaces rather than stacks.
                    val manager = getSystemService(android.app.NotificationManager::class.java)
                    manager?.notify(
                        Notifications.ONGOING_ID,
                        Notifications.ongoing(
                            context = this@SocketService,
                            status = status,
                            deviceName = repo.deviceName,
                            reconnecting = link is LinkState.Reconnecting,
                        ),
                    )
                }
        }

        // STICKY, so Android brings this back if it kills the process under
        // memory pressure overnight.
        //
        // It was NOT_STICKY on the reasoning that a restart with no intent would
        // leave a notification over a link nobody was rebuilding. That is no
        // longer true: SmartSocketApp reconnects to the remembered socket the
        // moment the process starts, and the socket is only remembered until the
        // user presses Disconnect. So a restart now genuinely does resume the
        // job rather than showing a notification for nothing.
        return START_STICKY
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
        /**
         * Safe to call from the background.
         *
         * Android 12+ throws ForegroundServiceStartNotAllowedException if the app
         * is not in the foreground or covered by an exemption. The reconnect that
         * BootReceiver kicks off completes seconds after the broadcast returns, by
         * which time the boot exemption may have lapsed - and losing the
         * notification is not worth crashing the process the link lives in. The
         * link keeps working either way; it is just killable sooner.
         */
        fun start(context: Context) {
            val intent = Intent(context, SocketService::class.java)
            runCatching {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    context.startForegroundService(intent)
                } else {
                    context.startService(intent)
                }
            }
        }

        fun stop(context: Context) {
            context.stopService(Intent(context, SocketService::class.java))
        }
    }
}
