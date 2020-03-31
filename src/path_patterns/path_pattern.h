#pragma once

#include "ebnf.h"
#include "../arithmetic/algebraic_expression.h"

typedef struct {
    const char *name;
    EBNFBase *ebnf_root;
    AlgebraicExpression *ae;
    GrB_Matrix m;
} PathPattern;

/* Create new PathPattern with NULL algebraic expression. */
PathPattern *PathPattern_New(const char *name, EBNFBase *ebnf, size_t reqiured_mdim);

char * PathPattern_ToString(PathPattern *pattern);