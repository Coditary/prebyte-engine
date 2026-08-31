.PHONY: build test coverage static-analysis sanitize tsan msan fuzz install-smoke all-checks

BUILD_DIR ?= build
CMAKE_ARGS ?=

build:
	cmake --preset dev $(CMAKE_ARGS)
	cmake --build --preset dev --parallel

test: build
	ctest --preset dev

coverage:
	cmake --preset coverage $(CMAKE_ARGS)
	cmake --build --preset coverage-tests --parallel
	ctest --preset coverage
	chmod +x scripts/ci/generate_coverage_report.sh
	COVERAGE_MIN_LINE=85 ./scripts/ci/generate_coverage_report.sh

static-analysis:
	chmod +x scripts/ci/run_clang_tidy.sh
	./scripts/ci/run_clang_tidy.sh all $(CMAKE_ARGS)

sanitize:
	chmod +x scripts/ci/run_sanitize_tests.sh
	./scripts/ci/run_sanitize_tests.sh asan $(CMAKE_ARGS)

tsan:
	chmod +x scripts/ci/run_sanitize_tests.sh
	./scripts/ci/run_sanitize_tests.sh tsan $(CMAKE_ARGS)

msan:
	chmod +x scripts/ci/run_sanitize_tests.sh scripts/ci/bootstrap_msan_libcxx.sh
	./scripts/ci/run_sanitize_tests.sh msan $(CMAKE_ARGS)

fuzz:
	chmod +x scripts/ci/run_fuzzers.sh
	./scripts/ci/run_fuzzers.sh $(CMAKE_ARGS)

install-smoke: build
	chmod +x scripts/ci/smoke_install.sh
	INSTALL_SMOKE_BUILD_DIR=build-cmake/dev ./scripts/ci/smoke_install.sh $(CMAKE_ARGS)

all-checks:
	chmod +x scripts/ci/run_all_checks.sh
	./scripts/ci/run_all_checks.sh
