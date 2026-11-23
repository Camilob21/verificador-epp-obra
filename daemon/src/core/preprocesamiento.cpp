#include "epp/preprocesamiento.hpp"

#include "epp/deteccion.hpp"

namespace epp {

namespace {
inline uint8_t saturar(int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }
}  // namespace

void yuyvARgb(const std::vector<uint8_t>& yuyv, std::vector<uint8_t>& rgb) {
  rgb.resize(static_cast<size_t>(CAM_W) * CAM_H * 3);
  const size_t pares = static_cast<size_t>(CAM_W) * CAM_H / 2;
  for (size_t i = 0; i < pares; ++i) {
    const int y0 = yuyv[4 * i] - 16, u = yuyv[4 * i + 1] - 128;
    const int y1 = yuyv[4 * i + 2] - 16, v = yuyv[4 * i + 3] - 128;
    const int ys[2] = {y0, y1};
    for (int k = 0; k < 2; ++k) {
      const int c = 298 * ys[k];
      uint8_t* p = &rgb[(2 * i + static_cast<size_t>(k)) * 3];
      p[0] = saturar((c + 409 * v + 128) >> 8);
      p[1] = saturar((c - 100 * u - 208 * v + 128) >> 8);
      p[2] = saturar((c + 516 * u + 128) >> 8);
    }
  }
}

void rgbATensor(const std::vector<uint8_t>& rgb, const GeometriaRed& red, std::vector<float>& tensor) {
  const size_t plano = static_cast<size_t>(red.alto) * static_cast<size_t>(red.ancho);
  tensor.assign(3 * plano, 114.0f / 255.0f);
  for (int y = 0; y < CAM_H; ++y) {
    const uint8_t* fila = &rgb[static_cast<size_t>(y) * CAM_W * 3];
    const size_t base = static_cast<size_t>(y + red.relleno_arriba) * static_cast<size_t>(red.ancho);
    for (int x = 0; x < CAM_W; ++x)
      for (int c = 0; c < 3; ++c)
        tensor[static_cast<size_t>(c) * plano + base + static_cast<size_t>(x)] = fila[3 * x + c] / 255.0f;
  }
}

}  // namespace epp
