#include "EngineTestHelpers.h"

#include "config/VariableDefinitionParser.h"

namespace prebyte::test::helpers {

void apply_render_options(Prebyte& engine, const RenderOptions& options) {
    for (const auto& path : options.include_paths) {
        engine.add_include_path(path.string());
    }
    for (const auto& profile : options.profile_names) {
        engine.set_profile(profile);
    }
    for (const auto& ignore : options.ignore_names) {
        engine.set_ignore(ignore);
    }
    for (const auto& rule : options.rule_args) {
        const auto separator = rule.find('=');
        if (separator == std::string::npos) {
            engine.set_rule(rule, "");
        } else {
            engine.set_rule(rule.substr(0, separator), rule.substr(separator + 1));
        }
    }
    for (const auto& arg : options.render_args) {
        engine.add_argument(arg);
    }
    for (const auto& define : options.define_args) {
        const auto separator = define.find('=');
        if (separator == std::string::npos) {
            engine.set_variable(define);
        } else if (separator + 1 == define.size()) {
            engine.set_variable(define.substr(0, separator));
        } else {
            engine.set_variable(define.substr(0, separator), define.substr(separator + 1));
        }
    }
    static_cast<void>(options.debug);
}

std::string render(const RenderOptions& options) {
    Prebyte engine(options.settings_path.has_value() ? options.settings_path->string() : std::string{});
    apply_render_options(engine, options);

    if (options.inline_input.has_value()) {
        if (options.output_path.has_value()) {
            engine.process(*options.inline_input, options.output_path->string());
            return {};
        }
        return engine.process(*options.inline_input);
    }

    if (!options.input_path.has_value()) {
        return {};
    }

    if (options.output_path.has_value()) {
        engine.process_file(options.input_path->string(), options.output_path->string());
        return {};
    }
    return engine.process_file(options.input_path->string());
}

void render_to_file(const RenderOptions& options) {
    RenderOptions copy = options;
    if (!copy.output_path.has_value()) {
        return;
    }
    static_cast<void>(render(copy));
}

std::string render_inline(const std::string& source,
                          const std::vector<std::string>& define_args,
                          const std::vector<std::string>& rule_args) {
    RenderOptions options;
    options.inline_input = source;
    options.define_args = define_args;
    options.rule_args = rule_args;
    return render(options);
}

std::string render_file(const std::filesystem::path& path,
                        const std::vector<std::string>& define_args,
                        const std::vector<std::string>& rule_args,
                        const std::optional<std::filesystem::path>& settings_path,
                        const std::vector<std::string>& profile_names) {
    RenderOptions options;
    options.input_path = path;
    options.define_args = define_args;
    options.rule_args = rule_args;
    options.settings_path = settings_path;
    options.profile_names = profile_names;
    return render(options);
}

}
