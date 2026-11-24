#include "sistema_linux.hpp"

#include <string>

namespace epp {

MuestreadorSistema::Textos leerTextosSistema() {
  MuestreadorSistema::Textos x;
  x.stat = leerArchivo("/proc/stat");
  x.meminfo = leerArchivo("/proc/meminfo");
  x.netdev = leerArchivo("/proc/net/dev");
  x.tcp = leerArchivo("/proc/net/tcp");
  x.tcp6 = leerArchivo("/proc/net/tcp6");
  x.uptime = leerArchivo("/proc/uptime");
  x.gpu_load = leerArchivo("/sys/devices/gpu.0/load");  // solo existe en la Jetson
  for (int i = 0; i < 10; ++i) {
    const std::string base = "/sys/class/thermal/thermal_zone" + std::to_string(i);
    std::string tipo = leerArchivo(base + "/type");
    if (tipo.empty()) break;
    while (!tipo.empty() && (tipo.back() == '\n' || tipo.back() == ' ')) tipo.pop_back();
    x.temperaturas.emplace_back(tipo, leerArchivo(base + "/temp"));
  }
  return x;
}

}  // namespace epp
