#ifndef LP_LEXER_H
#define LP_LEXER_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    // Literals
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,
    TOK_IDENT,
    
    // Keywords
    TOK_LET,
    TOK_FOR,
    TOK_IN,
    TOK_ASYNC,
    TOK_AWAIT,
    TOK_GPU,
    TOK_KERNEL,
    TOK_PARALLEL,  /* @parallel annotation */
    
    // Type keywords
    TOK_TYPE_I8,
    TOK_TYPE_I16,
    TOK_TYPE_I32,
    TOK_TYPE_I64,
    TOK_TYPE_U8,
    TOK_TYPE_U16,
    TOK_TYPE_U32,
    TOK_TYPE_U64,
    TOK_TYPE_F32,
    TOK_TYPE_F64,
    TOK_TYPE_STR,
    TOK_TYPE_PTR,
    TOK_TYPE_VOID,
    
    // Operators
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_EQ,
    TOK_EQEQ,
    TOK_NEQ,
    TOK_LT,
    TOK_GT,
    TOK_LTE,
    TOK_GTE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_BITAND,
    TOK_BITOR,
    TOK_BITXOR,
    TOK_SHL,
    TOK_SHR,
    
    // Lambda syntax
    TOK_BACKSLASH,  // λ abstraction
    TOK_ARROW,      // ->
    TOK_QUESTION,   // ?  (ternary)
    TOK_COLON,      // : (ternary/type)
    TOK_DOTDOT,     // .. (range)
    
    // Delimiters
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_SEMICOLON,
    TOK_COMMA,
    TOK_AT,         // @ for builtins
    
    // Special
    TOK_EOF,
    TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    size_t length;
    uint32_t line;
    uint32_t col;
    union {
        int64_t int_val;
        double float_val;
    } value;
} Token;

typedef struct {
    Token *tokens;
    size_t count;
    size_t capacity;
} TokenList;

typedef struct {
    const char *source;
    const char *current;
    const char *start;
    uint32_t line;
    uint32_t col;
} Lexer;

void lexer_init(Lexer *l, const char *source);
TokenList *lexer_tokenize(Lexer *l);
void token_list_free(TokenList *list);
const char *token_type_str(TokenType t);

#endif