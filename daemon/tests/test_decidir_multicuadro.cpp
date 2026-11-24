// Pruebas unitarias de la decision por prenda (GoogleTest). Corren en el PC, sin Jetson.
#include <gtest/gtest.h>

#include "epp/decidir_multicuadro.hpp"

using namespace epp;

namespace {
// Persona tipica del encuadre: torso hacia arriba, centrada
const Deteccion PERSONA{170, 60, 470, 480, 0.80f, PERSON};
Deteccion casco(float conf = 0.80f) { return {270, 40, 370, 120, conf, HELMET}; }      // centro y=80 (cabeza)
Deteccion chaleco(float conf = 0.80f) { return {190, 220, 450, 440, conf, VEST}; }     // centro y=330 (torso)
}  // namespace

TEST(PersonaPrincipal, EligeLaDeMayorAreaSobreElUmbral) {
  Detecciones d = {{0, 0, 50, 50, 0.9f, PERSON}, PERSONA, {0, 0, 600, 470, 0.30f, PERSON}};
  const Deteccion* p = personaPrincipal(d, 0.40f);
  ASSERT_NE(p, nullptr);
  EXPECT_FLOAT_EQ(p->x1, PERSONA.x1);  // la enorme con 0,30 se ignora por el umbral
}

TEST(PersonaPrincipal, SinPersonaDevuelveNulo) {
  Detecciones d = {casco(), chaleco()};
  EXPECT_EQ(personaPrincipal(d, 0.40f), nullptr);
}

TEST(ZonaDeLaPersona, CascoEnLaCabezaCuenta) {
  EXPECT_TRUE(cascoEnPersona(PERSONA, {PERSONA, casco()}, 0.5f));
}

TEST(ZonaDeLaPersona, CascoFueraDeLaPersonaNoCuenta) {
  Deteccion fondo{560, 20, 630, 80, 0.9f, HELMET};  // casco colgado en la pared, fuera de la caja
  EXPECT_FALSE(cascoEnPersona(PERSONA, {PERSONA, fondo}, 0.5f));
}

TEST(ZonaDeLaPersona, CascoEnLaCinturaNoCuenta) {
  Deteccion en_mano{270, 380, 370, 460, 0.9f, HELMET};  // casco sostenido a la altura de la cintura
  EXPECT_FALSE(cascoEnPersona(PERSONA, {PERSONA, en_mano}, 0.5f));
}

TEST(ZonaDeLaPersona, CascoBajoElUmbralNoCuenta) {
  EXPECT_FALSE(cascoEnPersona(PERSONA, {PERSONA, casco(0.47f)}, 0.5f));  // 0,47 = maximo falso positivo medido
}

TEST(ZonaDeLaPersona, ChalecoEnElTorsoCuenta) {
  EXPECT_TRUE(chalecoEnPersona(PERSONA, {PERSONA, chaleco()}, 0.5f));
}

TEST(ZonaDeLaPersona, ChalecoFueraDeLaPersonaNoCuenta) {
  Deteccion otro{500, 200, 640, 400, 0.9f, VEST};  // centro x=570, fuera de la persona
  EXPECT_FALSE(chalecoEnPersona(PERSONA, {PERSONA, otro}, 0.5f));
}

TEST(Decisor, ConCascoYChalecoEnTodosLosCuadrosCumple) {
  Decisor dec;
  for (int i = 0; i < 10; ++i) dec.agregarCuadro({PERSONA, casco(), chaleco()});
  const auto r = dec.resultado();
  EXPECT_TRUE(r.decidido);
  EXPECT_TRUE(r.cumple);
  EXPECT_FALSE(r.falta_casco);
  EXPECT_FALSE(r.falta_chaleco);
}

TEST(Decisor, SinPrendasFaltanAmbas) {
  Decisor dec;
  for (int i = 0; i < 10; ++i) dec.agregarCuadro({PERSONA, {270, 50, 370, 120, 0.8f, NO_HELMET}});
  const auto r = dec.resultado();
  EXPECT_TRUE(r.decidido);
  EXPECT_FALSE(r.cumple);
  EXPECT_TRUE(r.falta_casco);
  EXPECT_TRUE(r.falta_chaleco);
}

TEST(Decisor, CascoEnMenosDelSesentaPorCientoCuentaComoFalta) {
  Decisor dec;
  for (int i = 0; i < 10; ++i) {
    Detecciones d = {PERSONA, chaleco()};
    if (i < 5) d.push_back(casco());  // 50 %
    dec.agregarCuadro(d);
  }
  const auto r = dec.resultado();
  EXPECT_TRUE(r.falta_casco);
  EXPECT_FALSE(r.falta_chaleco);
  EXPECT_NEAR(r.fraccion_casco, 0.5f, 1e-6);
}

TEST(Decisor, FalsosPositivosAisladosNoDanVerde) {
  Decisor dec;
  for (int i = 0; i < 20; ++i) {
    Detecciones d = {PERSONA};
    if (i == 7) d.push_back(casco(0.6f));  // un solo cuadro con "casco"
    dec.agregarCuadro(d);
  }
  EXPECT_TRUE(dec.resultado().falta_casco);
}

TEST(Decisor, PocosCuadrosConPersonaNoDecide) {
  Decisor dec;
  for (int i = 0; i < 5; ++i) dec.agregarCuadro({PERSONA, casco(), chaleco()});
  for (int i = 0; i < 10; ++i) dec.agregarCuadro({casco(), chaleco()});  // sin persona: no cuentan
  const auto r = dec.resultado();
  EXPECT_FALSE(r.decidido);
  EXPECT_FALSE(r.cumple);
  EXPECT_EQ(r.cuadros_con_persona, 5);
}

TEST(Decisor, ChalecoNoExigidoSeIgnora) {
  ConfigDecision cfg;
  cfg.exigir_chaleco = false;
  Decisor dec(cfg);
  for (int i = 0; i < 10; ++i) dec.agregarCuadro({PERSONA, casco()});
  const auto r = dec.resultado();
  EXPECT_TRUE(r.cumple);
  EXPECT_FALSE(r.falta_chaleco);
}

TEST(Decisor, ReiniciarBorraLoAcumulado) {
  Decisor dec;
  for (int i = 0; i < 10; ++i) dec.agregarCuadro({PERSONA});
  dec.reiniciar();
  EXPECT_EQ(dec.cuadrosConPersona(), 0);
  EXPECT_FALSE(dec.resultado().decidido);
}
