# Project instructions

Read `docs/DEVELOPMENT_HANDOFF.md` before continuing motor development. Then use
`docs/UPSTREAM_INTEGRATION.md` for build-profile details and
`docs/LIVE_TUNING_FRONTEND.md` for MCUViewer operation. A local, ignored
`CONTEXT.md` may contain more recent session notes; check it when present.

- Development writes/pushes are local or to `scottnortman/stm32-servo-drive` only.
  `jnzim/stm32-servo-drive` is an upstream source for fetching updates. Do not
  push there or open upstream pull requests. Its local push URL is disabled.
- Reuse the repository's existing PI, PWM, ADC and trajectory modules. Preserve
  the standalone bench build profile when integrating upstream changes.
- The default `fw.elf` is a 24 V AKM/encoder application. The standalone GEMS
  harness uses `repo-current-tuning.elf`, a 12 V bench profile and no rotor sensor.
- Obtain fresh confirmation that EVM power is OFF before flashing or resetting
  the MCU. Do not assume a previous session's power state still holds. Preserve
  USER arming, current/ADC/fault guards and disabled-output startup.
- COM3 belongs to the user's serial console. Do not take it without instruction.
- For relevant firmware changes run upstream host tests and the current-tuning
  tests described in the handoff, and build affected firmware targets. Separate
  software validation from actual bench validation in reports.
- Keep the handoff updated with material bench results and changes in next steps.
