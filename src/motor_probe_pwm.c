#include "motor_probe_pwm.h"

_Static_assert(MOTOR_PROBE_MAX_CCR % 2u == 0u, "Balanced pair ceiling must be even");
#ifdef DRV_GATE_TORQUE_700MA
_Static_assert(MOTOR_PROBE_MAX_CCR * 100u <= MOTOR_PWM_ARR * 8u, "Differential ceiling <=8percent");
#elif defined(DRV_GATE_CRAWL_350MA)
_Static_assert(MOTOR_PROBE_MAX_CCR * 10000u <= MOTOR_PWM_ARR * 448u, "Differential ceiling <=4.48percent");
#elif defined(DRV_GATE_CRAWL_250MA)
_Static_assert(MOTOR_PROBE_MAX_CCR * 1000u <= MOTOR_PWM_ARR * 32u, "Differential ceiling <=3.2percent");
#else
_Static_assert(MOTOR_PROBE_MAX_CCR * 100u <= MOTOR_PWM_ARR * 2u, "Differential ceiling <=2percent");
#endif

bool motor_probe_pwm_compares(uint8_t source, uint8_t sink, uint16_t differential,
                              uint16_t compare[3])
{
    if (source >= 3u || sink >= 3u || source == sink || differential > MOTOR_PROBE_MAX_CCR)
    { return false; }
    const uint16_t half = differential / 2u;
    compare[0] = compare[1] = compare[2] = MOTOR_PWM_CENTER;
    compare[source] += half;
    compare[sink] -= half;
    return true;
}

bool motor_probe_pwm_valid(uint8_t source, uint8_t sink, const uint16_t compare[3])
{
    if (source >= 3u || sink >= 3u || source == sink) { return false; }
    const unsigned int unused = 3u - source - sink;
    return compare[source] >= MOTOR_PWM_CENTER && compare[sink] <= MOTOR_PWM_CENTER &&
           compare[source] <= MOTOR_PWM_CENTER + MOTOR_PROBE_MAX_CCR / 2u &&
           compare[sink] >= MOTOR_PWM_CENTER - MOTOR_PROBE_MAX_CCR / 2u &&
           compare[source] + compare[sink] == 2u * MOTOR_PWM_CENTER &&
           compare[unused] == MOTOR_PWM_CENTER;
}
