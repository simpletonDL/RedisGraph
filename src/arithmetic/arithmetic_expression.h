/*
* Copyright 2018-2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#pragma once

#include "./agg_ctx.h"
#include "rax.h"
#include "./agg_ctx.h"
#include "../ast/ast.h"
#include "./func_desc.h"
#include "../ast/ast_shared.h"
#include "../../deps/rax/rax.h"
#include "../execution_plan/record.h"
#include "../graph/entities/graph_entity.h"

/* AR_ExpNodeType lists the type of nodes within
 * an arithmetic expression tree. */
typedef enum {
	AR_EXP_UNKNOWN,
	AR_EXP_OP,
	AR_EXP_OPERAND
} AR_ExpNodeType;

/* AR_OPType type of operation
 * either an aggregation function which requires a context
 * or a stateless function. */
typedef enum {
	AR_OP_UNKNOWN,
	AR_OP_AGGREGATE,
	AR_OP_FUNC,
} AR_OPType;

/* AR_OperandNodeType type of leaf node,
 * either a constant: 3, or a variable: node.property. */
typedef enum {
	AR_EXP_OP_UNKNOWN,
	AR_EXP_CONSTANT,
	AR_EXP_VARIADIC,
} AR_OperandNodeType;

/* Success of an evaluation. */
typedef enum {
	EVAL_OK = 0,
	EVAL_ERR = (1 << 0),
} AR_EXP_Result;

/* AR_Func - Function pointer to an operation with an arithmetic expression */
typedef SIValue(*AR_Func)(SIValue *argv, int argc);

/* Op represents an operation applied to child args. */
typedef struct {
	union {
		AR_FuncDesc *f;
		AggCtx *agg_func;
	};                            /* Operation to perform on children. */
	const char *func_name;        /* Name of function. */
	int child_count;              /* Number of children. */
	struct AR_ExpNode **children; /* Child nodes. */
	AR_OPType type;
} AR_OpNode;

/* OperandNode represents either a constant numeric value,
 * or a graph entity property. */
typedef struct {
	union {
		SIValue constant;
		struct {
			const char *entity_alias;
			const char *entity_prop;
			int entity_alias_idx;
			Attribute_ID entity_prop_idx;
		} variadic;
	};
	AR_OperandNodeType type;
} AR_OperandNode;

/* AR_ExpNode a node within an arithmetic expression tree,
 * This node can take one of two forms:
 * 1. OpNode
 * 2. OperandNode */
typedef struct AR_ExpNode {
	union {
		AR_OperandNode operand;
		AR_OpNode op;
	};
	AR_ExpNodeType type;
	// The string representation of the node, such as the literal string "ID(a) + 5"
	const char *resolved_name;
} AR_ExpNode;

/* Creates a new Arithmetic expression operation node */
AR_ExpNode *AR_EXP_NewOpNode(const char *func_name, uint child_count);

/* Creates a new Arithmetic expression variable operand node */
AR_ExpNode *AR_EXP_NewVariableOperandNode(const char *alias, const char *prop);

/* Creates a new Arithmetic expression constant operand node */
AR_ExpNode *AR_EXP_NewConstOperandNode(SIValue constant);

/* Return AR_OperandNodeType for operands and -1 for operations. */
int AR_EXP_GetOperandType(AR_ExpNode *exp);

/* Evaluate arithmetic expression tree. */
SIValue AR_EXP_Evaluate(AR_ExpNode *root, const Record r);
void AR_EXP_Aggregate(const AR_ExpNode *root, const Record r);
void AR_EXP_Reduce(const AR_ExpNode *root);

/* Utility functions */
/* Traverse an expression tree and add all graph entities
 * (from variadic) to a triemap. */
void AR_EXP_CollectEntities(AR_ExpNode *root, rax *record_ids);

/* Traverse an expression tree and add all mentioned attributes:
 * n.attr > 3 to a prefix tree. */
void AR_EXP_CollectAttributes(AR_ExpNode *root, rax *attributes);

/* Search for an aggregation node within the expression tree.
 * Return 1 and sets agg_node to the aggregation node if exists,
 * Please note an expression tree can only contain a single aggregation node. */
bool AR_EXP_ContainsAggregation(AR_ExpNode *root, AR_ExpNode **agg_node);

/* Constructs string representation of arithmetic expression tree. */
void AR_EXP_ToString(const AR_ExpNode *root, char **str);

AR_ExpNode *AR_EXP_NewVariableOperandNode(const char *alias, const char *prop);
AR_ExpNode *AR_EXP_NewConstOperandNode(SIValue constant);

/* Construct an arithmetic expression tree from a CYPHER_AST_EXPRESSION node. */
AR_ExpNode *AR_EXP_FromExpression(const cypher_astnode_t *expr);

/* Clones given expression. */
AR_ExpNode *AR_EXP_Clone(AR_ExpNode *exp);

/* Free arithmetic expression tree. */
void AR_EXP_Free(AR_ExpNode *root);

