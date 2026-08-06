package gh.group38.smartsocket.data

import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

/**
 * Matching real modules, since the alternative is carrying boards around.
 *
 * These mirror the desktop app's BleSerialProfileTests case for case. The two
 * clients have to agree about which characteristic carries the socket's data,
 * or a module that works on a laptop is dead on a phone for reasons nobody can
 * see.
 */
class BleSerialProfileTest {

    private fun short(id: Int): UUID =
        UUID.fromString(String.format("0000%04X-0000-1000-8000-00805F9B34FB", id))

    private fun generic() = BleService(
        short(0x1800),
        listOf(BleCharacteristic(short(0x2A00), canNotify = false, canWrite = true)),
    )

    private fun deviceInfo() = BleService(
        short(0x180A),
        listOf(BleCharacteristic(short(0x2A29), canNotify = false, canWrite = false)),
    )

    /** The module actually in this project. */
    @Test
    fun `an HM-10 is matched on its single dual-purpose characteristic`() {
        val profile = BleSerialProfiles.resolve(
            listOf(
                generic(),
                BleService(
                    short(0xFFE0),
                    listOf(BleCharacteristic(short(0xFFE1), canNotify = true, canWrite = true)),
                ),
            )
        )

        assertNotNull(profile)
        assertEquals("HM-10 / FFE0", profile!!.name)
        assertEquals(short(0xFFE1), profile.notify)
        assertEquals(short(0xFFE1), profile.write)
    }

    /**
     * The crossover is the easy thing to get backwards: 0003 is the *device's*
     * TX, so it is what the host subscribes to. Writing to it instead would
     * connect cleanly and never deliver a command.
     */
    @Test
    fun `Nordic UART directions are not swapped`() {
        val nus = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val tx = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
        val rx = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")

        val profile = BleSerialProfiles.resolve(
            listOf(
                BleService(
                    nus,
                    listOf(
                        BleCharacteristic(tx, canNotify = true, canWrite = false),
                        BleCharacteristic(rx, canNotify = false, canWrite = true),
                    ),
                )
            )
        )

        assertNotNull(profile)
        assertEquals(tx, profile!!.notify)
        assertEquals(rx, profile.write)
    }

    @Test
    fun `an uncatalogued module is resolved from what it can do`() {
        val vendor = UUID.fromString("12345678-1234-5678-1234-56789ABCDEF0")
        val up = UUID.fromString("12345678-1234-5678-1234-56789ABCDEF1")
        val down = UUID.fromString("12345678-1234-5678-1234-56789ABCDEF2")

        val profile = BleSerialProfiles.resolve(
            listOf(
                generic(),
                deviceInfo(),
                BleService(
                    vendor,
                    listOf(
                        BleCharacteristic(up, canNotify = true, canWrite = false),
                        BleCharacteristic(down, canNotify = false, canWrite = true),
                    ),
                ),
            )
        )

        assertNotNull(profile)
        assertEquals("Discovered", profile!!.name)
        assertEquals(up, profile.notify)
        assertEquals(down, profile.write)
    }

    /**
     * Device Name in Generic Access is writable on plenty of modules. Picking it
     * as a serial channel would rename the device on every command sent.
     */
    @Test
    fun `standard services are never used as a serial channel`() {
        val profile = BleSerialProfiles.resolve(
            listOf(
                BleService(
                    short(0x1800),
                    listOf(BleCharacteristic(short(0x2A00), canNotify = true, canWrite = true)),
                ),
                BleService(
                    short(0x180F),
                    listOf(BleCharacteristic(short(0x2A19), canNotify = true, canWrite = true)),
                ),
            )
        )

        assertNull(profile)
    }

    @Test
    fun `a device with no serial channel reports none rather than guessing`() {
        assertNull(BleSerialProfiles.resolve(listOf(generic(), deviceInfo())))
        assertNull(BleSerialProfiles.resolve(emptyList()))
    }

    /**
     * Half a link is worse than none: a connection that shows live data and
     * silently drops every Cut command is the worst failure this app has.
     */
    @Test
    fun `notify without write is not a serial channel`() {
        val vendor = UUID.fromString("12345678-1234-5678-1234-56789ABCDEF0")

        assertNull(
            BleSerialProfiles.resolve(
                listOf(
                    BleService(
                        vendor,
                        listOf(
                            BleCharacteristic(UUID.randomUUID(), canNotify = true, canWrite = false)
                        ),
                    )
                )
            )
        )
    }

    @Test
    fun `write without notify is not a serial channel`() {
        val vendor = UUID.fromString("12345678-1234-5678-1234-56789ABCDEF0")

        assertNull(
            BleSerialProfiles.resolve(
                listOf(
                    BleService(
                        vendor,
                        listOf(
                            BleCharacteristic(UUID.randomUUID(), canNotify = false, canWrite = true)
                        ),
                    )
                )
            )
        )
    }

    @Test
    fun `a known profile wins over the fallback`() {
        val vendor = UUID.fromString("12345678-1234-5678-1234-56789ABCDEF0")

        val profile = BleSerialProfiles.resolve(
            listOf(
                BleService(
                    vendor,
                    listOf(BleCharacteristic(UUID.randomUUID(), canNotify = true, canWrite = true)),
                ),
                BleService(
                    short(0xFFE0),
                    listOf(BleCharacteristic(short(0xFFE1), canNotify = true, canWrite = true)),
                ),
            )
        )

        assertEquals("HM-10 / FFE0", profile?.name)
    }

    /**
     * FFE0 present but its characteristic missing the properties the table
     * expects: the exact match fails, and the fallback should still rescue it
     * rather than the whole resolution collapsing.
     */
    @Test
    fun `a partial known service falls through to discovery`() {
        val other = UUID.fromString("0000AB00-0000-1000-8000-00805F9B34FB")

        val profile = BleSerialProfiles.resolve(
            listOf(
                BleService(
                    short(0xFFE0),
                    listOf(BleCharacteristic(short(0xFFE1), canNotify = true, canWrite = false)),
                ),
                BleService(
                    other,
                    listOf(BleCharacteristic(UUID.randomUUID(), canNotify = true, canWrite = true)),
                ),
            )
        )

        assertNotNull(profile)
        assertEquals("Discovered", profile!!.name)
        assertEquals(other, profile.service)
    }
}
