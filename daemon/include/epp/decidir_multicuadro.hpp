// decidir_multicuadro.hpp — decision por prenda usando varios cuadros
//
// Regla (medida en la prueba B): una prenda cuenta como presente en un cuadro solo si
//   - su confianza es >= umbral_prenda (0,5), y
//   - esta en la zona correcta de la persona principal (casco en la cabeza, chaleco en el torso).
// Al final de la ventana, la prenda se da por puesta si aparecio en al menos fraccion_minima
// de los cuadros donde se vio a la persona. Ante la duda (pocos cuadros), no se decide -> rojo.
#pragma once
#include "epp/deteccion.hpp"

namespace epp {

struct ConfigDecision {
  float umbral_persona = 0.40f;   // la persona sale con menos confianza (p. ej. 0,47 con ropa blanca)
  float umbral_prenda = 0.50f;
  float fraccion_minima = 0.60f;  // de los cuadros con persona
  int cuadros_minimos = 8;        // cuadros con persona necesarios para poder decidir
  bool exigir_casco = true;
  bool exigir_chaleco = true;
};

struct ResultadoVerificacion {
  bool decidido = false;  // false: no hubo suficientes cuadros con persona
  bool cumple = false;
  bool falta_casco = false;
  bool falta_chaleco = false;
  int cuadros_con_persona = 0;
  float fraccion_casco = 0.0f;
  float fraccion_chaleco = 0.0f;
};

// Persona principal: la de mayor area con confianza >= umbral. nullptr si no hay.
const Deteccion* personaPrincipal(const Detecciones& dets, float umbral);

// ¿Hay un casco (>= umbral) en la zona de la cabeza de la persona?
bool cascoEnPersona(const Deteccion& persona, const Detecciones& dets, float umbral);

// ¿Hay un chaleco (>= umbral) en la zona del torso de la persona?
bool chalecoEnPersona(const Deteccion& persona, const Detecciones& dets, float umbral);

class Decisor {
 public:
  explicit Decisor(const ConfigDecision& cfg = ConfigDecision{}) : cfg_(cfg) {}
  void reiniciar();
  void agregarCuadro(const Detecciones& dets);
  ResultadoVerificacion resultado() const;
  int cuadrosConPersona() const { return con_persona_; }
  const ConfigDecision& config() const { return cfg_; }

 private:
  ConfigDecision cfg_;
  int con_persona_ = 0;
  int con_casco_ = 0;
  int con_chaleco_ = 0;
};

}  // namespace epp
