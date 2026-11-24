#include "epp/http.hpp"

#include <sstream>

#include "epp/json.hpp"

namespace epp {

namespace {
const char* textoCodigo(int c) {
  switch (c) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    default: return "Error";
  }
}
}  // namespace

Peticion analizarPeticion(const std::string& cab) {
  Peticion p;
  const auto fin = cab.find("\r\n");
  std::istringstream l(cab.substr(0, fin));
  std::string objetivo, version;
  if (!(l >> p.metodo >> objetivo >> version)) return p;
  if (version.compare(0, 5, "HTTP/") != 0 || objetivo.empty() || objetivo[0] != '/') return p;
  const auto q = objetivo.find('?');
  p.ruta = objetivo.substr(0, q);
  if (q != std::string::npos) {
    std::istringstream cs(objetivo.substr(q + 1));
    std::string par;
    while (std::getline(cs, par, '&')) {
      const auto igual = par.find('=');
      if (igual == std::string::npos)
        p.consulta[par] = "";
      else
        p.consulta[par.substr(0, igual)] = par.substr(igual + 1);
    }
  }
  p.valida = true;
  return p;
}

std::string serializar(const Respuesta& r) {
  std::ostringstream o;
  o << "HTTP/1.1 " << r.codigo << " " << textoCodigo(r.codigo) << "\r\n"
    << "Content-Type: " << r.tipo << "\r\n"
    << "Content-Length: " << r.cuerpo.size() << "\r\n"
    << "Cache-Control: no-store\r\n"
    << "Connection: close\r\n\r\n"
    << r.cuerpo;
  return o.str();
}

Respuesta respuestaError(int codigo, const std::string& mensaje) {
  Respuesta r;
  r.codigo = codigo;
  r.cuerpo = "{\"error\":" + jsonTexto(mensaje) + "}";
  return r;
}

}  // namespace epp
