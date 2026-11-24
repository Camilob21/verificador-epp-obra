// inferencia_tensorrt.hpp — ejecuta el motor YOLOv8n (.engine) con TensorRT 8.2 en la Jetson.
// El motor se genera EN la Jetson con trtexec (no es portable entre versiones de TensorRT).
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "epp/preprocesamiento.hpp"

namespace epp {

class MotorTensorRT {
 public:
  // Carga el motor y lee la geometria de la red. Acepta entrada 1x3x480x640 o 1x3x640x640
  // y salida 1x13xN. Lanza std::runtime_error si algo no cuadra.
  explicit MotorTensorRT(const std::string& ruta_motor);
  ~MotorTensorRT();
  MotorTensorRT(const MotorTensorRT&) = delete;
  MotorTensorRT& operator=(const MotorTensorRT&) = delete;

  const GeometriaRed& geometria() const { return red_; }
  // entrada: tensor CHW de rgbATensor(). salida: [13][N] para decodificar().
  void inferir(const std::vector<float>& entrada, std::vector<float>& salida);

 private:
  struct Impl;  // oculta los tipos de TensorRT/CUDA a quien incluya este encabezado
  std::unique_ptr<Impl> impl_;
  GeometriaRed red_;
};

}  // namespace epp
