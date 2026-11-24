// Prueba de integracion del puerto serie con un pseudo-terminal (pty) del sistema:
// el daemon escribe por el lado "esclavo" como si fuera /dev/ttyTHS1 y la prueba lee por el
// "maestro" como si fuera el ESP32. No necesita hardware.
#include <gtest/gtest.h>

#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdexcept>
#include <vector>

#include "epp/enlace.hpp"
#include "puerto_serie.hpp"

using namespace epp;

namespace {
struct Pty {
  int maestro = -1;
  std::string esclavo;
  Pty() {
    maestro = posix_openpt(O_RDWR | O_NOCTTY);
    if (maestro < 0 || grantpt(maestro) != 0 || unlockpt(maestro) != 0) return;
    esclavo = ptsname(maestro);
  }
  ~Pty() {
    if (maestro >= 0) close(maestro);
  }
  std::vector<uint8_t> leer(int espera_ms) {
    std::vector<uint8_t> out;
    pollfd p{maestro, POLLIN, 0};
    while (poll(&p, 1, espera_ms) > 0) {
      uint8_t buf[256];
      const ssize_t n = read(maestro, buf, sizeof(buf));
      if (n <= 0) break;
      out.insert(out.end(), buf, buf + n);
      espera_ms = 20;
    }
    return out;
  }
};
}  // namespace

TEST(PuertoSerie, ElLatidoLlegaIntactoPorElPuerto) {
  Pty pty;
  ASSERT_FALSE(pty.esclavo.empty());
  PuertoSerie puerto(pty.esclavo, 115200);
  EnlaceEsp32 enlace(1000.0);
  const auto out = enlace.tick(0.0);
  puerto.escribir(out.data(), out.size());
  const auto llegado = pty.leer(200);
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(llegado, out);  // mismos bytes, sin traducciones de fin de linea (modo crudo)
}

TEST(PuertoSerie, LoQueEnviaElEsp32LlegaAlEnlace) {
  Pty pty;
  ASSERT_FALSE(pty.esclavo.empty());
  PuertoSerie puerto(pty.esclavo, 115200);
  proto_trama_t t;
  proto_msg_evento_pir(&t, true, true);
  t.seq = 5;
  uint8_t buf[PROTO_MAX_TRAMA];
  const size_t n = proto_codificar(&t, buf, sizeof(buf));
  ASSERT_EQ(write(pty.maestro, buf, n), static_cast<ssize_t>(n));
  uint8_t rx[64];
  size_t total = 0;
  for (int i = 0; i < 10 && total < n; ++i) total += puerto.leer(rx + total, sizeof(rx) - total, 50);
  ASSERT_EQ(total, n);
  EnlaceEsp32 enlace(1000.0);
  const auto ev = enlace.recibir(rx, total, 0.0);
  ASSERT_EQ(ev.size(), 1u);
  EXPECT_EQ(ev[0].tipo, EventoEsp32::Tipo::PIR);
}

TEST(PuertoSerie, DispositivoInexistenteLanzaError) {
  EXPECT_THROW(PuertoSerie("/dev/no_existe_epp", 115200), std::runtime_error);
}
