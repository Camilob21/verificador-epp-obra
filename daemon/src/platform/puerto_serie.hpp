// puerto_serie.hpp — puerto serie POSIX en modo crudo 8N1, sin control de flujo.
// Sirve para /dev/ttyTHS1 (UART del conector J41 de la Jetson) y /dev/ttyACM0 (USB del ESP32).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace epp {

class PuertoSerie {
 public:
  // Abre y configura el puerto. Lanza std::runtime_error si falla.
  PuertoSerie(const std::string& dispositivo, int baudios);
  ~PuertoSerie();
  PuertoSerie(const PuertoSerie&) = delete;
  PuertoSerie& operator=(const PuertoSerie&) = delete;

  // Espera hasta 'espera_ms' a que lleguen datos y lee lo disponible (hasta 'max').
  // Devuelve los bytes leidos (0 = no llego nada). Lanza si el puerto desaparece.
  size_t leer(uint8_t* buf, size_t max, int espera_ms);
  // Escribe todo el bloque. Lanza si falla.
  void escribir(const uint8_t* datos, size_t n);

 private:
  int fd_ = -1;
  std::string nombre_;
};

}  // namespace epp
