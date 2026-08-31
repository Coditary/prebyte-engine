#include "PrebyteEngine.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main() {
    prebyte::Prebyte engine;
    engine.set_variable("name", "Ada");
    const std::string output = engine.process("Hello {{ name }}");
    if (output != "Hello Ada") {
        std::cerr << "unexpected output: " << output << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
