#ifndef LP_PARSER_H
#define LP_PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    TokenList *tokens;
    size_t current;
} Parser;

void parser_init(Parser *p, TokenList *tokens);
ASTNode *parser_parse(Parser *p);

#endif