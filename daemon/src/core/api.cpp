#include "epp/api.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "epp/json.hpp"

namespace epp {

void MuestreadorSistema::actualizar(double t, const Textos& x) {
  const MuestraCpu cpu = analizarStat(x.stat);
  const auto ifaces = analizarNetDev(x.netdev);
  memoria = analizarMeminfo(x.meminfo);
  uptime_s = analizarUptime(x.uptime);

  std::vector<int> p = analizarPuertosEscucha(x.tcp);
  for (int q : analizarPuertosEscucha(x.tcp6)) p.push_back(q);
  std::sort(p.begin(), p.end());
  p.erase(std::unique(p.begin(), p.end()), p.end());
  puertos = p;

  gpu_pct = -1;
  if (!x.gpu_load.empty()) gpu_pct = std::atof(x.gpu_load.c_str()) / 10.0;  // la Jetson da 0..1000

  temperaturas_c.clear();
  for (const auto& tz : x.temperaturas) {
    const double mc = std::atof(tz.second.c_str());
    if (mc > -40000 && mc < 150000) temperaturas_c.emplace_back(tz.first, mc / 1000.0);
  }

  red.clear();
  const double dt = t - t_previo_;
  for (const auto& i : ifaces) {
    if (i.nombre == "lo") continue;
    TasaRed r;
    r.nombre = i.nombre;
    r.rx_bytes = i.rx_bytes;
    r.tx_bytes = i.tx_bytes;
    if (hay_previa_ && dt > 0) {
      for (const auto& a : red_previa_)
        if (a.nombre == i.nombre && i.rx_bytes >= a.rx_bytes && i.tx_bytes >= a.tx_bytes) {
          r.rx_bps = static_cast<double>(i.rx_bytes - a.rx_bytes) / dt;
          r.tx_bps = static_cast<double>(i.tx_bytes - a.tx_bytes) / dt;
        }
    }
    red.push_back(r);
  }
  if (hay_previa_) {
    const double c = porcentajeCpu(cpu_previa_, cpu);
    cpu_pct = std::isfinite(c) ? c : cpu_pct;
  }
  cpu_previa_ = cpu;
  red_previa_ = ifaces;
  t_previo_ = t;
  hay_previa_ = true;
}

std::string jsonEvento(const Evento& e) {
  std::ostringstream o;
  o << "{\"id\":" << e.id << ",\"fecha\":" << jsonTexto(e.fecha) << ",\"hora_confiable\":" << jsonBool(e.hora_confiable)
    << ",\"resultado\":" << jsonTexto(e.resultado) << ",\"falta_casco\":" << jsonBool(e.falta_casco)
    << ",\"falta_chaleco\":" << jsonBool(e.falta_chaleco) << ",\"fraccion_casco\":" << jsonNumero(e.fraccion_casco)
    << ",\"fraccion_chaleco\":" << jsonNumero(e.fraccion_chaleco) << ",\"cuadros\":" << e.cuadros << "}";
  return o.str();
}

std::string jsonEstado(const Instantanea& s) {
  std::ostringstream o;
  o << "{\"t_s\":" << jsonNumero(s.t_s, 1) << ",\"estado\":" << jsonTexto(nombreEstado(s.estado))
    << ",\"luz\":" << jsonTexto(nombreLuz(s.luz)) << ",\"rele\":" << jsonBool(s.rele)
    << ",\"mensaje\":" << jsonTexto(s.mensaje) << ",\"fps\":" << jsonNumero(s.fps, 1)
    << ",\"inferencia_ms\":" << jsonNumero(s.inferencia_ms, 1) << ",\"preproceso_ms\":" << jsonNumero(s.preproceso_ms, 1)
    << ",\"cuadros\":" << s.cuadros << ",\"verdes\":" << s.verdes << ",\"rojos\":" << s.rojos
    << ",\"ultimo\":" << (s.hay_ultimo ? jsonEvento(s.ultimo) : "null") << ",\"esp32\":{\"configurado\":"
    << jsonBool(s.hay_uart) << ",\"enlace\":" << jsonBool(s.enlace_ok) << ",\"pir\":" << jsonBool(s.pir) << "}"
    << ",\"registro\":" << jsonBool(s.registro_activo) << ",\"camara_en_vivo\":" << jsonBool(s.camara_en_vivo)
    << "}";
  return o.str();
}

std::string jsonSistema(const Instantanea& s) {
  const auto& m = s.sistema;
  std::ostringstream o;
  o << "{\"uptime_s\":" << jsonNumero(m.uptime_s, 0)
    << ",\"cpu_pct\":" << (m.cpu_pct < 0 ? "null" : jsonNumero(m.cpu_pct, 1))
    << ",\"gpu_pct\":" << (m.gpu_pct < 0 ? "null" : jsonNumero(m.gpu_pct, 1)) << ",\"memoria\":{\"total_kb\":"
    << m.memoria.total_kb << ",\"disponible_kb\":" << m.memoria.disponible_kb << "},\"temperaturas\":[";
  for (size_t i = 0; i < m.temperaturas_c.size(); ++i)
    o << (i ? "," : "") << "{\"zona\":" << jsonTexto(m.temperaturas_c[i].first)
      << ",\"c\":" << jsonNumero(m.temperaturas_c[i].second, 1) << "}";
  o << "],\"red\":[";
  for (size_t i = 0; i < m.red.size(); ++i) {
    const auto& r = m.red[i];
    o << (i ? "," : "") << "{\"interfaz\":" << jsonTexto(r.nombre) << ",\"rx_bytes\":" << r.rx_bytes
      << ",\"tx_bytes\":" << r.tx_bytes << ",\"rx_bps\":" << jsonNumero(r.rx_bps, 0)
      << ",\"tx_bps\":" << jsonNumero(r.tx_bps, 0) << "}";
  }
  o << "],\"puertos_escucha\":[";
  for (size_t i = 0; i < m.puertos.size(); ++i) o << (i ? "," : "") << m.puertos[i];
  o << "],\"daemon\":{\"fps\":" << jsonNumero(s.fps, 1) << ",\"inferencia_ms\":" << jsonNumero(s.inferencia_ms, 1)
    << ",\"cuadros\":" << s.cuadros << "}";
  // enlace con el ESP32 (contadores del lado Jetson) y telemetria del MCU
  const auto& e = s.enlace;
  o << ",\"enlace\":{\"configurado\":" << jsonBool(s.hay_uart) << ",\"ok\":" << jsonBool(s.enlace_ok)
    << ",\"latidos\":" << e.latidos << ",\"ordenes\":" << e.ordenes << ",\"reintentos\":" << e.reintentos
    << ",\"acks\":" << e.acks << ",\"nacks\":" << e.nacks << ",\"sin_ack\":" << e.sin_ack << ",\"eventos\":" << e.eventos
    << ",\"rx_tramas_ok\":" << s.parser.tramas_ok << ",\"rx_err_crc\":" << s.parser.err_crc
    << ",\"rx_err_longitud\":" << s.parser.err_longitud << ",\"rx_err_timeout\":" << s.parser.err_timeout
    << ",\"rx_bytes_descartados\":" << s.parser.bytes_descartados << "}";
  o << ",\"mcu\":";
  if (!s.hay_telemetria) {
    o << "null";
  } else {
    const auto& t = s.telemetria;
    o << "{\"uptime_s\":" << t.uptime_s << ",\"estado\":" << static_cast<int>(t.estado)
      << ",\"cpu_pct\":" << (t.cpu_pct == PROTO_CPU_NO_DISPONIBLE ? std::string("null") : std::to_string(t.cpu_pct))
      << ",\"heap_libre\":" << t.heap_libre << ",\"heap_min\":" << t.heap_min << ",\"tramas_ok\":" << t.tramas_ok
      << ",\"err_crc\":" << t.err_crc << ",\"err_longitud\":" << t.err_longitud << ",\"err_timeout\":" << t.err_timeout
      << ",\"err_uart\":" << t.err_uart << ",\"fallos_enlace\":" << t.fallos_enlace
      << ",\"rele\":" << jsonBool(t.banderas & PROTO_TEL_RELE) << ",\"stack_libre\":[";
    for (unsigned i = 0; i < PROTO_TEL_N_TAREAS; ++i) o << (i ? "," : "") << t.stack_libre[i];
    o << "]}";
  }
  o << "}";
  return o.str();
}

std::string jsonConfig(const Config& c) {
  std::ostringstream o;
  o << "{\"camara\":" << jsonTexto(c.camara) << ",\"uart\":" << jsonTexto(c.uart) << ",\"exigir_casco\":"
    << jsonBool(c.decision.exigir_casco) << ",\"exigir_chaleco\":" << jsonBool(c.decision.exigir_chaleco)
    << ",\"umbral_persona\":" << jsonNumero(c.decision.umbral_persona) << ",\"umbral_prenda\":"
    << jsonNumero(c.decision.umbral_prenda) << ",\"fraccion_minima\":" << jsonNumero(c.decision.fraccion_minima)
    << ",\"cuadros_minimos\":" << c.decision.cuadros_minimos << ",\"usar_pir\":" << jsonBool(c.maquina.usar_pir)
    << ",\"duracion_resultado_s\":" << jsonNumero(c.maquina.duracion_resultado_s, 1)
    << ",\"camara_en_vivo\":" << jsonBool(c.camara_en_vivo) << "}";
  return o.str();
}

Respuesta atender(const Peticion& p, const Instantanea& s, const Config& c, const FuenteEventos& eventos,
                  const FuenteImagen& imagen) {
  if (!p.valida) return respuestaError(400, "peticion invalida");
  if (p.metodo != "GET") return respuestaError(405, "solo GET");
  Respuesta r;
  if (p.ruta == "/api/estado") {
    r.cuerpo = jsonEstado(s);
  } else if (p.ruta == "/api/sistema") {
    r.cuerpo = jsonSistema(s);
  } else if (p.ruta == "/api/config") {
    r.cuerpo = jsonConfig(c);
  } else if (p.ruta == "/api/camara.jpg") {
    if (!imagen) return respuestaError(404, "vista en vivo desactivada (camara_en_vivo = no en epp.conf)");
    r.cuerpo = imagen();
    if (r.cuerpo.empty()) return respuestaError(503, "todavia no hay imagen");
    r.tipo = "image/jpeg";
  } else if (p.ruta == "/api/eventos") {
    if (!eventos) return respuestaError(503, "registro de eventos desactivado (db vacio en epp.conf)");
    int limite = 50;
    const auto it = p.consulta.find("limite");
    if (it != p.consulta.end()) {
      limite = std::atoi(it->second.c_str());
      if (limite < 1 || limite > 500) return respuestaError(400, "limite debe estar entre 1 y 500");
    }
    std::ostringstream o;
    o << "[";
    const auto lista = eventos(limite);
    for (size_t i = 0; i < lista.size(); ++i) o << (i ? "," : "") << jsonEvento(lista[i]);
    o << "]";
    r.cuerpo = o.str();
  } else {
    return respuestaError(404, "ruta desconocida");
  }
  return r;
}

}  // namespace epp
