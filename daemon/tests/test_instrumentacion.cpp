// Pruebas de la lectura de /proc con textos reales (copiados de la Jetson y de un PC).
#include <gtest/gtest.h>

#include <cmath>

#include "epp/instrumentacion.hpp"

using namespace epp;

TEST(Instrumentacion, CpuEntreDosMuestras) {
  const auto a = analizarStat("cpu  100 0 100 800 0 0 0 0 0 0\ncpu0 50 0 50 400 0 0 0 0 0 0\n");
  const auto b = analizarStat("cpu  150 0 150 900 0 0 0 0 0 0\ncpu0 75 0 75 450 0 0 0 0 0 0\n");
  EXPECT_EQ(a.total, 1000u);
  EXPECT_EQ(a.ocupado, 200u);
  EXPECT_NEAR(porcentajeCpu(a, b), 50.0, 1e-9);  // 100 de 200 ticks nuevos ocupados
}

TEST(Instrumentacion, IowaitCuentaComoInactivo) {
  const auto a = analizarStat("cpu  0 0 0 0 0 0 0 0\n");
  const auto b = analizarStat("cpu  10 0 0 40 50 0 0 0\n");
  EXPECT_NEAR(porcentajeCpu(a, b), 10.0, 1e-9);
}

TEST(Instrumentacion, CpuSinAvanceEsNaN) {
  const auto a = analizarStat("cpu  1 1 1 1 0 0 0 0\n");
  EXPECT_TRUE(std::isnan(porcentajeCpu(a, a)));
}

TEST(Instrumentacion, Meminfo) {
  const auto m = analizarMeminfo("MemTotal:        4059272 kB\nMemFree:          812344 kB\nMemAvailable:    2967016 kB\n");
  EXPECT_EQ(m.total_kb, 4059272u);
  EXPECT_EQ(m.disponible_kb, 2967016u);
}

TEST(Instrumentacion, NetDev) {
  const auto v = analizarNetDev(
      "Inter-|   Receive                                                |  Transmit\n"
      " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
      "    lo:   12345      67    0    0    0     0          0         0    12345      67    0    0    0     0       0          0\n"
      "  eth0: 9876543    8000    0    0    0     0          0        10  1234567    5000    0    0    0     0       0          0\n");
  ASSERT_EQ(v.size(), 2u);
  EXPECT_EQ(v[1].nombre, "eth0");
  EXPECT_EQ(v[1].rx_bytes, 9876543u);
  EXPECT_EQ(v[1].tx_bytes, 1234567u);
  EXPECT_EQ(v[1].rx_paquetes, 8000u);
}

TEST(Instrumentacion, PuertosEnEscuchaTcp) {
  // 0016 = 22 (ssh, LISTEN), 1F91 = 8081 (LISTEN), 0050 = 80 (ESTABLISHED: no cuenta)
  const auto p = analizarPuertosEscucha(
      "  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode\n"
      "   0: 00000000:0016 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 1 1\n"
      "   1: 0100007F:1F91 00000000:0000 0A 00000000:00000000 00:00000000 00000000  1000        0 2 1\n"
      "   2: 0A00000F:0050 0A000001:C350 01 00000000:00000000 00:00000000 00000000     0        0 3 1\n"
      "   3: 00000000:0016 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 4 1\n");
  ASSERT_EQ(p.size(), 2u);
  EXPECT_EQ(p[0], 22);
  EXPECT_EQ(p[1], 8081);
}

TEST(Instrumentacion, PuertosTcp6) {
  const auto p = analizarPuertosEscucha(
      "  sl  local_address                         remote_address                        st\n"
      "   0: 00000000000000000000000000000000:0050 00000000000000000000000000000000:0000 0A 0 0 0\n");
  ASSERT_EQ(p.size(), 1u);
  EXPECT_EQ(p[0], 80);
}

TEST(Instrumentacion, Uptime) { EXPECT_DOUBLE_EQ(analizarUptime("3605.27 12000.10\n"), 3605.27); }

TEST(Instrumentacion, TextoVacioNoRompe) {
  EXPECT_EQ(analizarStat("").total, 0u);
  EXPECT_EQ(analizarMeminfo("").total_kb, 0u);
  EXPECT_TRUE(analizarNetDev("").empty());
  EXPECT_TRUE(analizarPuertosEscucha("").empty());
  EXPECT_TRUE(std::isnan(analizarUptime("")));
}
