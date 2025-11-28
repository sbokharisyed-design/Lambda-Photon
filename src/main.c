#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "optimize.h"

#define VERSION "0.2.0-alpha"

typedef struct {
    char *input_file;
    char *output_file;
    int emit_llvm;
    int optimize;
    char *target;
} Options;

static void print_usage(const char *prog) {
    fprintf(stderr, "Lambda Photon %s\n", VERSION);
    fprintf(stderr, "Usage: %s <input.lp> [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o <file>       Output file\n");
    fprintf(stderr, "  --emit-llvm     Output LLVM IR only\n");
    fprintf(stderr, "  -O<n>           Optimization level (0-3)\n");
    fprintf(stderr, "  --version       Show version\n");
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (! f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

static Options parse_args(int argc, char **argv) {
    Options opts = {0};
    opts.optimize = 2;
    opts.output_file = "a.out";
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                opts.output_file = argv[++i];
            } else if (strcmp(argv[i], "--emit-llvm") == 0) {
                opts.emit_llvm = 1;
            } else if (strncmp(argv[i], "-O", 2) == 0) {
                opts.optimize = argv[i][2] - '0';
            } else if (strcmp(argv[i], "--version") == 0) {
                printf("Lambda Photon %s\n", VERSION);
                exit(0);
            } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                print_usage(argv[0]);
                exit(0);
            }
        } else {
            opts.input_file = argv[i];
        }
    }
    return opts;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    Options opts = parse_args(argc, argv);
    
    if (! opts.input_file) {
        fprintf(stderr, "E: no input\n");
        return 1;
    }
    
    /* Read source file */
    char *source = read_file(opts.input_file);
    if (!source) {
        fprintf(stderr, "E: cannot read '%s'\n", opts.input_file);
        return 1;
    }
    
    /* Lexical analysis */
    Lexer lexer;
    lexer_init(&lexer, source);
    TokenList *tokens = lexer_tokenize(&lexer);
    if (!tokens || tokens->tokens[tokens->count-1].type == TOK_ERROR) {
        fprintf(stderr, "E: lex\n");
        return 1;
    }
    
    /* Parsing */
    Parser parser;
    parser_init(&parser, tokens);
    ASTNode *ast = parser_parse(&parser);
    if (!ast) {
        fprintf(stderr, "E: parse\n");
        return 1;
    }
    
    /* Optimization - compile-time evaluation */
    ast = optimize(ast);
    
    /* Code generation */
    CodeGen cg;
    codegen_init(&cg, opts.target, opts.optimize);
    char *llvm_ir = codegen_emit(&cg, ast);
    
    if (opts.emit_llvm) {
        /* Output LLVM IR */
        if (strcmp(opts.output_file, "a.out") == 0) {
            printf("%s", llvm_ir);
        } else {
            FILE *out = fopen(opts.output_file, "w");
            if (out) {
                fprintf(out, "%s", llvm_ir);
                fclose(out);
            }
        }
    } else {
        /* Compile to native executable */
        if (codegen_compile(&cg, opts.output_file) != 0) {
            fprintf(stderr, "E: compile\n");
            return 1;
        }
    }
    
    /* Cleanup */
    LLVMDisposeMessage(llvm_ir);
    free(source);
    token_list_free(tokens);
    ast_free(ast);
    codegen_cleanup(&cg);
    
    return 0;
}