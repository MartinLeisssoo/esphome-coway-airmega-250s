#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Dgettimeofday=coway_test_gettimeofday \
  -Itests -Icomponents/coway_250s components/coway_250s/coway_250s.cpp \
  tests/protocol-test.cpp -o "$test_dir/protocol"
"$test_dir/protocol" tests/fixtures.txt
"${CXX:-c++}" -std=c++17 -Dgettimeofday=coway_test_gettimeofday \
  -Itests -Icomponents/coway_250s components/coway_250s/coway_250s.cpp \
  components/coway_250s/coway_fan.cpp tests/fan-test.cpp -o "$test_dir/fan"
"$test_dir/fan"
