#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_SEPARATOR,
    TOKEN_OPEN_PAR,
    TOKEN_CLOSE_PAR,
    TOKEN_OPEN_CURLY,
    TOKEN_CLOSE_CURLY,
    TOKEN_OPEN_SQUARE,
    TOKEN_CLOSE_SQUARE,
    TOKEN_OTHER
} token_type_t;

typedef struct Token {
    char *data;
    token_type_t type;
    struct Token *next;
} Token;

typedef struct LineList {
    int line_number;
    Token *head;
    Token *tail;
    struct LineList *next;
} LineList;

typedef struct QueueNode {
    LineList *line;
    struct QueueNode *next;
} QueueNode;

typedef struct Queue {
    QueueNode *front;
    QueueNode *rear;
} Queue;

static Token *create_token(const char *text, token_type_t type)
{
    Token *token = malloc(sizeof(*token));
    if (!token) {
        return NULL;
    }

    token->data = malloc(strlen(text) + 1);
    if (!token->data) {
        free(token);
        return NULL;
    }

    strcpy(token->data, text);
    token->type = type;
    token->next = NULL;
    return token;
}

static void append_token(LineList *line, Token *token)
{
    if (!line->head) {
        line->head = token;
        line->tail = token;
    } else {
        line->tail->next = token;
        line->tail = token;
    }
}

static LineList *create_line_list(int line_number)
{
    LineList *line = malloc(sizeof(*line));
    if (!line) {
        return NULL;
    }

    line->line_number = line_number;
    line->head = NULL;
    line->tail = NULL;
    line->next = NULL;
    return line;
}

static void enqueue_line(Queue *queue, LineList *line)
{
    QueueNode *node = malloc(sizeof(*node));
    if (!node) {
        return;
    }

    node->line = line;
    node->next = NULL;

    if (!queue->rear) {
        queue->front = node;
        queue->rear = node;
    } else {
        queue->rear->next = node;
        queue->rear = node;
    }
}

static void tokenize_line(const char *line_text, LineList *line)
{
    int i = 0;
    while (line_text[i] != '\0') {
        unsigned char ch = (unsigned char)line_text[i];

        if (isspace(ch)) {
            i++;
            continue;
        }

        if (ch == '!') {
            Token *token = create_token("!", TOKEN_SEPARATOR);
            append_token(line, token);
            i++;
        } else if (ch == '(') {
            Token *token = create_token("(", TOKEN_OPEN_PAR);
            append_token(line, token);
            i++;
        } else if (ch == ')') {
            Token *token = create_token(")", TOKEN_CLOSE_PAR);
            append_token(line, token);
            i++;
        } else if (ch == '{') {
            Token *token = create_token("{", TOKEN_OPEN_CURLY);
            append_token(line, token);
            i++;
        } else if (ch == '}') {
            Token *token = create_token("}", TOKEN_CLOSE_CURLY);
            append_token(line, token);
            i++;
        } else if (ch == '[') {
            Token *token = create_token("[", TOKEN_OPEN_SQUARE);
            append_token(line, token);
            i++;
        } else if (ch == ']') {
            Token *token = create_token("]", TOKEN_CLOSE_SQUARE);
            append_token(line, token);
            i++;
        } else if (isalpha(ch)) {
            int j = 0;
            char *word = malloc(256);
            if (!word) {
                return;
            }

            while (isalpha((unsigned char)line_text[i + j]) && j < 255) {
                word[j] = line_text[i + j];
                j++;
            }
            word[j] = '\0';

            Token *token = create_token(word, TOKEN_IDENTIFIER);
            append_token(line, token);
            free(word);
            i += j;
        } else if (isdigit(ch)) {
            int j = 0;
            char *number = malloc(256);
            if (!number) {
                return;
            }

            while (isdigit((unsigned char)line_text[i + j]) && j < 255) {
                number[j] = line_text[i + j];
                j++;
            }
            number[j] = '\0';

            Token *token = create_token(number, TOKEN_NUMBER);
            append_token(line, token);
            free(number);
            i += j;
        } else {
            char single[2] = { (char)ch, '\0' };
            Token *token = create_token(single, TOKEN_OTHER);
            append_token(line, token);
            i++;
        }
    }
}

static void tokenize_file(const char *buffer, Queue *queue)
{
    const char *cursor = buffer;
    int line_number = 1;

    while (*cursor != '\0') {
        const char *line_start = cursor;
        while (*cursor != '\0' && *cursor != '\n') {
            cursor++;
        }

        size_t line_length = (size_t)(cursor - line_start);
        char *line_copy = malloc(line_length + 1);
        if (!line_copy) {
            return;
        }

        memcpy(line_copy, line_start, line_length);
        line_copy[line_length] = '\0';

        LineList *line = create_line_list(line_number++);
        if (line) {
            tokenize_line(line_copy, line);
            enqueue_line(queue, line);
        }

        free(line_copy);

        if (*cursor == '\n') {
            cursor++;
        }
    }
}

static void print_queue(const Queue *queue)
{
    QueueNode *node = queue->front;
    while (node) {
        LineList *line = node->line;
        printf("Line %d:", line->line_number);

        for (Token *token = line->head; token; token = token->next) {
            printf(" [%s]", token->data);
        }
        putchar('\n');
        node = node->next;
    }
}

static void free_tokens(Token *token)
{
    while (token) {
        Token *next = token->next;
        free(token->data);
        free(token);
        token = next;
    }
}

static void free_queue(Queue *queue)
{
    QueueNode *node = queue->front;
    while (node) {
        QueueNode *next = node->next;
        free_tokens(node->line->head);
        free(node->line);
        free(node);
        node = next;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.dna>\n", argv[0]);
        return 1;
    }

    char *buffer = NULL;
    long length;
    FILE *f = fopen(argv[1], "rb");

    if (!f) {
        perror("fopen");
        return 1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return 1;
    }

    length = ftell(f);
    if (length < 0) {
        perror("ftell");
        fclose(f);
        return 1;
    }

    rewind(f);

    buffer = malloc((size_t)length + 1);
    if (!buffer) {
        fprintf(stderr, "malloc failed\n");
        fclose(f);
        return 1;
    }

    if (fread(buffer, 1, (size_t)length, f) != (size_t)length) {
        fprintf(stderr, "fread failed\n");
        free(buffer);
        fclose(f);
        return 1;
    }

    buffer[length] = '\0';
    fclose(f);

    Queue queue = { 0 };
    tokenize_file(buffer, &queue);
    print_queue(&queue);

    free(buffer);
    free_queue(&queue);
    return 0;
}