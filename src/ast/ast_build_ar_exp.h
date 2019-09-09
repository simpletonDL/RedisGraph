/*
* Copyright 2018-2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#include "ast.h"
#include "ast_shared.h"
#include "../arithmetic/arithmetic_expression.h"

#pragma once

AR_ExpNode *AR_EXP_FromExpression(const cypher_astnode_t *expr);
