#include <stdint.h>
#include "unity.h"
#include "protocolo.h"

void setUp(void) {}
void tearDown(void) {}

static void test_crc_conocido(void) {
    const uint8_t ejemplo[] = {'1','2','3','4','5','6','7','8','9'};
    TEST_ASSERT_EQUAL_HEX16(0x29B1, epp_crc16_ccitt_false(ejemplo, sizeof(ejemplo)));
}

static void test_trama_no_cabe(void) {
    uint8_t salida[7] = {0};
    TEST_ASSERT_EQUAL_UINT32(0u, epp_frame_encode(EPP_MSG_HEARTBEAT, 0u, NULL, 0u, salida, sizeof(salida)));
}

void app_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_crc_conocido);
    RUN_TEST(test_trama_no_cabe);
    UNITY_END();
}
