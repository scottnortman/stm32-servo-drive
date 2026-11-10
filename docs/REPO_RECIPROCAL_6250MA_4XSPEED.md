#6.25A reciprocal motion,5seconds each way

User reports V5working and requests another2x speed plus25% current. Separate
target repo-reciprocal-6250ma-4xspeed (V6) retains original repo control/PWM/ADC
functions, gain20 and V4configuration fix. It inherits the REPO_CURRENT_5000MA
gain20 setup path, with REPO_CURRENT_6250MA selecting new profile values.

Motion:100000 samples/leg at20kHz=5seconds,1commanded revolution forward/back;
samecosine easing and1salignment. Average12rpm/leg,peak18.85rpm. No timed total
stop; USER/fault stops. Nominal phase magnitude6.25A,4V voltage ceiling,
7.5A filtered/8A sampled raw trips. Supply12V/3.5A INPUT limit.

The4V ceiling needs a shorter quiet-window sequence: board sample setting15cycles
at25MHz and CCR4=2489 (formerly28cycles/2449). Original trigger routing/ADC ISR
are unchanged. Max phaseCCR2081 gives next switching edge29.17us. Latest trigger
25.09us+3.24us sequence+0.13us maximum latency=28.46us,0.71us margin. Boot checks
both sample setting and trigger compare. TI56ohm/2.2nF output filter plus7pF ADC
charge-sharing model gives residual2.47e-5 fullscale after0.6us acquisition,
below0.5LSB. This model does not substitute for hardware observation.

Raw scaling/watchdog range unchanged from5A,gain20,8A raw1390counts. Filtered
trip raised to7.5A for6.25A target. All USER,nFAULT,ADC freshness/rail/offset and
configuration guards retained; fixed12V assumption after bus preflight retained.

Build and ASan/UBSan trajectory tests PASS:4alternating legs,4electrical turns
perleg,continuous wrapped phase and zero endpoint velocity. Link/vector/earlyFPU,
compiled profile/timing checks PASS. Existing unused header-static warning remains.
Prior working5A/10s HEX unchanged. Motor-control and trajectory algorithms unchanged.

HEX SHA256: 9275845060B8FC061B4B8F4D7DFEC533ED96EBAA1B2F4367D6D6A21FD8F1DF37
Evidence:docs/bench/2026-11-10-repo-reciprocal-6250ma-4xspeed-*
Programmed/verified/reset after fresh EVM OFF confirmation. Boot selftestPASS,
counters0,driveenablesLOW,ADC15cycles/CCR42489,no corefault,interrupts enabled. Afterflash12V/3.5A,USERstart;compare
one forward/back pair (~11seconds including alignment),USERstop/report.

User checkpoint (2026-11-10T12:55:22-05:00): Scott reports "it is working". Preserve V6 as
the current working baseline. No additional speed/current changes are pending.
