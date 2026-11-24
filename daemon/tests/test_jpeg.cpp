// Pruebas del codificador JPEG propio: estructura del archivo y manejo de errores.
// (La fidelidad se verifico aparte decodificando con Pillow: PSNR 37 dB en una imagen real.)
#include <gtest/gtest.h>

#include <set>
#include <stdexcept>

#include "epp/imagen_ppm.hpp"
#include "epp/jpeg.hpp"

using namespace epp;

namespace {
std::vector<uint8_t> imagenPrueba(int w, int h) {
  std::vector<uint8_t> rgb(static_cast<size_t>(w) * static_cast<size_t>(h) * 3);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      uint8_t* p = &rgb[(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3];
      p[0] = static_cast<uint8_t>(x * 255 / w);
      p[1] = static_cast<uint8_t>(y * 255 / h);
      p[2] = static_cast<uint8_t>((x + y) % 256);
    }
  return rgb;
}
// Posicion del marcador FF xx (fuera de los datos comprimidos), o -1
long buscarMarcador(const std::vector<uint8_t>& j, uint8_t m) {
  for (size_t i = 0; i + 1 < j.size(); ++i)
    if (j[i] == 0xFF && j[i + 1] == m) return static_cast<long>(i);
  return -1;
}
}  // namespace

TEST(Jpeg, EmpiezaYTerminaConLosMarcadoresCorrectos) {
  const auto j = codificarJpeg(imagenPrueba(640, 480), 640, 480, 80);
  ASSERT_GT(j.size(), 1000u);
  EXPECT_EQ(j[0], 0xFF);
  EXPECT_EQ(j[1], 0xD8);                // SOI
  EXPECT_EQ(j[j.size() - 2], 0xFF);
  EXPECT_EQ(j[j.size() - 1], 0xD9);     // EOI
}

TEST(Jpeg, CabeceraSof0ConLasDimensiones) {
  const auto j = codificarJpeg(imagenPrueba(64, 32), 64, 32, 80);
  const long sof = buscarMarcador(j, 0xC0);
  ASSERT_GT(sof, 0);
  EXPECT_EQ(j[static_cast<size_t>(sof) + 4], 8);                                   // 8 bits
  EXPECT_EQ((j[static_cast<size_t>(sof) + 5] << 8) | j[static_cast<size_t>(sof) + 6], 32);  // alto
  EXPECT_EQ((j[static_cast<size_t>(sof) + 7] << 8) | j[static_cast<size_t>(sof) + 8], 64);  // ancho
  EXPECT_EQ(j[static_cast<size_t>(sof) + 9], 3);                                   // 3 componentes
}

TEST(Jpeg, LosDatosComprimidosNoTienenMarcadoresSueltos) {
  const auto j = codificarJpeg(imagenPrueba(640, 480), 640, 480, 95);
  const long sos = buscarMarcador(j, 0xDA);
  ASSERT_GT(sos, 0);
  const size_t ini = static_cast<size_t>(sos) + 2 + 12;  // despues del segmento SOS (largo 12)
  for (size_t i = ini; i + 2 < j.size(); ++i)
    if (j[i] == 0xFF) {
      EXPECT_EQ(j[i + 1], 0x00);  // 0xFF sin relleno = archivo corrupto
      ++i;
    }
}

TEST(Jpeg, MasCalidadOcupaMas) {
  const auto img = imagenPrueba(640, 480);
  EXPECT_LT(codificarJpeg(img, 640, 480, 40).size(), codificarJpeg(img, 640, 480, 90).size());
}

TEST(Jpeg, RechazaDimensionesInvalidas) {
  EXPECT_THROW(codificarJpeg(std::vector<uint8_t>(30 * 30 * 3), 30, 30), std::invalid_argument);
  EXPECT_THROW(codificarJpeg(std::vector<uint8_t>(10), 64, 64), std::invalid_argument);
}

TEST(Dibujo, CadaClaseTieneUnColorDistinto) {
  std::set<int> colores;
  for (int c = 0; c < NUM_CLASES; ++c)
    colores.insert(COLOR_CLASE[c][0] << 16 | COLOR_CLASE[c][1] << 8 | COLOR_CLASE[c][2]);
  EXPECT_EQ(colores.size(), static_cast<size_t>(NUM_CLASES));  // antes casco y chaleco compartian verde
}

TEST(Dibujo, PintaElBordeConElColorDeLaClase) {
  std::vector<uint8_t> rgb(static_cast<size_t>(CAM_W) * CAM_H * 3, 0);
  dibujarCajas(rgb, {Deteccion{10, 20, 50, 60, 0.9f, VEST}});
  const uint8_t* esquina = &rgb[(20u * CAM_W + 10u) * 3];
  EXPECT_EQ(esquina[0], COLOR_CLASE[VEST][0]);
  EXPECT_EQ(esquina[1], COLOR_CLASE[VEST][1]);
  EXPECT_EQ(esquina[2], COLOR_CLASE[VEST][2]);
  const uint8_t* centro = &rgb[(40u * CAM_W + 30u) * 3];
  EXPECT_EQ(centro[0], 0);  // el interior no se pinta
}
