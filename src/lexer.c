#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static inline int is_at_end(Lexer *l) {
    return *l->current == '\0';
}

static inline char advance(Lexer *l) {
    l->col++;
    return *l->current++;
}

static inline char peek(Lexer *l) {
    return *l->current;
}

static inline char peek_next(Lexer *l) {
    if (is_at_end(l)) return '\0';
    return l->current[1];
}

static inline int match(Lexer *l, char expected) {
    if (is_at_end(l) || *l->current != expected) return 0;
    l->current++;
    l->col++;
    return 1;
}

static void skip_whitespace(Lexer *l) {
    for (;;) {
        char c = peek(l);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance(l);
                break;
            case '\n':
                l->line++;
                l->col = 0;
                advance(l);
                break;
            case '/':
                if (peek_next(l) == '/') {
                    while (peek(l) != '\n' && !is_at_end(l)) advance(l);
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static Token make_token(Lexer *l, TokenType type) {
    Token t;
    t.type = type;
    t.start = l->start;
    t. length = (size_t)(l->current - l->start);
    t. line = l->line;
    t. col = l->col - t.length;
    return t;
}

static Token error_token(Lexer *l) {
    Token t;
    t.type = TOK_ERROR;
    t. start = l->start;
    t. length = 1;
    t. line = l->line;
    t. col = l->col;
    return t;
}

static TokenType check_keyword(const char *start, size_t len, 
                                const char *rest, size_t rest_len, TokenType type) {
    if (len == rest_len + 1 && memcmp(start + 1, rest, rest_len) == 0) {
        return type;
    }
    return TOK_IDENT;
}

static TokenType identifier_type(Lexer *l) {
    size_t len = l->current - l->start;
    
    /* Check for type keywords with exact matching */
    if (len == 2) {
        if (memcmp(l->start, "i8", 2) == 0) return TOK_TYPE_I8;
        if (memcmp(l->start, "u8", 2) == 0) return TOK_TYPE_U8;
    } else if (len == 3) {
        if (memcmp(l->start, "i16", 3) == 0) return TOK_TYPE_I16;
        if (memcmp(l->start, "i32", 3) == 0) return TOK_TYPE_I32;
        if (memcmp(l->start, "i64", 3) == 0) return TOK_TYPE_I64;
        if (memcmp(l->start, "u16", 3) == 0) return TOK_TYPE_U16;
        if (memcmp(l->start, "u32", 3) == 0) return TOK_TYPE_U32;
        if (memcmp(l->start, "u64", 3) == 0) return TOK_TYPE_U64;
        if (memcmp(l->start, "f32", 3) == 0) return TOK_TYPE_F32;
        if (memcmp(l->start, "f64", 3) == 0) return TOK_TYPE_F64;
        if (memcmp(l->start, "str", 3) == 0) return TOK_TYPE_STR;
        if (memcmp(l->start, "ptr", 3) == 0) return TOK_TYPE_PTR;
        if (memcmp(l->start, "gpu", 3) == 0) return TOK_GPU;
        if (memcmp(l->start, "for", 3) == 0) return TOK_FOR;
        if (memcmp(l->start, "let", 3) == 0) return TOK_LET;
    } else if (len == 4) {
        if (memcmp(l->start, "void", 4) == 0) return TOK_TYPE_VOID;
    }
    
    switch (l->start[0]) {
        case 'a':
            if (len > 1) {
                if (l->start[1] == 's') 
                    return check_keyword(l->start + 1, len - 1, "ync", 3, TOK_ASYNC);
                if (l->start[1] == 'w') 
                    return check_keyword(l->start + 1, len - 1, "ait", 3, TOK_AWAIT);
            }
            break;
        case 'i': 
            if (len == 2 && l->start[1] == 'n') return TOK_IN;
            break;
        case 'k': return check_keyword(l->start, len, "ernel", 5, TOK_KERNEL);
    }
    return TOK_IDENT;
}

static Token identifier(Lexer *l) {
    while (isalnum(peek(l)) || peek(l) == '_') advance(l);
    return make_token(l, identifier_type(l));
}

static Token number(Lexer *l) {
    Token t;
    int is_float = 0;
    
    while (isdigit(peek(l))) advance(l);
    
    if (peek(l) == '.' && isdigit(peek_next(l))) {
        is_float = 1;
        advance(l);
        while (isdigit(peek(l))) advance(l);
    }
    
    if (peek(l) == 'e' || peek(l) == 'E') {
        is_float = 1;
        advance(l);
        if (peek(l) == '+' || peek(l) == '-') advance(l);
        while (isdigit(peek(l))) advance(l);
    }
    
    t = make_token(l, is_float ?  TOK_FLOAT : TOK_INT);
    
    if (is_float) {
        t. value.float_val = strtod(l->start, NULL);
    } else {
        t. value.int_val = strtoll(l->start, NULL, 10);
    }
    
    return t;
}

static Token string(Lexer *l) {
    while (peek(l) != '"' && ! is_at_end(l)) {
        if (peek(l) == '\n') { l->line++; l->col = 0; }
        if (peek(l) == '\\' && peek_next(l) != '\0') advance(l);
        advance(l);
    }
    
    if (is_at_end(l)) return error_token(l);
    
    advance(l);
    return make_token(l, TOK_STRING);
}

static Token scan_token(Lexer *l) {
    skip_whitespace(l);
    l->start = l->current;
    
    if (is_at_end(l)) return make_token(l, TOK_EOF);
    
    char c = advance(l);
    
    if (isalpha(c) || c == '_') return identifier(l);
    if (isdigit(c)) return number(l);
    
    switch (c) {
        case '(': return make_token(l, TOK_LPAREN);
        case ')': return make_token(l, TOK_RPAREN);
        case '{': return make_token(l, TOK_LBRACE);
        case '}': return make_token(l, TOK_RBRACE);
        case '[': return make_token(l, TOK_LBRACKET);
        case ']': return make_token(l, TOK_RBRACKET);
        case ';': return make_token(l, TOK_SEMICOLON);
        case ',': return make_token(l, TOK_COMMA);
        case '@': return make_token(l, TOK_AT);
        case '\\': return make_token(l, TOK_BACKSLASH);
        case '?': return make_token(l, TOK_QUESTION);
        case ':': return make_token(l, TOK_COLON);
        case '+': return make_token(l, TOK_PLUS);
        case '*': return make_token(l, TOK_STAR);
        case '/': return make_token(l, TOK_SLASH);
        case '%': return make_token(l, TOK_PERCENT);
        case '^': return make_token(l, TOK_BITXOR);
        case '"': return string(l);
        
        case '-':
            return make_token(l, match(l, '>') ? TOK_ARROW : TOK_MINUS);
        case '.':
            return make_token(l, match(l, '.') ? TOK_DOTDOT : TOK_ERROR);
        case '=':
            return make_token(l, match(l, '=') ? TOK_EQEQ : TOK_EQ);
        case '!':
            return make_token(l, match(l, '=') ? TOK_NEQ : TOK_NOT);
        case '<':
            if (match(l, '<')) return make_token(l, TOK_SHL);
            return make_token(l, match(l, '=') ? TOK_LTE : TOK_LT);
        case '>':
            if (match(l, '>')) return make_token(l, TOK_SHR);
            return make_token(l, match(l, '=') ? TOK_GTE : TOK_GT);
        case '&':
            return make_token(l, match(l, '&') ? TOK_AND : TOK_BITAND);
        case '|':
            return make_token(l, match(l, '|') ? TOK_OR : TOK_BITOR);
    }
    
    return error_token(l);
}

void lexer_init(Lexer *l, const char *source) {
    l->source = source;
    l->current = source;
    l->start = source;
    l->line = 1;
    l->col = 1;
}

TokenList *lexer_tokenize(Lexer *l) {
    TokenList *list = malloc(sizeof(TokenList));
    list->capacity = 256;
    list->count = 0;
    list->tokens = malloc(sizeof(Token) * list->capacity);
    
    for (;;) {
        Token t = scan_token(l);
        
        if (list->count >= list->capacity) {
            list->capacity *= 2;
            list->tokens = realloc(list->tokens, sizeof(Token) * list->capacity);
        }
        
        list->tokens[list->count++] = t;
        
        if (t.type == TOK_EOF || t.type == TOK_ERROR) break;
    }
    
    return list;
}

void token_list_free(TokenList *list) {
    free(list->tokens);
    free(list);
}

const char *token_type_str(TokenType t) {
    static const char *names[] = {
        "INT", "FLOAT", "STRING", "IDENT",
        "LET", "FOR", "IN", "ASYNC", "AWAIT", "GPU", "KERNEL",
        "PLUS", "MINUS", "STAR", "SLASH", "PERCENT",
        "EQ", "EQEQ", "NEQ", "LT", "GT", "LTE", "GTE",
        "AND", "OR", "NOT", "BITAND", "BITOR", "BITXOR", "SHL", "SHR",
        "BACKSLASH", "ARROW", "QUESTION", "COLON", "DOTDOT",
        "LPAREN", "RPAREN", "LBRACE", "RBRACE", "LBRACKET", "RBRACKET",
        "SEMICOLON", "COMMA", "AT",
        "EOF", "ERROR"
    };
    return names[t];
}