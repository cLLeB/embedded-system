package gh.group38.smartsocket

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

/**
 * Picks the link back up after a restart.
 *
 * A phone left charging overnight that reboots - an update, a flat battery, a
 * crash - would otherwise come back with nothing watching it, and the user finds
 * out in the morning when the battery has sat at 100% for six hours. That is
 * precisely the case the whole app exists to prevent, so it is worth handling.
 *
 * BOOT_COMPLETED is one of the few exemptions to Android 12's ban on starting a
 * foreground service from the background. MY_PACKAGE_REPLACED is here for the
 * same reason and is the case that actually gets hit during development.
 *
 * Reconnecting is deliberately silent about failure. Nobody is looking at the
 * phone when this runs, and a socket that is switched off at the wall is the
 * normal reason it fails.
 */
class BootReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent?) {
        when (intent?.action) {
            Intent.ACTION_BOOT_COMPLETED, Intent.ACTION_MY_PACKAGE_REPLACED -> Unit
            else -> return
        }

        val app = context.applicationContext as? SmartSocketApp ?: return
        val repo = app.repository

        // Cleared by disconnect(), so this is null whenever the user hung up on
        // purpose. Not remembering that would make the app reconnect to a socket
        // someone deliberately walked away from.
        val device = repo.lastDevice() ?: return

        // Bluetooth off at boot is common and is not a failure. The user will
        // reconnect from the app when they turn it on.
        if (!repo.bluetoothOn()) return

        repo.connect(device)
    }
}
