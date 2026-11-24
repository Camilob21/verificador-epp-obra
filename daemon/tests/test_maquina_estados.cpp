// Pruebas unitarias de la maquina de estados (GoogleTest). El tiempo es simulado.
#include <gtest/gtest.h>

#include "epp/maquina_estados.hpp"

using namespace epp;

namespace {
const double DT = 1.0 / 15.0;  // ~15 cuadros por segundo, como en la Jetson
const Deteccion PERSONA{170, 60, 470, 480, 0.80f, PERSON};
const Deteccion LEJANA{290, 300, 350, 420, 0.80f, PERSON};   // pequena: lejos de la marca
const Deteccion CASCO{270, 40, 370, 120, 0.80f, HELMET};
const Deteccion CHALECO{190, 220, 450, 440, 0.80f, VEST};

// Avanza la maquina con las mismas detecciones durante 'segundos'; devuelve la ultima salida
Salida correr(MaquinaEstados& m, double& t, double segundos, const Detecciones& d, bool pir = true) {
  Salida s;
  const int n = static_cast<int>(segundos / DT + 0.5);
  for (int i = 0; i < n; ++i) {
    s = m.actualizar(t, pir, d);
    t += DT;
  }
  return s;
}
}  // namespace

TEST(Maquina, SinPirPasaDeReposoAEsperarPersona) {
  MaquinaEstados m;
  const Salida s = m.actualizar(0.0, false, {});
  EXPECT_EQ(s.estado, Estado::ESPERANDO_PERSONA);
  EXPECT_EQ(s.luz, Luz::AMARILLO_PARPADEO);
  EXPECT_TRUE(s.cambio);
  EXPECT_FALSE(s.rele);
}

TEST(Maquina, ConPirSeQuedaEnReposoSinPresencia) {
  ConfigMaquina cm;
  cm.usar_pir = true;
  MaquinaEstados m(cm);
  double t = 0;
  const Salida s = correr(m, t, 2.0, {PERSONA}, /*pir=*/false);
  EXPECT_EQ(s.estado, Estado::REPOSO);
  EXPECT_EQ(s.luz, Luz::APAGADO);
}

TEST(Maquina, PersonaLejanaNoIniciaVerificacion) {
  MaquinaEstados m;
  double t = 0;
  const Salida s = correr(m, t, 3.0, {LEJANA});
  EXPECT_EQ(s.estado, Estado::ESPERANDO_PERSONA);
}

TEST(Maquina, PersonaEnPosicionIniciaVerificacion) {
  MaquinaEstados m;
  double t = 0;
  correr(m, t, DT, {});  // REPOSO -> ESPERANDO
  const Salida s = correr(m, t, 0.5, {PERSONA});
  EXPECT_EQ(s.estado, Estado::VERIFICANDO);
  EXPECT_EQ(s.luz, Luz::AMARILLO_FIJO);
}

TEST(Maquina, ConCascoYChalecoDaVerdeYActivaElRele) {
  MaquinaEstados m;
  double t = 0;
  correr(m, t, DT, {});
  const Salida s = correr(m, t, 3.0, {PERSONA, CASCO, CHALECO});
  EXPECT_EQ(s.estado, Estado::RESULTADO_VERDE);
  EXPECT_EQ(s.luz, Luz::VERDE);
  EXPECT_TRUE(s.rele);
}

TEST(Maquina, SinPrendasDaRojoYDiceQueFalta) {
  MaquinaEstados m;
  double t = 0;
  correr(m, t, DT, {});
  Salida s;
  bool visto_resultado = false;
  for (int i = 0; i < 60; ++i) {  // 4 s
    s = m.actualizar(t, true, {PERSONA});
    t += DT;
    if (s.resultado) {
      visto_resultado = true;
      EXPECT_TRUE(s.resultado->falta_casco);
      EXPECT_TRUE(s.resultado->falta_chaleco);
      break;
    }
  }
  EXPECT_TRUE(visto_resultado);
  EXPECT_EQ(s.estado, Estado::RESULTADO_ROJO);
  EXPECT_FALSE(s.rele);
  EXPECT_NE(s.mensaje.find("casco"), std::string::npos);
  EXPECT_NE(s.mensaje.find("chaleco"), std::string::npos);
}

TEST(Maquina, SiLaPersonaSeVaDuranteLaVerificacionDaRojo) {
  MaquinaEstados m;
  double t = 0;
  correr(m, t, DT, {});
  correr(m, t, 0.5, {PERSONA});  // entra a VERIFICANDO
  ASSERT_EQ(m.estado(), Estado::VERIFICANDO);
  const Salida s = correr(m, t, 5.0, {});  // se fue: no hay cuadros suficientes
  EXPECT_EQ(s.estado, Estado::RESULTADO_ROJO);
  EXPECT_NE(s.mensaje.find("No se pudo verificar"), std::string::npos);
}

TEST(Maquina, ElResultadoDuraYVuelveAReposo) {
  MaquinaEstados m;
  double t = 0;
  correr(m, t, DT, {});
  // Avanza cuadro a cuadro hasta el instante exacto en que entra a VERDE
  for (int i = 0; i < 100 && m.estado() != Estado::RESULTADO_VERDE; ++i) {
    m.actualizar(t, true, {PERSONA, CASCO, CHALECO});
    t += DT;
  }
  ASSERT_EQ(m.estado(), Estado::RESULTADO_VERDE);
  correr(m, t, 4.0, {});
  EXPECT_EQ(m.estado(), Estado::RESULTADO_VERDE);  // aun dentro de los 5 s
  correr(m, t, 1.5, {});
  EXPECT_NE(m.estado(), Estado::RESULTADO_VERDE);  // ya salio del resultado
}

TEST(Maquina, ElReleSoloSeActivaEnVerde) {
  MaquinaEstados m;
  double t = 0;
  for (int i = 0; i < 200; ++i) {  // recorre varios estados con persona sin EPP
    const Salida s = m.actualizar(t, true, {PERSONA});
    t += DT;
    if (s.estado != Estado::RESULTADO_VERDE) EXPECT_FALSE(s.rele);
  }
}

TEST(Maquina, ConSensorPideAcercarLaMano) {
  ConfigMaquina cm;
  cm.usar_pir = true;
  MaquinaEstados m(cm);
  const Salida s = m.actualizar(0.0, false, {});
  EXPECT_EQ(s.estado, Estado::REPOSO);
  EXPECT_NE(s.mensaje.find("Acerque la mano"), std::string::npos);
}

TEST(Maquina, LaManoEsUnDisparoQueEsperaALaPersona) {
  // HC-SR04 como boton sin contacto: la mano esta 0,3 s y se retira; la persona tarda ~4 s en
  // llegar a la marca. La espera no debe cancelarse al retirar la mano.
  ConfigMaquina cm;
  cm.usar_pir = true;
  MaquinaEstados m(cm);
  double t = 0;
  Salida s = correr(m, t, 0.3, {}, /*pir=*/true);
  EXPECT_EQ(s.estado, Estado::ESPERANDO_PERSONA);
  s = correr(m, t, 4.0, {}, /*pir=*/false);  // camina hacia la marca, sin presencia en el sensor
  EXPECT_EQ(s.estado, Estado::ESPERANDO_PERSONA);
  s = correr(m, t, 0.5, {PERSONA}, /*pir=*/false);  // ya en la marca
  EXPECT_EQ(s.estado, Estado::VERIFICANDO);
}

TEST(Maquina, SiNadieLlegaALaMarcaVuelveAReposo) {
  ConfigMaquina cm;
  cm.usar_pir = true;
  MaquinaEstados m(cm);
  double t = 0;
  correr(m, t, 0.3, {}, /*pir=*/true);
  Salida s = correr(m, t, 19.0, {}, /*pir=*/false);
  EXPECT_EQ(s.estado, Estado::ESPERANDO_PERSONA);  // aun dentro de los 20 s
  s = correr(m, t, 2.0, {}, /*pir=*/false);
  EXPECT_EQ(s.estado, Estado::REPOSO);
  EXPECT_FALSE(s.rele);
}
