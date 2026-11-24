/*
 * enlace.c — Comunicación con la Jetson y fail-safe por latido.
 *
 * Tarea "rx": lee bytes del transporte (UART1 o USB Serial/JTAG), los pasa
 *             por el parser y atiende cada trama válida.
 * Tarea "enlace": cada EPP_ENLACE_PERIODO_MS revisa cuánto hace que llegó el
 *             último LATIDO válido. Si supera EPP_LATIDO_TIMEOUT_MS: fail-safe
 *             (relé apagado, rojo, estado SIN_ENLACE) y avisa a la Jetson con
 *             EVENTO_ENLACE(0) por si ella sí puede oír al ESP32.
 *
 * Por qué el monitor es una tarea aparte y de mayor prioridad que "rx":
 * si "rx" se bloquea o se cuelga, el monitor igual detecta la falta de
 * latidos. Además ambas están vigiladas por el task watchdog.
 */
#include <stdatomic.h>
#include <string.h>

#include "epp.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#if CONFIG_EPP_TRANSPORTE_UART1
#include "driver/gpio.h"
#include "driver/uart.h"
#define EPP_UART UART_NUM_1
static QueueHandle_t s_uart_eventos;
#else
#include "driver/usb_serial_jtag.h"
#endif

static const char *TAG = "enlace";

static SemaphoreHandle_t s_tx_mutex;
static uint8_t s_tx_seq;

static proto_parser_t s_parser;           /* solo lo toca la tarea rx */
static SemaphoreHandle_t s_stats_mutex;   /* protege copias de s_parser.st */
static proto_stats_t s_stats_copia;

static _Atomic uint32_t s_ultimo_latido_ms; /* 0 = nunca */
static _Atomic bool s_enlace = false;
static _Atomic uint32_t s_err_uart;
static _Atomic uint32_t s_fallos_enlace;

static uint32_t ahora_ms(void) { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }

bool enlace_ok(void) { return atomic_load(&s_enlace); }

/* ------------------------------------------------------------------ */
/* Transporte                                                          */
/* ------------------------------------------------------------------ */
static void transporte_init(void)
{
#if CONFIG_EPP_TRANSPORTE_UART1
    uart_config_t cfg = {
        .baud_rate = CONFIG_EPP_UART_BAUDIOS,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    /* RX: 1024 B de buffer (~89 ms de datos a 115200). TX: 512 B para que
     * uart_write_bytes no bloquee a quien envía. Cola de 16 eventos para
     * enterarnos de desbordes y errores de trama. */
    ESP_ERROR_CHECK(uart_driver_install(EPP_UART, 1024, 512, 16, &s_uart_eventos, 0));
    ESP_ERROR_CHECK(uart_param_config(EPP_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(EPP_UART, CONFIG_EPP_PIN_UART_TX, CONFIG_EPP_PIN_UART_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    /* Pull-up en RX: si el cable de la Jetson está suelto, la línea queda en
     * reposo (alto) en vez de flotar y generar basura. */
    gpio_set_pull_mode(CONFIG_EPP_PIN_UART_RX, GPIO_PULLUP_ONLY);
    ESP_LOGI(TAG, "UART1 %d 8N1, TX=GPIO%d RX=GPIO%d", CONFIG_EPP_UART_BAUDIOS,
             CONFIG_EPP_PIN_UART_TX, CONFIG_EPP_PIN_UART_RX);
#else
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));
    ESP_LOGW(TAG, "MODO PRUEBA: protocolo por el USB nativo (USB Serial/JTAG)");
#endif
}

static int transporte_leer(uint8_t *buf, size_t max, TickType_t espera)
{
#if CONFIG_EPP_TRANSPORTE_UART1
    return uart_read_bytes(EPP_UART, buf, max, espera);
#else
    return usb_serial_jtag_read_bytes(buf, max, espera);
#endif
}

static void transporte_escribir(const uint8_t *buf, size_t n)
{
#if CONFIG_EPP_TRANSPORTE_UART1
    uart_write_bytes(EPP_UART, buf, n);
#else
    usb_serial_jtag_write_bytes(buf, n, pdMS_TO_TICKS(20));
#endif
}

/* Revisa (sin bloquear) los eventos de error del driver UART. */
static void transporte_revisar_errores(void)
{
#if CONFIG_EPP_TRANSPORTE_UART1
    uart_event_t ev;
    while (xQueueReceive(s_uart_eventos, &ev, 0) == pdTRUE) {
        switch (ev.type) {
        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
            /* Recomendación de la documentación de ESP-IDF: vaciar la entrada. */
            atomic_fetch_add(&s_err_uart, 1);
            uart_flush_input(EPP_UART);
            xQueueReset(s_uart_eventos);
            proto_parser_timeout(&s_parser);
            ESP_LOGW(TAG, "desborde en RX del UART");
            break;
        case UART_FRAME_ERR:
        case UART_PARITY_ERR:
            atomic_fetch_add(&s_err_uart, 1);
            break;
        default:
            break;
        }
    }
#endif
}

/* ------------------------------------------------------------------ */
/* Envío (seguro entre tareas)                                         */
/* ------------------------------------------------------------------ */
uint8_t enlace_enviar(proto_trama_t *t)
{
    uint8_t buf[PROTO_MAX_TRAMA];
    xSemaphoreTake(s_tx_mutex, portMAX_DELAY);
    if (t->tipo != PROTO_ACK && t->tipo != PROTO_NACK) {
        t->seq = s_tx_seq++;
    }
    size_t n = proto_codificar(t, buf, sizeof(buf));
    if (n > 0) {
        transporte_escribir(buf, n);
    }
    uint8_t seq = t->seq;
    xSemaphoreGive(s_tx_mutex);
    return seq;
}

static void responder_ack(const proto_trama_t *t)
{
    proto_trama_t r;
    proto_msg_ack(&r, t->seq, t->tipo);
    enlace_enviar(&r);
}

static void responder_nack(const proto_trama_t *t, proto_nack_t cod)
{
    proto_trama_t r;
    proto_msg_nack(&r, t->seq, t->tipo, cod);
    enlace_enviar(&r);
    ESP_LOGW(TAG, "NACK tipo=0x%02X seq=%u codigo=%d", t->tipo, t->seq, (int)cod);
}

/* ------------------------------------------------------------------ */
/* Atención de tramas válidas (llamado desde el parser, tarea rx)      */
/* ------------------------------------------------------------------ */
static void al_recibir(const proto_trama_t *t, void *ctx)
{
    (void)ctx;
    proto_nack_t v = proto_validar(t);
    if (v != PROTO_NACK_OK) {
        responder_nack(t, v);
        return;
    }

    switch (t->tipo) {
    case PROTO_LATIDO:
        atomic_store(&s_ultimo_latido_ms, ahora_ms() | 1u); /* |1: nunca vale 0 */
        break;

    case PROTO_ESTADO: {
        proto_estado_t e;
        if (!proto_leer_estado(t, &e)) {
            responder_nack(t, PROTO_NACK_VALOR);
        } else if (!enlace_ok()) {
            /* Sin latido válido no se obedece ningún estado: evita que un
             * VERDE "suelto" active el relé sin supervisión. */
            responder_nack(t, PROTO_NACK_SIN_ENLACE);
        } else {
            salidas_ordenar_estado(e);
            responder_ack(t);
            ESP_LOGI(TAG, "ESTADO %d (seq %u)", (int)e, t->seq);
        }
        break;
    }

    case PROTO_BUZZER: {
        proto_buzzer_t b;
        if (!proto_leer_buzzer(t, &b)) {
            responder_nack(t, PROTO_NACK_VALOR);
        } else {
            salidas_buzzer(b);
            responder_ack(t);
        }
        break;
    }

    case PROTO_PEDIR_TELEMETRIA:
        telemetria_pedir();
        break;

    case PROTO_ACK: {
        uint8_t tipo;
        if (proto_leer_ack(t, &tipo) && tipo == PROTO_EVENTO_BOTON) {
            entradas_boton_confirmado();
        }
        break;
    }

    case PROTO_NACK:
        ESP_LOGW(TAG, "la Jetson rechazó tipo 0x%02X (codigo %u)", t->datos[0], t->datos[1]);
        break;

    default:
        /* Tipo conocido pero que solo envía el ESP32 (p. ej. TELEMETRIA). */
        responder_nack(t, PROTO_NACK_TIPO_DESCONOCIDO);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Tareas                                                              */
/* ------------------------------------------------------------------ */
static void tarea_rx(void *arg)
{
    (void)arg;
    uint8_t buf[128];
    const TickType_t espera = pdMS_TO_TICKS(CONFIG_EPP_RX_TIMEOUT_TRAMA_MS);
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    for (;;) {
        int n = transporte_leer(buf, sizeof(buf), espera);
        if (n > 0) {
            proto_parser_feed(&s_parser, buf, (size_t)n);
        } else if (proto_parser_en_curso(&s_parser)) {
            /* Pasó EPP_RX_TIMEOUT_TRAMA_MS sin bytes con una trama a medias. */
            proto_parser_timeout(&s_parser);
        }
        transporte_revisar_errores();

        xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
        s_stats_copia = s_parser.st;
        xSemaphoreGive(s_stats_mutex);

        esp_task_wdt_reset();
    }
}

static void tarea_enlace(void *arg)
{
    (void)arg;
    TickType_t ultimo = xTaskGetTickCount();
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    for (;;) {
        uint32_t ult = atomic_load(&s_ultimo_latido_ms);
        bool vivo = (ult != 0) && ((uint32_t)(ahora_ms() - ult) <= CONFIG_EPP_LATIDO_TIMEOUT_MS);
        bool antes = atomic_load(&s_enlace);

        if (vivo && !antes) {
            atomic_store(&s_enlace, true);
            salidas_set_enlace(true);
            ESP_LOGI(TAG, "ENLACE OK (latido recibido)");
            proto_trama_t t;
            proto_msg_evento_enlace(&t, true);
            enlace_enviar(&t);
        } else if (!vivo && antes) {
            atomic_store(&s_enlace, false);
            salidas_set_enlace(false); /* apaga el relé de inmediato */
            atomic_fetch_add(&s_fallos_enlace, 1);
            ESP_LOGW(TAG, "FAIL-SAFE: %u ms sin latido valido", (unsigned)(ahora_ms() - ult));
            proto_trama_t t;
            proto_msg_evento_enlace(&t, false);
            enlace_enviar(&t);
        }

        esp_task_wdt_reset();
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(CONFIG_EPP_ENLACE_PERIODO_MS));
    }
}

void enlace_leer_stats(enlace_stats_t *s)
{
    xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
    s->parser = s_stats_copia;
    xSemaphoreGive(s_stats_mutex);
    s->err_uart = atomic_load(&s_err_uart);
    s->fallos_enlace = atomic_load(&s_fallos_enlace);
}

void enlace_init(void)
{
    s_tx_mutex = xSemaphoreCreateMutex();
    s_stats_mutex = xSemaphoreCreateMutex();
    configASSERT(s_tx_mutex && s_stats_mutex);
    proto_parser_init(&s_parser, al_recibir, NULL);
    transporte_init();
}

void enlace_iniciar_tareas(void)
{
    BaseType_t ok;
    ok = xTaskCreate(tarea_enlace, "enlace", EPP_STACK_ENLACE, NULL, EPP_PRIO_ENLACE,
                     &epp_tareas[EPP_T_ENLACE]);
    configASSERT(ok == pdPASS);
    ok = xTaskCreate(tarea_rx, "rx", EPP_STACK_RX, NULL, EPP_PRIO_RX, &epp_tareas[EPP_T_RX]);
    configASSERT(ok == pdPASS);
}
