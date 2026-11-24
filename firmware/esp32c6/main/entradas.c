/*
 * entradas.c — Sensor de presencia (PIR o ultrasónico HC-SR04) y botón de factory reset.
 *
 * Se usa muestreo periódico cada 10 ms en lugar de interrupciones porque:
 *  - el antirrebote por muestreo es simple y fácil de verificar;
 *  - un botón que rebota genera ráfagas de interrupciones que no aportan nada;
 *  - 10 ms de latencia es irrelevante para una persona o un PIR.
 *
 * PIR: durante EPP_PIR_CALENTAMIENTO_S no se reportan cambios. Después, un
 *      cambio solo se acepta si la salida se mantiene EPP_PIR_FILTRO_MS.
 *      Al terminar el calentamiento se envía un EVENTO_PIR con listo=1.
 * HC-SR04 (opción por defecto, ver menuconfig → Entradas): cada EPP_US_PERIODO_MS se da un pulso
 *      de 10 µs en TRIG; una interrupción en ECHO mide el ancho del eco con esp_timer (µs).
 *      distancia = ancho × 343 m/s / 2. Se toma la mediana de las últimas 5 mediciones (descarta
 *      ecos sueltos) y la presencia se decide con histéresis temporal: en rango [MIN, MAX]
 *      durante EPP_US_CONFIRMAR_MS = presencia; fuera de rango EPP_US_AUSENCIA_MS = ausencia.
 *      La presencia se reporta con el mismo EVENTO_PIR (el daemon no cambia) y la distancia
 *      con EVENTO_DISTANCIA cada EPP_US_REPORTE_MS.
 *      ⚠ ECHO sale a 5 V: va al GPIO por un divisor de voltaje (README, sección 2.5).
 * Botón: activo en bajo (a GND, pull-up interno). Tras el antirrebote,
 *      si se mantiene presionado 1 s se muestra el aviso en los LEDs; al
 *      llegar a EPP_BOTON_FACTORY_MS se envía EVENTO_BOTON y se repite cada
 *      500 ms (máx. 10 veces) hasta que la Jetson responda ACK.
 */
#include <stdatomic.h>

#include "driver/gpio.h"
#include "epp.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
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

#if CONFIG_EPP_SENSOR_ULTRASONICO
/* ------------------------- HC-SR04 ------------------------- */
#define US_MUESTRAS      5      /* ventana de la mediana */
#define US_ANCHO_MAX_US  30000  /* > 30 ms (> 5 m) = sin eco util */

static _Atomic uint32_t s_eco_subida_us = 0; /* 32 bits: el C6 es RISC-V de 32 bits (atomicos nativos) */
static _Atomic int32_t s_eco_ancho_us = -1;  /* ultimo ancho medido; -1 = ninguno desde el disparo */
static _Atomic uint16_t s_distancia_mm = 0;
static _Atomic bool s_distancia_valida = false;

/* Interrupcion en ambos flancos de ECHO: guarda el instante de subida y calcula el ancho en la bajada. */
static void IRAM_ATTR isr_eco(void *arg)
{
    (void)arg;
    /* 32 bits de µs dan la vuelta cada ~71 min; la resta sin signo sigue siendo correcta */
    const uint32_t t = (uint32_t)esp_timer_get_time();
    if (gpio_get_level(CONFIG_EPP_PIN_PIR)) {
        atomic_store(&s_eco_subida_us, t | 1u); /* | 1: nunca 0, que significa "sin subida" */
    } else {
        const uint32_t subida = atomic_load(&s_eco_subida_us);
        if (subida != 0) {
            atomic_store(&s_eco_ancho_us, (int32_t)(t - subida));
        }
        atomic_store(&s_eco_subida_us, 0u);
    }
}

static void us_init(void)
{
    gpio_config_t trig = {
        .pin_bit_mask = 1ULL << CONFIG_EPP_PIN_TRIG,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&trig));
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_EPP_PIN_TRIG, 0));

    gpio_config_t eco = {
        .pin_bit_mask = 1ULL << CONFIG_EPP_PIN_PIR,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE, /* sin sensor = sin eco = "sin presencia" */
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&eco));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(CONFIG_EPP_PIN_PIR, isr_eco, NULL));
}

/* Mediana de los valores validos (0 = no valido). Devuelve 0 si hay menos de 3 validos. */
static uint16_t mediana_validos(const uint16_t *v, int n)
{
    uint16_t o[US_MUESTRAS];
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (v[i] == 0) {
            continue;
        }
        /* insercion ordenada (n es 5: no hace falta nada mas elaborado) */
        int j = k++;
        while (j > 0 && o[j - 1] > v[i]) {
            o[j] = o[j - 1];
            j--;
        }
        o[j] = v[i];
    }
    return k >= 3 ? o[k / 2] : 0;
}
#endif

void entradas_init(void)
{
#if CONFIG_EPP_SENSOR_ULTRASONICO
    us_init();
#else
    gpio_config_t pir = {
        .pin_bit_mask = 1ULL << CONFIG_EPP_PIN_PIR,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE, /* sin sensor conectado = "sin presencia" */
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pir));
#endif

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

#if CONFIG_EPP_SENSOR_ULTRASONICO
    /* HC-SR04 */
    uint16_t muestras[US_MUESTRAS] = {0};
    int idx = 0, medidas = 0;
    bool presencia = false, en_rango_ant = false;
    int64_t en_rango_desde = t0, prox_disparo = t0, prox_reporte = t0;
    ESP_LOGI(TAG, "HC-SR04: presencia entre %d y %d cm, medicion cada %d ms", CONFIG_EPP_US_DIST_MIN_CM,
             CONFIG_EPP_US_DIST_MAX_CM, CONFIG_EPP_US_PERIODO_MS);
#else
    /* PIR */
    bool pir_estable = false, pir_crudo_ant = false;
    int64_t pir_cambio_desde = t0;
#endif

    /* Botón */
    bool bot_estable = false, bot_crudo_ant = false;
    int64_t bot_cambio_desde = t0, bot_presionado_desde = 0;
    bool aviso_mostrado = false, solicitud_enviada = false;
    int reintentos = 0;
    int64_t proximo_reintento = 0;

    TickType_t ultimo = xTaskGetTickCount();
#if !CONFIG_EPP_SENSOR_ULTRASONICO
    ESP_LOGI(TAG, "PIR en calentamiento %d s", CONFIG_EPP_PIR_CALENTAMIENTO_S);
#endif

    for (;;) {
        int64_t t = ahora_ms();

#if CONFIG_EPP_SENSOR_ULTRASONICO
        /* ----------------------- HC-SR04 ----------------------- */
        if (t >= prox_disparo) {
            /* 1) recoger el eco del disparo anterior (si no llego, cuenta como "sin eco") */
            const int32_t ancho = atomic_exchange(&s_eco_ancho_us, -1);
            uint16_t mm = 0;
            if (ancho > 0 && ancho < US_ANCHO_MAX_US) {
                mm = (uint16_t)(((int64_t)ancho * 1715 + 5000) / 10000); /* 0,1715 mm/µs = 343 m/s / 2 */
            }
            muestras[idx] = mm;
            idx = (idx + 1) % US_MUESTRAS;
            if (medidas < US_MUESTRAS) {
                medidas++;
            }
            const uint16_t d = mediana_validos(muestras, medidas);
            atomic_store(&s_distancia_mm, d);
            atomic_store(&s_distancia_valida, d != 0);

            /* 2) presencia con histeresis temporal */
            const bool en_rango = d != 0 && d >= CONFIG_EPP_US_DIST_MIN_CM * 10 && d <= CONFIG_EPP_US_DIST_MAX_CM * 10;
            if (en_rango != en_rango_ant) {
                en_rango_ant = en_rango;
                en_rango_desde = t;
            }
            if (medidas == US_MUESTRAS && !atomic_load(&s_pir_listo)) {
                presencia = en_rango;
                atomic_store(&s_pir, presencia);
                atomic_store(&s_pir_listo, true);
                ESP_LOGI(TAG, "HC-SR04 listo: %u mm", d);
                enviar_pir(presencia, true);
            } else if (atomic_load(&s_pir_listo) && en_rango != presencia &&
                       (t - en_rango_desde) >= (en_rango ? CONFIG_EPP_US_CONFIRMAR_MS : CONFIG_EPP_US_AUSENCIA_MS)) {
                presencia = en_rango;
                atomic_store(&s_pir, presencia);
                enviar_pir(presencia, true);
            }

            /* 3) nuevo disparo: pulso de 10 µs en TRIG */
            gpio_set_level(CONFIG_EPP_PIN_TRIG, 1);
            esp_rom_delay_us(10);
            gpio_set_level(CONFIG_EPP_PIN_TRIG, 0);
            prox_disparo = t + CONFIG_EPP_US_PERIODO_MS;
        }
        if (CONFIG_EPP_US_REPORTE_MS > 0 && t >= prox_reporte && atomic_load(&s_pir_listo)) {
            proto_trama_t dt;
            proto_msg_evento_distancia(&dt, atomic_load(&s_distancia_mm), atomic_load(&s_distancia_valida));
            enlace_enviar(&dt);
            prox_reporte = t + CONFIG_EPP_US_REPORTE_MS;
        }
#else
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

#endif

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
