#pragma once

#include "PrebyteEngine.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace prebyte::test::helpers {

struct RenderOptions {
    std::optional<std::string> inline_input;
    std::optional<std::filesystem::path> input_path;
    std::optional<std::filesystem::path> output_path;
    std::optional<std::filesystem::path> settings_path;
    std::vector<std::string> render_args;
    std::vector<std::filesystem::path> include_paths;
    std::vector<std::string> define_args;
    std::vector<std::string> rule_args;
    std::vector<std::string> ignore_names;
    std::vector<std::string> profile_names;
    bool debug = false;
};

void apply_render_options(Prebyte& engine, const RenderOptions& options);

std::string render(const RenderOptions& options);
void render_to_file(const RenderOptions& options);

std::string render_inline(const std::string& source,
                          const std::vector<std::string>& define_args = {},
                          const std::vector<std::string>& rule_args = {});

std::string render_file(const std::filesystem::path& path,
                        const std::vector<std::string>& define_args = {},
                        const std::vector<std::string>& rule_args = {},
                        const std::optional<std::filesystem::path>& settings_path = std::nullopt,
                        const std::vector<std::string>& profile_names = {});

}