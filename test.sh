#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
output="$(mktemp /tmp/adv-airmouse-test.XXXXXX)"
trap 'rm -f "$output"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -Iinclude tests/test_motion.cpp -o "$output"
"$output"
echo 'Motion and arming tests passed'
