# ESP32-S3 VGA

VGA video output from an ESP32-S3 using the [SpikePavel ESP32-S3-VGA](https://github.com/SpikePavel/ESP32-S3-VGA) library. Displays a 640x480 @ 60Hz 8-bit color test pattern with text overlay.

## Hardware

- **Board:** ESP32-S3-Box (ESP32-S3, 8MB RAM, 16MB Flash)
- **Display:** VGA monitor via direct GPIO connection

### Pin Wiring

| Signal | GPIO |
|--------|------|
| Red    | 45   |
| Green  | 42   |
| Blue   | 41   |
| HSync  | 47   |
| VSync  | 48   |

This uses a 3-pin color setup (1 bit per channel, 8 colors). For full 8-bit color (256 colors), additional GPIO pins are needed for the R/G/B channels.

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
pio run
```

## Uploading

```bash
pio run --target upload
```

## Serial Monitor

```bash
pio device monitor
```

## Dependencies

- [ESP32-S3-VGA](https://github.com/SpikePavel/ESP32-S3-VGA) by SpikePavel (installed automatically by PlatformIO)
