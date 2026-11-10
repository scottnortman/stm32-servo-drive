#include "motor_probe_pwm.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
#ifdef DRV_GATE_CRAWL_350MA
    const uint16_t expected_min=597u,expected_max=653u;
    assert(MOTOR_PROBE_MAX_CCR==56u && MOTOR_PROBE_TARGET_MA==350u);
#elif defined(DRV_GATE_CRAWL_250MA)
    const uint16_t expected_min=605u,expected_max=645u;
    assert(MOTOR_PROBE_MAX_CCR==40u && MOTOR_PROBE_TARGET_MA==250u);
#else
    const uint16_t expected_min=613u,expected_max=637u;
    assert(MOTOR_PROBE_MAX_CCR==24u && MOTOR_PROBE_TARGET_MA==100u);
#endif
    assert(MOTOR_PROBE_FILTER_TRIP_MA==500u && MOTOR_PROBE_RAW_TRIP_MA==1000u);
    uint16_t compare[3];
    for (uint8_t source = 0u; source < 3u; ++source)
    for (uint8_t sink = 0u; sink < 3u; ++sink)
    {
        if (source == sink) { continue; }
        for (uint16_t request = 0u; request <= MOTOR_PROBE_MAX_CCR; ++request)
        {
            assert(motor_probe_pwm_compares(source, sink, request, compare));
            assert(motor_probe_pwm_valid(source, sink, compare));
            assert((uint32_t)(compare[source] - compare[sink]) == (request / 2u) * 2u);
            assert(compare[source] - compare[sink] <= MOTOR_PROBE_MAX_CCR);
            for (unsigned int c = 0u; c < 3u; ++c)
            {
                assert(compare[c] >= expected_min && compare[c] <= expected_max);
                // Existing trigger at850 and3x68 ADC cycles (timer/ADC=4).
                assert(compare[c] + 100u <= 850u);
                assert(850u + 3u * 68u * 4u + 50u < 2500u - compare[c]);
            }
            if (request == 0u)
                assert(compare[0] == 625u && compare[1] == 625u && compare[2] == 625u);
        }
    }
    assert(!motor_probe_pwm_compares(0u, 0u, 0u, compare));
    assert(!motor_probe_pwm_compares(3u, 0u, 0u, compare));
    assert(!motor_probe_pwm_compares(0u, 1u, MOTOR_PROBE_MAX_CCR + 1u, compare));
    assert(!motor_probe_pwm_valid(0u, 1u, (uint16_t[3]){25u,0u,0u}));
    assert(!motor_probe_pwm_valid(0u, 1u, (uint16_t[3]){(uint16_t)(expected_max+1u),(uint16_t)(expected_min-1u),625u}));
    assert(!motor_probe_pwm_valid(0u, 1u, (uint16_t[3]){637u,613u,626u}));
    assert(!motor_probe_pwm_valid(0u, 0u, (uint16_t[3]){625u,625u,625u}));
#ifdef DRV_GATE_CRAWL_350MA
    puts("PASS: 350mA/4.48percent PWM bounds, unchanged500mA/1A trips, all pairs and ADC quiet window");
#elif defined(DRV_GATE_CRAWL_250MA)
    puts("PASS: 250mA/3.2percent PWM bounds, unchanged500mA/1A trips, all pairs and ADC quiet window");
#else
    puts("PASS: all phase pairs, centered zero, <=1.92percent differential, leg bounds and ADC quiet window");
#endif
    return 0;
}
