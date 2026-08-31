#include "prebyte_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    prebyte_session_t* session = prebyte_session_create();
    if (session == NULL) {
        fprintf(stderr, "session create failed: %s\n", prebyte_get_last_error());
        return EXIT_FAILURE;
    }

    if (prebyte_session_set_variable(session, "name", "Ada") != 0) {
        fprintf(stderr, "set variable failed: %s\n", prebyte_get_last_error());
        prebyte_session_destroy(session);
        return EXIT_FAILURE;
    }

    char* output = prebyte_session_render(session, "Hello {{ name }}");
    if (output == NULL) {
        fprintf(stderr, "render failed: %s\n", prebyte_get_last_error());
        prebyte_session_destroy(session);
        return EXIT_FAILURE;
    }

    int status = strcmp(output, "Hello Ada") == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    if (status != EXIT_SUCCESS) {
        fprintf(stderr, "unexpected output: %s\n", output);
    }

    prebyte_string_free(output);
    prebyte_session_destroy(session);
    return status;
}
