#include "servidor_http.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace epp {

namespace {
constexpr size_t MAX_CABECERAS = 8192;

void enviarTodo(int fd, const std::string& s) {
  size_t hecho = 0;
  while (hecho < s.size()) {
    const ssize_t w = ::send(fd, s.data() + hecho, s.size() - hecho, MSG_NOSIGNAL);
    if (w < 0) {
      if (errno == EINTR) continue;
      return;  // el cliente cerro: no hay a quien avisar
    }
    hecho += static_cast<size_t>(w);
  }
}
}  // namespace

ServidorHttp::ServidorHttp(const std::string& direccion, int puerto) {
  fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd_ < 0) throw std::runtime_error(std::string("socket: ") + std::strerror(errno));
  const int uno = 1;
  setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof(uno));
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(static_cast<uint16_t>(puerto));
  if (inet_pton(AF_INET, direccion.c_str(), &a.sin_addr) != 1) {
    ::close(fd_);
    throw std::runtime_error("direccion invalida para la API: " + direccion);
  }
  if (::bind(fd_, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0 || ::listen(fd_, 8) != 0) {
    const std::string e = std::strerror(errno);
    ::close(fd_);
    throw std::runtime_error("No se pudo escuchar en " + direccion + ":" + std::to_string(puerto) + ": " + e);
  }
  socklen_t n = sizeof(a);
  getsockname(fd_, reinterpret_cast<sockaddr*>(&a), &n);
  puerto_ = ntohs(a.sin_port);  // si se pidio el puerto 0 (pruebas), el sistema asigna uno libre
}

ServidorHttp::~ServidorHttp() {
  if (fd_ >= 0) ::close(fd_);
}

void ServidorHttp::atenderHasta(const std::atomic<bool>& salir, const Manejador& manejador) {
  while (!salir) {
    pollfd p{fd_, POLLIN, 0};
    if (::poll(&p, 1, 200) <= 0) continue;
    const int c = ::accept4(fd_, nullptr, nullptr, SOCK_CLOEXEC);
    if (c < 0) continue;
    // Leer cabeceras (hasta la linea vacia) con un limite de tiempo y de tamano
    std::string cab;
    char buf[1024];
    bool completo = false;
    while (cab.size() < MAX_CABECERAS) {
      pollfd q{c, POLLIN, 0};
      if (::poll(&q, 1, 1000) <= 0) break;
      const ssize_t n = ::recv(c, buf, sizeof(buf), 0);
      if (n <= 0) break;
      cab.append(buf, static_cast<size_t>(n));
      if (cab.find("\r\n\r\n") != std::string::npos) {
        completo = true;
        break;
      }
    }
    Respuesta r;
    if (!completo)
      r = respuestaError(cab.size() >= MAX_CABECERAS ? 413 : 400, "cabeceras incompletas");
    else {
      try {
        r = manejador(analizarPeticion(cab));
      } catch (const std::exception& e) {
        r = respuestaError(500, e.what());
      }
    }
    enviarTodo(c, serializar(r));
    ::close(c);
  }
}

}  // namespace epp
