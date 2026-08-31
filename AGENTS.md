# prebyte-engine

C++23 templating engine extracted from Prebyte. Parser, compiler, renderer, Lua sandbox, and embed APIs.

## Targets

| Target | Alias | Description |
| --- | --- | --- |
| `prebyte_engine_static` | `prebyte::engine_static` | Static library |
| `prebyte_engine` | `prebyte::engine` | Shared C++ library |
| `prebyte_engine_c` | `prebyte::engine_c` | C FFI shared library |

## Layout

```
prebyte-engine/
├── include/              # Public + internal headers (flat include paths)
│   ├── Engine.h          # Low-level compile/render API
│   ├── PrebyteEngine.h   # High-level session API
│   └── prebyte_engine.h  # C FFI
├── src/
│   ├── config/ io/ parser/ runtime/ template/ support/ datatypes/
│   ├── PrebyteEngine.cpp
│   └── ffi/prebyte_engine_c.cpp
└── cmake/prebyte-engine-config.cmake.in
```

## APIs

### C++ high-level (`PrebyteEngine.h`)

```cpp
prebyte::Prebyte engine;
engine.set_variable("name", "Ada");
std::string out = engine.process("Hello {{ name }}");
```

### C++ low-level (`Engine.h`)

```cpp
prebyte::Engine engine;
auto tpl = engine.compile("Hello {{ name }}");
prebyte::RenderContext ctx;
ctx.set("name", "Ada");
std::string out = engine.render(tpl, ctx);
```

### C FFI (`prebyte_engine.h`)

```c
prebyte_session_t* s = prebyte_session_create();
prebyte_session_set_variable(s, "name", "Ada");
char* out = prebyte_session_render(s, "Hello {{ name }}");
prebyte_string_free(out);
prebyte_session_destroy(s);
```

Errors: check return codes / null pointers, then `prebyte_get_last_error()`. Strings from FFI must be freed with `prebyte_string_free()`.

## Build

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Requirements: CMake ≥ 3.31, C++23, Lua dev package.

## Tests & CI

| Area | Command |
| --- | --- |
| Unit/integration tests | `ctest --preset dev` |
| Coverage (85% gate) | `make coverage` |
| clang-tidy | `make static-analysis` |
| ASan/UBSan | `make sanitize` |
| TSan | `make tsan` |
| MSan | `make msan` |
| libFuzzer | `make fuzz` |
| Install + `find_package` smoke | `make install-smoke` |
| Full local QA loop | `make all-checks` |

CI workflow: `.github/workflows/ci.yml` — multi-platform build/test, install smoke, coverage, clang-tidy, sanitizers, fuzzers, and tagged library release artifacts.

Install smoke (`tests/cmake-consumer/`) verifies `find_package(prebyte-engine)` for C++ (`prebyte::engine_static`) and C FFI (`prebyte::engine_c`).

## Consumption from Prebyte CLI

```cmake
add_subdirectory(../prebyte-engine prebyte-engine-build)
target_link_libraries(prebyte_cli PUBLIC prebyte::engine_static)
```

Override engine path: `-DPREBYTE_ENGINE_SOURCE_DIR=/path/to/prebyte-engine`.

## Code style

- Namespace `prebyte`; errors via `DiagnosticError`.
- `Value` = runtime type; `Data` = config/import type.
- Keep module boundaries: lexer/parser/runtime/config separate.
- Lua sandbox blocks `os`, `io`, `debug`, `package`, `require`, `dofile`, `loadfile`.

## Boundaries

- No CLI code here (`cli/`, `app/`, `main.cpp` stay in Prebyte repo).
- `.pbc` write API is internal; public API loads/compiles but does not expose serialization yet.
- Thread safety: `Engine::render()` is parallel-safe; `Prebyte` serializes config + render on one instance.

## License

MIT. See `LICENSE`.
