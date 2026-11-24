/*
 * test_protocolo.c — Pruebas unitarias del protocolo con Unity.
 * Se compilan y ejecutan en el PC (gcc/clang), sin hardware:
 *   cmake -S common/protocolo -B build-proto && cmake --build build-proto
 *   ctest --test-dir build-proto --output-on-failure
 * Cada prueba indica en su nombre el ID de la matriz de verificación (PU-xx).
 */
#include <stdlib.h>
#include <string.h>

#include "protocolo.h"
#include "unity.h"

/* ---------- utilidades: capturar las tramas que entrega el parser ---------- */
#define MAX_CAPT 16
typedef struct {
    proto_trama_t t[MAX_CAPT];
    int n;
} captura_t;

static captura_t cap;
static proto_parser_t parser;

static void capturar(const proto_trama_t *t, void *ctx)
{
    captura_t *c = (captura_t *)ctx;
    if (c->n < MAX_CAPT) {
        c->t[c->n++] = *t;
    }
}

void setUp(void)
{
    memset(&cap, 0, sizeof(cap));
    proto_parser_init(&parser, capturar, &cap);
}

void tearDown(void) {}

static size_t trama_estado(uint8_t *buf, uint8_t seq, proto_estado_t e)
{
    proto_trama_t t;
    proto_msg_estado(&t, e);
    t.seq = seq;
    return proto_codificar(&t, buf, PROTO_MAX_TRAMA);
}

/* ============================ CRC ============================ */

/* PU-01: vector estándar de CRC-16/CCITT-FALSE. */
void test_PU01_crc_vector_123456789(void)
{
    const char *s = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x29B1, proto_crc16((const uint8_t *)s, strlen(s)));
}

/* PU-02: CRC de entrada vacía = valor inicial; un byte cambia el resultado. */
void test_PU02_crc_vacio_y_sensibilidad(void)
{
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, proto_crc16(NULL, 0));
    uint8_t a[] = {0x01, 0x02, 0x03};
    uint8_t b[] = {0x01, 0x02, 0x07};
    TEST_ASSERT_NOT_EQUAL(proto_crc16(a, 3), proto_crc16(b, 3));
}

/* ======================= Codificación ======================= */

/* PU-03: bytes exactos de una trama conocida (sirve de ejemplo en docs/). */
void test_PU03_codificar_latido_bytes_exactos(void)
{
    proto_trama_t t;
    proto_msg_latido(&t);
    t.seq = 0x05;
    uint8_t buf[PROTO_MAX_TRAMA];
    size_t n = proto_codificar(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT(6, n);
    uint8_t cuerpo[] = {0x01, 0x05, 0x00};
    uint16_t crc = proto_crc16(cuerpo, 3);
    uint8_t esperado[] = {0xAA, 0x01, 0x05, 0x00, (uint8_t)(crc & 0xFF), (uint8_t)(crc >> 8)};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(esperado, buf, 6);
}

/* PU-04: codificar rechaza LEN > máximo y buffer insuficiente. */
void test_PU04_codificar_rechaza_parametros_invalidos(void)
{
    proto_trama_t t;
    proto_msg_latido(&t);
    uint8_t buf[PROTO_MAX_TRAMA];
    t.len = PROTO_MAX_DATOS + 1;
    TEST_ASSERT_EQUAL_UINT(0, proto_codificar(&t, buf, sizeof(buf)));
    t.len = 0;
    TEST_ASSERT_EQUAL_UINT(0, proto_codificar(&t, buf, 5)); /* necesita 6 */
}

/* PU-05: ida y vuelta de todos los mensajes, incluida la telemetría. */
void test_PU05_ida_y_vuelta_todos_los_mensajes(void)
{
    proto_trama_t m[11];
    proto_msg_latido(&m[0]);
    proto_msg_estado(&m[1], PROTO_ST_VERDE);
    proto_msg_buzzer(&m[2], PROTO_BZ_ERROR);
    proto_msg_pedir_telemetria(&m[3]);
    proto_msg_ack(&m[4], 77, PROTO_ESTADO);
    proto_msg_nack(&m[5], 78, PROTO_BUZZER, PROTO_NACK_VALOR);
    proto_msg_arranque(&m[6], 3, 1, 0);
    proto_msg_evento_pir(&m[7], true, true);
    proto_msg_evento_boton(&m[8]);
    proto_msg_evento_enlace(&m[9], false);
    proto_telemetria_t tel = {
        .version_proto = PROTO_VERSION, .estado = PROTO_ST_SIN_ENLACE,
        .banderas = PROTO_TEL_RELE | PROTO_TEL_PIR_LISTO, .cpu_pct = 7,
        .uptime_s = 0x01020304u, .heap_libre = 300000u, .heap_min = 250000u,
        .tramas_ok = 0xA1B2C3D4u, .err_crc = 9, .err_longitud = 2, .err_timeout = 3,
        .err_uart = 4, .fallos_enlace = 5, .stack_libre = {1000, 1100, 1200, 1300, 65535}};
    proto_msg_telemetria(&m[10], &tel);

    uint8_t flujo[11 * PROTO_MAX_TRAMA];
    size_t total = 0;
    for (int i = 0; i < 11; i++) {
        if (m[i].tipo != PROTO_ACK && m[i].tipo != PROTO_NACK) {
            m[i].seq = (uint8_t)(200 + i);
        }
        TEST_ASSERT_EQUAL(PROTO_NACK_OK, proto_validar(&m[i]));
        size_t n = proto_codificar(&m[i], &flujo[total], sizeof(flujo) - total);
        TEST_ASSERT_NOT_EQUAL(0, n);
        total += n;
    }
    proto_parser_feed(&parser, flujo, total);

    TEST_ASSERT_EQUAL_INT(11, cap.n);
    TEST_ASSERT_EQUAL_UINT32(11, parser.st.tramas_ok);
    TEST_ASSERT_EQUAL_UINT32(0, parser.st.err_crc);
    TEST_ASSERT_EQUAL_UINT32(0, parser.st.bytes_descartados);
    for (int i = 0; i < 11; i++) {
        TEST_ASSERT_EQUAL_UINT8(m[i].tipo, cap.t[i].tipo);
        TEST_ASSERT_EQUAL_UINT8(m[i].seq, cap.t[i].seq);
        TEST_ASSERT_EQUAL_UINT8(m[i].len, cap.t[i].len);
        if (m[i].len > 0) { /* Unity no admite comparar 0 bytes */
            TEST_ASSERT_EQUAL_MEMORY(m[i].datos, cap.t[i].datos, m[i].len);
        }
    }

    proto_estado_t e;
    TEST_ASSERT_TRUE(proto_leer_estado(&cap.t[1], &e));
    TEST_ASSERT_EQUAL(PROTO_ST_VERDE, e);
    uint8_t tipo, cod;
    TEST_ASSERT_TRUE(proto_leer_nack(&cap.t[5], &tipo, &cod));
    TEST_ASSERT_EQUAL_UINT8(PROTO_BUZZER, tipo);
    TEST_ASSERT_EQUAL_UINT8(PROTO_NACK_VALOR, cod);
    bool pres, listo;
    TEST_ASSERT_TRUE(proto_leer_evento_pir(&cap.t[7], &pres, &listo));
    TEST_ASSERT_TRUE(pres && listo);
    TEST_ASSERT_TRUE(proto_leer_evento_boton(&cap.t[8]));

    proto_telemetria_t tel2;
    TEST_ASSERT_TRUE(proto_leer_telemetria(&cap.t[10], &tel2));
    TEST_ASSERT_EQUAL_UINT32(tel.uptime_s, tel2.uptime_s);
    TEST_ASSERT_EQUAL_UINT32(tel.tramas_ok, tel2.tramas_ok);
    TEST_ASSERT_EQUAL_UINT32(tel.heap_min, tel2.heap_min);
    TEST_ASSERT_EQUAL_UINT16(tel.fallos_enlace, tel2.fallos_enlace);
    TEST_ASSERT_EQUAL_UINT16_ARRAY(tel.stack_libre, tel2.stack_libre, PROTO_TEL_N_TAREAS);
    TEST_ASSERT_EQUAL_UINT8(tel.banderas, tel2.banderas);
}

/* PU-06: la telemetría usa little-endian en el cable (independiente de la CPU). */
void test_PU06_telemetria_little_endian(void)
{
    proto_telemetria_t tel;
    memset(&tel, 0, sizeof(tel));
    tel.uptime_s = 0x11223344u;
    proto_trama_t t;
    proto_msg_telemetria(&t, &tel);
    TEST_ASSERT_EQUAL_UINT8(PROTO_TEL_LEN, t.len);
    uint8_t esperado[] = {0x44, 0x33, 0x22, 0x11};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(esperado, &t.datos[4], 4);
}

/* ======================== Parser ======================== */

/* PU-07: trama con un bit cambiado -> rechazada y contada como error de CRC. */
void test_PU07_trama_corrupta_rechazada(void)
{
    uint8_t buf[PROTO_MAX_TRAMA];
    size_t n = trama_estado(buf, 1, PROTO_ST_ROJO);
    buf[4] ^= 0x01; /* dato corrupto */
    proto_parser_feed(&parser, buf, n);
    TEST_ASSERT_EQUAL_INT(0, cap.n);
    TEST_ASSERT_EQUAL_UINT32(1, parser.st.err_crc);
    TEST_ASSERT_FALSE(proto_parser_en_curso(&parser));
}

/* PU-08: CRC corrupto -> rechazada; la siguiente trama válida sí se acepta. */
void test_PU08_crc_corrupto_y_recuperacion(void)
{
    uint8_t buf[2 * PROTO_MAX_TRAMA];
    size_t n1 = trama_estado(buf, 1, PROTO_ST_ROJO);
    buf[n1 - 1] ^= 0xFF;
    size_t n2 = trama_estado(&buf[n1], 2, PROTO_ST_VERDE);
    proto_parser_feed(&parser, buf, n1 + n2);
    TEST_ASSERT_EQUAL_INT(1, cap.n);
    TEST_ASSERT_EQUAL_UINT8(2, cap.t[0].seq);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, parser.st.err_crc);
}

/* PU-09: trama incompleta -> no se entrega; queda en curso; timeout la descarta. */
void test_PU09_trama_incompleta_y_timeout(void)
{
    uint8_t buf[PROTO_MAX_TRAMA];
    size_t n = trama_estado(buf, 9, PROTO_ST_ROJO);
    proto_parser_feed(&parser, buf, n - 2);
    TEST_ASSERT_EQUAL_INT(0, cap.n);
    TEST_ASSERT_TRUE(proto_parser_en_curso(&parser));
    proto_parser_timeout(&parser);
    TEST_ASSERT_FALSE(proto_parser_en_curso(&parser));
    TEST_ASSERT_EQUAL_UINT32(1, parser.st.err_timeout);
    /* después del timeout, una trama completa se acepta */
    proto_parser_feed(&parser, buf, n);
    TEST_ASSERT_EQUAL_INT(1, cap.n);
}

/* PU-10: basura sin 0xAA antes de una trama válida. */
void test_PU10_basura_simple_antes_de_trama(void)
{
    uint8_t buf[10 + PROTO_MAX_TRAMA] = {0x00, 0x13, 0xFF, 0x55, 0x01, 0x02, 0x7E, 0x80, 0x10, 0x11};
    size_t n = trama_estado(&buf[10], 3, PROTO_ST_AMARILLO_FIJO);
    proto_parser_feed(&parser, buf, 10 + n);
    TEST_ASSERT_EQUAL_INT(1, cap.n);
    TEST_ASSERT_EQUAL_UINT8(PROTO_ESTADO, cap.t[0].tipo);
    TEST_ASSERT_EQUAL_UINT32(10, parser.st.bytes_descartados);
}

/* PU-11: basura que contiene 0xAA (falso inicio) justo antes de la trama.
 * Sin resincronización, el SOF falso "se come" la trama real. */
void test_PU11_falso_sof_resincroniza(void)
{
    uint8_t buf[3 + PROTO_MAX_TRAMA] = {0xAA, 0x02, 0x07};
    size_t n = trama_estado(&buf[3], 4, PROTO_ST_VERDE);
    proto_parser_feed(&parser, buf, 3 + n);
    TEST_ASSERT_EQUAL_INT(1, cap.n);
    TEST_ASSERT_EQUAL_UINT8(4, cap.t[0].seq);
    proto_estado_t e;
    TEST_ASSERT_TRUE(proto_leer_estado(&cap.t[0], &e));
    TEST_ASSERT_EQUAL(PROTO_ST_VERDE, e);
}

/* PU-12: LEN inválida (> 48) -> error de longitud y resincronización. */
void test_PU12_longitud_invalida(void)
{
    uint8_t buf[4 + PROTO_MAX_TRAMA] = {0xAA, 0x02, 0x00, 0xC8}; /* LEN = 200 */
    size_t n = trama_estado(&buf[4], 5, PROTO_ST_REPOSO);
    proto_parser_feed(&parser, buf, 4 + n);
    TEST_ASSERT_EQUAL_UINT32(1, parser.st.err_longitud);
    TEST_ASSERT_EQUAL_INT(1, cap.n);
    TEST_ASSERT_EQUAL_UINT8(5, cap.t[0].seq);
}

/* PU-13: bytes entregados de a uno (como llegan por UART) y tramas pegadas. */
void test_PU13_byte_a_byte_y_tramas_consecutivas(void)
{
    uint8_t buf[3 * PROTO_MAX_TRAMA];
    size_t n = 0;
    for (uint8_t i = 0; i < 3; i++) {
        n += trama_estado(&buf[n], i, (proto_estado_t)i);
    }
    for (size_t i = 0; i < n; i++) {
        proto_parser_byte(&parser, buf[i]);
    }
    TEST_ASSERT_EQUAL_INT(3, cap.n);
    for (uint8_t i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_UINT8(i, cap.t[i].seq);
    }
}

/* PU-14: el byte 0xAA dentro de los datos o del CRC no confunde al parser. */
void test_PU14_sof_dentro_de_los_datos(void)
{
    proto_trama_t t;
    proto_msg_ack(&t, 0xAA, 0xAA); /* SEQ y dato = 0xAA */
    uint8_t buf[PROTO_MAX_TRAMA];
    size_t n = proto_codificar(&t, buf, sizeof(buf));
    proto_parser_feed(&parser, buf, n);
    TEST_ASSERT_EQUAL_INT(1, cap.n);
    TEST_ASSERT_EQUAL_UINT8(0xAA, cap.t[0].seq);
    TEST_ASSERT_EQUAL_UINT8(0xAA, cap.t[0].datos[0]);
}

/* PU-15: validación semántica: tipo desconocido, longitud y valor fuera de rango. */
void test_PU15_validacion_semantica(void)
{
    proto_trama_t t;
    memset(&t, 0, sizeof(t));
    t.tipo = 0x55;
    TEST_ASSERT_EQUAL(PROTO_NACK_TIPO_DESCONOCIDO, proto_validar(&t));
    proto_msg_estado(&t, PROTO_ST_ROJO);
    t.len = 2;
    TEST_ASSERT_EQUAL(PROTO_NACK_LONGITUD, proto_validar(&t));
    proto_msg_estado(&t, PROTO_ST_ROJO);
    t.datos[0] = 9; /* estado inexistente */
    proto_estado_t e;
    TEST_ASSERT_FALSE(proto_leer_estado(&t, &e));
    t.datos[0] = PROTO_ST_SIN_ENLACE; /* no se puede ordenar */
    TEST_ASSERT_FALSE(proto_leer_estado(&t, &e));
    proto_msg_buzzer(&t, PROTO_BZ_N);
    proto_buzzer_t b;
    TEST_ASSERT_FALSE(proto_leer_buzzer(&t, &b));
}

/* PU-16: robustez — 200 000 bytes aleatorios con tramas válidas intercaladas.
 * No debe fallar (se corre bajo valgrind / sanitizers) y todas las tramas
 * válidas insertadas deben recuperarse. */
void test_PU16_robustez_flujo_aleatorio(void)
{
    srand(12345);
    int insertadas = 0;
    uint8_t trama[PROTO_MAX_TRAMA];
    for (int i = 0; i < 200000; i++) {
        uint8_t b = (uint8_t)(rand() & 0xFF);
        if (b == PROTO_SOF) {
            b = 0x00; /* la basura no contiene 0xAA... */
        }
        proto_parser_byte(&parser, b);
        if (i % 5000 == 0) {
            proto_parser_timeout(&parser); /* ...y hay silencio antes de cada trama */
            size_t n = trama_estado(trama, (uint8_t)insertadas, PROTO_ST_ROJO);
            proto_parser_feed(&parser, trama, n);
            insertadas++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32((uint32_t)insertadas, parser.st.tramas_ok);
}

/* PU-17: robustez con 0xAA en la basura: no se pierde memoria ni se cae, y el
 * contador de tramas válidas nunca supera lo plausible. */
void test_PU17_robustez_basura_con_sof(void)
{
    srand(999);
    for (int i = 0; i < 200000; i++) {
        proto_parser_byte(&parser, (uint8_t)(rand() & 0xFF));
    }
    /* Una trama "válida" aleatoria requiere acertar un CRC de 16 bits:
     * se toleran unas pocas, nunca cientos. */
    TEST_ASSERT_LESS_THAN_UINT32(50, parser.st.tramas_ok);
    TEST_ASSERT_TRUE(parser.n < PROTO_MAX_TRAMA);
}

/* PU-18: vectores "dorados" publicados en docs/protocolo_uart.md. Si alguien
 * cambia el formato, esta prueba avisa que la documentación quedó vieja. */
void test_PU18_vectores_documentados(void)
{
    uint8_t buf[PROTO_MAX_TRAMA];
    proto_trama_t t;

    proto_msg_latido(&t);
    t.seq = 0;
    const uint8_t latido[] = {0xAA, 0x01, 0x00, 0x00, 0xAC, 0xFB};
    TEST_ASSERT_EQUAL_UINT(sizeof(latido), proto_codificar(&t, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(latido, buf, sizeof(latido));

    proto_msg_estado(&t, PROTO_ST_VERDE);
    t.seq = 1;
    const uint8_t verde[] = {0xAA, 0x02, 0x01, 0x01, 0x03, 0xCA, 0x5D};
    TEST_ASSERT_EQUAL_UINT(sizeof(verde), proto_codificar(&t, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(verde, buf, sizeof(verde));

    proto_msg_nack(&t, 1, PROTO_ESTADO, PROTO_NACK_SIN_ENLACE);
    const uint8_t nack[] = {0xAA, 0x11, 0x01, 0x02, 0x02, 0x04, 0x35, 0x81};
    TEST_ASSERT_EQUAL_UINT(sizeof(nack), proto_codificar(&t, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(nack, buf, sizeof(nack));
}

/* PU-19: EVENTO_DISTANCIA (HC-SR04): ida y vuelta, little-endian, longitud y bandera invalidas. */
void test_PU19_evento_distancia(void)
{
    uint8_t buf[PROTO_MAX_TRAMA];
    proto_trama_t t, r;
    proto_msg_evento_distancia(&t, 1625, true); /* 1,625 m = 0x0659 */
    t.seq = 7;
    size_t n = proto_codificar(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT(4u + 3u + 2u, n);
    TEST_ASSERT_EQUAL_HEX8(PROTO_EVENTO_DISTANCIA, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x59, buf[4]); /* byte bajo primero */
    TEST_ASSERT_EQUAL_HEX8(0x06, buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[6]);

    uint16_t mm = 0;
    bool valida = false;
    TEST_ASSERT_TRUE(proto_leer_evento_distancia(&t, &mm, &valida));
    TEST_ASSERT_EQUAL_UINT16(1625, mm);
    TEST_ASSERT_TRUE(valida);

    proto_msg_evento_distancia(&r, 0, false);
    TEST_ASSERT_TRUE(proto_leer_evento_distancia(&r, &mm, &valida));
    TEST_ASSERT_FALSE(valida);

    r.datos[2] = 2; /* bandera fuera de rango */
    TEST_ASSERT_FALSE(proto_leer_evento_distancia(&r, &mm, &valida));
    r.datos[2] = 1;
    r.len = 2; /* longitud incorrecta */
    TEST_ASSERT_EQUAL_INT(PROTO_NACK_LONGITUD, proto_validar(&r));
    TEST_ASSERT_FALSE(proto_leer_evento_distancia(&r, &mm, &valida));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_PU01_crc_vector_123456789);
    RUN_TEST(test_PU02_crc_vacio_y_sensibilidad);
    RUN_TEST(test_PU03_codificar_latido_bytes_exactos);
    RUN_TEST(test_PU04_codificar_rechaza_parametros_invalidos);
    RUN_TEST(test_PU05_ida_y_vuelta_todos_los_mensajes);
    RUN_TEST(test_PU06_telemetria_little_endian);
    RUN_TEST(test_PU07_trama_corrupta_rechazada);
    RUN_TEST(test_PU08_crc_corrupto_y_recuperacion);
    RUN_TEST(test_PU09_trama_incompleta_y_timeout);
    RUN_TEST(test_PU10_basura_simple_antes_de_trama);
    RUN_TEST(test_PU11_falso_sof_resincroniza);
    RUN_TEST(test_PU12_longitud_invalida);
    RUN_TEST(test_PU13_byte_a_byte_y_tramas_consecutivas);
    RUN_TEST(test_PU14_sof_dentro_de_los_datos);
    RUN_TEST(test_PU15_validacion_semantica);
    RUN_TEST(test_PU16_robustez_flujo_aleatorio);
    RUN_TEST(test_PU17_robustez_basura_con_sof);
    RUN_TEST(test_PU18_vectores_documentados);
    RUN_TEST(test_PU19_evento_distancia);
    return UNITY_END();
}
