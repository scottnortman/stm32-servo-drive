# 5 A reciprocal motion at twice speed

Scott reports V4 works and is smoother, then requests double speed and reflash.
Separate target `repo-reciprocal-5000ma-2xspeed.elf/.hex` retains V4 configuration
fix and all current/control/protection settings. REPO_RECIP_DOUBLE_SPEED selects
200000 samples per leg rather than400000 at20kHz:10s per commanded revolution,
forward/back continuously. Same1s alignment and cosine trajectory, zero endpoint
velocity. Average6rpm per leg, peak about9.42rpm. Peak acceleration rises4x because
the same trajectory is traversed in half the time. Travel is unchanged.

5A phase target,gain20,3.2V phase ceiling,6.5A filtered/8A raw trips,28cycle ADC,
12V/2.5A input limit remain unchanged. No control, PWM, ADC or trajectory algorithm
changes; only duration selection, target and identifying console text. V4 slow
HEX preserved. Source test step bound now derives from actual profile duration.

ASan/UBSan host test links actual repo helper and checks4alternating legs,
4electrical turns/leg, bounded continuous phase, exact commanded endpoints and
zero endpoint velocity. Build/vector/call-chain/FPU checks pass. Existing unused
static declaration warning remains. Actual shaft smoothness is a bench observation.

HEX SHA256: 011828E15137B1FF06611DB660D1806198ED0D77B28D2B96BE678FB3AE00ED46
Programmed/verified/reset after fresh EVM OFF confirmation. Boot selftestPASS,
counters0,all drive enablesLOW,ADC28cycles,no corefault,interrupts enabled. After flash apply12V/2.5A,USER start;
observe forward/back pair (~21s including alignment),thenUSER stop/report.
