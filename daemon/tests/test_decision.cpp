#include <gtest/gtest.h>
#include "epp/decision.hpp"

TEST(DecisionFase1, PorDefectoDenegado) {
    EXPECT_FALSE(epp::fase1_completa({}));
}
TEST(DecisionFase1, CascoSinChalecoDenegado) {
    EXPECT_FALSE(epp::fase1_completa({true, false}));
}
TEST(DecisionFase1, ChalecoSinCascoDenegado) {
    EXPECT_FALSE(epp::fase1_completa({false, true}));
}
TEST(DecisionFase1, AmbosPresentesEnEntradaSimulada) {
    EXPECT_TRUE(epp::fase1_completa({true, true}));
}
