#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(PREBYTE_ENGINE_C_EXPORTS)
        #define PREBYTE_API __declspec(dllexport)
    #else
        #define PREBYTE_API __declspec(dllimport)
    #endif
#else
    #if defined(PREBYTE_ENGINE_C_EXPORTS)
        #define PREBYTE_API __attribute__((visibility("default")))
    #else
        #define PREBYTE_API
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct prebyte_session prebyte_session_t;
typedef struct prebyte_engine prebyte_engine_t;
typedef struct prebyte_template prebyte_template_t;
typedef struct prebyte_render_context prebyte_render_context_t;

PREBYTE_API const char* prebyte_version(void);

PREBYTE_API const char* prebyte_get_last_error(void);
PREBYTE_API void prebyte_clear_last_error(void);
PREBYTE_API void prebyte_string_free(char* value);

PREBYTE_API prebyte_session_t* prebyte_session_create(void);
PREBYTE_API prebyte_session_t* prebyte_session_create_with_settings(const char* settings_path);
PREBYTE_API void prebyte_session_destroy(prebyte_session_t* session);

PREBYTE_API int prebyte_session_set_variable(prebyte_session_t* session, const char* name, const char* value);
PREBYTE_API int prebyte_session_set_variable_flag(prebyte_session_t* session, const char* name);
PREBYTE_API int prebyte_session_add_argument(prebyte_session_t* session, const char* value);
PREBYTE_API int prebyte_session_add_include_path(prebyte_session_t* session, const char* path);
PREBYTE_API int prebyte_session_set_profile(prebyte_session_t* session, const char* profile_name);
PREBYTE_API int prebyte_session_set_ignore(prebyte_session_t* session, const char* ignore_item);
PREBYTE_API int prebyte_session_set_rule(prebyte_session_t* session, const char* rule_name, const char* rule_value);

PREBYTE_API char* prebyte_session_render(prebyte_session_t* session, const char* input);
PREBYTE_API char* prebyte_session_render_file(prebyte_session_t* session, const char* file_path);
PREBYTE_API int prebyte_session_render_to_file(prebyte_session_t* session, const char* input, const char* output_path);
PREBYTE_API int prebyte_session_render_file_to_file(prebyte_session_t* session, const char* input_path, const char* output_path);

PREBYTE_API prebyte_engine_t* prebyte_engine_create(void);
PREBYTE_API void prebyte_engine_destroy(prebyte_engine_t* engine);

PREBYTE_API prebyte_template_t* prebyte_engine_compile(prebyte_engine_t* engine,
                                                       const char* source,
                                                       const char* source_path);
PREBYTE_API prebyte_template_t* prebyte_engine_compile_file(prebyte_engine_t* engine, const char* path);
PREBYTE_API void prebyte_template_destroy(prebyte_template_t* compiled);

PREBYTE_API prebyte_render_context_t* prebyte_render_context_create(void);
PREBYTE_API void prebyte_render_context_destroy(prebyte_render_context_t* context);
PREBYTE_API int prebyte_render_context_set(prebyte_render_context_t* context, const char* name, const char* value);
PREBYTE_API int prebyte_render_context_set_args(prebyte_render_context_t* context, const char* const* args, size_t count);

PREBYTE_API char* prebyte_engine_render(prebyte_engine_t* engine,
                                        prebyte_template_t* compiled,
                                        prebyte_render_context_t* context);

#ifdef __cplusplus
}
#endif
