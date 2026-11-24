// registro_sqlite.hpp — registro de decisiones en SQLite (archivo en /var/lib/epp/eventos.db).
// Guarda solo el resultado y que prenda falto: sin imagenes ni identidad de la persona.
#pragma once
#include <string>
#include <vector>

#include "epp/api.hpp"

struct sqlite3;

namespace epp {

class RegistroEventos {
 public:
  // Abre (o crea) la base y la tabla. Lanza std::runtime_error si no se puede.
  explicit RegistroEventos(const std::string& ruta);
  ~RegistroEventos();
  RegistroEventos(const RegistroEventos&) = delete;
  RegistroEventos& operator=(const RegistroEventos&) = delete;

  // Inserta y devuelve el id asignado. 'fecha' en ISO 8601; ver fechaActualIso().
  long long registrar(const Evento& e);
  std::vector<Evento> ultimos(int limite);  // mas recientes primero
  long long contar();

 private:
  sqlite3* db_ = nullptr;
};

// Fecha y hora local actual en ISO 8601 ("2026-11-24T14:30:05-05:00")
std::string fechaActualIso();
// true si el reloj del sistema parece sincronizado (la Jetson no tiene pila de RTC y arranca en 2021)
bool relojConfiable();

}  // namespace epp
