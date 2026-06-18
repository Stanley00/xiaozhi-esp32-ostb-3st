# OSTB-3ST-WIFI

ESP32-S3 touchscreen AI companion (`ostb-3st`).

## Hardware

| Subsystem | Part | Interface |
|---|---|---|
| MCU | ESP32-S3 (8 MB PSRAM, 16 MB flash) | — |
| Display | NV3023 color SPI TFT, 240 × 296 | SPI3 (write-only) + PWM backlight |
| Touch | CST816 capacitive (@0x15), polled | shared I2C |
| Output codec | ES8311 (@0x18) → speaker | I2C + I2S |
| IMU | LIS3DH (@0x19) → 3 DOF accel | I2C |
| Input ADC | ES7210 (@0x40) ← mic | I2C + I2S |
| Power | 1S Li-ion, TP4056-class charger, ADC voltage estimate | GPIO/ADC |

Pin map: see `config.h` (derived from `esp32/HARDWARE_AND_WIRING.md`, firmware v2.5.7).

### IMU note

```cpp

// LIS3DH Register Definitions for Data Output
#define LIS3DH_REG_OUT_X_L    0x28
#define LIS3DH_REG_OUT_X_H    0x29
#define LIS3DH_REG_OUT_Y_L    0x2A
#define LIS3DH_REG_OUT_Y_H    0x2B
#define LIS3DH_REG_OUT_Z_L    0x2C
#define LIS3DH_REG_OUT_Z_H    0x2D

// Assuming 'X' is the instance name of your struct/class
void ReadAccelerometerData(X& x) {
	// 0. Wake up the chip, should called only once
	x.WriteReg(0x20, 0x57);
    // 1. Read the Low and High bytes for each axis
    
    uint8_t x_l = x.ReadReg(LIS3DH_REG_OUT_X_L);
    uint8_t x_h = x.ReadReg(LIS3DH_REG_OUT_X_H);
    
    uint8_t y_l = x.ReadReg(LIS3DH_REG_OUT_Y_L);
    uint8_t y_h = x.ReadReg(LIS3DH_REG_OUT_Y_H);
    
    uint8_t z_l = x.ReadReg(LIS3DH_REG_OUT_Z_L);
    uint8_t z_h = x.ReadReg(LIS3DH_REG_OUT_Z_H);

    // 2. Combine the 8-bit registers into signed 16-bit integers
    int16_t raw_x = (int16_t)((x_h << 8) | x_l);
    int16_t raw_y = (int16_t)((y_h << 8) | y_l);
    int16_t raw_z = (int16_t)((z_h << 8) | z_l);

    // 3. Print the raw values
    ESP_LOGI(TAG, "IMU Raw Data -> X: %d | Y: %d | Z: %d", raw_x, raw_y, raw_z);
}
```

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
