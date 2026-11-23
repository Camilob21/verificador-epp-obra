// Pruebas de la decodificacion de la salida YOLOv8 [13][N] y del NMS por clase.
#include <gtest/gtest.h>

#include "epp/posprocesamiento.hpp"

using namespace epp;

namespace {
// Salida sintetica con N candidatos, todos con puntaje 0 salvo los que se agreguen.
struct SalidaFalsa {
  explicit SalidaFalsa(int n) : n_(n), datos(static_cast<size_t>(FILAS_SALIDA) * static_cast<size_t>(n), 0.0f) {}
  void poner(int a, float cx, float cy, float w, float h, int clase, float conf) {
    at(0, a) = cx;
    at(1, a) = cy;
    at(2, a) = w;
    at(3, a) = h;
    at(4 + clase, a) = conf;
  }
  float& at(int fila, int a) { return datos[static_cast<size_t>(fila) * static_cast<size_t>(n_) + static_cast<size_t>(a)]; }
  int n_;
  std::vector<float> datos;
};
GeometriaRed red480(int n) { return GeometriaRed{480, 640, 0, n}; }
}  // namespace

TEST(Posprocesamiento, IouDeCajasIgualesEsUnoYDeSeparadasCero) {
  const Deteccion a{0, 0, 10, 10, 1, 0}, b{20, 20, 30, 30, 1, 0};
  EXPECT_FLOAT_EQ(iou(a, a), 1.0f);
  EXPECT_FLOAT_EQ(iou(a, b), 0.0f);
  const Deteccion c{5, 0, 15, 10, 1, 0};  // mitad superpuesta: 50 / 150
  EXPECT_NEAR(iou(a, c), 1.0f / 3.0f, 1e-6f);
}

TEST(Posprocesamiento, SalidaVaciaNoDaDetecciones) {
  SalidaFalsa s(10);
  EXPECT_TRUE(decodificar(s.datos.data(), red480(10), 0.25f).empty());
}

TEST(Posprocesamiento, ConvierteCentroYTamanoAEsquinas) {
  SalidaFalsa s(10);
  s.poner(3, 320, 240, 100, 200, HELMET, 0.9f);
  const auto d = decodificar(s.datos.data(), red480(10), 0.25f);
  ASSERT_EQ(d.size(), 1u);
  EXPECT_EQ(d[0].clase, HELMET);
  EXPECT_FLOAT_EQ(d[0].conf, 0.9f);
  EXPECT_FLOAT_EQ(d[0].x1, 270);
  EXPECT_FLOAT_EQ(d[0].y1, 140);
  EXPECT_FLOAT_EQ(d[0].x2, 370);
  EXPECT_FLOAT_EQ(d[0].y2, 340);
}

TEST(Posprocesamiento, EligeLaClaseDeMayorPuntaje) {
  SalidaFalsa s(4);
  s.poner(0, 100, 100, 50, 50, VEST, 0.6f);
  s.at(4 + NO_VEST, 0) = 0.8f;
  const auto d = decodificar(s.datos.data(), red480(4), 0.25f);
  ASSERT_EQ(d.size(), 1u);
  EXPECT_EQ(d[0].clase, NO_VEST);
}

TEST(Posprocesamiento, DescartaPorDebajoDelUmbral) {
  SalidaFalsa s(4);
  s.poner(0, 100, 100, 50, 50, PERSON, 0.24f);
  s.poner(1, 300, 300, 50, 50, PERSON, 0.25f);
  const auto d = decodificar(s.datos.data(), red480(4), 0.25f);
  ASSERT_EQ(d.size(), 1u);
  EXPECT_FLOAT_EQ(d[0].x1, 275);
}

TEST(Posprocesamiento, RestaElRellenoDeLaRed640x640) {
  SalidaFalsa s(8);
  s.poner(0, 320, 320, 100, 100, PERSON, 0.9f);  // centro de la red 640x640 = centro de la camara
  const auto d = decodificar(s.datos.data(), GeometriaRed{640, 640, 80, 8}, 0.25f);
  ASSERT_EQ(d.size(), 1u);
  EXPECT_FLOAT_EQ(d[0].y1, 190);  // 270 - 80
  EXPECT_FLOAT_EQ(d[0].y2, 290);  // 370 - 80
}

TEST(Posprocesamiento, RecortaAlCuadroYDescartaCajasFuera) {
  SalidaFalsa s(8);
  s.poner(0, 10, 90, 40, 40, PERSON, 0.9f);      // tras restar el relleno se sale por arriba a la izquierda
  s.poner(1, 320, 20, 50, 30, HELMET, 0.9f);     // con red 640x640 queda entera en el relleno
  const auto d = decodificar(s.datos.data(), GeometriaRed{640, 640, 80, 8}, 0.25f);
  ASSERT_EQ(d.size(), 1u);
  EXPECT_EQ(d[0].clase, PERSON);
  EXPECT_FLOAT_EQ(d[0].x1, 0);
  EXPECT_FLOAT_EQ(d[0].y1, 0);
}

TEST(Posprocesamiento, NmsSuprimeDuplicadosDeLaMismaClase) {
  SalidaFalsa s(8);
  s.poner(0, 300, 200, 100, 100, HELMET, 0.70f);
  s.poner(1, 305, 202, 100, 100, HELMET, 0.90f);  // casi la misma caja, mas confianza
  s.poner(2, 500, 200, 100, 100, HELMET, 0.60f);  // otro casco aparte
  const auto d = decodificar(s.datos.data(), red480(8), 0.25f);
  ASSERT_EQ(d.size(), 2u);
  EXPECT_FLOAT_EQ(d[0].conf, 0.90f);  // ordenadas de mayor a menor
  EXPECT_FLOAT_EQ(d[1].conf, 0.60f);
}

TEST(Posprocesamiento, NmsNoMezclaClasesDistintas) {
  SalidaFalsa s(8);
  s.poner(0, 300, 200, 100, 100, HELMET, 0.90f);
  s.poner(1, 300, 200, 100, 100, NO_HELMET, 0.80f);  // misma caja, otra clase: se conservan ambas
  const auto d = decodificar(s.datos.data(), red480(8), 0.25f);
  EXPECT_EQ(d.size(), 2u);
}
