#include "TestHarness.h"

#include "EngineTestHelpers.h"

#include "runtime/compiled/CompiledTemplateCompiler.h"
#include "runtime/compiled/CompiledTemplateSerializer.h"
#include "support/Diagnostic.h"

#include <filesystem>
#include <fstream>

namespace {

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path engine_feature_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-engine-feature-tests" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

}

TEST_CASE(Engine_render_file_builtins_from_input_path) {
    const std::filesystem::path input_path = std::filesystem::temp_directory_path() / "prebyte-builtins" / "demo.pbt";
    write_file(input_path, "{{ __FILENAME__ }}|{{ __EXTENSION__ }}|{{ __DIR__ }}");

    const std::string output = prebyte::test::helpers::render_file(input_path);
    REQUIRE(output.find("demo|pbt|") == 0);
}

TEST_CASE(Engine_render_top_level_pbc_file_with_functions_and_builtins) {
    const std::filesystem::path root = engine_feature_root("top-level-pbc-functions");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::string source =
        "{{ fn greet(name) }}Hello {{ name }} from {{ __OS__ }}{{ endfn }}{{ greet(\"Ada\") }}";

    write_file(source_path, source);
    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source(source, source_path, source_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(compiled_path, serializer.serialize(program));

    const std::string output = prebyte::test::helpers::render_file(compiled_path);
    REQUIRE(output.find("Hello Ada from ") == 0);
}

TEST_CASE(Engine_function_wrong_arity_fails) {
    prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ fn greet(name) }}{{ name }}{{ endfn }}{{ greet() }}";
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(options), prebyte::DiagnosticError);
}

TEST_CASE(Engine_render_functions_with_structured_imports_and_loop) {
    const std::filesystem::path root = engine_feature_root("functions-structured-imports");
    const std::filesystem::path items_path = root / "items.yaml";
    write_file(items_path, "- Ada\n- Grace\n");

    const std::string output = prebyte::test::helpers::render_inline(
        "{{ fn wrap(name) }}<{{ name }}>{{ endfn }}"
        "{{ for item in items }}{{ wrap(item) }}{{ endfor }}",
        {"items=@" + items_path.string()});
    REQUIRE_EQ(output, std::string("<Ada><Grace>"));
}
