/*
* Copyright 2018-2020 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#include "utils.h"
#include "../../query_ctx.h"
#include "../algebraic_expression.h"

// Forward declarations
GrB_Matrix _AlgebraicExpression_EvalArbitrary(const AlgebraicExpression *exp, GrB_Matrix res);

static GrB_Matrix _Eval_TransposeArbitrary( const AlgebraicExpression *exp, GrB_Matrix res) {
    // In path patterns transpose operation can contain another operation
    assert(exp && AlgebraicExpression_ChildCount(exp) == 1);
    AlgebraicExpression *child = FIRST_CHILD(exp);

    if (child->type == AL_OPERATION) {
        _AlgebraicExpression_EvalArbitrary(child, res);
    }

    GrB_Info info = GrB_transpose(res, GrB_NULL, GrB_NULL, res, GrB_NULL);
    assert(info == GrB_SUCCESS);
    return res;
}

static GrB_Matrix _Eval_AddArbitrary(const AlgebraicExpression *exp, GrB_Matrix res) {
    assert(exp && AlgebraicExpression_ChildCount(exp) > 1);

    GrB_Info info;
    GrB_Index nrows;                // Number of rows of operand.
    GrB_Index ncols;                // Number of columns of operand.
    GrB_Matrix a = GrB_NULL;        // Left operand.
    GrB_Matrix b = GrB_NULL;        // Right operand.
    GrB_Matrix inter = GrB_NULL;    // Intermidate matrix.
    GrB_Descriptor desc = GrB_NULL; // Descriptor used for transposing operands.
    bool res_in_use = false;        // Can we use `res` for intermidate evaluation.

    GrB_Descriptor_new(&desc);

    // Get left and right operands.
    AlgebraicExpression *left = CHILD_AT(exp, 0);
    AlgebraicExpression *right = CHILD_AT(exp, 1);

    /* If left operand is a matrix, simply get it.
     * Otherwise evaluate left hand side using `res` to store LHS value. */
    if(left->type == AL_OPERAND) {
        a = left->operand.matrix;
    } else {
        if(left->operation.op == AL_EXP_TRANSPOSE) {
            a = _AlgebraicExpression_EvalArbitrary(left->operation.children[0], res);
            GrB_Descriptor_set(desc, GrB_INP0, GrB_TRAN);
            res_in_use = true;
        } else {
            a = _AlgebraicExpression_EvalArbitrary(left, res);
            res_in_use = true;
        }
    }

    /* If right operand is a matrix, simply get it.
     * Otherwise evaluate right hand side using `res` if free or create an additional matrix to store RHS value. */
    if (right->type == AL_OPERATION && right->operation.op == AL_EXP_TRANSPOSE) {
        GrB_Descriptor_set(desc, GrB_INP1, GrB_TRAN);
        right = right->operation.children[0];
    }

    if(right->type == AL_OPERAND) {
        b = right->operand.matrix;
    } else {
        assert(right->operation.op != AL_EXP_TRANSPOSE);
        if(res_in_use) {
            // `res` is in use, create an additional matrix.
            GrB_Matrix_nrows(&nrows, a);
            GrB_Matrix_ncols(&ncols, a);
            info = GrB_Matrix_new(&inter, GrB_BOOL, nrows, ncols);
            if(info != GrB_SUCCESS) {
                fprintf(stderr, "%s", GrB_error());
                assert(false);
            }
            b = _AlgebraicExpression_EvalArbitrary(right, inter);
        } else {
            // `res` is not used just yet, use it for RHS evaluation.
            b = _AlgebraicExpression_EvalArbitrary(right, res);
        }
    }

    // Perform addition.
    if(GrB_eWiseAdd_Matrix_Semiring(res, GrB_NULL, GrB_NULL, GxB_ANY_PAIR_BOOL, a, b,
                                    desc) != GrB_SUCCESS) {
        printf("Failed adding operands, error:%s\n", GrB_error());
        assert(false);
    }

    // Reset descriptor.
    GrB_Descriptor_set(desc, GrB_INP0, GxB_DEFAULT);

    uint child_count = AlgebraicExpression_ChildCount(exp);
    // Expression has more than 2 operands, e.g. A+B+C...
    for(uint i = 2; i < child_count; i++) {
        // Reset descriptor.
        GrB_Descriptor_set(desc, GrB_INP1, GxB_DEFAULT);
        right = CHILD_AT(exp, i);

        if(right->type == AL_OPERAND) {
            b = right->operand.matrix;
        } else {
            if(right->operation.op == AL_EXP_TRANSPOSE) {
                GrB_Descriptor_set(desc, GrB_INP1, GrB_TRAN);
            }
            if(inter == GrB_NULL) {
                // Can't use `res`, use an intermidate matrix.
                GrB_Matrix_nrows(&nrows, res);
                GrB_Matrix_ncols(&ncols, res);
                GrB_Matrix_new(&inter, GrB_BOOL, nrows, ncols);
            }
            b = _AlgebraicExpression_EvalArbitrary(right, inter);
        }

        // Perform addition.
        if(GrB_eWiseAdd_Matrix_Semiring(res, GrB_NULL, GrB_NULL, GxB_ANY_PAIR_BOOL, res, b,
                                        GrB_NULL) != GrB_SUCCESS) {
            printf("Failed adding operands, error:%s\n", GrB_error());
            assert(false);
        }
    }

    if(inter != GrB_NULL) GrB_Matrix_free(&inter);
    GrB_free(&desc);
    return res;
}

static GrB_Matrix _Eval_MulArbitrary(const AlgebraicExpression *exp, GrB_Matrix res) {
    assert(exp && AlgebraicExpression_ChildCount(exp) > 1);

    GrB_Matrix A;
    GrB_Matrix B;
    GrB_Index nrows;                // Number of rows of operand.
    GrB_Index ncols;                // Number of columns of operand.
    GrB_Matrix inter = GrB_NULL;    // Intermidate matrix.

    GrB_Info info;
    GrB_Index nvals;
    GrB_Descriptor desc;
    AlgebraicExpression *left = CHILD_AT(exp, 0);
    AlgebraicExpression *right = CHILD_AT(exp, 1);
    bool res_in_use = false;

    GrB_Descriptor_new(&desc);  // Descriptor used for transposing operands.

    if(left->type == AL_OPERAND) {
        A = left->operand.matrix;
    } else {
        if(left->operation.op == AL_EXP_TRANSPOSE) {
            A = _AlgebraicExpression_EvalArbitrary(left->operation.children[0], res);
            GrB_Descriptor_set(desc, GrB_INP0, GrB_TRAN);
            res_in_use = true;
        } else {
            A = _AlgebraicExpression_EvalArbitrary(left, res);
            res_in_use = true;
        }
    }

//    if(left->type == AL_OPERATION) {
//		assert(left->operation.op == AL_EXP_TRANSPOSE);
//		GrB_Descriptor_set(desc, GrB_INP0, GrB_TRAN);
//		left = CHILD_AT(left, 0);
//	}
//	A = left->operand.matrix;

    if (right->type == AL_OPERATION && right->operation.op == AL_EXP_TRANSPOSE) {
        GrB_Descriptor_set(desc, GrB_INP1, GrB_TRAN);
        right = right->operation.children[0];
    }

    if(right->type == AL_OPERAND) {
        B = right->operand.matrix;
    } else {
        assert(right->operation.op != AL_EXP_TRANSPOSE);
        if(res_in_use) {
            // `res` is in use, create an additional matrix.
            GrB_Matrix_nrows(&nrows, A);
            GrB_Matrix_ncols(&ncols, A);
            info = GrB_Matrix_new(&inter, GrB_BOOL, nrows, ncols);
            if(info != GrB_SUCCESS) {
                fprintf(stderr, "%s", GrB_error());
                assert(false);
            }
            B = _AlgebraicExpression_EvalArbitrary(right, inter);
        } else {
            // `res` is not used just yet, use it for RHS evaluation.
            B = _AlgebraicExpression_EvalArbitrary(right, res);
        }
    }
//	if(right->type == AL_OPERATION) {
//		assert(right->operation.op == AL_EXP_TRANSPOSE);
//		GrB_Descriptor_set(desc, GrB_INP1, GrB_TRAN);
//		right = CHILD_AT(right, 0);
//	}
//	B = right->operand.matrix;

    if(B == IDENTITY_MATRIX) {
        // B is the identity matrix, Perform A * I.
        info = GrB_Matrix_apply(res, GrB_NULL, GrB_NULL, GrB_IDENTITY_BOOL, A, desc);
        if(info != GrB_SUCCESS) {
            // If the multiplication failed, print error info to stderr and exit.
            fprintf(stderr, "Encountered an error in matrix multiplication:\n%s\n", GrB_error());
            assert(false);
        }
    } else {
        // Perform multiplication.
        info = GrB_mxm(res, GrB_NULL, GrB_NULL, GxB_ANY_PAIR_BOOL, A, B, desc);
        if(info != GrB_SUCCESS) {
            // If the multiplication failed, print error info to stderr and exit.
            fprintf(stderr, "Encountered an error in matrix multiplication:\n%s\n", GrB_error());
            assert(false);
        }
    }

    GrB_Matrix_nvals(&nvals, res);

    // Reset descriptor.
    GrB_Descriptor_set(desc, GrB_INP0, GxB_DEFAULT);

    uint child_count = AlgebraicExpression_ChildCount(exp);
    for(uint i = 2; i < child_count; i++) {
        // Reset descriptor.
        GrB_Descriptor_set(desc, GrB_INP1, GxB_DEFAULT);

        right = CHILD_AT(exp, i);
        if(right->type == AL_OPERAND) {
            B = right->operand.matrix;
        } else {
            if(right->operation.op == AL_EXP_TRANSPOSE) {
                GrB_Descriptor_set(desc, GrB_INP1, GrB_TRAN);
            }
            if(inter == GrB_NULL) {
                // Can't use `res`, use an intermidate matrix.
                GrB_Matrix_nrows(&nrows, res);
                GrB_Matrix_ncols(&ncols, res);
                GrB_Matrix_new(&inter, GrB_BOOL, nrows, ncols);
            }
            B = _AlgebraicExpression_EvalArbitrary(right, inter);
        }

//		if(right->type == AL_OPERATION) {
//			assert(right->operation.op == AL_EXP_TRANSPOSE);
//			GrB_Descriptor_set(desc, GrB_INP1, GrB_TRAN);
//			right = CHILD_AT(right, 0);
//		}
//		B = right->operand.matrix;

        if(B != IDENTITY_MATRIX) {
            // Perform multiplication.
            info = GrB_mxm(res, GrB_NULL, GrB_NULL, GxB_ANY_PAIR_BOOL, res, B, desc);
            if(info != GrB_SUCCESS) {
                // If the multiplication failed, print error info to stderr and exit.
                fprintf(stderr, "Encountered an error in matrix multiplication:\n%s\n", GrB_error());
                assert(false);
            }
        }
        GrB_Matrix_nvals(&nvals, res);
        if(nvals == 0) break;
    }

    if(inter != GrB_NULL) GrB_Matrix_free(&inter);
    GrB_free(&desc);
    return res;
}

GrB_Matrix _AlgebraicExpression_EvalArbitrary(const AlgebraicExpression *exp, GrB_Matrix res) {
    assert(exp);

    // Perform operation.
    switch(exp->type) {
        case AL_OPERATION:
            switch(exp->operation.op) {
                case AL_EXP_MUL:
                    res = _Eval_MulArbitrary(exp, res);
                    break;

                case AL_EXP_ADD:
                    res = _Eval_AddArbitrary(exp, res);
                    break;

                case AL_EXP_TRANSPOSE:
                    res = _Eval_TransposeArbitrary(exp, res);
                    break;

                default:
                    assert("Unknown algebraic expression operation" && false);
            }
            break;
        case AL_OPERAND:
            res = exp->operand.matrix;
            break;
        default:
            assert("Unknow algebraic expression node type" && false);
    }

    return res;
}

void AlgebraicExpression_EvalArbitrary(const AlgebraicExpression *exp, GrB_Matrix res) {
    // Why we can`t evaluate operand? Fetch and eval support case of ae operand.
    assert(exp && exp->type == AL_OPERATION);

    // On first evaluation we need to fetch operands
    _AlgebraicExpression_FetchOperands((AlgebraicExpression *)exp, QueryCtx_GetGraphCtx(),
                                       QueryCtx_GetGraph());

    _AlgebraicExpression_EvalArbitrary(exp, res);
}