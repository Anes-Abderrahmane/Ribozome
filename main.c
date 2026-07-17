#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "codegen.h"

static char *read_file(const char *path) {
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
    return buffer;
}

static void print_tokens(const Token *tokens, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        printf("%zu:%zu %-16s '%s'\n",
               tokens[i].line,
               tokens[i].column,
               token_type_name(tokens[i].type),
               tokens[i].lexeme);
    }
}

int main(int argc, char *argv[]) {
    int verbose = 0;
    const char *source_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options] <source.dna>\n", argv[0]);
            printf("Options:\n");
            printf("  -v, --verbose  Print tokens and AST details\n");
            printf("  -h, --help     Show this help message\n");
            return EXIT_SUCCESS;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            fprintf(stderr, "Unknown option '%s'\n", argv[i]);
            return EXIT_FAILURE;
        } else if (!source_path) {
            source_path = argv[i];
        } else {
            fprintf(stderr, "Unexpected argument '%s'\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    if (!source_path) {
        fprintf(stderr, "Usage: %s [options] <source.dna>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *source = read_file(source_path);
    if (!source) {
        fprintf(stderr, "Unable to read source file '%s'\n", source_path);
        return EXIT_FAILURE;
    }

    size_t token_count = 0;
    Token *tokens = tokenize(source, &token_count);
    if (!tokens) {
        free(source);
        fprintf(stderr, "Failed to tokenize source.\n");
        return EXIT_FAILURE;
    }

    if (verbose) {
        print_tokens(tokens, token_count);
    }

    ASTNode *ast = parse_tokens(tokens, token_count);
    if (!ast) {
        free_tokens(tokens, token_count);
        free(source);
        return EXIT_FAILURE;
    }

    if (verbose) {
        printf("AST root: %s\n", ast_node_type_name(ast->type));
    }

    if (generate_rna(ast) != 0) {
        fprintf(stderr, "Failed to generate output.rna\n");
        free_ast(ast);
        free_tokens(tokens, token_count);
        free(source);
        return EXIT_FAILURE;
    }

    printf("Generated output.rna\n");

    if (system("./translator output.rna output.asm") != 0) {
        fprintf(stderr, "Failed to translate output.rna to output.asm\n");
        free_ast(ast);
        free_tokens(tokens, token_count);
        free(source);
        return EXIT_FAILURE;
    }

    if (system("nasm -f elf64 output.asm -o output.o") != 0) {
        fprintf(stderr, "Failed to assemble output.asm\n");
        free_ast(ast);
        free_tokens(tokens, token_count);
        free(source);
        return EXIT_FAILURE;
    }

    if (system("ld output.o -o output.protein -e NUCLEUS") != 0) {
        fprintf(stderr, "Failed to link output.o into output.protein\n");
        free_ast(ast);
        free_tokens(tokens, token_count);
        free(source);
        return EXIT_FAILURE;
    }

    printf("Built final executable: output.protein\n");
    free_ast(ast);
    free_tokens(tokens, token_count);
    free(source);
    return EXIT_SUCCESS;
}
