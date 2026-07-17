#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keyword mapping for tokens recognized by the lexer. */
typedef struct {
    const char *text;
    TokenType type;
} Keyword;

static const Keyword kKeywords[] = {
    {"NUCLEOTIDE", TOKEN_NUCLEOTIDE},
    {"SEQUENCE", TOKEN_SEQUENCE},
    {"GENE", TOKEN_GENE},
    {"NUCLEUS", TOKEN_NUCLEUS},
    {"IF_HOMOLOGOUS", TOKEN_IF_HOMOLOGOUS},
    {"ELSE_MUTATE", TOKEN_ELSE_MUTATE},
    {"REPLICATE_WHILE", TOKEN_REPLICATE_WHILE},
    {"EXPRESS", TOKEN_EXPRESS},
    {"SECRETE", TOKEN_SECRETE},

    /* Alias keywords from the broader Ribozome language. */
    {"RELEASE", TOKEN_RELEASE},
    {"RIBOSOME", TOKEN_RIBOSOME},
    {"IF_BIND", TOKEN_IF_BIND},
    {"REPLICATE", TOKEN_REPLICATE},
    {"TRANSCRIPT", TOKEN_TRANSCRIPT},
    {"BASES", TOKEN_BASES},
    {"EMPTY", TOKEN_EMPTY},
};

static void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Ribozome lexer: allocation failed\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static char *copy_lexeme(const char *start, size_t length) {
    char *copy = xmalloc(length + 1);
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static TokenType keyword_lookup(const char *text, size_t length) {
    char buffer[128];
    if (length >= sizeof(buffer)) {
        return TOKEN_IDENTIFIER;
    }

    for (size_t i = 0; i < length; ++i) {
        buffer[i] = (char)toupper((unsigned char)text[i]);
    }
    buffer[length] = '\0';

    for (size_t i = 0; i < sizeof(kKeywords) / sizeof(kKeywords[0]); ++i) {
        if (strcmp(buffer, kKeywords[i].text) == 0) {
            return kKeywords[i].type;
        }
    }

    return TOKEN_IDENTIFIER;
}

static void append_token(Token **tokens, size_t *count, size_t *capacity, Token token) {
    if (*count >= *capacity) {
        size_t new_capacity = *capacity == 0 ? 16 : *capacity * 2;
        Token *new_tokens = xmalloc(new_capacity * sizeof(Token));
        if (*tokens) {
            memcpy(new_tokens, *tokens, (*count) * sizeof(Token));
            free(*tokens);
        }
        *tokens = new_tokens;
        *capacity = new_capacity;
    }
    (*tokens)[*count] = token;
    *count += 1;
}

static Token create_token(TokenType type, const char *lexeme, size_t line, size_t column) {
    Token token;
    token.type = type;
    token.lexeme = copy_lexeme(lexeme, strlen(lexeme));
    token.line = line;
    token.column = column;
    return token;
}

static Token create_token_from_range(TokenType type, const char *source, size_t start, size_t end, size_t line, size_t column) {
    Token token;
    token.type = type;
    token.lexeme = copy_lexeme(source + start, end - start);
    token.line = line;
    token.column = column;
    return token;
}

static void skip_whitespace(const char *source, size_t *index, size_t *line, size_t *column) {
    while (source[*index] != '\0') {
        char ch = source[*index];
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            (*index)++;
            (*column)++;
            continue;
        }
        if (ch == '\n') {
            (*index)++;
            (*line)++;
            *column = 1;
            continue;
        }
        break;
    }
}

static void skip_comment(const char *source, size_t *index, size_t *column) {
    while (source[*index] != '\0' && source[*index] != '\n') {
        (*index)++;
        (*column)++;
    }
}

Token *tokenize(const char *source, size_t *out_count) {
    if (!source) {
        if (out_count) {
            *out_count = 0;
        }
        return NULL;
    }

    size_t index = 0;
    size_t line = 1;
    size_t column = 1;
    size_t count = 0;
    size_t capacity = 0;
    Token *tokens = NULL;

    while (source[index] != '\0') {
        skip_whitespace(source, &index, &line, &column);
        if (source[index] == '\0') {
            break;
        }

        size_t token_line = line;
        size_t token_column = column;
        char ch = source[index];

        if (ch == '/' && source[index + 1] == '/') {
            skip_comment(source, &index, &column);
            continue;
        }

        if (isalpha((unsigned char)ch) || ch == '_') {
            size_t start = index;
            while (isalnum((unsigned char)source[index]) || source[index] == '_') {
                index++;
                column++;
            }
            TokenType type = keyword_lookup(source + start, index - start);
            Token token = create_token_from_range(type, source, start, index, token_line, token_column);
            append_token(&tokens, &count, &capacity, token);
            continue;
        }

        if (isdigit((unsigned char)ch)) {
            size_t start = index;
            while (isdigit((unsigned char)source[index])) {
                index++;
                column++;
            }
            Token token = create_token_from_range(TOKEN_NUMBER, source, start, index, token_line, token_column);
            append_token(&tokens, &count, &capacity, token);
            continue;
        }

        if (ch == '"' || ch == '\'') {
            char quote = ch;
            size_t start = index + 1;
            index++;
            column++;
            while (source[index] != '\0' && source[index] != quote) {
                if (source[index] == '\\' && source[index + 1] != '\0') {
                    index += 2;
                    column += 2;
                    continue;
                }
                if (source[index] == '\n') {
                    line++;
                    column = 1;
                } else {
                    column++;
                }
                index++;
            }
            size_t end = index;
            if (source[index] == quote) {
                index++;
                column++;
            }
            Token token = create_token_from_range(TOKEN_STRING, source, start, end, token_line, token_column);
            append_token(&tokens, &count, &capacity, token);
            continue;
        }

        switch (ch) {
            case '(':
                append_token(&tokens, &count, &capacity, create_token(TOKEN_LPAREN, "(", token_line, token_column));
                index++;
                column++;
                continue;
            case ')':
                append_token(&tokens, &count, &capacity, create_token(TOKEN_RPAREN, ")", token_line, token_column));
                index++;
                column++;
                continue;
            case '{':
                append_token(&tokens, &count, &capacity, create_token(TOKEN_LBRACE, "{", token_line, token_column));
                index++;
                column++;
                continue;
            case '}':
                append_token(&tokens, &count, &capacity, create_token(TOKEN_RBRACE, "}", token_line, token_column));
                index++;
                column++;
                continue;
            case ';':
                append_token(&tokens, &count, &capacity, create_token(TOKEN_SEMICOLON, ";", token_line, token_column));
                index++;
                column++;
                continue;
            case ',':
                append_token(&tokens, &count, &capacity, create_token(TOKEN_COMMA, ",", token_line, token_column));
                index++;
                column++;
                continue;
            case '+':
                append_token(&tokens, &count, &capacity, create_token(TOKEN_PLUS, "+", token_line, token_column));
                index++;
                column++;
                continue;
            case '-':
                append_token(&tokens, &count, &capacity, create_token(TOKEN_MINUS, "-", token_line, token_column));
                index++;
                column++;
                continue;
            case '*':
                append_token(&tokens, &count, &capacity, create_token(TOKEN_ASTERISK, "*", token_line, token_column));
                index++;
                column++;
                continue;
            case '/':
                append_token(&tokens, &count, &capacity, create_token(TOKEN_SLASH, "/", token_line, token_column));
                index++;
                column++;
                continue;
            case '<':
                if (source[index + 1] == '=') {
                    append_token(&tokens, &count, &capacity, create_token(TOKEN_LEQ, "<=", token_line, token_column));
                    index += 2;
                    column += 2;
                } else {
                    append_token(&tokens, &count, &capacity, create_token(TOKEN_LT, "<", token_line, token_column));
                    index++;
                    column++;
                }
                continue;
            case '>':
                if (source[index + 1] == '=') {
                    append_token(&tokens, &count, &capacity, create_token(TOKEN_GEQ, ">=", token_line, token_column));
                    index += 2;
                    column += 2;
                } else {
                    append_token(&tokens, &count, &capacity, create_token(TOKEN_GT, ">", token_line, token_column));
                    index++;
                    column++;
                }
                continue;
            case '!':
                if (source[index + 1] == '=') {
                    append_token(&tokens, &count, &capacity, create_token(TOKEN_NEQ, "!=", token_line, token_column));
                    index += 2;
                    column += 2;
                } else {
                    append_token(&tokens, &count, &capacity, create_token(TOKEN_UNKNOWN, "!", token_line, token_column));
                    index++;
                    column++;
                }
                continue;
            case '=':
                if (source[index + 1] == '=') {
                    append_token(&tokens, &count, &capacity, create_token(TOKEN_EQUAL, "==", token_line, token_column));
                    index += 2;
                    column += 2;
                } else {
                    append_token(&tokens, &count, &capacity, create_token(TOKEN_ASSIGN, "=", token_line, token_column));
                    index++;
                    column++;
                }
                continue;
            default:
                append_token(&tokens, &count, &capacity, create_token_from_range(TOKEN_UNKNOWN, source, index, index + 1, token_line, token_column));
                index++;
                column++;
                continue;
        }
    }

    append_token(&tokens, &count, &capacity, create_token(TOKEN_EOF, "", line, column));

    if (out_count) {
        *out_count = count;
    }
    return tokens;
}

void free_tokens(Token *tokens, size_t count) {
    if (!tokens) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        free(tokens[i].lexeme);
    }
    free(tokens);
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_UNKNOWN: return "UNKNOWN";
        case TOKEN_EOF: return "EOF";
        case TOKEN_NUCLEOTIDE: return "NUCLEOTIDE";
        case TOKEN_SEQUENCE: return "SEQUENCE";
        case TOKEN_GENE: return "GENE";
        case TOKEN_NUCLEUS: return "NUCLEUS";
        case TOKEN_IF_HOMOLOGOUS: return "IF_HOMOLOGOUS";
        case TOKEN_ELSE_MUTATE: return "ELSE_MUTATE";
        case TOKEN_REPLICATE_WHILE: return "REPLICATE_WHILE";
        case TOKEN_EXPRESS: return "EXPRESS";
        case TOKEN_SECRETE: return "SECRETE";
        case TOKEN_RELEASE: return "RELEASE";
        case TOKEN_RIBOSOME: return "RIBOSOME";
        case TOKEN_IF_BIND: return "IF_BIND";
        case TOKEN_REPLICATE: return "REPLICATE";
        case TOKEN_TRANSCRIPT: return "TRANSCRIPT";
        case TOKEN_BASES: return "BASES";
        case TOKEN_EMPTY: return "EMPTY";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_ASSIGN: return "ASSIGN";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_ASTERISK: return "ASTERISK";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_LT: return "LT";
        case TOKEN_GT: return "GT";
        case TOKEN_EQUAL: return "EQUAL";
        case TOKEN_NEQ: return "NEQ";
        case TOKEN_LEQ: return "LEQ";
        case TOKEN_GEQ: return "GEQ";
        default: return "UNKNOWN";
    }
}
