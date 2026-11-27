// enlace.hpp — lado Jetson del protocolo UART con el ESP32-C6 (docs/protocolo_uart.md).
//
// Logica pura, sin hilos ni puerto serie: recibe el tiempo y los bytes que llegan, y devuelve
// los bytes que hay que enviar. Asi se prueba completa en el PC (tests/test_enlace.cpp) y el
// programa principal solo la conecta al puerto real (platform/puerto_serie).
//
// Reglas que implementa (las fija el firmware de nuestro companero):
//   - LATIDO cada 200 ms; si el ESP32 deja de recibirlo 800 ms, se pone en ROJO solo (fail-safe).
//   - ESTADO y BUZZER esperan ACK; sin ACK en 100 ms se reenvian con SEQ nuevo (maximo 10 veces).
//     Un NACK "sin enlace" tambien se reintenta: llega si el ESTADO salio antes del primer latido.
//   - EVENTO_BOTON se confirma con ACK siempre (el ESP32 lo repite hasta recibirlo).
//   - Si el ESP32 avisa que reinicio (EVENTO_ARRANQUE) o que recupero el enlace (EVENTO_ENLACE 1),
//     se queda en ROJO: el daemon debe reiniciar su verificacion (nunca retomar un verde viejo).
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "epp/maquina_estados.hpp"
#include "protocolo.h"

namespace epp {

// Luz de la maquina de estados -> estado del semaforo que entiende el ESP32
proto_estado_t estadoDeLuz(Luz luz);

struct EventoEsp32 {
  enum class Tipo { PIR, BOTON, ENLACE_PERDIDO, ENLACE_RESTABLECIDO, ARRANQUE, TELEMETRIA, NACK, SIN_ACK, DISTANCIA } tipo;
  bool presencia = false;   // PIR
  bool pir_listo = false;   // PIR
  uint16_t distancia_mm = 0;      // DISTANCIA (HC-SR04)
  bool distancia_valida = false;  // DISTANCIA: false = sin eco o fuera de alcance
  uint8_t motivo = 0;       // ARRANQUE: motivo de reinicio del ESP32
  uint8_t fw_mayor = 0, fw_menor = 0;
  uint8_t tipo_rechazado = 0, codigo = 0;  // NACK / SIN_ACK (tipo que se dejo de reintentar)
  proto_telemetria_t telemetria{};
};

struct EstadisticasEnlace {
  uint32_t latidos = 0;
  uint32_t ordenes = 0;      // ESTADO + BUZZER enviados la primera vez
  uint32_t reintentos = 0;
  uint32_t acks = 0;
  uint32_t nacks = 0;
  uint32_t sin_ack = 0;      // ordenes abandonadas tras el maximo de reintentos
  uint32_t eventos = 0;
  double ultimo_rx_s = -1;   // instante de la ultima trama valida recibida (-1 = nunca)
};

class EnlaceEsp32 {
 public:
  static constexpr double PERIODO_LATIDO_S = 0.200;
  static constexpr double ESPERA_ACK_S = 0.100;
  static constexpr int MAX_REINTENTOS = 10;

  explicit EnlaceEsp32(double intervalo_telemetria_s = 5.0);

  // Ordenes de la Jetson. Una orden nueva del mismo tipo reemplaza a la que esperaba ACK.
  void ordenarEstado(proto_estado_t estado);
  void ordenarBuzzer(proto_buzzer_t patron);

  // Llamar seguido (cada 10-50 ms). Devuelve los bytes a escribir en el puerto ahora.
  std::vector<uint8_t> tick(double t);

  // Entregar los bytes leidos del puerto. Devuelve los eventos completos que trajeron.
  // Las respuestas que hagan falta (ACK de EVENTO_BOTON) salen en el siguiente tick().
  std::vector<EventoEsp32> recibir(const uint8_t* datos, size_t n, double t);

  // true si se recibio algo valido del ESP32 en los ultimos 'ventana_s' segundos
  bool hayEnlace(double t, double ventana_s = 1.0) const;
  const EstadisticasEnlace& estadisticas() const { return st_; }
  const proto_stats_t& estadisticasParser() const { return parser_.st; }
  proto_estado_t estadoPedido() const { return estado_pedido_; }
  bool estadoConfirmado() const { return estado_confirmado_; }

 private:
  struct Pendiente {
    bool activa = false;
    proto_trama_t trama{};
    uint8_t seq = 0;
    double enviada_s = 0;
    int reintentos = 0;
  };
  static void alRecibir(const proto_trama_t* t, void* ctx);
  void procesar(const proto_trama_t& t);
  void agregar(std::vector<uint8_t>& out, proto_trama_t& tr, uint8_t seq);
  void servirPendiente(Pendiente& p, double t, std::vector<uint8_t>& out);

  proto_parser_t parser_{};
  uint8_t seq_ = 0;
  double ultimo_latido_s_ = -1e9;
  double ultima_telemetria_s_ = -1e9;
  double intervalo_telemetria_s_;
  Pendiente estado_, buzzer_;
  proto_estado_t estado_pedido_ = PROTO_ST_REPOSO;
  bool estado_confirmado_ = false;
  std::vector<proto_trama_t> respuestas_;  // ACK a enviar en el proximo tick
  std::vector<EventoEsp32> eventos_;       // acumulados durante recibir()
  double t_actual_ = 0;
  EstadisticasEnlace st_;
};

}  // namespace epp
