#ifndef LP_CODEGEN_H
#define LP_CODEGEN_H

#include "ast.h"
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/PassBuilder.h>

typedef struct {
    char *name;
    LLVMValueRef value;
    LLVMTypeRef type;
    struct Symbol *next;
} Symbol;

typedef struct Scope {
    Symbol *symbols;
    struct Scope *parent;
} Scope;

typedef struct {
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMTargetMachineRef target_machine;
    Scope *current_scope;
    int opt_level;
} CodeGen;

void codegen_init(CodeGen *cg, const char *target_triple, int opt_level);
char *codegen_emit(CodeGen *cg, ASTNode *ast);
int codegen_compile(CodeGen *cg, const char *output_file);
void codegen_cleanup(CodeGen *cg);

#endif