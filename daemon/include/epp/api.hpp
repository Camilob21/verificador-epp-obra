// api.hpp — contenido de la API local del daemon (JSON para el dashboard).
//
//   GET /api/estado    estado actual del punto de control, enlace con el ESP32, rendimiento
//   GET /api/eventos   ultimas decisiones registradas (?limite=N, por defecto 50, maximo 500)
//   GET /api/sistema   instrumentacion: CPU, GPU, memoria, temperaturas, red, puertos, MCU
//   GET /api/config    configuracion efectiva (solo lectura)
//
// Todo es puro (sin sockets): la "Instantanea" la llena el programa principal y aqui solo se
// convierte a JSON. Asi la API se prueba en el PC (tests/test_api.cpp).
#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "epp/config.hpp"
#include "epp/enlace.hpp"
#include "epp/http.hpp"
#include "epp/instrumentacion.hpp"
#include "epp/maquina_estados.hpp"

namespace epp {

// Una decision registrada (tabla "eventos" de SQLite)
struct Evento {
  long long id = 0;
  std::string fecha;          // ISO 8601 local, p. ej. "2026-11-24T14:30:05-05:00"
  bool hora_confiable = true; // false si el reloj no estaba sincronizado al registrar
  std::string resultado;      // "verde", "rojo" o "no_verificado"
  bool falta_casco = false, falta_chaleco = false;
  double fraccion_casco = 0, fraccion_chaleco = 0;
  int cuadros = 0;
};

struct TasaRed {
  std::string nombre;
  uint64_t rx_bytes = 0, tx_bytes = 0;
  double rx_bps = 0, tx_bps = 0;  // bytes por segundo, promedio del ultimo intervalo
};

// Metricas del sistema que el programa principal muestrea cada ~1 s
class MuestreadorSistema {
 public:
  struct Textos {
    std::string stat, meminfo, netdev, tcp, tcp6, uptime, gpu_load;
    std::vector<std::pair<std::string, std::string>> temperaturas;  // (tipo, milligrados)
  };
  void actualizar(double t, const Textos& x);

  double cpu_pct = -1;   // -1 = aun sin dos muestras
  double gpu_pct = -1;   // -1 = no disponible (fuera de la Jetson)
  Memoria memoria;
  std::vector<TasaRed> red;
  std::vector<int> puertos;
  std::vector<std::pair<std::string, double>> temperaturas_c;
  double uptime_s = 0;

 private:
  bool hay_previa_ = false;
  double t_previo_ = 0;
  MuestraCpu cpu_previa_;
  std::vector<Interfaz> red_previa_;
};

// Todo lo que la API puede mostrar, copiado bajo candado por el hilo principal
struct Instantanea {
  double t_s = 0;  // segundos desde que arranco el daemon
  Estado estado = Estado::REPOSO;
  Luz luz = Luz::APAGADO;
  bool rele = false;
  std::string mensaje;
  double fps = 0, inferencia_ms = 0, preproceso_ms = 0;
  unsigned long long cuadros = 0;
  int verdes = 0, rojos = 0;
  bool hay_ultimo = false;
  Evento ultimo;
  // enlace con el ESP32
  bool hay_uart = false;
  bool enlace_ok = false;
  EstadisticasEnlace enlace;
  proto_stats_t parser{};
  bool hay_telemetria = false;
  proto_telemetria_t telemetria{};
  bool pir = false;
  // sistema
  MuestreadorSistema sistema;
  bool registro_activo = false;
};

using FuenteEventos = std::function<std::vector<Evento>(int limite)>;

std::string jsonEvento(const Evento& e);
std::string jsonEstado(const Instantanea& s);
std::string jsonSistema(const Instantanea& s);
std::string jsonConfig(const Config& c);

// Enruta una peticion. 'eventos' puede ser nulo (sin base de datos).
Respuesta atender(const Peticion& p, const Instantanea& s, const Config& c, const FuenteEventos& eventos);

}  // namespace epp
