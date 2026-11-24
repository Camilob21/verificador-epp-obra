/*
 * protocolo.c — Implementación del protocolo UART Jetson <-> ESP32-C6.
 * C99 puro: sin malloc, sin printf, sin dependencias de plataforma.
 */
#include "protocolo.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Utilidades little-endian (serialización byte a byte: no depende del  */
/* endianness del procesador ni del empaquetado de structs)            */
/* ------------------------------------------------------------------ */
static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)(v >> 24);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* ------------------------------------------------------------------ */
/* CRC-16/CCITT-FALSE (bit a bit: lento pero pequeño y fácil de auditar; */
/* a 115200 baudios llegan ~11.5 kB/s, muy por debajo de su costo)       */
/* ------------------------------------------------------------------ */
uint16_t proto_crc16_actualizar(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)((uint16_t)byte << 8);
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000u) {
            crc = (uint16_t)(((uint32_t)crc << 1) ^ 0x1021u);
        } else {
            crc = (uint16_t)((uint32_t)crc << 1);
        }
    }
    return crc;
}

uint16_t proto_crc16(const uint8_t *datos, size_t n)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < n; i++) {
        crc = proto_crc16_actualizar(crc, datos[i]);
    }
    return crc;
}

/* ------------------------------------------------------------------ */
/* Codificación                                                        */
/* ------------------------------------------------------------------ */
size_t proto_codificar(const proto_trama_t *t, uint8_t *salida, size_t capacidad)
{
    if (t == NULL || salida == NULL || t->len > PROTO_MAX_DATOS) {
        return 0;
    }
    size_t total = PROTO_CABECERA + t->len + PROTO_TAM_CRC;
    if (capacidad < total) {
        return 0;
    }
    salida[0] = PROTO_SOF;
    salida[1] = t->tipo;
    salida[2] = t->seq;
    salida[3] = t->len;
    if (t->len > 0) {
        memcpy(&salida[4], t->datos, t->len);
    }
    uint16_t crc = proto_crc16(&salida[1], (size_t)3u + t->len); /* TIPO..DATOS */
    put_u16(&salida[4 + t->len], crc);
    return total;
}

/* Longitud esperada por tipo; -1 = tipo desconocido. */
static int len_esperada(uint8_t tipo)
{
    switch (tipo) {
    case PROTO_LATIDO:           return 0;
    case PROTO_ESTADO:           return 1;
    case PROTO_BUZZER:           return 1;
    case PROTO_PEDIR_TELEMETRIA: return 0;
    case PROTO_ACK:              return 1;
    case PROTO_NACK:             return 2;
    case PROTO_EVENTO_ARRANQUE:  return 3;
    case PROTO_EVENTO_PIR:       return 2;
    case PROTO_EVENTO_BOTON:     return 1;
    case PROTO_TELEMETRIA:       return (int)PROTO_TEL_LEN;
    case PROTO_EVENTO_ENLACE:    return 1;
    case PROTO_EVENTO_DISTANCIA: return 3;
    default:                     return -1;
    }
}

proto_nack_t proto_validar(const proto_trama_t *t)
{
    int esperada = len_esperada(t->tipo);
    if (esperada < 0) {
        return PROTO_NACK_TIPO_DESCONOCIDO;
    }
    if ((int)t->len != esperada) {
        return PROTO_NACK_LONGITUD;
    }
    return PROTO_NACK_OK;
}

static void iniciar(proto_trama_t *t, uint8_t tipo, uint8_t len)
{
    memset(t, 0, sizeof(*t));
    t->tipo = tipo;
    t->len = len;
}

void proto_msg_latido(proto_trama_t *t) { iniciar(t, PROTO_LATIDO, 0); }

void proto_msg_estado(proto_trama_t *t, proto_estado_t estado)
{
    iniciar(t, PROTO_ESTADO, 1);
    t->datos[0] = (uint8_t)estado;
}

void proto_msg_buzzer(proto_trama_t *t, proto_buzzer_t patron)
{
    iniciar(t, PROTO_BUZZER, 1);
    t->datos[0] = (uint8_t)patron;
}

void proto_msg_pedir_telemetria(proto_trama_t *t) { iniciar(t, PROTO_PEDIR_TELEMETRIA, 0); }

void proto_msg_ack(proto_trama_t *t, uint8_t seq, uint8_t tipo_confirmado)
{
    iniciar(t, PROTO_ACK, 1);
    t->seq = seq;
    t->datos[0] = tipo_confirmado;
}

void proto_msg_nack(proto_trama_t *t, uint8_t seq, uint8_t tipo_rechazado, proto_nack_t codigo)
{
    iniciar(t, PROTO_NACK, 2);
    t->seq = seq;
    t->datos[0] = tipo_rechazado;
    t->datos[1] = (uint8_t)codigo;
}

void proto_msg_arranque(proto_trama_t *t, uint8_t motivo_reset, uint8_t fw_mayor, uint8_t fw_menor)
{
    iniciar(t, PROTO_EVENTO_ARRANQUE, 3);
    t->datos[0] = motivo_reset;
    t->datos[1] = fw_mayor;
    t->datos[2] = fw_menor;
}

void proto_msg_evento_pir(proto_trama_t *t, bool presencia, bool listo)
{
    iniciar(t, PROTO_EVENTO_PIR, 2);
    t->datos[0] = presencia ? 1u : 0u;
    t->datos[1] = listo ? 1u : 0u;
}

void proto_msg_evento_boton(proto_trama_t *t)
{
    iniciar(t, PROTO_EVENTO_BOTON, 1);
    t->datos[0] = 1u;
}

void proto_msg_evento_enlace(proto_trama_t *t, bool restablecido)
{
    iniciar(t, PROTO_EVENTO_ENLACE, 1);
    t->datos[0] = restablecido ? 1u : 0u;
}

void proto_msg_evento_distancia(proto_trama_t *t, uint16_t distancia_mm, bool valida)
{
    iniciar(t, PROTO_EVENTO_DISTANCIA, 3);
    put_u16(&t->datos[0], distancia_mm);
    t->datos[2] = valida ? 1u : 0u;
}

void proto_msg_telemetria(proto_trama_t *t, const proto_telemetria_t *tel)
{
    iniciar(t, PROTO_TELEMETRIA, (uint8_t)PROTO_TEL_LEN);
    uint8_t *d = t->datos;
    d[0] = tel->version_proto;
    d[1] = tel->estado;
    d[2] = tel->banderas;
    d[3] = tel->cpu_pct;
    put_u32(&d[4], tel->uptime_s);
    put_u32(&d[8], tel->heap_libre);
    put_u32(&d[12], tel->heap_min);
    put_u32(&d[16], tel->tramas_ok);
    put_u32(&d[20], tel->err_crc);
    put_u16(&d[24], tel->err_longitud);
    put_u16(&d[26], tel->err_timeout);
    put_u16(&d[28], tel->err_uart);
    put_u16(&d[30], tel->fallos_enlace);
    for (unsigned i = 0; i < PROTO_TEL_N_TAREAS; i++) {
        put_u16(&d[32 + 2 * i], tel->stack_libre[i]);
    }
    /* 32 + 2*5 = 42 = PROTO_TEL_LEN */
}

/* ------------------------------------------------------------------ */
/* Lectores                                                            */
/* ------------------------------------------------------------------ */
static bool es(const proto_trama_t *t, uint8_t tipo)
{
    return t != NULL && t->tipo == tipo && proto_validar(t) == PROTO_NACK_OK;
}

bool proto_leer_estado(const proto_trama_t *t, proto_estado_t *estado)
{
    if (!es(t, PROTO_ESTADO) || t->datos[0] >= PROTO_ST_N_ORDENABLES) {
        return false;
    }
    *estado = (proto_estado_t)t->datos[0];
    return true;
}

bool proto_leer_buzzer(const proto_trama_t *t, proto_buzzer_t *patron)
{
    if (!es(t, PROTO_BUZZER) || t->datos[0] >= PROTO_BZ_N) {
        return false;
    }
    *patron = (proto_buzzer_t)t->datos[0];
    return true;
}

bool proto_leer_ack(const proto_trama_t *t, uint8_t *tipo_confirmado)
{
    if (!es(t, PROTO_ACK)) {
        return false;
    }
    *tipo_confirmado = t->datos[0];
    return true;
}

bool proto_leer_nack(const proto_trama_t *t, uint8_t *tipo_rechazado, uint8_t *codigo)
{
    if (!es(t, PROTO_NACK)) {
        return false;
    }
    *tipo_rechazado = t->datos[0];
    *codigo = t->datos[1];
    return true;
}

bool proto_leer_arranque(const proto_trama_t *t, uint8_t *motivo, uint8_t *fw_mayor, uint8_t *fw_menor)
{
    if (!es(t, PROTO_EVENTO_ARRANQUE)) {
        return false;
    }
    *motivo = t->datos[0];
    *fw_mayor = t->datos[1];
    *fw_menor = t->datos[2];
    return true;
}

bool proto_leer_evento_pir(const proto_trama_t *t, bool *presencia, bool *listo)
{
    if (!es(t, PROTO_EVENTO_PIR) || t->datos[0] > 1u || t->datos[1] > 1u) {
        return false;
    }
    *presencia = t->datos[0] != 0u;
    *listo = t->datos[1] != 0u;
    return true;
}

bool proto_leer_evento_boton(const proto_trama_t *t)
{
    return es(t, PROTO_EVENTO_BOTON) && t->datos[0] == 1u;
}

bool proto_leer_evento_enlace(const proto_trama_t *t, bool *restablecido)
{
    if (!es(t, PROTO_EVENTO_ENLACE) || t->datos[0] > 1u) {
        return false;
    }
    *restablecido = t->datos[0] != 0u;
    return true;
}

bool proto_leer_evento_distancia(const proto_trama_t *t, uint16_t *distancia_mm, bool *valida)
{
    if (!es(t, PROTO_EVENTO_DISTANCIA) || t->datos[2] > 1u) {
        return false;
    }
    *distancia_mm = get_u16(&t->datos[0]);
    *valida = t->datos[2] != 0u;
    return true;
}

bool proto_leer_telemetria(const proto_trama_t *t, proto_telemetria_t *tel)
{
    if (!es(t, PROTO_TELEMETRIA)) {
        return false;
    }
    const uint8_t *d = t->datos;
    tel->version_proto = d[0];
    tel->estado = d[1];
    tel->banderas = d[2];
    tel->cpu_pct = d[3];
    tel->uptime_s = get_u32(&d[4]);
    tel->heap_libre = get_u32(&d[8]);
    tel->heap_min = get_u32(&d[12]);
    tel->tramas_ok = get_u32(&d[16]);
    tel->err_crc = get_u32(&d[20]);
    tel->err_longitud = get_u16(&d[24]);
    tel->err_timeout = get_u16(&d[26]);
    tel->err_uart = get_u16(&d[28]);
    tel->fallos_enlace = get_u16(&d[30]);
    for (unsigned i = 0; i < PROTO_TEL_N_TAREAS; i++) {
        tel->stack_libre[i] = get_u16(&d[32 + 2 * i]);
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Parser                                                              */
/*                                                                     */
/* Estados implícitos según p->n (bytes acumulados en p->buf):         */
/*   n == 0          : BUSCANDO_SOF (se descartan bytes != 0xAA)       */
/*   1 <= n < 4      : LEYENDO_CABECERA (TIPO, SEQ, LEN)               */
/*   n == 4          : se valida LEN                                   */
/*   4 <= n < total  : LEYENDO_DATOS_Y_CRC                             */
/*   n == total      : se verifica el CRC                              */
/*                                                                     */
/* Resincronización: si LEN es inválida o el CRC falla, NO se descarta  */
/* todo lo acumulado: solo el 0xAA inicial. Los bytes siguientes se     */
/* vuelven a examinar, porque la trama verdadera puede empezar dentro   */
/* de la falsa (p. ej. basura "AA 01" seguida de una trama válida).     */
/* Se hace de forma iterativa con una cola interna de tamaño fijo, sin  */
/* recursión. Invariante: n + pendientes <= PROTO_MAX_TRAMA.            */
/* ------------------------------------------------------------------ */
void proto_parser_init(proto_parser_t *p, proto_cb_t cb, void *ctx)
{
    memset(p, 0, sizeof(*p));
    p->cb = cb;
    p->ctx = ctx;
}

bool proto_parser_en_curso(const proto_parser_t *p) { return p->n > 0; }

void proto_parser_timeout(proto_parser_t *p)
{
    if (p->n > 0) {
        p->st.err_timeout++;
        p->st.bytes_descartados += (uint32_t)p->n;
        p->n = 0;
    }
}

typedef enum { PASO_SIGUE, PASO_TRAMA, PASO_ERROR } paso_t;

/* Procesa un byte. Devuelve PASO_ERROR si la trama en curso resultó inválida
 * (el llamador se encarga de resincronizar). */
static paso_t paso(proto_parser_t *p, uint8_t b)
{
    if (p->n == 0) {
        if (b != PROTO_SOF) {
            p->st.bytes_descartados++;
            return PASO_SIGUE;
        }
        p->buf[p->n++] = b;
        return PASO_SIGUE;
    }

    p->buf[p->n++] = b;

    if (p->n == PROTO_CABECERA) {
        if (p->buf[3] > PROTO_MAX_DATOS) {
            p->st.err_longitud++;
            return PASO_ERROR;
        }
    }

    if (p->n >= PROTO_CABECERA) {
        size_t total = PROTO_CABECERA + p->buf[3] + PROTO_TAM_CRC;
        if (p->n == total) {
            size_t len = p->buf[3];
            uint16_t calc = proto_crc16(&p->buf[1], 3u + len);
            uint16_t recib = get_u16(&p->buf[PROTO_CABECERA + len]);
            if (calc != recib) {
                p->st.err_crc++;
                return PASO_ERROR;
            }
            proto_trama_t t;
            t.tipo = p->buf[1];
            t.seq = p->buf[2];
            t.len = (uint8_t)len;
            if (len > 0) {
                memcpy(t.datos, &p->buf[PROTO_CABECERA], len);
            }
            p->n = 0;
            p->st.tramas_ok++;
            if (p->cb != NULL) {
                p->cb(&t, p->ctx);
            }
            return PASO_TRAMA;
        }
    }
    return PASO_SIGUE;
}

void proto_parser_byte(proto_parser_t *p, uint8_t byte)
{
    size_t qi = 0, qn = 0;
    p->cola[qn++] = byte;

    while (qi < qn) {
        uint8_t b = p->cola[qi++];
        if (paso(p, b) != PASO_ERROR) {
            continue;
        }
        /* Resincronizar: descartar el SOF falso (buf[0]) y volver a examinar
         * buf[1..n-1] seguidos de lo que quedaba en la cola. */
        p->st.bytes_descartados++;
        size_t reexaminar = p->n - 1u;
        size_t pendientes = qn - qi;
        uint8_t tmp[PROTO_MAX_TRAMA];
        memcpy(tmp, &p->buf[1], reexaminar);
        memcpy(&tmp[reexaminar], &p->cola[qi], pendientes);
        qn = reexaminar + pendientes; /* <= PROTO_MAX_TRAMA - 1 por el invariante */
        memcpy(p->cola, tmp, qn);
        qi = 0;
        p->n = 0;
    }
}

void proto_parser_feed(proto_parser_t *p, const uint8_t *datos, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        proto_parser_byte(p, datos[i]);
    }
}
