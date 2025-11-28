#include "ast.h"
#include <stdlib.h>
#include <string.h>

ASTNode *ast_new(NodeType type, uint32_t line, uint32_t col) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->col = col;
    return node;
}

void ast_free(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_STRING_LIT:
            free(node->data.string.value);
            break;
        case NODE_IDENT:
            free(node->data.ident.name);
            break;
        case NODE_BINARY:
            ast_free(node->data.binary.left);
            ast_free(node->data.binary.right);
            break;
        case NODE_UNARY:
            ast_free(node->data.unary.operand);
            break;
        case NODE_LAMBDA:
            for (size_t i = 0; i < node->data.lambda. param_count; i++) {
                free(node->data. lambda.params[i]);
                if (node->data. lambda.param_types)
                    type_free(node->data.lambda.param_types[i]);
            }
            free(node->data.lambda.params);
            free(node->data.lambda. param_types);
            ast_free(node->data.lambda.body);
            break;
        case NODE_APPLY:
            ast_free(node->data.apply. func);
            for (size_t i = 0; i < node->data. apply.arg_count; i++)
                ast_free(node->data. apply.args[i]);
            free(node->data.apply.args);
            break;
        case NODE_TERNARY:
            ast_free(node->data.ternary.cond);
            ast_free(node->data. ternary.then_branch);
            ast_free(node->data.ternary.else_branch);
            break;
        case NODE_LET:
            free(node->data. let.name);
            type_free(node->data.let.type_annotation);
            ast_free(node->data.let. value);
            break;
        case NODE_FOR:
            free(node->data. for_loop.var);
            ast_free(node->data.for_loop. start);
            ast_free(node->data.for_loop.end);
            ast_free(node->data.for_loop.body);
            break;
        case NODE_BLOCK:
        case NODE_PROGRAM:
            for (size_t i = 0; i < node->data.block.count; i++)
                ast_free(node->data. block.stmts[i]);
            free(node->data.block.stmts);
            break;
        case NODE_ASYNC:
        case NODE_AWAIT:
            ast_free(node->data.async_expr. expr);
            break;
        case NODE_ARRAY:
            for (size_t i = 0; i < node->data. array.count; i++)
                ast_free(node->data.array.elements[i]);
            free(node->data. array.elements);
            break;
        case NODE_INDEX:
            ast_free(node->data. index.array);
            ast_free(node->data.index.index);
            break;
        case NODE_BUILTIN:
            free(node->data.builtin.name);
            for (size_t i = 0; i < node->data.builtin. count; i++)
                ast_free(node->data.builtin. elements[i]);
            free(node->data.builtin.elements);
            break;
        case NODE_GPU_KERNEL:
            free(node->data.gpu_kernel.name);
            for (size_t i = 0; i < node->data.gpu_kernel.param_count; i++) {
                free(node->data.gpu_kernel.params[i]);
                type_free(node->data.gpu_kernel.param_types[i]);
            }
            free(node->data.gpu_kernel.params);
            free(node->data.gpu_kernel.param_types);
            ast_free(node->data. gpu_kernel.body);
            break;
        default:
            break;
    }
    
    type_free(node->resolved_type);
    free(node);
}

Type *type_new(int kind) {  
    Type *t = calloc(1, sizeof(Type));
    t->kind = kind;
    return t;
}

void type_free(Type *t) {
    if (!t) return;
    type_free(t->inner);
    type_free(t->ret);
    for (size_t i = 0; i < t->param_count; i++)
        type_free(t->params[i]);
    free(t->params);
    free(t);
}

Type *type_clone(Type *t) {
    if (!t) return NULL;
    Type *c = type_new(t->kind);
    c->inner = type_clone(t->inner);
    c->ret = type_clone(t->ret);
    c->array_len = t->array_len;
    c->param_count = t->param_count;
    if (t->param_count) {
        c->params = malloc(sizeof(Type*) * t->param_count);
        for (size_t i = 0; i < t->param_count; i++)
            c->params[i] = type_clone(t->params[i]);
    }
    return c;
}