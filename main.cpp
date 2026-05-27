/**
 * @file    main.cpp
 * @brief   ESP32 Automotive Dashboard Firmware (ESP-IDF)
 * @details Đọc ADC từ biến trở (Speed/RPM) và nút nhấn (PRND),
 *          sau đó gửi dữ liệu qua UART theo định dạng:
 *          S[Speed],R[RPM],G[Gear],T[Trip]\n
 *
 * Sơ đồ kết nối phần cứng:
 *   GPIO34 (ADC1_CH6) → Biến trở RPM (0-3.3V)
 *   GPIO35 (ADC1_CH7) → Biến trở Speed (0-3.3V)
 *   GPIO18             → Nút P (Park)
 *   GPIO19             → Nút R (Reverse)
 *   GPIO21             → Nút N (Neutral)
 *   GPIO22             → Nút D (Drive)
 *   TX0/RX0            → Giao tiếp UART với PC qua USB
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

// ─────────────────────────────────────────────
// CẤU HÌNH PHẦN CỨNG
// ─────────────────────────────────────────────
#define TAG             "DASHBOARD"

// ADC Channels
#define ADC_RPM_CHANNEL     ADC1_CHANNEL_6   // GPIO34
#define ADC_SPEED_CHANNEL   ADC1_CHANNEL_7   // GPIO35
#define ADC_MAX_RAW         4095             // 12-bit ADC

// Giá trị max
#define MAX_RPM             8000
#define MAX_SPEED           240

// GPIO nút nhấn PRND
#define BTN_P   GPIO_NUM_18
#define BTN_R   GPIO_NUM_19
#define BTN_N   GPIO_NUM_21
#define BTN_D   GPIO_NUM_22

// UART
#define UART_PORT       UART_NUM_0
#define UART_BAUD       115200
#define UART_TX         GPIO_NUM_1
#define UART_RX         GPIO_NUM_3
#define TX_BUF_SIZE     256

// Chu kỳ gửi: 20ms = 50FPS
#define SEND_INTERVAL_MS    20

// ─────────────────────────────────────────────
// BIẾN TOÀN CỤC
// ─────────────────────────────────────────────
static char  current_gear = 'D';   // Gear mặc định: Drive
static float trip_km      = 0.0f;  // Quãng đường tích lũy

// ─────────────────────────────────────────────
// KHỞI TẠO ADC
// Sử dụng độ phân giải 12-bit (0-4095)
// Attenuation 11dB → đo được điện áp 0-3.3V
// ─────────────────────────────────────────────
static void adc_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_RPM_CHANNEL,   ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_SPEED_CHANNEL, ADC_ATTEN_DB_11);
}

// ─────────────────────────────────────────────
// ĐỌC VÀ CHUYỂN ĐỔI ADC → GIÁ TRỊ THỰC
// Dùng kỹ thuật lấy trung bình 8 mẫu để giảm nhiễu ADC
// ─────────────────────────────────────────────
static int adc_read_avg(adc1_channel_t channel, int samples)
{
    long sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += adc1_get_raw(channel);
    }
    return (int)(sum / samples);
}

static int read_rpm(void)
{
    int raw = adc_read_avg(ADC_RPM_CHANNEL, 8);
    // Map: 0..4095 → 0..8000
    return (int)((float)raw / ADC_MAX_RAW * MAX_RPM);
}

static int read_speed(void)
{
    int raw = adc_read_avg(ADC_SPEED_CHANNEL, 8);
    // Map: 0..4095 → 0..240
    return (int)((float)raw / ADC_MAX_RAW * MAX_SPEED);
}

// ─────────────────────────────────────────────
// KHỞI TẠO GPIO NÚT NHẤN
// INPUT_PULLUP: nút nhấn nối GND → LOW khi nhấn
// ─────────────────────────────────────────────
static void gpio_buttons_init(void)
{
    gpio_num_t btns[] = { BTN_P, BTN_R, BTN_N, BTN_D };
    gpio_config_t cfg = {
        .pin_bit_mask = 0,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    for (int i = 0; i < 4; i++) {
        cfg.pin_bit_mask = (1ULL << btns[i]);
        gpio_config(&cfg);
    }
}

// ─────────────────────────────────────────────
// DEBOUNCE VÀ ĐỌC TRẠNG THÁI PRND
//
// Thuật toán debounce: Chỉ ghi nhận thay đổi nếu
// trạng thái ổn định liên tiếp trong 50ms.
// Tránh tín hiệu nhiễu cơ học từ nút nhấn.
// ─────────────────────────────────────────────
typedef struct {
    gpio_num_t pin;
    char       gear;
    int        last_state;
    int        stable_state;
    uint32_t   last_change_ms;
} ButtonDebounce;

static ButtonDebounce buttons[4];
#define DEBOUNCE_MS 50

static void buttons_debounce_init(void)
{
    buttons[0] = { BTN_P, 'P', 1, 1, 0 };
    buttons[1] = { BTN_R, 'R', 1, 1, 0 };
    buttons[2] = { BTN_N, 'N', 1, 1, 0 };
    buttons[3] = { BTN_D, 'D', 1, 1, 0 };
}

static void update_gear_from_buttons(void)
{
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    for (int i = 0; i < 4; i++) {
        int current = gpio_get_level(buttons[i].pin);

        // Phát hiện thay đổi edge
        if (current != buttons[i].last_state) {
            buttons[i].last_change_ms = now_ms;
            buttons[i].last_state     = current;
        }

        // Kiểm tra đã ổn định chưa (qua thời gian DEBOUNCE_MS)
        if ((now_ms - buttons[i].last_change_ms) > DEBOUNCE_MS) {
            if (current != buttons[i].stable_state) {
                buttons[i].stable_state = current;
                // LOW (0) = nút được nhấn (active low)
                if (current == 0) {
                    current_gear = buttons[i].gear;
                    ESP_LOGI(TAG, "Gear changed to: %c", current_gear);
                }
            }
        }
    }
}

// ─────────────────────────────────────────────
// TÍNH TOÁN TRIP / ODO
// Tích phân vận tốc theo thời gian → quãng đường
// distance = speed (km/h) × delta_time (h)
// ─────────────────────────────────────────────
static void update_trip(int speed_kmh, float delta_sec)
{
    // Chỉ tính khi đang ở Gear D hoặc R và speed > 0
    if ((current_gear == 'D' || current_gear == 'R') && speed_kmh > 0) {
        float delta_hour = delta_sec / 3600.0f;
        trip_km += (float)speed_kmh * delta_hour;
    }
}

// ─────────────────────────────────────────────
// KHỞI TẠO UART
// ─────────────────────────────────────────────
static void uart_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_PORT, &uart_cfg);
    uart_driver_install(UART_PORT, TX_BUF_SIZE * 2, TX_BUF_SIZE * 2, 0, NULL, 0);
    ESP_LOGI(TAG, "UART initialized at %d baud", UART_BAUD);
}

// ─────────────────────────────────────────────
// TASK CHÍNH: ĐỌC VÀ GỬI DỮ LIỆU
// ─────────────────────────────────────────────
static void dashboard_task(void *pvParameters)
{
    char    tx_buf[64];
    float   delta_sec = SEND_INTERVAL_MS / 1000.0f;
    TickType_t last_wake = xTaskGetTickCount();

    ESP_LOGI(TAG, "Dashboard task started. Sending at %dms intervals.", SEND_INTERVAL_MS);

    while (1) {
        // 1. Đọc giá trị cảm biến
        int speed = read_speed();
        int rpm   = read_rpm();

        // 2. Cập nhật trạng thái nút PRND
        update_gear_from_buttons();

        // 3. Cập nhật trip
        update_trip(speed, delta_sec);

        // 4. Đóng gói dữ liệu theo format: S[Speed],R[RPM],G[Gear],T[Trip]\n
        int len = snprintf(tx_buf, sizeof(tx_buf),
                           "S%d,R%d,G%c,T%.1f\n",
                           speed, rpm, current_gear, trip_km);

        // 5. Gửi qua UART
        uart_write_bytes(UART_PORT, tx_buf, len);

        // 6. Giữ chu kỳ chính xác 20ms bằng vTaskDelayUntil
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SEND_INTERVAL_MS));
    }
}

// ─────────────────────────────────────────────
// ENTRY POINT
// ─────────────────────────────────────────────
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== Automotive Dashboard Firmware v1.0 ===");

    adc_init();
    gpio_buttons_init();
    buttons_debounce_init();
    uart_init();

    // Tạo FreeRTOS task trên Core 1, stack 4KB, priority 5
    xTaskCreatePinnedToCore(
        dashboard_task,
        "dashboard_task",
        4096,
        NULL,
        5,
        NULL,
        1   // Pin to Core 1 (Core 0 dành cho WiFi/BT nếu cần)
        );
}