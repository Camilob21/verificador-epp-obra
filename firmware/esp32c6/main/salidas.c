/*
 * salidas.c — LEDs del semáforo, buzzer y relé.
 *
 * Esta tarea es la ÚNICA que escribe en los GPIO de LEDs y buzzer, y la que
 * decide el relé. Las demás tareas le piden cosas por una cola. Así no hay
 * dos tareas peleando por el mismo pin. La única excepción es
 * salidas_rele_apagar_ya(), que cualquier tarea puede llamar para apagar el
 * relé de inmediato (apagar siempre es seguro).
 *
 * Regla del relé (se evalúa cada 10 ms):
 *   relé ON  <=>  hay enlace  Y  estado == VERDE  Y  lleva < EPP_VERDE_MAX_MS en verde
 */
#include <stdatomic.h>

#include "driver/gpio.h"
#include "epp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/queue.h"
#include "sdkconfig.h"

#if CONFIG_EPP_BUZZER_PASIVO
#include "driver/ledc.h"
#endif

static const char *TAG = "salidas";

#define PERIODO_MS 10

#if CONFIG_EPP_RELE_ACTIVO_BAJO
#define RELE_ON  0
#define RELE_OFF 1
#else
#define RELE_ON  1
#define RELE_OFF 0
#endif

typedef enum { CMD_ESTADO, CMD_BUZZER, CMD_ENLACE, CMD_AVISO_BOTON } cmd_tipo_t;
typedef struct {
    cmd_tipo_t tipo;
    uint8_t valor;
} cmd_t;

static QueueHandle_t s_cola;
static _Atomic uint8_t s_estado_mostrado = PROTO_ST_SIN_ENLACE;
static _Atomic bool s_rele = false;

/* Patrones de buzzer: duraciones en ms alternando ON, OFF, ON... terminadas en 0. */
static const uint16_t PAT_BIP_CORTO[] = {100, 0};
static const uint16_t PAT_BIP_DOBLE[] = {100, 100, 100, 0};
static const uint16_t PAT_OK[]        = {600, 0};
static const uint16_t PAT_ERROR[]     = {250, 150, 250, 150, 250, 0};

static const uint16_t *patron_de(proto_buzzer_t p)
{
    switch (p) {
    case PROTO_BZ_BIP_CORTO: return PAT_BIP_CORTO;
    case PROTO_BZ_BIP_DOBLE: return PAT_BIP_DOBLE;
    case PROTO_BZ_OK:        return PAT_OK;
    case PROTO_BZ_ERROR:     return PAT_ERROR;
    default:                 return NULL;
    }
}

static int64_t ahora_ms(void) { return esp_timer_get_time() / 1000; }

/* ------------------------------------------------------------------ */
/* Acceso al hardware                                                  */
/* ------------------------------------------------------------------ */
static void rele_escribir(bool on)
{
    gpio_set_level(CONFIG_EPP_PIN_RELE, on ? RELE_ON : RELE_OFF);
    atomic_store(&s_rele, on);
}

void salidas_rele_apagar_ya(void)
{
    /* gpio_set_level escribe en los registros W1TS/W1TC del GPIO (una sola
     * escritura): es seguro llamarla desde otra tarea. */
    rele_escribir(false);
}

static void leds(bool r, bool a, bool v)
{
    gpio_set_level(CONFIG_EPP_PIN_LED_ROJO, r);
    gpio_set_level(CONFIG_EPP_PIN_LED_AMARILLO, a);
    gpio_set_level(CONFIG_EPP_PIN_LED_VERDE, v);
}

static void buzzer_escribir(bool on)
{
#if CONFIG_EPP_BUZZER_PASIVO
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, on ? 512 : 0); /* 50 % de 10 bits */
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
#else
    gpio_set_level(CONFIG_EPP_PIN_BUZZER, on);
#endif
}

void salidas_init_seguro(void)
{
    /* 1) Relé: se fija el nivel de salida ANTES de habilitar el pin como
     *    salida, para que no haya un instante con el nivel equivocado.
     *    Durante el reset y el bootloader (antes de llegar aquí) el pin está
     *    como entrada: lo mantiene apagado la resistencia externa del
     *    diagrama de conexiones (pull-up o pull-down según el módulo). */
    gpio_set_level(CONFIG_EPP_PIN_RELE, RELE_OFF);
    gpio_config_t rele = {
        .pin_bit_mask = 1ULL << CONFIG_EPP_PIN_RELE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&rele));
    rele_escribir(false);

    /* 2) LEDs: rojo encendido desde el arranque. */
    gpio_config_t led = {
        .pin_bit_mask = (1ULL << CONFIG_EPP_PIN_LED_ROJO) | (1ULL << CONFIG_EPP_PIN_LED_AMARILLO) |
                        (1ULL << CONFIG_EPP_PIN_LED_VERDE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led));
    leds(true, false, false);

    /* 3) Buzzer apagado. */
#if CONFIG_EPP_BUZZER_PASIVO
    ledc_timer_config_t tim = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = CONFIG_EPP_BUZZER_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tim));
    ledc_channel_config_t ch = {
        .gpio_num = CONFIG_EPP_PIN_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
#else
    gpio_set_level(CONFIG_EPP_PIN_BUZZER, 0);
    gpio_config_t bz = {
        .pin_bit_mask = 1ULL << CONFIG_EPP_PIN_BUZZER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bz));
#endif
    buzzer_escribir(false);

    s_cola = xQueueCreate(8, sizeof(cmd_t));
    configASSERT(s_cola != NULL);
    ESP_LOGI(TAG, "Salidas en estado seguro: rele OFF (nivel %d), LED rojo ON", RELE_OFF);
}

/* ------------------------------------------------------------------ */
/* API para otras tareas (no bloquean: si la cola está llena se avisa)  */
/* ------------------------------------------------------------------ */
static void enviar_cmd(cmd_tipo_t tipo, uint8_t valor)
{
    cmd_t c = {.tipo = tipo, .valor = valor};
    if (xQueueSend(s_cola, &c, 0) != pdTRUE) {
        ESP_LOGW(TAG, "cola de salidas llena; comando %d descartado", (int)tipo);
    }
}

void salidas_ordenar_estado(proto_estado_t e) { enviar_cmd(CMD_ESTADO, (uint8_t)e); }
void salidas_buzzer(proto_buzzer_t p) { enviar_cmd(CMD_BUZZER, (uint8_t)p); }
void salidas_aviso_boton(uint8_t fase) { enviar_cmd(CMD_AVISO_BOTON, fase); }

void salidas_set_enlace(bool ok)
{
    if (!ok) {
        salidas_rele_apagar_ya(); /* no esperar al próximo ciclo de la tarea */
    }
    enviar_cmd(CMD_ENLACE, ok ? 1 : 0);
}

proto_estado_t salidas_estado_actual(void) { return (proto_estado_t)atomic_load(&s_estado_mostrado); }
bool salidas_rele_activo(void) { return atomic_load(&s_rele); }

/* ------------------------------------------------------------------ */
/* Tarea                                                               */
/* ------------------------------------------------------------------ */
static void tarea_salidas(void *arg)
{
    (void)arg;
    bool enlace = false;
    bool hubo_enlace = false; /* distingue "nunca conectó" de "perdió el enlace" */
    proto_estado_t ordenado = PROTO_ST_ROJO; /* "ante la duda, rojo" */
    int64_t verde_desde = 0;

    const uint16_t *bz_pat = NULL;
    int bz_idx = 0;
    int64_t bz_hasta = 0;

    uint8_t aviso = 0;
    int64_t aviso_hasta = 0;

#if CONFIG_EPP_PRUEBA_LAMPARAS
    /* Prueba de lámparas al arrancar (bring-up): R, A, V 200 ms cada uno y un
     * bip corto. Permite verificar el cableado de un vistazo. El relé sigue
     * apagado durante toda la prueba. */
    leds(false, true, false);
    vTaskDelay(pdMS_TO_TICKS(200));
    leds(false, false, true);
    buzzer_escribir(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_escribir(false);
    vTaskDelay(pdMS_TO_TICKS(100));
    leds(true, false, false);
#endif

    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    TickType_t ultimo = xTaskGetTickCount();

    for (;;) {
        int64_t t = ahora_ms();

        /* 1) Atender comandos pendientes. */
        cmd_t c;
        while (xQueueReceive(s_cola, &c, 0) == pdTRUE) {
            switch (c.tipo) {
            case CMD_ENLACE:
                enlace = c.valor != 0;
                hubo_enlace = hubo_enlace || enlace;
                ordenado = PROTO_ST_ROJO; /* al perder o recuperar enlace: rojo hasta nueva orden */
                break;
            case CMD_ESTADO:
                if (c.valor == PROTO_ST_VERDE && ordenado != PROTO_ST_VERDE) {
                    verde_desde = t;
                }
                ordenado = (proto_estado_t)c.valor;
                break;
            case CMD_BUZZER:
                bz_pat = patron_de((proto_buzzer_t)c.valor);
                bz_idx = 0;
                bz_hasta = bz_pat ? t + bz_pat[0] : 0;
                break;
            case CMD_AVISO_BOTON:
                aviso = c.valor;
                aviso_hasta = (aviso == 2) ? t + 1500 : 0;
                if (aviso == 2) {
                    bz_pat = PAT_OK;
                    bz_idx = 0;
                    bz_hasta = t + PAT_OK[0];
                }
                break;
            }
        }

        /* 2) Tiempo máximo en verde (protección local del ESP32). */
        if (enlace && ordenado == PROTO_ST_VERDE && (t - verde_desde) > CONFIG_EPP_VERDE_MAX_MS) {
            ESP_LOGW(TAG, "VERDE superó %d ms: paso a ROJO", CONFIG_EPP_VERDE_MAX_MS);
            ordenado = PROTO_ST_ROJO;
        }

        proto_estado_t efectivo = enlace ? ordenado : PROTO_ST_SIN_ENLACE;
        atomic_store(&s_estado_mostrado, (uint8_t)efectivo);

        /* 3) Relé. */
        bool rele_deseado = (efectivo == PROTO_ST_VERDE);
        if (rele_deseado != atomic_load(&s_rele)) {
            ESP_LOGI(TAG, "rele -> %s", rele_deseado ? "ON" : "OFF");
        }
        rele_escribir(rele_deseado);

        /* 4) LEDs. */
        if (aviso == 2 && t > aviso_hasta) {
            aviso = 0;
        }
        if (aviso == 1) {                         /* botón mantenido: rojo/amarillo alternando */
            bool fase = (t / 150) % 2;
            leds(fase, !fase, false);
        } else if (aviso == 2) {                  /* factory reset confirmado: los tres parpadean */
            bool on = (t / 100) % 2;
            leds(on, on, on);
        } else {
            switch (efectivo) {
            case PROTO_ST_REPOSO:            leds(false, false, false); break;
            case PROTO_ST_AMARILLO_PARPADEO: leds(false, (t / 500) % 2, false); break;
            case PROTO_ST_AMARILLO_FIJO:     leds(false, true, false); break;
            case PROTO_ST_VERDE:             leds(false, false, true); break;
            case PROTO_ST_ROJO:              leds(true, false, false); break;
            default: /* SIN_ENLACE */
                if (!hubo_enlace) {
                    leds(true, false, false);               /* arranque: rojo fijo */
                } else {
                    leds((t % 1000) >= 100, false, false);  /* enlace perdido: rojo con un apagón
                                                               de 100 ms cada segundo */
                }
                break;
            }
        }

        /* 5) Buzzer. */
        if (bz_pat != NULL) {
            if (t >= bz_hasta) {
                bz_idx++;
                if (bz_pat[bz_idx] == 0) {
                    bz_pat = NULL;
                } else {
                    bz_hasta = t + bz_pat[bz_idx];
                }
            }
        }
        buzzer_escribir(bz_pat != NULL && (bz_idx % 2) == 0);

        esp_task_wdt_reset();
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_MS));
    }
}

void salidas_iniciar_tarea(void)
{
    BaseType_t ok = xTaskCreate(tarea_salidas, "salidas", EPP_STACK_SALIDAS, NULL, EPP_PRIO_SALIDAS,
                                &epp_tareas[EPP_T_SALIDAS]);
    configASSERT(ok == pdPASS);
}
