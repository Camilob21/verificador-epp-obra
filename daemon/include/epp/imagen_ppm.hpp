// imagen_ppm.hpp — dibujo de detecciones e imagenes de evidencia (PPM y JPEG), sin bibliotecas externas.
// OJO: estas imagenes pueden mostrar personas; el .gitignore excluye *.ppm y *.jpg.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "epp/deteccion.hpp"

namespace epp {

// Color RGB de cada clase en los dibujos (el dashboard muestra la misma leyenda).
// Casco verde y chaleco magenta: bien distintos incluso tras la compresion JPEG.
extern const uint8_t COLOR_CLASE[NUM_CLASES][3];

// Dibuja en el RGB 640x480 el borde (3 px) de cada caja con el color de su clase.
void dibujarCajas(std::vector<uint8_t>& rgb, const Detecciones& dets);

// Guardan el RGB con las cajas dibujadas. false si no pudo escribir.
bool guardarPpm(const std::string& ruta, std::vector<uint8_t> rgb, const Detecciones& dets);
bool guardarJpeg(const std::string& ruta, std::vector<uint8_t> rgb, const Detecciones& dets, int calidad = 85);

// Lee un PPM P6 de 640x480 y maximo 255. false si no existe o tiene otro formato.
bool leerPpm(const std::string& ruta, std::vector<uint8_t>& rgb);

}  // namespace epp
