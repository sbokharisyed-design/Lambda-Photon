#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/PassBuilder.h>

/* ========== Scope Management ========== */

static Scope *scope_new(Scope *parent) {
    Scope *s = calloc(1, sizeof(Scope));
    s->parent = parent;
    return s;
}

static void scope_free(Scope *s) {
    Symbol *sym = s->symbols;
    while (sym) {
        Symbol *next = sym->next;
        free(sym->name);
        free(sym);
        sym = next;
    }
    free(s);
}

static void scope_define(Scope *s, const char *name, LLVMValueRef value, LLVMTypeRef type) {
    Symbol *sym = malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->value = value;
    sym->type = type;
    sym->next = s->symbols;
    s->symbols = sym;
}

static LLVMValueRef scope_lookup(Scope *s, const char *name) {
    for (Scope *scope = s; scope; scope = scope->parent) {
        for (Symbol *sym = scope->symbols; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                return sym->value;
            }
        }
    }
    return NULL;
}

static LLVMTypeRef scope_lookup_type(Scope *s, const char *name) {
    for (Scope *scope = s; scope; scope = scope->parent) {
        for (Symbol *sym = scope->symbols; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                return sym->type;
            }
        }
    }
    return NULL;
}

/* ========== Forward Declarations ========== */

static LLVMValueRef codegen_expr(CodeGen *cg, ASTNode *node);
static void codegen_stmt(CodeGen *cg, ASTNode *node);

/* ========== Type Mapping ========== */

static LLVMTypeRef get_llvm_type(CodeGen *cg, Type *t) {
    if (!t) return LLVMInt64TypeInContext(cg->context);
    
    switch (t->kind) {
        case TYPE_VOID:  return LLVMVoidTypeInContext(cg->context);
        case TYPE_I8:    return LLVMInt8TypeInContext(cg->context);
        case TYPE_I16:   return LLVMInt16TypeInContext(cg->context);
        case TYPE_I32:   return LLVMInt32TypeInContext(cg->context);
        case TYPE_I64:   return LLVMInt64TypeInContext(cg->context);
        case TYPE_U8:    return LLVMInt8TypeInContext(cg->context);
        case TYPE_U16:   return LLVMInt16TypeInContext(cg->context);
        case TYPE_U32:   return LLVMInt32TypeInContext(cg->context);
        case TYPE_U64:   return LLVMInt64TypeInContext(cg->context);
        case TYPE_F32:   return LLVMFloatTypeInContext(cg->context);
        case TYPE_F64:   return LLVMDoubleTypeInContext(cg->context);
        case TYPE_STR:   return LLVMPointerTypeInContext(cg->context, 0);
        case TYPE_PTR:   return LLVMPointerTypeInContext(cg->context, 0);
        default:         return LLVMInt64TypeInContext(cg->context);
    }
}

/* ========== Builtin Functions ========== */

static LLVMValueRef get_printf(CodeGen *cg) {
    LLVMValueRef func = LLVMGetNamedFunction(cg->module, "printf");
    if (! func) {
        LLVMTypeRef param_types[] = { LLVMPointerTypeInContext(cg->context, 0) };
        LLVMTypeRef printf_type = LLVMFunctionType(
            LLVMInt32TypeInContext(cg->context),
            param_types, 1, 1
        );
        func = LLVMAddFunction(cg->module, "printf", printf_type);
    }
    return func;
}

static LLVMValueRef codegen_builtin(CodeGen *cg, ASTNode *node) {
    if (strcmp(node->data.builtin.name, "print") == 0) {
        LLVMValueRef printf_fn = get_printf(cg);
        
        if (node->data.builtin.count == 0) return NULL;
        
        LLVMValueRef val = codegen_expr(cg, node->data.builtin.elements[0]);
        if (!val) return NULL;
        
        LLVMTypeRef val_type = LLVMTypeOf(val);
        LLVMTypeKind kind = LLVMGetTypeKind(val_type);
        
        LLVMValueRef format_str;
        LLVMValueRef args[2];
        
        if (kind == LLVMFloatTypeKind) {
            /* Promote float to double for printf */
            val = LLVMBuildFPExt(cg->builder, val, LLVMDoubleTypeInContext(cg->context), "ftod");
            format_str = LLVMBuildGlobalStringPtr(cg->builder, "%f\n", "fmt_float");
            args[0] = format_str;
            args[1] = val;
        } else if (kind == LLVMDoubleTypeKind) {
            format_str = LLVMBuildGlobalStringPtr(cg->builder, "%f\n", "fmt_float");
            args[0] = format_str;
            args[1] = val;
        } else if (kind == LLVMPointerTypeKind) {
            format_str = LLVMBuildGlobalStringPtr(cg->builder, "%s\n", "fmt_str");
            args[0] = format_str;
            args[1] = val;
        } else if (kind == LLVMIntegerTypeKind) {
            /* Extend smaller integers to 64-bit for printf */
            unsigned bits = LLVMGetIntTypeWidth(val_type);
            if (bits < 64) {
                val = LLVMBuildSExt(cg->builder, val, LLVMInt64TypeInContext(cg->context), "ext");
            }
            format_str = LLVMBuildGlobalStringPtr(cg->builder, "%lld\n", "fmt_int");
            args[0] = format_str;
            args[1] = val;
        } else {
            format_str = LLVMBuildGlobalStringPtr(cg->builder, "%lld\n", "fmt_int");
            args[0] = format_str;
            args[1] = val;
        }
        
        LLVMTypeRef printf_type = LLVMFunctionType(
            LLVMInt32TypeInContext(cg->context),
            (LLVMTypeRef[]){ LLVMPointerTypeInContext(cg->context, 0) }, 1, 1
        );
        
        return LLVMBuildCall2(cg->builder, printf_type, printf_fn, args, 2, "");
    }
    
    return NULL;
}

/* ========== Expression Codegen ========== */

static LLVMValueRef codegen_binary(CodeGen *cg, ASTNode *node) {
    LLVMValueRef left = codegen_expr(cg, node->data.binary.left);
    LLVMValueRef right = codegen_expr(cg, node->data.binary.right);
    
    if (!left || !right) return NULL;
    
    LLVMTypeRef left_type = LLVMTypeOf(left);
    LLVMTypeRef right_type = LLVMTypeOf(right);
    LLVMTypeKind left_kind = LLVMGetTypeKind(left_type);
    LLVMTypeKind right_kind = LLVMGetTypeKind(right_type);
    
    int left_is_float = (left_kind == LLVMFloatTypeKind || left_kind == LLVMDoubleTypeKind);
    int right_is_float = (right_kind == LLVMFloatTypeKind || right_kind == LLVMDoubleTypeKind);
    int is_float = left_is_float || right_is_float;
    
    /* Type coercion: promote int to float if needed */
    if (is_float) {
        LLVMTypeRef double_type = LLVMDoubleTypeInContext(cg->context);
        if (! left_is_float) {
            left = LLVMBuildSIToFP(cg->builder, left, double_type, "int_to_float");
        }
        if (!right_is_float) {
            right = LLVMBuildSIToFP(cg->builder, right, double_type, "int_to_float");
        }
    }
    
    switch (node->data.binary.op) {
        case OP_ADD:
            return is_float ? LLVMBuildFAdd(cg->builder, left, right, "fadd")
                           : LLVMBuildAdd(cg->builder, left, right, "add");
        case OP_SUB:
            return is_float ? LLVMBuildFSub(cg->builder, left, right, "fsub")
                           : LLVMBuildSub(cg->builder, left, right, "sub");
        case OP_MUL:
            return is_float ? LLVMBuildFMul(cg->builder, left, right, "fmul")
                           : LLVMBuildMul(cg->builder, left, right, "mul");
        case OP_DIV:
            return is_float ? LLVMBuildFDiv(cg->builder, left, right, "fdiv")
                           : LLVMBuildSDiv(cg->builder, left, right, "sdiv");
        case OP_MOD:
            return is_float ? LLVMBuildFRem(cg->builder, left, right, "fmod")
                           : LLVMBuildSRem(cg->builder, left, right, "mod");
        
        /* Comparisons - native CPU ops, NOT Church encoding (faster) */
        case OP_EQ:
            if (is_float) {
                LLVMValueRef cmp = LLVMBuildFCmp(cg->builder, LLVMRealOEQ, left, right, "fcmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "eq");
            } else {
                LLVMValueRef cmp = LLVMBuildICmp(cg->builder, LLVMIntEQ, left, right, "icmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "eq");
            }
        case OP_NEQ:
            if (is_float) {
                LLVMValueRef cmp = LLVMBuildFCmp(cg->builder, LLVMRealONE, left, right, "fcmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "neq");
            } else {
                LLVMValueRef cmp = LLVMBuildICmp(cg->builder, LLVMIntNE, left, right, "icmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "neq");
            }
        case OP_LT:
            if (is_float) {
                LLVMValueRef cmp = LLVMBuildFCmp(cg->builder, LLVMRealOLT, left, right, "fcmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "lt");
            } else {
                LLVMValueRef cmp = LLVMBuildICmp(cg->builder, LLVMIntSLT, left, right, "icmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "lt");
            }
        case OP_GT:
            if (is_float) {
                LLVMValueRef cmp = LLVMBuildFCmp(cg->builder, LLVMRealOGT, left, right, "fcmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "gt");
            } else {
                LLVMValueRef cmp = LLVMBuildICmp(cg->builder, LLVMIntSGT, left, right, "icmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "gt");
            }
        case OP_LTE:
            if (is_float) {
                LLVMValueRef cmp = LLVMBuildFCmp(cg->builder, LLVMRealOLE, left, right, "fcmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "lte");
            } else {
                LLVMValueRef cmp = LLVMBuildICmp(cg->builder, LLVMIntSLE, left, right, "icmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "lte");
            }
        case OP_GTE:
            if (is_float) {
                LLVMValueRef cmp = LLVMBuildFCmp(cg->builder, LLVMRealOGE, left, right, "fcmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "gte");
            } else {
                LLVMValueRef cmp = LLVMBuildICmp(cg->builder, LLVMIntSGE, left, right, "icmp");
                return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "gte");
            }
        
        /* Bitwise - direct hardware instructions */
        case OP_BITAND:
            return LLVMBuildAnd(cg->builder, left, right, "bitand");
        case OP_BITOR:
            return LLVMBuildOr(cg->builder, left, right, "bitor");
        case OP_BITXOR:
            return LLVMBuildXor(cg->builder, left, right, "bitxor");
        case OP_SHL:
            return LLVMBuildShl(cg->builder, left, right, "shl");
        case OP_SHR:
            return LLVMBuildAShr(cg->builder, left, right, "shr");
        
        /* Logical */
        case OP_AND: {
            LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(cg->context), 0, 0);
            LLVMValueRef lb = LLVMBuildICmp(cg->builder, LLVMIntNE, left, zero, "");
            LLVMValueRef rb = LLVMBuildICmp(cg->builder, LLVMIntNE, right, zero, "");
            LLVMValueRef res = LLVMBuildAnd(cg->builder, lb, rb, "");
            return LLVMBuildZExt(cg->builder, res, LLVMInt64TypeInContext(cg->context), "and");
        }
        case OP_OR: {
            LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(cg->context), 0, 0);
            LLVMValueRef lb = LLVMBuildICmp(cg->builder, LLVMIntNE, left, zero, "");
            LLVMValueRef rb = LLVMBuildICmp(cg->builder, LLVMIntNE, right, zero, "");
            LLVMValueRef res = LLVMBuildOr(cg->builder, lb, rb, "");
            return LLVMBuildZExt(cg->builder, res, LLVMInt64TypeInContext(cg->context), "or");
        }
        
        default:
            return NULL;
    }
}

static LLVMValueRef codegen_unary(CodeGen *cg, ASTNode *node) {
    LLVMValueRef operand = codegen_expr(cg, node->data.unary.operand);
    if (!operand) return NULL;
    
    LLVMTypeKind kind = LLVMGetTypeKind(LLVMTypeOf(operand));
    int is_float = (kind == LLVMFloatTypeKind || kind == LLVMDoubleTypeKind);
    
    switch (node->data.unary.op) {
        case OP_NEG:
            return is_float ? LLVMBuildFNeg(cg->builder, operand, "fneg")
                           : LLVMBuildNeg(cg->builder, operand, "neg");
        case OP_NOT: {
            LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(cg->context), 0, 0);
            LLVMValueRef cmp = LLVMBuildICmp(cg->builder, LLVMIntEQ, operand, zero, "");
            return LLVMBuildZExt(cg->builder, cmp, LLVMInt64TypeInContext(cg->context), "not");
        }
        default:
            return NULL;
    }
}

static LLVMValueRef codegen_ternary(CodeGen *cg, ASTNode *node) {
    LLVMValueRef cond = codegen_expr(cg, node->data.ternary.cond);
    if (!cond) return NULL;
    
    LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(cg->context), 0, 0);
    LLVMValueRef cond_bool = LLVMBuildICmp(cg->builder, LLVMIntNE, cond, zero, "cond");
    
    LLVMValueRef then_val = codegen_expr(cg, node->data.ternary.then_branch);
    LLVMValueRef else_val = codegen_expr(cg, node->data.ternary.else_branch);
    
    if (!then_val || !else_val) return NULL;
    
    /* Use select - faster than branch for simple ternary */
    return LLVMBuildSelect(cg->builder, cond_bool, then_val, else_val, "ternary");
}

static LLVMValueRef codegen_expr(CodeGen *cg, ASTNode *node) {
    if (! node) return NULL;
    
    switch (node->type) {
        case NODE_INT_LIT:
            return LLVMConstInt(LLVMInt64TypeInContext(cg->context), 
                               node->data.int_val, 1);
        
        case NODE_FLOAT_LIT:
            return LLVMConstReal(LLVMDoubleTypeInContext(cg->context),
                                node->data.float_val);
        
        case NODE_STRING_LIT:
            return LLVMBuildGlobalStringPtr(cg->builder, 
                                            node->data.string.value, "str");
        
        case NODE_IDENT: {
            LLVMValueRef val = scope_lookup(cg->current_scope, node->data.ident.name);
            LLVMTypeRef type = scope_lookup_type(cg->current_scope, node->data.ident.name);
            if (!val) return NULL;
            if (LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind && type) {
                return LLVMBuildLoad2(cg->builder, type, val, node->data.ident.name);
            }
            return val;
        }
        
        case NODE_BINARY:
            return codegen_binary(cg, node);
        
        case NODE_UNARY:
            return codegen_unary(cg, node);
        
        case NODE_TERNARY:
            return codegen_ternary(cg, node);
        
        case NODE_BUILTIN:
            return codegen_builtin(cg, node);
        
        default:
            return NULL;
    }
}

/* ========== Statement Codegen ========== */

static void codegen_let(CodeGen *cg, ASTNode *node) {
    LLVMValueRef init = codegen_expr(cg, node->data.let.value);
    if (!init) return;
    
    LLVMTypeRef type;
    
    /* Use explicit type annotation if provided */
    if (node->data.let.type_annotation) {
        type = get_llvm_type(cg, node->data.let.type_annotation);
        
        /* Convert init value to target type if needed */
        LLVMTypeRef init_type = LLVMTypeOf(init);
        LLVMTypeKind init_kind = LLVMGetTypeKind(init_type);
        LLVMTypeKind target_kind = LLVMGetTypeKind(type);
        
        int init_is_float = (init_kind == LLVMFloatTypeKind || init_kind == LLVMDoubleTypeKind);
        int target_is_float = (target_kind == LLVMFloatTypeKind || target_kind == LLVMDoubleTypeKind);
        
        if (init_is_float && !target_is_float) {
            /* Float to int conversion */
            init = LLVMBuildFPToSI(cg->builder, init, type, "ftoi");
        } else if (!init_is_float && target_is_float) {
            /* Int to float conversion */
            init = LLVMBuildSIToFP(cg->builder, init, type, "itof");
        } else if (init_is_float && target_is_float && init_type != type) {
            /* Float to float conversion (f32 <-> f64) */
            init = LLVMBuildFPCast(cg->builder, init, type, "fcast");
        } else if (init_kind == LLVMIntegerTypeKind && target_kind == LLVMIntegerTypeKind) {
            /* Int to int conversion (truncate or extend) */
            unsigned init_bits = LLVMGetIntTypeWidth(init_type);
            unsigned target_bits = LLVMGetIntTypeWidth(type);
            if (init_bits > target_bits) {
                init = LLVMBuildTrunc(cg->builder, init, type, "trunc");
            } else if (init_bits < target_bits) {
                init = LLVMBuildSExt(cg->builder, init, type, "sext");
            }
        }
    } else {
        /* Infer type from init value */
        type = LLVMTypeOf(init);
    }
    
    LLVMValueRef alloca = LLVMBuildAlloca(cg->builder, type, node->data.let.name);
    LLVMBuildStore(cg->builder, init, alloca);
    
    scope_define(cg->current_scope, node->data.let.name, alloca, type);
}

/* Helper to get or create the parallel runtime functions */
static LLVMValueRef get_parallel_for_func(CodeGen *cg) {
    LLVMValueRef func = LLVMGetNamedFunction(cg->module, "__lp_parallel_for");
    if (func) return func;
    
    /* void __lp_parallel_for(i64 start, i64 end, void (*body)(i64, void*), void* ctx) */
    LLVMTypeRef i64 = LLVMInt64TypeInContext(cg->context);
    LLVMTypeRef ptr = LLVMPointerTypeInContext(cg->context, 0);
    LLVMTypeRef void_type = LLVMVoidTypeInContext(cg->context);
    
    LLVMTypeRef params[] = { i64, i64, ptr, ptr };
    LLVMTypeRef func_type = LLVMFunctionType(void_type, params, 4, 0);
    
    return LLVMAddFunction(cg->module, "__lp_parallel_for", func_type);
}

static void codegen_for(CodeGen *cg, ASTNode *node) {
    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(cg->builder));
    
    LLVMValueRef start = codegen_expr(cg, node->data.for_loop.start);
    LLVMValueRef end = codegen_expr(cg, node->data.for_loop.end);
    if (!start || !end) return;
    
    LLVMTypeRef i64 = LLVMInt64TypeInContext(cg->context);
    LLVMValueRef loop_var = LLVMBuildAlloca(cg->builder, i64, node->data.for_loop.var);
    LLVMBuildStore(cg->builder, start, loop_var);
    
    LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(cg->context, func, "loop");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(cg->context, func, "body");
    LLVMBasicBlockRef after_bb = LLVMAppendBasicBlockInContext(cg->context, func, "after");
    
    LLVMBuildBr(cg->builder, loop_bb);
    
    /* Loop condition */
    LLVMPositionBuilderAtEnd(cg->builder, loop_bb);
    LLVMValueRef cur = LLVMBuildLoad2(cg->builder, i64, loop_var, "i");
    LLVMValueRef cond = LLVMBuildICmp(cg->builder, LLVMIntSLT, cur, end, "loopcond");
    LLVMValueRef br = LLVMBuildCondBr(cg->builder, cond, body_bb, after_bb);
    
    /* Add parallel loop metadata for vectorization hints */
    if (node->data.for_loop.parallel) {
        LLVMContextRef ctx = cg->context;
        
        /* Create loop metadata */
        LLVMMetadataRef loop_id = LLVMMDNodeInContext2(ctx, NULL, 0);
        
        /* llvm.loop.parallel_accesses - hint that memory accesses are independent */
        LLVMMetadataRef parallel_str = LLVMMDStringInContext2(ctx, "llvm.loop.parallel_accesses", 27);
        LLVMMetadataRef parallel_md = LLVMMDNodeInContext2(ctx, &parallel_str, 1);
        
        /* llvm.loop.vectorize.enable = true */
        LLVMMetadataRef vec_str = LLVMMDStringInContext2(ctx, "llvm.loop.vectorize.enable", 26);
        LLVMMetadataRef true_val = LLVMValueAsMetadata(LLVMConstInt(LLVMInt1TypeInContext(ctx), 1, 0));
        LLVMMetadataRef vec_ops[] = { vec_str, true_val };
        LLVMMetadataRef vec_md = LLVMMDNodeInContext2(ctx, vec_ops, 2);
        
        /* llvm.loop.unroll.enable = true */
        LLVMMetadataRef unroll_str = LLVMMDStringInContext2(ctx, "llvm.loop.unroll.enable", 23);
        LLVMMetadataRef unroll_ops[] = { unroll_str, true_val };
        LLVMMetadataRef unroll_md = LLVMMDNodeInContext2(ctx, unroll_ops, 2);
        
        /* Combine into loop metadata */
        LLVMMetadataRef loop_ops[] = { loop_id, parallel_md, vec_md, unroll_md };
        LLVMMetadataRef loop_md = LLVMMDNodeInContext2(ctx, loop_ops, 4);
        
        /* Attach to branch instruction */
        LLVMSetMetadata(br, LLVMGetMDKindIDInContext(ctx, "llvm.loop", 9), 
                        LLVMMetadataAsValue(ctx, loop_md));
    }
    
    /* Loop body */
    LLVMPositionBuilderAtEnd(cg->builder, body_bb);
    
    Scope *loop_scope = scope_new(cg->current_scope);
    scope_define(loop_scope, node->data.for_loop.var, loop_var, i64);
    Scope *prev_scope = cg->current_scope;
    cg->current_scope = loop_scope;
    
    /* Generate body statements */
    if (node->data.for_loop.body) {
        for (size_t i = 0; i < node->data.for_loop.body->data.block.count; i++) {
            codegen_stmt(cg, node->data.for_loop.body->data.block.stmts[i]);
        }
    }
    
    cg->current_scope = prev_scope;
    scope_free(loop_scope);
    
    /* Increment */
    LLVMValueRef cur_val = LLVMBuildLoad2(cg->builder, i64, loop_var, "cur");
    LLVMValueRef next = LLVMBuildAdd(cg->builder, cur_val, 
        LLVMConstInt(i64, 1, 0), "next");
    LLVMBuildStore(cg->builder, next, loop_var);
    LLVMBuildBr(cg->builder, loop_bb);
    
    LLVMPositionBuilderAtEnd(cg->builder, after_bb);
}

static void codegen_block(CodeGen *cg, ASTNode *node) {
    Scope *block_scope = scope_new(cg->current_scope);
    Scope *prev = cg->current_scope;
    cg->current_scope = block_scope;
    
    for (size_t i = 0; i < node->data.block.count; i++) {
        codegen_stmt(cg, node->data.block.stmts[i]);
    }
    
    cg->current_scope = prev;
    scope_free(block_scope);
}

static void codegen_stmt(CodeGen *cg, ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_LET:
            codegen_let(cg, node);
            break;
        case NODE_FOR:
            codegen_for(cg, node);
            break;
        case NODE_BLOCK:
            codegen_block(cg, node);
            break;
        case NODE_BUILTIN:
            codegen_builtin(cg, node);
            break;
        default:
            codegen_expr(cg, node);
            break;
    }
}

/* ========== Public API ========== */

void codegen_init(CodeGen *cg, const char *target_triple, int opt_level) {
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
    
    cg->context = LLVMContextCreate();
    cg->module = LLVMModuleCreateWithNameInContext("lambda_photon", cg->context);
    cg->builder = LLVMCreateBuilderInContext(cg->context);
    cg->current_scope = scope_new(NULL);
    cg->opt_level = opt_level;
    
    /* Set target triple */
    char *triple = target_triple ?  strdup(target_triple) : LLVMGetDefaultTargetTriple();
    LLVMSetTarget(cg->module, triple);
    
    char *error = NULL;
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
        fprintf(stderr, "E: %s\n", error);
        LLVMDisposeMessage(error);
        free(triple);
        return;
    }
    
    cg->target_machine = LLVMCreateTargetMachine(
        target, triple, "generic", "",
        opt_level >= 3 ? LLVMCodeGenLevelAggressive :
        opt_level >= 2 ?  LLVMCodeGenLevelDefault :
        opt_level >= 1 ? LLVMCodeGenLevelLess : LLVMCodeGenLevelNone,
        LLVMRelocDefault,
        LLVMCodeModelDefault
    );
    
    free(triple);
}

char *codegen_emit(CodeGen *cg, ASTNode *ast) {
    /* Create main function */
    LLVMTypeRef main_type = LLVMFunctionType(
        LLVMInt32TypeInContext(cg->context), NULL, 0, 0);
    LLVMValueRef main_fn = LLVMAddFunction(cg->module, "main", main_type);
    
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
        cg->context, main_fn, "entry");
    LLVMPositionBuilderAtEnd(cg->builder, entry);
    
    /* Generate code for all statements */
    if (ast->type == NODE_PROGRAM) {
        for (size_t i = 0; i < ast->data.block.count; i++) {
            codegen_stmt(cg, ast->data.block.stmts[i]);
        }
    }
    
    /* Return 0 */
    LLVMBuildRet(cg->builder, 
        LLVMConstInt(LLVMInt32TypeInContext(cg->context), 0, 0));
    
    /* Verify module */
    char *error = NULL;
    if (LLVMVerifyModule(cg->module, LLVMReturnStatusAction, &error) != 0) {
        fprintf(stderr, "E: %s\n", error);
        LLVMDisposeMessage(error);
    }
    
    /* Run LLVM optimization passes */
    if (cg->opt_level > 0) {
        const char *passes;
        switch (cg->opt_level) {
            case 1:  passes = "default<O1>"; break;
            case 2:  passes = "default<O2>"; break;
            default: passes = "default<O3>"; break;
        }
        
        LLVMPassBuilderOptionsRef opts = LLVMCreatePassBuilderOptions();
        LLVMPassBuilderOptionsSetLoopVectorization(opts, 1);
        LLVMPassBuilderOptionsSetSLPVectorization(opts, 1);
        LLVMPassBuilderOptionsSetLoopUnrolling(opts, 1);
        
        LLVMRunPasses(cg->module, passes, cg->target_machine, opts);
        LLVMDisposePassBuilderOptions(opts);
    }
    
    return LLVMPrintModuleToString(cg->module);
}

int codegen_compile(CodeGen *cg, const char *output_file) {
    char *error = NULL;
    
    /* Emit object file */
    char obj_file[256];
    snprintf(obj_file, sizeof(obj_file), "%s.o", output_file);
    
    if (LLVMTargetMachineEmitToFile(cg->target_machine, cg->module, 
                                     obj_file, LLVMObjectFile, &error) != 0) {
        fprintf(stderr, "E: %s\n", error);
        LLVMDisposeMessage(error);
        return 1;
    }
    
    /* Link with system linker - pass optimization flags */
    char cmd[512];
    const char *opt_flag = cg->opt_level >= 3 ? "-O3" :
                           cg->opt_level >= 2 ? "-O2" :
                           cg->opt_level >= 1 ? "-O1" : "-O0";
    snprintf(cmd, sizeof(cmd), "clang %s \"%s\" -o \"%s\"", opt_flag, obj_file, output_file);
    int result = system(cmd);
    
    /* Cleanup obj file */
    remove(obj_file);
    
    return result;
}

void codegen_cleanup(CodeGen *cg) {
    scope_free(cg->current_scope);
    LLVMDisposeBuilder(cg->builder);
    LLVMDisposeModule(cg->module);
    LLVMContextDispose(cg->context);
    if (cg->target_machine) {
        LLVMDisposeTargetMachine(cg->target_machine);
    }
}