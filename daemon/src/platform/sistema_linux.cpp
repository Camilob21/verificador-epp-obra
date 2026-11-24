#include "sistema_linux.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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

int podarArchivos(const std::string& dir, const std::string& prefijo, int maximo) {
  if (maximo <= 0) return 0;
  DIR* d = opendir(dir.c_str());
  if (!d) return 0;
  std::vector<std::pair<long long, std::string>> archivos;  // (mtime en ns, ruta)
  while (dirent* e = readdir(d)) {
    const std::string nombre = e->d_name;
    if (nombre.compare(0, prefijo.size(), prefijo) != 0) continue;
    const std::string ruta = dir + "/" + nombre;
    struct stat st;
    if (stat(ruta.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
    archivos.emplace_back(static_cast<long long>(st.st_mtim.tv_sec) * 1000000000LL + st.st_mtim.tv_nsec, ruta);
  }
  closedir(d);
  if (static_cast<int>(archivos.size()) <= maximo) return 0;
  std::sort(archivos.begin(), archivos.end());  // mas viejos primero
  int borrados = 0;
  const size_t sobran = archivos.size() - static_cast<size_t>(maximo);
  for (size_t i = 0; i < sobran; ++i)
    if (unlink(archivos[i].second.c_str()) == 0) ++borrados;
  return borrados;
}

}  // namespace epp
