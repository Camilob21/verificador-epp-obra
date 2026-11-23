// preprocesamiento.hpp — de la imagen de la camara al tensor de entrada del modelo
// (C++17 puro: compila y se prueba en cualquier PC, sin CUDA ni V4L2)
#pragma once
#include <cstdint>
#include <vector>

namespace epp {

// Geometria de la red: se lee del motor TensorRT al cargarlo.
//   motor 480x640 -> sin relleno (coincide con la camara), 6300 cajas candidatas  <- el que usamos
//   motor 640x640 -> 80 px de relleno arriba y abajo, 8400 cajas candidatas
struct GeometriaRed {
  int alto = 480;
  int ancho = 640;
  int relleno_arriba = 0;
  int candidatos = 6300;
};

// YUYV 640x480 (BT.601, rango limitado, como reporta v4l2-ctl) -> RGB intercalado 640x480
void yuyvARgb(const std::vector<uint8_t>& yuyv, std::vector<uint8_t>& rgb);

// RGB 640x480 -> tensor CHW float [0,1] del tamano de la red. Si la red es mas alta que la
// camara, rellena con gris 114 (el mismo valor que usa Ultralytics) arriba y abajo.
void rgbATensor(const std::vector<uint8_t>& rgb, const GeometriaRed& red, std::vector<float>& tensor);

}  // namespace epp
