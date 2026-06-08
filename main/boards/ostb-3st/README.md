# OSTB-3ST-WIFI

ESP32-S3 touchscreen AI companion (`ostb-3st`).

## Hardware

| Subsystem | Part | Interface |
|---|---|---|
| MCU | ESP32-S3 (8 MB PSRAM, 16 MB flash) | — |
| Display | NV3023 color SPI TFT, 240 × 296 | SPI3 (write-only) + PWM backlight |
| Touch | CST816 capacitive (@0x15), polled | shared I2C |
| Output codec | ES8311 (@0x18) → speaker | I2C + I2S |
| Input ADC | ES7210 (@0x40) ← mic | I2C + I2S |
| Power | 1S Li-ion, TP4056-class charger, ADC voltage estimate | GPIO/ADC |

Pin map: see `config.h` (derived from `esp32/HARDWARE_AND_WIRING.md`, firmware v2.5.7).

## Build

```bash
idf.py set-target esp32s3
idf.py menuconfig   # Xiaozhi Assistant → Board Type → "OSTB-3ST-WIFI 触屏 AI 伴侣"
idf.py build flash monitor
```

## To verify on first bring-up

These were recovered from firmware / estimated and should be confirmed on the
actual device:

- **Display orientation/offsets** (`DISPLAY_SWAP_XY`, `DISPLAY_MIRROR_X/Y`,
  `DISPLAY_OFFSET_X/Y`) — adjust if the image is flipped, mirrored, or shifted.
- **Battery voltage curve** in `power_manager.h` — the ADC breakpoints are
  estimates; calibrate against measured cell voltage at known charge levels.
- **Volume button mapping** (GPIO39 / GPIO40) — swap `VOLUME_UP_BUTTON_GPIO` /
  `VOLUME_DOWN_BUTTON_GPIO` if up/down feel reversed.
