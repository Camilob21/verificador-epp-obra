// json.hpp — escritura minima de JSON (sin bibliotecas externas). Solo lo que usa la API.
#pragma once
#include <cmath>
#include <cstdio>
#include <string>

namespace epp {

// Texto JSON con comillas y caracteres de control escapados.
inline std::string jsonTexto(const std::string& s) {
  std::string o = "\"";
  for (const unsigned char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default:
        if (c < 0x20) {
          char b[8];
          std::snprintf(b, sizeof(b), "\\u%04x", c);
          o += b;
        } else {
          o += static_cast<char>(c);
        }
    }
  }
  return o + "\"";
}

// Numero JSON; NaN o infinito (no validos en JSON) salen como null.
inline std::string jsonNumero(double v, int decimales = 2) {
  if (!std::isfinite(v)) return "null";
  char b[64];
  std::snprintf(b, sizeof(b), "%.*f", decimales, v);
  return b;
}

inline std::string jsonBool(bool b) { return b ? "true" : "false"; }

}  // namespace epp
