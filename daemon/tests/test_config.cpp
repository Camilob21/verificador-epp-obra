// Pruebas del archivo de configuracion: valores validos, invalidos y claves desconocidas.
#include <gtest/gtest.h>

#include "epp/config.hpp"

using namespace epp;

TEST(Config, PorDefectoCoincideConLoMedidoEnLaPruebaB) {
  Config c;
  EXPECT_FLOAT_EQ(c.decision.umbral_prenda, 0.50f);
  EXPECT_FLOAT_EQ(c.decision.umbral_persona, 0.40f);
  EXPECT_TRUE(c.uart.empty());
}

TEST(Config, AplicaValoresValidosYComentarios) {
  Config c;
  const auto avisos = aplicarTexto(c,
                                   "# comentario\n"
                                   "umbral_prenda = 0.45   # bajado tras la prueba con casco real\n"
                                   "\n"
                                   "cuadros_minimos=10\n"
                                   "uart = /dev/ttyTHS1\n"
                                   "usar_pir = si\n");
  EXPECT_TRUE(avisos.empty());
  EXPECT_FLOAT_EQ(c.decision.umbral_prenda, 0.45f);
  EXPECT_EQ(c.decision.cuadros_minimos, 10);
  EXPECT_EQ(c.uart, "/dev/ttyTHS1");
  EXPECT_TRUE(c.maquina.usar_pir);
}

TEST(Config, PuestaEnMarchaYFactoryReset) {
  Config c;
  EXPECT_TRUE(c.marca_puesta_en_marcha.empty());  // por defecto sin asistente
  EXPECT_TRUE(c.solicitud_fabrica.empty());
  const auto avisos = aplicarTexto(c,
                                   "marca_puesta_en_marcha = /var/lib/epp/puesta_en_marcha.ok\n"
                                   "solicitud_fabrica = /var/lib/epp/solicitud_fabrica\n");
  EXPECT_TRUE(avisos.empty());
  EXPECT_EQ(c.marca_puesta_en_marcha, "/var/lib/epp/puesta_en_marcha.ok");
  EXPECT_EQ(c.solicitud_fabrica, "/var/lib/epp/solicitud_fabrica");
  EXPECT_NE(describirConfig(c).find("puesta en marcha: /var/lib/epp/puesta_en_marcha.ok"), std::string::npos);
}

TEST(Config, ValorInvalidoConservaElPorDefectoYAvisa) {
  Config c;
  const auto avisos = aplicarTexto(c, "umbral_prenda = 1.7\ncuadros_minimos = ocho\nusar_pir = quizas\n");
  EXPECT_EQ(avisos.size(), 3u);
  EXPECT_FLOAT_EQ(c.decision.umbral_prenda, 0.50f);
  EXPECT_EQ(c.decision.cuadros_minimos, 8);
  EXPECT_FALSE(c.maquina.usar_pir);
}

TEST(Config, ClaveDesconocidaYLineaSinIgualSoloAvisan) {
  Config c;
  const auto avisos = aplicarTexto(c, "umbral_casco = 0.3\nesto no tiene igual\n");
  ASSERT_EQ(avisos.size(), 2u);
  EXPECT_NE(avisos[0].find("desconocida"), std::string::npos);
}

TEST(Config, NumeroConBasuraAlFinalEsInvalido) {
  Config c;
  EXPECT_FALSE(aplicarLinea(c, "umbral_prenda = 0.5x").empty());
  EXPECT_FLOAT_EQ(c.decision.umbral_prenda, 0.50f);
}

TEST(Config, ResultadoNoPuedeDurarMasQueElLimiteDelEsp32) {
  Config c;
  EXPECT_FALSE(aplicarLinea(c, "duracion_resultado_s = 10").empty());  // el ESP32 corta a los 8 s
  EXPECT_TRUE(aplicarLinea(c, "duracion_resultado_s = 6").empty());
  EXPECT_DOUBLE_EQ(c.maquina.duracion_resultado_s, 6.0);
}

TEST(Config, CorrigeEsperaMenorQueLaVentana) {
  Config c;
  const auto avisos = aplicarTexto(c, "ventana_verificacion_s = 3\nespera_max_verificacion_s = 2\n");
  EXPECT_EQ(avisos.size(), 1u);
  EXPECT_DOUBLE_EQ(c.maquina.espera_max_verificacion_s, 3.0);
}

TEST(Config, AvisaSiNoSeExigeNingunaPrenda) {
  Config c;
  const auto avisos = aplicarTexto(c, "exigir_casco = no\nexigir_chaleco = no\n");
  ASSERT_EQ(avisos.size(), 1u);
  EXPECT_NE(avisos[0].find("ATENCION"), std::string::npos);
}

TEST(Config, CuentaRegresiva) {
  Config c;
  EXPECT_DOUBLE_EQ(c.maquina.cuenta_regresiva_s, 10.0);
  EXPECT_TRUE(aplicarLinea(c, "cuenta_regresiva_s = 0").empty());
  EXPECT_DOUBLE_EQ(c.maquina.cuenta_regresiva_s, 0.0);
  EXPECT_FALSE(aplicarLinea(c, "cuenta_regresiva_s = 90").empty());  // fuera de rango
  EXPECT_DOUBLE_EQ(c.maquina.cuenta_regresiva_s, 0.0);
}
