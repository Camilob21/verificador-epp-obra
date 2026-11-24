// config.hpp — configuracion del daemon leida de un archivo de texto (clave = valor).
//
// Permite ajustar umbrales y tiempos SIN recompilar: se edita el archivo y se reinicia el
// servicio. Ejemplo comentado en sistema/epp.conf.example.
//
// Reglas del formato:
//   - una clave por linea:  umbral_prenda = 0.5
//   - '#' inicia un comentario; se ignoran lineas vacias y espacios
//   - una clave desconocida o un valor invalido NO detienen el arranque: se conserva el valor
//     por defecto y se agrega un aviso (asi un error de tipeo nunca deja el sistema sin decidir)
#pragma once
#include <string>
#include <vector>

#include "epp/decidir_multicuadro.hpp"
#include "epp/maquina_estados.hpp"

namespace epp {

struct Config {
  std::string motor = "/home/epp/yolov8n_480x640_fp16.engine";
  std::string camara = "/dev/video0";
  std::string uart;              // vacio = sin ESP32 (solo consola)
  int baudios = 115200;
  float umbral_deteccion = 0.25f;  // umbral al decodificar; la decision usa los suyos
  bool guardar_evidencia = true;   // imagenes resultado_NN_*.ppm
  std::string dir_evidencia = ".";
  double intervalo_telemetria_s = 5.0;
  ConfigDecision decision;
  ConfigMaquina maquina;
};

// Aplica una linea "clave = valor". Devuelve "" si se aplico, o el texto del aviso si no.
std::string aplicarLinea(Config& cfg, const std::string& linea);

// Aplica todas las lineas de un texto. Devuelve los avisos (vacio = todo correcto).
std::vector<std::string> aplicarTexto(Config& cfg, const std::string& texto);

// Lee el archivo y lo aplica sobre cfg. false si el archivo no se pudo abrir.
bool cargarConfig(const std::string& ruta, Config& cfg, std::vector<std::string>& avisos);

// Resumen legible de la configuracion efectiva (para el registro al arrancar).
std::string describirConfig(const Config& cfg);

}  // namespace epp
