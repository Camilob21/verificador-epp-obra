// Prueba de integracion del servidor HTTP con un socket real en 127.0.0.1 (puerto libre al azar).
#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>

#include "servidor_http.hpp"

using namespace epp;

namespace {
std::string pedir(int puerto, const std::string& peticion) {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(static_cast<uint16_t>(puerto));
  inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
  if (connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
    close(fd);
    return "";
  }
  send(fd, peticion.data(), peticion.size(), 0);
  std::string r;
  char buf[512];
  ssize_t n;
  while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) r.append(buf, static_cast<size_t>(n));
  close(fd);
  return r;
}
}  // namespace

TEST(ServidorHttp, RespondeYTermina) {
  ServidorHttp srv("127.0.0.1", 0);
  ASSERT_GT(srv.puerto(), 0);
  std::atomic<bool> salir{false};
  std::thread h([&] {
    srv.atenderHasta(salir, [](const Peticion& p) {
      Respuesta r;
      r.cuerpo = "{\"ruta\":\"" + p.ruta + "\"}";
      return r;
    });
  });
  const auto r = pedir(srv.puerto(), "GET /api/estado HTTP/1.1\r\nHost: x\r\n\r\n");
  EXPECT_NE(r.find("200 OK"), std::string::npos);
  EXPECT_NE(r.find("{\"ruta\":\"/api/estado\"}"), std::string::npos);
  const auto mala = pedir(srv.puerto(), "basura sin fin de cabeceras");
  EXPECT_NE(mala.find("400"), std::string::npos);
  salir = true;
  h.join();  // debe terminar en <= 200 ms
}

TEST(ServidorHttp, ExcepcionDelManejadorDa500) {
  ServidorHttp srv("127.0.0.1", 0);
  std::atomic<bool> salir{false};
  std::thread h([&] {
    srv.atenderHasta(salir, [](const Peticion&) -> Respuesta { throw std::runtime_error("fallo"); });
  });
  const auto r = pedir(srv.puerto(), "GET / HTTP/1.1\r\n\r\n");
  EXPECT_NE(r.find("500"), std::string::npos);
  salir = true;
  h.join();
}

TEST(ServidorHttp, PuertoOcupadoLanzaError) {
  ServidorHttp a("127.0.0.1", 0);
  EXPECT_THROW(ServidorHttp("127.0.0.1", a.puerto()), std::runtime_error);
}

TEST(ServidorHttp, DireccionInvalidaLanzaError) {
  EXPECT_THROW(ServidorHttp("no-es-ip", 0), std::runtime_error);
}
