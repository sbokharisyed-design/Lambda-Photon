#include "optimize.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Check if a node is a constant literal */
int is_constant_expr(ASTNode *node) {
    if (!node) return 0;
    
    switch (node->type) {
        case NODE_INT_LIT:
        case NODE_FLOAT_LIT:
            return 1;
            
        case NODE_BINARY:
            return is_constant_expr(node->data.binary.left) &&
                   is_constant_expr(node->data.binary.right);
                   
        case NODE_UNARY:
            return is_constant_expr(node->data.unary.operand);
            
        case NODE_TERNARY:
            return is_constant_expr(node->data.ternary.cond) &&
                   is_constant_expr(node->data.ternary.then_branch) &&
                   is_constant_expr(node->data.ternary.else_branch);
                   
        default:
            return 0;
    }
}

/* Helper: check if any operand is float */
static int has_float_operand(ASTNode *left, ASTNode *right) {
    return (left->type == NODE_FLOAT_LIT) || (right->type == NODE_FLOAT_LIT);
}

/* Get numeric value as double */
static double get_numeric_value(ASTNode *node) {
    if (node->type == NODE_FLOAT_LIT) return node->data.float_val;
    if (node->type == NODE_INT_LIT) return (double)node->data.int_val;
    return 0.0;
}

/* Evaluate a constant binary expression */
static ASTNode *eval_binary(ASTNode *node) {
    ASTNode *left = eval_constant(node->data.binary.left);
    ASTNode *right = eval_constant(node->data.binary.right);
    
    if (!left || !right) {
        if (left) ast_free(left);
        if (right) ast_free(right);
        return NULL;
    }
    
    ASTNode *result = ast_new(NODE_INT_LIT, node->line, node->col);
    int use_float = has_float_operand(left, right);
    
    if (use_float) {
        result->type = NODE_FLOAT_LIT;
        double l = get_numeric_value(left);
        double r = get_numeric_value(right);
        
        switch (node->data.binary.op) {
            case OP_ADD: result->data.float_val = l + r; break;
            case OP_SUB: result->data.float_val = l - r; break;
            case OP_MUL: result->data.float_val = l * r; break;
            case OP_DIV: result->data.float_val = r != 0 ? l / r : 0; break;
            case OP_MOD: result->data.float_val = fmod(l, r); break;
            case OP_EQ:  result->type = NODE_INT_LIT; result->data.int_val = l == r; break;
            case OP_NEQ: result->type = NODE_INT_LIT; result->data.int_val = l != r; break;
            case OP_LT:  result->type = NODE_INT_LIT; result->data.int_val = l < r; break;
            case OP_GT:  result->type = NODE_INT_LIT; result->data.int_val = l > r; break;
            case OP_LTE: result->type = NODE_INT_LIT; result->data.int_val = l <= r; break;
            case OP_GTE: result->type = NODE_INT_LIT; result->data.int_val = l >= r; break;
            default:
                ast_free(result);
                ast_free(left);
                ast_free(right);
                return NULL;
        }
    } else {
        int64_t l = left->data.int_val;
        int64_t r = right->data.int_val;
        
        switch (node->data.binary.op) {
            case OP_ADD: result->data.int_val = l + r; break;
            case OP_SUB: result->data.int_val = l - r; break;
            case OP_MUL: result->data.int_val = l * r; break;
            case OP_DIV: result->data.int_val = r != 0 ? l / r : 0; break;
            case OP_MOD: result->data.int_val = r != 0 ? l % r : 0; break;
            case OP_EQ:  result->data.int_val = l == r; break;
            case OP_NEQ: result->data.int_val = l != r; break;
            case OP_LT:  result->data.int_val = l < r; break;
            case OP_GT:  result->data.int_val = l > r; break;
            case OP_LTE: result->data.int_val = l <= r; break;
            case OP_GTE: result->data.int_val = l >= r; break;
            case OP_AND: result->data.int_val = l && r; break;
            case OP_OR:  result->data.int_val = l || r; break;
            case OP_BITAND: result->data.int_val = l & r; break;
            case OP_BITOR:  result->data.int_val = l | r; break;
            case OP_BITXOR: result->data.int_val = l ^ r; break;
            case OP_SHL: result->data.int_val = l << r; break;
            case OP_SHR: result->data.int_val = l >> r; break;
            default:
                ast_free(result);
                ast_free(left);
                ast_free(right);
                return NULL;
        }
    }
    
    ast_free(left);
    ast_free(right);
    return result;
}

/* Evaluate a constant unary expression */
static ASTNode *eval_unary(ASTNode *node) {
    ASTNode *operand = eval_constant(node->data.unary.operand);
    if (!operand) return NULL;
    
    ASTNode *result = ast_new(operand->type, node->line, node->col);
    
    switch (node->data.unary.op) {
        case OP_NEG:
            if (operand->type == NODE_FLOAT_LIT) {
                result->data.float_val = -operand->data.float_val;
            } else {
                result->data.int_val = -operand->data.int_val;
            }
            break;
        case OP_NOT:
            result->type = NODE_INT_LIT;
            if (operand->type == NODE_FLOAT_LIT) {
                result->data.int_val = operand->data.float_val == 0.0;
            } else {
                result->data.int_val = !operand->data.int_val;
            }
            break;
        default:
            ast_free(result);
            ast_free(operand);
            return NULL;
    }
    
    ast_free(operand);
    return result;
}

/* Evaluate a constant ternary expression */
static ASTNode *eval_ternary(ASTNode *node) {
    ASTNode *cond = eval_constant(node->data.ternary.cond);
    if (!cond) return NULL;
    
    int cond_true;
    if (cond->type == NODE_FLOAT_LIT) {
        cond_true = cond->data.float_val != 0.0;
    } else {
        cond_true = cond->data.int_val != 0;
    }
    ast_free(cond);
    
    if (cond_true) {
        return eval_constant(node->data.ternary.then_branch);
    } else {
        return eval_constant(node->data.ternary.else_branch);
    }
}

/* Main constant evaluation function */
ASTNode *eval_constant(ASTNode *node) {
    if (!node) return NULL;
    
    switch (node->type) {
        case NODE_INT_LIT: {
            ASTNode *copy = ast_new(NODE_INT_LIT, node->line, node->col);
            copy->data.int_val = node->data.int_val;
            return copy;
        }
        case NODE_FLOAT_LIT: {
            ASTNode *copy = ast_new(NODE_FLOAT_LIT, node->line, node->col);
            copy->data.float_val = node->data.float_val;
            return copy;
        }
        case NODE_BINARY:
            return eval_binary(node);
        case NODE_UNARY:
            return eval_unary(node);
        case NODE_TERNARY:
            return eval_ternary(node);
        default:
            return NULL;
    }
}

/* Optimize a single node by constant folding */
ASTNode *optimize_const_fold(ASTNode *node) {
    if (!node) return NULL;
    
    switch (node->type) {
        case NODE_BINARY: {
            /* First optimize children */
            node->data.binary.left = optimize_const_fold(node->data.binary.left);
            node->data.binary.right = optimize_const_fold(node->data.binary.right);
            
            /* Then try to fold this node */
            if (is_constant_expr(node)) {
                ASTNode *folded = eval_constant(node);
                if (folded) {
                    ast_free(node);
                    return folded;
                }
            }
            return node;
        }
        
        case NODE_UNARY: {
            node->data.unary.operand = optimize_const_fold(node->data.unary.operand);
            
            if (is_constant_expr(node)) {
                ASTNode *folded = eval_constant(node);
                if (folded) {
                    ast_free(node);
                    return folded;
                }
            }
            return node;
        }
        
        case NODE_TERNARY: {
            node->data.ternary.cond = optimize_const_fold(node->data.ternary.cond);
            node->data.ternary.then_branch = optimize_const_fold(node->data.ternary.then_branch);
            node->data.ternary.else_branch = optimize_const_fold(node->data.ternary.else_branch);
            
            /* If condition is constant, eliminate the branch */
            if (is_constant_expr(node->data.ternary.cond)) {
                ASTNode *cond = eval_constant(node->data.ternary.cond);
                if (cond) {
                    int cond_true;
                    if (cond->type == NODE_FLOAT_LIT) {
                        cond_true = cond->data.float_val != 0.0;
                    } else {
                        cond_true = cond->data.int_val != 0;
                    }
                    ast_free(cond);
                    
                    ASTNode *result;
                    if (cond_true) {
                        result = node->data.ternary.then_branch;
                        node->data.ternary.then_branch = NULL;
                    } else {
                        result = node->data.ternary.else_branch;
                        node->data.ternary.else_branch = NULL;
                    }
                    ast_free(node);
                    return result;
                }
            }
            return node;
        }
        
        case NODE_LET: {
            node->data.let.value = optimize_const_fold(node->data.let.value);
            return node;
        }
        
        case NODE_FOR: {
            node->data.for_loop.start = optimize_const_fold(node->data.for_loop.start);
            node->data.for_loop.end = optimize_const_fold(node->data.for_loop.end);
            node->data.for_loop.body = optimize_const_fold(node->data.for_loop.body);
            return node;
        }
        
        case NODE_BLOCK: {
            for (size_t i = 0; i < node->data.block.count; i++) {
                node->data.block.stmts[i] = optimize_const_fold(node->data.block.stmts[i]);
            }
            return node;
        }
        
        case NODE_PROGRAM: {
            for (size_t i = 0; i < node->data.block.count; i++) {
                node->data.block.stmts[i] = optimize_const_fold(node->data.block.stmts[i]);
            }
            return node;
        }
        
        case NODE_BUILTIN: {
            for (size_t i = 0; i < node->data.builtin.count; i++) {
                node->data.builtin.elements[i] = optimize_const_fold(node->data.builtin.elements[i]);
            }
            return node;
        }
        
        default:
            return node;
    }
}

/* Run all optimization passes */
ASTNode *optimize(ASTNode *ast) {
    if (!ast) return NULL;
    
    /* Pass 1: Constant folding */
    ast = optimize_const_fold(ast);
    
    return ast;
}
