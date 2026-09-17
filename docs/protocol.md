# UART protocol

Verified from the original module's traffic and subsequent ESP-only control tests on an Airmega 250S. UART is **115200, 8N1**, with **5 V signals on the Coway side**.

## Frames

The motherboard sends `AT*ICT*AWS_SEND=<STX><payload><checksum><ETX>\r`. The bridge acknowledges with `*ICT*AWS_SEND:OK\r\n`, followed by `*ICT*AWS_IND:SEND OK\r\n`.

Control and query frames use:

```text
*ICT*AWS_RECV:<STX><length>A101<operation><correlation>-22{attributes}<checksum><ETX>\r
```

- STX is `0x02`. ETX is `0x03`.
- `length` is four uppercase hexadecimal characters: the number of ASCII bytes from the length field through `}`, minus one.
- Operation `1` writes attributes. Operation `3` queries with `{}`. Replies use `0` and `2` respectively.
- Correlation is 13 decimal epoch-millisecond digits for these commands. The operation plus correlation occupies 14 characters. Unsolicited packets use other identifiers.
- Checksum is two uppercase hexadecimal characters: `(2 + sum(payload ASCII bytes)) & 0xff`, excluding the checksum and ETX.

Captured AQI-off command, with control characters escaped:

```text
*ICT*AWS_RECV:\x020020A10111789648220120-22{0007:1}25\x03\r
```

The component generates a fresh correlation value for each command.

## A101 settings

| Attribute | Meaning | Verified command values |
| --- | --- | --- |
| `0001` | Power | 0 off, 1 on |
| `0002` | Mode | 1 Auto, 2 Sleep, 5 Rapid |
| `0003` | Manual speed | 1, 2, 3 |
| `0007` | Lighting | 0 all on, 1 AQI off, 2 all off |
| `0008` | Off timer, minutes | 0, 60, 120, 240, 480 |
| `000A` | Sensitivity | 1 Sensitive, 2 Moderate, 3 Insensitive |
| `0024` | Button lock | 0 unlocked, 1 locked |

Status mode 0 means manual and 4 means off. Speed status can be 0 in Sleep, 5 in Rapid, and 99 when off. Selecting a manual speed clears the preset.

## A102 sensors

| Attribute | Meaning |
| --- | --- |
| `0001` | PM2.5 |
| `0002` | PM10 |
| `0007` | Raw ambient light |
| `0011` | Pre-filter usage percentage |
| `0012` | MAX2 usage percentage |

Filter life remaining is `100 − usage`. The YAML applies the model-specific ambient-light correction.

## Startup and scope

The bridge emulates the original module's `DEVICEREADY`, setup acknowledgements, and paced connection announcements. `AWS_IND` messages are local compatibility replies to the motherboard.

The fixed `IPALLOCATED` UART announcement contains documentation addresses (`192.0.2.x`). Wi-Fi configuration comes from `secrets.yaml`. The sanitised announcement passed the firmware build and still needs testing on the appliance.

Home Assistant supplies local time. Commands wait for valid time, and a query refreshes operating state after time sync and every minute. The fan waits for power-on feedback before sending a requested speed or mode.

The app's pre-filter cleaning interval produced no UART command during capture. Filter resets, scheduling commands, and other models are outside the verified protocol.
