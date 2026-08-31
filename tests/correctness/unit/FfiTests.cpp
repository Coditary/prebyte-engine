#include "TestHarness.h"

#include "EngineTestHelpers.h"

#include "prebyte_engine.h"

#include <cstring>
#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path ffi_test_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-ffi-tests" / name;
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root);
    return root;
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

}

TEST_CASE(Ffi_version_returns_non_empty_string) {
    REQUIRE(std::strlen(prebyte_version()) > 0);
}

TEST_CASE(Ffi_session_render_returns_expected_output) {
    prebyte_session_t* session = prebyte_session_create();
    REQUIRE(session != nullptr);
    REQUIRE(prebyte_session_set_variable(session, "name", "Ada") == 0);

    char* output = prebyte_session_render(session, "Hello {{ name }}");
    REQUIRE(output != nullptr);
    REQUIRE(std::strcmp(output, "Hello Ada") == 0);

    prebyte_string_free(output);
    prebyte_session_destroy(session);
}

TEST_CASE(Ffi_engine_compile_and_render_returns_expected_output) {
    prebyte_engine_t* engine = prebyte_engine_create();
    REQUIRE(engine != nullptr);

    prebyte_template_t* compiled = prebyte_engine_compile(engine, "Hi {{ name }}", "inline.txt");
    REQUIRE(compiled != nullptr);

    prebyte_render_context_t* context = prebyte_render_context_create();
    REQUIRE(context != nullptr);
    REQUIRE(prebyte_render_context_set(context, "name", "Grace") == 0);

    char* output = prebyte_engine_render(engine, compiled, context);
    REQUIRE(output != nullptr);
    REQUIRE(std::strcmp(output, "Hi Grace") == 0);

    prebyte_string_free(output);
    prebyte_render_context_destroy(context);
    prebyte_template_destroy(compiled);
    prebyte_engine_destroy(engine);
}

TEST_CASE(Ffi_invalid_arguments_set_last_error) {
    prebyte_clear_last_error();
    REQUIRE(prebyte_session_render(nullptr, "x") == nullptr);
    REQUIRE(std::strlen(prebyte_get_last_error()) > 0);
    prebyte_clear_last_error();
    REQUIRE(std::strlen(prebyte_get_last_error()) == 0);
}

TEST_CASE(Ffi_session_setters_and_file_render_paths) {
    const std::filesystem::path root = ffi_test_root("session-file-render");
    const std::filesystem::path template_file = root / "template.pbt";
    const std::filesystem::path output_file = root / "output.txt";
    write_file(template_file, "Hello {{ name }}");

    prebyte_session_t* session = prebyte_session_create();
    REQUIRE(session != nullptr);
    REQUIRE(prebyte_session_set_variable(session, "name", "Ada") == 0);

    char* rendered_file = prebyte_session_render_file(session, template_file.string().c_str());
    REQUIRE(rendered_file != nullptr);
    REQUIRE(std::strcmp(rendered_file, "Hello Ada") == 0);
    prebyte_string_free(rendered_file);

    REQUIRE(prebyte_session_render_to_file(session, "Hi {{ name }}", output_file.string().c_str()) == 0);
    REQUIRE_EQ(read_file(output_file), std::string("Hi Ada"));

    const std::filesystem::path copied_output = root / "copied.txt";
    REQUIRE(prebyte_session_render_file_to_file(session, template_file.string().c_str(), copied_output.string().c_str()) == 0);
    REQUIRE_EQ(read_file(copied_output), std::string("Hello Ada"));

    REQUIRE(prebyte_session_set_variable_flag(session, "enabled") == 0);
    REQUIRE(prebyte_session_add_argument(session, "--verbose") == 0);
    REQUIRE(prebyte_session_add_include_path(session, root.string().c_str()) == 0);
    REQUIRE(prebyte_session_set_ignore(session, "secret") == 0);
    REQUIRE(prebyte_session_set_rule(session, "trim", "true") == 0);
    REQUIRE(prebyte_session_set_profile(session, "dev") == 0);

    prebyte_session_destroy(session);
}

TEST_CASE(Ffi_session_setters_reject_invalid_arguments) {
    prebyte_clear_last_error();
    REQUIRE(prebyte_session_set_variable(nullptr, "name", "Ada") == -1);
    REQUIRE(prebyte_session_set_variable_flag(nullptr, "flag") == -1);
    REQUIRE(prebyte_session_add_argument(nullptr, "arg") == -1);
    REQUIRE(prebyte_session_add_include_path(nullptr, "/tmp") == -1);
    REQUIRE(prebyte_session_set_profile(nullptr, "dev") == -1);
    REQUIRE(prebyte_session_set_ignore(nullptr, "secret") == -1);
    REQUIRE(prebyte_session_set_rule(nullptr, "trim", "true") == -1);
    REQUIRE(prebyte_session_render_to_file(nullptr, "x", "y") == -1);
    REQUIRE(prebyte_session_render_file_to_file(nullptr, "x", "y") == -1);
    REQUIRE(prebyte_session_render_file(nullptr, "x") == nullptr);
    REQUIRE(std::strlen(prebyte_get_last_error()) > 0);
}

TEST_CASE(Ffi_session_create_with_settings_empty_path) {
    prebyte_session_t* session = prebyte_session_create_with_settings(nullptr);
    REQUIRE(session != nullptr);
    REQUIRE(prebyte_session_set_variable(session, "name", "Ada") == 0);
    char* output = prebyte_session_render(session, "Hi {{ name }}");
    REQUIRE(output != nullptr);
    REQUIRE(std::strcmp(output, "Hi Ada") == 0);
    prebyte_string_free(output);
    prebyte_session_destroy(session);
}

TEST_CASE(Ffi_engine_compile_file_and_render_with_args) {
    const std::filesystem::path root = ffi_test_root("engine-compile-file");
    const std::filesystem::path template_file = root / "template.txt";
    write_file(template_file, "Args={{ ARGS[0] }}");

    prebyte_engine_t* engine = prebyte_engine_create();
    REQUIRE(engine != nullptr);

    prebyte_template_t* compiled = prebyte_engine_compile_file(engine, template_file.string().c_str());
    REQUIRE(compiled != nullptr);

    prebyte_render_context_t* context = prebyte_render_context_create();
    REQUIRE(context != nullptr);
    const char* args[] = {"one", "two"};
    REQUIRE(prebyte_render_context_set_args(context, args, 2) == 0);

    char* with_context = prebyte_engine_render(engine, compiled, context);
    REQUIRE(with_context != nullptr);
    REQUIRE(std::strcmp(with_context, "Args=one") == 0);
    prebyte_string_free(with_context);

    prebyte_template_t* static_compiled = prebyte_engine_compile(engine, "Static", "inline.txt");
    REQUIRE(static_compiled != nullptr);
    char* without_context = prebyte_engine_render(engine, static_compiled, nullptr);
    REQUIRE(without_context != nullptr);
    REQUIRE(std::strcmp(without_context, "Static") == 0);
    prebyte_string_free(without_context);
    prebyte_template_destroy(static_compiled);

    prebyte_clear_last_error();
    REQUIRE(prebyte_render_context_set(nullptr, "name", "Ada") == -1);
    REQUIRE(prebyte_render_context_set_args(nullptr, args, 1) == -1);
    REQUIRE(prebyte_engine_compile(nullptr, "x", "inline") == nullptr);
    REQUIRE(prebyte_engine_compile_file(nullptr, template_file.string().c_str()) == nullptr);
    REQUIRE(prebyte_engine_render(nullptr, compiled, context) == nullptr);
    REQUIRE(prebyte_engine_render(engine, nullptr, context) == nullptr);
    REQUIRE(std::strlen(prebyte_get_last_error()) > 0);

    prebyte_render_context_destroy(context);
    prebyte_template_destroy(compiled);
    prebyte_engine_destroy(engine);
}
