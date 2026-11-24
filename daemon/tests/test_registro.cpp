// Pruebas del registro SQLite con una base temporal.
#include <gtest/gtest.h>

#include <unistd.h>

#include <cstdio>
#include <stdexcept>
#include <string>

#include "registro_sqlite.hpp"

using namespace epp;

namespace {
struct BaseTemporal {
  std::string ruta;
  BaseTemporal() {
    char plantilla[] = "/tmp/epp_eventos_XXXXXX";
    const int fd = mkstemp(plantilla);
    if (fd >= 0) close(fd);
    ruta = plantilla;
  }
  ~BaseTemporal() {
    std::remove(ruta.c_str());
    std::remove((ruta + "-wal").c_str());
    std::remove((ruta + "-shm").c_str());
  }
};
}  // namespace

TEST(Registro, GuardaYLeeEnOrdenInverso) {
  BaseTemporal t;
  RegistroEventos r(t.ruta);
  EXPECT_EQ(r.contar(), 0);
  Evento e;
  e.fecha = "2026-11-24T14:00:00-05:00";
  e.resultado = "rojo";
  e.falta_casco = true;
  e.fraccion_casco = 0.1;
  e.cuadros = 12;
  const long long id1 = r.registrar(e);
  e.resultado = "verde";
  e.falta_casco = false;
  const long long id2 = r.registrar(e);
  EXPECT_GT(id2, id1);
  EXPECT_EQ(r.contar(), 2);
  const auto v = r.ultimos(10);
  ASSERT_EQ(v.size(), 2u);
  EXPECT_EQ(v[0].resultado, "verde");  // el mas reciente primero
  EXPECT_EQ(v[1].resultado, "rojo");
  EXPECT_TRUE(v[1].falta_casco);
  EXPECT_EQ(v[1].cuadros, 12);
  EXPECT_NEAR(v[1].fraccion_casco, 0.1, 1e-9);
  EXPECT_EQ(r.ultimos(1).size(), 1u);
}

TEST(Registro, PersisteAlReabrir) {
  BaseTemporal t;
  {
    RegistroEventos r(t.ruta);
    Evento e;
    e.fecha = "x";
    e.resultado = "no_verificado";
    r.registrar(e);
  }
  RegistroEventos r2(t.ruta);
  EXPECT_EQ(r2.contar(), 1);
}

TEST(Registro, RechazaResultadoInvalido) {
  BaseTemporal t;
  RegistroEventos r(t.ruta);
  Evento e;
  e.fecha = "x";
  e.resultado = "amarillo";
  EXPECT_THROW(r.registrar(e), std::runtime_error);
}

TEST(Registro, RutaImposibleLanzaError) {
  EXPECT_THROW(RegistroEventos("/no/existe/epp.db"), std::runtime_error);
}

TEST(Registro, FechaIsoTieneZonaHoraria) {
  const auto f = fechaActualIso();
  ASSERT_GE(f.size(), 25u);  // 2026-11-24T14:30:05-05:00
  EXPECT_EQ(f[10], 'T');
  EXPECT_EQ(f[f.size() - 3], ':');
}

#include <sys/stat.h>
#include <utime.h>

#include <fstream>

#include "sistema_linux.hpp"

TEST(Evidencias, PodaConservaLasMasRecientes) {
  char plantilla[] = "/tmp/epp_evid_XXXXXX";
  ASSERT_NE(mkdtemp(plantilla), nullptr);
  const std::string dir = plantilla;
  for (int i = 0; i < 5; ++i) {
    const std::string ruta = dir + "/evento_00000" + std::to_string(i) + "_rojo.jpg";
    std::ofstream(ruta) << "x";
    utimbuf t{1000 + i, 1000 + i};  // mtime creciente: el 0 es el mas viejo
    utime(ruta.c_str(), &t);
  }
  std::ofstream(dir + "/eventos.db") << "no se toca";
  EXPECT_EQ(podarArchivos(dir, "evento_", 3), 2);
  struct stat st;
  EXPECT_NE(stat((dir + "/evento_000000_rojo.jpg").c_str(), &st), 0);  // borrado
  EXPECT_NE(stat((dir + "/evento_000001_rojo.jpg").c_str(), &st), 0);  // borrado
  EXPECT_EQ(stat((dir + "/evento_000004_rojo.jpg").c_str(), &st), 0);  // conservado
  EXPECT_EQ(stat((dir + "/eventos.db").c_str(), &st), 0);              // otro prefijo: intacto
  EXPECT_EQ(podarArchivos(dir, "evento_", 0), 0);                      // 0 = sin limite
  for (int i = 2; i < 5; ++i) std::remove((dir + "/evento_00000" + std::to_string(i) + "_rojo.jpg").c_str());
  std::remove((dir + "/eventos.db").c_str());
  rmdir(dir.c_str());
}
