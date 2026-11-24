/*
 * telemetria.c — Instrumentación "soft" del ESP32.
 *
 * Cada EPP_TELEMETRIA_PERIODO_MS (o cuando la Jetson envía PEDIR_TELEMETRIA)
 * se arma y envía una trama TELEMETRIA con:
 *   - heap libre y mínimo histórico         (esp_get_free_heap_size / esp_get_minimum_free_heap_size)
 *   - high-water mark del stack de 5 tareas (uxTaskGetStackHighWaterMark; en ESP-IDF viene en bytes)
 *   - uso de CPU en %                       (tiempo de la tarea IDLE, estadísticas de FreeRTOS)
 *   - contadores del parser y del UART, veces en fail-safe, tiempo encendido
 * También imprime un resumen en la consola de logs.
 */
#include <string.h>

#include "epp.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "telemetria";

static uint16_t sat16(uint32_t v) { return v > 0xFFFFu ? 0xFFFFu : (uint16_t)v; }

#if (configUSE_TRACE_FACILITY == 1) && (configGENERATE_RUN_TIME_STATS == 1)
/* Porcentaje de CPU usado desde la medición anterior = 100 - % de la tarea IDLE. */
static uint8_t medir_cpu(void)
{
    static configRUN_TIME_COUNTER_TYPE idle_ant = 0, total_ant = 0;
    UBaseType_t n = uxTaskGetNumberOfTasks();
    TaskStatus_t *v = pvPortMalloc(n * sizeof(TaskStatus_t));
    if (v == NULL) {
        return PROTO_CPU_NO_DISPONIBLE;
    }
    configRUN_TIME_COUNTER_TYPE total = 0, idle = 0;
    n = uxTaskGetSystemState(v, n, &total);
    for (UBaseType_t i = 0; i < n; i++) {
        if (strncmp(v[i].pcTaskName, "IDLE", 4) == 0) {
            idle += v[i].ulRunTimeCounter;
        }
    }
    vPortFree(v);

    configRUN_TIME_COUNTER_TYPE d_total = total - total_ant;
    configRUN_TIME_COUNTER_TYPE d_idle = idle - idle_ant;
    idle_ant = idle;
    total_ant = total;
    if (d_total == 0 || d_idle > d_total) {
        return PROTO_CPU_NO_DISPONIBLE;
    }
    return (uint8_t)(100u - (uint32_t)((100ull * d_idle) / d_total));
}
#else
static uint8_t medir_cpu(void) { return PROTO_CPU_NO_DISPONIBLE; }
#endif

static void armar(proto_telemetria_t *tel)
{
    memset(tel, 0, sizeof(*tel));
    enlace_stats_t es;
    enlace_leer_stats(&es);

    tel->version_proto = PROTO_VERSION;
    tel->estado = (uint8_t)salidas_estado_actual();
    tel->banderas = (enlace_ok() ? PROTO_TEL_ENLACE : 0) | (salidas_rele_activo() ? PROTO_TEL_RELE : 0) |
                    (entradas_pir_presencia() ? PROTO_TEL_PIR : 0) |
                    (entradas_pir_listo() ? PROTO_TEL_PIR_LISTO : 0) |
                    (entradas_boton_presionado() ? PROTO_TEL_BOTON : 0);
    tel->cpu_pct = medir_cpu();
    tel->uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
    tel->heap_libre = esp_get_free_heap_size();
    tel->heap_min = esp_get_minimum_free_heap_size();
    tel->tramas_ok = es.parser.tramas_ok;
    tel->err_crc = es.parser.err_crc;
    tel->err_longitud = sat16(es.parser.err_longitud);
    tel->err_timeout = sat16(es.parser.err_timeout);
    tel->err_uart = sat16(es.err_uart);
    tel->fallos_enlace = sat16(es.fallos_enlace);
    for (int i = 0; i < EPP_T_N; i++) {
        tel->stack_libre[i] =
            epp_tareas[i] ? sat16((uint32_t)uxTaskGetStackHighWaterMark(epp_tareas[i])) : 0;
    }
}

static void tarea_telemetria(void *arg)
{
    (void)arg;
    const TickType_t periodo = CONFIG_EPP_TELEMETRIA_PERIODO_MS > 0
                                   ? pdMS_TO_TICKS(CONFIG_EPP_TELEMETRIA_PERIODO_MS)
                                   : portMAX_DELAY;
    int ciclos = 0;
    for (;;) {
        /* Despierta por periodo o por notificación (PEDIR_TELEMETRIA). */
        ulTaskNotifyTake(pdTRUE, periodo);

        proto_telemetria_t tel;
        armar(&tel);
        proto_trama_t t;
        proto_msg_telemetria(&t, &tel);
        enlace_enviar(&t);

        if (++ciclos % 5 == 0) { /* log cada ~5 s para no saturar la consola */
            ESP_LOGI(TAG,
                     "up=%lus est=%u flags=0x%02X cpu=%u%% heap=%lu min=%lu ok=%lu crc=%lu "
                     "len=%u tmo=%u uart=%u failsafe=%u stack[rx,enl,sal,ent,tel]=%u,%u,%u,%u,%u",
                     (unsigned long)tel.uptime_s, tel.estado, tel.banderas, tel.cpu_pct,
                     (unsigned long)tel.heap_libre, (unsigned long)tel.heap_min,
                     (unsigned long)tel.tramas_ok, (unsigned long)tel.err_crc, tel.err_longitud,
                     tel.err_timeout, tel.err_uart, tel.fallos_enlace, tel.stack_libre[0],
                     tel.stack_libre[1], tel.stack_libre[2], tel.stack_libre[3],
                     tel.stack_libre[4]);
        }
    }
}

void telemetria_pedir(void)
{
    if (epp_tareas[EPP_T_TELEMETRIA]) {
        xTaskNotifyGive(epp_tareas[EPP_T_TELEMETRIA]);
    }
}

void telemetria_iniciar_tarea(void)
{
    BaseType_t ok = xTaskCreate(tarea_telemetria, "telemetria", EPP_STACK_TELEMETRIA, NULL,
                                EPP_PRIO_TELEMETRIA, &epp_tareas[EPP_T_TELEMETRIA]);
    configASSERT(ok == pdPASS);
}
