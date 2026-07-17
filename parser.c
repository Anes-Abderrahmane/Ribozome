#include "parser.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const Token *tokens;
    size_t count;
    size_t position;
    int had_error;
} Parser;

static void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Ribozome parser: allocation failed\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static char *copy_string(const char *source) {
    size_t length = strlen(source);
    char *copy = xmalloc(length + 1);
    memcpy(copy, source, length + 1);
    return copy;
}

static ASTNode *allocate_node(ASTNodeType type, size_t line, size_t column) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->column = column;
    memset(&node->as, 0, sizeof(node->as));
    return node;
}

static ASTExpr *allocate_expr(ASTExprType type, size_t line, size_t column) {
    ASTExpr *expr = xmalloc(sizeof(ASTExpr));
    expr->type = type;
    expr->line = line;
    expr->column = column;
    expr->op = TOKEN_UNKNOWN;
    expr->as.identifier = NULL;
    return expr;
}

static ASTNode *allocate_program(void) {
    ASTNode *node = allocate_node(AST_NODE_PROGRAM, 1, 1);
    node->as.program.declarations = NULL;
    node->as.program.declaration_count = 0;
    return node;
}

static void append_node(ASTNode ***items, size_t *count, size_t *capacity, ASTNode *node) {
    if (*count >= *capacity) {
        size_t new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        ASTNode **new_items = xmalloc(new_capacity * sizeof(ASTNode *));
        if (*items) {
            memcpy(new_items, *items, (*count) * sizeof(ASTNode *));
            free(*items);
        }
        *items = new_items;
        *capacity = new_capacity;
    }
    (*items)[*count] = node;
    *count += 1;
}

static void append_expr(ASTExpr ***items, size_t *count, size_t *capacity, ASTExpr *expr) {
    if (*count >= *capacity) {
        size_t new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        ASTExpr **new_items = xmalloc(new_capacity * sizeof(ASTExpr *));
        if (*items) {
            memcpy(new_items, *items, (*count) * sizeof(ASTExpr *));
            free(*items);
        }
        *items = new_items;
        *capacity = new_capacity;
    }
    (*items)[*count] = expr;
    *count += 1;
}

static Token *current(Parser *parser) {
    if (parser->position >= parser->count) {
        return NULL;
    }
    return (Token *)&parser->tokens[parser->position];
}

static Token *previous_token(Parser *parser) {
    if (parser->position == 0) {
        return NULL;
    }
    return (Token *)&parser->tokens[parser->position - 1];
}

static int is_at_end(Parser *parser) {
    Token *token = current(parser);
    return token && token->type == TOKEN_EOF;
}

static int check(Parser *parser, TokenType type) {
    Token *token = current(parser);
    return token && token->type == type;
}

static int match(Parser *parser, TokenType type) {
    if (check(parser, type)) {
        parser->position += 1;
        return 1;
    }
    return 0;
}

static Token *advance(Parser *parser) {
    if (!is_at_end(parser)) {
        parser->position += 1;
    }
    return previous_token(parser);
}

static Token *consume(Parser *parser, TokenType type, const char *message) {
    if (check(parser, type)) {
        return advance(parser);
    }

    Token *token = current(parser);
    if (token) {
        fprintf(stderr, "Parse error at %zu:%zu: %s\n", token->line, token->column, message);
    } else {
        fprintf(stderr, "Parse error: %s\n", message);
    }
    parser->had_error = 1;
    return NULL;
}

static int is_type_keyword(TokenType type) {
    return type == TOKEN_NUCLEOTIDE || type == TOKEN_SEQUENCE || type == TOKEN_BASES || type == TOKEN_EMPTY;
}

static ASTExpr *parse_expression(Parser *parser);
static ASTExpr *parse_assignment(Parser *parser);
static ASTExpr *parse_equality(Parser *parser);
static ASTExpr *parse_comparison(Parser *parser);
static ASTExpr *parse_term(Parser *parser);
static ASTExpr *parse_factor(Parser *parser);
static ASTExpr *parse_unary(Parser *parser);
static ASTExpr *parse_call(Parser *parser);
static ASTExpr *parse_primary(Parser *parser);
static void free_expr(ASTExpr *expr);
static void free_node(ASTNode *node);
static ASTNode *parse_declaration(Parser *parser);
static ASTNode *parse_statement(Parser *parser);
static ASTNode *parse_block(Parser *parser);
static ASTNode *parse_function(Parser *parser, TokenType keyword_type);
static ASTExpr *new_literal_expr(TokenType literal_type, const char *lexeme, size_t line, size_t column) {
    ASTExpr *expr = allocate_expr(AST_EXPR_LITERAL, line, column);
    expr->op = literal_type;
    if (literal_type == TOKEN_NUMBER) {
        expr->as.number = strtoll(lexeme, NULL, 10);
    } else {
        expr->as.string = copy_string(lexeme);
    }
    return expr;
}

static ASTExpr *new_identifier_expr(const char *name, size_t line, size_t column) {
    ASTExpr *expr = allocate_expr(AST_EXPR_IDENTIFIER, line, column);
    expr->as.identifier = copy_string(name);
    return expr;
}

static ASTExpr *new_binary_expr(TokenType op, ASTExpr *left, ASTExpr *right, size_t line, size_t column) {
    ASTExpr *expr = allocate_expr(AST_EXPR_BINARY, line, column);
    expr->op = op;
    expr->as.binary.left = left;
    expr->as.binary.right = right;
    return expr;
}

static ASTExpr *new_unary_expr(TokenType op, ASTExpr *operand, size_t line, size_t column) {
    ASTExpr *expr = allocate_expr(AST_EXPR_UNARY, line, column);
    expr->op = op;
    expr->as.unary.operand = operand;
    return expr;
}

static ASTExpr *new_call_expr(ASTExpr *callee, ASTExpr **arguments, size_t argument_count, size_t line, size_t column) {
    ASTExpr *expr = allocate_expr(AST_EXPR_CALL, line, column);
    expr->as.call.callee = callee;
    expr->as.call.arguments = arguments;
    expr->as.call.argument_count = argument_count;
    return expr;
}

static ASTNode *parse_program(Parser *parser) {
    ASTNode *program = allocate_program();
    size_t capacity = 0;

    while (!is_at_end(parser)) {
        ASTNode *declaration = parse_declaration(parser);
        if (!declaration) {
            break;
        }
        append_node(&program->as.program.declarations, &program->as.program.declaration_count, &capacity, declaration);
    }
    return program;
}

static ASTNode *parse_function(Parser *parser, TokenType keyword_type) {
    Token *keyword = previous_token(parser);
    ASTNode *function = allocate_node(AST_NODE_FUNCTION, keyword->line, keyword->column);
    function->as.function.is_main = keyword_type == TOKEN_NUCLEUS;
    function->as.function.name = NULL;
    function->as.function.parameters = NULL;
    function->as.function.parameter_count = 0;
    function->as.function.body = NULL;

    int has_parameters = 0;
    if (!function->as.function.is_main) {
        Token *name_token = consume(parser, TOKEN_IDENTIFIER, "Expected function name after gene keyword.");
        if (!name_token) {
            free_ast(function);
            return NULL;
        }
        function->as.function.name = copy_string(name_token->lexeme);
        if (!consume(parser, TOKEN_LPAREN, "Expected '(' after function name.")) {
            free_ast(function);
            return NULL;
        }
        has_parameters = 1;
    } else {
        function->as.function.name = copy_string("NUCLEUS");
        if (check(parser, TOKEN_LPAREN)) {
            advance(parser);
            has_parameters = 1;
        } else if (!check(parser, TOKEN_LBRACE)) {
            if (!consume(parser, TOKEN_LPAREN, "Expected '(' or '{' after nucleus keyword.")) {
                free_ast(function);
                return NULL;
            }
            has_parameters = 1;
        }
    }

    if (has_parameters) {
        size_t param_capacity = 0;
        while (!check(parser, TOKEN_RPAREN) && !is_at_end(parser)) {
            if (!is_type_keyword(current(parser)->type)) {
                fprintf(stderr, "Parse error at %zu:%zu: Expected parameter type in function declaration.\n", current(parser)->line, current(parser)->column);
                parser->had_error = 1;
                free_ast(function);
                return NULL;
            }

            ASTParam param;
            param.type = current(parser)->type;
            param.line = current(parser)->line;
            param.column = current(parser)->column;
            advance(parser);

            Token *name_token = consume(parser, TOKEN_IDENTIFIER, "Expected parameter name.");
            if (!name_token) {
                free_ast(function);
                return NULL;
            }
            param.name = copy_string(name_token->lexeme);

            if (function->as.function.parameter_count >= param_capacity) {
                size_t new_capacity = param_capacity == 0 ? 4 : param_capacity * 2;
                ASTParam *new_parameters = xmalloc(new_capacity * sizeof(ASTParam));
                if (function->as.function.parameters) {
                    memcpy(new_parameters, function->as.function.parameters, function->as.function.parameter_count * sizeof(ASTParam));
                    free(function->as.function.parameters);
                }
                function->as.function.parameters = new_parameters;
                param_capacity = new_capacity;
            }
            function->as.function.parameters[function->as.function.parameter_count++] = param;

            if (!match(parser, TOKEN_COMMA)) {
                break;
            }
        }

        if (!consume(parser, TOKEN_RPAREN, "Expected ')' after function parameters.")) {
            free_ast(function);
            return NULL;
        }
    }

    if (!consume(parser, TOKEN_LBRACE, "Expected '{' before function body.")) {
        free_ast(function);
        return NULL;
    }

    ASTNode *body = parse_block(parser);
    if (!body) {
        free_ast(function);
        return NULL;
    }
    function->as.function.body = body;
    return function;
}

static ASTNode *parse_variable_declaration(Parser *parser) {
    Token *type_token = previous_token(parser);
    if (!type_token) {
        return NULL;
    }

    Token *name_token = consume(parser, TOKEN_IDENTIFIER, "Expected variable name after type.");
    if (!name_token) {
        return NULL;
    }

    ASTNode *declaration = allocate_node(AST_NODE_VARIABLE_DECL, type_token->line, type_token->column);
    declaration->as.variable_decl.type = type_token->type;
    declaration->as.variable_decl.name = copy_string(name_token->lexeme);
    declaration->as.variable_decl.initializer = NULL;

    if (match(parser, TOKEN_ASSIGN)) {
        ASTExpr *initializer = parse_expression(parser);
        if (!initializer) {
            free_ast(declaration);
            return NULL;
        }
        declaration->as.variable_decl.initializer = initializer;
    }

    if (!consume(parser, TOKEN_SEMICOLON, "Expected ';' after variable declaration.")) {
        free_ast(declaration);
        return NULL;
    }
    return declaration;
}

static ASTNode *parse_if_statement(Parser *parser) {
    Token *keyword = previous_token(parser);
    ASTNode *node = allocate_node(AST_NODE_IF, keyword->line, keyword->column);
    node->as.if_stmt.else_branch = NULL;

    if (!consume(parser, TOKEN_LPAREN, "Expected '(' after if keyword.")) {
        free_ast(node);
        return NULL;
    }

    node->as.if_stmt.condition = parse_expression(parser);
    if (!node->as.if_stmt.condition) {
        free_ast(node);
        return NULL;
    }

    if (!consume(parser, TOKEN_RPAREN, "Expected ')' after if condition.")) {
        free_ast(node);
        return NULL;
    }

    if (!consume(parser, TOKEN_LBRACE, "Expected '{' after if condition.")) {
        free_ast(node);
        return NULL;
    }

    ASTNode *then_branch = parse_block(parser);
    if (!then_branch) {
        free_ast(node);
        return NULL;
    }
    node->as.if_stmt.then_branch = then_branch;

    if (match(parser, TOKEN_ELSE_MUTATE)) {
        if (!consume(parser, TOKEN_LBRACE, "Expected '{' after else_mutate.")) {
            free_ast(node);
            return NULL;
        }
        ASTNode *else_branch = parse_block(parser);
        if (!else_branch) {
            free_ast(node);
            return NULL;
        }
        node->as.if_stmt.else_branch = else_branch;
    }

    return node;
}

static ASTNode *parse_while_statement(Parser *parser) {
    Token *keyword = previous_token(parser);
    ASTNode *node = allocate_node(AST_NODE_WHILE, keyword->line, keyword->column);

    if (!consume(parser, TOKEN_LPAREN, "Expected '(' after while keyword.")) {
        free_ast(node);
        return NULL;
    }

    node->as.while_stmt.condition = parse_expression(parser);
    if (!node->as.while_stmt.condition) {
        free_ast(node);
        return NULL;
    }

    if (!consume(parser, TOKEN_RPAREN, "Expected ')' after while condition.")) {
        free_ast(node);
        return NULL;
    }

    if (!consume(parser, TOKEN_LBRACE, "Expected '{' after while condition.")) {
        free_ast(node);
        return NULL;
    }

    ASTNode *body = parse_block(parser);
    if (!body) {
        free_ast(node);
        return NULL;
    }
    node->as.while_stmt.body = body;
    return node;
}

static ASTNode *parse_print_statement(Parser *parser) {
    Token *keyword = previous_token(parser);
    ASTNode *node = allocate_node(AST_NODE_PRINT, keyword->line, keyword->column);

    node->as.print_stmt.value = parse_expression(parser);
    if (!node->as.print_stmt.value) {
        free_ast(node);
        return NULL;
    }

    if (!consume(parser, TOKEN_SEMICOLON, "Expected ';' after express statement.")) {
        free_ast(node);
        return NULL;
    }
    return node;
}

static ASTNode *parse_return_statement(Parser *parser) {
    Token *keyword = previous_token(parser);
    ASTNode *node = allocate_node(AST_NODE_RETURN, keyword->line, keyword->column);

    node->as.return_stmt.value = parse_expression(parser);
    if (!node->as.return_stmt.value) {
        free_ast(node);
        return NULL;
    }

    if (!consume(parser, TOKEN_SEMICOLON, "Expected ';' after secrete statement.")) {
        free_ast(node);
        return NULL;
    }
    return node;
}

static ASTNode *parse_expression_statement(Parser *parser) {
    ASTExpr *expression = parse_expression(parser);
    if (!expression) {
        return NULL;
    }

    if (!consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression.")) {
        free_expr(expression);
        return NULL;
    }

    ASTNode *node = allocate_node(AST_NODE_EXPRESSION_STMT, expression->line, expression->column);
    node->as.expression_stmt.expression = expression;
    return node;
}

static ASTNode *parse_block(Parser *parser) {
    Token *left_brace = previous_token(parser);
    ASTNode *node = allocate_node(AST_NODE_BLOCK, left_brace->line, left_brace->column);
    node->as.block.statements = NULL;
    node->as.block.statement_count = 0;
    size_t capacity = 0;

    while (!check(parser, TOKEN_RBRACE) && !is_at_end(parser)) {
        ASTNode *statement = parse_statement(parser);
        if (!statement) {
            free_ast(node);
            return NULL;
        }
        append_node(&node->as.block.statements, &node->as.block.statement_count, &capacity, statement);
    }

    if (!consume(parser, TOKEN_RBRACE, "Expected '}' after block.")) {
        free_ast(node);
        return NULL;
    }
    return node;
}

static ASTNode *parse_declaration(Parser *parser) {
    if (match(parser, TOKEN_GENE) || match(parser, TOKEN_RIBOSOME) || match(parser, TOKEN_NUCLEUS)) {
        return parse_function(parser, previous_token(parser)->type);
    }
    if (is_type_keyword(current(parser)->type)) {
        advance(parser);
        return parse_variable_declaration(parser);
    }
    return parse_statement(parser);
}

static ASTNode *parse_statement(Parser *parser) {
    if (match(parser, TOKEN_LBRACE)) {
        return parse_block(parser);
    }
    if (match(parser, TOKEN_IF_HOMOLOGOUS) || match(parser, TOKEN_IF_BIND)) {
        return parse_if_statement(parser);
    }
    if (match(parser, TOKEN_REPLICATE_WHILE) || match(parser, TOKEN_REPLICATE)) {
        return parse_while_statement(parser);
    }
    if (match(parser, TOKEN_EXPRESS)) {
        return parse_print_statement(parser);
    }
    if (match(parser, TOKEN_SECRETE)) {
        return parse_return_statement(parser);
    }
    if (is_type_keyword(current(parser)->type)) {
        advance(parser);
        return parse_variable_declaration(parser);
    }
    return parse_expression_statement(parser);
}

static ASTExpr *parse_expression(Parser *parser) {
    return parse_assignment(parser);
}

static ASTExpr *parse_assignment(Parser *parser) {
    ASTExpr *expr = parse_equality(parser);
    if (!expr) {
        return NULL;
    }

    if (match(parser, TOKEN_ASSIGN)) {
        Token *equals = previous_token(parser);
        ASTExpr *value = parse_assignment(parser);
        if (!value) {
            free_expr(expr);
            return NULL;
        }

        if (expr->type != AST_EXPR_IDENTIFIER) {
            fprintf(stderr, "Parse error at %zu:%zu: Invalid assignment target.\n", expr->line, expr->column);
            parser->had_error = 1;
            free_expr(expr);
            free_expr(value);
            return NULL;
        }

        ASTExpr *assignment = new_binary_expr(TOKEN_ASSIGN, expr, value, equals->line, equals->column);
        return assignment;
    }
    return expr;
}

static ASTExpr *parse_equality(Parser *parser) {
    ASTExpr *expr = parse_comparison(parser);
    if (!expr) {
        return NULL;
    }

    while (match(parser, TOKEN_EQUAL) || match(parser, TOKEN_NEQ)) {
        Token *operator = previous_token(parser);
        ASTExpr *right = parse_comparison(parser);
        if (!right) {
            free_expr(expr);
            return NULL;
        }
        expr = new_binary_expr(operator->type, expr, right, operator->line, operator->column);
    }
    return expr;
}

static ASTExpr *parse_comparison(Parser *parser) {
    ASTExpr *expr = parse_term(parser);
    if (!expr) {
        return NULL;
    }

    while (match(parser, TOKEN_LT) || match(parser, TOKEN_GT) || match(parser, TOKEN_LEQ) || match(parser, TOKEN_GEQ)) {
        Token *operator = previous_token(parser);
        ASTExpr *right = parse_term(parser);
        if (!right) {
            free_expr(expr);
            return NULL;
        }
        expr = new_binary_expr(operator->type, expr, right, operator->line, operator->column);
    }
    return expr;
}

static ASTExpr *parse_term(Parser *parser) {
    ASTExpr *expr = parse_factor(parser);
    if (!expr) {
        return NULL;
    }

    while (match(parser, TOKEN_PLUS) || match(parser, TOKEN_MINUS)) {
        Token *operator = previous_token(parser);
        ASTExpr *right = parse_factor(parser);
        if (!right) {
            free_expr(expr);
            return NULL;
        }
        expr = new_binary_expr(operator->type, expr, right, operator->line, operator->column);
    }
    return expr;
}

static ASTExpr *parse_factor(Parser *parser) {
    ASTExpr *expr = parse_unary(parser);
    if (!expr) {
        return NULL;
    }

    while (match(parser, TOKEN_ASTERISK) || match(parser, TOKEN_SLASH)) {
        Token *operator = previous_token(parser);
        ASTExpr *right = parse_unary(parser);
        if (!right) {
            free_expr(expr);
            return NULL;
        }
        expr = new_binary_expr(operator->type, expr, right, operator->line, operator->column);
    }
    return expr;
}

static ASTExpr *parse_unary(Parser *parser) {
    if (match(parser, TOKEN_MINUS)) {
        Token *operator = previous_token(parser);
        ASTExpr *right = parse_unary(parser);
        if (!right) {
            return NULL;
        }
        return new_unary_expr(operator->type, right, operator->line, operator->column);
    }
    return parse_call(parser);
}

static ASTExpr *parse_call(Parser *parser) {
    ASTExpr *expr = parse_primary(parser);
    if (!expr) {
        return NULL;
    }

    while (1) {
        if (match(parser, TOKEN_LPAREN)) {
            size_t arg_capacity = 0;
            ASTExpr **arguments = NULL;
            size_t argument_count = 0;
            if (!check(parser, TOKEN_RPAREN)) {
                do {
                    ASTExpr *argument = parse_expression(parser);
                    if (!argument) {
                        free_expr(expr);
                        for (size_t i = 0; i < argument_count; ++i) {
                            free_expr(arguments[i]);
                        }
                        free(arguments);
                        return NULL;
                    }
                    append_expr(&arguments, &argument_count, &arg_capacity, argument);
                } while (match(parser, TOKEN_COMMA));
            }
            if (!consume(parser, TOKEN_RPAREN, "Expected ')' after call arguments.")) {
                free_expr(expr);
                for (size_t i = 0; i < argument_count; ++i) {
                    free_expr(arguments[i]);
                }
                free(arguments);
                return NULL;
            }
            expr = new_call_expr(expr, arguments, argument_count, expr->line, expr->column);
            continue;
        }
        break;
    }

    return expr;
}

static ASTExpr *parse_primary(Parser *parser) {
    if (match(parser, TOKEN_NUMBER)) {
        Token *token = previous_token(parser);
        return new_literal_expr(TOKEN_NUMBER, token->lexeme, token->line, token->column);
    }
    if (match(parser, TOKEN_STRING)) {
        Token *token = previous_token(parser);
        return new_literal_expr(TOKEN_STRING, token->lexeme, token->line, token->column);
    }
    if (match(parser, TOKEN_IDENTIFIER)) {
        Token *token = previous_token(parser);
        return new_identifier_expr(token->lexeme, token->line, token->column);
    }
    if (match(parser, TOKEN_LPAREN)) {
        ASTExpr *expr = parse_expression(parser);
        if (!expr) {
            return NULL;
        }
        if (!consume(parser, TOKEN_RPAREN, "Expected ')' after expression.")) {
            free_expr(expr);
            return NULL;
        }
        return expr;
    }

    Token *token = current(parser);
    if (token) {
        fprintf(stderr, "Parse error at %zu:%zu: Expected expression but found '%s'.\n", token->line, token->column, token->lexeme);
    } else {
        fprintf(stderr, "Parse error: Expected expression but reached end of input.\n");
    }
    parser->had_error = 1;
    return NULL;
}

static void free_node(ASTNode *node);

static void free_expr(ASTExpr *expr) {
    if (!expr) {
        return;
    }

    switch (expr->type) {
        case AST_EXPR_LITERAL:
            if (expr->op != TOKEN_NUMBER && expr->as.string) {
                free(expr->as.string);
            }
            break;
        case AST_EXPR_IDENTIFIER:
            free(expr->as.identifier);
            break;
        case AST_EXPR_BINARY:
            free_expr(expr->as.binary.left);
            free_expr(expr->as.binary.right);
            break;
        case AST_EXPR_UNARY:
            free_expr(expr->as.unary.operand);
            break;
        case AST_EXPR_CALL:
            free_expr(expr->as.call.callee);
            for (size_t i = 0; i < expr->as.call.argument_count; ++i) {
                free_expr(expr->as.call.arguments[i]);
            }
            free(expr->as.call.arguments);
            break;
    }
    free(expr);
}

static void free_node(ASTNode *node) {
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_NODE_PROGRAM:
            for (size_t i = 0; i < node->as.program.declaration_count; ++i) {
                free_node(node->as.program.declarations[i]);
            }
            free(node->as.program.declarations);
            break;
        case AST_NODE_FUNCTION:
            free(node->as.function.name);
            for (size_t i = 0; i < node->as.function.parameter_count; ++i) {
                free(node->as.function.parameters[i].name);
            }
            free(node->as.function.parameters);
            free_node(node->as.function.body);
            break;
        case AST_NODE_VARIABLE_DECL:
            free(node->as.variable_decl.name);
            free_expr(node->as.variable_decl.initializer);
            break;
        case AST_NODE_RETURN:
            free_expr(node->as.return_stmt.value);
            break;
        case AST_NODE_PRINT:
            free_expr(node->as.print_stmt.value);
            break;
        case AST_NODE_IF:
            free_expr(node->as.if_stmt.condition);
            free_node(node->as.if_stmt.then_branch);
            free_node(node->as.if_stmt.else_branch);
            break;
        case AST_NODE_WHILE:
            free_expr(node->as.while_stmt.condition);
            free_node(node->as.while_stmt.body);
            break;
        case AST_NODE_BLOCK:
            for (size_t i = 0; i < node->as.block.statement_count; ++i) {
                free_node(node->as.block.statements[i]);
            }
            free(node->as.block.statements);
            break;
        case AST_NODE_EXPRESSION_STMT:
            free_expr(node->as.expression_stmt.expression);
            break;
    }
    free(node);
}

ASTNode *parse_tokens(const Token *tokens, size_t count) {
    if (!tokens || count == 0) {
        return NULL;
    }

    Parser parser;
    parser.tokens = tokens;
    parser.count = count;
    parser.position = 0;
    parser.had_error = 0;

    ASTNode *program = parse_program(&parser);
    if (parser.had_error) {
        free_ast(program);
        return NULL;
    }
    return program;
}

void free_ast(ASTNode *root) {
    free_node(root);
}

const char *ast_node_type_name(ASTNodeType type) {
    switch (type) {
        case AST_NODE_PROGRAM: return "PROGRAM";
        case AST_NODE_FUNCTION: return "FUNCTION";
        case AST_NODE_VARIABLE_DECL: return "VARIABLE_DECL";
        case AST_NODE_RETURN: return "RETURN";
        case AST_NODE_PRINT: return "PRINT";
        case AST_NODE_IF: return "IF";
        case AST_NODE_WHILE: return "WHILE";
        case AST_NODE_BLOCK: return "BLOCK";
        case AST_NODE_EXPRESSION_STMT: return "EXPRESSION_STMT";
        default: return "UNKNOWN";
    }
}

const char *ast_expr_type_name(ASTExprType type) {
    switch (type) {
        case AST_EXPR_LITERAL: return "LITERAL";
        case AST_EXPR_IDENTIFIER: return "IDENTIFIER";
        case AST_EXPR_BINARY: return "BINARY";
        case AST_EXPR_UNARY: return "UNARY";
        case AST_EXPR_CALL: return "CALL";
        default: return "UNKNOWN";
    }
}
