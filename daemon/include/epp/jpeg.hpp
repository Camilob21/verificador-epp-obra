// jpeg.hpp — codificador JPEG baseline propio (sin libjpeg): RGB 4:4:4, tablas estandar del
// anexo K de la norma ITU-T T.81. Sirve para la vista en vivo del dashboard y las evidencias:
// una imagen 640x480 pasa de 900 KB (PPM) a ~30-60 KB.
//
// Por que propio: evita una dependencia mas en la Jetson y en la CI, y se puede verificar
// decodificando la salida con cualquier visor (tests/test_jpeg.cpp revisa la estructura; la
// fidelidad se comprobo decodificando con Pillow: PSNR > 30 dB con calidad 80).
#pragma once
#include <cstdint>
#include <vector>

namespace epp {

// rgb: ancho*alto*3 bytes intercalados. ancho y alto deben ser multiplos de 8 (640x480 lo es).
// calidad: 1..100 (75-85 es un buen compromiso). Devuelve el archivo JPEG completo.
std::vector<uint8_t> codificarJpeg(const std::vector<uint8_t>& rgb, int ancho, int alto, int calidad = 80);

}  // namespace epp
