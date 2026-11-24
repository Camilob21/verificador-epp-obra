/*
 * main.c — Punto de entrada del firmware EPP (ESP32-C6, ESP-IDF).
 *
 * Orden de arranque (importa):
 *   1. salidas_init_seguro(): relé APAGADO y LED rojo ENCENDIDO antes que
 *      cualquier otra cosa (fail-safe desde el primer instante del programa).
 *   2. Entradas y enlace (UART o USB).
 *   3. Tareas, de la más crítica a la menos.
 *   4. EVENTO_ARRANQUE a la Jetson con el motivo del último reinicio
 *      (permite detectar reinicios por watchdog, brownout o pánico).
 */
#include "epp.h"
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"

static const char *TAG = "main";

TaskHandle_t epp_tareas[EPP_T_N];

static const char *motivo_texto(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON:  return "encendido";
    case ESP_RST_SW:       return "esp_restart";
    case ESP_RST_PANIC:    return "panico";
    case ESP_RST_INT_WDT:  return "watchdog de interrupciones";
    case ESP_RST_TASK_WDT: return "watchdog de tareas";
    case ESP_RST_WDT:      return "otro watchdog";
    case ESP_RST_BROWNOUT: return "brownout (caida de voltaje)";
    case ESP_RST_EXT:      return "pin externo";
    case ESP_RST_USB:      return "USB";
    default:               return "otro";
    }
}

void app_main(void)
{
    salidas_init_seguro(); /* SIEMPRE primero */

    esp_reset_reason_t motivo = esp_reset_reason();
    ESP_LOGI(TAG, "EPP firmware v%d.%d, protocolo v%u. Motivo del reinicio: %s (%d)",
             EPP_FW_MAYOR, EPP_FW_MENOR, PROTO_VERSION, motivo_texto(motivo), (int)motivo);
    ESP_LOGI(TAG, "Pines: R=%d A=%d V=%d BZ=%d RELE=%d PIR=%d BOTON=%d TX=%d RX=%d",
             CONFIG_EPP_PIN_LED_ROJO, CONFIG_EPP_PIN_LED_AMARILLO, CONFIG_EPP_PIN_LED_VERDE,
             CONFIG_EPP_PIN_BUZZER, CONFIG_EPP_PIN_RELE, CONFIG_EPP_PIN_PIR, CONFIG_EPP_PIN_BOTON,
             CONFIG_EPP_PIN_UART_TX, CONFIG_EPP_PIN_UART_RX);
    ESP_LOGI(TAG, "Fail-safe: %d ms sin latido (revision cada %d ms). Verde max: %d ms",
             CONFIG_EPP_LATIDO_TIMEOUT_MS, CONFIG_EPP_ENLACE_PERIODO_MS, CONFIG_EPP_VERDE_MAX_MS);

    entradas_init();
    enlace_init();

    enlace_iniciar_tareas();
    salidas_iniciar_tarea();
    entradas_iniciar_tarea();
    telemetria_iniciar_tarea();

    proto_trama_t t;
    proto_msg_arranque(&t, (uint8_t)motivo, EPP_FW_MAYOR, EPP_FW_MENOR);
    enlace_enviar(&t);

    ESP_LOGI(TAG, "Listo. Esperando el primer latido de la Jetson (LED rojo fijo).");
    /* app_main puede terminar: las tareas siguen corriendo. */
}
