// Pruebas del asistente de puesta en marcha (bring-up).
#include <gtest/gtest.h>

#include "epp/puesta_en_marcha.hpp"

using namespace epp;

namespace {
EntradasPuesta todo(bool mano) {
  EntradasPuesta e;
  e.camara = e.esp32 = true;
  e.presencia = mano;
  return e;
}
}  // namespace

TEST(PuestaEnMarcha, EmpiezaIncompletaYPideAcomodarLaCamara) {
  PuestaEnMarcha p(3.0);
  const auto s = p.actualizar(0, todo(false));
  EXPECT_FALSE(s.completa);
  EXPECT_FALSE(s.sensor);
  EXPECT_EQ(s.confirmacion_total_s, 3.0);
  EXPECT_NE(s.mensaje.find("acomode la camara"), std::string::npos);
}

TEST(PuestaEnMarcha, ManoTresSegundosCompleta) {
  PuestaEnMarcha p(3.0);
  p.actualizar(0, todo(false));
  auto s = p.actualizar(1.0, todo(true));
  EXPECT_TRUE(s.sensor);
  EXPECT_FALSE(s.completa);
  s = p.actualizar(3.5, todo(true));
  EXPECT_NEAR(s.confirmacion_s, 2.5, 1e-9);
  EXPECT_FALSE(s.completa);
  s = p.actualizar(4.0, todo(true));
  EXPECT_TRUE(s.completa);
}

TEST(PuestaEnMarcha, RetirarLaManoReiniciaElContador) {
  PuestaEnMarcha p(3.0);
  p.actualizar(0, todo(true));
  p.actualizar(2.0, todo(false));
  auto s = p.actualizar(2.5, todo(true));
  EXPECT_NEAR(s.confirmacion_s, 0.0, 1e-9);
  s = p.actualizar(5.0, todo(true));
  EXPECT_FALSE(s.completa);
  s = p.actualizar(5.6, todo(true));
  EXPECT_TRUE(s.completa);
  EXPECT_TRUE(s.sensor);  // el sensor ya quedo probado aunque se retire la mano
}

TEST(PuestaEnMarcha, SinEnlaceNoSePuedeConfirmar) {
  PuestaEnMarcha p(3.0);
  EntradasPuesta e = todo(true);
  e.esp32 = false;
  p.actualizar(0, e);
  const auto s = p.actualizar(10, e);
  EXPECT_FALSE(s.completa);
  EXPECT_FALSE(s.sensor);
  EXPECT_NE(s.mensaje.find("ESP32"), std::string::npos);
}

TEST(PuestaEnMarcha, SinCamaraNoSePuedeConfirmar) {
  PuestaEnMarcha p(3.0);
  EntradasPuesta e = todo(true);
  e.camara = false;
  p.actualizar(0, e);
  const auto s = p.actualizar(10, e);
  EXPECT_FALSE(s.completa);
  EXPECT_NE(s.mensaje.find("camara"), std::string::npos);
}

TEST(PuestaEnMarcha, LaHoraNoBloquea) {
  PuestaEnMarcha p(3.0);
  EntradasPuesta e = todo(true);
  e.hora = false;
  p.actualizar(0, e);
  const auto s = p.actualizar(3.0, e);
  EXPECT_TRUE(s.completa);
  EXPECT_FALSE(s.hora);
}

TEST(PuestaEnMarcha, CompletaEsDefinitiva) {
  PuestaEnMarcha p(3.0);
  p.actualizar(0, todo(true));
  p.actualizar(3.0, todo(true));
  EntradasPuesta e;  // todo falla despues
  const auto s = p.actualizar(4.0, e);
  EXPECT_TRUE(s.completa);
  EXPECT_EQ(s.confirmacion_s, 3.0);
}
