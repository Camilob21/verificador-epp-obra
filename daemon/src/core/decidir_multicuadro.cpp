#include "epp/decidir_multicuadro.hpp"

namespace epp {

const Deteccion* personaPrincipal(const Detecciones& dets, float umbral) {
  const Deteccion* mejor = nullptr;
  float area = 0.0f;
  for (const auto& d : dets) {
    if (d.clase != PERSON || d.conf < umbral) continue;
    const float a = d.ancho() * d.alto();
    if (a > area) {
      area = a;
      mejor = &d;
    }
  }
  return mejor;
}

namespace {
bool dentroHorizontal(const Deteccion& persona, float x) { return x >= persona.x1 && x <= persona.x2; }
}  // namespace

bool cascoEnPersona(const Deteccion& persona, const Detecciones& dets, float umbral) {
  const float h = persona.alto();
  // Cabeza: desde un poco por encima de la caja (el casco sobresale) hasta el 35 % superior
  const float ymin = persona.y1 - 0.10f * h;
  const float ymax = persona.y1 + 0.35f * h;
  for (const auto& d : dets)
    if (d.clase == HELMET && d.conf >= umbral && dentroHorizontal(persona, d.cx()) && d.cy() >= ymin &&
        d.cy() <= ymax)
      return true;
  return false;
}

bool chalecoEnPersona(const Deteccion& persona, const Detecciones& dets, float umbral) {
  const float h = persona.alto();
  // Torso: entre el 15 % y el 90 % del alto de la persona (encuadre del torso hacia arriba)
  const float ymin = persona.y1 + 0.15f * h;
  const float ymax = persona.y1 + 0.90f * h;
  for (const auto& d : dets)
    if (d.clase == VEST && d.conf >= umbral && dentroHorizontal(persona, d.cx()) && d.cy() >= ymin &&
        d.cy() <= ymax)
      return true;
  return false;
}

void Decisor::reiniciar() { con_persona_ = con_casco_ = con_chaleco_ = 0; }

void Decisor::agregarCuadro(const Detecciones& dets) {
  const Deteccion* p = personaPrincipal(dets, cfg_.umbral_persona);
  if (!p) return;  // sin persona, el cuadro no cuenta
  ++con_persona_;
  if (cascoEnPersona(*p, dets, cfg_.umbral_prenda)) ++con_casco_;
  if (chalecoEnPersona(*p, dets, cfg_.umbral_prenda)) ++con_chaleco_;
}

ResultadoVerificacion Decisor::resultado() const {
  ResultadoVerificacion r;
  r.cuadros_con_persona = con_persona_;
  if (con_persona_ < cfg_.cuadros_minimos || con_persona_ == 0) return r;  // no decidido
  r.decidido = true;
  r.fraccion_casco = static_cast<float>(con_casco_) / static_cast<float>(con_persona_);
  r.fraccion_chaleco = static_cast<float>(con_chaleco_) / static_cast<float>(con_persona_);
  r.falta_casco = cfg_.exigir_casco && r.fraccion_casco < cfg_.fraccion_minima;
  r.falta_chaleco = cfg_.exigir_chaleco && r.fraccion_chaleco < cfg_.fraccion_minima;
  r.cumple = !r.falta_casco && !r.falta_chaleco;
  return r;
}

}  // namespace epp
