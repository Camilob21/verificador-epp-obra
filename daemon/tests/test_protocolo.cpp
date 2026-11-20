#include <gtest/gtest.h>
#include "protocolo.h"

TEST(Protocolo, VectorCrcConocido) {
    const uint8_t datos[] = {'1','2','3','4','5','6','7','8','9'};
    EXPECT_EQ(0x29B1u, epp_crc16_ccitt_false(datos, sizeof(datos)));
}
TEST(Protocolo, TramaVaciaTieneSofYLongitudCorrecta) {
    uint8_t salida[8] = {};
    EXPECT_EQ(8u, epp_frame_encode(EPP_MSG_HEARTBEAT, 7, nullptr, 0, salida, sizeof(salida)));
    EXPECT_EQ(EPP_PROTO_SOF0, salida[0]);
    EXPECT_EQ(EPP_PROTO_SOF1, salida[1]);
    EXPECT_EQ(EPP_PROTO_VERSION, salida[2]);
    EXPECT_EQ(0u, salida[5]);
    const auto crc = epp_crc16_ccitt_false(salida + 2, 4);
    EXPECT_EQ(static_cast<uint8_t>(crc & 0xFFu), salida[6]);
    EXPECT_EQ(static_cast<uint8_t>(crc >> 8u), salida[7]);
}
TEST(Protocolo, RechazaBufferCorto) {
    uint8_t salida[7] = {};
    EXPECT_EQ(0u, epp_frame_encode(EPP_MSG_HEARTBEAT, 0, nullptr, 0, salida, sizeof(salida)));
}
TEST(Protocolo, RechazaPayloadNuloNoVacio) {
    uint8_t salida[10] = {};
    EXPECT_EQ(0u, epp_frame_encode(EPP_MSG_STATUS, 0, nullptr, 1, salida, sizeof(salida)));
}
