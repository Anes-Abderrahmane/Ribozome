#include "codegen.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *label;
    char *value;
    size_t length;
} StringLiteral;

typedef struct {
    char *name;
} VariableInfo;

typedef struct {
    FILE *out;
    StringLiteral *strings;
    size_t string_count;
    size_t string_capacity;
    VariableInfo *variables;
    size_t variable_count;
    size_t variable_capacity;
    size_t label_counter;
} CodeGenContext;

static void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Ribozome codegen: allocation failed\n");
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

static char *make_label(CodeGenContext *ctx, const char *prefix) {
    size_t length = strlen(prefix) + 32;
    char *label = xmalloc(length);
    snprintf(label, length, "%s_%zu", prefix, ctx->label_counter++);
    return label;
}

static const char *find_string_label(CodeGenContext *ctx, const char *value) {
    for (size_t i = 0; i < ctx->string_count; ++i) {
        if (strcmp(ctx->strings[i].value, value) == 0) {
            return ctx->strings[i].label;
        }
    }
    return NULL;
}

static int is_variable_registered(CodeGenContext *ctx, const char *name) {
    for (size_t i = 0; i < ctx->variable_count; ++i) {
        if (strcmp(ctx->variables[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void register_variable(CodeGenContext *ctx, const char *name) {
    if (is_variable_registered(ctx, name)) {
        return;
    }

    if (ctx->variable_count >= ctx->variable_capacity) {
        size_t new_capacity = ctx->variable_capacity == 0 ? 8 : ctx->variable_capacity * 2;
        VariableInfo *new_variables = xmalloc(new_capacity * sizeof(VariableInfo));
        if (ctx->variables) {
            memcpy(new_variables, ctx->variables, ctx->variable_count * sizeof(VariableInfo));
            free(ctx->variables);
        }
        ctx->variables = new_variables;
        ctx->variable_capacity = new_capacity;
    }

    ctx->variables[ctx->variable_count++].name = copy_string(name);
}

static const char *register_string_literal(CodeGenContext *ctx, const char *value) {
    const char *label = find_string_label(ctx, value);
    if (label) {
        return label;
    }

    if (ctx->string_count >= ctx->string_capacity) {
        size_t new_capacity = ctx->string_capacity == 0 ? 8 : ctx->string_capacity * 2;
        StringLiteral *new_strings = xmalloc(new_capacity * sizeof(StringLiteral));
        if (ctx->strings) {
            memcpy(new_strings, ctx->strings, ctx->string_count * sizeof(StringLiteral));
            free(ctx->strings);
        }
        ctx->strings = new_strings;
        ctx->string_capacity = new_capacity;
    }

    size_t length = strlen(value);
    StringLiteral literal;
    literal.label = make_label(ctx, "msg");
    literal.value = copy_string(value);
    literal.length = length;

    ctx->strings[ctx->string_count++] = literal;
    return ctx->strings[ctx->string_count - 1].label;
}

/* Count the number of decoded bytes a raw string literal represents,
 * accounting for escape sequences like \n, \t, \xNN, etc. */
static size_t literal_byte_length(const char *value) {
    size_t length = 0;
    while (*value) {
        if (*value == '\\' && *(value + 1) != '\0') {
            char next = *(value + 1);
            switch (next) {
                case 'x':
                    value += 4;
                    break;
                default:
                    value += 2;
                    break;
            }
        } else {
            value += 1;
        }
        length += 1;
    }
    return length;
}

static unsigned int hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') return (unsigned int)(ch - '0');
    if (ch >= 'a' && ch <= 'f') return (unsigned int)(ch - 'a' + 10);
    if (ch >= 'A' && ch <= 'F') return (unsigned int)(ch - 'A' + 10);
    return 0;
}

/* Emit a string literal as a comma-separated list of raw byte values.
 * NASM does not interpret C-style escapes inside db strings, so we decode
 * escape sequences ourselves and write literal byte tokens. */
static void escape_and_write_string(FILE *out, const char *value) {
    int first = 1;
    while (*value) {
        unsigned char ch;
        if (*value == '\\' && *(value + 1) != '\0') {
            char next = *(value + 1);
            switch (next) {
                case 'n': ch = '\n'; value += 2; break;
                case 't': ch = '\t'; value += 2; break;
                case 'r': ch = '\r'; value += 2; break;
                case '0': ch = '\0'; value += 2; break;
                case '\\': ch = '\\'; value += 2; break;
                case '"': ch = '"'; value += 2; break;
                case 'x':
                    if (value[2] != '\0' && value[3] != '\0') {
                        ch = (unsigned char)((hex_digit(value[2]) << 4) | hex_digit(value[3]));
                        value += 4;
                        break;
                    }
                    ch = (unsigned char)*value;
                    value += 1;
                    break;
                default:
                    ch = (unsigned char)*value;
                    value += 1;
                    break;
            }
        } else {
            ch = (unsigned char)*value;
            value += 1;
        }

        if (!first) {
            fputc(',', out);
        }
        first = 0;

        if (ch >= 32 && ch < 127 && ch != '\\' && ch != '\'') {
            fprintf(out, "'%c'", ch);
        } else {
            fprintf(out, "0x%02x", ch);
        }
    }
}

static void collect_string_literals(ASTExpr *expr, CodeGenContext *ctx);
static void collect_node_literals(ASTNode *node, CodeGenContext *ctx);

static void collect_string_literals(ASTExpr *expr, CodeGenContext *ctx) {
    if (!expr) {
        return;
    }

    switch (expr->type) {
        case AST_EXPR_LITERAL:
            if (expr->op == TOKEN_STRING) {
                register_string_literal(ctx, expr->as.string);
            }
            break;
        case AST_EXPR_BINARY:
            collect_string_literals(expr->as.binary.left, ctx);
            collect_string_literals(expr->as.binary.right, ctx);
            break;
        case AST_EXPR_UNARY:
            collect_string_literals(expr->as.unary.operand, ctx);
            break;
        case AST_EXPR_CALL:
            collect_string_literals(expr->as.call.callee, ctx);
            for (size_t i = 0; i < expr->as.call.argument_count; ++i) {
                collect_string_literals(expr->as.call.arguments[i], ctx);
            }
            break;
        default:
            break;
    }
}

static void collect_node_literals(ASTNode *node, CodeGenContext *ctx) {
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_NODE_PROGRAM:
            for (size_t i = 0; i < node->as.program.declaration_count; ++i) {
                collect_node_literals(node->as.program.declarations[i], ctx);
            }
            break;
        case AST_NODE_FUNCTION:
            collect_node_literals(node->as.function.body, ctx);
            break;
        case AST_NODE_VARIABLE_DECL:
            register_variable(ctx, node->as.variable_decl.name);
            collect_string_literals(node->as.variable_decl.initializer, ctx);
            break;
        case AST_NODE_RETURN:
            collect_string_literals(node->as.return_stmt.value, ctx);
            break;
        case AST_NODE_PRINT:
            collect_string_literals(node->as.print_stmt.value, ctx);
            break;
        case AST_NODE_IF:
            collect_string_literals(node->as.if_stmt.condition, ctx);
            collect_node_literals(node->as.if_stmt.then_branch, ctx);
            collect_node_literals(node->as.if_stmt.else_branch, ctx);
            break;
        case AST_NODE_WHILE:
            collect_string_literals(node->as.while_stmt.condition, ctx);
            collect_node_literals(node->as.while_stmt.body, ctx);
            break;
        case AST_NODE_BLOCK:
            for (size_t i = 0; i < node->as.block.statement_count; ++i) {
                collect_node_literals(node->as.block.statements[i], ctx);
            }
            break;
        case AST_NODE_EXPRESSION_STMT:
            collect_string_literals(node->as.expression_stmt.expression, ctx);
            break;
        default:
            break;
    }
}

static void generate_expression(ASTExpr *expr, CodeGenContext *ctx);
static void generate_node(ASTNode *node, CodeGenContext *ctx);

static void generate_expression(ASTExpr *expr, CodeGenContext *ctx) {
    if (!expr) {
        return;
    }

    switch (expr->type) {
        case AST_EXPR_LITERAL:
            if (expr->op == TOKEN_NUMBER) {
                fprintf(ctx->out, "PHOSPHORYLATE KINASE_A, %lld\n", (long long)expr->as.number);
            } else if (expr->op == TOKEN_STRING) {
                const char *label = find_string_label(ctx, expr->as.string);
                if (label) {
                    fprintf(ctx->out, "PHOSPHORYLATE KINASE_A, %s\n", label);
                }
            }
            break;
        case AST_EXPR_IDENTIFIER:
            if (is_variable_registered(ctx, expr->as.identifier)) {
                fprintf(ctx->out, "PHOSPHORYLATE KINASE_A, [%s]\n", expr->as.identifier);
            } else {
                fprintf(ctx->out, "PHOSPHORYLATE KINASE_A, %s\n", expr->as.identifier);
            }
            break;
        case AST_EXPR_BINARY:
            generate_expression(expr->as.binary.left, ctx);
            if (expr->as.binary.right) {
                if (expr->as.binary.right->type == AST_EXPR_LITERAL && expr->as.binary.right->op == TOKEN_NUMBER) {
                    fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_D, %lld\n", (long long)expr->as.binary.right->as.number);
                } else if (expr->as.binary.right->type == AST_EXPR_LITERAL && expr->as.binary.right->op == TOKEN_STRING) {
                    const char *label = find_string_label(ctx, expr->as.binary.right->as.string);
                    fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_D, %s\n", label ? label : "0");
                } else if (expr->as.binary.right->type == AST_EXPR_IDENTIFIER) {
                    fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_D, %s\n", expr->as.binary.right->as.identifier);
                } else {
                    generate_expression(expr->as.binary.right, ctx);
                    fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_D, KINASE_A\n");
                }

                switch (expr->op) {
                    case TOKEN_PLUS:
                        fprintf(ctx->out, "POLYMERIZE KINASE_A, RECEPTOR_D\n");
                        break;
                    case TOKEN_MINUS:
                        fprintf(ctx->out, "HYDROLYZE KINASE_A, RECEPTOR_D\n");
                        break;
                    default:
                        fprintf(ctx->out, "PHOSPHORYLATE KINASE_A, RECEPTOR_D\n");
                        break;
                }
            }
            break;
        case AST_EXPR_UNARY:
            generate_expression(expr->as.unary.operand, ctx);
            if (expr->op == TOKEN_MINUS) {
                fprintf(ctx->out, "NEGATE KINASE_A\n");
            }
            break;
        case AST_EXPR_CALL:
            generate_expression(expr->as.call.callee, ctx);
            for (size_t i = 0; i < expr->as.call.argument_count; ++i) {
                generate_expression(expr->as.call.arguments[i], ctx);
            }
            break;
        default:
            break;
    }
}

static void generate_node(ASTNode *node, CodeGenContext *ctx) {
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_NODE_PROGRAM:
            for (size_t i = 0; i < node->as.program.declaration_count; ++i) {
                generate_node(node->as.program.declarations[i], ctx);
            }
            break;
        case AST_NODE_FUNCTION: {
            fprintf(ctx->out, "%s:\n", node->as.function.name);
            generate_node(node->as.function.body, ctx);
            if (node->as.function.is_main) {
                int ends_with_return = 0;
                ASTNode *body = node->as.function.body;
                if (body && body->type == AST_NODE_BLOCK && body->as.block.statement_count > 0) {
                    ASTNode *last = body->as.block.statements[body->as.block.statement_count - 1];
                    if (last && last->type == AST_NODE_RETURN) {
                        ends_with_return = 1;
                    }
                }
                if (!ends_with_return) {
                    fprintf(ctx->out, "PHOSPHORYLATE KINASE_A, 60\n");
                    fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_D, 0\n");
                    fprintf(ctx->out, "EXOCYTOSE\n");
                }
            }
            break;
        }
        case AST_NODE_VARIABLE_DECL:
            if (node->as.variable_decl.initializer) {
                generate_expression(node->as.variable_decl.initializer, ctx);
                fprintf(ctx->out, "PHOSPHORYLATE [%s], KINASE_A\n", node->as.variable_decl.name);
            }
            break;
        case AST_NODE_RETURN:
            generate_expression(node->as.return_stmt.value, ctx);
            fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_D, KINASE_A\n");
            fprintf(ctx->out, "PHOSPHORYLATE KINASE_A, 60\n");
            fprintf(ctx->out, "EXOCYTOSE\n");
            break;
        case AST_NODE_PRINT:
            if (node->as.print_stmt.value && node->as.print_stmt.value->type == AST_EXPR_LITERAL && node->as.print_stmt.value->op == TOKEN_STRING) {
                const char *label = find_string_label(ctx, node->as.print_stmt.value->as.string);
                size_t length = label ? literal_byte_length(node->as.print_stmt.value->as.string) + 1 : 0;
                fprintf(ctx->out, "PHOSPHORYLATE KINASE_A, 1\n");
                fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_D, 1\n");
                fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_S, %s\n", label ? label : "0");
                fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_X, %zu\n", length);
                fprintf(ctx->out, "EXOCYTOSE\n");
            } else {
                generate_expression(node->as.print_stmt.value, ctx);
                fprintf(ctx->out, "PHOSPHORYLATE KINASE_A, 1\n");
                fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_D, 1\n");
                fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_S, KINASE_A\n");
                fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_X, 8\n");
                fprintf(ctx->out, "EXOCYTOSE\n");
            }
            break;
        case AST_NODE_IF: {
            char *else_label = make_label(ctx, "else");
            char *end_label = make_label(ctx, "endif");
            generate_expression(node->as.if_stmt.condition, ctx);
            fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_D, 0\n");
            fprintf(ctx->out, "ANNEAL KINASE_A, RECEPTOR_D\n");
            fprintf(ctx->out, "BIND_MATCH %s\n", else_label);
            generate_node(node->as.if_stmt.then_branch, ctx);
            fprintf(ctx->out, "TRANSLOCATE %s\n", end_label);
            fprintf(ctx->out, "%s:\n", else_label);
            if (node->as.if_stmt.else_branch) {
                generate_node(node->as.if_stmt.else_branch, ctx);
            }
            fprintf(ctx->out, "%s:\n", end_label);
            free(else_label);
            free(end_label);
            break;
        }
        case AST_NODE_WHILE: {
            char *start_label = make_label(ctx, "while_start");
            char *end_label = make_label(ctx, "while_end");
            fprintf(ctx->out, "%s:\n", start_label);
            generate_expression(node->as.while_stmt.condition, ctx);
            fprintf(ctx->out, "PHOSPHORYLATE RECEPTOR_D, 0\n");
            fprintf(ctx->out, "ANNEAL KINASE_A, RECEPTOR_D\n");
            fprintf(ctx->out, "BIND_MATCH %s\n", end_label);
            generate_node(node->as.while_stmt.body, ctx);
            fprintf(ctx->out, "TRANSLOCATE %s\n", start_label);
            fprintf(ctx->out, "%s:\n", end_label);
            free(start_label);
            free(end_label);
            break;
        }
        case AST_NODE_BLOCK:
            for (size_t i = 0; i < node->as.block.statement_count; ++i) {
                generate_node(node->as.block.statements[i], ctx);
            }
            break;
        case AST_NODE_EXPRESSION_STMT:
            generate_expression(node->as.expression_stmt.expression, ctx);
            break;
        default:
            break;
    }
}

static void write_data_section(CodeGenContext *ctx) {
    fprintf(ctx->out, "CHROMOSOME_CODING\n");
    for (size_t i = 0; i < ctx->string_count; ++i) {
        fprintf(ctx->out, "%s: db ", ctx->strings[i].label);
        escape_and_write_string(ctx->out, ctx->strings[i].value);
        fprintf(ctx->out, ", 0x0A\n");
    }
    for (size_t i = 0; i < ctx->variable_count; ++i) {
        fprintf(ctx->out, "%s: dq 0\n", ctx->variables[i].name);
    }
    fprintf(ctx->out, "\n");
}

static void free_context(CodeGenContext *ctx) {
    if (!ctx) {
        return;
    }
    for (size_t i = 0; i < ctx->string_count; ++i) {
        free(ctx->strings[i].label);
        free(ctx->strings[i].value);
    }
    free(ctx->strings);
    for (size_t i = 0; i < ctx->variable_count; ++i) {
        free(ctx->variables[i].name);
    }
    free(ctx->variables);
}

int generate_rna(ASTNode *root) {
    if (!root) {
        return -1;
    }

    CodeGenContext ctx;
    ctx.out = fopen("output.rna", "w");
    if (!ctx.out) {
        fprintf(stderr, "Ribozome codegen: failed to open output.rna for writing\n");
        return -1;
    }

    ctx.strings = NULL;
    ctx.string_count = 0;
    ctx.string_capacity = 0;
    ctx.variables = NULL;
    ctx.variable_count = 0;
    ctx.variable_capacity = 0;
    ctx.label_counter = 0;

    collect_node_literals(root, &ctx);
    write_data_section(&ctx);

    fprintf(ctx.out, "CHROMOSOME_EXPRESSION\n");
    fprintf(ctx.out, "global NUCLEUS\n");
    generate_node(root, &ctx);

    fclose(ctx.out);
    free_context(&ctx);
    return 0;
}
