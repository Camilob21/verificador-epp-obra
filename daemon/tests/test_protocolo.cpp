// Pruebas de integracion C++ <-> biblioteca C del protocolo (common/protocolo, del firmware).
// Verifican que el daemon (C++17) arma exactamente los mismos bytes que el ESP32 espera:
// los vectores son los ejemplos de docs/protocolo_uart.md.
#include <gtest/gtest.h>

#include <vector>

#include "protocolo.h"

namespace {
std::vector<uint8_t> codificar(proto_trama_t t, uint8_t seq) {
  t.seq = seq;
  uint8_t buf[PROTO_MAX_TRAMA];
  const size_t n = proto_codificar(&t, buf, sizeof(buf));
  return std::vector<uint8_t>(buf, buf + n);
}
}  // namespace

TEST(Protocolo, VectorCrcConocido) {
  const uint8_t datos[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  EXPECT_EQ(0x29B1u, proto_crc16(datos, sizeof(datos)));
}

TEST(Protocolo, LatidoIgualAlDocumento) {
  proto_trama_t t;
  proto_msg_latido(&t);
  EXPECT_EQ(codificar(t, 0), (std::vector<uint8_t>{0xAA, 0x01, 0x00, 0x00, 0xAC, 0xFB}));
}

TEST(Protocolo, EstadoVerdeYRojoIgualAlDocumento) {
  proto_trama_t t;
  proto_msg_estado(&t, PROTO_ST_VERDE);
  EXPECT_EQ(codificar(t, 1), (std::vector<uint8_t>{0xAA, 0x02, 0x01, 0x01, 0x03, 0xCA, 0x5D}));
  proto_msg_estado(&t, PROTO_ST_ROJO);
  EXPECT_EQ(codificar(t, 2), (std::vector<uint8_t>{0xAA, 0x02, 0x02, 0x01, 0x04, 0x7D, 0x74}));
}

TEST(Protocolo, ParserEntregaTramaValidaYDescartaCorrupta) {
  struct Ctx {
    int n = 0;
    uint8_t tipo = 0;
  } ctx;
  proto_parser_t p;
  proto_parser_init(
      &p, [](const proto_trama_t* t, void* c) {
        static_cast<Ctx*>(c)->n++;
        static_cast<Ctx*>(c)->tipo = t->tipo;
      },
      &ctx);
  const uint8_t ack[] = {0xAA, 0x10, 0x01, 0x01, 0x02, 0x24, 0xBB};  // ACK de ESTADO seq=1 (documento)
  proto_parser_feed(&p, ack, sizeof(ack));
  EXPECT_EQ(ctx.n, 1);
  EXPECT_EQ(ctx.tipo, PROTO_ACK);
  uint8_t malo[sizeof(ack)];
  std::copy(ack, ack + sizeof(ack), malo);
  malo[4] ^= 0x01;  // un bit cambiado en los datos
  proto_parser_feed(&p, malo, sizeof(malo));
  EXPECT_EQ(ctx.n, 1);
  EXPECT_EQ(p.st.err_crc, 1u);
}
