#include "registro_sqlite.hpp"

#include <sqlite3.h>

#include <ctime>
#include <stdexcept>

namespace epp {

namespace {
void ejecutar(sqlite3* db, const char* sql) {
  char* err = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
    const std::string m = err ? err : "error desconocido";
    sqlite3_free(err);
    throw std::runtime_error(std::string("SQLite: ") + m);
  }
}
}  // namespace

RegistroEventos::RegistroEventos(const std::string& ruta) {
  if (sqlite3_open(ruta.c_str(), &db_) != SQLITE_OK) {
    const std::string m = db_ ? sqlite3_errmsg(db_) : "sin memoria";
    sqlite3_close(db_);
    db_ = nullptr;
    throw std::runtime_error("No se pudo abrir la base " + ruta + ": " + m);
  }
  sqlite3_busy_timeout(db_, 1000);
  // WAL: el hilo de la API puede leer mientras el principal escribe
  ejecutar(db_, "PRAGMA journal_mode=WAL;");
  ejecutar(db_,
           "CREATE TABLE IF NOT EXISTS eventos ("
           " id INTEGER PRIMARY KEY AUTOINCREMENT,"
           " fecha TEXT NOT NULL,"
           " hora_confiable INTEGER NOT NULL,"
           " resultado TEXT NOT NULL CHECK (resultado IN ('verde','rojo','no_verificado')),"
           " falta_casco INTEGER NOT NULL,"
           " falta_chaleco INTEGER NOT NULL,"
           " fraccion_casco REAL NOT NULL,"
           " fraccion_chaleco REAL NOT NULL,"
           " cuadros INTEGER NOT NULL);");
}

RegistroEventos::~RegistroEventos() {
  if (db_) sqlite3_close(db_);
}

long long RegistroEventos::registrar(const Evento& e) {
  sqlite3_stmt* st = nullptr;
  const char* sql =
      "INSERT INTO eventos (fecha, hora_confiable, resultado, falta_casco, falta_chaleco, fraccion_casco,"
      " fraccion_chaleco, cuadros) VALUES (?,?,?,?,?,?,?,?);";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    throw std::runtime_error(std::string("SQLite: ") + sqlite3_errmsg(db_));
  sqlite3_bind_text(st, 1, e.fecha.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(st, 2, e.hora_confiable ? 1 : 0);
  sqlite3_bind_text(st, 3, e.resultado.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(st, 4, e.falta_casco ? 1 : 0);
  sqlite3_bind_int(st, 5, e.falta_chaleco ? 1 : 0);
  sqlite3_bind_double(st, 6, e.fraccion_casco);
  sqlite3_bind_double(st, 7, e.fraccion_chaleco);
  sqlite3_bind_int(st, 8, e.cuadros);
  const int rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE) throw std::runtime_error(std::string("SQLite: ") + sqlite3_errmsg(db_));
  return static_cast<long long>(sqlite3_last_insert_rowid(db_));
}

std::vector<Evento> RegistroEventos::ultimos(int limite) {
  std::vector<Evento> out;
  sqlite3_stmt* st = nullptr;
  const char* sql =
      "SELECT id, fecha, hora_confiable, resultado, falta_casco, falta_chaleco, fraccion_casco, fraccion_chaleco,"
      " cuadros FROM eventos ORDER BY id DESC LIMIT ?;";
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    throw std::runtime_error(std::string("SQLite: ") + sqlite3_errmsg(db_));
  sqlite3_bind_int(st, 1, limite);
  while (sqlite3_step(st) == SQLITE_ROW) {
    Evento e;
    e.id = sqlite3_column_int64(st, 0);
    e.fecha = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
    e.hora_confiable = sqlite3_column_int(st, 2) != 0;
    e.resultado = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
    e.falta_casco = sqlite3_column_int(st, 4) != 0;
    e.falta_chaleco = sqlite3_column_int(st, 5) != 0;
    e.fraccion_casco = sqlite3_column_double(st, 6);
    e.fraccion_chaleco = sqlite3_column_double(st, 7);
    e.cuadros = sqlite3_column_int(st, 8);
    out.push_back(e);
  }
  sqlite3_finalize(st);
  return out;
}

long long RegistroEventos::contar() {
  sqlite3_stmt* st = nullptr;
  long long n = -1;
  if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM eventos;", -1, &st, nullptr) == SQLITE_OK &&
      sqlite3_step(st) == SQLITE_ROW)
    n = sqlite3_column_int64(st, 0);
  sqlite3_finalize(st);
  return n;
}

std::string fechaActualIso() {
  const std::time_t t = std::time(nullptr);
  std::tm local{};
  localtime_r(&t, &local);
  char b[40];
  std::strftime(b, sizeof(b), "%Y-%m-%dT%H:%M:%S%z", &local);  // ...-0500
  std::string s = b;
  if (s.size() >= 5) s.insert(s.size() - 2, ":");                // ...-05:00 (ISO 8601)
  return s;
}

bool relojConfiable() {
  // 2026-01-01 00:00:00 UTC. Sin red, la Jetson arranca con la fecha de la imagen (2021).
  return std::time(nullptr) > 1767225600;
}

}  // namespace epp
