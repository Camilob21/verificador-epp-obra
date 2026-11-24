#include "epp/maquina_estados.hpp"

#include <cmath>

namespace epp {

const char* nombreEstado(Estado e) {
  switch (e) {
    case Estado::REPOSO: return "REPOSO";
    case Estado::ESPERANDO_PERSONA: return "ESPERANDO_PERSONA";
    case Estado::VERIFICANDO: return "VERIFICANDO";
    case Estado::RESULTADO_VERDE: return "RESULTADO_VERDE";
    case Estado::RESULTADO_ROJO: return "RESULTADO_ROJO";
  }
  return "?";
}

const char* nombreLuz(Luz l) {
  switch (l) {
    case Luz::APAGADO: return "apagado";
    case Luz::AMARILLO_PARPADEO: return "amarillo parpadeando";
    case Luz::AMARILLO_FIJO: return "amarillo fijo";
    case Luz::VERDE: return "verde";
    case Luz::ROJO: return "rojo";
  }
  return "?";
}

MaquinaEstados::MaquinaEstados(const ConfigMaquina& cm, const ConfigDecision& cd) : cfg_(cm), decisor_(cd) {}

bool MaquinaEstados::personaEnPosicion(const Detecciones& dets) const {
  const Deteccion* p = personaPrincipal(dets, decisor_.config().umbral_persona);
  if (!p) return false;
  const bool grande = p->alto() >= cfg_.alto_minimo * CAM_H;
  const bool centrada = std::fabs(p->cx() - 0.5f * CAM_W) <= cfg_.margen_centro * CAM_W;
  return grande && centrada;
}

Luz MaquinaEstados::luzDe(Estado e) const {
  switch (e) {
    case Estado::REPOSO: return Luz::APAGADO;
    case Estado::ESPERANDO_PERSONA: return Luz::AMARILLO_PARPADEO;
    case Estado::VERIFICANDO: return Luz::AMARILLO_FIJO;
    case Estado::RESULTADO_VERDE: return Luz::VERDE;
    case Estado::RESULTADO_ROJO: return Luz::ROJO;
  }
  return Luz::ROJO;
}

void MaquinaEstados::entrar(Estado e, double t) {
  estado_ = e;
  t_entrada_ = t;
  seguidos_en_posicion_ = 0;
  if (e == Estado::VERIFICANDO) decisor_.reiniciar();
}

Salida MaquinaEstados::actualizar(double t, bool pir, const Detecciones& dets) {
  Salida s;
  const Estado antes = estado_;
  const double transcurrido = t - t_entrada_;

  switch (estado_) {
    case Estado::REPOSO:
      if (!cfg_.usar_pir || pir) {
        entrar(Estado::ESPERANDO_PERSONA, t);
        ultimo_mensaje_ = "Parese en la marca, de frente a la camara";
      } else {
        // Con sensor de presencia (HC-SR04 como boton sin contacto) se espera la mano
        ultimo_mensaje_ = "Acerque la mano al sensor para comenzar";
      }
      break;

    case Estado::ESPERANDO_PERSONA:
      seguidos_en_posicion_ = personaEnPosicion(dets) ? seguidos_en_posicion_ + 1 : 0;
      if (seguidos_en_posicion_ >= cfg_.cuadros_en_posicion) {
        entrar(Estado::VERIFICANDO, t);
        ultimo_mensaje_ = "Verificando, quedese quieto";
      } else if (cfg_.usar_pir && transcurrido >= cfg_.espera_max_persona_s) {
        entrar(Estado::REPOSO, t);
        ultimo_mensaje_ = "";
      }
      break;

    case Estado::VERIFICANDO: {
      decisor_.agregarCuadro(dets);
      if (transcurrido >= cfg_.ventana_verificacion_s) {
        const ResultadoVerificacion r = decisor_.resultado();
        if (r.decidido) {
          s.resultado = r;
          if (r.cumple) {
            entrar(Estado::RESULTADO_VERDE, t);
            ultimo_mensaje_ = "Cumple: puede pasar";
          } else {
            entrar(Estado::RESULTADO_ROJO, t);
            std::string m = "Falta:";
            if (r.falta_casco) m += " casco";
            if (r.falta_casco && r.falta_chaleco) m += ",";
            if (r.falta_chaleco) m += " chaleco";
            ultimo_mensaje_ = m;
          }
        } else if (transcurrido >= cfg_.espera_max_verificacion_s) {
          s.resultado = r;
          entrar(Estado::RESULTADO_ROJO, t);
          ultimo_mensaje_ = "No se pudo verificar: parese de frente a la camara";
        }
      }
      break;
    }

    case Estado::RESULTADO_VERDE:
    case Estado::RESULTADO_ROJO:
      if (transcurrido >= cfg_.duracion_resultado_s) {
        entrar(Estado::REPOSO, t);
        ultimo_mensaje_ = "";
      }
      break;
  }

  s.estado = estado_;
  s.luz = luzDe(estado_);
  s.rele = (estado_ == Estado::RESULTADO_VERDE);
  s.cambio = (estado_ != antes);
  s.mensaje = ultimo_mensaje_;
  return s;
}

}  // namespace epp
