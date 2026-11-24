// deteccion.hpp — tipos comunes del verificador de EPP (sin dependencias de la Jetson)
#pragma once
#include <vector>

namespace epp {

constexpr int CAM_W = 640;
constexpr int CAM_H = 480;

// Clases del modelo (mismo orden que epp.yaml del entrenamiento)
enum Clase { PERSON = 0, HELMET, NO_HELMET, VEST, NO_VEST, GOGGLES, NO_GOGGLE, GLOVES, NO_GLOVES, NUM_CLASES };

inline constexpr const char* NOMBRES_CLASE[NUM_CLASES] = {"person",  "helmet",    "no_helmet", "vest",     "no_vest",
                                                          "goggles", "no_goggle", "gloves",    "no_gloves"};

// Caja en pixeles de la imagen de la camara (640x480)
struct Deteccion {
  float x1, y1, x2, y2, conf;
  int clase;
  float ancho() const { return x2 - x1; }
  float alto() const { return y2 - y1; }
  float cx() const { return 0.5f * (x1 + x2); }
  float cy() const { return 0.5f * (y1 + y2); }
};

using Detecciones = std::vector<Deteccion>;

}  // namespace epp
