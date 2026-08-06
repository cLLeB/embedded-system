package gh.group38.smartsocket.data

import java.util.UUID

/** What a discovered GATT characteristic can do. */
data class BleCharacteristic(
    val uuid: UUID,
    val canNotify: Boolean,
    val canWrite: Boolean,
)

data class BleService(
    val uuid: UUID,
    val characteristics: List<BleCharacteristic>,
)

/**
 * Which characteristic carries bytes each way on a BLE serial module.
 * [notify] is device to host, [write] is host to device. On the commonest
 * modules they are the same characteristic.
 */
data class BleSerialProfile(
    val service: UUID,
    val notify: UUID,
    val write: UUID,
    val name: String,
)

/**
 * Works out how to speak serial over a BLE module that has no standard for it.
 *
 * BLE has no equivalent of Classic Bluetooth's SPP. Every vendor bolted a serial
 * channel onto GATT with private UUIDs, so a module advertising itself as
 * "HC-05" may be any of half a dozen designs - or something nobody has
 * catalogued. Rather than hardcode one vendor and fail on the rest, this tries
 * the known ones by name and then falls back to what the device says it can do.
 *
 * THE MODULE IN THIS PROJECT IS ONE OF THESE, not the HC-05 it is sold as. It
 * matched `HM-10 / FFE0` on the desktop app, which is the CC2541 design and by a
 * wide margin the likeliest thing in an HC-05-shaped package.
 *
 * A direct port of the desktop app's `BleSerialProfiles`, kept identical on
 * purpose: a module that works on one client has to work on the other, and the
 * matching rules are the thing most likely to drift apart silently.
 */
object BleSerialProfiles {

    private fun short(id: Int): UUID =
        UUID.fromString(String.format("0000%04X-0000-1000-8000-00805F9B34FB", id))

    /**
     * The catalogued designs, most common first. HM-10 and its clones - AT-09,
     * JDY-08, CC2541 boards, and most things sold as a "BLE HC-05" - are tried
     * first because they are what you almost always have.
     */
    val known: List<BleSerialProfile> = listOf(
        // HM-10 / AT-09 / JDY / CC2541. One characteristic, both directions.
        BleSerialProfile(short(0xFFE0), short(0xFFE1), short(0xFFE1), "HM-10 / FFE0"),

        // Nordic UART Service. Note the crossover: 0003 is the device's TX, so
        // it is the host's notify, and 0002 is what the host writes.
        BleSerialProfile(
            UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"),
            UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"),
            UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"),
            "Nordic UART",
        ),

        // Microchip RN4870 / RN4871 transparent UART.
        BleSerialProfile(
            UUID.fromString("49535343-FE7D-4AE5-8FA9-9FAFD205E455"),
            UUID.fromString("49535343-1E4D-4BD9-BA61-23C647249616"),
            UUID.fromString("49535343-8841-43F4-A8D4-ECBE34729BB3"),
            "Microchip transparent UART",
        ),

        // Feasycom and several other FFF0-range modules, split characteristics.
        BleSerialProfile(short(0xFFF0), short(0xFFF1), short(0xFFF2), "FFF0 split"),
    )

    /**
     * Services every BLE device carries. None of them is a serial channel, and
     * the fallback must not pick a characteristic out of one - writing to Device
     * Name because it happened to be writable would rename the module on every
     * command sent.
     */
    private val standard: Set<UUID> = setOf(
        short(0x1800), // Generic Access
        short(0x1801), // Generic Attribute
        short(0x180A), // Device Information
        short(0x180F), // Battery
        short(0x1805), // Current Time
        short(0x181C), // User Data
    )

    /**
     * Null when nothing on the device can carry a byte stream. That is a real
     * answer, not a failure to try: some BLE devices genuinely have no serial
     * channel, and guessing one produces a link that connects and never works.
     */
    fun resolve(services: List<BleService>): BleSerialProfile? {
        // 1. A catalogued design, matched exactly.
        for (candidate in known) {
            val service = services.firstOrNull { it.uuid == candidate.service } ?: continue

            val notify = service.characteristics.firstOrNull {
                it.uuid == candidate.notify && it.canNotify
            }
            val write = service.characteristics.firstOrNull {
                it.uuid == candidate.write && it.canWrite
            }

            if (notify != null && write != null) return candidate
        }

        // 2. Anything that behaves like one. A serial channel needs a way for the
        //    device to push bytes up and for the host to push bytes down; a
        //    vendor service offering both is almost certainly the UART.
        for (service in services) {
            if (service.uuid in standard) continue

            val notify = service.characteristics.firstOrNull { it.canNotify } ?: continue
            val write = service.characteristics.firstOrNull { it.canWrite } ?: continue

            return BleSerialProfile(service.uuid, notify.uuid, write.uuid, "Discovered")
        }

        return null
    }
}
