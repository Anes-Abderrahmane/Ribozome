#ifndef RIBOZOME_LEXER_H
#define RIBOZOME_LEXER_H

#include <stddef.h>

/* Token types produced by the Ribozome lexer. */
typedef enum {
    TOKEN_UNKNOWN,
    TOKEN_EOF,

    /* High-level language keywords. */
    TOKEN_NUCLEOTIDE,
    TOKEN_SEQUENCE,
    TOKEN_GENE,
    TOKEN_NUCLEUS,
    TOKEN_IF_HOMOLOGOUS,
    TOKEN_ELSE_MUTATE,
    TOKEN_REPLICATE_WHILE,
    TOKEN_EXPRESS,
    TOKEN_SECRETE,

    /* Aliases and convenience keywords. */
    TOKEN_RELEASE,
    TOKEN_RIBOSOME,
    TOKEN_IF_BIND,
    TOKEN_REPLICATE,
    TOKEN_TRANSCRIPT,
    TOKEN_BASES,
    TOKEN_EMPTY,

    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,

    /* Simple punctuation and operators. */
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_ASSIGN,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_ASTERISK,
    TOKEN_SLASH,
    TOKEN_LT,
    TOKEN_GT,
    TOKEN_EQUAL,
    TOKEN_NEQ,
    TOKEN_LEQ,
    TOKEN_GEQ,
} TokenType;

/* A single token returned by the lexer. */
typedef struct {
    TokenType type;
    char *lexeme;
    size_t line;
    size_t column;
} Token;

/*
 * Tokenize the supplied source text.
 *
 * Returns a newly allocated array of Token objects. The caller owns the array
 * and must call free_tokens() when finished. On failure, returns NULL and
 * sets *out_count to zero if out_count is not NULL.
 */
Token *tokenize(const char *source, size_t *out_count);

/* Free the token array returned by tokenize(). */
void free_tokens(Token *tokens, size_t count);

/* Return a textual name for a token type. */
const char *token_type_name(TokenType type);

#endif /* RIBOZOME_LEXER_H */
