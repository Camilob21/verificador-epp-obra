// sistema_linux.hpp — lee de /proc y /sys los textos que analiza MuestreadorSistema.
#pragma once
#include "epp/api.hpp"

namespace epp {
MuestreadorSistema::Textos leerTextosSistema();

// Deja en 'directorio' solo los 'maximo' archivos mas recientes cuyo nombre empieza por 'prefijo'.
// Devuelve cuantos borro. maximo <= 0: no borra nada.
int podarArchivos(const std::string& directorio, const std::string& prefijo, int maximo);
}
