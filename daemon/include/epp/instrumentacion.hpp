// instrumentacion.hpp — metricas del sistema leidas de /proc y /sys (requisito del curso:
// CPU, memoria, perifericos, trafico de red y puertos).
//
// Los "analizar*" reciben el TEXTO del archivo y son funciones puras: se prueban en el PC con
// copias reales de esos archivos (tests/test_instrumentacion.cpp). "leerArchivo" es lo unico que
// toca el sistema de archivos.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace epp {

std::string leerArchivo(const std::string& ruta);  // "" si no existe o no se puede leer

// /proc/stat: linea "cpu" agregada
struct MuestraCpu {
  uint64_t ocupado = 0, total = 0;
};
MuestraCpu analizarStat(const std::string& texto);
double porcentajeCpu(const MuestraCpu& antes, const MuestraCpu& despues);  // 0..100, NaN si no hay datos

// /proc/meminfo
struct Memoria {
  uint64_t total_kb = 0, disponible_kb = 0;
};
Memoria analizarMeminfo(const std::string& texto);

// /proc/net/dev
struct Interfaz {
  std::string nombre;
  uint64_t rx_bytes = 0, tx_bytes = 0, rx_paquetes = 0, tx_paquetes = 0;
};
std::vector<Interfaz> analizarNetDev(const std::string& texto);

// /proc/net/tcp y /proc/net/tcp6: puertos en estado LISTEN (0A), sin repetidos, ordenados
std::vector<int> analizarPuertosEscucha(const std::string& texto);

// /proc/uptime: segundos desde el arranque
double analizarUptime(const std::string& texto);

}  // namespace epp
