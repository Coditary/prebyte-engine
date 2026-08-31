#include "prebyte_engine.h"

#include "Engine.h"
#include "PrebyteEngine.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

thread_local std::string g_last_error;
std::mutex g_error_mutex;

void set_last_error(std::string message) {
    std::lock_guard lock(g_error_mutex);
    g_last_error = std::move(message);
}

void set_last_error_from_current_exception() {
    try {
        throw;
    } catch (const std::exception& error) {
        set_last_error(error.what());
    } catch (...) {
        set_last_error("unknown error");
    }
}

char* copy_string(std::string value) {
    char* output = static_cast<char*>(std::malloc(value.size() + 1));
    if (output == nullptr) {
        set_last_error("out of memory");
        return nullptr;
    }
    std::memcpy(output, value.data(), value.size());
    output[value.size()] = '\0';
    return output;
}

int ok_or_error(auto&& action) {
    try {
        action();
        return 0;
    } catch (...) {
        set_last_error_from_current_exception();
        return -1;
    }
}

template <typename T>
T* ok_or_null(auto&& action) {
    try {
        return action();
    } catch (...) {
        set_last_error_from_current_exception();
        return nullptr;
    }
}

}

extern "C" {

const char* prebyte_version(void) {
    return "1.1.1";
}

const char* prebyte_get_last_error(void) {
    return g_last_error.c_str();
}

void prebyte_clear_last_error(void) {
    g_last_error.clear();
}

void prebyte_string_free(char* value) {
    std::free(value);
}

struct prebyte_session {
    std::unique_ptr<prebyte::Prebyte> impl;
};

prebyte_session_t* prebyte_session_create(void) {
    return ok_or_null<prebyte_session_t>([] {
        auto session = new prebyte_session();
        session->impl = std::make_unique<prebyte::Prebyte>();
        return static_cast<prebyte_session_t*>(session);
    });
}

prebyte_session_t* prebyte_session_create_with_settings(const char* settings_path) {
    return ok_or_null<prebyte_session_t>([settings_path] {
        auto session = new prebyte_session();
        session->impl = std::make_unique<prebyte::Prebyte>(settings_path != nullptr ? settings_path : "");
        return static_cast<prebyte_session_t*>(session);
    });
}

void prebyte_session_destroy(prebyte_session_t* session) {
    delete session;
}

int prebyte_session_set_variable(prebyte_session_t* session, const char* name, const char* value) {
    if (session == nullptr || name == nullptr || value == nullptr) {
        set_last_error("invalid argument");
        return -1;
    }
    return ok_or_error([&] { session->impl->set_variable(name, value); });
}

int prebyte_session_set_variable_flag(prebyte_session_t* session, const char* name) {
    if (session == nullptr || name == nullptr) {
        set_last_error("invalid argument");
        return -1;
    }
    return ok_or_error([&] { session->impl->set_variable(name); });
}

int prebyte_session_add_argument(prebyte_session_t* session, const char* value) {
    if (session == nullptr || value == nullptr) {
        set_last_error("invalid argument");
        return -1;
    }
    return ok_or_error([&] { session->impl->add_argument(value); });
}

int prebyte_session_add_include_path(prebyte_session_t* session, const char* path) {
    if (session == nullptr || path == nullptr) {
        set_last_error("invalid argument");
        return -1;
    }
    return ok_or_error([&] { session->impl->add_include_path(path); });
}

int prebyte_session_set_profile(prebyte_session_t* session, const char* profile_name) {
    if (session == nullptr || profile_name == nullptr) {
        set_last_error("invalid argument");
        return -1;
    }
    return ok_or_error([&] { session->impl->set_profile(profile_name); });
}

int prebyte_session_set_ignore(prebyte_session_t* session, const char* ignore_item) {
    if (session == nullptr || ignore_item == nullptr) {
        set_last_error("invalid argument");
        return -1;
    }
    return ok_or_error([&] { session->impl->set_ignore(ignore_item); });
}

int prebyte_session_set_rule(prebyte_session_t* session, const char* rule_name, const char* rule_value) {
    if (session == nullptr || rule_name == nullptr || rule_value == nullptr) {
        set_last_error("invalid argument");
        return -1;
    }
    return ok_or_error([&] { session->impl->set_rule(rule_name, rule_value); });
}

char* prebyte_session_render(prebyte_session_t* session, const char* input) {
    if (session == nullptr || input == nullptr) {
        set_last_error("invalid argument");
        return nullptr;
    }
    return ok_or_null<char>([&] { return copy_string(session->impl->process(input)); });
}

char* prebyte_session_render_file(prebyte_session_t* session, const char* file_path) {
    if (session == nullptr || file_path == nullptr) {
        set_last_error("invalid argument");
        return nullptr;
    }
    return ok_or_null<char>([&] { return copy_string(session->impl->process_file(file_path)); });
}

int prebyte_session_render_to_file(prebyte_session_t* session, const char* input, const char* output_path) {
    if (session == nullptr || input == nullptr || output_path == nullptr) {
        set_last_error("invalid argument");
        return -1;
    }
    return ok_or_error([&] { session->impl->process(input, output_path); });
}

int prebyte_session_render_file_to_file(prebyte_session_t* session, const char* input_path, const char* output_path) {
    if (session == nullptr || input_path == nullptr || output_path == nullptr) {
        set_last_error("invalid argument");
        return -1;
    }
    return ok_or_error([&] { session->impl->process_file(input_path, output_path); });
}

struct prebyte_engine {
    std::unique_ptr<prebyte::Engine> impl;
};

prebyte_engine_t* prebyte_engine_create(void) {
    return ok_or_null<prebyte_engine_t>([] {
        auto engine = new prebyte_engine();
        engine->impl = std::make_unique<prebyte::Engine>();
        return static_cast<prebyte_engine_t*>(engine);
    });
}

void prebyte_engine_destroy(prebyte_engine_t* engine) {
    delete engine;
}

struct prebyte_template {
    prebyte::CompiledTemplate compiled;
};

prebyte_template_t* prebyte_engine_compile(prebyte_engine_t* engine, const char* source, const char* source_path) {
    if (engine == nullptr || source == nullptr) {
        set_last_error("invalid argument");
        return nullptr;
    }
    return ok_or_null<prebyte_template_t>([&] {
        auto compiled = new prebyte_template();
        compiled->compiled = engine->impl->compile(source, source_path != nullptr ? source_path : "");
        return static_cast<prebyte_template_t*>(compiled);
    });
}

prebyte_template_t* prebyte_engine_compile_file(prebyte_engine_t* engine, const char* path) {
    if (engine == nullptr || path == nullptr) {
        set_last_error("invalid argument");
        return nullptr;
    }
    return ok_or_null<prebyte_template_t>([&] {
        auto compiled = new prebyte_template();
        compiled->compiled = engine->impl->compile_file(path);
        return static_cast<prebyte_template_t*>(compiled);
    });
}

void prebyte_template_destroy(prebyte_template_t* compiled) {
    delete compiled;
}

struct prebyte_render_context {
    prebyte::RenderContext impl;
};

prebyte_render_context_t* prebyte_render_context_create(void) {
    return ok_or_null<prebyte_render_context_t>([] {
        return static_cast<prebyte_render_context_t*>(new prebyte_render_context());
    });
}

void prebyte_render_context_destroy(prebyte_render_context_t* context) {
    delete context;
}

int prebyte_render_context_set(prebyte_render_context_t* context, const char* name, const char* value) {
    if (context == nullptr || name == nullptr || value == nullptr) {
        set_last_error("invalid argument");
        return -1;
    }
    return ok_or_error([&] { context->impl.set(name, value); });
}

int prebyte_render_context_set_args(prebyte_render_context_t* context, const char* const* args, size_t count) {
    if (context == nullptr || (count > 0 && args == nullptr)) {
        set_last_error("invalid argument");
        return -1;
    }
    return ok_or_error([&] {
        std::vector<std::string> values;
        values.reserve(count);
        for (size_t index = 0; index < count; ++index) {
            values.emplace_back(args[index] != nullptr ? args[index] : "");
        }
        context->impl.set_args(std::move(values));
    });
}

char* prebyte_engine_render(prebyte_engine_t* engine,
                            prebyte_template_t* compiled,
                            prebyte_render_context_t* context) {
    if (engine == nullptr || compiled == nullptr) {
        set_last_error("invalid argument");
        return nullptr;
    }
    return ok_or_null<char>([&] {
        if (context == nullptr) {
            return copy_string(engine->impl->render(compiled->compiled));
        }
        return copy_string(engine->impl->render(compiled->compiled, context->impl));
    });
}

}
