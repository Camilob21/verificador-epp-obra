#include "epp/imagen_ppm.hpp"

#include <cstring>
#include <fstream>

namespace epp {

bool guardarPpm(const std::string& ruta, std::vector<uint8_t> rgb, const Detecciones& dets) {
  static const uint8_t colores[NUM_CLASES][3] = {{0, 0, 255},   {0, 200, 0},   {255, 0, 0},
                                                 {0, 200, 0},   {255, 0, 0},   {255, 0, 255},
                                                 {255, 128, 0}, {255, 255, 0}, {255, 128, 0}};
  auto pixel = [&](int x, int y, const uint8_t* col) {
    if (x < 0 || y < 0 || x >= CAM_W || y >= CAM_H) return;
    std::memcpy(&rgb[(static_cast<size_t>(y) * CAM_W + static_cast<size_t>(x)) * 3], col, 3);
  };
  for (const auto& d : dets) {
    if (d.clase < 0 || d.clase >= NUM_CLASES) continue;
    const int x1 = static_cast<int>(d.x1), y1 = static_cast<int>(d.y1);
    const int x2 = static_cast<int>(d.x2) - 1, y2 = static_cast<int>(d.y2) - 1;
    for (int g = 0; g < 2; ++g) {
      for (int x = x1; x <= x2; ++x) {
        pixel(x, y1 + g, colores[d.clase]);
        pixel(x, y2 - g, colores[d.clase]);
      }
      for (int y = y1; y <= y2; ++y) {
        pixel(x1 + g, y, colores[d.clase]);
        pixel(x2 - g, y, colores[d.clase]);
      }
    }
  }
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
