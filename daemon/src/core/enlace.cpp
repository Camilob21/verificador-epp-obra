#include "epp/enlace.hpp"

namespace epp {

namespace {
// Tolerancia para comparar tiempos en coma flotante (0,8 - 0,6 da 0,19999... y no 0,2)
constexpr double EPS = 1e-6;
}  // namespace

proto_estado_t estadoDeLuz(Luz luz) {
  switch (luz) {
    case Luz::APAGADO: return PROTO_ST_REPOSO;
    case Luz::AMARILLO_PARPADEO: return PROTO_ST_AMARILLO_PARPADEO;
    case Luz::AMARILLO_FIJO: return PROTO_ST_AMARILLO_FIJO;
    case Luz::VERDE: return PROTO_ST_VERDE;
    case Luz::ROJO: return PROTO_ST_ROJO;
  }
  return PROTO_ST_ROJO;  // ante la duda, rojo
}

EnlaceEsp32::EnlaceEsp32(double intervalo_telemetria_s) : intervalo_telemetria_s_(intervalo_telemetria_s) {
  proto_parser_init(&parser_, &EnlaceEsp32::alRecibir, this);
}

void EnlaceEsp32::ordenarEstado(proto_estado_t estado) {
  proto_msg_estado(&estado_.trama, estado);
  estado_.activa = true;
  estado_.reintentos = 0;
  estado_.enviada_s = -1e9;  // enviar en el proximo tick
  estado_pedido_ = estado;
  estado_confirmado_ = false;
  st_.ordenes++;
}

void EnlaceEsp32::ordenarBuzzer(proto_buzzer_t patron) {
  proto_msg_buzzer(&buzzer_.trama, patron);
  buzzer_.activa = true;
  buzzer_.reintentos = 0;
  buzzer_.enviada_s = -1e9;
  st_.ordenes++;
}

void EnlaceEsp32::agregar(std::vector<uint8_t>& out, proto_trama_t& tr, uint8_t seq) {
  tr.seq = seq;
  uint8_t buf[PROTO_MAX_TRAMA];
  const size_t n = proto_codificar(&tr, buf, sizeof(buf));
  out.insert(out.end(), buf, buf + n);
}

void EnlaceEsp32::servirPendiente(Pendiente& p, double t, std::vector<uint8_t>& out) {
  if (!p.activa || t - p.enviada_s < ESPERA_ACK_S - EPS) return;
  if (p.enviada_s > -1e8) {  // ya se habia enviado: esto es un reintento
    if (p.reintentos >= MAX_REINTENTOS) {
      p.activa = false;
      st_.sin_ack++;
      EventoEsp32 e{EventoEsp32::Tipo::SIN_ACK};
      e.tipo_rechazado = p.trama.tipo;
      eventos_.push_back(e);
      return;
    }
    p.reintentos++;
    st_.reintentos++;
  }
  p.seq = ++seq_;  // SEQ nuevo en cada envio (docs/protocolo_uart.md, regla 6)
  p.enviada_s = t;
  agregar(out, p.trama, p.seq);
}

std::vector<uint8_t> EnlaceEsp32::tick(double t) {
  std::vector<uint8_t> out;
  t_actual_ = t;
  // 1) el latido primero: sin el, el ESP32 rechaza los ESTADO con NACK "sin enlace"
  if (t - ultimo_latido_s_ >= PERIODO_LATIDO_S - EPS) {
    proto_trama_t tr;
    proto_msg_latido(&tr);
    agregar(out, tr, ++seq_);
    ultimo_latido_s_ = t;
    st_.latidos++;
  }
  // 2) respuestas pendientes (ACK de EVENTO_BOTON)
  for (auto& r : respuestas_) agregar(out, r, r.seq);
  respuestas_.clear();
  // 3) ordenes que esperan ACK
  servirPendiente(estado_, t, out);
  servirPendiente(buzzer_, t, out);
  // 4) telemetria periodica (sin ACK: si se pierde, llega la siguiente)
  if (t - ultima_telemetria_s_ >= intervalo_telemetria_s_ - EPS) {
    proto_trama_t tr;
    proto_msg_pedir_telemetria(&tr);
    agregar(out, tr, ++seq_);
    ultima_telemetria_s_ = t;
  }
  return out;
}

void EnlaceEsp32::alRecibir(const proto_trama_t* t, void* ctx) { static_cast<EnlaceEsp32*>(ctx)->procesar(*t); }

void EnlaceEsp32::procesar(const proto_trama_t& t) {
  st_.ultimo_rx_s = t_actual_;
  EventoEsp32 e{EventoEsp32::Tipo::NACK};
  switch (t.tipo) {
    case PROTO_ACK: {
      uint8_t tipo = 0;
      if (!proto_leer_ack(&t, &tipo)) return;
      st_.acks++;
      if (tipo == PROTO_ESTADO && estado_.activa && t.seq == estado_.seq) {
        estado_.activa = false;
        estado_confirmado_ = true;
      } else if (tipo == PROTO_BUZZER && buzzer_.activa && t.seq == buzzer_.seq) {
        buzzer_.activa = false;
      }
      return;  // un ACK viejo (SEQ distinto) se ignora: la orden sigue esperando
    }
    case PROTO_NACK: {
      uint8_t tipo = 0, codigo = 0;
      if (!proto_leer_nack(&t, &tipo, &codigo)) return;
      st_.nacks++;
      e.tipo_rechazado = tipo;
      e.codigo = codigo;
      // Valor o longitud invalidos: reintentar no sirve. Sin enlace: se reintenta (sigue activa).
      if (codigo != PROTO_NACK_SIN_ENLACE) {
        if (tipo == PROTO_ESTADO && t.seq == estado_.seq) estado_.activa = false;
        if (tipo == PROTO_BUZZER && t.seq == buzzer_.seq) buzzer_.activa = false;
      }
      break;
    }
    case PROTO_EVENTO_PIR:
      if (!proto_leer_evento_pir(&t, &e.presencia, &e.pir_listo)) return;
      e.tipo = EventoEsp32::Tipo::PIR;
      break;
    case PROTO_EVENTO_BOTON: {
      if (!proto_leer_evento_boton(&t)) return;
      proto_trama_t ack;
      proto_msg_ack(&ack, t.seq, PROTO_EVENTO_BOTON);
      respuestas_.push_back(ack);
      e.tipo = EventoEsp32::Tipo::BOTON;
      break;
    }
    case PROTO_EVENTO_ENLACE: {
      bool restablecido = false;
      if (!proto_leer_evento_enlace(&t, &restablecido)) return;
      e.tipo = restablecido ? EventoEsp32::Tipo::ENLACE_RESTABLECIDO : EventoEsp32::Tipo::ENLACE_PERDIDO;
      break;
    }
    case PROTO_EVENTO_ARRANQUE:
      if (!proto_leer_arranque(&t, &e.motivo, &e.fw_mayor, &e.fw_menor)) return;
      e.tipo = EventoEsp32::Tipo::ARRANQUE;
      break;
    case PROTO_TELEMETRIA:
      if (!proto_leer_telemetria(&t, &e.telemetria)) return;
      e.tipo = EventoEsp32::Tipo::TELEMETRIA;
      break;
    default:
      return;  // tipos de Jetson->ESP32 o desconocidos: no se esperan aqui
  }
  // Tras un reinicio o una recuperacion, el ESP32 esta en ROJO: lo pedido ya no esta confirmado.
  if (e.tipo == EventoEsp32::Tipo::ARRANQUE || e.tipo == EventoEsp32::Tipo::ENLACE_RESTABLECIDO ||
      e.tipo == EventoEsp32::Tipo::ENLACE_PERDIDO)
    estado_confirmado_ = false;
  st_.eventos++;
  eventos_.push_back(e);
}

std::vector<EventoEsp32> EnlaceEsp32::recibir(const uint8_t* datos, size_t n, double t) {
  t_actual_ = t;
  proto_parser_feed(&parser_, datos, n);
  std::vector<EventoEsp32> out;
  out.swap(eventos_);
  return out;
}

bool EnlaceEsp32::hayEnlace(double t, double ventana_s) const {
  return st_.ultimo_rx_s >= 0 && t - st_.ultimo_rx_s <= ventana_s;
}

}  // namespace epp
