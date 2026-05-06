#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
ENGINE_TEST_BIN="${BUILD_DIR}/calculator_engine_test"
INPUT_TEST_BIN="${BUILD_DIR}/calculator_input_test"

mkdir -p "${BUILD_DIR}"

c++ -std=c++17 -Wall -Wextra -pedantic \
    -I"${PROJECT_DIR}/main/include" \
    "${PROJECT_DIR}/main/src/calculator_engine.cpp" \
    "${SCRIPT_DIR}/calculator_engine_test.cpp" \
    -o "${ENGINE_TEST_BIN}"

c++ -std=c++17 -Wall -Wextra -pedantic \
    -I"${PROJECT_DIR}/main/include" \
    "${PROJECT_DIR}/main/src/calculator_input.cpp" \
    "${SCRIPT_DIR}/calculator_input_test.cpp" \
    -o "${INPUT_TEST_BIN}"

"${ENGINE_TEST_BIN}"
"${INPUT_TEST_BIN}"
