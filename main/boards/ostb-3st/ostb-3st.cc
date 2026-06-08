#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "power_manager.h"
#include "power_save_timer.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_nv3023.h>
#include <esp_sleep.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "OSTB_3ST"

// NV3023 panel init sequence (shared with the other 78/nv3023 boards).
static const nv3023_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xff, (uint8_t[]){0xa5}, 1, 0},
    {0x3E, (uint8_t[]){0x09}, 1, 0},
    {0x3A, (uint8_t[]){0x65}, 1, 0},
    {0x82, (uint8_t[]){0x00}, 1, 0},
    {0x98, (uint8_t[]){0x00}, 1, 0},
    {0x63, (uint8_t[]){0x0f}, 1, 0},
    {0x64, (uint8_t[]){0x0f}, 1, 0},
    {0xB4, (uint8_t[]){0x34}, 1, 0},
    {0xB5, (uint8_t[]){0x30}, 1, 0},
    {0x83, (uint8_t[]){0x03}, 1, 0},
    {0x86, (uint8_t[]){0x04}, 1, 0},
    {0x87, (uint8_t[]){0x16}, 1, 0},
    {0x88, (uint8_t[]){0x0A}, 1, 0},
    {0x89, (uint8_t[]){0x27}, 1, 0},
    {0x93, (uint8_t[]){0x63}, 1, 0},
    {0x96, (uint8_t[]){0x81}, 1, 0},
    {0xC3, (uint8_t[]){0x10}, 1, 0},
    {0xE6, (uint8_t[]){0x00}, 1, 0},
    {0x99, (uint8_t[]){0x01}, 1, 0},
    {0x70, (uint8_t[]){0x09}, 1, 0},
    {0x71, (uint8_t[]){0x1D}, 1, 0},
    {0x72, (uint8_t[]){0x14}, 1, 0},
    {0x73, (uint8_t[]){0x0a}, 1, 0},
    {0x74, (uint8_t[]){0x11}, 1, 0},
    {0x75, (uint8_t[]){0x16}, 1, 0},
    {0x76, (uint8_t[]){0x38}, 1, 0},
    {0x77, (uint8_t[]){0x0B}, 1, 0},
    {0x78, (uint8_t[]){0x08}, 1, 0},
    {0x79, (uint8_t[]){0x3E}, 1, 0},
    {0x7a, (uint8_t[]){0x07}, 1, 0},
    {0x7b, (uint8_t[]){0x0D}, 1, 0},
    {0x7c, (uint8_t[]){0x16}, 1, 0},
    {0x7d, (uint8_t[]){0x0F}, 1, 0},
    {0x7e, (uint8_t[]){0x14}, 1, 0},
    {0x7f, (uint8_t[]){0x05}, 1, 0},
    {0xa0, (uint8_t[]){0x04}, 1, 0},
    {0xa1, (uint8_t[]){0x28}, 1, 0},
    {0xa2, (uint8_t[]){0x0c}, 1, 0},
    {0xa3, (uint8_t[]){0x11}, 1, 0},
    {0xa4, (uint8_t[]){0x0b}, 1, 0},
    {0xa5, (uint8_t[]){0x23}, 1, 0},
    {0xa6, (uint8_t[]){0x45}, 1, 0},
    {0xa7, (uint8_t[]){0x07}, 1, 0},
    {0xa8, (uint8_t[]){0x0a}, 1, 0},
    {0xa9, (uint8_t[]){0x3b}, 1, 0},
    {0xaa, (uint8_t[]){0x0d}, 1, 0},
    {0xab, (uint8_t[]){0x18}, 1, 0},
    {0xac, (uint8_t[]){0x14}, 1, 0},
    {0xad, (uint8_t[]){0x0F}, 1, 0},
    {0xae, (uint8_t[]){0x19}, 1, 0},
    {0xaf, (uint8_t[]){0x08}, 1, 0},
    {0xff, (uint8_t[]){0x00}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x29, (uint8_t[]){0x00}, 0, 10}
};

// CST816 capacitive touch — driven as a directly-polled I2C device, matching
// stock firmware (no INT/RST line configured). A tap toggles the chat state.
class Cst816Touch : public I2cDevice {
public:
    Cst816Touch(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {}

    // Returns the number of active touch points (0 = not touched).
    int ReadFingerCount() {
        uint8_t buffer[6];
        ReadRegs(0x02, buffer, sizeof(buffer));
        return buffer[0] & 0x0F;
    }
};

class Ostb3st : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    Cst816Touch* touch_ = nullptr;
    PowerManager* power_manager_ = nullptr;
    PowerSaveTimer* power_save_timer_ = nullptr;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    SpiLcdDisplay* display_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    TaskHandle_t touch_task_handle_ = nullptr;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializePowerManager() {
        power_manager_ = new PowerManager(CHARGE_STATUS_GPIO);
    }

    // Idle power saving: after kSecondsToSleep the CPU drops to a low clock and
    // auto light-sleeps (display dimmed); after kSecondsToShutdown the device
    // enters deep sleep, woken only by the BOOT button (the only RTC-capable
    // input — the CST816 touch has no INT line).
    void InitializePowerSaveTimer() {
        const int kSecondsToSleep = 60;
        const int kSecondsToShutdown = 300;
        power_save_timer_ = new PowerSaveTimer(240, kSecondsToSleep, kSecondsToShutdown);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Entering light sleep");
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            ESP_LOGI(TAG, "Exiting light sleep");
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Entering deep sleep, wake on BOOT button (GPIO%d)", BOOT_BUTTON_GPIO);
            esp_lcd_panel_disp_on_off(panel_, false);
            GetBacklight()->SetBrightness(0);
            // BOOT button is active-low; wake when it is pulled low. ESP32-S3
            // uses ext1 (ext0 is unsupported); GPIO0 is RTC-capable.
            esp_sleep_enable_ext1_wakeup_io(1ULL << BOOT_BUTTON_GPIO, ESP_EXT1_WAKEUP_ANY_LOW);
            esp_deep_sleep_start();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCL;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_HEIGHT * 80 * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeNv3023Display() {
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = NV3023_PANEL_IO_SPI_CONFIG(DISPLAY_CS, DISPLAY_DC, NULL, NULL);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &panel_io_));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        nv3023_vendor_config_t vendor_config = {
            .init_cmds = lcd_init_cmds,
            .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(nv3023_lcd_init_cmd_t),
        };
        panel_config.reset_gpio_num = DISPLAY_RES;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &vendor_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3023(panel_io_, &panel_config, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new SpiLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            ESP_LOGI(TAG, "Boot button clicked");
            if (power_save_timer_) power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        volume_up_button_.OnClick([this]() {
            ESP_LOGI(TAG, "Volume up clicked");
            if (power_save_timer_) power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            ESP_LOGI(TAG, "Volume down clicked");
            if (power_save_timer_) power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    void InitializeTouch() {
        touch_ = new Cst816Touch(i2c_bus_, TOUCH_I2C_ADDR);
        xTaskCreatePinnedToCore(TouchTask, "touch", 4 * 1024, this, 5, &touch_task_handle_, 1);
    }

    static void TouchTask(void* arg) {
        auto* self = static_cast<Ostb3st*>(arg);
        bool was_touched = false;
        while (true) {
            bool is_touched = self->touch_->ReadFingerCount() > 0;
            if (is_touched && !was_touched) {
                ESP_LOGI(TAG, "Touch detected");
                if (self->power_save_timer_) self->power_save_timer_->WakeUp();
            }
            // Trigger on the release edge so a tap toggles the chat state once.
            if (!is_touched && was_touched) {
                ESP_LOGI(TAG, "Touch released -> ToggleChatState");
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateStarting) {
                    self->EnterWifiConfigMode();
                } else {
                    app.ToggleChatState();
                }
            }
            was_touched = is_touched;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

public:
    Ostb3st() :
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeI2c();
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeSpi();
        InitializeNv3023Display();
        InitializeButtons();
        InitializeTouch();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR,
            AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        // Only allow idle sleep/shutdown while running on battery.
        if (power_save_timer_) power_save_timer_->SetEnabled(discharging);
        level = power_manager_->GetBatteryLevel();
        return true;
    }
};

DECLARE_BOARD(Ostb3st);
