#!/usr/bin/env bash
set -euo pipefail

ROOT="${ALL_CHECKS_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$ROOT"

export PREBYTE_PBT_ITERATIONS="${PREBYTE_PBT_ITERATIONS:-250}"

run_step() {
    printf '\n========== %s ==========\n' "$1"
    shift
    "$@"
}

run_step "Coverage (tests + 85%% gate)" make coverage
if [[ "${PREBYTE_ENGINE_SKIP_STATIC_ANALYSIS:-}" != "1" ]]; then
    run_step "Static analysis (clang-tidy analyze, lint, security)" make static-analysis
else
    printf '\nSkipping static analysis (PREBYTE_ENGINE_SKIP_STATIC_ANALYSIS=1).\n'
fi
run_step "Install + find_package smoke" make install-smoke
run_step "ASan/UBSan tests" make sanitize
run_step "ThreadSanitizer tests" make tsan
if [[ "${PREBYTE_ENGINE_SKIP_FUZZ:-}" != "1" ]]; then
    run_step "libFuzzer targets + regression replay" make fuzz
else
    printf '\nSkipping libFuzzer targets (PREBYTE_ENGINE_SKIP_FUZZ=1).\n'
fi

if [[ "${PREBYTE_ENGINE_ALL_CHECKS_MSAN:-}" == "1" ]]; then
    run_step "MemorySanitizer tests" make msan
fi

printf '\n=== All engine checks passed ===\n'
