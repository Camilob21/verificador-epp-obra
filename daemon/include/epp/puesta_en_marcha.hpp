// puesta_en_marcha.hpp — asistente de puesta en marcha (bring-up) del punto de control.
//
// Un equipo recien instalado, o recien restaurado con el factory reset, NO controla el acceso:
// primero quien lo instala comprueba que todo funciona y lo confirma con la mano en el sensor.
//
//   Comprobaciones obligatorias           Comprobacion informativa
//   - camara: llegan cuadros               - hora: el reloj se sincronizo por la red
//   - esp32:  hay enlace (latido y ACK)      (sin internet no se puede; no bloquea, pero el
//   - sensor: detecto la mano al menos       dashboard avisa que las fechas no son confiables)
//             una vez
//
//   Confirmacion: con camara y ESP32 listos, mantener la mano frente al sensor
//   'segundos_confirmar' seguidos. Si se retira antes, el contador vuelve a cero.
//
// Mientras dure, el rele queda apagado (el programa principal no ejecuta la maquina de estados)
// y el semaforo muestra amarillo fijo. Al completar, el programa principal crea el archivo
// marca (p. ej. /var/lib/epp/puesta_en_marcha.ok); el factory reset lo borra.
//
// Logica pura, sin archivos ni hilos: se prueba en el PC (tests/test_puesta_en_marcha.cpp).
#pragma once
#include <string>

namespace epp {

struct EntradasPuesta {
  bool camara = false;     // la camara entrega cuadros
  bool esp32 = false;      // enlace vivo con el ESP32
  bool presencia = false;  // el sensor ve la mano (5-40 cm)
  bool hora = false;       // la hora se sincronizo por la red en este arranque
};

struct EstadoPuesta {
  bool camara = false, esp32 = false, sensor = false, hora = false;
  double confirmacion_s = 0;  // tiempo que lleva la mano puesta (cuenta hacia segundos_confirmar)
  double confirmacion_total_s = 0;
  bool completa = false;
  std::string mensaje;
};

class PuestaEnMarcha {
 public:
  explicit PuestaEnMarcha(double segundos_confirmar = 3.0);
  // Llamar en cada cuadro con el tiempo t (s, monotono). Una vez completa, sigue completa.
  EstadoPuesta actualizar(double t, const EntradasPuesta& e);

 private:
  double segundos_;
  bool sensor_visto_ = false;
  bool mano_puesta_ = false;
  double t_mano_ = 0;
  bool completa_ = false;
};

}  // namespace epp
