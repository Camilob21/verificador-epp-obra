#include "epp/imagen_ppm.hpp"

#include "epp/jpeg.hpp"

#include <cstring>
#include <fstream>

namespace epp {

const uint8_t COLOR_CLASE[NUM_CLASES][3] = {
    {40, 90, 255},    // person     azul
    {0, 200, 0},      // helmet     verde
    {230, 0, 0},      // no_helmet  rojo
    {255, 0, 255},    // vest       magenta (el cian se confundia con el verde al comprimir a JPEG)
    {255, 140, 0},    // no_vest    naranja
    {0, 220, 220},    // goggles    cian
    {255, 120, 180},  // no_goggle  rosado
    {230, 230, 0},    // gloves     amarillo
    {150, 75, 0}};    // no_gloves  cafe

void dibujarCajas(std::vector<uint8_t>& rgb, const Detecciones& dets) {
  auto pixel = [&](int x, int y, const uint8_t* col) {
    if (x < 0 || y < 0 || x >= CAM_W || y >= CAM_H) return;
    std::memcpy(&rgb[(static_cast<size_t>(y) * CAM_W + static_cast<size_t>(x)) * 3], col, 3);
  };
  for (const auto& d : dets) {
    if (d.clase < 0 || d.clase >= NUM_CLASES) continue;
    const uint8_t* col = COLOR_CLASE[d.clase];
    const int x1 = static_cast<int>(d.x1), y1 = static_cast<int>(d.y1);
    const int x2 = static_cast<int>(d.x2) - 1, y2 = static_cast<int>(d.y2) - 1;
    for (int g = 0; g < 3; ++g) {  // borde de 3 px: sobrevive mejor a la compresion JPEG
      for (int x = x1; x <= x2; ++x) {
        pixel(x, y1 + g, col);
        pixel(x, y2 - g, col);
      }
      for (int y = y1; y <= y2; ++y) {
        pixel(x1 + g, y, col);
        pixel(x2 - g, y, col);
      }
    }
  }
}

bool guardarJpeg(const std::string& ruta, std::vector<uint8_t> rgb, const Detecciones& dets, int calidad) {
  if (rgb.size() != static_cast<size_t>(CAM_W) * CAM_H * 3) return false;
  dibujarCajas(rgb, dets);
  const auto jpg = codificarJpeg(rgb, CAM_W, CAM_H, calidad);
  std::ofstream f(ruta, std::ios::binary);
  if (!f) return false;
  f.write(reinterpret_cast<const char*>(jpg.data()), static_cast<std::streamsize>(jpg.size()));
  return static_cast<bool>(f);
}

bool guardarPpm(const std::string& ruta, std::vector<uint8_t> rgb, const Detecciones& dets) {
  dibujarCajas(rgb, dets);
  std::ofstream f(ruta, std::ios::binary);
  if (!f) return false;
  f << "P6\n" << CAM_W << " " << CAM_H << "\n255\n";
  f.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
  return static_cast<bool>(f);
}

bool leerPpm(const std::string& ruta, std::vector<uint8_t>& rgb) {
  std::ifstream f(ruta, std::ios::binary);
  if (!f) return false;
  std::string magia;
  int w = 0, h = 0, maxv = 0;
  f >> magia >> w >> h >> maxv;
  f.get();  // un espacio en blanco tras el encabezado
  if (magia != "P6" || w != CAM_W || h != CAM_H || maxv != 255) return false;
  rgb.resize(static_cast<size_t>(CAM_W) * CAM_H * 3);
  f.read(reinterpret_cast<char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
  return static_cast<size_t>(f.gcount()) == rgb.size();
}

}  // namespace epp
