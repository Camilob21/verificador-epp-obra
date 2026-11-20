#include "epp/decision.hpp"

namespace epp {
bool fase1_completa(const PrendasRequeridas& prendas) noexcept {
    return prendas.casco && prendas.chaleco;
}
}
