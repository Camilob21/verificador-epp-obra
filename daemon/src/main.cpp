// epp-daemon — programa de la Jetson Nano (JetPack 4.6.1, TensorRT 8.2)
//
//   webcam (V4L2, YUYV 640x480) -> RGB -> TensorRT FP16 (YOLOv8n) -> decodificacion + NMS
//   -> maquina de estados + decision con varios cuadros
//
// Uso:  epp-daemon <motor.engine> [dispositivo=/dev/video0] [cuadros=0 (sin limite, Ctrl+C)] [umbral=0.25]
//       epp-daemon <motor.engine> <imagen.ppm> [umbral=0.25]   (modo imagen: lista detecciones)
//
// ESTADO: todavia NO habla con el ESP32 (sin UART), asi que no enciende LEDs ni rele.
// El rele solo aparece como texto en la consola.

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "captura_v4l2.hpp"
#include "epp/imagen_ppm.hpp"
#include "epp/maquina_estados.hpp"
#include "epp/posprocesamiento.hpp"
#include "epp/preprocesamiento.hpp"
#include "inferencia_tensorrt.hpp"

namespace {

constexpr int DESCARTAR = 30;  // primeros cuadros de la camara (el primero llega corrupto)

volatile std::sig_atomic_t g_salir = 0;
void alSenal(int) { g_salir = 1; }

using Reloj = std::chrono::steady_clock;
double ms(Reloj::time_point a, Reloj::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }

bool terminaEn(const std::string& s, const std::string& fin) {
  return s.size() >= fin.size() && s.compare(s.size() - fin.size(), fin.size(), fin) == 0;
}

int modoImagen(const std::string& ruta_motor, const std::string& img, float umbral) {
  epp::MotorTensorRT motor(ruta_motor);
  const auto& red = motor.geometria();
  std::vector<uint8_t> rgb;
  std::vector<float> tensor, salida;
  if (!epp::leerPpm(img, rgb)) throw std::runtime_error("No se pudo leer " + img + " (se espera PPM P6 640x480)");
  epp::rgbATensor(rgb, red, tensor);
  const auto t0 = Reloj::now();
  motor.inferir(tensor, salida);
  const auto t1 = Reloj::now();
  const auto dets = epp::decodificar(salida.data(), red, umbral);
  std::printf("%s (umbral %.2f, inferencia %.1f ms): %zu detecciones\n", img.c_str(), umbral, ms(t0, t1),
              dets.size());
  for (const auto& d : dets)
    std::printf("  %-10s conf %.3f  caja [%.0f, %.0f, %.0f, %.0f]\n", epp::NOMBRES_CLASE[d.clase], d.conf, d.x1,
                d.y1, d.x2, d.y2);
  epp::guardarPpm("epp_imagen_anotada.ppm", rgb, dets);
  return 0;
}

int modoCamara(const std::string& ruta_motor, const std::string& dispositivo, int cuadros, float umbral) {
  std::cout << "Cargando motor " << ruta_motor << "...\n";
  epp::MotorTensorRT motor(ruta_motor);
  const auto& red = motor.geometria();
  std::printf("Motor: entrada %dx%d (relleno %d px arriba y abajo), %d cajas candidatas\n", red.ancho, red.alto,
              red.relleno_arriba, red.candidatos);
  std::cout << "Abriendo " << dispositivo << "...\n";
  epp::CamaraV4L2 cam(dispositivo);

  std::vector<uint8_t> yuyv, rgb;
  std::vector<float> tensor, salida;
  for (int i = 0; i < DESCARTAR; ++i)
    if (!cam.capturar(yuyv)) throw std::runtime_error("La camara no entrega cuadros");

  epp::MaquinaEstados maquina;  // sin PIR por ahora: falta el enlace con el ESP32
  int verdes = 0, rojos = 0, n = 0;
  double tCap = 0, tPre = 0, tInf = 0, tPost = 0;
  const auto inicio = Reloj::now();
  std::printf("Funcionando (Ctrl+C para terminar)\n\n");

  for (; !g_salir && (cuadros == 0 || n < cuadros); ++n) {
    const auto t0 = Reloj::now();
    if (!cam.capturar(yuyv)) throw std::runtime_error("Timeout de la camara");
    const auto t1 = Reloj::now();
    epp::yuyvARgb(yuyv, rgb);
    epp::rgbATensor(rgb, red, tensor);
    const auto t2 = Reloj::now();
    motor.inferir(tensor, salida);
    const auto t3 = Reloj::now();
    const auto dets = epp::decodificar(salida.data(), red, umbral);
    const auto t4 = Reloj::now();
    tCap += ms(t0, t1);
    tPre += ms(t1, t2);
    tInf += ms(t2, t3);
    tPost += ms(t3, t4);

    const double t = ms(inicio, t4) / 1000.0;
    const epp::Salida s = maquina.actualizar(t, /*pir=*/true, dets);
    if (!s.cambio) continue;
    std::printf("[%6.1f s] %-17s | luz: %-20s | rele: %-3s | %s\n", t, epp::nombreEstado(s.estado),
                epp::nombreLuz(s.luz), s.rele ? "ON" : "off", s.mensaje.c_str());
    if (s.resultado) {
      const auto& r = *s.resultado;
      std::printf("           cuadros con persona: %d | casco en %.0f%% | chaleco en %.0f%%\n",
                  r.cuadros_con_persona, 100.0f * r.fraccion_casco, 100.0f * r.fraccion_chaleco);
      const bool verde = (s.estado == epp::Estado::RESULTADO_VERDE);
      (verde ? verdes : rojos)++;
      char nombre[64];
      std::snprintf(nombre, sizeof(nombre), "resultado_%02d_%s.ppm", verdes + rojos, verde ? "verde" : "rojo");
      epp::guardarPpm(nombre, rgb, dets);  // evidencia de cada decision
    }
  }

  if (n == 0) return 0;
  const double total = ms(inicio, Reloj::now());
  std::printf("\n=== Resumen: %d cuadros en %.1f s (%.1f cuadros/s) ===\n", n, total / 1000.0, 1000.0 * n / total);
  std::printf("Decisiones: %d verdes, %d rojas (imagenes resultado_*.ppm)\n", verdes, rojos);
  std::printf("Tiempo medio por cuadro: captura %.1f ms | preproceso %.1f ms | inferencia %.1f ms | "
              "posproceso %.1f ms\n",
              tCap / n, tPre / n, tInf / n, tPost / n);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Uso: " << argv[0] << " <motor.engine> [dispositivo=/dev/video0] [cuadros=0] [umbral=0.25]\n"
              << "     " << argv[0] << " <motor.engine> <imagen.ppm> [umbral=0.25]\n";
    return 1;
  }
  std::signal(SIGINT, alSenal);
  std::signal(SIGTERM, alSenal);
  try {
    const std::string motor = argv[1];
    if (argc > 2 && terminaEn(argv[2], ".ppm"))
      return modoImagen(motor, argv[2], argc > 3 ? static_cast<float>(std::atof(argv[3])) : 0.25f);
    const std::string dispositivo = argc > 2 ? argv[2] : "/dev/video0";
    const int cuadros = argc > 3 ? std::max(0, std::atoi(argv[3])) : 0;  // 0 = sin limite
    // Umbral bajo al decodificar: la decision aplica sus propios umbrales (persona 0,40; prendas 0,50)
    const float umbral = argc > 4 ? static_cast<float>(std::atof(argv[4])) : 0.25f;
    return modoCamara(motor, dispositivo, cuadros, umbral);
  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}
