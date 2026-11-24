// Pruebas de la API: analisis de peticiones, rutas, codigos de error y JSON.
#include <gtest/gtest.h>

#include "epp/api.hpp"
#include "epp/json.hpp"

using namespace epp;

TEST(Http, AnalizaRutaYConsulta) {
  const auto p = analizarPeticion("GET /api/eventos?limite=20&x HTTP/1.1\r\nHost: a\r\n\r\n");
  ASSERT_TRUE(p.valida);
  EXPECT_EQ(p.metodo, "GET");
  EXPECT_EQ(p.ruta, "/api/eventos");
  EXPECT_EQ(p.consulta.at("limite"), "20");
  EXPECT_EQ(p.consulta.count("x"), 1u);
}

TEST(Http, RechazaBasura) {
  EXPECT_FALSE(analizarPeticion("hola\r\n\r\n").valida);
  EXPECT_FALSE(analizarPeticion("GET sin-barra HTTP/1.1\r\n\r\n").valida);
  EXPECT_FALSE(analizarPeticion("").valida);
}

TEST(Http, SerializaConLongitudCorrecta) {
  Respuesta r;
  r.cuerpo = "{\"a\":1}";
  const auto s = serializar(r);
  EXPECT_EQ(s.compare(0, 15, "HTTP/1.1 200 OK"), 0);
  EXPECT_NE(s.find("Content-Length: 7\r\n"), std::string::npos);
  EXPECT_EQ(s.substr(s.size() - 7), "{\"a\":1}");
}

TEST(Json, EscapaComillasYControl) {
  EXPECT_EQ(jsonTexto("a\"b\\c\nd\x01"), "\"a\\\"b\\\\c\\nd\\u0001\"");
  EXPECT_EQ(jsonNumero(0.0 / 0.0), "null");
  EXPECT_EQ(jsonNumero(1.234, 1), "1.2");
}

TEST(Api, EstadoContieneLoPrincipal) {
  Instantanea s;
  s.estado = Estado::RESULTADO_ROJO;
  s.luz = Luz::ROJO;
  s.mensaje = "Falta: casco";
  s.rojos = 3;
  Config c;
  const auto r = atender(analizarPeticion("GET /api/estado HTTP/1.1\r\n\r\n"), s, c, nullptr);
  EXPECT_EQ(r.codigo, 200);
  EXPECT_NE(r.cuerpo.find("\"rojos\":3"), std::string::npos);
  EXPECT_NE(r.cuerpo.find("\"mensaje\":\"Falta: casco\""), std::string::npos);
  EXPECT_NE(r.cuerpo.find("\"rele\":false"), std::string::npos);
  EXPECT_NE(r.cuerpo.find("\"ultimo\":null"), std::string::npos);
}

TEST(Api, EventosConLimiteYSinBase) {
  Instantanea s;
  Config c;
  int pedido = 0;
  FuenteEventos fuente = [&](int limite) {
    pedido = limite;
    Evento e;
    e.id = 7;
    e.resultado = "verde";
    return std::vector<Evento>{e};
  };
  auto r = atender(analizarPeticion("GET /api/eventos?limite=5 HTTP/1.1\r\n\r\n"), s, c, fuente);
  EXPECT_EQ(r.codigo, 200);
  EXPECT_EQ(pedido, 5);
  EXPECT_NE(r.cuerpo.find("\"id\":7"), std::string::npos);
  EXPECT_EQ(atender(analizarPeticion("GET /api/eventos?limite=900 HTTP/1.1\r\n\r\n"), s, c, fuente).codigo, 400);
  EXPECT_EQ(atender(analizarPeticion("GET /api/eventos HTTP/1.1\r\n\r\n"), s, c, nullptr).codigo, 503);
}

TEST(Api, RutaDesconocidaYMetodoNoPermitido) {
  Instantanea s;
  Config c;
  EXPECT_EQ(atender(analizarPeticion("GET /api/nada HTTP/1.1\r\n\r\n"), s, c, nullptr).codigo, 404);
  EXPECT_EQ(atender(analizarPeticion("POST /api/estado HTTP/1.1\r\n\r\n"), s, c, nullptr).codigo, 405);
  EXPECT_EQ(atender(analizarPeticion("basura"), s, c, nullptr).codigo, 400);
}

TEST(Api, SistemaCalculaTasasDeRedYCpu) {
  MuestreadorSistema m;
  MuestreadorSistema::Textos a, b;
  a.stat = "cpu  100 0 100 800 0 0 0 0\n";
  b.stat = "cpu  150 0 150 900 0 0 0 0\n";
  a.netdev = "h\nh\n eth0: 1000 1 0 0 0 0 0 0 500 1 0 0 0 0 0 0\n";
  b.netdev = "h\nh\n eth0: 3000 2 0 0 0 0 0 0 1500 2 0 0 0 0 0 0\n";
  a.gpu_load = b.gpu_load = "345\n";
  b.temperaturas = {{"CPU-therm", "41500"}, {"PMIC-Die", "100000"}};
  m.actualizar(10.0, a);
  EXPECT_LT(m.cpu_pct, 0);  // aun no hay dos muestras
  m.actualizar(12.0, b);
  EXPECT_NEAR(m.cpu_pct, 50.0, 1e-9);
  ASSERT_EQ(m.red.size(), 1u);
  EXPECT_NEAR(m.red[0].rx_bps, 1000.0, 1e-9);  // 2000 B en 2 s
  EXPECT_NEAR(m.red[0].tx_bps, 500.0, 1e-9);
  EXPECT_NEAR(m.gpu_pct, 34.5, 1e-9);
  ASSERT_EQ(m.temperaturas_c.size(), 2u);
  EXPECT_NEAR(m.temperaturas_c[0].second, 41.5, 1e-9);
  Instantanea s;
  s.sistema = m;
  const auto j = jsonSistema(s);
  EXPECT_NE(j.find("\"interfaz\":\"eth0\""), std::string::npos);
  EXPECT_NE(j.find("\"mcu\":null"), std::string::npos);
}

TEST(Api, SistemaIncluyeTelemetriaDelMcu) {
  Instantanea s;
  s.hay_telemetria = true;
  s.telemetria.heap_libre = 200000;
  s.telemetria.cpu_pct = PROTO_CPU_NO_DISPONIBLE;
  const auto j = jsonSistema(s);
  EXPECT_NE(j.find("\"heap_libre\":200000"), std::string::npos);
  EXPECT_NE(j.find("\"cpu_pct\":null,\"heap_libre\""), std::string::npos);
}

TEST(Api, CamaraEnVivoDesactivadaVaciaYActiva) {
  Instantanea s;
  Config c;
  const auto p = analizarPeticion("GET /api/camara.jpg HTTP/1.1\r\n\r\n");
  EXPECT_EQ(atender(p, s, c, nullptr).codigo, 404);  // sin fuente = desactivada
  EXPECT_EQ(atender(p, s, c, nullptr, [] { return std::string(); }).codigo, 503);
  const auto r = atender(p, s, c, nullptr, [] { return std::string("\xFF\xD8...\xFF\xD9", 7); });
  EXPECT_EQ(r.codigo, 200);
  EXPECT_EQ(r.tipo, "image/jpeg");
  EXPECT_EQ(r.cuerpo.size(), 7u);
}
