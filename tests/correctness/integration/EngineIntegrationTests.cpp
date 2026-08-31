#include "TestHarness.h"

#include "EngineTestHelpers.h"

#include "runtime/expression/BuiltinRegistry.h"
#include "runtime/expression/ExpressionEvaluator.h"
#include "runtime/resolution/IncludeResolver.h"
#include "runtime/render/Renderer.h"
#include "config/RuleResolver.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>

#include "Engine.h"
#include "PrebyteEngine.h"
#include "io/InputBuffer.h"
#include "runtime/compiled/CompiledTemplateCompiler.h"
#include "runtime/compiled/CompiledTemplateSerializer.h"
#include "support/Diagnostic.h"

namespace {

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::filesystem::path test_temp_root(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "prebyte-tests" / name;
    std::error_code error;
    if (std::filesystem::exists(root, error)) {
        std::filesystem::permissions(root,
                                     std::filesystem::perms::owner_all | std::filesystem::perms::group_all
                                         | std::filesystem::perms::others_all,
                                     std::filesystem::perm_options::add,
                                     error);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied, error)) {
            std::filesystem::permissions(entry.path(),
                                         std::filesystem::perms::owner_all | std::filesystem::perms::group_all
                                             | std::filesystem::perms::others_all,
                                         std::filesystem::perm_options::add,
                                         error);
        }
    }
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

const char* shared_root_environment_name() {
#ifdef _WIN32
    return "LOCALAPPDATA";
#else
    return "HOME";
#endif
}

std::filesystem::path shared_root_environment_value(const std::filesystem::path& root) {
#ifdef _WIN32
    return root / "AppData" / "Local";
#else
    return root;
#endif
}

std::filesystem::path default_shared_root_for_test(const std::filesystem::path& root) {
#ifdef _WIN32
    return shared_root_environment_value(root) / "Prebyte" / "share";
#else
    return root / ".local" / "share" / "prebyte";
#endif
}

}

TEST_CASE(AppRunner_render_simple_fixture) {

        prebyte::test::helpers::RenderOptions options;
    options.input_path = "tests/fixtures/render_simple/input.txt";
    options.define_args = {"name=Ada"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Hello Ada\n"));

}
TEST_CASE(AppRunner_rules_variable_delimiters_replace_tabs_and_tab_size_affect_render) {
    prebyte::RuleResolver rule_resolver;
    prebyte::IncludeResolver include_resolver;
    prebyte::BuiltinRegistry builtins;
    prebyte::ExpressionEvaluator evaluator{builtins};
    prebyte::Renderer renderer{rule_resolver, include_resolver, evaluator};
    const prebyte::ResolvedConfiguration configuration = rule_resolver.resolve(
        prebyte::SettingsData{},
        {"variable_prefix=<<", "variable_suffix=>>", "replace_tabs=true", "tab_size=2"},
        {},
        {},
        false);
    const prebyte::EffectiveSettings settings = rule_resolver.resolve_for_file(configuration, {});
    prebyte::RenderSession session;
    session.variables.set("name", "Ada");
    const std::string output = renderer.render_source("\t<< name >>", settings, {}, session);
    REQUIRE_EQ(output, std::string("  Ada"));
}
TEST_CASE(AppRunner_legacy_include_path_rule_still_resolves_includes) {

    const std::filesystem::path root = test_temp_root("legacy-include-path-rule");
    write_file(root / "shared" / "java.pbt", "Legacy OK");

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ include \"java\" }}";
    options.rule_args = {"include_path=" + (root / "shared").string()};
    REQUIRE_EQ(prebyte::test::helpers::render(options), std::string("Legacy OK"));

}
TEST_CASE(AppRunner_output_encoding_and_error_on_false_input_rules_apply_to_file_output_and_conditions) {
    const std::filesystem::path root = test_temp_root("apprunner-rule-behavior");
    const std::filesystem::path output_path = root / "out.txt";

    try {
        prebyte::test::helpers::RenderOptions options;
        options.inline_input = "{{ if false }}bad{{ else }}ok{{ endif }}";
        options.rule_args = {"output_encoding=utf-16", "error_on_false_input=true"};
        static_cast<void>(prebyte::test::helpers::render(options));
        throw std::runtime_error("expected DiagnosticError");
    } catch (const prebyte::DiagnosticError& error) {
        REQUIRE_EQ(error.diagnostic().message, std::string("False input is not allowed in condition"));
    }

    prebyte::test::helpers::RenderOptions write_options;
    write_options.inline_input = "Hello";
    write_options.output_path = output_path;
    write_options.rule_args = {"output_encoding=utf-16", "error_on_false_input=false"};
    prebyte::test::helpers::render(write_options);

    const std::string bytes = std::string(prebyte::InputBuffer::from_file(output_path).view());
    REQUIRE_EQ(bytes.size(), static_cast<std::size_t>(12));
    REQUIRE_EQ(static_cast<unsigned char>(bytes[0]), 0xFFu);
    REQUIRE_EQ(static_cast<unsigned char>(bytes[1]), 0xFEu);

}
TEST_CASE(AppRunner_render_include_and_if_fixture) {

        prebyte::test::helpers::RenderOptions options;
    options.input_path = "tests/fixtures/render_include_if/input.txt";
    options.define_args = {"name=Ada", "enabled=true"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Header for Ada\n\nEnabled\nFooter\n"));

}
TEST_CASE(Engine_compile_file_renders_fixture_with_relative_include) {

    prebyte::Engine engine;
    const std::filesystem::path path = "tests/fixtures/render_include_if/input.txt";
    const prebyte::CompiledTemplate tpl = engine.compile_file(path);

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");
    ctx.set("enabled", "true");

    const std::string output = engine.render(tpl, ctx);
    REQUIRE_EQ(output, std::string("Header for Ada\n\nEnabled\nFooter\n"));

}
TEST_CASE(Engine_render_to_streams_fixture_with_relative_include) {

    prebyte::Engine engine;
    const std::filesystem::path path = "tests/fixtures/render_include_if/input.txt";
    const prebyte::CompiledTemplate tpl = engine.compile_file(path);

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");
    ctx.set("enabled", "true");

    std::string output;
    std::size_t chunk_count = 0;
    engine.render_to(tpl, [&](std::string_view chunk) {
        ++chunk_count;
        output.append(chunk.data(), chunk.size());
    }, ctx);

    REQUIRE_EQ(output, std::string("Header for Ada\n\nEnabled\nFooter\n"));
    REQUIRE(chunk_count > 1);

}
TEST_CASE(Engine_render_to_streams_mixed_control_flow_from_compiled_file) {

    const std::filesystem::path root = test_temp_root("engine-stream-mixed-control-flow");
    write_file(root / "partial.pbt", "{{ if member.admin }}*{{ else }}-{{ endif }}{{ member.name }}");
    write_file(root / "main.pbt",
               "{{ if members }}{{ for member in members }}{{ include \"./partial\" }};{{ endfor }}{{ elseif archived }}archived{{ else }}empty{{ endif }}");

    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.compile_file(root / "main.pbt");

    prebyte::RenderContext ctx;
    ctx.set("archived", "false");
    prebyte::Data::Array members;
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Ada")}, {"admin", prebyte::Data(true)}}));
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Grace")}, {"admin", prebyte::Data(false)}}));
    ctx.set("members", prebyte::Value::list(std::move(members)));

    std::string output;
    std::size_t chunk_count = 0;
    engine.render_to(tpl, [&](std::string_view chunk) {
        ++chunk_count;
        output.append(chunk.data(), chunk.size());
    }, ctx);

    REQUIRE_EQ(output, std::string("*Ada;-Grace;"));
    REQUIRE(chunk_count > 1);

}
TEST_CASE(AppRunner_render_settings_profile_merge_fixture) {

        prebyte::test::helpers::RenderOptions options;
    options.input_path = "tests/fixtures/settings_profile_merge/input.txt";
    options.settings_path = "tests/fixtures/settings_profile_merge/settings.yaml";
    options.define_args = {"name=Ada"};
    options.profile_names = {"friendly"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Hi Ada\n"));

}
TEST_CASE(AppRunner_apply_file_rule_to_include_extension) {

        prebyte::test::helpers::RenderOptions options;
    options.input_path = "tests/fixtures/file_rule_include/main.txt";
    options.rule_args = {".md::default_variable_value=Fallback"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Start\nValue=Fallback\n\nEnd\n"));

}
TEST_CASE(AppRunner_strict_variable_failure) {

        prebyte::test::helpers::RenderOptions options;
    options.input_path = "tests/fixtures/render_simple/input.txt";
    options.rule_args = {"strict_variables=true"};
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(options), prebyte::DiagnosticError);

}
TEST_CASE(AppRunner_render_lua_inline_fixture) {

        prebyte::test::helpers::RenderOptions options;
    options.input_path = "tests/fixtures/lua_inline/input.txt";
    options.define_args = {"name=Ada"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("ADA\n"));

}
TEST_CASE(AppRunner_render_lua_block_fixture) {

        prebyte::test::helpers::RenderOptions options;
    options.input_path = "tests/fixtures/lua_block/input.txt";
    options.define_args = {"name=Ada"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Hello Ada\n"));

}
TEST_CASE(AppRunner_render_for_else_fixture) {

    const std::filesystem::path root = test_temp_root("for-else-loop");
    const std::filesystem::path source_path = root / "sample.pbt";
    write_file(source_path, "{{ for item in missing }}{{ item }}{{ else }}empty{{ endfor }}\n");

        prebyte::test::helpers::RenderOptions options;
    options.input_path = source_path;
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("empty\n"));

}
TEST_CASE(AppRunner_render_mixed_native_lua_condition_fixture) {

        prebyte::test::helpers::RenderOptions options;
    options.input_path = "tests/fixtures/lua_condition/input.txt";
    options.define_args = {"enabled=true", "name=Ada"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("ok\n"));

}
TEST_CASE(AppRunner_native_false_string_is_false) {

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ if disabled }}bad{{ else }}ok{{ endif }}";
    options.define_args = {"disabled=false"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("ok"));

}
TEST_CASE(AppRunner_render_if_lua_block_condition) {

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ if lua:block }}\nlocal allowed = enabled == 'true'\nreturn allowed\n{{ endlua }}ok{{ else }}no{{ endif }}";
    options.define_args = {"enabled=true"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("ok"));

}
TEST_CASE(AppRunner_render_elseif_lua_block_condition) {

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ if disabled == true }}no{{ elseif lua:block }}\nlocal allowed = enabled == 'true'\nreturn allowed\n{{ endlua }}ok{{ else }}fallback{{ endif }}";
    options.define_args = {"disabled=false", "enabled=true"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("ok"));

}
TEST_CASE(AppRunner_render_lua_respects_configured_memory_limit) {

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ lua \"return string.rep('x', 2097152)\" }}";
    options.rule_args = {"lua_memory_limit_bytes=1048576"};
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(options), prebyte::DiagnosticError);

}
TEST_CASE(AppRunner_render_lua_respects_configured_instruction_limit) {

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ lua \"local sum = 0 for i = 1, 1000 do sum = sum + i end return sum\" }}";
    options.rule_args = {"lua_instruction_limit=10"};
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(options), prebyte::DiagnosticError);

}
TEST_CASE(AppRunner_render_named_structured_imports) {

    const std::filesystem::path root = test_temp_root("named-structured-imports");
    const std::filesystem::path template_path = root / "input.pbt";
    const std::filesystem::path user_path = root / "user.json";
    const std::filesystem::path items_path = root / "items.yaml";
    const std::filesystem::path config_path = root / "config.toml";

    write_file(template_path,
               "{{ user.name }}\n{{ items[1] }}\n{{ config.server.host }}:{{ config.server.port }}\n");
    write_file(user_path, "{\"name\":\"Ada\"}");
    write_file(items_path, "- Ada\n- Grace\n");
    write_file(config_path, "[server]\nhost=\"localhost\"\nport=8080\n");

        prebyte::test::helpers::RenderOptions options;
    options.input_path = template_path;
    options.define_args = {
        "user=@" + user_path.string(),
        "items=@" + items_path.string(),
        "config=@" + config_path.string(),
    };
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Ada\nGrace\nlocalhost:8080\n"));

}
TEST_CASE(AppRunner_render_args_are_available_in_native_and_lua) {

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ ARGS[0] }} {{ lua \"return ARGS[2]\" }}";
    options.render_args = {"alpha", "beta", "gamma"};
    const std::string output = prebyte::test::helpers::render(options);

    REQUIRE_EQ(output, std::string("alpha gamma"));

}
TEST_CASE(AppRunner_forbidden_env_vars_override_allow_env) {

    prebyte::test::ScopedEnvironmentVariable allowed_env("PREBYTE_APP_ALLOWED_ENV", "Ada");
    prebyte::test::ScopedEnvironmentVariable blocked_env("PREBYTE_APP_BLOCKED_ENV", "Secret");

    prebyte::test::helpers::RenderOptions allowed_options;
    allowed_options.inline_input = "{{ PREBYTE_APP_ALLOWED_ENV }}";
    allowed_options.rule_args = {"allow_env=true", "forbidden_env_vars=PREBYTE_APP_BLOCKED_ENV"};
    REQUIRE_EQ(prebyte::test::helpers::render(allowed_options), std::string("Ada"));

    prebyte::test::helpers::RenderOptions blocked_options = allowed_options;
    blocked_options.inline_input = "{{ PREBYTE_APP_BLOCKED_ENV }}";
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(blocked_options), prebyte::DiagnosticError);

}
TEST_CASE(AppRunner_max_include_depth_and_allow_includes_interaction) {
    const std::filesystem::path root = test_temp_root("app-runner-max-include-depth");
    write_file(root / "level2.pbt", "deep");
    write_file(root / "level1.pbt", "{{ include \"./level2\" }}");
    write_file(root / "main.pbt", "{{ include \"./level1\" }}");

    prebyte::test::helpers::RenderOptions depth_fail_options;
    depth_fail_options.input_path = root / "main.pbt";
    depth_fail_options.rule_args = {"max_include_depth=1"};
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(depth_fail_options), prebyte::DiagnosticError);

    prebyte::test::helpers::RenderOptions includes_disabled_options = depth_fail_options;
    includes_disabled_options.rule_args = {"allow_includes=false", "max_include_depth=1"};
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(includes_disabled_options), prebyte::DiagnosticError);
}
TEST_CASE(AppRunner_profile_override_applies_new_rules) {

    const std::filesystem::path root = test_temp_root("profile-override-new-rules");
    write_file(root / "partial.pbt", "x");
    write_file(root / "main.pbt", "{{ include \"./partial\" }}");
    write_file(root / "settings.json", std::string("{\n")
        + "  \"profiles\": {\n"
        + "    \"strict\": {\n"
        + "      \"rules\": {\n"
        + "        \"max_include_depth\": \"0\"\n"
        + "      }\n"
        + "    }\n"
        + "  }\n"
        + "}\n");

        prebyte::test::helpers::RenderOptions options;
    options.input_path = root / "main.pbt";
    options.settings_path = root / "settings.json";
    options.profile_names = {"strict"};
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(options), prebyte::DiagnosticError);

}
TEST_CASE(AppRunner_file_rule_override_applies_new_rules_to_included_file) {

    const std::filesystem::path root = test_temp_root("file-rule-new-rules");
    write_file(root / "partial.pbt", "0123456789");
    write_file(root / "main.pbt", "{{ include \"./partial\" }}");
    write_file(root / "settings.json", std::string("{\n")
        + "  \"file_rules\": {\n"
        + "    \"partial\": {\n"
        + "      \"max_output_size_bytes\": \"5\"\n"
        + "    }\n"
        + "  }\n"
        + "}\n");

        prebyte::test::helpers::RenderOptions options;
    options.input_path = root / "main.pbt";
    options.settings_path = root / "settings.json";
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(options), prebyte::DiagnosticError);

}
TEST_CASE(AppRunner_file_rule_override_applies_forbidden_env_vars_to_included_file) {

    const std::filesystem::path root = test_temp_root("file-rule-forbidden-env");
    prebyte::test::ScopedEnvironmentVariable blocked_env("PREBYTE_FILE_RULE_BLOCKED", "secret");
    write_file(root / "partial.pbt", "{{ PREBYTE_FILE_RULE_BLOCKED }}");
    write_file(root / "main.pbt", "{{ include \"./partial\" }}");
    write_file(root / "settings.json", std::string("{\n")
        + "  \"rules\": {\n"
        + "    \"allow_env\": \"true\"\n"
        + "  },\n"
        + "  \"file_rules\": {\n"
        + "    \"partial\": {\n"
        + "      \"forbidden_env_vars\": \"PREBYTE_FILE_RULE_BLOCKED\"\n"
        + "    }\n"
        + "  }\n"
        + "}\n");

        prebyte::test::helpers::RenderOptions options;
    options.input_path = root / "main.pbt";
    options.settings_path = root / "settings.json";
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(options), prebyte::DiagnosticError);

}
TEST_CASE(AppRunner_max_output_size_bytes_rule_fails_early) {

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "Hello!";
    options.rule_args = {"max_output_size_bytes=5"};
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(options), prebyte::DiagnosticError);

}
TEST_CASE(AppRunner_max_loop_iteration_rule_applies_per_nested_loop) {
    prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ fn groups() lua:block }}return { {'A', 'B'}, {'C'} }{{ endfn }}{{ for group in groups() }}{{ for item in group }}{{ item }}{{ endfor }}|{{ endfor }}";
    options.rule_args = {"max_loop_iteration=1"};
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(options), prebyte::DiagnosticError);

    options.rule_args = {"max_loop_iteration=2"};
    REQUIRE_EQ(prebyte::test::helpers::render(options), std::string("AB|C|"));
}
TEST_CASE(AppRunner_max_render_time_rule_interrupts_lua_block) {

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ lua:block }} while true do end return 'x' {{ endlua }}";
    options.rule_args = {"max_render_time_ms=0"};
    REQUIRE_THROWS_AS(prebyte::test::helpers::render(options), prebyte::DiagnosticError);

}
TEST_CASE(PrebyteEngine_render_args_are_available) {

    prebyte::Prebyte engine;
    engine.add_argument("alpha");
    engine.add_argument("beta");

    const std::string output = engine.process("{{ ARGS[1] }}");
    REQUIRE_EQ(output, std::string("beta"));

}
TEST_CASE(PrebyteEngine_process_file_renders_include_loop_template) {

    const std::filesystem::path root = test_temp_root("engine-process-file-loop-include");
    write_file(root / "partial.pbt", "<{{ loop.index }}:{{ item }}>");
    write_file(root / "main.pbt", "A{{ for item in items }}{{ include \"./partial\" }}{{ endfor }}Z");
    write_file(root / "items.yaml", "- Ada\n- Grace\n");

    prebyte::Prebyte engine;
    engine.set_variable("items", "@" + (root / "items.yaml").string());

    const std::string output = engine.process_file((root / "main.pbt").string());
    REQUIRE_EQ(output, std::string("A<1:Ada><2:Grace>Z"));

}
TEST_CASE(AppRunner_include_uses_cli_include_path_first_match) {

    const std::filesystem::path root = test_temp_root("include-cli-path");
    write_file(root / "a" / "shared" / "java.pbt", "A");
    write_file(root / "b" / "shared" / "java.pbt", "B");

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ include \"java\" }}";
    options.include_paths = {root / "a/shared", root / "b/shared"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("A"));

}
TEST_CASE(AppRunner_include_uses_directory_index_template) {

    const std::filesystem::path root = test_temp_root("include-index");
    write_file(root / "shared" / "maven" / "java" / "index.pbt", "Index OK");

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ include \"maven/java\" }}";
    options.include_paths = {root / "shared"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Index OK"));

}
TEST_CASE(AppRunner_render_top_level_pbc_file) {

    const std::filesystem::path root = test_temp_root("top-level-pbc");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::filesystem::path logical_path = root / "sample";
    write_file(source_path, "Hello {{ name }}");

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source("Hello {{ name }}", source_path, logical_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(compiled_path, serializer.serialize(program));

        prebyte::test::helpers::RenderOptions options;
    options.input_path = compiled_path;
    options.define_args = {"name=Ada"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Hello Ada"));

}
TEST_CASE(Engine_load_compiled_file_renders_top_level_pbc) {

    const std::filesystem::path root = test_temp_root("engine-top-level-pbc");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::filesystem::path logical_path = root / "sample";
    write_file(source_path, "Hello {{ name }}");

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source("Hello {{ name }}", source_path, logical_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(compiled_path, serializer.serialize(program));

    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.load_compiled_file(compiled_path);

    prebyte::RenderContext ctx;
    ctx.set("name", "Ada");

    REQUIRE_EQ(tpl.source_path(), source_path);
    REQUIRE_EQ(tpl.logical_path(), logical_path);
    REQUIRE_EQ(engine.render(tpl, ctx), std::string("Hello Ada"));

}
TEST_CASE(Engine_load_compiled_file_render_to_streams_nested_control_flow) {

    const std::filesystem::path root = test_temp_root("engine-top-level-pbc-nested-control-flow");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::filesystem::path logical_path = root / "sample";
    const std::string source =
        "{{ if members }}{{ for member in members }}{{ include \"./partial\" }};{{ endfor }}{{ elseif archived }}archived{{ else }}empty{{ endif }}";
    write_file(root / "partial.pbt", "{{ if member.admin }}*{{ else }}-{{ endif }}{{ member.name }}");
    write_file(source_path, source);

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source(source, source_path, logical_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(compiled_path, serializer.serialize(program));

    prebyte::Engine engine;
    const prebyte::CompiledTemplate tpl = engine.load_compiled_file(compiled_path);

    prebyte::RenderContext ctx;
    ctx.set("archived", "false");
    prebyte::Data::Array members;
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Ada")}, {"admin", prebyte::Data(true)}}));
    members.push_back(prebyte::Data(prebyte::Data::Map{{"name", prebyte::Data("Grace")}, {"admin", prebyte::Data(false)}}));
    ctx.set("members", prebyte::Value::list(std::move(members)));

    std::string output;
    std::size_t chunk_count = 0;
    engine.render_to(tpl, [&](std::string_view chunk) {
        ++chunk_count;
        output.append(chunk.data(), chunk.size());
    }, ctx);

    REQUIRE_EQ(output, std::string("*Ada;-Grace;"));
    REQUIRE(chunk_count > 1);

}
TEST_CASE(AppRunner_render_top_level_pbc_file_with_named_structured_imports) {

    const std::filesystem::path root = test_temp_root("top-level-pbc-structured-imports");
    const std::filesystem::path source_path = root / "sample.pbt";
    const std::filesystem::path compiled_path = root / "sample.pbc";
    const std::filesystem::path logical_path = root / "sample";
    const std::filesystem::path user_path = root / "user.json";
    const std::filesystem::path items_path = root / "items.yaml";
    write_file(source_path, "{{ user.name }} {{ items[1] }}");
    write_file(user_path, "{\"name\":\"Ada\"}");
    write_file(items_path, "- Ada\n- Grace\n");

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source("{{ user.name }} {{ items[1] }}", source_path, logical_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(compiled_path, serializer.serialize(program));

        prebyte::test::helpers::RenderOptions options;
    options.input_path = compiled_path;
    options.define_args = {"user=@" + user_path.string(), "items=@" + items_path.string()};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Ada Grace"));

}
TEST_CASE(AppRunner_render_source_file_from_adjacent_pbc_without_reading_source) {

    const std::filesystem::path root = test_temp_root("top-level-adjacent-pbc");
    const std::filesystem::path source_path = root / "sample.txt";
    write_file(source_path, "Hello {{ name }}");

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source("Hello {{ name }}", source_path, source_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(serializer.compiled_path_for_source(source_path), serializer.serialize(program));

    std::error_code permissions_error;
    std::filesystem::permissions(source_path,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::group_read
                                     | std::filesystem::perms::others_read,
                                 std::filesystem::perm_options::remove,
                                 permissions_error);
    REQUIRE(!permissions_error);

        prebyte::test::helpers::RenderOptions options;
    options.input_path = source_path;
    options.define_args = {"name=Ada"};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Hello Ada"));

}
TEST_CASE(AppRunner_render_source_file_from_adjacent_pbc_with_named_structured_imports) {

    const std::filesystem::path root = test_temp_root("adjacent-pbc-structured-imports");
    const std::filesystem::path source_path = root / "sample.txt";
    const std::filesystem::path user_path = root / "user.json";
    const std::filesystem::path config_path = root / "config.toml";
    write_file(source_path, "{{ user.name }} {{ config.server.host }}");
    write_file(user_path, "{\"name\":\"Ada\"}");
    write_file(config_path, "[server]\nhost=\"localhost\"\n");

    prebyte::CompiledTemplateCompiler compiler;
    prebyte::EffectiveSettings settings;
    const prebyte::CompiledProgram program = compiler.compile_source("{{ user.name }} {{ config.server.host }}", source_path, source_path, settings);
    prebyte::CompiledTemplateSerializer serializer;
    write_file(serializer.compiled_path_for_source(source_path), serializer.serialize(program));

    std::error_code permissions_error;
    std::filesystem::permissions(source_path,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::group_read
                                     | std::filesystem::perms::others_read,
                                 std::filesystem::perm_options::remove,
                                 permissions_error);
    REQUIRE(!permissions_error);

        prebyte::test::helpers::RenderOptions options;
    options.input_path = source_path;
    options.define_args = {"user=@" + user_path.string(), "config=@" + config_path.string()};
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Ada localhost"));

}
TEST_CASE(AppRunner_reuse_in_memory_compiled_template_when_pbc_write_fails) {

    const std::filesystem::path root = test_temp_root("in-memory-compiled-cache");
    const std::filesystem::path source_path = root / "sample.txt";
    write_file(source_path, "Hello {{ name }}");

    std::error_code permissions_error;
    std::filesystem::permissions(root,
                                 std::filesystem::perms::owner_write | std::filesystem::perms::group_write
                                     | std::filesystem::perms::others_write,
                                 std::filesystem::perm_options::remove,
                                 permissions_error);
    REQUIRE(!permissions_error);

        prebyte::test::helpers::RenderOptions options;
    options.input_path = source_path;
    options.define_args = {"name=Ada"};
    REQUIRE_EQ(prebyte::test::helpers::render(options), std::string("Hello Ada"));

    std::filesystem::permissions(source_path,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::group_read
                                     | std::filesystem::perms::others_read,
                                 std::filesystem::perm_options::remove,
                                 permissions_error);
    REQUIRE(!permissions_error);

    std::filesystem::permissions(source_path,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_write
                                     | std::filesystem::perms::group_read | std::filesystem::perms::group_write
                                     | std::filesystem::perms::others_read | std::filesystem::perms::others_write,
                                 std::filesystem::perm_options::add,
                                 permissions_error);
    REQUIRE(!permissions_error);
    std::filesystem::permissions(root,
                                 std::filesystem::perms::owner_write | std::filesystem::perms::group_write
                                     | std::filesystem::perms::others_write,
                                 std::filesystem::perm_options::add,
                                 permissions_error);
    REQUIRE(!permissions_error);

}
TEST_CASE(AppRunner_include_uses_default_shared_root) {

    const std::filesystem::path root = test_temp_root("default-shared-root");
    const std::filesystem::path shared = default_shared_root_for_test(root) / "java" / "index.pbt";
    write_file(shared, "Shared OK");
    prebyte::test::ScopedEnvironmentVariable shared_root_env(shared_root_environment_name(), shared_root_environment_value(root).string());

        prebyte::test::helpers::RenderOptions options;
    options.inline_input = "{{ include \"java\" }}";
    const std::string output = prebyte::test::helpers::render(options);
    REQUIRE_EQ(output, std::string("Shared OK"));

}
TEST_CASE(AppRunner_render_functions_and_builtins) {
    prebyte::test::helpers::RenderOptions options;
    options.inline_input =
        "{{ fn greet(name) }}Hello {{ name }} from {{ __OS__ }}{{ endfn }}"
        "{{ greet(\"Ada\") }}"
        "|{{ fn users() lua:block }}return { { name = 'Grace' }, { name = 'Linus' } }{{ endfn }}"
        "{{ for user in users() }}{{ user.name }};{{ endfor }}"
        "|{{ __UUID__ == __UUID__ }}|{{ __RANDOM__ == __RANDOM__ }}";
    const std::string output = prebyte::test::helpers::render(options);

    REQUIRE(output.find("Hello Ada from ") == 0);
    REQUIRE(output.find("|Grace;Linus;|true|true") != std::string::npos);
}
