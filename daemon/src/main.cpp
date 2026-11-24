// epp-daemon — programa de la Jetson Nano (JetPack 4.6.1, TensorRT 8.2)
//
//   webcam (V4L2) -> TensorRT FP16 (YOLOv8n) -> decodificacion + NMS -> maquina de estados
//   -> ESP32-C6 por UART (semaforo, buzzer, rele) con latido y fail-safe
//   -> registro de decisiones en SQLite y API HTTP local (127.0.0.1:8081) para el dashboard
//
// Uso:  epp-daemon [-c archivo.conf] [-n cuadros] [-i imagen.ppm]
//   -c  configuracion (por defecto /etc/epp/epp.conf si existe; ver sistema/epp.conf.example)
//   -n  terminar tras N cuadros (por defecto 0 = sin limite, Ctrl+C o systemctl stop)
//   -i  modo imagen: lista las detecciones de un PPM 640x480 y termina
//
// Hilos: el principal hace camara + inferencia + decision; uno atiende el UART (latido cada
// 200 ms aunque la inferencia tarde) y otro la API HTTP. Comparten datos solo bajo candado.

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "captura_v4l2.hpp"
#include "epp/api.hpp"
#include "epp/config.hpp"
#include "epp/enlace.hpp"
#include "epp/imagen_ppm.hpp"
#include "epp/maquina_estados.hpp"
#include "epp/posprocesamiento.hpp"
#include "epp/preprocesamiento.hpp"
#include "inferencia_tensorrt.hpp"
#include "puerto_serie.hpp"
#include "registro_sqlite.hpp"
#include "servidor_http.hpp"
#include "sistema_linux.hpp"

namespace {

constexpr int DESCARTAR = 30;  // primeros cuadros de la camara (el primero llega corrupto)
constexpr const char* CONFIG_POR_DEFECTO = "/etc/epp/epp.conf";

std::atomic<bool> g_salir{false};
void alSenal(int) { g_salir = true; }

using Reloj = std::chrono::steady_clock;
double ms(Reloj::time_point a, Reloj::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }

// ---------------------------------------------------------------- enlace con el ESP32 (hilo propio)
struct Compartido {
  std::mutex m;
  epp::EnlaceEsp32 enlace;
  std::deque<epp::EventoEsp32> eventos;
  explicit Compartido(double tel) : enlace(tel) {}
};

void hiloEnlace(epp::PuertoSerie* puerto, Compartido* c, Reloj::time_point inicio) {
  uint8_t buf[256];
  while (!g_salir) {
    size_t n = 0;
    try {
      n = puerto->leer(buf, sizeof(buf), 20);
    } catch (const std::exception& e) {
      // Sin puerto, el ESP32 deja de recibir latido y se pone en ROJO solo (fail-safe).
      std::fprintf(stderr, "[uart] %s; reintento en 1 s\n", e.what());
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }
    const double t = ms(inicio, Reloj::now()) / 1000.0;
    std::vector<uint8_t> salida;
    {
      std::lock_guard<std::mutex> lock(c->m);
      for (auto& ev : c->enlace.recibir(buf, n, t)) c->eventos.push_back(ev);
      salida = c->enlace.tick(t);
    }
    try {
      if (!salida.empty()) puerto->escribir(salida.data(), salida.size());
    } catch (const std::exception& e) {
      std::fprintf(stderr, "[uart] %s\n", e.what());
    }
  }
}

// Lo que la API lee: el hilo principal lo actualiza y el de la API lo copia, ambos bajo candado.
struct EstadoApi {
  std::mutex m;
  epp::Instantanea s;
};

// El registro se usa desde el hilo principal (escribir) y el de la API (leer)
struct RegistroCompartido {
  std::mutex m;
  std::unique_ptr<epp::RegistroEventos> db;
};

proto_buzzer_t buzzerDe(epp::Estado e) {
  switch (e) {
    case epp::Estado::VERIFICANDO: return PROTO_BZ_BIP_CORTO;
    case epp::Estado::RESULTADO_VERDE: return PROTO_BZ_OK;
    case epp::Estado::RESULTADO_ROJO: return PROTO_BZ_ERROR;
    default: return PROTO_BZ_SILENCIO;
  }
}

// ---------------------------------------------------------------- modos
int modoImagen(const epp::Config& cfg, const std::string& img) {
  epp::MotorTensorRT motor(cfg.motor);
  const auto& red = motor.geometria();
  std::vector<uint8_t> rgb;
  std::vector<float> tensor, salida;
  if (!epp::leerPpm(img, rgb)) throw std::runtime_error("No se pudo leer " + img + " (se espera PPM P6 640x480)");
  epp::rgbATensor(rgb, red, tensor);
  const auto t0 = Reloj::now();
  motor.inferir(tensor, salida);
  const auto t1 = Reloj::now();
  const auto dets = epp::decodificar(salida.data(), red, cfg.umbral_deteccion);
  std::printf("%s (umbral %.2f, inferencia %.1f ms): %zu detecciones\n", img.c_str(), cfg.umbral_deteccion,
              ms(t0, t1), dets.size());
  for (const auto& d : dets)
    std::printf("  %-10s conf %.3f  caja [%.0f, %.0f, %.0f, %.0f]\n", epp::NOMBRES_CLASE[d.clase], d.conf, d.x1,
                d.y1, d.x2, d.y2);
  epp::guardarPpm("epp_imagen_anotada.ppm", rgb, dets);
  return 0;
}

int modoCamara(const epp::Config& cfg, int cuadros) {
  std::printf("Cargando motor %s...\n", cfg.motor.c_str());
  epp::MotorTensorRT motor(cfg.motor);
  const auto& red = motor.geometria();
  std::printf("Motor: entrada %dx%d (relleno %d px), %d cajas candidatas\n", red.ancho, red.alto, red.relleno_arriba,
              red.candidatos);
  std::printf("Abriendo %s...\n", cfg.camara.c_str());
  epp::CamaraV4L2 cam(cfg.camara);

  const auto inicio = Reloj::now();
  std::unique_ptr<epp::PuertoSerie> puerto;
  std::unique_ptr<Compartido> comp;
  std::thread hilo;
  // Si algo lanza una excepcion (p. ej. timeout de la camara), detener y unir el hilo antes de
  // destruir el puerto; un std::thread sin unir terminaria el programa de golpe.
  struct UnirHilo {
    std::thread& h;
    ~UnirHilo() {
      g_salir = true;
      if (h.joinable()) h.join();
    }
  } guardia{hilo};
  if (!cfg.uart.empty()) {
    puerto.reset(new epp::PuertoSerie(cfg.uart, cfg.baudios));
    comp.reset(new Compartido(cfg.intervalo_telemetria_s));
    comp->enlace.ordenarEstado(PROTO_ST_REPOSO);
    hilo = std::thread(hiloEnlace, puerto.get(), comp.get(), inicio);
    std::printf("Enlace con el ESP32 por %s a %d baudios (latido cada 200 ms)\n", cfg.uart.c_str(), cfg.baudios);
  } else {
    std::printf("Sin ESP32 (uart vacio): las luces y el rele solo se muestran en la consola\n");
  }
  // -- registro de eventos (opcional: sin base, el sistema sigue decidiendo)
  RegistroCompartido reg;
  if (!cfg.db.empty()) {
    try {
      reg.db.reset(new epp::RegistroEventos(cfg.db));
      std::printf("Registro de eventos: %s (%lld eventos guardados)\n", cfg.db.c_str(), reg.db->contar());
    } catch (const std::exception& e) {
      std::printf("AVISO: sin registro de eventos: %s\n", e.what());
    }
  }
  if (!epp::relojConfiable())
    std::printf("AVISO: el reloj no parece sincronizado; los eventos se marcan con hora_confiable = false\n");

  // -- API HTTP local (hilo propio)
  EstadoApi api;
  api.s.hay_uart = static_cast<bool>(comp);
  api.s.registro_activo = static_cast<bool>(reg.db);
  std::unique_ptr<epp::ServidorHttp> servidor;
  std::thread hilo_api;
  struct UnirApi {
    std::thread& h;
    ~UnirApi() {
      g_salir = true;
      if (h.joinable()) h.join();
    }
  } guardia_api{hilo_api};
  if (cfg.api_puerto > 0) {
    try {
      servidor.reset(new epp::ServidorHttp(cfg.api_direccion, cfg.api_puerto));
      epp::FuenteEventos fuente;
      if (reg.db)
        fuente = [&reg](int limite) {
          std::lock_guard<std::mutex> lock(reg.m);
          return reg.db->ultimos(limite);
        };
      hilo_api = std::thread([&servidor, &api, &cfg, fuente]() {
        servidor->atenderHasta(g_salir, [&](const epp::Peticion& p) {
          epp::Instantanea copia;
          {
            std::lock_guard<std::mutex> lock(api.m);
            copia = api.s;
          }
          return epp::atender(p, copia, cfg, fuente);
        });
      });
      std::printf("API: http://%s:%d/api/estado\n", cfg.api_direccion.c_str(), servidor->puerto());
    } catch (const std::exception& e) {
      std::printf("AVISO: sin API: %s\n", e.what());
    }
  }

  if (cfg.maquina.usar_pir && !comp)
    std::printf("AVISO: usar_pir = si pero no hay ESP32; se asume presencia siempre\n");

  std::vector<uint8_t> yuyv, rgb;
  std::vector<float> tensor, salida;
  for (int i = 0; i < DESCARTAR; ++i)
    if (!cam.capturar(yuyv)) throw std::runtime_error("La camara no entrega cuadros");

  epp::MaquinaEstados maquina(cfg.maquina, cfg.decision);
  bool pir_presencia = false;
  int verdes = 0, rojos = 0, n = 0;
  double tCap = 0, tPre = 0, tInf = 0, tPost = 0, t_ultima_tel = -1e9;
  double t_muestreo = -1e9;
  int cuadros_ventana = 0;
  double inf_ventana = 0, pre_ventana = 0;
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
    const auto dets = epp::decodificar(salida.data(), red, cfg.umbral_deteccion);
    const auto t4 = Reloj::now();
    tCap += ms(t0, t1);
    tPre += ms(t1, t2);
    tInf += ms(t2, t3);
    tPost += ms(t3, t4);
    const double t = ms(inicio, t4) / 1000.0;
    ++cuadros_ventana;
    inf_ventana += ms(t2, t3);
    pre_ventana += ms(t1, t2);

    // -- instrumentacion para la API, una vez por segundo (leer /proc fuera del candado)
    if (t - t_muestreo >= 1.0) {
      const auto textos = epp::leerTextosSistema();
      epp::EstadisticasEnlace st_enlace;
      proto_stats_t st_parser{};
      bool enlace_ok = false;
      if (comp) {
        std::lock_guard<std::mutex> lock(comp->m);
        st_enlace = comp->enlace.estadisticas();
        st_parser = comp->enlace.estadisticasParser();
        enlace_ok = comp->enlace.hayEnlace(t);
      }
      std::lock_guard<std::mutex> lock(api.m);
      api.s.sistema.actualizar(t, textos);
      const double ventana = t - t_muestreo;
      if (t_muestreo > -1e8 && ventana > 0) api.s.fps = cuadros_ventana / ventana;
      api.s.inferencia_ms = inf_ventana / cuadros_ventana;
      api.s.preproceso_ms = pre_ventana / cuadros_ventana;
      api.s.enlace = st_enlace;
      api.s.parser = st_parser;
      api.s.enlace_ok = enlace_ok;
      cuadros_ventana = 0;
      inf_ventana = pre_ventana = 0;
      t_muestreo = t;
    }

    // -- eventos del ESP32
    if (comp) {
      std::deque<epp::EventoEsp32> evs;
      {
        std::lock_guard<std::mutex> lock(comp->m);
        evs.swap(comp->eventos);
      }
      for (const auto& e : evs) {
        using T = epp::EventoEsp32::Tipo;
        switch (e.tipo) {
          case T::PIR:
            pir_presencia = e.presencia && e.pir_listo;
            {
              std::lock_guard<std::mutex> lock(api.m);
              api.s.pir = pir_presencia;
            }
            std::printf("[%6.1f s] PIR: %s%s\n", t, e.presencia ? "presencia" : "sin presencia",
                        e.pir_listo ? "" : " (calentando)");
            break;
          case T::BOTON:
            // TODO fase 6: borrar configuracion y eventos y volver al bring-up
            std::printf("[%6.1f s] BOTON: factory reset solicitado (confirmado con ACK)\n", t);
            break;
          case T::ENLACE_PERDIDO:
            std::printf("[%6.1f s] ESP32: perdio el latido, esta en fail-safe (ROJO)\n", t);
            break;
          case T::ENLACE_RESTABLECIDO:
          case T::ARRANQUE: {
            // El ESP32 quedo en ROJO: se reinicia la verificacion; nunca se retoma un verde viejo.
            if (e.tipo == T::ARRANQUE)
              std::printf("[%6.1f s] ESP32: arranco (motivo %u, firmware %u.%u); se reinicia la verificacion\n", t,
                          e.motivo, e.fw_mayor, e.fw_menor);
            else
              std::printf("[%6.1f s] ESP32: enlace restablecido; se reinicia la verificacion\n", t);
            maquina = epp::MaquinaEstados(cfg.maquina, cfg.decision);
            std::lock_guard<std::mutex> lock(comp->m);
            comp->enlace.ordenarEstado(PROTO_ST_REPOSO);
            break;
          }
          case T::TELEMETRIA:
            {
              std::lock_guard<std::mutex> lock(api.m);
              api.s.telemetria = e.telemetria;
              api.s.hay_telemetria = true;
            }
            if (t - t_ultima_tel >= 60.0) {  // una linea por minuto basta en el registro
              const auto& m = e.telemetria;
              std::printf("[%6.1f s] ESP32: uptime %u s, heap libre %u B (min %u), tramas ok %u, err crc %u, "
                          "fallos de enlace %u\n",
                          t, m.uptime_s, m.heap_libre, m.heap_min, m.tramas_ok, m.err_crc, m.fallos_enlace);
              t_ultima_tel = t;
            }
            break;
          case T::NACK:
            std::printf("[%6.1f s] ESP32 rechazo tipo 0x%02X (codigo %u)\n", t, e.tipo_rechazado, e.codigo);
            break;
          case T::SIN_ACK:
            std::printf("[%6.1f s] AVISO: el ESP32 no confirmo la orden 0x%02X tras 10 reintentos\n", t,
                        e.tipo_rechazado);
            break;
        }
      }
    }

    // -- decision
    const bool pir = cfg.maquina.usar_pir && comp ? pir_presencia : true;
    const epp::Salida s = maquina.actualizar(t, pir, dets);
    {
      std::lock_guard<std::mutex> lock(api.m);
      api.s.t_s = t;
      api.s.cuadros = static_cast<unsigned long long>(n + 1);
      if (s.cambio) {
        api.s.estado = s.estado;
        api.s.luz = s.luz;
        api.s.rele = s.rele;
        api.s.mensaje = s.mensaje;
      }
    }
    if (!s.cambio) continue;
    if (comp) {
      std::lock_guard<std::mutex> lock(comp->m);
      comp->enlace.ordenarEstado(epp::estadoDeLuz(s.luz));
      const proto_buzzer_t bz = buzzerDe(s.estado);
      if (bz != PROTO_BZ_SILENCIO) comp->enlace.ordenarBuzzer(bz);
    }
    std::printf("[%6.1f s] %-17s | luz: %-17s | rele: %-3s | %s\n", t, epp::nombreEstado(s.estado),
                epp::nombreLuz(s.luz), s.rele ? "ON" : "off", s.mensaje.c_str());
    if (s.resultado) {
      const auto& r = *s.resultado;
      std::printf("           cuadros con persona: %d | casco en %.0f%% | chaleco en %.0f%%\n",
                  r.cuadros_con_persona, 100.0f * r.fraccion_casco, 100.0f * r.fraccion_chaleco);
      const bool verde = (s.estado == epp::Estado::RESULTADO_VERDE);
      (verde ? verdes : rojos)++;
      epp::Evento ev;
      ev.fecha = epp::fechaActualIso();
      ev.hora_confiable = epp::relojConfiable();
      ev.resultado = !r.decidido ? "no_verificado" : (verde ? "verde" : "rojo");
      ev.falta_casco = r.falta_casco;
      ev.falta_chaleco = r.falta_chaleco;
      ev.fraccion_casco = r.fraccion_casco;
      ev.fraccion_chaleco = r.fraccion_chaleco;
      ev.cuadros = r.cuadros_con_persona;
      if (reg.db) {
        try {
          std::lock_guard<std::mutex> lock(reg.m);
          ev.id = reg.db->registrar(ev);
        } catch (const std::exception& e) {
          std::printf("AVISO: no se pudo registrar el evento: %s\n", e.what());
        }
      }
      {
        std::lock_guard<std::mutex> lock(api.m);
        api.s.verdes = verdes;
        api.s.rojos = rojos;
        api.s.ultimo = ev;
        api.s.hay_ultimo = true;
      }
      if (cfg.guardar_evidencia) {
        char nombre[64];
        std::snprintf(nombre, sizeof(nombre), "/resultado_%02d_%s.ppm", verdes + rojos, verde ? "verde" : "rojo");
        epp::guardarPpm(cfg.dir_evidencia + nombre, rgb, dets);
      }
    }
    std::fflush(stdout);
  }

  g_salir = true;  // detiene los hilos; sin latido, el ESP32 pasa a ROJO en 800 ms
  if (hilo.joinable()) hilo.join();
  if (hilo_api.joinable()) hilo_api.join();
  if (comp) {
    const auto& st = comp->enlace.estadisticas();
    const auto& ps = comp->enlace.estadisticasParser();
    std::printf("\nEnlace: %u latidos, %u ordenes, %u reintentos, %u ACK, %u NACK, %u sin ACK, %u eventos | "
                "parser: %u tramas ok, %u err CRC, %u descartados\n",
                st.latidos, st.ordenes, st.reintentos, st.acks, st.nacks, st.sin_ack, st.eventos, ps.tramas_ok,
                ps.err_crc, ps.bytes_descartados);
  }
  if (n == 0) return 0;
  const double total = ms(inicio, Reloj::now());
  std::printf("=== Resumen: %d cuadros en %.1f s (%.1f cuadros/s) ===\n", n, total / 1000.0, 1000.0 * n / total);
  std::printf("Decisiones: %d verdes, %d rojas\n", verdes, rojos);
  std::printf("Tiempo medio por cuadro: captura %.1f ms | preproceso %.1f ms | inferencia %.1f ms | "
              "posproceso %.1f ms\n",
              tCap / n, tPre / n, tInf / n, tPost / n);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string ruta_config, imagen;
  int cuadros = 0;
  int opt;
  while ((opt = getopt(argc, argv, "c:n:i:h")) != -1) {
    switch (opt) {
      case 'c': ruta_config = optarg; break;
      case 'n': cuadros = std::max(0, std::atoi(optarg)); break;
      case 'i': imagen = optarg; break;
      default:
        std::fprintf(stderr, "Uso: %s [-c archivo.conf] [-n cuadros] [-i imagen.ppm]\n", argv[0]);
        return opt == 'h' ? 0 : 1;
    }
  }
  std::signal(SIGINT, alSenal);
  std::signal(SIGTERM, alSenal);
  setvbuf(stdout, nullptr, _IOLBF, 0);  // una linea a la vez en journalctl

  epp::Config cfg;
  std::vector<std::string> avisos;
  const bool explicita = !ruta_config.empty();
  if (!explicita) ruta_config = CONFIG_POR_DEFECTO;
  if (epp::cargarConfig(ruta_config, cfg, avisos)) {
    std::printf("Configuracion: %s\n", ruta_config.c_str());
  } else if (explicita) {
    std::fprintf(stderr, "ERROR: no se pudo abrir %s\n", ruta_config.c_str());
    return 1;
  } else {
    std::printf("Configuracion: valores por defecto (%s no existe)\n", ruta_config.c_str());
  }
  for (const auto& a : avisos) std::printf("  AVISO config: %s\n", a.c_str());
  std::printf("%s\n", epp::describirConfig(cfg).c_str());

  try {
    if (!imagen.empty()) return modoImagen(cfg, imagen);
    return modoCamara(cfg, cuadros);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "ERROR: %s\n", e.what());
    return 1;
  }
}
