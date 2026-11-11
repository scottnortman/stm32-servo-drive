#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
gcc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer -IInclude src/control.c src/current_tuning.c tests/current_tuning_test.c -lm -o /tmp/current-tuning-tests
/tmp/current-tuning-tests
gcc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer -DREPO_TUNING_24V -IInclude src/control.c src/current_tuning.c tests/current_tuning_test.c -lm -o /tmp/current-tuning-24v-tests
/tmp/current-tuning-24v-tests
gcc -std=c11 -Wall -Wextra -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-sign-compare -g -no-pie -fsanitize=address,undefined -fno-omit-frame-pointer -Itests/mcuviewer_stubs -IInclude -Ithird_party/mcuviewer_recorder src/control.c src/current_tuning.c src/tune_gain_store.c src/repo_tune_mcuviewer.c third_party/mcuviewer_recorder/recorder.c tests/mcuviewer_tuning_test.c -lm -o /tmp/mcuviewer-tuning-tests
/tmp/mcuviewer-tuning-tests
gcc -std=c11 -Wall -Wextra -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-sign-compare -g -no-pie -fsanitize=address,undefined -fno-omit-frame-pointer -DREPO_TUNING_24V -Itests/mcuviewer_stubs -IInclude -Ithird_party/mcuviewer_recorder src/control.c src/current_tuning.c src/tune_gain_store.c src/repo_tune_mcuviewer.c third_party/mcuviewer_recorder/recorder.c tests/mcuviewer_tuning_test.c -lm -o /tmp/mcuviewer-tuning-24v-tests
/tmp/mcuviewer-tuning-24v-tests
gcc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer -IInclude src/tune_gain_store.c tests/tune_gain_store_test.c -lm -o /tmp/tune-gain-store-tests
/tmp/tune-gain-store-tests
gcc -std=c11 -Wall -Wextra -Werror -IInclude tests/bench_profile_test.c -lm -o /tmp/upstream-profile-test
/tmp/upstream-profile-test
gcc -std=c11 -Wall -Wextra -Werror -IInclude -DREPO_MOTOR_BENCH -DREPO_RECIPROCAL -DREPO_CURRENT_5000MA -DREPO_CURRENT_6250MA -DSHUNT_GAIN=20.0f tests/bench_profile_test.c -lm -o /tmp/bench-profile-test
/tmp/bench-profile-test
gcc -std=c11 -Wall -Wextra -Werror -IInclude -DREPO_MOTOR_BENCH -DREPO_TUNING_24V -DREPO_RECIPROCAL -DREPO_CURRENT_5000MA -DREPO_CURRENT_6250MA -DSHUNT_GAIN=20.0f tests/bench_profile_test.c -lm -o /tmp/bench-24v-profile-test
/tmp/bench-24v-profile-test
