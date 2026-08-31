# prebyte-engine

C++23 templating engine extracted from [Prebyte](https://github.com/Coditary/Prebyte). Provides the core parser, compiler, renderer, Lua sandbox, and embed APIs.

## Targets

| Target | Alias | Description |
| --- | --- | --- |
| `prebyte_engine_static` | `prebyte::engine_static` | Static library for CLI and single-binary embedders |
| `prebyte_engine` | `prebyte::engine` | Shared C++ library |
| `prebyte_engine_c` | `prebyte::engine_c` | C FFI shared library for other languages |

## Public APIs

### C++ high-level

```cpp
#include "PrebyteEngine.h"

prebyte::Prebyte engine;
engine.set_variable("name", "Ada");
std::string output = engine.process("Hello {{ name }}\n");
```

### C++ low-level

```cpp
#include "Engine.h"

prebyte::Engine engine;
auto tpl = engine.compile("Hello {{ name }}\n");
prebyte::RenderContext ctx;
ctx.set("name", "Ada");
std::string output = engine.render(tpl, ctx);
```

### C FFI

```c
#include "prebyte_engine.h"

prebyte_session_t* session = prebyte_session_create();
prebyte_session_set_variable(session, "name", "Ada");
char* output = prebyte_session_render(session, "Hello {{ name }}\n");
prebyte_string_free(output);
prebyte_session_destroy(session);
```

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Requirements: CMake ≥ 3.31, C++23 compiler, Lua development package.

## Install

```bash
cmake --install build
```

Installs headers (`Engine.h`, `PrebyteEngine.h`, `prebyte_engine.h`), libraries, and `prebyte-engine-config.cmake`.

## Consumption from Prebyte CLI

The CLI repo adds this project as a sibling directory:

```cmake
add_subdirectory(../prebyte-engine prebyte-engine-build)
target_link_libraries(prebyte PRIVATE prebyte::engine_static)
```

## License

MIT. See `LICENSE`.
