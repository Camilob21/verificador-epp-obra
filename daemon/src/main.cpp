#include <iostream>
#include "epp/decision.hpp"

int main() {
    const epp::PrendasRequeridas predeterminado{};
    std::cout << "EPP daemon (esqueleto): acceso="
              << (epp::fase1_completa(predeterminado) ? "AUTORIZADO" : "DENEGADO")
              << " | sin camara, sin inferencia, sin actuacion\n";
    return 0;
}
