#include "epp/config.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>

namespace epp {

namespace {

std::string recortar(const std::string& s) {
  const auto ini = s.find_first_not_of(" \t\r\n");
  if (ini == std::string::npos) return "";
  const auto fin = s.find_last_not_of(" \t\r\n");
  return s.substr(ini, fin - ini + 1);
}

bool aDouble(const std::string& v, double& out) {
  if (v.empty()) return false;
  errno = 0;
  char* fin = nullptr;
  const double d = std::strtod(v.c_str(), &fin);
  if (errno != 0 || fin == v.c_str() || *fin != '\0') return false;
  out = d;
  return true;
}

bool aEntero(const std::string& v, long& out) {
  if (v.empty()) return false;
  errno = 0;
  char* fin = nullptr;
  const long n = std::strtol(v.c_str(), &fin, 10);
  if (errno != 0 || fin == v.c_str() || *fin != '\0') return false;
  out = n;
  return true;
}

bool aBool(const std::string& v, bool& out) {
  if (v == "1" || v == "si" || v == "true" || v == "on") {
    out = true;
    return true;
  }
  if (v == "0" || v == "no" || v == "false" || v == "off") {
    out = false;
    return true;
  }
  return false;
}

// Cada clave sabe validar su valor; devuelve "" si lo aplico o el motivo del rechazo.
using Aplicador = std::function<std::string(Config&, const std::string&)>;

Aplicador fraccion(float ConfigDecision::*campo, double min, double max) {
  return [=](Config& c, const std::string& v) -> std::string {
    double d;
    if (!aDouble(v, d) || d < min || d > max) return "debe ser un numero entre " + std::to_string(min) + " y " + std::to_string(max);
    c.decision.*campo = static_cast<float>(d);
    return "";
  };
}

Aplicador fraccionM(float ConfigMaquina::*campo, double min, double max) {
  return [=](Config& c, const std::string& v) -> std::string {
    double d;
    if (!aDouble(v, d) || d < min || d > max) return "debe ser un numero entre " + std::to_string(min) + " y " + std::to_string(max);
    c.maquina.*campo = static_cast<float>(d);
    return "";
  };
}

Aplicador segundos(double ConfigMaquina::*campo, double min, double max) {
  return [=](Config& c, const std::string& v) -> std::string {
    double d;
    if (!aDouble(v, d) || d < min || d > max) return "debe ser un tiempo en segundos entre " + std::to_string(min) + " y " + std::to_string(max);
    c.maquina.*campo = d;
    return "";
  };
}

Aplicador texto(std::string Config::*campo) {
  return [=](Config& c, const std::string& v) -> std::string {
    c.*campo = v;
    return "";
  };
}

const std::map<std::string, Aplicador>& tabla() {
  static const std::map<std::string, Aplicador> t = {
      {"motor", texto(&Config::motor)},
      {"camara", texto(&Config::camara)},
      {"uart", texto(&Config::uart)},
      {"dir_evidencia", texto(&Config::dir_evidencia)},
      {"db", texto(&Config::db)},
      {"api_direccion",
       [](Config& c, const std::string& v) -> std::string {
         // solo IPv4 en forma de cuatro numeros; la validacion fina la hace inet_pton al abrir
         int a, b, d, e;
         char extra;
         if (std::sscanf(v.c_str(), "%d.%d.%d.%d%c", &a, &b, &d, &e, &extra) != 4) return "debe ser una IPv4, p. ej. 127.0.0.1";
         c.api_direccion = v;
         return "";
       }},
      {"api_puerto",
       [](Config& c, const std::string& v) -> std::string {
         long n;
         if (!aEntero(v, n) || n < 0 || n > 65535) return "debe ser un puerto entre 0 y 65535 (0 = sin API)";
         c.api_puerto = static_cast<int>(n);
         return "";
       }},
      {"baudios",
       [](Config& c, const std::string& v) -> std::string {
         long n;
         if (!aEntero(v, n) || (n != 9600 && n != 57600 && n != 115200 && n != 230400 && n != 460800 && n != 921600))
           return "debe ser 9600, 57600, 115200, 230400, 460800 o 921600";
         c.baudios = static_cast<int>(n);
         return "";
       }},
      {"umbral_deteccion",
       [](Config& c, const std::string& v) -> std::string {
         double d;
         if (!aDouble(v, d) || d < 0.05 || d > 0.95) return "debe ser un numero entre 0.05 y 0.95";
         c.umbral_deteccion = static_cast<float>(d);
         return "";
       }},
      {"guardar_evidencia",
       [](Config& c, const std::string& v) -> std::string {
         bool b;
         if (!aBool(v, b)) return "debe ser si/no";
         c.guardar_evidencia = b;
         return "";
       }},
      {"camara_en_vivo",
       [](Config& c, const std::string& v) -> std::string {
         bool b;
         if (!aBool(v, b)) return "debe ser si/no";
         c.camara_en_vivo = b;
         return "";
       }},
      {"max_evidencias",
       [](Config& c, const std::string& v) -> std::string {
         long n;
         if (!aEntero(v, n) || n < 0 || n > 100000) return "debe ser un entero entre 0 y 100000 (0 = sin limite)";
         c.max_evidencias = static_cast<int>(n);
         return "";
       }},
      {"calidad_jpeg",
       [](Config& c, const std::string& v) -> std::string {
         long n;
         if (!aEntero(v, n) || n < 30 || n > 95) return "debe ser un entero entre 30 y 95";
         c.calidad_jpeg = static_cast<int>(n);
         return "";
       }},
      {"intervalo_telemetria_s",
       [](Config& c, const std::string& v) -> std::string {
         double d;
         if (!aDouble(v, d) || d < 1.0 || d > 600.0) return "debe ser un tiempo entre 1 y 600 s";
         c.intervalo_telemetria_s = d;
         return "";
       }},
      {"umbral_persona", fraccion(&ConfigDecision::umbral_persona, 0.05, 0.95)},
      {"umbral_prenda", fraccion(&ConfigDecision::umbral_prenda, 0.05, 0.95)},
      {"fraccion_minima", fraccion(&ConfigDecision::fraccion_minima, 0.10, 1.00)},
      {"cuadros_minimos",
       [](Config& c, const std::string& v) -> std::string {
         long n;
         if (!aEntero(v, n) || n < 1 || n > 200) return "debe ser un entero entre 1 y 200";
         c.decision.cuadros_minimos = static_cast<int>(n);
         return "";
       }},
      {"exigir_casco",
       [](Config& c, const std::string& v) -> std::string {
         bool b;
         if (!aBool(v, b)) return "debe ser si/no";
         c.decision.exigir_casco = b;
         return "";
       }},
      {"exigir_chaleco",
       [](Config& c, const std::string& v) -> std::string {
         bool b;
         if (!aBool(v, b)) return "debe ser si/no";
         c.decision.exigir_chaleco = b;
         return "";
       }},
      {"usar_pir",
       [](Config& c, const std::string& v) -> std::string {
         bool b;
         if (!aBool(v, b)) return "debe ser si/no";
         c.maquina.usar_pir = b;
         return "";
       }},
      {"alto_minimo", fraccionM(&ConfigMaquina::alto_minimo, 0.05, 1.00)},
      {"margen_centro", fraccionM(&ConfigMaquina::margen_centro, 0.05, 0.50)},
      {"cuadros_en_posicion",
       [](Config& c, const std::string& v) -> std::string {
         long n;
         if (!aEntero(v, n) || n < 1 || n > 100) return "debe ser un entero entre 1 y 100";
         c.maquina.cuadros_en_posicion = static_cast<int>(n);
         return "";
       }},
      {"ventana_verificacion_s", segundos(&ConfigMaquina::ventana_verificacion_s, 0.2, 30.0)},
      {"espera_max_verificacion_s", segundos(&ConfigMaquina::espera_max_verificacion_s, 0.5, 60.0)},
      {"espera_max_persona_s", segundos(&ConfigMaquina::espera_max_persona_s, 1.0, 600.0)},
      {"cuenta_regresiva_s",
       [](Config& c, const std::string& v) -> std::string {
         double d;
         if (!aDouble(v, d) || d < 0.0 || d > 60.0) return "debe ser un tiempo entre 0 y 60 s (0 = criterio de encuadre)";
         c.maquina.cuenta_regresiva_s = d;
         return "";
       }},
      // El ESP32 corta el verde a los 8 s pase lo que pase; por eso el maximo es 8.
      {"duracion_resultado_s", segundos(&ConfigMaquina::duracion_resultado_s, 1.0, 8.0)},
  };
  return t;
}

}  // namespace

std::string aplicarLinea(Config& cfg, const std::string& linea_original) {
  std::string linea = linea_original;
  const auto com = linea.find('#');
  if (com != std::string::npos) linea.erase(com);
  linea = recortar(linea);
  if (linea.empty()) return "";
  const auto igual = linea.find('=');
  if (igual == std::string::npos) return "linea sin '=': \"" + linea + "\"";
  const std::string clave = recortar(linea.substr(0, igual));
  const std::string valor = recortar(linea.substr(igual + 1));
  const auto& t = tabla();
  const auto it = t.find(clave);
  if (it == t.end()) return "clave desconocida \"" + clave + "\" (se ignora)";
  const std::string error = it->second(cfg, valor);
  if (!error.empty()) return clave + " = \"" + valor + "\": " + error + "; se conserva el valor por defecto";
  return "";
}

std::vector<std::string> aplicarTexto(Config& cfg, const std::string& texto) {
  std::vector<std::string> avisos;
  std::istringstream in(texto);
  std::string linea;
  int n = 0;
  while (std::getline(in, linea)) {
    ++n;
    const std::string aviso = aplicarLinea(cfg, linea);
    if (!aviso.empty()) avisos.push_back("linea " + std::to_string(n) + ": " + aviso);
  }
  // Coherencia entre claves: un tiempo maximo menor que la ventana nunca dejaria decidir.
  if (cfg.maquina.espera_max_verificacion_s < cfg.maquina.ventana_verificacion_s) {
    avisos.push_back("espera_max_verificacion_s < ventana_verificacion_s: se iguala a la ventana");
    cfg.maquina.espera_max_verificacion_s = cfg.maquina.ventana_verificacion_s;
  }
  if (!cfg.decision.exigir_casco && !cfg.decision.exigir_chaleco)
    avisos.push_back("ATENCION: no se exige ninguna prenda; cualquier persona en posicion dara verde");
  return avisos;
}

bool cargarConfig(const std::string& ruta, Config& cfg, std::vector<std::string>& avisos) {
  std::ifstream f(ruta);
  if (!f) return false;
  std::stringstream ss;
  ss << f.rdbuf();
  avisos = aplicarTexto(cfg, ss.str());
  return true;
}

std::string describirConfig(const Config& c) {
  std::ostringstream o;
  o << "motor=" << c.motor << " camara=" << c.camara << " uart=" << (c.uart.empty() ? "(sin ESP32)" : c.uart)
    << " baudios=" << c.baudios << "\n"
    << "registro: db=" << (c.db.empty() ? "(sin registro)" : c.db) << " api="
    << (c.api_puerto ? c.api_direccion + ":" + std::to_string(c.api_puerto) : std::string("(sin API)"))
    << " camara_en_vivo=" << (c.camara_en_vivo ? "si" : "no") << " evidencias=" << (c.guardar_evidencia ? "si" : "no")
    << " (max " << c.max_evidencias << ")\n"
    << "decision: persona>=" << c.decision.umbral_persona << " prenda>=" << c.decision.umbral_prenda
    << " fraccion>=" << c.decision.fraccion_minima << " cuadros>=" << c.decision.cuadros_minimos
    << " casco=" << (c.decision.exigir_casco ? "si" : "no") << " chaleco=" << (c.decision.exigir_chaleco ? "si" : "no")
    << "\n"
    << "maquina: pir=" << (c.maquina.usar_pir ? "si" : "no") << " alto>=" << c.maquina.alto_minimo
    << " margen=" << c.maquina.margen_centro << " ventana=" << c.maquina.ventana_verificacion_s
    << "s max_verif=" << c.maquina.espera_max_verificacion_s << "s max_persona=" << c.maquina.espera_max_persona_s
    << "s resultado=" << c.maquina.duracion_resultado_s << "s cuenta_regresiva=" << c.maquina.cuenta_regresiva_s
    << "s";
  return o.str();
}

}  // namespace epp
