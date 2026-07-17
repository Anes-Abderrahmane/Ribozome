#ifndef RIBOZOME_PARSER_H
#define RIBOZOME_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include "lexer.h"

typedef enum {
    AST_NODE_PROGRAM,
    AST_NODE_FUNCTION,
    AST_NODE_VARIABLE_DECL,
    AST_NODE_RETURN,
    AST_NODE_PRINT,
    AST_NODE_IF,
    AST_NODE_WHILE,
    AST_NODE_BLOCK,
    AST_NODE_EXPRESSION_STMT,
} ASTNodeType;

typedef enum {
    AST_EXPR_LITERAL,
    AST_EXPR_IDENTIFIER,
    AST_EXPR_BINARY,
    AST_EXPR_UNARY,
    AST_EXPR_CALL,
} ASTExprType;

typedef enum {
    AST_LITERAL_NUMBER,
    AST_LITERAL_STRING,
} ASTLiteralType;

typedef struct ASTExpr ASTExpr;
typedef struct ASTNode ASTNode;
typedef struct ASTParam ASTParam;

struct ASTParam {
    TokenType type;
    char *name;
    size_t line;
    size_t column;
};

struct ASTExpr {
    ASTExprType type;
    size_t line;
    size_t column;
    TokenType op;
    union {
        int64_t number;
        char *string;
        char *identifier;
        struct {
            ASTExpr *left;
            ASTExpr *right;
        } binary;
        struct {
            ASTExpr *operand;
        } unary;
        struct {
            ASTExpr *callee;
            ASTExpr **arguments;
            size_t argument_count;
        } call;
    } as;
};

struct ASTNode {
    ASTNodeType type;
    size_t line;
    size_t column;
    union {
        struct {
            ASTNode **declarations;
            size_t declaration_count;
        } program;
        struct {
            char *name;
            int is_main;
            ASTParam *parameters;
            size_t parameter_count;
            ASTNode *body;
        } function;
        struct {
            TokenType type;
            char *name;
            ASTExpr *initializer;
        } variable_decl;
        struct {
            ASTExpr *value;
        } return_stmt;
        struct {
            ASTExpr *value;
        } print_stmt;
        struct {
            ASTExpr *condition;
            ASTNode *then_branch;
            ASTNode *else_branch;
        } if_stmt;
        struct {
            ASTExpr *condition;
            ASTNode *body;
        } while_stmt;
        struct {
            ASTNode **statements;
            size_t statement_count;
        } block;
        struct {
            ASTExpr *expression;
        } expression_stmt;
    } as;
};

ASTNode *parse_tokens(const Token *tokens, size_t count);
void free_ast(ASTNode *root);
const char *ast_node_type_name(ASTNodeType type);
const char *ast_expr_type_name(ASTExprType type);

#endif /* RIBOZOME_PARSER_H */
