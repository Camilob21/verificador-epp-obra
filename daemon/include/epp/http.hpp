// http.hpp — lo minimo de HTTP/1.1 para la API local del daemon (detras de nginx).
// Funciones puras: se prueban sin red. El servidor de sockets esta en platform/servidor_http.
#pragma once
#include <map>
#include <string>

namespace epp {

struct Peticion {
  std::string metodo;                       // "GET", "POST", ...
  std::string ruta;                         // "/api/eventos" (sin la consulta)
  std::map<std::string, std::string> consulta;  // ?limite=20 -> {"limite": "20"}
  bool valida = false;
};

struct Respuesta {
  int codigo = 200;
  std::string tipo = "application/json; charset=utf-8";
  std::string cuerpo;
};

// Analiza la primera linea ("GET /api/eventos?limite=20 HTTP/1.1"). valida=false si no se entiende.
Peticion analizarPeticion(const std::string& cabeceras);

// Respuesta completa lista para escribir en el socket (con Content-Length y Connection: close)
std::string serializar(const Respuesta& r);

Respuesta respuestaError(int codigo, const std::string& mensaje);

}  // namespace epp
