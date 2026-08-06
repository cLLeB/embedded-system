package gh.group38.smartsocket

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.core.app.NotificationCompat
import gh.group38.smartsocket.data.SocketState
import gh.group38.smartsocket.data.SocketStatus
import java.util.Locale

/**
 * Two channels, because they are two different jobs.
 *
 * The ongoing one is silent and unrankable - it exists because Android requires
 * a visible notification for a foreground service, not because anyone wants to
 * read it. The alert channel is the one that matters: it is the whole reason to
 * keep a service running at all.
 */
object Notifications {

    const val ONGOING_CHANNEL = "socket_link"
    const val ALERT_CHANNEL = "socket_alerts"

    const val ONGOING_ID = 1001
    private const val ALERT_ID = 1002

    // Its own id, so losing the link never overwrites the "your laptop is
    // charged" that was the point of being connected in the first place.
    private const val LINK_LOST_ID = 1003

    fun ensureChannels(context: Context) {
        val manager = context.getSystemService(NotificationManager::class.java) ?: return

        manager.createNotificationChannel(
            NotificationChannel(
                ONGOING_CHANNEL,
                "Socket connection",
                NotificationManager.IMPORTANCE_LOW,
            ).apply {
                description = "Keeps the link to the socket alive while the app is closed"
                setShowBadge(false)
            }
        )

        manager.createNotificationChannel(
            NotificationChannel(
                ALERT_CHANNEL,
                "Charging finished",
                NotificationManager.IMPORTANCE_HIGH,
            ).apply {
                description = "Tells you when the socket has cut power"
            }
        )
    }

    fun ongoing(
        context: Context,
        status: SocketStatus,
        deviceName: String,
        reconnecting: Boolean = false,
    ): Notification {
        val text = when {
            // First, because during a reconnect every other line here would be
            // reporting a measurement that is minutes old as if it were now.
            reconnecting -> "Reconnecting · last seen ${status.state.label.lowercase(Locale.UK)}"

            status.state == SocketState.CHARGING ->
                String.format(Locale.UK, "Charging · %.2f A", status.amps)

            status.state == SocketState.CUTOFF -> "Power cut · charged"
            status.state.isAlarm -> status.state.label
            else -> status.state.label
        }

        return NotificationCompat.Builder(context, ONGOING_CHANNEL)
            .setSmallIcon(R.drawable.ic_stat_socket)
            .setContentTitle(deviceName)
            .setContentText(text)
            .setContentIntent(openApp(context))
            .setOngoing(true)
            .setSilent(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }

    fun alertCutoff(context: Context, status: SocketStatus) {
        val manager = context.getSystemService(NotificationManager::class.java) ?: return

        val body = if (status.peakMa > 0) {
            String.format(
                Locale.UK,
                "Charging finished and the wall is off. Peak was %.2f A.",
                status.peakAmps,
            )
        } else {
            "Charging finished and the wall is off."
        }

        manager.notify(
            ALERT_ID,
            NotificationCompat.Builder(context, ALERT_CHANNEL)
                .setSmallIcon(R.drawable.ic_stat_socket)
                .setContentTitle("Power cut")
                .setContentText(body)
                .setStyle(NotificationCompat.BigTextStyle().bigText(body))
                .setContentIntent(openApp(context))
                .setAutoCancel(true)
                .setPriority(NotificationCompat.PRIORITY_HIGH)
                .build(),
        )
    }

    fun alertStuckRelay(context: Context) {
        val manager = context.getSystemService(NotificationManager::class.java) ?: return

        val body = "The socket commanded power off and current is still flowing. " +
            "Unplug it at the wall."

        manager.notify(
            ALERT_ID,
            NotificationCompat.Builder(context, ALERT_CHANNEL)
                .setSmallIcon(R.drawable.ic_stat_socket)
                .setContentTitle("Relay stuck")
                .setContentText(body)
                .setStyle(NotificationCompat.BigTextStyle().bigText(body))
                .setContentIntent(openApp(context))
                .setAutoCancel(false)
                .setPriority(NotificationCompat.PRIORITY_MAX)
                .build(),
        )
    }

    /**
     * The socket is no longer being watched.
     *
     * Worth interrupting for. Someone who set a cutoff and walked away has no
     * other way to learn that it is not going to happen, and a charger left on
     * overnight is the exact thing this project exists to prevent.
     */
    fun alertLinkLost(context: Context, deviceName: String) {
        val manager = context.getSystemService(NotificationManager::class.java) ?: return

        val body = "Lost the Bluetooth link to $deviceName and could not get it back. " +
            "The socket is still running on its own, but this phone is no longer " +
            "watching it."

        manager.notify(
            LINK_LOST_ID,
            NotificationCompat.Builder(context, ALERT_CHANNEL)
                .setSmallIcon(R.drawable.ic_stat_socket)
                .setContentTitle("Disconnected")
                .setContentText(body)
                .setStyle(NotificationCompat.BigTextStyle().bigText(body))
                .setContentIntent(openApp(context))
                .setAutoCancel(true)
                .setPriority(NotificationCompat.PRIORITY_DEFAULT)
                .build(),
        )
    }

    private fun openApp(context: Context): PendingIntent {
        val intent = Intent(context, MainActivity::class.java)
            .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP)
        return PendingIntent.getActivity(
            context,
            0,
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
    }
}
