/*
 * epp.h — Declaraciones compartidas entre los módulos del firmware.
 *
 * Módulos y tareas (ver README, sección "Arquitectura"):
 *   salidas.c    tarea "salidas"    LEDs, buzzer y relé (único dueño de esos GPIO)
 *   enlace.c     tarea "rx"         recepción UART + parser + atención de comandos
 *                tarea "enlace"     monitor del latido (fail-safe)
 *   entradas.c   tarea "entradas"   PIR y botón de factory reset
 *   telemetria.c tarea "telemetria" instrumentación periódica o bajo pedido
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "protocolo.h"

#define EPP_FW_MAYOR 1
#define EPP_FW_MENOR 0

/* ---------------- Tareas: prioridad y stack (bytes) ----------------
 * En ESP-IDF el tamaño del stack de xTaskCreate se da en BYTES.
 * Prioridades: mayor número = más prioridad. La tarea idle es 0 y el
 * temporizador de FreeRTOS de ESP-IDF usa 1 por defecto.
 * Justificación en el README. Los tamaños se verifican en ejecución con el
 * high-water mark que reporta la TELEMETRIA (campo stack_libre).          */
#define EPP_PRIO_ENLACE      6  /* seguridad: debe correr aunque todo lo demás esté ocupado */
#define EPP_PRIO_RX          5  /* no perder bytes del UART */
#define EPP_PRIO_SALIDAS     4  /* patrones de 10 ms */
#define EPP_PRIO_ENTRADAS    3  /* muestreo de 10 ms; tolera algo de retraso */
#define EPP_PRIO_TELEMETRIA  1  /* lo menos urgente */

#define EPP_STACK_ENLACE     3072
#define EPP_STACK_RX         4096
#define EPP_STACK_SALIDAS    3072
#define EPP_STACK_ENTRADAS   3072
#define EPP_STACK_TELEMETRIA 4096

/* Índices en la telemetría (mismo orden que proto_telemetria_t.stack_libre). */
typedef enum {
    EPP_T_RX = 0,
    EPP_T_ENLACE,
    EPP_T_SALIDAS,
    EPP_T_ENTRADAS,
    EPP_T_TELEMETRIA,
    EPP_T_N
} epp_tarea_t;

extern TaskHandle_t epp_tareas[EPP_T_N];

/* ---------------- salidas.c ---------------- */
void salidas_init_seguro(void);              /* lo PRIMERO en app_main: relé off, rojo on */
void salidas_iniciar_tarea(void);
void salidas_ordenar_estado(proto_estado_t e);
void salidas_buzzer(proto_buzzer_t patron);
void salidas_set_enlace(bool enlace_ok);     /* false = fail-safe */
void salidas_aviso_boton(uint8_t fase);      /* 0 = nada, 1 = manteniendo, 2 = confirmado */
void salidas_rele_apagar_ya(void);           /* apagado directo, desde cualquier tarea */
proto_estado_t salidas_estado_actual(void);  /* incluye PROTO_ST_SIN_ENLACE */
bool salidas_rele_activo(void);

/* ---------------- enlace.c ---------------- */
void enlace_init(void);
void enlace_iniciar_tareas(void);
bool enlace_ok(void);
/* Asigna número de secuencia, codifica y transmite. Seguro entre tareas.
 * Devuelve el número de secuencia usado. */
uint8_t enlace_enviar(proto_trama_t *t);

typedef struct {
    proto_stats_t parser;
    uint32_t err_uart;
    uint32_t fallos_enlace;
} enlace_stats_t;
void enlace_leer_stats(enlace_stats_t *s);

/* ---------------- entradas.c ---------------- */
void entradas_init(void);
void entradas_iniciar_tarea(void);
void entradas_boton_confirmado(void);        /* la Jetson envió ACK del EVENTO_BOTON */
bool entradas_pir_presencia(void);
bool entradas_pir_listo(void);
bool entradas_boton_presionado(void);

/* ---------------- telemetria.c ---------------- */
void telemetria_iniciar_tarea(void);
void telemetria_pedir(void);                 /* responder a PEDIR_TELEMETRIA */
