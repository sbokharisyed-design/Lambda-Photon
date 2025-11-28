#include "parser.h"
#include <stdlib.h>
#include <string.h>

static Token *current(Parser *p) {
    return &p->tokens->tokens[p->current];
}

static Token *previous(Parser *p) {
    return &p->tokens->tokens[p->current - 1];
}

static int is_at_end(Parser *p) {
    return current(p)->type == TOK_EOF;
}

static Token *advance(Parser *p) {
    if (! is_at_end(p)) p->current++;
    return previous(p);
}

static int check(Parser *p, TokenType type) {
    if (is_at_end(p)) return 0;
    return current(p)->type == type;
}

static int match(Parser *p, TokenType type) {
    if (check(p, type)) { advance(p); return 1; }
    return 0;
}

static char *copy_token_str(Token *t) {
    char *s = malloc(t->length + 1);
    memcpy(s, t->start, t->length);
    s[t->length] = '\0';
    return s;
}

/* Parse a type annotation */
static Type *parse_type(Parser *p) {
    Token *t = current(p);
    Type *type = NULL;
    
    switch (t->type) {
        case TOK_TYPE_I8:  type = type_new(TYPE_I8); break;
        case TOK_TYPE_I16: type = type_new(TYPE_I16); break;
        case TOK_TYPE_I32: type = type_new(TYPE_I32); break;
        case TOK_TYPE_I64: type = type_new(TYPE_I64); break;
        case TOK_TYPE_U8:  type = type_new(TYPE_U8); break;
        case TOK_TYPE_U16: type = type_new(TYPE_U16); break;
        case TOK_TYPE_U32: type = type_new(TYPE_U32); break;
        case TOK_TYPE_U64: type = type_new(TYPE_U64); break;
        case TOK_TYPE_F32: type = type_new(TYPE_F32); break;
        case TOK_TYPE_F64: type = type_new(TYPE_F64); break;
        case TOK_TYPE_STR: type = type_new(TYPE_STR); break;
        case TOK_TYPE_PTR: type = type_new(TYPE_PTR); break;
        case TOK_TYPE_VOID: type = type_new(TYPE_VOID); break;
        default:
            /* Unknown type - default to i64 */
            type = type_new(TYPE_I64);
            return type;
    }
    
    advance(p);
    return type;
}

static ASTNode *expression(Parser *p);
static ASTNode *statement(Parser *p);

static ASTNode *primary(Parser *p) {
    Token *t = current(p);
    
    if (match(p, TOK_INT)) {
        ASTNode *n = ast_new(NODE_INT_LIT, t->line, t->col);
        n->data.int_val = previous(p)->value. int_val;
        return n;
    }
    
    if (match(p, TOK_FLOAT)) {
        ASTNode *n = ast_new(NODE_FLOAT_LIT, t->line, t->col);
        n->data.float_val = previous(p)->value.float_val;
        return n;
    }
    
    if (match(p, TOK_STRING)) {
        ASTNode *n = ast_new(NODE_STRING_LIT, t->line, t->col);
        Token *prev = previous(p);
        n->data.string.len = prev->length - 2;
        n->data.string.value = malloc(n->data.string. len + 1);
        memcpy(n->data. string.value, prev->start + 1, n->data. string.len);
        n->data. string.value[n->data.string. len] = '\0';
        return n;
    }
    
    if (match(p, TOK_IDENT)) {
        ASTNode *n = ast_new(NODE_IDENT, t->line, t->col);
        n->data. ident.name = copy_token_str(previous(p));
        n->data.ident.len = previous(p)->length;
        return n;
    }
    
    if (match(p, TOK_LPAREN)) {
        ASTNode *expr = expression(p);
        match(p, TOK_RPAREN);
        return expr;
    }
    
    if (match(p, TOK_LBRACKET)) {
        ASTNode *n = ast_new(NODE_ARRAY, t->line, t->col);
        size_t cap = 8;
        n->data.array. elements = malloc(sizeof(ASTNode*) * cap);
        n->data. array.count = 0;
        
        if (! check(p, TOK_RBRACKET)) {
            do {
                if (n->data. array.count >= cap) {
                    cap *= 2;
                    n->data.array.elements = realloc(n->data.array.elements, 
                                                      sizeof(ASTNode*) * cap);
                }
                n->data.array.elements[n->data.array.count++] = expression(p);
            } while (match(p, TOK_COMMA));
        }
        match(p, TOK_RBRACKET);
        return n;
    }
    
    if (match(p, TOK_AT)) {
        ASTNode *n = ast_new(NODE_BUILTIN, t->line, t->col);
        n->data.builtin.name = copy_token_str(current(p));
        advance(p);
        
        n->data.builtin.elements = NULL;
        n->data.builtin. count = 0;
        
        if (match(p, TOK_LPAREN)) {
            size_t cap = 4;
            n->data.builtin. elements = malloc(sizeof(ASTNode*) * cap);
            
            if (!check(p, TOK_RPAREN)) {
                do {
                    if (n->data.builtin.count >= cap) {
                        cap *= 2;
                        n->data.builtin.elements = realloc(n->data.builtin.elements,
                                                           sizeof(ASTNode*) * cap);
                    }
                    n->data.builtin.elements[n->data. builtin.count++] = expression(p);
                } while (match(p, TOK_COMMA));
            }
            match(p, TOK_RPAREN);
        }
        return n;
    }
    
    if (match(p, TOK_BACKSLASH)) {
        ASTNode *n = ast_new(NODE_LAMBDA, t->line, t->col);
        size_t cap = 4;
        n->data.lambda.params = malloc(sizeof(char*) * cap);
        n->data.lambda.param_types = NULL;
        n->data. lambda.param_count = 0;
        
        while (check(p, TOK_IDENT)) {
            if (n->data. lambda.param_count >= cap) {
                cap *= 2;
                n->data.lambda.params = realloc(n->data.lambda.params, 
                                                 sizeof(char*) * cap);
            }
            n->data.lambda.params[n->data. lambda.param_count++] = copy_token_str(current(p));
            advance(p);
        }
        
        match(p, TOK_ARROW);
        n->data.lambda. body = expression(p);
        return n;
    }
    
    return NULL;
}

static ASTNode *postfix(Parser *p) {
    ASTNode *left = primary(p);
    if (! left) return NULL;
    
    for (;;) {
        Token *t = current(p);
        
        if (check(p, TOK_LBRACKET)) {
            advance(p);
            ASTNode *idx = ast_new(NODE_INDEX, t->line, t->col);
            idx->data.index.array = left;
            idx->data.index. index = expression(p);
            match(p, TOK_RBRACKET);
            left = idx;
            continue;
        }
        
        break;
    }
    
    return left;
}

static ASTNode *unary(Parser *p) {
    Token *t = current(p);
    
    if (match(p, TOK_MINUS)) {
        ASTNode *n = ast_new(NODE_UNARY, t->line, t->col);
        n->data. unary.op = OP_NEG;
        n->data.unary.operand = unary(p);
        return n;
    }
    
    if (match(p, TOK_NOT)) {
        ASTNode *n = ast_new(NODE_UNARY, t->line, t->col);
        n->data.unary.op = OP_NOT;
        n->data.unary.operand = unary(p);
        return n;
    }
    
    return postfix(p);
}

static ASTNode *factor(Parser *p) {
    ASTNode *left = unary(p);
    
    while (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT)) {
        Token *t = current(p);
        Operator op;
        if (match(p, TOK_STAR)) op = OP_MUL;
        else if (match(p, TOK_SLASH)) op = OP_DIV;
        else { match(p, TOK_PERCENT); op = OP_MOD; }
        
        ASTNode *n = ast_new(NODE_BINARY, t->line, t->col);
        n->data.binary. op = op;
        n->data.binary.left = left;
        n->data.binary.right = unary(p);
        left = n;
    }
    
    return left;
}

static ASTNode *term(Parser *p) {
    ASTNode *left = factor(p);
    
    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        Token *t = current(p);
        Operator op = match(p, TOK_PLUS) ?  OP_ADD : (advance(p), OP_SUB);
        
        ASTNode *n = ast_new(NODE_BINARY, t->line, t->col);
        n->data.binary.op = op;
        n->data.binary.left = left;
        n->data.binary. right = factor(p);
        left = n;
    }
    
    return left;
}

static ASTNode *comparison(Parser *p) {
    ASTNode *left = term(p);
    
    while (check(p, TOK_LT) || check(p, TOK_GT) || 
           check(p, TOK_LTE) || check(p, TOK_GTE)) {
        Token *t = current(p);
        Operator op;
        if (match(p, TOK_LT)) op = OP_LT;
        else if (match(p, TOK_GT)) op = OP_GT;
        else if (match(p, TOK_LTE)) op = OP_LTE;
        else { match(p, TOK_GTE); op = OP_GTE; }
        
        ASTNode *n = ast_new(NODE_BINARY, t->line, t->col);
        n->data.binary.op = op;
        n->data.binary.left = left;
        n->data.binary.right = term(p);
        left = n;
    }
    
    return left;
}

static ASTNode *equality(Parser *p) {
    ASTNode *left = comparison(p);
    
    while (check(p, TOK_EQEQ) || check(p, TOK_NEQ)) {
        Token *t = current(p);
        Operator op = match(p, TOK_EQEQ) ? OP_EQ : (advance(p), OP_NEQ);
        
        ASTNode *n = ast_new(NODE_BINARY, t->line, t->col);
        n->data.binary.op = op;
        n->data.binary.left = left;
        n->data.binary. right = comparison(p);
        left = n;
    }
    
    return left;
}

static ASTNode *logical_and(Parser *p) {
    ASTNode *left = equality(p);
    
    while (match(p, TOK_AND)) {
        Token *t = previous(p);
        ASTNode *n = ast_new(NODE_BINARY, t->line, t->col);
        n->data.binary.op = OP_AND;
        n->data.binary.left = left;
        n->data.binary.right = equality(p);
        left = n;
    }
    
    return left;
}

static ASTNode *logical_or(Parser *p) {
    ASTNode *left = logical_and(p);
    
    while (match(p, TOK_OR)) {
        Token *t = previous(p);
        ASTNode *n = ast_new(NODE_BINARY, t->line, t->col);
        n->data.binary. op = OP_OR;
        n->data.binary. left = left;
        n->data. binary.right = logical_and(p);
        left = n;
    }
    
    return left;
}

static ASTNode *ternary_expr(Parser *p) {
    ASTNode *cond = logical_or(p);
    
    if (match(p, TOK_QUESTION)) {
        Token *t = previous(p);
        ASTNode *n = ast_new(NODE_TERNARY, t->line, t->col);
        n->data.ternary. cond = cond;
        n->data.ternary.then_branch = expression(p);
        match(p, TOK_COLON);
        n->data.ternary.else_branch = ternary_expr(p);
        return n;
    }
    
    return cond;
}

static ASTNode *expression(Parser *p) {
    return ternary_expr(p);
}

static ASTNode *block(Parser *p) {
    Token *t = current(p);
    ASTNode *n = ast_new(NODE_BLOCK, t->line, t->col);
    size_t cap = 16;
    n->data.block.stmts = malloc(sizeof(ASTNode*) * cap);
    n->data.block.count = 0;
    
    while (!check(p, TOK_RBRACE) && !is_at_end(p)) {
        if (n->data.block.count >= cap) {
            cap *= 2;
            n->data.block.stmts = realloc(n->data.block.stmts, 
                                           sizeof(ASTNode*) * cap);
        }
        n->data.block.stmts[n->data.block.count++] = statement(p);
    }
    
    match(p, TOK_RBRACE);
    return n;
}

static ASTNode *statement(Parser *p) {
    Token *t = current(p);
    
    /* Check for @parallel annotation */
    int is_parallel = 0;
    if (match(p, TOK_AT)) {
        Token *annotation = current(p);
        if (annotation->length == 8 && memcmp(annotation->start, "parallel", 8) == 0) {
            is_parallel = 1;
            advance(p);
            t = current(p);  /* Update t to the for token */
        } else {
            /* Not @parallel, this is a builtin call - backtrack */
            p->current--;
        }
    }
    
    if (match(p, TOK_LET)) {
        ASTNode *n = ast_new(NODE_LET, t->line, t->col);
        n->data.let.name = copy_token_str(current(p));
        advance(p);
        n->data.let.type_annotation = NULL;
        
        if (match(p, TOK_COLON)) {
            n->data.let.type_annotation = parse_type(p);
        }
        
        match(p, TOK_EQ);
        n->data.let.value = expression(p);
        match(p, TOK_SEMICOLON);
        return n;
    }
    
    if (match(p, TOK_FOR)) {
        ASTNode *n = ast_new(NODE_FOR, t->line, t->col);
        n->data.for_loop.parallel = is_parallel;
        n->data.for_loop.var = copy_token_str(current(p));
        advance(p);
        match(p, TOK_IN);
        n->data.for_loop.start = expression(p);
        match(p, TOK_DOTDOT);
        n->data.for_loop.end = expression(p);
        match(p, TOK_LBRACE);
        n->data.for_loop.body = block(p);
        match(p, TOK_SEMICOLON);
        return n;
    }
    
    if (match(p, TOK_LBRACE)) {
        return block(p);
    }
    
    ASTNode *expr = expression(p);
    match(p, TOK_SEMICOLON);
    return expr;
}

void parser_init(Parser *p, TokenList *tokens) {
    p->tokens = tokens;
    p->current = 0;
}

ASTNode *parser_parse(Parser *p) {
    Token *t = current(p);
    ASTNode *program = ast_new(NODE_PROGRAM, t->line, t->col);
    size_t cap = 32;
    program->data.block.stmts = malloc(sizeof(ASTNode*) * cap);
    program->data.block.count = 0;
    
    while (!is_at_end(p)) {
        if (program->data.block. count >= cap) {
            cap *= 2;
            program->data.block. stmts = realloc(program->data.block.stmts,
                                                 sizeof(ASTNode*) * cap);
        }
        program->data.block.stmts[program->data.block.count++] = statement(p);
    }
    
    return program;
}