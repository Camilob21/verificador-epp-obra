// imagen_ppm.hpp — leer/guardar imagenes PPM (P6) de 640x480, sin bibliotecas externas.
// Se usan como evidencia de cada decision y para comparar la Jetson con el PC.
// OJO: estas imagenes pueden mostrar personas; el .gitignore las excluye (*.ppm).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "epp/deteccion.hpp"

namespace epp {

// Guarda el RGB con las cajas dibujadas (borde de 2 px, color por clase). false si no pudo escribir.
bool guardarPpm(const std::string& ruta, std::vector<uint8_t> rgb, const Detecciones& dets);

// Lee un PPM P6 de 640x480 y maximo 255. false si no existe o tiene otro formato.
bool leerPpm(const std::string& ruta, std::vector<uint8_t>& rgb);

}  // namespace epp
