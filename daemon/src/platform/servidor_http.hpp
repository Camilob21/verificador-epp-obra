// servidor_http.hpp — servidor HTTP minimo (un hilo, una conexion a la vez, Connection: close).
// Escucha SOLO en la direccion configurada (127.0.0.1 por defecto): el acceso desde la red pasa
// por nginx, que sirve el dashboard y reenvia /api/ aqui.
#pragma once
#include <atomic>
#include <functional>
#include <string>

#include "epp/http.hpp"

namespace epp {

class ServidorHttp {
 public:
  using Manejador = std::function<Respuesta(const Peticion&)>;
  // Abre el socket. Lanza std::runtime_error si el puerto esta ocupado.
  ServidorHttp(const std::string& direccion, int puerto);
  ~ServidorHttp();
  ServidorHttp(const ServidorHttp&) = delete;
  ServidorHttp& operator=(const ServidorHttp&) = delete;

  // Atiende peticiones hasta que 'salir' sea true (revisa cada 200 ms).
  void atenderHasta(const std::atomic<bool>& salir, const Manejador& manejador);
  int puerto() const { return puerto_; }

 private:
  int fd_ = -1;
  int puerto_ = 0;
};

}  // namespace epp
