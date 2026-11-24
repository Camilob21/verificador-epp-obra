/*
 * entradas.c — Sensor PIR y botón de factory reset.
 *
 * Se usa muestreo periódico cada 10 ms en lugar de interrupciones porque:
 *  - el antirrebote por muestreo es simple y fácil de verificar;
 *  - un botón que rebota genera ráfagas de interrupciones que no aportan nada;
 *  - 10 ms de latencia es irrelevante para una persona o un PIR.
 *
 * PIR: durante EPP_PIR_CALENTAMIENTO_S no se reportan cambios. Después, un
 *      cambio solo se acepta si la salida se mantiene EPP_PIR_FILTRO_MS.
 *      Al terminar el calentamiento se envía un EVENTO_PIR con listo=1.
 * Botón: activo en bajo (a GND, pull-up interno). Tras el antirrebote,
 *      si se mantiene presionado 1 s se muestra el aviso en los LEDs; al
 *      llegar a EPP_BOTON_FACTORY_MS se envía EVENTO_BOTON y se repite cada
 *      500 ms (máx. 10 veces) hasta que la Jetson responda ACK.
 */
#include <stdatomic.h>

#include "driver/gpio.h"
#include "epp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "entradas";

#define PERIODO_MS          10
#define AVISO_BOTON_MS      1000
#define REINTENTO_BOTON_MS  500
#define MAX_REINTENTOS      10

static _Atomic bool s_pir = false;
static _Atomic bool s_pir_listo = false;
static _Atomic bool s_boton = false;
static _Atomic bool s_boton_ack = false;

bool entradas_pir_presencia(void) { return atomic_load(&s_pir); }
bool entradas_pir_listo(void) { return atomic_load(&s_pir_listo); }
bool entradas_boton_presionado(void) { return atomic_load(&s_boton); }
void entradas_boton_confirmado(void) { atomic_store(&s_boton_ack, true); }

static int64_t ahora_ms(void) { return esp_timer_get_time() / 1000; }

void entradas_init(void)
{
    gpio_config_t pir = {
        .pin_bit_mask = 1ULL << CONFIG_EPP_PIN_PIR,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE, /* sin sensor conectado = "sin presencia" */
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pir));

    gpio_config_t bot = {
        .pin_bit_mask = 1ULL << CONFIG_EPP_PIN_BOTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bot));
}

static void enviar_pir(bool presencia, bool listo)
{
    proto_trama_t t;
    proto_msg_evento_pir(&t, presencia, listo);
    enlace_enviar(&t);
    ESP_LOGI(TAG, "EVENTO_PIR presencia=%d listo=%d", presencia, listo);
}

static void tarea_entradas(void *arg)
{
    (void)arg;
    const int64_t t0 = ahora_ms();

    /* PIR */
    bool pir_estable = false, pir_crudo_ant = false;
    int64_t pir_cambio_desde = t0;

    /* Botón */
    bool bot_estable = false, bot_crudo_ant = false;
    int64_t bot_cambio_desde = t0, bot_presionado_desde = 0;
    bool aviso_mostrado = false, solicitud_enviada = false;
    int reintentos = 0;
    int64_t proximo_reintento = 0;

    TickType_t ultimo = xTaskGetTickCount();
    ESP_LOGI(TAG, "PIR en calentamiento %d s", CONFIG_EPP_PIR_CALENTAMIENTO_S);

    for (;;) {
        int64_t t = ahora_ms();

        /* ------------------------- PIR ------------------------- */
        bool pir_crudo = gpio_get_level(CONFIG_EPP_PIN_PIR) != 0;
        if (pir_crudo != pir_crudo_ant) {
            pir_crudo_ant = pir_crudo;
            pir_cambio_desde = t;
        }
        bool calentando = (t - t0) < (int64_t)CONFIG_EPP_PIR_CALENTAMIENTO_S * 1000;
        if (!calentando && !atomic_load(&s_pir_listo)) {
            atomic_store(&s_pir_listo, true);
            pir_estable = pir_crudo;
            atomic_store(&s_pir, pir_estable);
            ESP_LOGI(TAG, "PIR listo");
            enviar_pir(pir_estable, true);
        } else if (!calentando && pir_crudo != pir_estable &&
                   (t - pir_cambio_desde) >= CONFIG_EPP_PIR_FILTRO_MS) {
            pir_estable = pir_crudo;
            atomic_store(&s_pir, pir_estable);
            enviar_pir(pir_estable, true);
        }

        /* ------------------------ Botón ------------------------ */
        bool bot_crudo = gpio_get_level(CONFIG_EPP_PIN_BOTON) == 0; /* activo en bajo */
        if (bot_crudo != bot_crudo_ant) {
            bot_crudo_ant = bot_crudo;
            bot_cambio_desde = t;
        }
        if (bot_crudo != bot_estable && (t - bot_cambio_desde) >= CONFIG_EPP_BOTON_ANTIRREBOTE_MS) {
            bot_estable = bot_crudo;
            atomic_store(&s_boton, bot_estable);
            if (bot_estable) {
                bot_presionado_desde = t;
                solicitud_enviada = false;
            } else {
                if (aviso_mostrado && !solicitud_enviada) {
                    salidas_aviso_boton(0); /* soltó antes de tiempo: cancelar aviso */
                    ESP_LOGI(TAG, "boton soltado antes de %d ms: cancelado", CONFIG_EPP_BOTON_FACTORY_MS);
                }
                aviso_mostrado = false;
            }
        }

        if (bot_estable && !solicitud_enviada) {
            int64_t dur = t - bot_presionado_desde;
            if (!aviso_mostrado && dur >= AVISO_BOTON_MS) {
                aviso_mostrado = true;
                salidas_aviso_boton(1);
            }
            if (dur >= CONFIG_EPP_BOTON_FACTORY_MS) {
                solicitud_enviada = true;
                salidas_aviso_boton(2);
                atomic_store(&s_boton_ack, false);
                reintentos = 0;
                proximo_reintento = t; /* enviar ya */
                ESP_LOGW(TAG, "FACTORY RESET solicitado (%lld ms presionado)", (long long)dur);
            }
        }

        /* Envío y reintentos de EVENTO_BOTON hasta recibir ACK. */
        if (solicitud_enviada && reintentos < MAX_REINTENTOS && !atomic_load(&s_boton_ack) &&
            t >= proximo_reintento) {
            proto_trama_t ev;
            proto_msg_evento_boton(&ev);
            enlace_enviar(&ev);
            reintentos++;
            proximo_reintento = t + REINTENTO_BOTON_MS;
            if (reintentos == MAX_REINTENTOS) {
                ESP_LOGE(TAG, "EVENTO_BOTON sin ACK tras %d intentos", MAX_REINTENTOS);
            }
        }

        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_MS));
    }
}

void entradas_iniciar_tarea(void)
{
    BaseType_t ok = xTaskCreate(tarea_entradas, "entradas", EPP_STACK_ENTRADAS, NULL,
                                EPP_PRIO_ENTRADAS, &epp_tareas[EPP_T_ENTRADAS]);
    configASSERT(ok == pdPASS);
}
