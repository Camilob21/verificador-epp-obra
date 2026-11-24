#include "puerto_serie.hpp"

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace epp {

namespace {
speed_t velocidad(int baudios) {
  switch (baudios) {
    case 9600: return B9600;
    case 57600: return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    case 921600: return B921600;
    default: throw std::runtime_error("baudios no soportados: " + std::to_string(baudios));
  }
}
}  // namespace

PuertoSerie::PuertoSerie(const std::string& dispositivo, int baudios) : nombre_(dispositivo) {
  fd_ = ::open(dispositivo.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
  if (fd_ < 0) throw std::runtime_error("No se pudo abrir " + dispositivo + ": " + std::strerror(errno));
  termios tio{};
  if (tcgetattr(fd_, &tio) != 0) {
    const std::string e = std::strerror(errno);
    ::close(fd_);
    throw std::runtime_error("tcgetattr " + dispositivo + ": " + e);
  }
  cfmakeraw(&tio);                      // sin eco, sin traduccion de fin de linea, 8 bits
  tio.c_cflag |= CLOCAL | CREAD;        // ignora lineas de modem, habilita recepcion
  tio.c_cflag &= ~static_cast<tcflag_t>(CSTOPB | PARENB | CRTSCTS);  // 1 bit de parada, sin paridad ni RTS/CTS
  tio.c_cflag &= ~static_cast<tcflag_t>(HUPCL);  // no bajar DTR al cerrar (evita reiniciar el ESP32 por USB)
  tio.c_cc[VMIN] = 0;
  tio.c_cc[VTIME] = 0;
  const speed_t v = velocidad(baudios);
  cfsetispeed(&tio, v);
  cfsetospeed(&tio, v);
  if (tcsetattr(fd_, TCSANOW, &tio) != 0) {
    const std::string e = std::strerror(errno);
    ::close(fd_);
    throw std::runtime_error("tcsetattr " + dispositivo + ": " + e);
  }
  tcflush(fd_, TCIOFLUSH);  // descarta basura acumulada antes de abrir
}

PuertoSerie::~PuertoSerie() {
  if (fd_ >= 0) ::close(fd_);
}

size_t PuertoSerie::leer(uint8_t* buf, size_t max, int espera_ms) {
  pollfd p{fd_, POLLIN, 0};
  const int r = ::poll(&p, 1, espera_ms);
  if (r < 0) {
    if (errno == EINTR) return 0;
    throw std::runtime_error("poll " + nombre_ + ": " + std::strerror(errno));
  }
  if (r == 0) return 0;
  if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) throw std::runtime_error(nombre_ + " se desconecto");
  const ssize_t n = ::read(fd_, buf, max);
  if (n < 0) {
    if (errno == EAGAIN || errno == EINTR) return 0;
    throw std::runtime_error("read " + nombre_ + ": " + std::strerror(errno));
  }
  return static_cast<size_t>(n);
}

void PuertoSerie::escribir(const uint8_t* datos, size_t n) {
  size_t hecho = 0;
  while (hecho < n) {
    const ssize_t w = ::write(fd_, datos + hecho, n - hecho);
    if (w < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN) {  // buffer del driver lleno: esperar a que se pueda escribir
        pollfd p{fd_, POLLOUT, 0};
        ::poll(&p, 1, 50);
        continue;
      }
      throw std::runtime_error("write " + nombre_ + ": " + std::strerror(errno));
    }
    hecho += static_cast<size_t>(w);
  }
}

}  // namespace epp
