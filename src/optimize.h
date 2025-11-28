#ifndef LP_OPTIMIZE_H
#define LP_OPTIMIZE_H

#include "ast.h"

/*
 * Constant folding - evaluate pure expressions at compile time
 * Returns a new optimized AST (or the same node if no optimization possible)
 */
ASTNode *optimize_const_fold(ASTNode *node);

/*
 * Check if an expression is a compile-time constant
 */
int is_constant_expr(ASTNode *node);

/*
 * Evaluate a constant expression at compile time
 * Returns NULL if not a constant
 */
ASTNode *eval_constant(ASTNode *node);

/*
 * Run all optimization passes on an AST
 */
ASTNode *optimize(ASTNode *ast);

#endif
