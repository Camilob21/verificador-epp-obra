// Pruebas del enlace Jetson <-> ESP32 con un "ESP32 falso" que usa la misma biblioteca del firmware.
#include <gtest/gtest.h>

#include <vector>

#include "epp/enlace.hpp"

using namespace epp;

namespace {
// Decodifica todas las tramas de un bloque de bytes (lo que "recibiria" el ESP32)
std::vector<proto_trama_t> tramas(const std::vector<uint8_t>& bytes) {
  std::vector<proto_trama_t> out;
  proto_parser_t p;
  proto_parser_init(
      &p, [](const proto_trama_t* t, void* c) { static_cast<std::vector<proto_trama_t>*>(c)->push_back(*t); }, &out);
  proto_parser_feed(&p, bytes.data(), bytes.size());
  return out;
}
int contar(const std::vector<proto_trama_t>& ts, uint8_t tipo) {
  int n = 0;
  for (const auto& t : ts) n += (t.tipo == tipo);
  return n;
}
const proto_trama_t* buscar(const std::vector<proto_trama_t>& ts, uint8_t tipo) {
  for (const auto& t : ts)
    if (t.tipo == tipo) return &t;
  return nullptr;
}
// Bytes que "enviaria" el ESP32
std::vector<uint8_t> bytes(proto_trama_t t, uint8_t seq = 0) {
  if (t.tipo != PROTO_ACK && t.tipo != PROTO_NACK) t.seq = seq;
  uint8_t buf[PROTO_MAX_TRAMA];
  const size_t n = proto_codificar(&t, buf, sizeof(buf));
  return std::vector<uint8_t>(buf, buf + n);
}
}  // namespace

TEST(Enlace, MapeoDeLucesAlSemaforoDelEsp32) {
  EXPECT_EQ(estadoDeLuz(Luz::APAGADO), PROTO_ST_REPOSO);
  EXPECT_EQ(estadoDeLuz(Luz::AMARILLO_PARPADEO), PROTO_ST_AMARILLO_PARPADEO);
  EXPECT_EQ(estadoDeLuz(Luz::AMARILLO_FIJO), PROTO_ST_AMARILLO_FIJO);
  EXPECT_EQ(estadoDeLuz(Luz::VERDE), PROTO_ST_VERDE);
  EXPECT_EQ(estadoDeLuz(Luz::ROJO), PROTO_ST_ROJO);
}

TEST(Enlace, LatidoCada200msNiMasNiMenos) {
  EnlaceEsp32 e(1000.0);
  int latidos = 0;
  for (int i = 0; i <= 100; ++i) latidos += contar(tramas(e.tick(i * 0.010)), PROTO_LATIDO);  // 1 s, ticks de 10 ms
  EXPECT_EQ(latidos, 6);  // t = 0; 0,2; 0,4; 0,6; 0,8; 1,0
}

TEST(Enlace, ElLatidoSaleAntesQueElEstado) {
  EnlaceEsp32 e(1000.0);
  e.ordenarEstado(PROTO_ST_ROJO);
  const auto ts = tramas(e.tick(0.0));
  ASSERT_GE(ts.size(), 2u);
  EXPECT_EQ(ts[0].tipo, PROTO_LATIDO);
  EXPECT_EQ(ts[1].tipo, PROTO_ESTADO);
}

TEST(Enlace, AckConfirmaElEstadoYNoSeReenvia) {
  EnlaceEsp32 e(1000.0);
  e.ordenarEstado(PROTO_ST_VERDE);
  const auto ts = tramas(e.tick(0.0));
  const proto_trama_t* est = buscar(ts, PROTO_ESTADO);
  ASSERT_NE(est, nullptr);
  proto_trama_t ack;
  proto_msg_ack(&ack, est->seq, PROTO_ESTADO);
  const auto b = bytes(ack);
  e.recibir(b.data(), b.size(), 0.02);
  EXPECT_TRUE(e.estadoConfirmado());
  int reenvios = 0;
  for (int i = 1; i <= 50; ++i) reenvios += contar(tramas(e.tick(i * 0.010)), PROTO_ESTADO);
  EXPECT_EQ(reenvios, 0);
}

TEST(Enlace, AckConSeqViejoNoConfirma) {
  EnlaceEsp32 e(1000.0);
  e.ordenarEstado(PROTO_ST_VERDE);
  const proto_trama_t* est = nullptr;
  const auto ts = tramas(e.tick(0.0));
  est = buscar(ts, PROTO_ESTADO);
  ASSERT_NE(est, nullptr);
  proto_trama_t ack;
  proto_msg_ack(&ack, static_cast<uint8_t>(est->seq + 7), PROTO_ESTADO);
  const auto b = bytes(ack);
  e.recibir(b.data(), b.size(), 0.02);
  EXPECT_FALSE(e.estadoConfirmado());
}

TEST(Enlace, SinAckReintentaConSeqNuevoYSeRindeTras10) {
  EnlaceEsp32 e(1000.0);
  e.ordenarEstado(PROTO_ST_ROJO);
  std::vector<uint8_t> seqs;
  for (int i = 0; i <= 200; ++i)  // 2 s sin respuesta
    for (const auto& t : tramas(e.tick(i * 0.010)))
      if (t.tipo == PROTO_ESTADO) seqs.push_back(t.seq);
  ASSERT_EQ(seqs.size(), 11u);  // 1 envio + 10 reintentos
  for (size_t i = 1; i < seqs.size(); ++i) EXPECT_NE(seqs[i], seqs[i - 1]);
  EXPECT_EQ(e.estadisticas().sin_ack, 1u);
  const auto ev = e.recibir(nullptr, 0, 2.0);
  ASSERT_EQ(ev.size(), 1u);
  EXPECT_EQ(ev[0].tipo, EventoEsp32::Tipo::SIN_ACK);
}

TEST(Enlace, NackSinEnlaceSeReintentaPeroNackDeValorNo) {
  EnlaceEsp32 e(1000.0);
  e.ordenarEstado(PROTO_ST_AMARILLO_FIJO);
  auto est = *buscar(tramas(e.tick(0.0)), PROTO_ESTADO);
  proto_trama_t nack;
  proto_msg_nack(&nack, est.seq, PROTO_ESTADO, PROTO_NACK_SIN_ENLACE);
  auto b = bytes(nack);
  e.recibir(b.data(), b.size(), 0.01);
  EXPECT_EQ(contar(tramas(e.tick(0.15)), PROTO_ESTADO), 1);  // se reintenta
  e.ordenarBuzzer(PROTO_BZ_OK);
  auto bz = *buscar(tramas(e.tick(0.30)), PROTO_BUZZER);
  proto_msg_nack(&nack, bz.seq, PROTO_BUZZER, PROTO_NACK_VALOR);
  b = bytes(nack);
  e.recibir(b.data(), b.size(), 0.31);
  int reenvios = 0;
  for (int i = 32; i <= 100; ++i) reenvios += contar(tramas(e.tick(i * 0.010)), PROTO_BUZZER);
  EXPECT_EQ(reenvios, 0);  // un valor invalido no se arregla reintentando
}

TEST(Enlace, BotonSeConfirmaConAckDelMismoSeq) {
  EnlaceEsp32 e(1000.0);
  proto_trama_t boton;
  proto_msg_evento_boton(&boton);
  const auto b = bytes(boton, 42);
  const auto ev = e.recibir(b.data(), b.size(), 0.0);
  ASSERT_EQ(ev.size(), 1u);
  EXPECT_EQ(ev[0].tipo, EventoEsp32::Tipo::BOTON);
  const auto ts = tramas(e.tick(0.01));
  const proto_trama_t* ack = buscar(ts, PROTO_ACK);
  ASSERT_NE(ack, nullptr);
  EXPECT_EQ(ack->seq, 42);
  uint8_t tipo = 0;
  ASSERT_TRUE(proto_leer_ack(ack, &tipo));
  EXPECT_EQ(tipo, PROTO_EVENTO_BOTON);
}

TEST(Enlace, EventosDePirArranqueEnlaceYTelemetria) {
  EnlaceEsp32 e(1000.0);
  std::vector<uint8_t> todo;
  proto_trama_t t;
  proto_msg_evento_pir(&t, true, true);
  auto b = bytes(t, 1);
  todo.insert(todo.end(), b.begin(), b.end());
  proto_msg_arranque(&t, 3, 1, 2);
  b = bytes(t, 2);
  todo.insert(todo.end(), b.begin(), b.end());
  proto_msg_evento_enlace(&t, true);
  b = bytes(t, 3);
  todo.insert(todo.end(), b.begin(), b.end());
  proto_telemetria_t tel{};
  tel.version_proto = PROTO_VERSION;
  tel.estado = PROTO_ST_ROJO;
  tel.heap_libre = 123456;
  tel.err_crc = 2;
  proto_msg_telemetria(&t, &tel);
  b = bytes(t, 4);
  todo.insert(todo.end(), b.begin(), b.end());

  const auto ev = e.recibir(todo.data(), todo.size(), 1.0);
  ASSERT_EQ(ev.size(), 4u);
  EXPECT_EQ(ev[0].tipo, EventoEsp32::Tipo::PIR);
  EXPECT_TRUE(ev[0].presencia);
  EXPECT_EQ(ev[1].tipo, EventoEsp32::Tipo::ARRANQUE);
  EXPECT_EQ(ev[1].motivo, 3);
  EXPECT_EQ(ev[2].tipo, EventoEsp32::Tipo::ENLACE_RESTABLECIDO);
  EXPECT_EQ(ev[3].tipo, EventoEsp32::Tipo::TELEMETRIA);
  EXPECT_EQ(ev[3].telemetria.heap_libre, 123456u);
  EXPECT_EQ(ev[3].telemetria.err_crc, 2u);
  EXPECT_TRUE(e.hayEnlace(1.5));
  EXPECT_FALSE(e.hayEnlace(3.0));
}

TEST(Enlace, TrasRecuperarElEnlaceElEstadoDejaDeEstarConfirmado) {
  EnlaceEsp32 e(1000.0);
  e.ordenarEstado(PROTO_ST_VERDE);
  auto est = *buscar(tramas(e.tick(0.0)), PROTO_ESTADO);
  proto_trama_t t;
  proto_msg_ack(&t, est.seq, PROTO_ESTADO);
  auto b = bytes(t);
  e.recibir(b.data(), b.size(), 0.01);
  ASSERT_TRUE(e.estadoConfirmado());
  proto_msg_evento_enlace(&t, true);
  b = bytes(t, 9);
  e.recibir(b.data(), b.size(), 2.0);
  EXPECT_FALSE(e.estadoConfirmado());  // el ESP32 esta en ROJO; el daemon debe reiniciar
}

TEST(Enlace, PideTelemetriaPeriodicamente) {
  EnlaceEsp32 e(5.0);
  int pedidos = 0;
  for (int i = 0; i <= 1100; ++i) pedidos += contar(tramas(e.tick(i * 0.010)), PROTO_PEDIR_TELEMETRIA);  // 11 s
  EXPECT_EQ(pedidos, 3);  // t = 0, 5, 10
}

TEST(Enlace, BytesBasuraNoGeneranEventos) {
  EnlaceEsp32 e(1000.0);
  const uint8_t basura[] = {0x00, 0xAA, 0x55, 0xAA, 0x02, 0xFF, 0x13, 0x37, 0xAA};
  EXPECT_TRUE(e.recibir(basura, sizeof(basura), 0.0).empty());
  EXPECT_FALSE(e.hayEnlace(0.0));
}
