#ifndef LUMINATE_RC_TEST_H
#define LUMINATE_RC_TEST_H

#include <cassert>

#define RC_TEST(rc)                                                              \
    {                                                                            \
        RED_RC red_rc_test = (rc);                                               \
        if (red_rc_test != RED_OK) {                                             \
            printf("RC_TEST failed :: " #rc " :: code = 0x%04x\n", red_rc_test); \
            return red_rc_test;                                                  \
        }                                                                        \
    }

#define RC_CHECK(rc)                                                               \
    {                                                                              \
        RED_RC red_rc_check = (rc);                                                \
        if (red_rc_check != RED_OK) {                                              \
            printf("RC_CHECK failed :: " #rc " :: code = 0x%04x\n", red_rc_check); \
            assert(false);                                                         \
        }                                                                          \
    }

#endif