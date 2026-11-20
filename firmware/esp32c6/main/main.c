#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "protocolo.h"

static const char *TAG = "EPP_DEMO";

void app_main(void) {
    const uint8_t datos[] = {'1','2','3','4','5','6','7','8','9'};
    uint16_t crc = epp_crc16_ccitt_false(datos, sizeof(datos));
    ESP_LOGW(TAG, "ESQUELETO: no hay control de relé, LEDs ni buzzer");
    ESP_LOGI(TAG, "Hello world ESP32-C6 | CRC test vector=0x%04X", (unsigned)crc);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
