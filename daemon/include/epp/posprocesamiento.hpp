// posprocesamiento.hpp — de la salida de YOLOv8 a la lista de detecciones
// (C++17 puro: se prueba en el PC con salidas sinteticas)
#pragma once
#include "epp/deteccion.hpp"
#include "epp/preprocesamiento.hpp"

namespace epp {

constexpr int FILAS_SALIDA = 4 + NUM_CLASES;  // cx, cy, w, h + 9 puntajes = 13
constexpr float IOU_NMS = 0.45f;

// Interseccion sobre union de dos cajas (0 si no se tocan)
float iou(const Deteccion& a, const Deteccion& b);

// salida: matriz [13][red.candidatos] en orden de filas.
//   Filas 0-3: cx, cy, w, h en pixeles de la entrada de la red.
//   Filas 4-12: puntaje por clase (el modelo exportado ya aplica la sigmoide).
// Devuelve cajas en pixeles de la camara (resta el relleno y recorta al cuadro 640x480),
// con puntaje >= umbral y NMS por clase (IoU > 0,45 suprime la de menor puntaje).
Detecciones decodificar(const float* salida, const GeometriaRed& red, float umbral);

}  // namespace epp
