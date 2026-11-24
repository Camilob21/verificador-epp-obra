// sistema_linux.hpp — lee de /proc y /sys los textos que analiza MuestreadorSistema.
#pragma once
#include "epp/api.hpp"

namespace epp {
MuestreadorSistema::Textos leerTextosSistema();
}
