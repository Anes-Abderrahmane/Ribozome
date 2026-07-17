#ifndef RIBOZOME_CODEGEN_H
#define RIBOZOME_CODEGEN_H

#include "parser.h"

/* Generate an output.rna file from the AST. Returns 0 on success, non-zero on failure. */
int generate_rna(ASTNode *root);

#endif /* RIBOZOME_CODEGEN_H */
