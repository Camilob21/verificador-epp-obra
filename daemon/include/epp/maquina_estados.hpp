// maquina_estados.hpp — flujo del punto de control
//
//   REPOSO --(PIR, o siempre si no hay PIR)--> ESPERANDO_PERSONA
//   ESPERANDO_PERSONA --(persona grande y centrada N cuadros seguidos)--> VERIFICANDO
//   ESPERANDO_PERSONA --(tiempo maximo, solo con PIR)--> REPOSO
//   VERIFICANDO --(ventana completa y decidido)--> RESULTADO_VERDE / RESULTADO_ROJO
//   VERIFICANDO --(tiempo maximo sin cuadros suficientes)--> RESULTADO_ROJO ("no se pudo verificar")
//   RESULTADO_* --(duracion del resultado)--> REPOSO
//
// El tiempo se pasa como argumento (segundos) para poder probar la maquina sin reloj real.
#pragma once
#include <optional>
#include <string>

#include "epp/decidir_multicuadro.hpp"

namespace epp {

enum class Estado { REPOSO, ESPERANDO_PERSONA, VERIFICANDO, RESULTADO_VERDE, RESULTADO_ROJO };
enum class Luz { APAGADO, AMARILLO_PARPADEO, AMARILLO_FIJO, VERDE, ROJO };

const char* nombreEstado(Estado e);
const char* nombreLuz(Luz l);

struct ConfigMaquina {
  bool usar_pir = false;               // sin ESP32 todavia: se asume presencia siempre
  float alto_minimo = 0.45f;           // alto de la caja person / alto de la imagen para "en posicion"
  float margen_centro = 0.30f;         // el centro debe estar en [0,5 +- margen] del ancho
  int cuadros_en_posicion = 5;         // cuadros seguidos en posicion para empezar a verificar
  double ventana_verificacion_s = 1.5; // tiempo minimo acumulando cuadros
  double espera_max_verificacion_s = 4.0;
  double espera_max_persona_s = 15.0;  // solo con PIR
  double duracion_resultado_s = 5.0;
};

struct Salida {
  Estado estado = Estado::REPOSO;
  Luz luz = Luz::APAGADO;
  bool rele = false;  // solo se activa en RESULTADO_VERDE
  bool cambio = false;
  std::string mensaje;
  std::optional<ResultadoVerificacion> resultado;  // solo al entrar a un resultado
};

class MaquinaEstados {
 public:
  MaquinaEstados(const ConfigMaquina& cm = ConfigMaquina{}, const ConfigDecision& cd = ConfigDecision{});
  Salida actualizar(double t, bool pir, const Detecciones& dets);
  Estado estado() const { return estado_; }

  // Criterio "persona en posicion" (publico para probarlo)
  bool personaEnPosicion(const Detecciones& dets) const;

 private:
  void entrar(Estado e, double t);
  Luz luzDe(Estado e) const;

  ConfigMaquina cfg_;
  Decisor decisor_;
  Estado estado_ = Estado::REPOSO;
  double t_entrada_ = 0.0;
  int seguidos_en_posicion_ = 0;
  std::string ultimo_mensaje_;
};

}  // namespace epp
