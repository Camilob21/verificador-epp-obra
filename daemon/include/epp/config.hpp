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
  bool guardar_evidencia = true;   // imagen JPEG de cada decision (evento_<id>_<resultado>.jpg)
  int max_evidencias = 200;        // se borran las mas viejas; 0 = sin limite
  bool camara_en_vivo = false;     // publicar /api/camara.jpg (muestra personas a la red local)
  int calidad_jpeg = 75;
  std::string dir_evidencia = ".";
  double intervalo_telemetria_s = 5.0;
  std::string db;                     // base SQLite de eventos; vacio = sin registro
  std::string api_direccion = "127.0.0.1";  // solo local: el acceso desde la red pasa por nginx
  int api_puerto = 8081;              // 0 = sin API
  // Puesta en marcha (bring-up): mientras este archivo no exista, el equipo no controla el acceso
  // y muestra el asistente. Vacio = sin asistente (equipo de laboratorio).
  std::string marca_puesta_en_marcha;
  // Factory reset: el daemon (usuario epp) crea este archivo al recibir el boton del ESP32 y
  // epp-fabrica.path (root) hace el borrado. Vacio = el boton solo se registra.
  std::string solicitud_fabrica;
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
