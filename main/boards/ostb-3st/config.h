#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// OSTB-3ST-WIFI — ESP32-S3 touchscreen AI companion.
// Pin map derived from HARDWARE_AND_WIRING.md (firmware v2.5.7).
// ✅ = confirmed on hardware, 🔬 = recovered from firmware (verify on bring-up).

#include <driver/gpio.h>

// BoxAudioCodec drives one full-duplex I2S bus, so input and output sample
// rates must match (asserted in box_audio_codec.cc).
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_INPUT_REFERENCE    true   // ES7210 provides a speaker reference channel (per spec)

// I2S audio data plane (ES8311 output codec + ES7210 mic ADC) 🔬
#define AUDIO_I2S_GPIO_MCLK     GPIO_NUM_5
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_15
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_16
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_6
#define AUDIO_I2S_GPIO_DIN      GPIO_NUM_7
#define AUDIO_CODEC_PA_PIN      GPIO_NUM_4

// Shared I2C control bus ✅ (SDA=12, SCL=11)
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_12
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_11
#define AUDIO_CODEC_ES8311_ADDR ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR ES7210_CODEC_DEFAULT_ADDR

// CST816 capacitive touch on the shared I2C bus, polled (no INT/RST) ✅
#define TOUCH_I2C_ADDR          0x15

// Buttons
#define BOOT_BUTTON_GPIO        GPIO_NUM_0   // center power/BOOT button ✅
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40  // ✅ verified on hardware
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39  // ✅ verified on hardware

// Power / battery (no fuel gauge — ADC voltage estimate)
#define CHARGE_STATUS_GPIO      GPIO_NUM_47  // ✅ 0 = charging, 1 = full/idle
// Battery sense is on GPIO17 = ADC2_CH6 ✅ (handled in power_manager.h)

// NV3023 color SPI TFT (4-wire, write-only) 🔬, panel 240 x 296
#define DISPLAY_SDA GPIO_NUM_10  // MOSI
#define DISPLAY_SCL GPIO_NUM_9   // SCLK
#define DISPLAY_DC  GPIO_NUM_8
#define DISPLAY_CS  GPIO_NUM_14
#define DISPLAY_RES GPIO_NUM_18

// Panel is 240 x 296 native; mounted rotated, so the UI runs 296 x 240
// (landscape) via swap_xy. Values below match stock firmware v2.5.7 exactly
// (recovered from live_app.bin: LcdDisplay(296, 240, 24, 0, true, true, true),
// panel swap_xy=1, mirror(0,1), invert=0).
#define DISPLAY_WIDTH   296
#define DISPLAY_HEIGHT  240
#define DISPLAY_SWAP_XY  true
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true
#define DISPLAY_OFFSET_X 24
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_BACKLIGHT_PIN           GPIO_NUM_13  // PWM ✅
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#endif // _BOARD_CONFIG_H_
