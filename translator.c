#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *from;
    const char *to;
} Replacement;

static const Replacement kReplacements[] = {
    {"CHROMOSOME_CODING", "section .data"},
    {"CHROMOSOME_EXPRESSION", "section .text"},
    {"PHOSPHORYLATE", "mov"},
    {"POLYMERIZE", "add"},
    {"HYDROLYZE", "sub"},
    {"ANNEAL", "cmp"},
    {"TRANSLOCATE", "jmp"},
    {"BIND_MATCH", "je"},
    {"EXOCYTOSE", "syscall"},
    {"NEGATE", "neg"},
    {"KINASE_A", "rax"},
    {"RECEPTOR_D", "rdi"},
    {"RECEPTOR_S", "rsi"},
    {"RECEPTOR_X", "rdx"},
};

static char *read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t read_count = fread(buffer, 1, (size_t)size, file);
    fclose(file);

    if (read_count != (size_t)size) {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    if (out_size) {
        *out_size = (size_t)size;
    }
    return buffer;
}

static int write_file(const char *path, const char *data, size_t size) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        return -1;
    }

    size_t written = fwrite(data, 1, size, file);
    fclose(file);
    return written == size ? 0 : -1;
}

static bool is_token_boundary(char c) {
    return c == '\0' || isspace((unsigned char)c) || c == ',' || c == ';' || c == '(' || c == ')' || c == '{' || c == '}' || c == '+' || c == '-' || c == '*' || c == '/' || c == '=' || c == '<' || c == '>' || c == '!';
}

static int ensure_capacity(char **buffer, size_t *capacity, size_t required) {
    if (*capacity >= required) {
        return 0;
    }

    size_t new_capacity = *capacity == 0 ? required : *capacity;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    char *new_buffer = realloc(*buffer, new_capacity);
    if (!new_buffer) {
        return -1;
    }
    *buffer = new_buffer;
    *capacity = new_capacity;
    return 0;
}

static int translate_text(const char *source, char **out_text, size_t *out_size) {
    size_t source_len = strlen(source);
    size_t capacity = source_len + 1;
    char *result = malloc(capacity);
    if (!result) {
        return -1;
    }

    bool in_string = false;
    char quote_char = '\0';
    size_t ri = 0;

    for (size_t si = 0; si < source_len; ++si) {
        char ch = source[si];

        if (in_string) {
            result[ri++] = ch;
            if (ch == quote_char) {
                in_string = false;
                quote_char = '\0';
            }
            continue;
        }

        if (ch == '"' || ch == '\'') {
            in_string = true;
            quote_char = ch;
            result[ri++] = ch;
            continue;
        }

        bool replaced = false;
        for (size_t ri_map = 0; ri_map < sizeof(kReplacements) / sizeof(kReplacements[0]); ++ri_map) {
            const char *from = kReplacements[ri_map].from;
            size_t from_len = strlen(from);

            if (si + from_len <= source_len && strncmp(source + si, from, from_len) == 0) {
                char prev = si == 0 ? '\0' : source[si - 1];
                char next = source[si + from_len];
                if (is_token_boundary(prev) && is_token_boundary(next)) {
                    const char *to = kReplacements[ri_map].to;
                    size_t to_len = strlen(to);
                    if (ensure_capacity(&result, &capacity, ri + to_len + 1) != 0) {
                        free(result);
                        return -1;
                    }
                    memcpy(result + ri, to, to_len);
                    ri += to_len;
                    si += from_len - 1;
                    replaced = true;
                    break;
                }
            }
        }

        if (!replaced) {
            if (ensure_capacity(&result, &capacity, ri + 2) != 0) {
                free(result);
                return -1;
            }
            result[ri++] = ch;
        }
    }

    result[ri] = '\0';
    *out_text = result;
    if (out_size) {
        *out_size = ri;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const char *input_path = "output.rna";
    const char *output_path = "output.asm";

    if (argc == 3) {
        input_path = argv[1];
        output_path = argv[2];
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [input.rna output.asm]\n", argv[0]);
        return EXIT_FAILURE;
    }

    size_t source_size = 0;
    char *source = read_file(input_path, &source_size);
    if (!source) {
        fprintf(stderr, "Failed to read '%s'\n", input_path);
        return EXIT_FAILURE;
    }

    char *translated = NULL;
    size_t translated_size = 0;
    if (translate_text(source, &translated, &translated_size) != 0) {
        free(source);
        fprintf(stderr, "Translation failed due to memory error\n");
        return EXIT_FAILURE;
    }

    if (write_file(output_path, translated, translated_size) != 0) {
        free(source);
        free(translated);
        fprintf(stderr, "Failed to write '%s'\n", output_path);
        return EXIT_FAILURE;
    }

    free(source);
    free(translated);
    return EXIT_SUCCESS;
}
