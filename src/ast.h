#ifndef LP_AST_H
#define LP_AST_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STRING_LIT,
    NODE_IDENT,
    NODE_BINARY,
    NODE_UNARY,
    NODE_LAMBDA,
    NODE_APPLY,
    NODE_TERNARY,
    NODE_ARRAY,
    NODE_INDEX,
    NODE_BUILTIN,
    NODE_LET,
    NODE_FOR,
    NODE_BLOCK,
    NODE_ASYNC,
    NODE_AWAIT,
    NODE_GPU_KERNEL,
    NODE_PROGRAM
} NodeType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LTE, OP_GTE,
    OP_AND, OP_OR,
    OP_BITAND, OP_BITOR, OP_BITXOR, OP_SHL, OP_SHR,
    OP_NEG, OP_NOT
} Operator;

typedef struct ASTNode ASTNode;
typedef struct Type Type;

struct Type {
    enum {
        TYPE_UNKNOWN,
        TYPE_VOID,
        TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64,
        TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64,
        TYPE_F32, TYPE_F64,
        TYPE_STR,
        TYPE_PTR,
        TYPE_ARRAY,
        TYPE_FUNC,
        TYPE_ASYNC
    } kind;
    Type *inner;
    Type **params;
    size_t param_count;
    Type *ret;
    size_t array_len;
};

struct ASTNode {
    NodeType type;
    Type *resolved_type;
    uint32_t line;
    uint32_t col;
    
    union {
        int64_t int_val;
        double float_val;
        
        struct { char *value; size_t len; } string;
        struct { char *name; size_t len; } ident;
        
        struct {
            Operator op;
            ASTNode *left;
            ASTNode *right;
        } binary;
        
        struct {
            Operator op;
            ASTNode *operand;
        } unary;
        
        struct {
            char **params;
            size_t param_count;
            Type **param_types;
            ASTNode *body;
        } lambda;
        
        struct {
            ASTNode *func;
            ASTNode **args;
            size_t arg_count;
        } apply;
        
        struct {
            ASTNode *cond;
            ASTNode *then_branch;
            ASTNode *else_branch;
        } ternary;
        
        struct {
            char *name;
            Type *type_annotation;
            ASTNode *value;
        } let;
        
        struct {
            char *var;
            ASTNode *start;
            ASTNode *end;
            ASTNode *body;
            int parallel;  /* 1 if @parallel annotation present */
        } for_loop;
        
        struct {
            ASTNode **stmts;
            size_t count;
        } block;
        
        struct {
            ASTNode *expr;
        } async_expr;
        
        struct {
            char *name;
            ASTNode **elements;
            size_t count;
        } builtin;
        
        struct {
            char *name;
            char **params;
            Type **param_types;
            size_t param_count;
            ASTNode *body;
        } gpu_kernel;
        
        struct {
            ASTNode **elements;
            size_t count;
        } array;
        
        struct {
            ASTNode *array;
            ASTNode *index;
        } index;
    } data;
};

ASTNode *ast_new(NodeType type, uint32_t line, uint32_t col);
void ast_free(ASTNode *node);
Type *type_new(int kind);
void type_free(Type *t);
Type *type_clone(Type *t);

#endif