/*
 * protocolo.h — Protocolo UART Jetson Nano <-> ESP32-C6 (proyecto EPP)
 *
 * C99 puro, sin dependencias de ESP-IDF ni de Linux: se compila igual en el
 * firmware (ESP-IDF), en el daemon C++17 de la Jetson y en el PC para las
 * pruebas unitarias. Documentación completa: docs/protocolo_uart.md
 *
 * Formato de trama (todos los campos multibyte en little-endian, incluido el CRC):
 *
 *   +------+------+------+------+-----------------+--------+--------+
 *   | 0xAA | TIPO | SEQ  | LEN  | DATOS (0..48 B) | CRC lo | CRC hi |
 *   +------+------+------+------+-----------------+--------+--------+
 *            \__________ cubierto por el CRC _________/
 *
 *   CRC-16/CCITT-FALSE: polinomio 0x1021, valor inicial 0xFFFF, sin reflexión,
 *   sin XOR final. Vector de verificación: "123456789" -> 0x29B1.
 */
#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Constantes de la trama                                              */
/* ------------------------------------------------------------------ */
#define PROTO_VERSION      1u     /* se reporta en EVENTO_ARRANQUE y TELEMETRIA */
#define PROTO_SOF          0xAAu  /* byte de inicio de trama */
#define PROTO_MAX_DATOS    48u    /* 32 no alcanza para la telemetría (42 B) */
#define PROTO_CABECERA     4u     /* SOF + TIPO + SEQ + LEN */
#define PROTO_TAM_CRC      2u
#define PROTO_MAX_TRAMA    (PROTO_CABECERA + PROTO_MAX_DATOS + PROTO_TAM_CRC) /* 54 */

/* ------------------------------------------------------------------ */
/* Tipos de mensaje                                                    */
/* ------------------------------------------------------------------ */
typedef enum {
    /* Jetson -> ESP32 (0x01..0x0F) */
    PROTO_LATIDO           = 0x01, /* LEN 0. Cada ~200 ms. Sin ACK. */
    PROTO_ESTADO           = 0x02, /* LEN 1: proto_estado_t. Responde ACK/NACK. */
    PROTO_BUZZER           = 0x03, /* LEN 1: proto_buzzer_t. Responde ACK/NACK. */
    PROTO_PEDIR_TELEMETRIA = 0x04, /* LEN 0. Responde TELEMETRIA. */

    /* Bidireccionales (0x10..0x1F) */
    PROTO_ACK              = 0x10, /* LEN 1: tipo confirmado. SEQ = SEQ de la trama confirmada. */
    PROTO_NACK             = 0x11, /* LEN 2: tipo rechazado, proto_nack_t. SEQ = SEQ rechazada. */

    /* ESP32 -> Jetson (0x80..0x8F) */
    PROTO_EVENTO_ARRANQUE  = 0x80, /* LEN 3: motivo de reinicio, versión fw mayor, menor. */
    PROTO_EVENTO_PIR       = 0x81, /* LEN 2: presencia (0/1), PIR listo (0/1). */
    PROTO_EVENTO_BOTON     = 0x82, /* LEN 1: 1 = factory reset solicitado. Espera ACK. */
    PROTO_TELEMETRIA       = 0x83, /* LEN 42: ver proto_telemetria_t. */
    PROTO_EVENTO_ENLACE    = 0x84, /* LEN 1: 0 = latido perdido (fail-safe), 1 = restablecido. */
} proto_tipo_t;

/* Estados del semáforo que la Jetson puede ordenar (mensaje ESTADO). */
typedef enum {
    PROTO_ST_REPOSO            = 0, /* todo apagado, relé apagado */
    PROTO_ST_AMARILLO_PARPADEO = 1, /* colóquese en la marca */
    PROTO_ST_AMARILLO_FIJO     = 2, /* verificando */
    PROTO_ST_VERDE             = 3, /* cumple: verde + relé activado */
    PROTO_ST_ROJO              = 4, /* no cumple */
    PROTO_ST_N_ORDENABLES      = 5,
    PROTO_ST_SIN_ENLACE        = 5  /* solo lo reporta el ESP32 en TELEMETRIA */
} proto_estado_t;

/* Patrones de buzzer (mensaje BUZZER). */
typedef enum {
    PROTO_BZ_SILENCIO  = 0,
    PROTO_BZ_BIP_CORTO = 1,
    PROTO_BZ_BIP_DOBLE = 2,
    PROTO_BZ_OK        = 3, /* un tono largo */
    PROTO_BZ_ERROR     = 4, /* tres tonos */
    PROTO_BZ_N
} proto_buzzer_t;

/* Códigos de NACK. */
typedef enum {
    PROTO_NACK_OK               = 0, /* no es NACK: usado por proto_validar() */
    PROTO_NACK_TIPO_DESCONOCIDO = 1,
    PROTO_NACK_LONGITUD         = 2,
    PROTO_NACK_VALOR            = 3,
    PROTO_NACK_SIN_ENLACE       = 4  /* ESTADO recibido antes del primer latido válido */
} proto_nack_t;

/* Bits del campo "banderas" de la telemetría. */
#define PROTO_TEL_ENLACE     (1u << 0)
#define PROTO_TEL_RELE       (1u << 1)
#define PROTO_TEL_PIR        (1u << 2)
#define PROTO_TEL_PIR_LISTO  (1u << 3)
#define PROTO_TEL_BOTON      (1u << 4)

#define PROTO_TEL_N_TAREAS   5u
#define PROTO_TEL_LEN        42u
#define PROTO_CPU_NO_DISPONIBLE 0xFFu

/* ------------------------------------------------------------------ */
/* Trama decodificada                                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    uint8_t tipo;
    uint8_t seq;
    uint8_t len;
    uint8_t datos[PROTO_MAX_DATOS];
} proto_trama_t;

/* Telemetría (ESP32 -> Jetson). Orden en el cable: ver docs/protocolo_uart.md */
typedef struct {
    uint8_t  version_proto;
    uint8_t  estado;          /* proto_estado_t (incluye SIN_ENLACE) */
    uint8_t  banderas;        /* PROTO_TEL_* */
    uint8_t  cpu_pct;         /* 0..100, o PROTO_CPU_NO_DISPONIBLE */
    uint32_t uptime_s;
    uint32_t heap_libre;
    uint32_t heap_min;
    uint32_t tramas_ok;
    uint32_t err_crc;
    uint16_t err_longitud;
    uint16_t err_timeout;
    uint16_t err_uart;        /* desbordes de FIFO/buffer, errores de framing/paridad */
    uint16_t fallos_enlace;   /* veces que entró en fail-safe */
    uint16_t stack_libre[PROTO_TEL_N_TAREAS]; /* high-water mark en bytes:
                                                 rx, enlace, salidas, entradas, telemetría */
} proto_telemetria_t;

/* ------------------------------------------------------------------ */
/* CRC                                                                 */
/* ------------------------------------------------------------------ */
uint16_t proto_crc16(const uint8_t *datos, size_t n);
uint16_t proto_crc16_actualizar(uint16_t crc, uint8_t byte);

/* ------------------------------------------------------------------ */
/* Codificación                                                        */
/* ------------------------------------------------------------------ */
/* Escribe la trama completa en 'salida'. Devuelve el número de bytes
 * escritos, o 0 si t->len > PROTO_MAX_DATOS o 'capacidad' no alcanza. */
size_t proto_codificar(const proto_trama_t *t, uint8_t *salida, size_t capacidad);

/* Devuelve PROTO_NACK_OK si el tipo es conocido y LEN es la esperada para
 * ese tipo; si no, el código de NACK que corresponde. No revisa rangos de
 * valores (eso lo hacen las funciones proto_leer_*). */
proto_nack_t proto_validar(const proto_trama_t *t);

/* Constructores de mensajes. Dejan seq = 0 salvo ACK/NACK: el emisor
 * asigna su propio contador de secuencia antes de codificar. */
void proto_msg_latido(proto_trama_t *t);
void proto_msg_estado(proto_trama_t *t, proto_estado_t estado);
void proto_msg_buzzer(proto_trama_t *t, proto_buzzer_t patron);
void proto_msg_pedir_telemetria(proto_trama_t *t);
void proto_msg_ack(proto_trama_t *t, uint8_t seq, uint8_t tipo_confirmado);
void proto_msg_nack(proto_trama_t *t, uint8_t seq, uint8_t tipo_rechazado, proto_nack_t codigo);
void proto_msg_arranque(proto_trama_t *t, uint8_t motivo_reset, uint8_t fw_mayor, uint8_t fw_menor);
void proto_msg_evento_pir(proto_trama_t *t, bool presencia, bool listo);
void proto_msg_evento_boton(proto_trama_t *t);
void proto_msg_evento_enlace(proto_trama_t *t, bool restablecido);
void proto_msg_telemetria(proto_trama_t *t, const proto_telemetria_t *tel);

/* Lectores: devuelven false si el tipo, la longitud o el valor no son válidos. */
bool proto_leer_estado(const proto_trama_t *t, proto_estado_t *estado);
bool proto_leer_buzzer(const proto_trama_t *t, proto_buzzer_t *patron);
bool proto_leer_ack(const proto_trama_t *t, uint8_t *tipo_confirmado);
bool proto_leer_nack(const proto_trama_t *t, uint8_t *tipo_rechazado, uint8_t *codigo);
bool proto_leer_arranque(const proto_trama_t *t, uint8_t *motivo, uint8_t *fw_mayor, uint8_t *fw_menor);
bool proto_leer_evento_pir(const proto_trama_t *t, bool *presencia, bool *listo);
bool proto_leer_evento_boton(const proto_trama_t *t);
bool proto_leer_evento_enlace(const proto_trama_t *t, bool *restablecido);
bool proto_leer_telemetria(const proto_trama_t *t, proto_telemetria_t *tel);

/* ------------------------------------------------------------------ */
/* Parser (máquina de estados con resincronización)                    */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t tramas_ok;
    uint32_t err_crc;
    uint32_t err_longitud;      /* LEN > PROTO_MAX_DATOS */
    uint32_t err_timeout;       /* trama incompleta descartada por silencio */
    uint32_t bytes_descartados; /* bytes que no formaban parte de ninguna trama válida */
} proto_stats_t;

typedef void (*proto_cb_t)(const proto_trama_t *t, void *ctx);

typedef struct {
    uint8_t       buf[PROTO_MAX_TRAMA]; /* bytes de la trama en curso */
    size_t        n;                    /* bytes acumulados en buf */
    uint8_t       cola[PROTO_MAX_TRAMA];/* bytes pendientes de re-examinar */
    proto_stats_t st;
    proto_cb_t    cb;
    void         *ctx;
} proto_parser_t;

/* 'cb' se llama una vez por cada trama válida (CRC correcto). Puede ser NULL. */
void proto_parser_init(proto_parser_t *p, proto_cb_t cb, void *ctx);

/* Entrega bytes recibidos al parser. Pueden llegar de a uno o en bloques. */
void proto_parser_byte(proto_parser_t *p, uint8_t byte);
void proto_parser_feed(proto_parser_t *p, const uint8_t *datos, size_t n);

/* true si hay una trama a medio recibir. */
bool proto_parser_en_curso(const proto_parser_t *p);

/* Llamar cuando la línea lleva demasiado tiempo en silencio con una trama a
 * medias (el firmware usa 20 ms). Descarta lo acumulado y cuenta err_timeout. */
void proto_parser_timeout(proto_parser_t *p);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOLO_H */
