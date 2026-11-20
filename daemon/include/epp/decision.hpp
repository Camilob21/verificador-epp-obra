#pragma once

namespace epp {
struct PrendasRequeridas {
    bool casco = false;
    bool chaleco = false;
};

// Regla de ejemplo: niega el acceso si falta cualquier prenda de fase 1.
// No representa inferencia real ni mayoría de cuadros.
bool fase1_completa(const PrendasRequeridas& prendas) noexcept;
}
