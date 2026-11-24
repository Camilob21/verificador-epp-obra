#include "epp/instrumentacion.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

namespace epp {

std::string leerArchivo(const std::string& ruta) {
  std::ifstream f(ruta);
  if (!f) return "";
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

MuestraCpu analizarStat(const std::string& texto) {
  MuestraCpu m;
  std::istringstream in(texto);
  std::string linea;
  while (std::getline(in, linea)) {
    if (linea.compare(0, 4, "cpu ") != 0) continue;
    std::istringstream l(linea.substr(4));
    // user nice system idle iowait irq softirq steal (guest ya va incluido en user)
    uint64_t v[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (auto& x : v)
      if (!(l >> x)) break;
    const uint64_t inactivo = v[3] + v[4];
    for (auto x : v) m.total += x;
    m.ocupado = m.total - inactivo;
    break;
  }
  return m;
}

double porcentajeCpu(const MuestraCpu& a, const MuestraCpu& b) {
  if (b.total <= a.total || b.ocupado < a.ocupado) return std::numeric_limits<double>::quiet_NaN();
  return 100.0 * static_cast<double>(b.ocupado - a.ocupado) / static_cast<double>(b.total - a.total);
}

Memoria analizarMeminfo(const std::string& texto) {
  Memoria m;
  std::istringstream in(texto);
  std::string clave;
  uint64_t valor;
  std::string unidad;
  std::string linea;
  while (std::getline(in, linea)) {
    std::istringstream l(linea);
    if (!(l >> clave >> valor)) continue;
    if (clave == "MemTotal:") m.total_kb = valor;
    if (clave == "MemAvailable:") m.disponible_kb = valor;
  }
  return m;
}

std::vector<Interfaz> analizarNetDev(const std::string& texto) {
  std::vector<Interfaz> out;
  std::istringstream in(texto);
  std::string linea;
  while (std::getline(in, linea)) {
    const auto dos = linea.find(':');
    if (dos == std::string::npos) continue;  // las dos lineas de cabecera no tienen ':'
    Interfaz i;
    std::istringstream n(linea.substr(0, dos));
    n >> i.nombre;
    std::istringstream l(linea.substr(dos + 1));
    uint64_t c[16] = {0};
    int leidos = 0;
    while (leidos < 16 && (l >> c[leidos])) ++leidos;
    if (leidos < 10) continue;
    i.rx_bytes = c[0];
    i.rx_paquetes = c[1];
    i.tx_bytes = c[8];
    i.tx_paquetes = c[9];
    out.push_back(i);
  }
  return out;
}

std::vector<int> analizarPuertosEscucha(const std::string& texto) {
  std::vector<int> puertos;
  std::istringstream in(texto);
  std::string linea;
  std::getline(in, linea);  // cabecera
  while (std::getline(in, linea)) {
    std::istringstream l(linea);
    std::string sl, local, remoto, estado;
    if (!(l >> sl >> local >> remoto >> estado)) continue;
    if (estado != "0A") continue;  // 0A = TCP_LISTEN
    const auto dos = local.rfind(':');
    if (dos == std::string::npos) continue;
    try {
      puertos.push_back(std::stoi(local.substr(dos + 1), nullptr, 16));
    } catch (...) {
    }
  }
  std::sort(puertos.begin(), puertos.end());
  puertos.erase(std::unique(puertos.begin(), puertos.end()), puertos.end());
  return puertos;
}

double analizarUptime(const std::string& texto) {
  std::istringstream in(texto);
  double s = 0;
  if (!(in >> s)) return std::numeric_limits<double>::quiet_NaN();
  return s;
}

}  // namespace epp
